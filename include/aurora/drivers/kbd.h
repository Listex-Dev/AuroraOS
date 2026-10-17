#ifndef _AURORA_DRIVERS_KBD_H
#define _AURORA_DRIVERS_KBD_H

#include <aurora/types.h>

void kbd_init(void);
void kbd_inject_scancode(uint8_t scancode);

#endif /* _AURORA_DRIVERS_KBD_H */
