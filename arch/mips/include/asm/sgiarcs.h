/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ARC firmware interface defines.
 *
 * Copyright (C) 1996 David S. Miller (davem@davemloft.net)
 * Copyright (C) 1999, 2001 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_SGIARCS_H
#define _ASM_SGIARCS_H

#include <asm/types.h>
#include <asm/fw/arc/types.h>

#define PROM_ESUCCESS                   0x00
#define PROM_E2BIG                      0x01
#define PROM_EACCESS                    0x02
#define PROM_EAGAIN                     0x03
#define PROM_EBADF                      0x04
#define PROM_EBUSY                      0x05
#define PROM_EFAULT                     0x06
#define PROM_EINVAL                     0x07
#define PROM_EIO                        0x08
#define PROM_EISDIR                     0x09
#define PROM_EMFILE                     0x0a
#define PROM_EMLINK                     0x0b
#define PROM_ENAMETOOLONG               0x0c
#define PROM_ENODEV                     0x0d
#define PROM_ENOENT                     0x0e
#define PROM_ENOEXEC                    0x0f
#define PROM_ENOMEM                     0x10
#define PROM_ENOSPC                     0x11
#define PROM_ENOTDIR                    0x12
#define PROM_ENOTTY                     0x13
#define PROM_ENXIO                      0x14
#define PROM_EROFS                      0x15
#define PROM_EADDRNOTAVAIL              0x1f
#define PROM_ETIMEDOUT                  0x20
#define PROM_ECONNABORTED               0x21
#define PROM_ENOCONNECT                 0x22

enum linux_devclass {
	system, processor, cache, adapter, controller, peripheral, memory
};

enum linux_devtypes {
	
	Arc, Cpu, Fpu,

	
	picache, pdcache,

	
	sicache, sdcache, sccache,

	memdev, eisa_adapter, tc_adapter, scsi_adapter, dti_adapter,
	multifunc_adapter, dsk_controller, tp_controller, cdrom_controller,
	worm_controller, serial_controller, net_controller, disp_controller,
	parallel_controller, ptr_controller, kbd_controller, audio_controller,
	misc_controller, disk_peripheral, flpy_peripheral, tp_peripheral,
	modem_peripheral, monitor_peripheral, printer_peripheral,
	ptr_peripheral, kbd_peripheral, term_peripheral, line_peripheral,
	net_peripheral, misc_peripheral, anon
};

enum linux_identifier {
	bogus, ronly, removable, consin, consout, input, output
};

struct linux_component {
	enum linux_devclass     class;	
	enum linux_devtypes     type;	
	enum linux_identifier   iflags;	
	USHORT 			vers;	
	USHORT 			rev;	
	ULONG 			key;	
	ULONG 			amask;	
	ULONG			cdsize;	
	ULONG			ilen;	
	_PULONG			iname;	
};
typedef struct linux_component pcomponent;

struct linux_sysid {
	char vend[8], prod[8];
};

enum arcs_memtypes {
	arcs_eblock,  
	arcs_rvpage,  
	arcs_fcontig, 
	arcs_free,    
	arcs_bmem,    
	arcs_prog,    
	arcs_atmp,    
	arcs_aperm,   
};

enum arc_memtypes {
	arc_eblock,  
	arc_rvpage,  
	arc_free,    
	arc_bmem,    
	arc_prog,    
	arc_atmp,    
	arc_aperm,   
	arc_fcontig, 
};

union linux_memtypes {
    enum arcs_memtypes arcs;
    enum arc_memtypes arc;
};

struct linux_mdesc {
	union linux_memtypes type;
	ULONG base;
	ULONG pages;
};

struct linux_tinfo {
	unsigned short yr;
	unsigned short mnth;
	unsigned short day;
	unsigned short hr;
	unsigned short min;
	unsigned short sec;
	unsigned short msec;
};

struct linux_vdirent {
	ULONG namelen;
	unsigned char attr;
	char fname[32]; 
};

enum linux_omode {
	rdonly, wronly, rdwr, wronly_creat, rdwr_creat,
	wronly_ssede, rdwr_ssede, dirent, dirent_creat
};

enum linux_seekmode {
	absolute, relative
};

