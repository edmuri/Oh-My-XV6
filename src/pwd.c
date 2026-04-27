#include "user.h"

int main(int argc, char* argv[]) {
  if (argc > 1) {
    printf(2, "pwd: too many arguments\n");
    exit();
  }
  char buf[512];
  if (getcwd(buf, sizeof(buf)) < 0) {
    printf(2, "pwd: failed\n");
    exit();
  }
  printf(1, "%s\n", buf);
  exit();
}
