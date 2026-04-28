#include "../../user.h"
#include "../../fcntl.h"
#include "../../console.h"
#include "../../fs.h"
#include "../../stat.h"

#define THEMES_DIR "/etc/ohmyxv6/themes"
#define MAX_THEMES 64
#define MAX_THEME_NAME 32
#define THEME_FILE_BUF 96

struct theme {
  char name[MAX_THEME_NAME];
  int fg;
  int bg;
};

static struct theme themes[MAX_THEMES];
static int nthemes;

static char* readword(char* p, char* dst, int max) {
  int n = 0;

  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
    p++;

  while (*p && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
    if (n + 1 < max)
      dst[n++] = *p;
    p++;
  }
  dst[n] = 0;
  return p;
}

static int themeparse(char* path, struct theme* theme) {
  int fd;
  int n;
  char buf[THEME_FILE_BUF];
  char num[8];
  char* p;

  if ((fd = open(path, O_RDONLY)) < 0)
    return -1;

  n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0)
    return -1;
  buf[n] = 0;

  p = readword(buf, theme->name, sizeof(theme->name));
  if (theme->name[0] == 0)
    return -1;

  p = readword(p, num, sizeof(num));
  theme->fg = atoi(num);
  p = readword(p, num, sizeof(num));
  theme->bg = atoi(num);
  if (theme->fg < 0 || theme->fg > 15 || theme->bg < 0 || theme->bg > 15)
    return -1;

  return 0;
}

static void sortthemes(void) {
  for (int i = 0; i < nthemes; i++) {
    for (int j = i + 1; j < nthemes; j++) {
      int cmp;
      if (strcmp(themes[i].name, "default") == 0)
        cmp = -1;
      else if (strcmp(themes[j].name, "default") == 0)
        cmp = 1;
      else
        cmp = strcmp(themes[i].name, themes[j].name);

      if (cmp > 0) {
        struct theme tmp = themes[i];
        themes[i] = themes[j];
        themes[j] = tmp;
      }
    }
  }
}

static int loadthemes(void) {
  int fd;
  struct dirent de;
  struct stat st;
  char path[128];
  char* p;

  nthemes = 0;
  strcpy(themes[nthemes].name, "default");
  themes[nthemes].fg = 7;
  themes[nthemes].bg = 0;
  nthemes++;

  if ((fd = open(THEMES_DIR, O_RDONLY)) < 0) {
    printf(2, "theme: cannot open %s\n", THEMES_DIR);
    return 0;
  }

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0 || de.name[0] == '.')
      continue;
    if (nthemes == MAX_THEMES) {
      printf(2, "theme: too many theme files\n");
      close(fd);
      return -1;
    }

    strcpy(path, THEMES_DIR);
    p = path + strlen(path);
    *p++ = '/';
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    if (stat(path, &st) < 0 || st.type != T_FILE)
      continue;
    if (themeparse(path, &themes[nthemes]) == 0)
      nthemes++;
  }

  close(fd);
  sortthemes();
  return 0;
}

static int themeapply(char* name) {
  for (int i = 0; i < nthemes; i++) {
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
  if (loadthemes() < 0)
    exit();

  if (argc < 2 || strcmp(argv[1], "list") == 0) {
    printf(1, "themes:\n");
    for (int i = 0; i < nthemes; i++)
      printf(1, "- %s\n", themes[i].name);
    exit();
  }

  if (themeapply(argv[1]) < 0) {
    printf(2, "theme: unknown theme '%s'\n", argv[1]);
    printf(2, "run 'ohmyxv6 theme' to see available themes\n");
  }

  exit();
}
