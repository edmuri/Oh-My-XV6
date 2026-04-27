#include "types.h"
#include "user.h"

#define PLUGIN_PATH "/bin/ohmyxv6-"
#define MAX_PATH 64

static char* plugins[] = {
    "font",
    "theme",
};

#define NPLUGINS ((int)(sizeof(plugins) / sizeof(plugins[0])))

static void list_plugins(void) {
  printf(1, "plugins:\n");
  for (int i = 0; i < NPLUGINS; i++)
    printf(1, "- %s\n", plugins[i]);
}

int main(int argc, char* argv[]) {
  char path[MAX_PATH];

  if (argc < 2 || strcmp(argv[1], "list") == 0) {
    list_plugins();
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
