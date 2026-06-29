#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "io.h"

void keyboard_init(void);
void keyboard_isr(u8 scancode);
char kb_read(void);
int kb_available(void);

#endif
