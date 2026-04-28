// CGA 16-color palette indices
#define CGA_BLACK 0
#define CGA_BLUE 1
#define CGA_GREEN 2
#define CGA_CYAN 3
#define CGA_RED 4
#define CGA_MAGENTA 5
#define CGA_BROWN 6
#define CGA_LIGHT_GRAY 7
#define CGA_DARK_GRAY 8
#define CGA_LIGHT_BLUE 9
#define CGA_LIGHT_GREEN 10
#define CGA_LIGHT_CYAN 11
#define CGA_LIGHT_RED 12
#define CGA_LIGHT_MAGENTA 13
#define CGA_YELLOW 14
#define CGA_WHITE 15

// console ioctl params
#define CONSOLE_SET_COLOR 0 // value = (bg<<4)|fg
#define CONSOLE_GET_COLOR 1 // value = (bg<<4)|fg
#define CONSOLE_GET_FG 2    // value = fg (0-15)
#define CONSOLE_GET_BG 3    // value = bg (0-15)
#define CONSOLE_SET_FG 4    // value = fg (0-15)
#define CONSOLE_SET_BG 5    // value = bg (0-15)
#define CONSOLE_RAW_TOGGLE 6
#define CONSOLE_RAW_READ 7
#define CONSOLE_REPAINT 8 // repaint all cells with current fg/bg
#define CONSOLE_FONT_BEGIN 9
#define CONSOLE_FONT_CANCEL 10
#define CONSOLE_FONT_DEFAULT 11
#define CONSOLE_FONT_BYTES 4096 // 256 glyphs * 16 bytes
