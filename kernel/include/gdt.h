#ifndef GDT_H
#define GDT_H

#include "io.h"

void gdt_setup_tss(void);
void enter_ring3(void (*entry)(void));

#endif
