#ifndef _AURORA_DRIVERS_CONSOLE_H
#define _AURORA_DRIVERS_CONSOLE_H

#include <aurora/types.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define MAX_VTS     6

#define VGA_MEM_PHYS 0x000B8000ULL
#define VGA_MEM_VIRT 0xFFFF8000000B8000ULL

/* VGA Colors */
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN   14
#define VGA_COLOR_WHITE         15

#define VGA_ATTR(fg, bg) ((uint8_t)((((bg) & 0x7) << 4) | ((fg) & 0xF)))

enum ansi_state {
    ANSI_STATE_NORMAL,
    ANSI_STATE_ESC,
    ANSI_STATE_CSI,
};

struct vt_console {
    uint16_t buffer[VGA_HEIGHT * VGA_WIDTH];
    int cursor_x;
    int cursor_y;
    uint8_t cur_attr;

    /* ANSI Parser state */
    enum ansi_state state;
    int params[8];
    int param_count;
    bool in_param;
};

void console_init(void);
void vt_switch(int vt_num);
int vt_get_active(void);
void vt_write(int vt_num, const char *buf, size_t count);
void vt_clear(int vt_num);
void vt_set_cursor(int vt_num, int x, int y);
uint16_t *vt_get_buffer(int vt_num);

#endif /* _AURORA_DRIVERS_CONSOLE_H */
