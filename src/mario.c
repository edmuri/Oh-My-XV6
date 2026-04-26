// Simple Super Mario demo for xv6 VGA Mode 13h.
#include "types.h"
#include "user.h"
#include "fcntl.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

int fd;
unsigned char framebuffer[SCREEN_SIZE];

// 16x16 sprite; 0 means transparent/background.
unsigned char mario_sprite[16 * 16] = {
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 3, 3, 3, 2, 2, 3, 2, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 2, 3, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 3, 2, 3, 3, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0,
    0, 3, 3, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 3, 1, 1, 3, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 3, 1, 1, 3, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 0, 0, 0, 0,
    2, 2, 1, 3, 2, 3, 3, 2, 3, 1, 2, 2, 0, 0, 0, 0,
    2, 2, 2, 3, 3, 3, 3, 3, 3, 2, 2, 2, 0, 0, 0, 0,
    2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 0, 0, 0, 0,
    0, 0, 3, 3, 3, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0,
    0, 3, 3, 3, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0,
    3, 3, 3, 3, 0, 0, 0, 0, 3, 3, 3, 3, 0, 0, 0, 0};

void set_palette(int index, int r, int g, int b) {
  if (ioctl(fd, 2, (index << 24) | (r << 16) | (g << 8) | b) < 0)
    printf(2, "Error setting palette color.\n");
}

void init_palette(void) {
  set_palette(0, 0, 0, 0);    // transparent/background
  set_palette(1, 63, 0, 0);   // red
  set_palette(2, 63, 50, 30); // skin tone
  set_palette(3, 0, 0, 63);   // blue (overalls)
  set_palette(4, 0, 40, 0);   // green (ground)
  set_palette(5, 30, 30, 63); // sky blue
  set_palette(6, 60, 60, 10); // yellow/gold (coins)
  set_palette(7, 25, 15, 0);  // brown (bricks/obstacles)
}

void draw_pixel(int x, int y, unsigned char color) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    return;
  framebuffer[y * SCREEN_WIDTH + x] = color;
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      draw_pixel(x + i, y + j, color);
}

void draw_mario(int x, int y) {
  for (int row = 0; row < 16; row++) {
    for (int col = 0; col < 16; col++) {
      unsigned char c = mario_sprite[row * 16 + col];
      if (c)
        draw_pixel(x + col, y + row, c);
    }
  }
}

// Simple obstacle/coin structs for a runner-style loop.
struct entity {
  int x, y, w, h;
  int active;
};

#define MAX_OBS 8
#define MAX_COINS 8

static struct entity obstacles[MAX_OBS];
static struct entity coins[MAX_COINS];
static unsigned int rng_state = 1;
static int best_score = 0;
static int best_level = 0;

// 3x5 pixel glyphs for digits 0-9 (5 rows of 3 bits each).
static const unsigned char digit_bits[10][5] = {
    {0x07, 0x05, 0x05, 0x05, 0x07}, // 0
    {0x02, 0x06, 0x02, 0x02, 0x07}, // 1
    {0x07, 0x01, 0x07, 0x04, 0x07}, // 2
    {0x07, 0x01, 0x07, 0x01, 0x07}, // 3
    {0x05, 0x05, 0x07, 0x01, 0x01}, // 4
    {0x07, 0x04, 0x07, 0x01, 0x07}, // 5
    {0x07, 0x04, 0x07, 0x05, 0x07}, // 6
    {0x07, 0x01, 0x01, 0x01, 0x01}, // 7
    {0x07, 0x05, 0x07, 0x05, 0x07}, // 8
    {0x07, 0x05, 0x07, 0x01, 0x07}, // 9
};

static void draw_digit(int x, int y, int d, unsigned char color) {
  if (d < 0 || d > 9)
    return;
  for (int row = 0; row < 5; row++) {
    unsigned char bits = digit_bits[d][row];
    for (int col = 0; col < 3; col++) {
      if (bits & (1 << (2 - col))) {
        draw_pixel(x + col, y + row, color);
      }
    }
  }
}

