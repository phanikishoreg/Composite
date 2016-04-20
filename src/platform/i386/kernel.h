#ifndef KERNEL_H
#define KERNEL_H

#include "shared/cos_config.h"
#include "shared/cos_types.h"
#include "chal.h"
//#include "multiboot.h"
#include "multiboot2.h"

#include "chal_asm_inc.h"

/* A not so nice way of oopsing */
#define die(fmt, ...) do {              \
    printk(fmt,##__VA_ARGS__);   \
    khalt();				\
} while(0)

#ifdef ENABLE_CONSOLE
void vga_clear(void);
void vga_puts(const char *s);
void console_init(void);
#endif

#ifdef ENABLE_SCREEN
void cls(void);
void putch(unsigned char c);
void puts(unsigned char *text);
void settextcolor(unsigned char forecolor, unsigned char backcolor);
void init_video(void);
#endif

#ifdef ENABLE_VGA
void vga_init (void);
void printv (const char *format, ...);
void settextcolor(unsigned char forecolor, unsigned char backcolor);
#endif

#ifdef ENABLE_SERIAL
void serial_init(void);
#endif

#ifdef ENABLE_TIMER
void timer_init(u32_t frequency);
#endif

void tss_init(void);
void idt_init(void);
void gdt_init(void);
void user_init(void);
void paging_init(void);
void kern_paging_map_init(void *pa);

//void printk(const char *fmt, ...);
int printk_register_handler(void (*handler)(const char *));

void khalt(void);

#endif /* KERNEL_H */
