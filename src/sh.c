// Shell.

#include "user.h"
#include "fcntl.h"
#include "stat.h"
#include "fs.h"

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10
#define MAX_JUMP_ENTRIES 50

#define TOOL_MAX_LENGTH 16

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char* argv[MAXARGS];
  char* eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd* cmd;
  char* file;
  char* efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd* left;
  struct cmd* right;
};

struct listcmd {
  int type;
  struct cmd* left;
  struct cmd* right;
};

struct backcmd {
  int type;
  struct cmd* cmd;
};

struct {
  char name[32];
  char path[64];
} jump_table[MAX_JUMP_ENTRIES];

int jump_count;

int fork1(void); // Fork but panics on failure.
void panic(char*);
struct cmd* parsecmd(char*);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
// Execute cmd.  Never returns.
void runcmd(struct cmd* cmd) {
  int p[2];
  struct backcmd* bcmd;
  struct execcmd* ecmd;
  struct listcmd* lcmd;
  struct pipecmd* pcmd;
  struct redircmd* rcmd;

  if (cmd == 0)
    exit();

  switch (cmd->type) {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if (ecmd->argv[0] == 0)
      exit();

    struct stat st;
    if (stat(ecmd->argv[0], &st) != -1) {
      exec(ecmd->argv[0], ecmd->argv);
      break;
    }

    int sz = strlen(ecmd->argv[0]);
    char* bincmd = malloc(sz + 6);
    memmove(bincmd, "/bin/", 5);
    memmove(bincmd + 5, ecmd->argv[0], sz + 1);
    if (stat(bincmd, &st) != -1) {
      exec(bincmd, ecmd->argv);
      break;
    }

    free(bincmd);
    printf(2, "sh: unknown command\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0) {
      printf(2, "open %s failed\n", rcmd->file);
      exit();
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0) {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0) {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit();
}
#pragma GCC diagnostic pop

struct command {
  struct command* next;
  char* content;
};

static struct history {
  int fd;
  int pos;
  int topped;
  struct command* prev_end;
  struct command* prev;
  struct command* curr;
  struct command* next;
} history = {
    .fd = -1,
    .pos = -1,
    .topped = 0,
    .prev_end = 0x00,
    .prev = 0x00,
    .curr = 0x00,
    .next = 0x00,
};

void load_history_command(char* content) {
  int len = strlen(content);
  char* con = malloc(len + 1);
  struct command* com = malloc(sizeof(struct command));

  memmove(con, content, len + 1);
  com->content = con;
  com->next = 0x00;

  if (history.prev == 0) {
    history.prev = com;
    history.prev_end = com;
  } else {
    history.prev_end->next = com;
    history.prev_end = com;
  }
}

void load_history() {
  const int DEFAULT_LOAD = 255;
  char buf[DEFAULT_LOAD + 1];

  int start_pos = history.pos;
  if ((history.pos = fseek(history.fd, history.pos - DEFAULT_LOAD, 0)) == -1) {
    printf(2, "failed to seek to history loading point\n");
    exit();
  }
  if (history.pos == 0)
    history.topped = 1;

  int nread;
  if ((nread = read(history.fd, buf, start_pos - history.pos)) == -1) {
    printf(2, "failed to read history");
    exit();
  }

  if (nread == 0)
    return;

  int pos = nread - 2; // -0x00, -\n
  buf[pos + 1] = 0x00; // clear \n
  buf[nread] = 0x00;   // terminate

  int last_nl = nread - 1;
  for (int i = 0; i < nread - 2; ++i) {
    if (buf[pos] == '\n') {
      last_nl = pos;
      buf[pos] = 0x00;
      load_history_command(&buf[pos + 1]);
    }
    --pos;
  }

  if (history.topped) {
    load_history_command(buf);
    return;
  }

  history.pos += last_nl + 1;
}

void init_history() {
  if ((history.fd = open(".sh_history", O_CREATE | O_RDWR)) == -1) {
    printf(2, "failed to open history file");
    exit();
  }

  struct stat s;
  if (fstat(history.fd, &s) == -1) {
    printf(2, "failed to stat history file");
    exit();
  }

  history.pos = s.size;

  load_history();
}

void reset_command_position() {
  if (history.curr) {
    history.curr->next = history.prev;
    history.prev = history.curr;
    history.curr = 0x00;
  }

  while (history.next) {
    struct command* com = history.next;
    history.next = history.next->next;
    com->next = history.prev;
    history.prev = com;
  }
}

void log_command(char* buf) {
  reset_command_position();

  // find size, remove newline
  char* pos = buf;
  while (*pos != 0x0)
    ++pos;
  int sz = pos - buf;
  buf[sz - 1] = 0x00;

  // avoid sequentially duplicate entries
  if (history.prev && (strcmp(history.prev->content, buf) == 0))
    return;

  // FILE WRITING
  fseek(history.fd, 0, 2);
  buf[sz - 1] = '\n'; // put the newline back for a moment
  write(history.fd, buf, pos - buf);

  // INTERNAL STATE WRITING
  buf[sz - 1] = 0x00; // remove the newline again
  int len = strlen(buf);
  char* con = malloc(len + 1);
  struct command* com = malloc(sizeof(struct command));

  memmove(con, buf, len + 1);
  com->content = con;

  if (history.prev == 0) {
    com->next = 0x00;
    history.prev = com;
    history.prev_end = com;
  } else {
    com->next = history.prev;
    history.prev = com;
  }
}

char* get_prev_command() {
  if (history.prev == 0x00) {
    if (!history.topped) {
      load_history();
      return get_prev_command();
    }
    return 0x00;
  }

  if (history.curr) {
    history.curr->next = history.next;
    history.next = history.curr;
  }

  history.curr = history.prev;
  history.prev = history.prev->next;
  history.curr->next = 0x00;

  return history.curr->content;
}

char* get_next_command() {
  if (history.next == 0x00) {
    if (history.curr) {
      history.curr->next = history.prev;
      history.prev = history.curr;
      history.curr = 0x00;
    }

    return 0x00;
  }

  if (history.curr) {
    history.curr->next = history.prev;
    history.prev = history.curr;
  }

  history.curr = history.next;
  history.next = history.next->next;
  history.curr->next = 0x00;

  return history.curr->content;
}

int autocomplete(char* buf, int n) {
  int fd;
  struct dirent de;

  if ((fd = open(".", O_RDONLY)) < 0)
    return n;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    int last_word_start = n;
    while (last_word_start > 0 && buf[last_word_start - 1] != ' ')
      last_word_start--;

    int current_len = n - last_word_start;
    if (current_len <= 0)
      continue;

    char saved_char = de.name[current_len];
    de.name[current_len] = '\0';

    if (strcmp(buf + last_word_start, de.name) == 0) {
      de.name[current_len] = saved_char;
      char* suffix = de.name + current_len;
      int suffix_len = strlen(suffix);
      write(0, suffix, suffix_len);
      memmove(buf + n, suffix, suffix_len);

      close(fd);
      return n + suffix_len;
    }
    de.name[current_len] = saved_char;
  }
  close(fd);
  return n;
}

int getcmd(char* buf, int nbuf) {
  printf(2, "$ ");
  memset(buf, 0, nbuf);

  int n;
  char c;
  for (n = 0; n < nbuf;) {
    if (read(0, &c, 1) != 1)
      return -1;

    if (c == 0x10) { // ctrl-p
      char* com = get_prev_command();
      if (com != 0x00) {
        kbddecoy(com);
      } else if (history.curr) {
        kbddecoy(history.curr->content);
      }
      continue;
    }
    if (c == 0x0e) { // ctrl-n
      char* com = get_next_command();
      if (com != 0x00)
        kbddecoy(com);
      continue;
    }

    if (c == '\t') {
      n = autocomplete(buf, n);
      memset(buf + n, 0, nbuf - n);
      continue;
    }

    if (c == '\b' || c == 127 || c == '\x7f') {
      if (n > 0) {
        n--;
        memset(buf + n, 0, nbuf - n);
        write(1, "\b \b", 3);
      }
      continue;
    }

    buf[n++] = c;

    if (c == '\n' || c == '\r')
      break;
  }

  buf[n] = 0x00;

  if (buf[0] == 0) // EOF
    return -1;
  return 0;
}

void cd(char* command) {
  command[strlen(command) - 1] = 0; // chop \n
  if (chdir(command + 3) < 0)
    printf(2, "cannot cd %s\n", command + 3);
}

void jump(char* command) {
  command[strlen(command) - 1] = 0; // chop \n
  char* filename = command + 5;

  for (int i = 0; i < jump_count; i++) {
    if (strcmp(filename, jump_table[i].name) == 0) {
      if (chdir(jump_table[i].path) < 0) {
        printf(2, "jump failed...could not reach %s\n", jump_table[i].path);
      }
      return;
    }
  }

  printf(2, "cannot find %s, not in index\n", filename);
}

int init_jump_table() {
  int fd = open(".jump_index", O_RDONLY);

  if (fd < 0)
    return -1;
  char c;
  int i = 0;
  jump_count = 0;

  while (read(fd, &c, 1) > 0 && jump_count < MAX_JUMP_ENTRIES) {
    i = 0;
    while (c != ' ' && c != '\n') {
      jump_table[jump_count].name[i++] = c;
      if (read(fd, &c, 1) <= 0)
        break;
    }
    jump_table[jump_count].name[i] = '\0';

    // Skip the space
    if (c == ' ')
      read(fd, &c, 1);

    // 2. Read the path (until newline)
    i = 0;
    while (c != '\n' && c != '\r') {
      jump_table[jump_count].path[i++] = c;
      if (read(fd, &c, 1) <= 0)
        break;
    }
    jump_table[jump_count].path[i] = '\0';
    jump_count++;
  }

  close(fd);
  return 0;
}

void rfsh(char* _ignored) {
  if (init_jump_table() == -1) {
    printf(2, "sh: failed to refresh jump table\n");
  } else {
    printf(2, "sh: jump table updated\n");
  }
}

enum TOOLS {
  CD,
  JUMP,
  RFSH,
  NUM_TOOLS, // INVARIANT: always last
};

struct tool {
  char name[TOOL_MAX_LENGTH];
  void (*handler)(char*);
  char followup;
} tools[NUM_TOOLS] = {
    [CD] = {"cd", cd, ' '},
    [JUMP] = {"jump", jump, ' '},
    [RFSH] = {"rfsh", rfsh, '\n'},
};

int check_tools(char* command) {
  for (int i = 0; i < NUM_TOOLS; ++i) {
    struct tool t = tools[i];
    char* name = t.name;
    char* comm = command;

    // ensure the first few characters match tool name
    while (*name && *comm) {
      if (*name++ != *comm++)
        goto next;
    }

    // ensure the followup matches
    if (*comm == t.followup) {
      t.handler(command);
      return 1;
    }

  next:
    continue;
  }

  return 0;
}

int main(void) {
  static char buf[100];
  int fd;
  // Ensure that three file descriptors are open.
  while ((fd = open("/dev/console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  // feature initialization
  init_jump_table();
  init_history();

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (check_tools(buf))
      continue;

    if (fork1() == 0)
      runcmd(parsecmd(buf));

    wait();
    log_command(buf);
  }
  exit();
}

void panic(char* s) {
  printf(2, "%s\n", s);
  exit();
}

int fork1(void) {
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

// PAGEBREAK!
//  Constructors

struct cmd*
execcmd(void) {
  struct execcmd* cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd* subcmd, char* file, char* efile, int mode, int fd) {
  struct redircmd* cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd* left, struct cmd* right) {
  struct pipecmd* cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd* left, struct cmd* right) {
  struct listcmd* cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd* subcmd) {
  struct backcmd* cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
// PAGEBREAK!
//  Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char** ps, char* es, char** q, char** eq) {
  char* s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s) {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>') {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char** ps, char* es, char* toks) {
  char* s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd* parseline(char**, char*);
struct cmd* parsepipe(char**, char*);
struct cmd* parseexec(char**, char*);
struct cmd* nulterminate(struct cmd*);

struct cmd*
parsecmd(char* s) {
  char* es;
  struct cmd* cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    printf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char** ps, char* es) {
  struct cmd* cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char** ps, char* es) {
  struct cmd* cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd* cmd, char** ps, char* es) {
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok) {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char** ps, char* es) {
  struct cmd* cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char** ps, char* es) {
  char *q, *eq;
  int tok, argc;
  struct execcmd* cmd;
  struct cmd* ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd* cmd) {
  int i;
  struct backcmd* bcmd;
  struct execcmd* ecmd;
  struct listcmd* lcmd;
  struct pipecmd* pcmd;
  struct redircmd* rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