enum linux_mountops {
	media_load, media_unload
};

struct linux_bigint {
#ifdef __MIPSEL__
	u32 lo;
	s32 hi;
#else 
	s32 hi;
	u32 lo;
#endif
};

struct linux_finfo {
	struct linux_bigint   begin;
	struct linux_bigint   end;
	struct linux_bigint   cur;
	enum linux_devtypes   dtype;
	unsigned long         namelen;
	unsigned char         attr;
	char                  name[32]; 
};

struct linux_romvec {
	LONG	load;			
	LONG	invoke;			
	LONG	exec;			
	LONG	halt;			
	LONG	pdown;			
	LONG	restart;		
	LONG	reboot;			
	LONG	imode;			
	LONG	_unused1;		

	
	LONG	next_component;
	LONG	child_component;
	LONG	parent_component;
	LONG	component_data;
	LONG	child_add;
	LONG	comp_del;
	LONG	component_by_path;

	
	LONG	cfg_save;
	LONG	get_sysid;

	
	LONG	get_mdesc;
	LONG	_unused2;		

	LONG	get_tinfo;
	LONG	get_rtime;

	
	LONG	get_vdirent;
	LONG	open;
	LONG	close;
	LONG	read;
	LONG	get_rstatus;
	LONG	write;
	LONG	seek;
	LONG	mount;

	
	LONG	get_evar;
	LONG	set_evar;

	LONG	get_finfo;
	LONG	set_finfo;

	
	LONG	cache_flush;
	LONG	TestUnicodeCharacter;		
	LONG	GetDisplayStatus;
};

typedef struct _SYSTEM_PARAMETER_BLOCK {
	ULONG			magic;		
#define PROMBLOCK_MAGIC      0x53435241

	ULONG			len;		
	USHORT			ver;		
	USHORT			rev;		
	_PLONG			rs_block;	
	_PLONG			dbg_block;	
	_PLONG			gevect;		
	_PLONG			utlbvect;	
	ULONG			rveclen;	
	_PVOID			romvec;		
	ULONG			pveclen;	
	_PVOID			pvector;	
	ULONG			adap_cnt;	
	ULONG			adap_typ0;	
	ULONG			adap_vcnt0;	
	_PVOID			adap_vector;	
	ULONG			adap_typ1;	
	ULONG			adap_vcnt1;	
	_PVOID			adap_vector1;	
	
} SYSTEM_PARAMETER_BLOCK, *PSYSTEM_PARAMETER_BLOCK;

#define PROMBLOCK ((PSYSTEM_PARAMETER_BLOCK) (int)0xA0001000)
#define ROMVECTOR ((struct linux_romvec *) (long)(PROMBLOCK)->romvec)

union linux_cache_key {
	struct param {
#ifdef __MIPSEL__
		unsigned short size;
		unsigned char lsize;
		unsigned char bsize;
#else 
		unsigned char bsize;
		unsigned char lsize;
		unsigned short size;
#endif
	} info;
	unsigned long allinfo;
};

struct linux_cdata {
	char *name;
	int mlen;
	enum linux_devtypes type;
};

#define SGIPROM_STDIN     0
#define SGIPROM_STDOUT    1

#define SGIPROM_ROFILE    0x01  
#define SGIPROM_HFILE     0x02  
#define SGIPROM_SFILE     0x04  
#define SGIPROM_AFILE     0x08  
#define SGIPROM_DFILE     0x10  
#define SGIPROM_DELFILE   0x20  

struct sgi_partition {
	unsigned char flag;
#define SGIPART_UNUSED 0x00
#define SGIPART_ACTIVE 0x80

	unsigned char shead, ssect, scyl; 
	unsigned char systype; 
	unsigned char ehead, esect, ecyl; 
	unsigned char rsect0, rsect1, rsect2, rsect3;
	unsigned char tsect0, tsect1, tsect2, tsect3;
};

#define SGIBBLOCK_MAGIC   0xaa55
#define SGIBBLOCK_MAXPART 0x0004

struct sgi_bootblock {
	unsigned char _unused[446];
	struct sgi_partition partitions[SGIBBLOCK_MAXPART];
	unsigned short magic;
};

