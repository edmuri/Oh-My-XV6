#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "traps.h"
#include "vga.h"
#include "x86.h"

static struct {
  struct spinlock lock;
  int locking;
} disp;

#define CRTPORT 0x3d4
static ushort* conscrt = (ushort*)P2V(0xb8000); // CGA memory
static uchar* dispcrt = (uchar*)P2V(0xa0000);   // VGA memory

// Global storage for the cursor position and the console content
static int cursor;
static ushort console[80 * 25];

int displayioctl(struct file* f, int param, int value) {
  acquire(&disp.lock);
  switch (param) {
  case 1: // mode 1: switch between VGA modes
    if (value == 0x3) {
      vgaMode3();

      // Restore cursor position
      outb(CRTPORT, 14);
      outb(CRTPORT + 1, cursor >> 8);
      outb(CRTPORT, 15);
      outb(CRTPORT + 1, cursor);

      // Restore console content
      memmove(conscrt, console, sizeof(console));
    } else if (value == 0x13) {
      // Save cursor position
      outb(CRTPORT, 14);
      cursor = inb(CRTPORT + 1) << 8;
      outb(CRTPORT, 15);
      cursor |= inb(CRTPORT + 1);

      // Save console content
      memmove(console, conscrt, sizeof(console));

      vgaMode13();
    }
    break;
  case 2:                               // mode 2: set VGA palette
    vgaSetPalette((value >> 24) & 0xff, // index
                  (value >> 16) & 0xff, // r
                  (value >> 8) & 0xff,  // g
                  value & 0xff);        // b
    break;
  }
  release(&disp.lock);

  return value;
}

int displaywrite(struct file* f, char* buf, int n) {
  acquire(&disp.lock);
  // Just move content from the buffer into the VGA memory
  memmove(dispcrt + f->off, buf, n);
  f->off += n; // move the file offset for the next writes to continue
  release(&disp.lock);

  return n;
}

void displayinit(void) {
  initlock(&disp.lock, "display");

  devsw[DISPLAY].write = displaywrite;
  devsw[DISPLAY].ioctl = displayioctl;
  disp.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}
