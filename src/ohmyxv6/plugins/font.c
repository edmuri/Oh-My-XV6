#include "../../types.h"
#include "../../user.h"
#include "../../fcntl.h"
#include "../../console.h"
#include "../../fs.h"
#include "../../stat.h"

#define FONTS_DIR "/etc/ohmyxv6/fonts"
#define MAX_FONTS 32
#define MAX_FONT_NAME 32
#define MAX_FONT_PATH 64
#define PSF1_HEADER_BYTES 4
#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04
#define PSF1_MODE256 0x00
#define PSF1_CHARSIZE16 0x10

struct font {
  char name[MAX_FONT_NAME];
  char path[MAX_FONT_PATH];
};

static struct font fonts[MAX_FONTS];
static int nfonts;

static void copyname(char* dst, char* src, int max) {
  int n = 0;

  while (n + 1 < max && n < DIRSIZ && src[n]) {
    dst[n] = src[n];
    n++;
  }
  dst[n] = 0;
}

static void sortfonts(void) {
  for (int i = 0; i < nfonts; i++) {
    for (int j = i + 1; j < nfonts; j++) {
      int cmp;
      if (strcmp(fonts[i].name, "default") == 0)
        cmp = -1;
      else if (strcmp(fonts[j].name, "default") == 0)
        cmp = 1;
      else
        cmp = strcmp(fonts[i].name, fonts[j].name);

      if (cmp > 0) {
        struct font tmp = fonts[i];
        fonts[i] = fonts[j];
        fonts[j] = tmp;
      }
    }
  }
}

static int loadfonts(void) {
  int fd;
  struct dirent de;
  struct stat st;
  char path[128];
  char* p;

  nfonts = 0;
  strcpy(fonts[nfonts].name, "default");
  fonts[nfonts].path[0] = 0;
  nfonts++;

  if ((fd = open(FONTS_DIR, O_RDONLY)) < 0)
    return 0;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0 || de.name[0] == '.')
      continue;
    if (nfonts == MAX_FONTS) {
      printf(2, "font: too many font files\n");
      close(fd);
      return -1;
    }

    strcpy(path, FONTS_DIR);
    p = path + strlen(path);
    *p++ = '/';
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    if (stat(path, &st) < 0 || st.type != T_FILE)
      continue;
    if (st.size != CONSOLE_FONT_BYTES &&
        st.size != CONSOLE_FONT_BYTES + PSF1_HEADER_BYTES)
      continue;

    copyname(fonts[nfonts].name, de.name, sizeof(fonts[nfonts].name));
    strcpy(fonts[nfonts].path, path);
    nfonts++;
  }

  close(fd);
  sortfonts();
  return 0;
}

static int colorvalue(char* name) {
  if (strcmp(name, "black") == 0)
    return CGA_BLACK;
  if (strcmp(name, "blue") == 0)
    return CGA_BLUE;
  if (strcmp(name, "green") == 0)
    return CGA_GREEN;
  if (strcmp(name, "cyan") == 0)
    return CGA_CYAN;
  if (strcmp(name, "red") == 0)
    return CGA_RED;
  if (strcmp(name, "magenta") == 0)
    return CGA_MAGENTA;
  if (strcmp(name, "brown") == 0)
    return CGA_BROWN;
  if (strcmp(name, "light-gray") == 0 || strcmp(name, "gray") == 0)
    return CGA_LIGHT_GRAY;
  if (strcmp(name, "dark-gray") == 0)
    return CGA_DARK_GRAY;
  if (strcmp(name, "light-blue") == 0)
    return CGA_LIGHT_BLUE;
  if (strcmp(name, "light-green") == 0)
    return CGA_LIGHT_GREEN;
  if (strcmp(name, "light-cyan") == 0)
    return CGA_LIGHT_CYAN;
  if (strcmp(name, "light-red") == 0)
    return CGA_LIGHT_RED;
  if (strcmp(name, "light-magenta") == 0)
    return CGA_LIGHT_MAGENTA;
  if (strcmp(name, "yellow") == 0)
    return CGA_YELLOW;
  if (strcmp(name, "white") == 0)
    return CGA_WHITE;

  if (name[0] >= '0' && name[0] <= '9') {
    int value = atoi(name);
    if (value >= 0 && value <= 15)
      return value;
  }
  return -1;
}

static void listcolors(void) {
  printf(1, "colors:\n");
  printf(1, "- 0 black\n");
  printf(1, "- 1 blue\n");
  printf(1, "- 2 green\n");
  printf(1, "- 3 cyan\n");
  printf(1, "- 4 red\n");
  printf(1, "- 5 magenta\n");
  printf(1, "- 6 brown\n");
  printf(1, "- 7 light-gray\n");
  printf(1, "- 8 dark-gray\n");
  printf(1, "- 9 light-blue\n");
  printf(1, "- 10 light-green\n");
  printf(1, "- 11 light-cyan\n");
  printf(1, "- 12 light-red\n");
  printf(1, "- 13 light-magenta\n");
  printf(1, "- 14 yellow\n");
  printf(1, "- 15 white\n");
}