struct sgi_bparm_block {
	unsigned short bytes_sect;    
	unsigned char  sect_clust;    
	unsigned short sect_resv;     
	unsigned char  nfats;         
	unsigned short nroot_dirents; 
	unsigned short sect_volume;   
	unsigned char  media_type;    
	unsigned short sect_fat;      
	unsigned short sect_track;    
	unsigned short nheads;        
	unsigned short nhsects;       
};

struct sgi_bsector {
	unsigned char   jmpinfo[3];
	unsigned char   manuf_name[8];
	struct sgi_bparm_block info;
};

#define SMB_DEBUG_MAGIC   0xfeeddead
struct linux_smonblock {
	unsigned long   magic;
	void            (*handler)(void);  
	unsigned long   dtable_base;       
	int             (*printf)(const char *fmt, ...);
	unsigned long   btable_base;       
	unsigned long   mpflushreqs;       
	unsigned long   ntab;              
	unsigned long   stab;              
	int             smax;              
};


#if defined(CONFIG_64BIT) && defined(CONFIG_ARC32)

#define __arc_clobbers							\
	"$2", "$3" , "$8", "$9", "$10", "$11", 			\
	"$12", "$13", "$14", "$15", "$16", "$24", "$25", "$31"

#define ARC_CALL0(dest)							\
({	long __res;							\
	long __vec = (long) romvec->dest;				\
	__asm__ __volatile__(						\
	"dsubu\t$29, 32\n\t"						\
	"jalr\t%1\n\t"							\
	"daddu\t$29, 32\n\t"						\
	"move\t%0, $2"							\
	: "=r" (__res), "=r" (__vec)					\
	: "1" (__vec)							\
	: __arc_clobbers, "$4", "$5", "$6", "$7");			\
	(unsigned long) __res;						\
})

#define ARC_CALL1(dest, a1)						\
({	long __res;							\
	register signed int __a1 __asm__("$4") = (int) (long) (a1);	\
	long __vec = (long) romvec->dest;				\
	__asm__ __volatile__(						\
	"dsubu\t$29, 32\n\t"						\
	"jalr\t%1\n\t"							\
	"daddu\t$29, 32\n\t"						\
	"move\t%0, $2"							\
	: "=r" (__res), "=r" (__vec)					\
	: "1" (__vec), "r" (__a1)					\
	: __arc_clobbers, "$5", "$6", "$7");				\
	(unsigned long) __res;						\
})

#define ARC_CALL2(dest, a1, a2)						\
({	long __res;							\
	register signed int __a1 __asm__("$4") = (int) (long) (a1);	\
	register signed int __a2 __asm__("$5") = (int) (long) (a2);	\
	long __vec = (long) romvec->dest;				\
	__asm__ __volatile__(						\
	"dsubu\t$29, 32\n\t"						\
	"jalr\t%1\n\t"							\
	"daddu\t$29, 32\n\t"						\
	"move\t%0, $2"							\
	: "=r" (__res), "=r" (__vec)					\
	: "1" (__vec), "r" (__a1), "r" (__a2)				\
	: __arc_clobbers, "$6", "$7");					\
	__res;								\
})

#define ARC_CALL3(dest, a1, a2, a3)					\
({	long __res;							\
	register signed int __a1 __asm__("$4") = (int) (long) (a1);	\
	register signed int __a2 __asm__("$5") = (int) (long) (a2);	\
	register signed int __a3 __asm__("$6") = (int) (long) (a3);	\
	long __vec = (long) romvec->dest;				\
	__asm__ __volatile__(						\
	"dsubu\t$29, 32\n\t"						\
	"jalr\t%1\n\t"							\
	"daddu\t$29, 32\n\t"						\
	"move\t%0, $2"							\
	: "=r" (__res), "=r" (__vec)					\
	: "1" (__vec), "r" (__a1), "r" (__a2), "r" (__a3)		\
	: __arc_clobbers, "$7");					\
	__res;								\
})

