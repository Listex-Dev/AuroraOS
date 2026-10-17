#include <aurora/drivers/console.h>
#include <aurora/io.h>
#include <aurora/string.h>

static struct vt_console g_vts[MAX_VTS];
static int g_active_vt = 0;
static volatile uint16_t *g_vga_mem = (volatile uint16_t *)VGA_MEM_VIRT;

static void update_hardware_cursor(int x, int y)
{
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void console_flush_active(void)
{
    struct vt_console *vt = &g_vts[g_active_vt];
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        g_vga_mem[i] = vt->buffer[i];
    }
    update_hardware_cursor(vt->cursor_x, vt->cursor_y);
}

void vt_clear(int vt_num)
{
    if (vt_num < 0 || vt_num >= MAX_VTS) return;
    struct vt_console *vt = &g_vts[vt_num];

    uint16_t blank = (uint16_t)(' ' | ((uint16_t)vt->cur_attr << 8));
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vt->buffer[i] = blank;
    }
    vt->cursor_x = 0;
    vt->cursor_y = 0;

    if (vt_num == g_active_vt) {
        console_flush_active();
    }
}

static void vt_scroll(struct vt_console *vt)
{
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)vt->cur_attr << 8));
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vt->buffer[y * VGA_WIDTH + x] = vt->buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        vt->buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
    vt->cursor_y = VGA_HEIGHT - 1;
}

static void vt_put_raw_char(struct vt_console *vt, int vt_num, char c)
{
    if (c == '\r') {
        vt->cursor_x = 0;
    } else if (c == '\n') {
        vt->cursor_x = 0;
        vt->cursor_y++;
        if (vt->cursor_y >= VGA_HEIGHT) {
            vt_scroll(vt);
        }
    } else if (c == '\b') {
        if (vt->cursor_x > 0) {
            vt->cursor_x--;
        }
    } else if (c == '\t') {
        int next_tab = (vt->cursor_x + 8) & ~7;
        if (next_tab >= VGA_WIDTH) {
            vt->cursor_x = 0;
            vt->cursor_y++;
            if (vt->cursor_y >= VGA_HEIGHT) {
                vt_scroll(vt);
            }
        } else {
            vt->cursor_x = next_tab;
        }
    } else if (c >= 32 && c <= 126) {
        int idx = vt->cursor_y * VGA_WIDTH + vt->cursor_x;
        uint16_t val = (uint16_t)((uint8_t)c | ((uint16_t)vt->cur_attr << 8));
        vt->buffer[idx] = val;
        if (vt_num == g_active_vt) {
            g_vga_mem[idx] = val;
        }

        vt->cursor_x++;
        if (vt->cursor_x >= VGA_WIDTH) {
            vt->cursor_x = 0;
            vt->cursor_y++;
            if (vt->cursor_y >= VGA_HEIGHT) {
                vt_scroll(vt);
                if (vt_num == g_active_vt) {
                    console_flush_active();
                }
            }
        }
    }

    if (vt_num == g_active_vt) {
        update_hardware_cursor(vt->cursor_x, vt->cursor_y);
    }
}

static void handle_sgr(struct vt_console *vt, int code)
{
    if (code == 0) {
        /* Reset */
        vt->cur_attr = VGA_ATTR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    } else if (code == 1) {
        /* Bold / Bright */
        uint8_t fg = vt->cur_attr & 0x0F;
        if (fg < 8) fg += 8;
        vt->cur_attr = (vt->cur_attr & 0xF0) | fg;
    } else if (code >= 30 && code <= 37) {
        /* Standard foreground colors */
        static const uint8_t ansi_to_vga_fg[8] = {
            VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_BROWN,
            VGA_COLOR_BLUE, VGA_COLOR_MAGENTA, VGA_COLOR_CYAN, VGA_COLOR_LIGHT_GREY
        };
        uint8_t fg = ansi_to_vga_fg[code - 30];
        if (vt->cur_attr & 0x08) fg += 8; /* preserve bold/bright */
        vt->cur_attr = (vt->cur_attr & 0xF0) | fg;
    } else if (code >= 40 && code <= 47) {
        /* Background colors */
        static const uint8_t ansi_to_vga_bg[8] = {
            VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_BROWN,
            VGA_COLOR_BLUE, VGA_COLOR_MAGENTA, VGA_COLOR_CYAN, VGA_COLOR_LIGHT_GREY
        };
        uint8_t bg = ansi_to_vga_bg[code - 40];
        vt->cur_attr = (vt->cur_attr & 0x0F) | ((bg & 0x7) << 4);
    }
}

