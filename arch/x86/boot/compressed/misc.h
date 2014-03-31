#ifndef BOOT_COMPRESSED_MISC_H
#define BOOT_COMPRESSED_MISC_H

#undef CONFIG_PARAVIRT
#ifdef CONFIG_X86_32
#define _ASM_X86_DESC_H 1
#endif

#include <linux/linkage.h>
#include <linux/screen_info.h>
#include <linux/elf.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/boot.h>
#include <asm/bootparam.h>

#define BOOT_BOOT_H
#include "../ctype.h"

extern struct boot_params *real_mode;		
void __putstr(int error, const char *s);
#define putstr(__x)  __putstr(0, __x)
#define puts(__x)  __putstr(0, __x)

int cmdline_find_option(const char *option, char *buffer, int bufsize);
int cmdline_find_option_bool(const char *option);

extern int early_serial_base;
void console_init(void);

#endif
