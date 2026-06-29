#ifndef SERIAL_H
#define SERIAL_H

#include "io.h"

void putc(char c);
void puts(const char *s);
void puthex(u64 v);

#endif