static void draw_number(int x, int y, int value, unsigned char color) {
  if (value == 0) {
    draw_digit(x, y, 0, color);
    return;
  }
  int digits[10];
  int n = 0;
  while (value > 0 && n < 10) {
    digits[n++] = value % 10;
    value /= 10;
  }
  for (int i = n - 1; i >= 0; i--) {
    draw_digit(x, y, digits[i], color);
    x += 4; // spacing between digits
  }
}

static const unsigned char letter_bits[][5] = {
    // A-Z 3x5 uppercase, in order
    {0x07, 0x05, 0x07, 0x05, 0x05}, // A
    {0x06, 0x05, 0x06, 0x05, 0x06}, // B
    {0x07, 0x04, 0x04, 0x04, 0x07}, // C
    {0x06, 0x05, 0x05, 0x05, 0x06}, // D
    {0x07, 0x04, 0x06, 0x04, 0x07}, // E
    {0x07, 0x04, 0x06, 0x04, 0x04}, // F
    {0x07, 0x04, 0x05, 0x05, 0x07}, // G
    {0x05, 0x05, 0x07, 0x05, 0x05}, // H
    {0x07, 0x02, 0x02, 0x02, 0x07}, // I
    {0x07, 0x01, 0x01, 0x05, 0x07}, // J
    {0x05, 0x05, 0x06, 0x05, 0x05}, // K
    {0x04, 0x04, 0x04, 0x04, 0x07}, // L
    {0x07, 0x07, 0x05, 0x05, 0x05}, // M
    {0x07, 0x07, 0x07, 0x05, 0x05}, // N
    {0x07, 0x05, 0x05, 0x05, 0x07}, // O
    {0x07, 0x05, 0x07, 0x04, 0x04}, // P
    {0x07, 0x05, 0x07, 0x01, 0x01}, // Q
    {0x07, 0x05, 0x07, 0x05, 0x05}, // R
    {0x07, 0x04, 0x07, 0x01, 0x07}, // S
    {0x07, 0x02, 0x02, 0x02, 0x02}, // T
    {0x05, 0x05, 0x05, 0x05, 0x07}, // U
    {0x05, 0x05, 0x05, 0x05, 0x02}, // V
    {0x05, 0x05, 0x05, 0x07, 0x07}, // W
    {0x05, 0x05, 0x02, 0x05, 0x05}, // X
    {0x05, 0x05, 0x02, 0x02, 0x02}, // Y
    {0x07, 0x01, 0x02, 0x04, 0x07}, // Z
};

static void draw_char(int x, int y, char c, unsigned char color) {
  if (c < 'A' || c > 'Z')
    return;
  int idx = c - 'A';
  for (int row = 0; row < 5; row++) {
    unsigned char bits = letter_bits[idx][row];
    for (int col = 0; col < 3; col++) {
      if (bits & (1 << (2 - col))) {
        draw_pixel(x + col, y + row, color);
      }
    }
  }
}

static void draw_string(int x, int y, const char* s, unsigned char color) {
  for (; *s; s++) {
    char c = *s;
    if (c >= 'a' && c <= 'z')
      c -= 32; // to uppercase
    draw_char(x, y, c, color);
    x += 4;
  }
}

struct level_config {
  int runner_speed;
  int move_step;
  int jump_impulse;
  int obstacle_interval;
  int coin_interval;
  int obstacle_gap;
};

static struct level_config levels[] = {
    // runner_speed, move_step, jump_impulse, obstacle_interval, coin_interval, obstacle_gap
    {2, 2, -18, 90, 110, 140}, // easy
    {3, 2, -16, 70, 90, 100},  // normal
    {4, 3, -14, 55, 70, 70},   // hard
};
static int current_level = 0;
static struct level_config active;

static unsigned int rnd(void) {
  rng_state = rng_state * 1103515245 + 12345;
  return (rng_state >> 16) & 0x7fff;
}

static void set_level(int idx) {
  if (idx < 0 || idx >= (int)(sizeof(levels) / sizeof(levels[0])))
    return;
  current_level = idx;
  active = levels[idx];
  if (current_level > best_level)
    best_level = current_level;
}

static void clear_entities(struct entity* arr, int len) {
  for (int i = 0; i < len; i++)
    arr[i].active = 0;
}

