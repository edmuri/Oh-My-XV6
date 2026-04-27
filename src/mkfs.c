#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#include "param.h"

#ifndef static_assert
#define static_assert(a, b) \
  do {                      \
    switch (0)              \
    case 0:                 \
    case (a):;              \
  } while (0)
#endif

#define NINODES 200
#define MAXDIRS 64

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / (BSIZE * 8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;   // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks; // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode* ip);
void rsect(uint sec, void* buf);
ushort xshort(ushort x);
uint ialloc(ushort type);
void iappend(uint inum, void* p, int n);
uint mkdirp(char* path);
uint dirlookup(char* path);
void dirlink(uint parent, char* name, uint child);

struct dirref {
  char path[128];
  uint inum;
};

struct dirref dirs[MAXDIRS];
int ndirs;

uint dirlookup(char* path) {
  for (int i = 0; i < ndirs; i++) {
    if (strcmp(dirs[i].path, path) == 0)
      return dirs[i].inum;
  }
  return 0;
}

void dirlink(uint parent, char* name, uint child) {
  struct dirent de;

  if (strlen(name) > DIRSIZ) {
    fprintf(stderr, "mkfs: %s: directory name too long for xv6\n", name);
    exit(1);
  }

  bzero(&de, sizeof(de));
  de.inum = xshort(child);
  strncpy(de.name, name, DIRSIZ);
  iappend(parent, &de, sizeof(de));
}

uint mkdirp(char* path) {
  char partial[128];
  char component[DIRSIZ + 1];
  char* p;
  uint parent;

  if (path[0] == 0)
    return ROOTINO;

  partial[0] = 0;
  parent = ROOTINO;
  p = path;
  while (*p) {
    int len = 0;
    while (p[len] && p[len] != '/')
      len++;

    if (len > DIRSIZ) {
      fprintf(stderr, "mkfs: %.*s: directory name too long for xv6\n", len, p);
      exit(1);
    }

    memmove(component, p, len);
    component[len] = 0;

    if (partial[0])
      strcat(partial, "/");
    strcat(partial, component);

    uint found = dirlookup(partial);
    if (found) {
      parent = found;
    } else {
      uint child = ialloc(T_DIR);
      if (ndirs >= MAXDIRS) {
        fprintf(stderr, "mkfs: too many directories\n");
        exit(1);
      }

      dirlink(parent, component, child);
      dirlink(child, ".", child);
      dirlink(child, "..", parent);
      strcpy(dirs[ndirs].path, partial);
      dirs[ndirs++].inum = child;
      parent = child;
    }

    p += len;
    if (*p == '/')
      p++;
  }

  return parent;
}

// convert to intel byte order
ushort
xshort(ushort x) {
  ushort y;
  uchar* a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint xint(uint x) {
  uint y;
  uchar* a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

int main(int argc, char* argv[]) {
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if (argc < 2) {
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fsfd < 0) {
    perror(argv[1]);
    exit(1);
  }

  // 1 fs block = 1 disk sector
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = FSSIZE - nmeta;

  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2 + nlog);
  sb.bmapstart = xint(2 + nlog + ninodeblocks);

  printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
         nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

  freeblock = nmeta; // the first free block that we can allocate

  for (i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);
  strcpy(dirs[ndirs].path, "");
  dirs[ndirs++].inum = rootino;

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  // make the /bin directory
  uchar bin = ialloc(T_DIR);
  bzero(&de, sizeof(de));
  de.inum = xshort(bin);
  strncpy(de.name, "bin", DIRSIZ);
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(bin);
  strcpy(de.name, ".");
  iappend(bin, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(bin, &de, sizeof(de));
  strcpy(dirs[ndirs].path, "bin");
  dirs[ndirs++].inum = bin;

  for (i = 2; i < argc; i++) {
    char original[256], fsname[256], dirpath[256], name[DIRSIZ + 1];
    uint parent;
    char* slash;

    if ((fd = open(argv[i], 0)) < 0) {
      perror(argv[i]);
      exit(1);
    }

    strcpy(original, argv[i]);

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if (argv[i][0] == '_') {
      strcpy(dirpath, "bin");
      strcpy(fsname, argv[i] + 1);
    } else if ((slash = index(original, '/')) != 0 &&
               strncmp(slash + 1, "etc/", 4) == 0) {
      char package[DIRSIZ + 1];
      char* rest = slash + 5;
      int package_len = slash - original;

      if (package_len > DIRSIZ) {
        fprintf(stderr, "mkfs: %.*s: package name too long for xv6\n",
                package_len, original);
        exit(1);
      }

      memmove(package, original, package_len);
      package[package_len] = 0;
      slash = rindex(rest, '/');
      strcpy(dirpath, "etc/");
      strcat(dirpath, package);
      if (slash == 0) {
        strcpy(fsname, rest);
      } else {
        *slash = 0;
        strcat(dirpath, "/");
        strcat(dirpath, rest);
        strcpy(fsname, slash + 1);
      }
    } else if ((slash = rindex(original, '/')) != 0) {
      *slash = 0;
      strcpy(dirpath, original);
      strcpy(fsname, slash + 1);
    } else {
      strcpy(dirpath, "bin");
      strcpy(fsname, original);
    }

    if (strlen(fsname) > DIRSIZ) {
      fprintf(stderr, "mkfs: %s: file name too long for xv6\n", fsname);
      exit(1);
    }

    parent = mkdirp(dirpath);
    strcpy(name, fsname);

    inum = ialloc(T_FILE);

    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, name, DIRSIZ);
    iappend(parent, &de, sizeof(de));

    while ((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);

    close(fd);
  }

  for (i = 0; i < ndirs; i++) {
    rinode(dirs[i].inum, &din);
    off = xint(din.size);
    off = ((off / BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(dirs[i].inum, &din);
  }

  balloc(freeblock);

  exit(0);
}

void wsect(uint sec, void* buf) {
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (write(fsfd, buf, BSIZE) != BSIZE) {
    perror("write");
    exit(1);
  }
}

void winode(uint inum, struct dinode* ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode* dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void rinode(uint inum, struct dinode* ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode* dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void rsect(uint sec, void* buf) {
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (read(fsfd, buf, BSIZE) != BSIZE) {
    perror("read");
    exit(1);
  }
}

uint ialloc(ushort type) {
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void balloc(int used) {
  uchar buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < BSIZE * 8);
  bzero(buf, BSIZE);
  for (i = 0; i < used; i++) {
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void* xp, int n) {
  char* p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while (n > 0) {
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if (fbn < NDIRECT) {
      if (xint(din.addrs[fbn]) == 0) {
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if (xint(din.addrs[NDIRECT]) == 0) {
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if (indirect[fbn - NDIRECT] == 0) {
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn - NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
