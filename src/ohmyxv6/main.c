#include "types.h"
#include "user.h"

#define PLUGIN_PATH "/bin/ohmyxv6-"
#define MAX_PATH 64

int main(int argc, char* argv[]) {
  char path[MAX_PATH];

  if (argc < 2) {
    printf(2, "usage: ohmyxv6 <plugin> [args...]\n");
    exit();
  }

  if (strlen(PLUGIN_PATH) + strlen(argv[1]) >= MAX_PATH) {
    printf(2, "ohmyxv6: plugin name too long\n");
    exit();
  }

  strcpy(path, PLUGIN_PATH);
  strcpy(path + strlen(PLUGIN_PATH), argv[1]);

  // argv[1] becomes the plugin's argv[0], remaining args follow
  exec(path, argv + 1);

  printf(2, "ohmyxv6: plugin not found: %s\n", argv[1]);
  exit();
}