#define ARC_CALL4(dest, a1, a2, a3, a4)					\
({	long __res;							\
	register signed int __a1 __asm__("$4") = (int) (long) (a1);	\
	register signed int __a2 __asm__("$5") = (int) (long) (a2);	\
	register signed int __a3 __asm__("$6") = (int) (long) (a3);	\
	register signed int __a4 __asm__("$7") = (int) (long) (a4);	\
	long __vec = (long) romvec->dest;				\
	__asm__ __volatile__(						\
	"dsubu\t$29, 32\n\t"						\
	"jalr\t%1\n\t"							\
	"daddu\t$29, 32\n\t"						\
	"move\t%0, $2"							\
	: "=r" (__res), "=r" (__vec)					\
	: "1" (__vec), "r" (__a1), "r" (__a2), "r" (__a3), 		\
	  "r" (__a4)							\
	: __arc_clobbers);						\
	__res;								\
})

#define ARC_CALL5(dest, a1, a2, a3, a4, a5)					\
({	long __res;							\
	register signed int __a1 __asm__("$4") = (int) (long) (a1);	\
	register signed int __a2 __asm__("$5") = (int) (long) (a2);	\
	register signed int __a3 __asm__("$6") = (int) (long) (a3);	\
	register signed int __a4 __asm__("$7") = (int) (long) (a4);	\
	register signed int __a5 = (int) (long) (a5);			\
	long __vec = (long) romvec->dest;				\
	__asm__ __volatile__(						\
	"dsubu\t$29, 32\n\t"						\
	"sw\t%7, 16($29)\n\t"						\
	"jalr\t%1\n\t"							\
	"daddu\t$29, 32\n\t"						\
	"move\t%0, $2"							\
	: "=r" (__res), "=r" (__vec)					\
	: "1" (__vec), 							\
	  "r" (__a1), "r" (__a2), "r" (__a3), "r" (__a4), 		\
	  "r" (__a5)							\
	: __arc_clobbers);						\
	__res;								\
})

#endif 

#if (defined(CONFIG_32BIT) && defined(CONFIG_ARC32)) ||		\
    (defined(CONFIG_64BIT) && defined(CONFIG_ARC64))

#define ARC_CALL0(dest)							\
({	long __res;							\
	long (*__vec)(void) = (void *) romvec->dest;			\
									\
	__res = __vec();						\
	__res;								\
})

#define ARC_CALL1(dest, a1)						\
({	long __res;							\
	long __a1 = (long) (a1);					\
	long (*__vec)(long) = (void *) romvec->dest;			\
									\
	__res = __vec(__a1);						\
	__res;								\
})

#define ARC_CALL2(dest, a1, a2)						\
({	long __res;							\
	long __a1 = (long) (a1);					\
	long __a2 = (long) (a2);					\
	long (*__vec)(long, long) = (void *) romvec->dest;		\
									\
	__res = __vec(__a1, __a2);					\
	__res;								\
})

#define ARC_CALL3(dest, a1, a2, a3)					\
({	long __res;							\
	long __a1 = (long) (a1);					\
	long __a2 = (long) (a2);					\
	long __a3 = (long) (a3);					\
	long (*__vec)(long, long, long)	= (void *) romvec->dest;	\
									\
	__res = __vec(__a1, __a2, __a3);				\
	__res;								\
})

#define ARC_CALL4(dest, a1, a2, a3, a4)					\
({	long __res;							\
	long __a1 = (long) (a1);					\
	long __a2 = (long) (a2);					\
	long __a3 = (long) (a3);					\
	long __a4 = (long) (a4);					\
	long (*__vec)(long, long, long, long) = (void *) romvec->dest;	\
									\
	__res = __vec(__a1, __a2, __a3, __a4);				\
	__res;								\
})

#define ARC_CALL5(dest, a1, a2, a3, a4, a5)				\
({	long __res;							\
	long __a1 = (long) (a1);					\
	long __a2 = (long) (a2);					\
	long __a3 = (long) (a3);					\
	long __a4 = (long) (a4);					\
	long __a5 = (long) (a5);					\
	long (*__vec)(long, long, long, long, long);			\
	__vec = (void *) romvec->dest;					\
									\
	__res = __vec(__a1, __a2, __a3, __a4, __a5);			\
	__res;								\
})
#endif 

#endif 
