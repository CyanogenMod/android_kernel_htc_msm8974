/*
 * Copyright (C) Paul Mackerras 1997.
 *
 * Updates for PPC64 by Todd Inglett, Dave Engebretsen & Peter Bergner.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <stdarg.h>
#include <stddef.h>
#include "elf.h"
#include "page.h"
#include "string.h"
#include "stdio.h"
#include "ops.h"
#include "gunzip_util.h"
#include "reg.h"

static struct gunzip_state gzstate;

struct addr_range {
	void *addr;
	unsigned long size;
};

#undef DEBUG

static struct addr_range prep_kernel(void)
{
	char elfheader[256];
	void *vmlinuz_addr = _vmlinux_start;
	unsigned long vmlinuz_size = _vmlinux_end - _vmlinux_start;
	void *addr = 0;
	struct elf_info ei;
	int len;

	
	gunzip_start(&gzstate, vmlinuz_addr, vmlinuz_size);
	gunzip_exactly(&gzstate, elfheader, sizeof(elfheader));

	if (!parse_elf64(elfheader, &ei) && !parse_elf32(elfheader, &ei))
		fatal("Error: not a valid PPC32 or PPC64 ELF file!\n\r");

	if (platform_ops.image_hdr)
		platform_ops.image_hdr(elfheader);

	printf("Allocating 0x%lx bytes for kernel ...\n\r", ei.memsize);

	if (platform_ops.vmlinux_alloc) {
		addr = platform_ops.vmlinux_alloc(ei.memsize);
	} else {
		if ((unsigned long)_start < ei.loadsize)
			fatal("Insufficient memory for kernel at address 0!"
			       " (_start=%p, uncompressed size=%08lx)\n\r",
			       _start, ei.loadsize);

		if ((unsigned long)_end < ei.memsize)
			fatal("The final kernel image would overwrite the "
					"device tree\n\r");
	}

	
	printf("gunzipping (0x%p <- 0x%p:0x%p)...", addr,
	       vmlinuz_addr, vmlinuz_addr+vmlinuz_size);
	
	gunzip_discard(&gzstate, ei.elfoffset - sizeof(elfheader));
	len = gunzip_finish(&gzstate, addr, ei.loadsize);
	if (len != ei.loadsize)
		fatal("ran out of data!  only got 0x%x of 0x%lx bytes.\n\r",
				len, ei.loadsize);
	printf("done 0x%x bytes\n\r", len);

	flush_cache(addr, ei.loadsize);

	return (struct addr_range){addr, ei.memsize};
}

static struct addr_range prep_initrd(struct addr_range vmlinux, void *chosen,
				     unsigned long initrd_addr,
				     unsigned long initrd_size)
{
	if (_initrd_end > _initrd_start) {
		printf("Attached initrd image at 0x%p-0x%p\n\r",
		       _initrd_start, _initrd_end);
		initrd_addr = (unsigned long)_initrd_start;
		initrd_size = _initrd_end - _initrd_start;
	} else if (initrd_size > 0) {
		printf("Using loader supplied ramdisk at 0x%lx-0x%lx\n\r",
		       initrd_addr, initrd_addr + initrd_size);
	}

	
	if (! initrd_size)
		return (struct addr_range){0, 0};

	if (initrd_addr < vmlinux.size) {
		void *old_addr = (void *)initrd_addr;

		printf("Allocating 0x%lx bytes for initrd ...\n\r",
		       initrd_size);
		initrd_addr = (unsigned long)malloc(initrd_size);
		if (! initrd_addr)
			fatal("Can't allocate memory for initial "
			       "ramdisk !\n\r");
		printf("Relocating initrd 0x%lx <- 0x%p (0x%lx bytes)\n\r",
		       initrd_addr, old_addr, initrd_size);
		memmove((void *)initrd_addr, old_addr, initrd_size);
	}

	printf("initrd head: 0x%lx\n\r", *((unsigned long *)initrd_addr));

	
	setprop_val(chosen, "linux,initrd-start", (u32)(initrd_addr));
	setprop_val(chosen, "linux,initrd-end", (u32)(initrd_addr+initrd_size));

	return (struct addr_range){(void *)initrd_addr, initrd_size};
}

static char cmdline[COMMAND_LINE_SIZE]
	__attribute__((__section__("__builtin_cmdline")));

static void prep_cmdline(void *chosen)
{
	if (cmdline[0] == '\0')
		getprop(chosen, "bootargs", cmdline, COMMAND_LINE_SIZE-1);

	printf("\n\rLinux/PowerPC load: %s", cmdline);
	
	if (console_ops.edit_cmdline)
		console_ops.edit_cmdline(cmdline, COMMAND_LINE_SIZE);
	printf("\n\r");

	
	setprop_str(chosen, "bootargs", cmdline);
}

struct platform_ops platform_ops;
struct dt_ops dt_ops;
struct console_ops console_ops;
struct loader_info loader_info;

void start(void)
{
	struct addr_range vmlinux, initrd;
	kernel_entry_t kentry;
	unsigned long ft_addr = 0;
	void *chosen;

	if ((loader_info.cmdline_len > 0) && (cmdline[0] == '\0'))
		memmove(cmdline, loader_info.cmdline,
			min(loader_info.cmdline_len, COMMAND_LINE_SIZE-1));

	if (console_ops.open && (console_ops.open() < 0))
		exit();
	if (platform_ops.fixups)
		platform_ops.fixups();

	printf("\n\rzImage starting: loaded at 0x%p (sp: 0x%p)\n\r",
	       _start, get_sp());

	
	chosen = finddevice("/chosen");
	if (!chosen)
		chosen = create_node(NULL, "chosen");

	vmlinux = prep_kernel();
	initrd = prep_initrd(vmlinux, chosen,
			     loader_info.initrd_addr, loader_info.initrd_size);
	prep_cmdline(chosen);

	printf("Finalizing device tree...");
	if (dt_ops.finalize)
		ft_addr = dt_ops.finalize();
	if (ft_addr)
		printf(" flat tree at 0x%lx\n\r", ft_addr);
	else
		printf(" using OF tree (promptr=%p)\n\r", loader_info.promptr);

	if (console_ops.close)
		console_ops.close();

	kentry = (kernel_entry_t) vmlinux.addr;
	if (ft_addr)
		kentry(ft_addr, 0, NULL);
	else
		kentry((unsigned long)initrd.addr, initrd.size,
		       loader_info.promptr);

	
	fatal("Error: Linux kernel returned to zImage boot wrapper!\n\r");
}
