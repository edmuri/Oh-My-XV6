// init: The initial user-level program

#include "user.h"
#include "fcntl.h"

char* argv[] = {"/bin/sh", 0};

int main(void) {
  int pid, wpid;

  if (open("/dev/console", O_RDWR) < 0) {
    mkdir("dev");
    mknod("/dev/console", 1, 1);
    open("/dev/console", O_RDWR);
  }
  dup(0); // stdout
  dup(0); // stderr

  if (open("/dev/display", O_RDWR) < 0) {
    mknod("/dev/display", 2, 1);
    open("/dev/display", O_RDWR);
  }

  pid = fork();
  if (pid == 0) {
    char* cargv[] = {"/bin/crawler", 0};
    exec("/bin/crawler", cargv);
    printf(1, "init: crawler failed\n");
    exit();
  }
  wait();

  for (;;) {
    printf(1, "init: starting sh\n");
    pid = fork();
    if (pid < 0) {
      printf(1, "init: fork failed\n");
      exit();
    }
    if (pid == 0) {
      if (chdir("/home") < 0) {
        mkdir("/home");
        if (chdir("/home") < 0)
          printf(1, "init: cannot enter /home\n");
      }
      exec("/bin/sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while ((wpid = wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
