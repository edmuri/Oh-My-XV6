#include "../../user.h"
#include "../../fcntl.h"
#include "../../console.h"

struct theme {
  char* name;
  int fg;
  int bg;
};

static struct theme themes[] = {
    {"default", 7, 0},
    {"matrix", 10, 0},
    {"dracula", 15, 8},
    {"monokai", 14, 8},
    {"nord", 11, 8},
    {"solarized-dark", 11, 0},
    {"gruvbox", 14, 0},
    {"ocean", 15, 1},
    {"cherry", 15, 4},
    {"forest", 10, 8},
    {"midnight", 9, 0},
    {"sunrise", 14, 4},
};

#define NTHEMES ((int)(sizeof(themes) / sizeof(themes[0])))

static int themeapply(char* name) {
  for (int i = 0; i < NTHEMES; i++) {
    if (strcmp(themes[i].name, name) == 0) {
      int fd = open("/dev/console", O_RDWR);
      if (fd < 0) {
        printf(2, "theme: cannot open console\n");
        return -1;
      }

      ioctl(fd, CONSOLE_SET_FG, themes[i].fg);
      ioctl(fd, CONSOLE_SET_BG, themes[i].bg);
      ioctl(fd, CONSOLE_REPAINT, 0);
      close(fd);

      printf(1, "theme: %s\n", name);
      return 0;
    }
  }
  return -1;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || strcmp(argv[1], "list") == 0) {
    printf(1, "themes:\n");
    for (int i = 0; i < NTHEMES; i++)
      printf(1, "- %s\n", themes[i].name);
    exit();
  }

  if (themeapply(argv[1]) < 0) {
    printf(2, "theme: unknown theme '%s'\n", argv[1]);
    printf(2, "run 'ohmyxv6 theme' to see available themes\n");
  }

  exit();
}