static void handle_csi_cmd(struct vt_console *vt, int vt_num, char cmd)
{
    int p1 = (vt->param_count > 0) ? vt->params[0] : 0;
    int p2 = (vt->param_count > 1) ? vt->params[1] : 0;

    switch (cmd) {
        case 'H':
        case 'f': {
            /* Cursor position: 1-indexed */
            int row = (p1 > 0) ? p1 - 1 : 0;
            int col = (p2 > 0) ? p2 - 1 : 0;
            if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
            if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
            vt->cursor_y = row;
            vt->cursor_x = col;
            break;
        }
        case 'A': {
            /* Cursor Up */
            int n = (p1 > 0) ? p1 : 1;
            vt->cursor_y = (vt->cursor_y >= n) ? vt->cursor_y - n : 0;
            break;
        }
        case 'B': {
            /* Cursor Down */
            int n = (p1 > 0) ? p1 : 1;
            vt->cursor_y += n;
            if (vt->cursor_y >= VGA_HEIGHT) vt->cursor_y = VGA_HEIGHT - 1;
            break;
        }
        case 'C': {
            /* Cursor Forward */
            int n = (p1 > 0) ? p1 : 1;
            vt->cursor_x += n;
            if (vt->cursor_x >= VGA_WIDTH) vt->cursor_x = VGA_WIDTH - 1;
            break;
        }
        case 'D': {
            /* Cursor Back */
            int n = (p1 > 0) ? p1 : 1;
            vt->cursor_x = (vt->cursor_x >= n) ? vt->cursor_x - n : 0;
            break;
        }
        case 'J': {
            /* Erase in Display */
            if (p1 == 2) {
                vt_clear(vt_num);
            }
            break;
        }
        case 'K': {
            /* Erase in Line */
            uint16_t blank = (uint16_t)(' ' | ((uint16_t)vt->cur_attr << 8));
            int start_x = (p1 == 1) ? 0 : vt->cursor_x;
            int end_x = (p1 == 1) ? vt->cursor_x : VGA_WIDTH;
            if (p1 == 2) { start_x = 0; end_x = VGA_WIDTH; }

            for (int x = start_x; x < end_x; x++) {
                int idx = vt->cursor_y * VGA_WIDTH + x;
                vt->buffer[idx] = blank;
                if (vt_num == g_active_vt) {
                    g_vga_mem[idx] = blank;
                }
            }
            break;
        }
        case 'm': {
            /* Select Graphic Rendition */
            if (vt->param_count == 0) {
                handle_sgr(vt, 0);
            } else {
                for (int i = 0; i < vt->param_count; i++) {
                    handle_sgr(vt, vt->params[i]);
                }
            }
            break;
        }
        default:
            break;
    }

    if (vt_num == g_active_vt) {
        update_hardware_cursor(vt->cursor_x, vt->cursor_y);
    }
}

void vt_write(int vt_num, const char *buf, size_t count)
{
    if (vt_num < 0 || vt_num >= MAX_VTS || !buf) return;
    struct vt_console *vt = &g_vts[vt_num];

    for (size_t i = 0; i < count; i++) {
        char c = buf[i];

        switch (vt->state) {
            case ANSI_STATE_NORMAL:
                if (c == '\033') {
                    vt->state = ANSI_STATE_ESC;
                } else {
                    vt_put_raw_char(vt, vt_num, c);
                }
                break;

            case ANSI_STATE_ESC:
                if (c == '[') {
                    vt->state = ANSI_STATE_CSI;
                    vt->param_count = 0;
                    vt->in_param = false;
                    memset(vt->params, 0, sizeof(vt->params));
                } else {
                    vt->state = ANSI_STATE_NORMAL;
                    vt_put_raw_char(vt, vt_num, c);
                }
                break;

            case ANSI_STATE_CSI:
                if (c >= '0' && c <= '9') {
                    if (!vt->in_param && vt->param_count < 8) {
                        vt->param_count++;
                        vt->in_param = true;
                    }
                    if (vt->param_count > 0 && vt->param_count <= 8) {
                        vt->params[vt->param_count - 1] =
                            vt->params[vt->param_count - 1] * 10 + (c - '0');
                    }
                } else if (c == ';') {
                    vt->in_param = false;
                } else {
                    /* CSI Final Command Character */
                    handle_csi_cmd(vt, vt_num, c);
                    vt->state = ANSI_STATE_NORMAL;
                }
                break;
        }
    }
}

void vt_set_cursor(int vt_num, int x, int y)
{
    if (vt_num < 0 || vt_num >= MAX_VTS) return;
    struct vt_console *vt = &g_vts[vt_num];
    vt->cursor_x = (x >= 0 && x < VGA_WIDTH) ? x : 0;
    vt->cursor_y = (y >= 0 && y < VGA_HEIGHT) ? y : 0;
    if (vt_num == g_active_vt) {
        update_hardware_cursor(vt->cursor_x, vt->cursor_y);
    }
}

uint16_t *vt_get_buffer(int vt_num)
{
    if (vt_num < 0 || vt_num >= MAX_VTS) return NULL;
    return g_vts[vt_num].buffer;
}

void vt_switch(int vt_num)
{
    if (vt_num < 0 || vt_num >= MAX_VTS) return;
    g_active_vt = vt_num;
    console_flush_active();
}

int vt_get_active(void)
{
    return g_active_vt;
}

void console_init(void)
{
    /* Enable cursor with scan lines 14..15 */
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);

    for (int i = 0; i < MAX_VTS; i++) {
        g_vts[i].cursor_x = 0;
        g_vts[i].cursor_y = 0;
        g_vts[i].cur_attr = VGA_ATTR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        g_vts[i].state = ANSI_STATE_NORMAL;
        vt_clear(i);
    }

    g_active_vt = 0;
    console_flush_active();
}
