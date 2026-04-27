// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include <stdarg.h>

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "console.h"
#include "vga.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static char digits[] = "0123456789abcdef";

static void
print_x64(addr_t x) {
  int i;
  for (i = 0; i < (sizeof(addr_t) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(addr_t) * 8 - 4)]);
}

static void
print_x32(uint x) {
  int i;
  for (i = 0; i < (sizeof(uint) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint) * 8 - 4)]);
}

static void
print_d(int v) {
  char buf[16];
  int64 x = v;

  if (v < 0)
    x = -x;

  int i = 0;
  do {
    buf[i++] = digits[x % 10];
    x /= 10;
  } while (x != 0);

  if (v < 0)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char* fmt, ...) {
  va_list ap;
  int i, c, locking;
  char* s;

  va_start(ap, fmt);

  locking = cons.locking;
  if (locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c != '%') {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c) {
    case 'd':
      print_d(va_arg(ap, int));
      break;
    case 'x':
      print_x32(va_arg(ap, uint));
      break;
    case 'p':
      print_x64(va_arg(ap, addr_t));
      break;
    case 's':
      if ((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      while (*s)
        consputc(*(s++));
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if (locking)
    release(&cons.lock);
}

__attribute__((noreturn)) void
panic(char* s) {
  int i;
  addr_t pcs[10];

  cli();
  cons.locking = 0;
  cprintf("cpu%d: panic: ", cpu->id);
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for (i = 0; i < 10; i++)
    cprintf(" %p\n", pcs[i]);
  panicked = 1; // freeze other CPU
  for (;;)
    hlt();
}

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort* crt = (ushort*)P2V(0xb8000); // CGA memory

// cgaputc but with color support
static void
cgaputc(int c, int attr) {
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  if (c == '\n')
    pos += 80 - pos % 80;
  else if (c == BACKSPACE) {
    if (pos > 0) {
      --pos;
      crt[pos] = ' ' | attr;
    }
  } else
    crt[pos++] = (c & 0xff) | attr;

  if ((pos / 80) >= 24) { // Scroll up.
    memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
    pos -= 80;
    for (int i = pos; i < 24 * 80; i++)
      crt[i] = (ushort)(' ' | attr);
  }

  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
}

static int global_fg = CGA_LIGHT_GRAY;
static int global_bg = CGA_BLACK;
static uchar font_buf[CONSOLE_FONT_BYTES];
static int font_loading;
static int font_offset;
#define MAKE_ATTR(fg, bg) (((bg) << 4 | (fg)) << 8)
#define global_attr MAKE_ATTR(global_fg, global_bg)

// consputc but with attr to add color support
void consputc_color(int c, int attr) {
  if (panicked) {
    cli();
    for (;;)
      hlt();
  }

  if (c == BACKSPACE) {
    uartputc('\b');
    uartputc(' ');
    uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c, attr);
}

// Wrapper to use "color" version but with global color by default
void consputc(int c) {
  consputc_color(c, global_attr);
}

// Move cursor left one cell without modifying the framebuffer or erasing.
// consputc(BACKSPACE) erases the char at the new position; this does not.
static void
cons_cursor_left(void) {
  uartputc('\b'); // serial: just move left, no erase
  // CGA: update hardware cursor register without touching crt[]
  outb(CRTPORT, 14);
  int pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);
  if (pos > 0)
    pos--;
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
}

#define INPUT_BUF 128
struct {
  struct spinlock lock;
  char buf[INPUT_BUF];
  uint r;
  uint w;
  uint e;
  uint cur; // terminal cursor: w <= cur <= e
  int raw;
  uint esc_state;
} input;

#define C(x) ((x) - '@')
// PS/2 scan codes from kbd.h for graphical QEMU
#define KEY_UP 0xE2
#define KEY_DN 0xE3
#define KEY_LF 0xE4
#define KEY_RT 0xE5

static void
clear(void) {
  // Move terminal cursor to end before erasing so redraw is correct
  while (input.cur < input.e)
    consputc(input.buf[input.cur++ % INPUT_BUF]);
  while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
    input.e--;
    consputc(BACKSPACE);
  }
  input.cur = input.e;
}

static char history_draft[INPUT_BUF];
static int history_draft_active;

static void
save_history_draft(void) {
  uint len = input.e - input.w;

  if (len >= INPUT_BUF)
    len = INPUT_BUF - 1;
  for (uint i = 0; i < len; i++)
    history_draft[i] = input.buf[(input.w + i) % INPUT_BUF];
  history_draft[len] = 0;
  history_draft_active = 1;
}

static void
replace_input(char* s) {
  char* replacement = s;
  int restoring_draft = *s == 0 && history_draft_active;

  if (restoring_draft)
    replacement = history_draft;
  else if (!history_draft_active)
    save_history_draft();

  clear();
  while (*replacement != 0 && input.e - input.r < INPUT_BUF - 1) {
    input.buf[input.e++ % INPUT_BUF] = *replacement;
    input.cur = input.e;
    consputc(*replacement++);
  }

  if (restoring_draft || *s == '\n' || *s == '\r') {
    history_draft_active = 0;
    history_draft[0] = 0;
  }
}

static void
handle_input(int c) {
  // PS/2 arrow keys (graphical QEMU)
  if (c == KEY_LF) {
    if (input.cur > input.w) {
      input.cur--;
      cons_cursor_left();
    }
    return;
  }
  if (c == KEY_RT) {
    if (input.cur < input.e)
      consputc(input.buf[input.cur++ % INPUT_BUF]);
    return;
  }
  if (c == KEY_UP || c == KEY_DN)
    c = (c == KEY_UP) ? C('P') : C('N');
  // ESC sequence state machine (serial/nographic arrow keys)
  else if (input.esc_state == 1) {
    input.esc_state = (c == '[') ? 2 : 0;
    return;
  } else if (input.esc_state == 2) {
    input.esc_state = 0;
    if (c == 'A' || c == 'B') {
      c = (c == 'A') ? C('P') : C('N');
    } else if (c == 'C') {
      if (input.cur < input.e)
        consputc(input.buf[input.cur++ % INPUT_BUF]);
      return;
    } else if (c == 'D') {
      if (input.cur > input.w) {
        input.cur--;
        cons_cursor_left();
      }
      return;
    } else {
      return;
    }
  } else if (c == '\x1b') {
    input.esc_state = 1;
    return;
  }

  if (c == C('Z')) {
    lidt(0, 0);
    return;
  }

  if (input.raw && input.e - input.r < INPUT_BUF) {
    input.buf[input.e++ % INPUT_BUF] = c;
    input.cur = input.e;
    input.w = input.e;
    wakeup(&input.r);
    return;
  }

  switch (c) {
  case C('U'):
    clear();
    history_draft_active = 0;
    break;
  case C('P'):
  case C('N'):
    if (!history_draft_active)
      save_history_draft();
    clear();
    if (input.e - input.r < INPUT_BUF) {
      input.buf[input.e++ % INPUT_BUF] = c;
      input.cur = input.e;
      input.w = input.e;
      wakeup(&input.r);
    }
    break;
  case C('H'):
  case '\x7f': {
    if (input.cur == input.w)
      break;
    input.cur--;
    for (uint i = input.cur; i < input.e - 1; i++)
      input.buf[i % INPUT_BUF] = input.buf[(i + 1) % INPUT_BUF];
    input.e--;
    uint n = input.e - input.cur; // chars from new cur to new end
    consputc(BACKSPACE);          // erase+move left; redraw covers it
    for (uint i = 0; i < n; i++)
      consputc(input.buf[(input.cur + i) % INPUT_BUF]);
    consputc(' '); // erase ghost at old end
    for (uint i = 0; i <= n; i++)
      cons_cursor_left(); // return to cur without erasing
    break;
  }
  default:
    if (c == 0 || input.e - input.r >= INPUT_BUF)
      break;
    c = (c == '\r') ? '\n' : c;
    if (c == '\n' || c == C('D')) {
      // Submit whole line regardless of cursor position
      while (input.cur < input.e)
        consputc(input.buf[input.cur++ % INPUT_BUF]);
      input.buf[input.e++ % INPUT_BUF] = c;
      consputc(c);
      input.cur = input.e;
      input.w = input.e;
      history_draft_active = 0;
      wakeup(&input.r);
    } else if (input.cur == input.e) {
      input.buf[input.e++ % INPUT_BUF] = c;
      input.cur = input.e;
      consputc(c);
      if (input.e == input.r + INPUT_BUF) {
        input.w = input.e;
        wakeup(&input.r);
      }
    } else {
      // Insert at cursor: shift tail right, redraw, return cursor
      for (uint i = input.e; i > input.cur; i--)
        input.buf[i % INPUT_BUF] = input.buf[(i - 1) % INPUT_BUF];
      input.buf[input.cur % INPUT_BUF] = c;
      input.e++;
      for (uint i = input.cur; i < input.e; i++)
        consputc(input.buf[i % INPUT_BUF]);
      input.cur++;
      for (uint i = input.cur; i < input.e; i++)
        cons_cursor_left();
      if (input.e == input.r + INPUT_BUF) {
        while (input.cur < input.e)
          consputc(input.buf[input.cur++ % INPUT_BUF]);
        input.w = input.e;
        wakeup(&input.r);
      }
    }
    break;
  }
}

void consoleinput(char* s) {
  acquire(&input.lock);
  replace_input(s);
  release(&input.lock);
}

void consoleintr(int (*getc)(void)) {
  int c;
  acquire(&input.lock);
  while ((c = getc()) >= 0)
    handle_input(c);
  release(&input.lock);
}

int consoleread(struct file* f, char* dst, int n) {
  uint target;
  int c;

  target = n;
  acquire(&input.lock);
  while (n > 0) {
    while (input.r == input.w) {
      if (proc->killed) {
        release(&input.lock);
        return -1;
      }
      sleep(&input.r, &input.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if (c == C('D')) { // EOF
      if (n < target) {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;

    // Return after reading one character in raw mode
    if (input.raw || c == '\n')
      break;
  }
  release(&input.lock);

  return target - n;
}

int consoleioctl(struct file* f, int param, int value) {
  switch (param) {
  case CONSOLE_SET_COLOR:
    acquire(&cons.lock);
    global_fg = value & 0xf;
    global_bg = (value >> 4) & 0xf;
    release(&cons.lock);
    return 0;
  case CONSOLE_SET_FG:
    acquire(&cons.lock);
    global_fg = value & 0xf;
    release(&cons.lock);
    return 0;
  case CONSOLE_SET_BG:
    acquire(&cons.lock);
    global_bg = value & 0xf;
    release(&cons.lock);
    return 0;
  case CONSOLE_RAW_TOGGLE:
    input.r = input.w = input.e = 0;
    input.raw = !input.raw;
    return 0;
  case CONSOLE_RAW_READ:
    if (!input.raw || input.r == input.w)
      return -1;
    return input.buf[input.r++ % INPUT_BUF];
  case CONSOLE_REPAINT: {
    int attr = (global_bg << 4 | global_fg) << 8;
    acquire(&cons.lock);
    for (int i = 0; i < 80 * 24; i++)
      crt[i] = (crt[i] & 0x00ff) | attr;
    release(&cons.lock);
    return 0;
  }
  case CONSOLE_FONT_BEGIN:
    acquire(&cons.lock);
    font_loading = 1;
    font_offset = 0;
    release(&cons.lock);
    return 0;
  case CONSOLE_FONT_CANCEL:
    acquire(&cons.lock);
    font_loading = 0;
    font_offset = 0;
    release(&cons.lock);
    return 0;
  case CONSOLE_FONT_DEFAULT:
    acquire(&cons.lock);
    font_loading = 0;
    font_offset = 0;
    vgaLoadDefaultFont();
    release(&cons.lock);
    return 0;
  default:
    return -1;
  }
}

int consolewrite(struct file* f, char* buf, int n) {
  int attr = global_attr;
  // Use local color if exists
  if (f && f->dev_payload)
    attr = (int)(uint64)f->dev_payload;

  acquire(&cons.lock);
  if (font_loading) {
    for (int i = 0; i < n && font_loading; i++) {
      font_buf[font_offset++] = buf[i];
      if (font_offset == CONSOLE_FONT_BYTES) {
        vgaLoadFont((char*)font_buf);
        font_loading = 0;
        font_offset = 0;
      }
    }
    release(&cons.lock);
    return n;
  }

  for (int i = 0; i < n; i++)
    consputc_color(buf[i] & 0xff, attr);
  release(&cons.lock);

  return n;
}

void consoleinit(void) {
  initlock(&cons.lock, "console");
  initlock(&input.lock, "input");
  input.raw = 0;

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].ioctl = consoleioctl;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}
