/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * Based on linux/include/asm-arm/setup.h
 *   Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_SETUP_H__
#define __ASM_AVR32_SETUP_H__

#define COMMAND_LINE_SIZE 256

#ifdef __KERNEL__

#define ATAG_MAGIC	0xa2a25441

#ifndef __ASSEMBLY__

struct tag_mem_range {
	u32			addr;
	u32			size;
	struct tag_mem_range *	next;
};

#define ATAG_NONE	0x00000000

struct tag_header {
	u32 size;
	u32 tag;
};

#define ATAG_CORE	0x54410001

struct tag_core {
	u32 flags;
	u32 pagesize;
	u32 rootdev;
};

#define ATAG_MEM	0x54410002

#define ATAG_CMDLINE	0x54410003

struct tag_cmdline {
	char	cmdline[1];	
};

#define ATAG_RDIMG	0x54410004

#define ATAG_CLOCK	0x54410005

struct tag_clock {
	u32	clock_id;	
	u32	clock_flags;	
	u64	clock_hz;	
};

#define CLOCK_BOOTCPU	0

#define ATAG_RSVD_MEM	0x54410006


#define ATAG_ETHERNET	0x54410007

struct tag_ethernet {
	u8	mac_index;
	u8	mii_phy_addr;
	u8	hw_address[6];
};

#define ETH_INVALID_PHY	0xff

#define ATAG_BOARDINFO	0x54410008

struct tag_boardinfo {
	u32	board_number;
};

struct tag {
	struct tag_header hdr;
	union {
		struct tag_core core;
		struct tag_mem_range mem_range;
		struct tag_cmdline cmdline;
		struct tag_clock clock;
		struct tag_ethernet ethernet;
		struct tag_boardinfo boardinfo;
	} u;
};

struct tagtable {
	u32	tag;
	int	(*parse)(struct tag *);
};

#define __tag __used __attribute__((__section__(".taglist.init")))
#define __tagtable(tag, fn)						\
	static struct tagtable __tagtable_##fn __tag = { tag, fn }

#define tag_member_present(tag,member)					\
	((unsigned long)(&((struct tag *)0L)->member + 1)		\
	 <= (tag)->hdr.size * 4)

#define tag_next(t)	((struct tag *)((u32 *)(t) + (t)->hdr.size))
#define tag_size(type)	((sizeof(struct tag_header) + sizeof(struct type)) >> 2)

#define for_each_tag(t,base)						\
	for (t = base; t->hdr.size; t = tag_next(t))

extern struct tag *bootloader_tags;

extern resource_size_t fbmem_start;
extern resource_size_t fbmem_size;
extern u32 board_number;

void setup_processor(void);

#endif 

#endif  

#endif 
