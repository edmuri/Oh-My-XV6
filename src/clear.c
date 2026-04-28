#include "user.h"
#include "console.h"

int main(int argc, char* argv[]) {
  if (argc > 1) {
    printf(2, "clear: too many arguments\n");
    exit();
  }

  if (ioctl(1, CONSOLE_CLEAR, 0) < 0)
    printf(2, "clear: failed\n");

  exit();
}