static void spawn_obstacle(void) {
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obstacles[i].active) {
      obstacles[i].active = 1;
      obstacles[i].w = 14;
      obstacles[i].h = 14 + (rnd() % 6);
      obstacles[i].x = SCREEN_WIDTH + active.obstacle_gap + (rnd() % 40);
      obstacles[i].y = 160 - obstacles[i].h;
      return;
    }
  }
}

static void spawn_coin_line(void) {
  int base_y = 120 + (rnd() % 20);
  for (int k = 0; k < MAX_COINS; k++) {
    if (!coins[k].active) {
      coins[k].active = 1;
      coins[k].w = coins[k].h = 6;
      coins[k].x = SCREEN_WIDTH + 20 * (rnd() % 4);
      coins[k].y = base_y - (k % 3) * 10;
      return;
    }
  }
}

static int intersects(struct entity* a, int x, int y, int w, int h) {
  if (!a->active)
    return 0;
  int pad = 2; // shrink hitboxes a bit to be more forgiving
  if (x + w - pad < a->x)
    return 0;
  if (a->x + a->w < x + pad)
    return 0;
  if (y + h - pad < a->y)
    return 0;
  if (a->y + a->h < y + pad)
    return 0;
  return 1;
}

static void update_entities(int speed) {
  for (int i = 0; i < MAX_OBS; i++) {
    if (obstacles[i].active) {
      obstacles[i].x -= speed;
      if (obstacles[i].x + obstacles[i].w < 0)
        obstacles[i].active = 0;
    }
  }
  for (int i = 0; i < MAX_COINS; i++) {
    if (coins[i].active) {
      coins[i].x -= speed;
      if (coins[i].x + coins[i].w < 0)
        coins[i].active = 0;
    }
  }
}

static void draw_world(int score) {
  // sky
  memset(framebuffer, 5, SCREEN_SIZE);
  // ground stripes
  draw_rect(0, 160, SCREEN_WIDTH, 40, 4);
  for (int i = 0; i < SCREEN_WIDTH; i += 8)
    draw_rect(i, 170, 4, 4, 7); // small texture flecks

  // status numbers with tiny labels
  draw_string(4, 2, "COINS", 6);
  draw_number(4, 10, score, 6);
  draw_string(4, 18, "BEST", 1);
  draw_number(4, 26, best_score, 1);

  for (int i = 0; i < MAX_OBS; i++)
    if (obstacles[i].active)
      draw_rect(obstacles[i].x, obstacles[i].y, obstacles[i].w, obstacles[i].h, 7);
  for (int i = 0; i < MAX_COINS; i++)
    if (coins[i].active)
      draw_rect(coins[i].x, coins[i].y, coins[i].w, coins[i].h, 6);

  // level indicator and best level indicator
  draw_rect(SCREEN_WIDTH - 30, 4, 6, 6, 1 + current_level); // current level
  if (best_level > 0)
    draw_rect(SCREEN_WIDTH - 20, 4, 6, 6, 1 + best_level); // best level color
}

// Push framebuffer to the display device.
void render(void) {
  close(fd);
  fd = open("display", O_WRONLY);
  if (fd < 0) {
    printf(2, "Error reopening display\n");
    exit();
  }
  write(fd, framebuffer, SCREEN_SIZE);
}

