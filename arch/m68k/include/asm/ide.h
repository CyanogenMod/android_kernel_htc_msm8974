/*
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/* Copyright(c) 1996 Kars de Jong */


#ifndef _M68K_IDE_H
#define _M68K_IDE_H

#ifdef __KERNEL__
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/irq.h>

#ifdef CONFIG_MMU

#undef readb
#undef readw
#undef writeb
#undef writew

#define readb				in_8
#define readw				in_be16
#define __ide_mm_insw(port, addr, n)	raw_insw((u16 *)port, addr, n)
#define __ide_mm_insl(port, addr, n)	raw_insl((u32 *)port, addr, n)
#define writeb(val, port)		out_8(port, val)
#define writew(val, port)		out_be16(port, val)
#define __ide_mm_outsw(port, addr, n)	raw_outsw((u16 *)port, addr, n)
#define __ide_mm_outsl(port, addr, n)	raw_outsl((u32 *)port, addr, n)

#else

#define __ide_mm_insw(port, addr, n)	io_insw((unsigned int)port, addr, n)
#define __ide_mm_insl(port, addr, n)	io_insl((unsigned int)port, addr, n)
#define __ide_mm_outsw(port, addr, n)	io_outsw((unsigned int)port, addr, n)
#define __ide_mm_outsl(port, addr, n)	io_outsl((unsigned int)port, addr, n)

#endif 

#endif 
#endif 
