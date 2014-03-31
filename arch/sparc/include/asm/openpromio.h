#ifndef	_SPARC_OPENPROMIO_H
#define	_SPARC_OPENPROMIO_H

#include <linux/compiler.h>
#include <linux/ioctl.h>
#include <linux/types.h>


struct openpromio
{
	u_int	oprom_size;		
	char	oprom_array[1];		
};

#define	OPROMMAXPARAM	4096		

#define	OPROMGETOPT		0x20004F01
#define	OPROMSETOPT		0x20004F02
#define	OPROMNXTOPT		0x20004F03
#define	OPROMSETOPT2		0x20004F04
#define	OPROMNEXT		0x20004F05
#define	OPROMCHILD		0x20004F06
#define	OPROMGETPROP		0x20004F07
#define	OPROMNXTPROP		0x20004F08
#define	OPROMU2P		0x20004F09
#define	OPROMGETCONS		0x20004F0A
#define	OPROMGETFBNAME		0x20004F0B
#define	OPROMGETBOOTARGS	0x20004F0C
				
#define OPROMSETCUR		0x20004FF0	
#define OPROMPCI2NODE		0x20004FF1	
#define OPROMPATH2NODE		0x20004FF2	


#define OPROMCONS_NOT_WSCONS    0
#define OPROMCONS_STDIN_IS_KBD  0x1     
#define OPROMCONS_STDOUT_IS_FB  0x2     
#define OPROMCONS_OPENPROM      0x4     



struct opiocdesc
{
	int	op_nodeid;		
	int	op_namelen;		
	char	__user *op_name;	
	int	op_buflen;		
	char	__user *op_buf;		
};

#define	OPIOCGET	_IOWR('O', 1, struct opiocdesc)
#define	OPIOCSET	_IOW('O', 2, struct opiocdesc)
#define	OPIOCNEXTPROP	_IOWR('O', 3, struct opiocdesc)
#define	OPIOCGETOPTNODE	_IOR('O', 4, int)
#define	OPIOCGETNEXT	_IOWR('O', 5, int)
#define	OPIOCGETCHILD	_IOWR('O', 6, int)

#endif 