int main(int argc, char** argv) {
  fd = open("display", O_WRONLY);
  if (fd < 0) {
    printf(2, "display not found\n");
    exit();
  }

  if (ioctl(fd, 1, 0x13) < 0) {
    printf(2, "error: ioctl to switch to VGA mode failed.\n");
    exit();
  }

  init_palette();

  if (ioctl(0, 3, 1) < 0) {
    printf(2, "error: ioctl to enable raw mode failed.\n");
    exit();
  }

  int mario_x = 80;
  int mario_y = 0x90;
  int velocity_y = 0;
  int is_jumping = 0;
  int score = 0;
  int frame = 0;
  int paused = 0;
  set_level(0);
  int runner_speed = active.runner_speed;
  int move_step = active.move_step;
  int jump_impulse = active.jump_impulse;

  if (argc > 1) {
    int s = atoi(argv[1]);
    if (s > 0)
      runner_speed = s;
  }
  if (argc > 2) {
    int j = atoi(argv[2]);
    if (j < 0)
      jump_impulse = j;
  }

  clear_entities(obstacles, MAX_OBS);
  clear_entities(coins, MAX_COINS);

  printf(1, "Controls: A/D move, W jump, R restart when paused, Q quit, +/- speed, [ ] jump, 1/2/3 levels\n");

  for (;;) {
    if (paused) {
      draw_world(score);
      draw_mario(mario_x, mario_y);
      render();
      int keyp = ioctl(0, 4, 0);
      if (keyp >= 0) {
        char in = (char)keyp;
        if (in == 'r') {
          // restart: reset state and make it a bit easier
          mario_x = 80;
          mario_y = 0x90;
          velocity_y = 0;
          is_jumping = 0;
          score = 0;
          frame = 0;
          clear_entities(obstacles, MAX_OBS);
          clear_entities(coins, MAX_COINS);
          paused = 0;
        } else if (in == 'q') {
          break;
        }
        if (in == '1') {
          set_level(0);
          runner_speed = active.runner_speed;
          move_step = active.move_step;
          jump_impulse = active.jump_impulse;
        }
        if (in == '2') {
          set_level(1);
          runner_speed = active.runner_speed;
          move_step = active.move_step;
          jump_impulse = active.jump_impulse;
        }
        if (in == '3') {
          set_level(2);
          runner_speed = active.runner_speed;
          move_step = active.move_step;
          jump_impulse = active.jump_impulse;
        }
      }
      sleep(2);
      continue;
    }

    frame++;

    if (frame % active.obstacle_interval == 0)
      spawn_obstacle();
    if (frame % active.coin_interval == 0)
      spawn_coin_line();

    draw_world(score);

    int key = ioctl(0, 4, 0); // non-blocking key fetch
    if (key >= 0) {
      char input = (char)key;
      if (input == 'a')
        mario_x -= move_step;
      if (input == 'd')
        mario_x += move_step;
      if (input == 'w' && !is_jumping) {
        velocity_y = jump_impulse;
        is_jumping = 1;
      }
      if (input == '+') {
        if (runner_speed < 8)
          runner_speed++;
      }
      if (input == '-') {
        if (runner_speed > 1)
          runner_speed--;
      }
      if (input == '[') {
        if (jump_impulse > -32)
          jump_impulse -= 1;
      }
      if (input == ']') {
        if (jump_impulse < -8)
          jump_impulse += 1;
      }
      if (input == '1') {
        set_level(0);
        runner_speed = active.runner_speed;
        move_step = active.move_step;
        jump_impulse = active.jump_impulse;
      }
      if (input == '2') {
        set_level(1);
        runner_speed = active.runner_speed;
        move_step = active.move_step;
        jump_impulse = active.jump_impulse;
      }
      if (input == '3') {
        set_level(2);
        runner_speed = active.runner_speed;
        move_step = active.move_step;
        jump_impulse = active.jump_impulse;
      }
      if (input == 'q')
        break;
    }

    mario_y += velocity_y;
    velocity_y += 1; // gravity

    if (mario_y > 0x8f) { // landed
      mario_y = 0x90;
      velocity_y = 0;
      is_jumping = 0;
    }

    update_entities(runner_speed);

    // collisions with coins
    for (int i = 0; i < MAX_COINS; i++) {
      if (intersects(&coins[i], mario_x, mario_y, 16, 16)) {
        coins[i].active = 0;
        score++;
        if (score > best_score)
          best_score = score;
      }
    }
    // collision with obstacles: end the game loop
    for (int i = 0; i < MAX_OBS; i++) {
      if (intersects(&obstacles[i], mario_x, mario_y, 16, 16)) {
        paused = 1;
        break;
      }
    }

    draw_mario(mario_x, mario_y);
    render();
    sleep(1);
  }

  ioctl(0, 3, 0);    // disable raw mode
  ioctl(fd, 1, 0x3); // back to text
  close(fd);
  exit();
  return 0;
}
