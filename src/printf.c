#include "types.h"
#include "user.h"
#include "console.h"
#include <stdarg.h>

static void
putc(int fd, char c) {
  write(fd, &c, 1);
}

static char digits[] = "0123456789abcdef";

static void
print_x64(int fd, addr_t x) {
  int i;
  for (i = 0; i < (sizeof(addr_t) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(addr_t) * 8 - 4)]);
}

static void
print_x32(int fd, uint x) {
  int i;
  for (i = 0; i < (sizeof(uint) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(uint) * 8 - 4)]);
}

static void
print_d(int fd, int v) {
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
    putc(fd, buf[i]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
// understands *some* ANSI escape sequences for color
void printf(int fd, char* fmt, ...) {
  static int color = -1;
  va_list ap;
  int i, c;
  char* s;

  va_start(ap, fmt);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c == '') {
      if ((c = fmt[++i] & 0xff) != '[') {
        putc(fd, '[');
        putc(fd, c);
        continue;
      }

      s = &fmt[++i];
      int num = 0;
      while ('0' <= *s && *s <= '9') {
        num = num * 10 + *s++ - '0';
        ++i;
      }

      switch ((c = fmt[i] & 0xff)) {
      case 'm':
        if (color == -1)
          color = ioctl(fd, CONSOLE_GET_COLOR, -1);
        switch (num) {
        case 30:
          ioctl(fd, CONSOLE_SET_FG, CGA_BLACK);
          break;
        case 31:
          ioctl(fd, CONSOLE_SET_FG, CGA_RED);
          break;
        case 32:
          ioctl(fd, CONSOLE_SET_FG, CGA_GREEN);
          break;
        case 33:
          ioctl(fd, CONSOLE_SET_FG, CGA_YELLOW);
          break;
        case 34:
          ioctl(fd, CONSOLE_SET_FG, CGA_BLUE);
          break;
        case 35:
          ioctl(fd, CONSOLE_SET_FG, CGA_MAGENTA);
          break;
        case 36:
          ioctl(fd, CONSOLE_SET_FG, CGA_CYAN);
          break;
        case 37:
          ioctl(fd, CONSOLE_SET_FG, CGA_WHITE);
          break;
        case 39:
        case 0:
          ioctl(fd, CONSOLE_SET_COLOR, color);
          color = -1;
          break;
        }
        break;
      default:
        putc(fd, c);
        break;
      }

      continue;
    }

    if (c != '%') {
      putc(fd, c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c) {
    case 'c':
      putc(fd, va_arg(ap, int));
      break;
    case 'd':
      print_d(fd, va_arg(ap, int));
      break;
    case 'x':
      print_x32(fd, va_arg(ap, uint));
      break;
    case 'p':
      print_x64(fd, va_arg(ap, addr_t));
      break;
    case 's':
      if ((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      while (*s)
        putc(fd, *(s++));
      break;
    case '%':
      putc(fd, '%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      putc(fd, '%');
      putc(fd, c);
      break;
    }
  }
}
