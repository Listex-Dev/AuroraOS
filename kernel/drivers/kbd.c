#include <aurora/drivers/kbd.h>
#include <aurora/drivers/tty.h>
#include <aurora/drivers/console.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/arch/x86_64/pic.h>
#include <aurora/io.h>
#include <aurora/printk.h>

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

static bool s_shift = false;
static bool s_ctrl = false;
static bool s_alt = false;
static bool s_caps = false;
static bool s_extended = false;

static const char kbd_us_unshifted[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char kbd_us_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

void kbd_inject_scancode(uint8_t scancode)
{
    if (scancode == 0xE0) {
        s_extended = true;
        return;
    }

    bool is_break = (scancode & 0x80) != 0;
    uint8_t code = scancode & 0x7F;

    /* Handle modifier key releases */
    if (is_break) {
        if (code == 0x2A || code == 0x36) {
            s_shift = false;
        } else if (code == 0x1D) {
            s_ctrl = false;
        } else if (code == 0x38) {
            s_alt = false;
        }
        s_extended = false;
        return;
    }

    /* Handle modifier key presses */
    if (code == 0x2A || code == 0x36) {
        s_shift = true;
        s_extended = false;
        return;
    }
    if (code == 0x1D) {
        s_ctrl = true;
        s_extended = false;
        return;
    }
    if (code == 0x38) {
        s_alt = true;
        s_extended = false;
        return;
    }
    if (code == 0x3A) {
        s_caps = !s_caps;
        s_extended = false;
        return;
    }

    /* Virtual Terminal switching: Alt + F1..F6 */
    if (s_alt && code >= 0x3B && code <= 0x40) {
        int target_vt = code - 0x3B;
        vt_switch(target_vt);
        s_extended = false;
        return;
    }

    /* Shift + PageUp / PageDown for console scrollback */
    if (s_shift && (code == 0x49 || (s_extended && code == 0x49))) {
        vt_scroll_view(vt_get_active(), 10);
        s_extended = false;
        return;
    }
    if (s_shift && (code == 0x51 || (s_extended && code == 0x51))) {
        vt_scroll_view(vt_get_active(), -10);
        s_extended = false;
        return;
    }

    /* Extended keys: cursor keys, navigation, etc. */
    if (s_extended) {
        s_extended = false;
        const char *seq = NULL;
        switch (code) {
            case 0x48: seq = "\033[A"; break; /* Up */
            case 0x50: seq = "\033[B"; break; /* Down */
            case 0x4D: seq = "\033[C"; break; /* Right */
            case 0x4B: seq = "\033[D"; break; /* Left */
            case 0x47: seq = "\033[H"; break; /* Home */
            case 0x4F: seq = "\033[F"; break; /* End */
            case 0x49: seq = "\033[5~"; break; /* Page Up */
            case 0x51: seq = "\033[6~"; break; /* Page Down */
            case 0x52: seq = "\033[2~"; break; /* Insert */
            case 0x53: seq = "\033[3~"; break; /* Delete */
            default: break;
        }
        if (seq) {
            struct tty *active_tty = tty_get_active_vt();
            if (active_tty) {
                while (*seq) {
                    tty_input_char(active_tty, *seq++);
                }
            }
            return;
        }
        return;
    }

    /* Translate scancode to character */
    char c = 0;
    if (code < 128) {
        bool uppercase = s_shift;
        if (s_caps) {
            /* If caps lock is on, invert shift for alpha keys */
            char base = kbd_us_unshifted[code];
            if (base >= 'a' && base <= 'z') {
                uppercase = !uppercase;
            }
        }
        c = uppercase ? kbd_us_shifted[code] : kbd_us_unshifted[code];
    }

    if (!c) {
        s_extended = false;
        return;
    }

    /* Handle Ctrl combinations */
    if (s_ctrl) {
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 1);
        } else if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 1);
        } else if (c == '[') {
            c = 27; /* ESC */
        }
    }

    /* Send input character to the active VT TTY */
    struct tty *active_tty = tty_get_active_vt();
    if (active_tty) {
        tty_input_char(active_tty, c);
    }

    s_extended = false;
}

static void kbd_irq_handler(interrupt_frame_t *frame)
{
    (void)frame;
    uint8_t status = inb(KBD_STATUS_PORT);
    if (status & 0x01) {
        uint8_t scancode = inb(KBD_DATA_PORT);
        kbd_inject_scancode(scancode);
    }
    pic_send_eoi(1);
}

void kbd_init(void)
{
    /* Register IRQ 1 handler (vector 33 in IDT) */
    isr_register_handler(PIC_MASTER_OFFSET + 1, kbd_irq_handler);

    /* Unmask IRQ 1 on master PIC */
    pic_clear_mask(1);

    printk("<6>[  0.000000] PS/2 Keyboard driver initialized on IRQ 1 (vector %d)\n",
           PIC_MASTER_OFFSET + 1);
}