static int setcolor(char* color) {
  int value = colorvalue(color);
  int fd;

  if (value < 0) {
    printf(2, "font: unknown color '%s'\n", color);
    return -1;
  }

  fd = open("/dev/console", O_RDWR);
  if (fd < 0) {
    printf(2, "font: cannot open console\n");
    return -1;
  }

  ioctl(fd, CONSOLE_SET_FG, value);
  ioctl(fd, CONSOLE_REPAINT, 0);
  close(fd);
  printf(1, "font color: %s\n", color);
  return 0;
}

static int loadfont(char* name) {
  char buf[128];
  uchar header[PSF1_HEADER_BYTES];
  struct stat st;
  int fontfd;
  int consfd;
  int total = 0;
  int psf1 = 0;

  if (strcmp(name, "default") == 0) {
    consfd = open("/dev/console", O_RDWR);
    if (consfd < 0) {
      printf(2, "font: cannot open console\n");
      return -1;
    }
    if (ioctl(consfd, CONSOLE_FONT_DEFAULT, 0) < 0) {
      close(consfd);
      printf(2, "font: console cannot restore default font\n");
      return -1;
    }
    close(consfd);
    printf(1, "font: default\n");
    return 0;
  }

  for (int i = 0; i < nfonts; i++) {
    if (strcmp(fonts[i].name, name) != 0)
      continue;

    if (fonts[i].path[0] == 0) {
      printf(2, "font: %s has no loadable font file\n", name);
      return -1;
    }

    if (stat(fonts[i].path, &st) < 0) {
      printf(2, "font: cannot stat %s\n", fonts[i].path);
      return -1;
    }
    if (st.size == CONSOLE_FONT_BYTES) {
      psf1 = 0;
    } else if (st.size == CONSOLE_FONT_BYTES + PSF1_HEADER_BYTES) {
      psf1 = 1;
    } else {
      printf(2, "font: %s must be %d raw bytes or %d PSF1 bytes\n",
             fonts[i].path, CONSOLE_FONT_BYTES,
             CONSOLE_FONT_BYTES + PSF1_HEADER_BYTES);
      return -1;
    }

    if ((fontfd = open(fonts[i].path, O_RDONLY)) < 0) {
      printf(2, "font: cannot open %s\n", fonts[i].path);
      return -1;
    }

    if (psf1) {
      if (read(fontfd, (char*)header, sizeof(header)) != sizeof(header)) {
        close(fontfd);
        printf(2, "font: failed reading %s\n", fonts[i].path);
        return -1;
      }
      if (header[0] != PSF1_MAGIC0 || header[1] != PSF1_MAGIC1 ||
          header[2] != PSF1_MODE256 || header[3] != PSF1_CHARSIZE16) {
        close(fontfd);
        printf(2, "font: %s is not a 256 glyph PSF1 8x16 font\n",
               fonts[i].path);
        return -1;
      }
    }

    consfd = open("/dev/console", O_RDWR);
    if (consfd < 0) {
      close(fontfd);
      printf(2, "font: cannot open console\n");
      return -1;
    }

    if (ioctl(consfd, CONSOLE_FONT_BEGIN, 0) < 0) {
      close(consfd);
      close(fontfd);
      printf(2, "font: console cannot load fonts\n");
      return -1;
    }

    for (;;) {
      int n = read(fontfd, buf, sizeof(buf));
      if (n < 0) {
        ioctl(consfd, CONSOLE_FONT_CANCEL, 0);
        close(consfd);
        close(fontfd);
        printf(2, "font: failed reading %s\n", fonts[i].path);
        return -1;
      }
      if (n == 0)
        break;
      total += n;
      if (total > CONSOLE_FONT_BYTES) {
        ioctl(consfd, CONSOLE_FONT_CANCEL, 0);
        close(consfd);
        close(fontfd);
        printf(2, "font: %s is larger than %d bytes\n",
               fonts[i].path, CONSOLE_FONT_BYTES);
        return -1;
      }
      if (write(consfd, buf, n) != n) {
        ioctl(consfd, CONSOLE_FONT_CANCEL, 0);
        close(consfd);
        close(fontfd);
        printf(2, "font: failed writing font to console\n");
        return -1;
      }
    }

    close(consfd);
    close(fontfd);
    if (total != CONSOLE_FONT_BYTES) {
      printf(2, "font: %s is %d bytes; expected %d raw 8x16 bytes\n",
             fonts[i].path, total, CONSOLE_FONT_BYTES);
      return -1;
    }

    printf(1, "font: %s\n", name);
    return 0;
  }

  printf(2, "font: unknown font '%s'\n", name);
  return -1;
}

int main(int argc, char* argv[]) {
  if (loadfonts() < 0)
    exit();

  if (argc < 2 || strcmp(argv[1], "list") == 0) {
    printf(1, "fonts:\n");
    for (int i = 0; i < nfonts; i++)
      printf(1, "- %s\n", fonts[i].name);
    exit();
  }

  if (strcmp(argv[1], "color") == 0) {
    if (argc < 3) {
      listcolors();
      exit();
    }
    setcolor(argv[2]);
    exit();
  }

  loadfont(argv[1]);
  exit();
}
