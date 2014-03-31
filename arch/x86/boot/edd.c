/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 *   This file is part of the Linux kernel, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */


#include "boot.h"
#include <linux/edd.h>

#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)

static int read_mbr(u8 devno, void *buf)
{
	struct biosregs ireg, oreg;

	initregs(&ireg);
	ireg.ax = 0x0201;		
	ireg.cx = 0x0001;		
	ireg.dl = devno;
	ireg.bx = (size_t)buf;

	intcall(0x13, &ireg, &oreg);

	return -(oreg.eflags & X86_EFLAGS_CF); 
}

static u32 read_mbr_sig(u8 devno, struct edd_info *ei, u32 *mbrsig)
{
	int sector_size;
	char *mbrbuf_ptr, *mbrbuf_end;
	u32 buf_base, mbr_base;
	extern char _end[];
	u16 mbr_magic;

	sector_size = ei->params.bytes_per_sector;
	if (!sector_size)
		sector_size = 512; 

	
	buf_base = (ds() << 4) + (u32)&_end;
	mbr_base = (buf_base+sector_size-1) & ~(sector_size-1);
	mbrbuf_ptr = _end + (mbr_base-buf_base);
	mbrbuf_end = mbrbuf_ptr + sector_size;

	
	if (!(boot_params.hdr.loadflags & CAN_USE_HEAP))
		return -1;
	if (mbrbuf_end > (char *)(size_t)boot_params.hdr.heap_end_ptr)
		return -1;

	memset(mbrbuf_ptr, 0, sector_size);
	if (read_mbr(devno, mbrbuf_ptr))
		return -1;

	*mbrsig = *(u32 *)&mbrbuf_ptr[EDD_MBR_SIG_OFFSET];
	mbr_magic = *(u16 *)&mbrbuf_ptr[510];

	
	return mbr_magic == 0xAA55 ? 0 : -1;
}

static int get_edd_info(u8 devno, struct edd_info *ei)
{
	struct biosregs ireg, oreg;

	memset(ei, 0, sizeof *ei);

	

	initregs(&ireg);
	ireg.ah = 0x41;
	ireg.bx = EDDMAGIC1;
	ireg.dl = devno;
	intcall(0x13, &ireg, &oreg);

	if (oreg.eflags & X86_EFLAGS_CF)
		return -1;	

	if (oreg.bx != EDDMAGIC2)
		return -1;

	ei->device  = devno;
	ei->version = oreg.ah;		 
	ei->interface_support = oreg.cx; 

	

	ei->params.length = sizeof(ei->params);
	ireg.ah = 0x48;
	ireg.si = (size_t)&ei->params;
	intcall(0x13, &ireg, &oreg);

	

	
	ireg.ah = 0x08;
	ireg.es = 0;
	intcall(0x13, &ireg, &oreg);

	if (!(oreg.eflags & X86_EFLAGS_CF)) {
		ei->legacy_max_cylinder = oreg.ch + ((oreg.cl & 0xc0) << 2);
		ei->legacy_max_head = oreg.dh;
		ei->legacy_sectors_per_track = oreg.cl & 0x3f;
	}

	return 0;
}

void query_edd(void)
{
	char eddarg[8];
	int do_mbr = 1;
#ifdef CONFIG_EDD_OFF
	int do_edd = 0;
#else
	int do_edd = 1;
#endif
	int be_quiet;
	int devno;
	struct edd_info ei, *edp;
	u32 *mbrptr;

	if (cmdline_find_option("edd", eddarg, sizeof eddarg) > 0) {
		if (!strcmp(eddarg, "skipmbr") || !strcmp(eddarg, "skip")) {
			do_edd = 1;
			do_mbr = 0;
		}
		else if (!strcmp(eddarg, "off"))
			do_edd = 0;
		else if (!strcmp(eddarg, "on"))
			do_edd = 1;
	}

	be_quiet = cmdline_find_option_bool("quiet");

	edp    = boot_params.eddbuf;
	mbrptr = boot_params.edd_mbr_sig_buffer;

	if (!do_edd)
		return;


	if (!be_quiet)
		printf("Probing EDD (edd=off to disable)... ");

	for (devno = 0x80; devno < 0x80+EDD_MBR_SIG_MAX; devno++) {
		if (!get_edd_info(devno, &ei)
		    && boot_params.eddbuf_entries < EDDMAXNR) {
			memcpy(edp, &ei, sizeof ei);
			edp++;
			boot_params.eddbuf_entries++;
		}

		if (do_mbr && !read_mbr_sig(devno, &ei, mbrptr++))
			boot_params.edd_mbr_sig_buf_entries = devno-0x80+1;
	}

	if (!be_quiet)
		printf("ok\n");
}

#endif
