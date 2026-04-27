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
    {"3024-day", 0, 7},
    {"adventuretime", 14, 1},
    {"alabaster", 8, 15},
    {"argonaut", 11, 1},
    {"atom-one-dark", 7, 1},
    {"atom-one-light", 1, 7},
    {"ayu-light", 8, 14},
    {"basic", 0, 15},
    {"cobalt-neon", 10, 1},
    {"eclipse", 5, 15},
    {"emacs", 0, 11},
    {"github", 1, 15},
    {"grass", 10, 2},
    {"gruvbox", 14, 0},
    {"homebrew", 10, 0},
    {"intellij-light", 8, 11},
    {"jetbrains-darcula", 13, 8},
    {"man-page", 0, 14},
    {"material-dark", 10, 8},
    {"matrix", 2, 0},
    {"monokai", 14, 8},
    {"nord", 11, 8},
    {"novel", 6, 7},
    {"ocean", 15, 1},
    {"pro", 15, 0},
    {"red-sands", 0, 4},
    {"silver-aerogel", 8, 7},
    {"solarized-dark", 11, 0},
    {"solarized-light", 1, 14},
    {"solid-colors", 0, 13},
    {"sublime", 14, 6},
    {"tomorrow-night-blue", 15, 9},
    {"ubuntu", 15, 12},
    {"vim", 10, 4},
    {"visual-studio", 15, 5},
    {"vs-code-dark-plus", 7, 8},
    {"vs-code-light-plus", 15, 7},
    {"vs-code-monokai", 13, 0},
    {"xcode-dark", 11, 9},
    {"xcode-light", 9, 15},
    {"zenburn", 2, 8},
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
