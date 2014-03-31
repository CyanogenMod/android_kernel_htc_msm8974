/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2007 John Williams
 *
 * Reasonably optimised generic C-code for memcpy on Microblaze
 * This is generic C code to do efficient, alignment-aware memcpy.
 *
 * It is based on demo code originally Copyright 2001 by Intel Corp, taken from
 * http://www.embedded.com/showArticle.jhtml?articleID=19205567
 *
 * Attempts were made, unsuccessfully, to contact the original
 * author of this code (Michael Morrow, Intel).  Below is the original
 * copyright notice.
 *
 * This software has been developed by Intel Corporation.
 * Intel specifically disclaims all warranties, express or
 * implied, and all liability, including consequential and
 * other indirect damages, for the use of this program, including
 * liability for infringement of any proprietary rights,
 * and including the warranties of merchantability and fitness
 * for a particular purpose. Intel does not assume any
 * responsibility for and errors which may appear in this program
 * not any responsibility to update it.
 */

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/compiler.h>
#include <linux/module.h>

#include <linux/string.h>

#ifdef __HAVE_ARCH_MEMCPY
#ifndef CONFIG_OPT_LIB_FUNCTION
void *memcpy(void *v_dst, const void *v_src, __kernel_size_t c)
{
	const char *src = v_src;
	char *dst = v_dst;

	
	while (c--)
		*dst++ = *src++;

	return v_dst;
}
#else 
void *memcpy(void *v_dst, const void *v_src, __kernel_size_t c)
{
	const char *src = v_src;
	char *dst = v_dst;

	const uint32_t *i_src;
	uint32_t *i_dst;

	if (likely(c >= 4)) {
		unsigned  value, buf_hold;

		
		
		switch ((unsigned long)dst & 3) {
		case 1:
			*dst++ = *src++;
			--c;
		case 2:
			*dst++ = *src++;
			--c;
		case 3:
			*dst++ = *src++;
			--c;
		}

		i_dst = (void *)dst;

		
		
		switch ((unsigned long)src & 3) {
		case 0x0:	
			i_src  = (const void *)src;

			for (; c >= 4; c -= 4)
				*i_dst++ = *i_src++;

			src  = (const void *)i_src;
			break;
		case 0x1:	
			
			i_src = (const void *) ((unsigned)src & ~3);
#ifndef __MICROBLAZEEL__
			
			buf_hold = *i_src++ << 8;

			for (; c >= 4; c -= 4) {
				value = *i_src++;
				*i_dst++ = buf_hold | value >> 24;
				buf_hold = value << 8;
			}
#else
			
			buf_hold = (*i_src++ & 0xFFFFFF00) >>8;

			for (; c >= 4; c -= 4) {
				value = *i_src++;
				*i_dst++ = buf_hold | ((value & 0xFF) << 24);
				buf_hold = (value & 0xFFFFFF00) >>8;
			}
#endif
			
			src = (const void *)i_src;
			src -= 3;
			break;
		case 0x2:	
			
			i_src = (const void *) ((unsigned)src & ~3);
#ifndef __MICROBLAZEEL__
			
			buf_hold = *i_src++ << 16;

			for (; c >= 4; c -= 4) {
				value = *i_src++;
				*i_dst++ = buf_hold | value >> 16;
				buf_hold = value << 16;
			}
#else
			
			buf_hold = (*i_src++ & 0xFFFF0000 )>>16;

			for (; c >= 4; c -= 4) {
				value = *i_src++;
				*i_dst++ = buf_hold | ((value & 0xFFFF)<<16);
				buf_hold = (value & 0xFFFF0000) >>16;
			}
#endif
			
			src = (const void *)i_src;
			src -= 2;
			break;
		case 0x3:	
			
			i_src = (const void *) ((unsigned)src & ~3);
#ifndef __MICROBLAZEEL__
			
			buf_hold = *i_src++ << 24;

			for (; c >= 4; c -= 4) {
				value = *i_src++;
				*i_dst++ = buf_hold | value >> 8;
				buf_hold = value << 24;
			}
#else
			
			buf_hold = (*i_src++ & 0xFF000000) >> 24;

			for (; c >= 4; c -= 4) {
				value = *i_src++;
				*i_dst++ = buf_hold | ((value & 0xFFFFFF) << 8);
				buf_hold = (value & 0xFF000000) >> 24;
			}
#endif
			
			src = (const void *)i_src;
			src -= 1;
			break;
		}
		dst = (void *)i_dst;
	}

	
	
	switch (c) {
	case 3:
		*dst++ = *src++;
	case 2:
		*dst++ = *src++;
	case 1:
		*dst++ = *src++;
	}

	return v_dst;
}
#endif 
EXPORT_SYMBOL(memcpy);
#endif 
