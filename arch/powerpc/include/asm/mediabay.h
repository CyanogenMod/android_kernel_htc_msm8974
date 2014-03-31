/*
 * mediabay.h: definitions for using the media bay
 * on PowerBook 3400 and similar computers.
 *
 * Copyright (C) 1997 Paul Mackerras.
 */
#ifndef _PPC_MEDIABAY_H
#define _PPC_MEDIABAY_H

#ifdef __KERNEL__

#define MB_FD		0	
#define MB_FD1		1	
#define MB_SOUND	2	
#define MB_CD		3	
#define MB_PCI		5	
#define MB_POWER	6	
#define MB_NO		7	

struct macio_dev;

#ifdef CONFIG_PMAC_MEDIABAY

extern int check_media_bay(struct macio_dev *bay);

extern void lock_media_bay(struct macio_dev *bay);
extern void unlock_media_bay(struct macio_dev *bay);

#else

static inline int check_media_bay(struct macio_dev *bay)
{
	return MB_NO;
}

static inline void lock_media_bay(struct macio_dev *bay) { }
static inline void unlock_media_bay(struct macio_dev *bay) { }

#endif

#endif 
#endif 
