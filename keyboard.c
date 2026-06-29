#include "keyboard.h"

#define KB_BUF_SIZE 256
static char kb_buf[KB_BUF_SIZE];
static int kb_head = 0, kb_tail = 0;

void keyboard_init(void)
{
    kb_head = kb_tail = 0;
}

void keyboard_isr(u8 scancode)
{
    static char map[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
        0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 0, 0, 0, 0,
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
    };
    static char shift_map[] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
        0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', 0, 0, 0, 0,
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
    };

    if (scancode & 0x80) return;

    char c;
    if (scancode < sizeof(map))
        c = map[scancode];
    else
        c = 0;

    if (c) {
        int next = (kb_head + 1) % KB_BUF_SIZE;
        if (next != kb_tail) {
            kb_buf[kb_head] = c;
            kb_head = next;
        }
    }
}

char kb_read(void)
{
    while (kb_head == kb_tail)
        __asm__ volatile("pause");
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}
