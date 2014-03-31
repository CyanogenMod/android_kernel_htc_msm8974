/*  Architecture specific parts of HP's STI (framebuffer) driver.
 *  Structures are HP-UX compatible for XFree86 usage.
 * 
 *    Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *    Copyright (C) 2001 Helge Deller (deller a parisc-linux org)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_PARISC_GRFIOCTL_H
#define __ASM_PARISC_GRFIOCTL_H


#define GRFGATOR		8
#define S9000_ID_S300		9
#define GRFBOBCAT		9
#define	GRFCATSEYE		9
#define S9000_ID_98720		10
#define GRFRBOX			10
#define S9000_ID_98550		11
#define GRFFIREEYE		11
#define S9000_ID_A1096A		12
#define GRFHYPERION		12
#define S9000_ID_FRI		13
#define S9000_ID_98730		14
#define GRFDAVINCI		14
#define S9000_ID_98705		0x26C08070	
#define S9000_ID_98736		0x26D148AB
#define S9000_ID_A1659A		0x26D1482A	
#define S9000_ID_ELK		S9000_ID_A1659A
#define S9000_ID_A1439A		0x26D148EE	
#define S9000_ID_A1924A		0x26D1488C	
#define S9000_ID_ELM		S9000_ID_A1924A
#define S9000_ID_98765		0x27480DEF
#define S9000_ID_ELK_768	0x27482101
#define S9000_ID_STINGER	0x27A4A402
#define S9000_ID_TIMBER		0x27F12392	
#define S9000_ID_TOMCAT		0x27FCCB6D	
#define S9000_ID_ARTIST		0x2B4DED6D	
#define S9000_ID_HCRX		0x2BCB015A	
#define CRX24_OVERLAY_PLANES	0x920825AA	

#define CRT_ID_ELK_1024		S9000_ID_ELK_768 
#define CRT_ID_ELK_1280		S9000_ID_A1659A	
#define CRT_ID_ELK_1024DB	0x27849CA5      
#define CRT_ID_ELK_GS		S9000_ID_A1924A	
#define CRT_ID_CRX24		S9000_ID_A1439A	
#define CRT_ID_VISUALIZE_EG	0x2D08C0A7      
#define CRT_ID_THUNDER		0x2F23E5FC      
#define CRT_ID_THUNDER2		0x2F8D570E      
#define CRT_ID_HCRX		S9000_ID_HCRX	
#define CRT_ID_CRX48Z		S9000_ID_STINGER 
#define CRT_ID_DUAL_CRX		S9000_ID_TOMCAT	
#define CRT_ID_PVRX		S9000_ID_98705	
#define CRT_ID_TIMBER		S9000_ID_TIMBER	
#define CRT_ID_TVRX		S9000_ID_98765	
#define CRT_ID_ARTIST		S9000_ID_ARTIST	
#define CRT_ID_SUMMIT		0x2FC1066B      
#define CRT_ID_LEGO		0x35ACDA30	
#define CRT_ID_PINNACLE		0x35ACDA16	 


#define gaddr_t unsigned long	

struct	grf_fbinfo {
	unsigned int	id;		
	unsigned int	mapsize;	
	unsigned int	dwidth, dlength;
	unsigned int	width, length;	
	unsigned int	xlen;		
	unsigned int	bpp, bppu;	
	unsigned int	npl, nplbytes;	
	char		name[32];	
	unsigned int	attr;		
	gaddr_t 	fbbase, regbase;
	gaddr_t		regions[6];	
};

#define	GCID		_IOR('G', 0, int)
#define	GCON		_IO('G', 1)
#define	GCOFF		_IO('G', 2)
#define	GCAON		_IO('G', 3)
#define	GCAOFF		_IO('G', 4)
#define	GCMAP		_IOWR('G', 5, int)
#define	GCUNMAP		_IOWR('G', 6, int)
#define	GCMAP_HPUX	_IO('G', 5)
#define	GCUNMAP_HPUX	_IO('G', 6)
#define	GCLOCK		_IO('G', 7)
#define	GCUNLOCK	_IO('G', 8)
#define	GCLOCK_MINIMUM	_IO('G', 9)
#define	GCUNLOCK_MINIMUM _IO('G', 10)
#define	GCSTATIC_CMAP	_IO('G', 11)
#define	GCVARIABLE_CMAP _IO('G', 12)
#define GCTERM		_IOWR('G',20,int)	 
#define GCDESCRIBE	_IOR('G', 21, struct grf_fbinfo)
#define GCFASTLOCK	_IO('G', 26)

#endif 

