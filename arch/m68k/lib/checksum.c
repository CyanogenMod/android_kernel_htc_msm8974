/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		IP/TCP/UDP checksumming routines
 *
 * Authors:	Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Tom May, <ftom@netcom.com>
 *		Andreas Schwab, <schwab@issan.informatik.uni-dortmund.de>
 *		Lots of code moved from tcp.c and ip.c; see those files
 *		for more names.
 *
 * 03/02/96	Jes Sorensen, Andreas Schwab, Roman Hodek:
 *		Fixed some nasty bugs, causing some horrible crashes.
 *		A: At some points, the sum (%0) was used as
 *		length-counter instead of the length counter
 *		(%1). Thanks to Roman Hodek for pointing this out.
 *		B: GCC seems to mess up if one uses too many
 *		data-registers to hold input values and one tries to
 *		specify d0 and d1 as scratch registers. Letting gcc
 *		choose these registers itself solves the problem.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * 1998/8/31	Andreas Schwab:
 *		Zero out rest of buffer on exception in
 *		csum_partial_copy_from_user.
 */

#include <linux/module.h>
#include <net/checksum.h>


__wsum csum_partial(const void *buff, int len, __wsum sum)
{
	unsigned long tmp1, tmp2;
	__asm__("movel %2,%3\n\t"
		"btst #1,%3\n\t"	
		"jeq 2f\n\t"
		"subql #2,%1\n\t"	
		"jgt 1f\n\t"
		"addql #2,%1\n\t"	
		"jra 4f\n"
	     "1:\t"
		"addw %2@+,%0\n\t"	
		"clrl %3\n\t"
		"addxl %3,%0\n"		
	     "2:\t"
		
		"movel %1,%3\n\t"	
		"lsrl #5,%1\n\t"	
		"jeq 2f\n\t"		
		"subql #1,%1\n"
	     "1:\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"dbra %1,1b\n\t"
		"clrl %4\n\t"
		"addxl %4,%0\n\t"	
		"clrw %1\n\t"
		"subql #1,%1\n\t"
		"jcc 1b\n"
	     "2:\t"
		"movel %3,%1\n\t"	
		"andw #0x1c,%3\n\t"	
		"jeq 4f\n\t"
		"lsrw #2,%3\n\t"
		"subqw #1,%3\n"
	     "3:\t"
		
		"movel %2@+,%4\n\t"
		"addxl %4,%0\n\t"
		"dbra %3,3b\n\t"
		"clrl %4\n\t"
		"addxl %4,%0\n"		
	     "4:\t"
		
		"andw #3,%1\n\t"
		"jeq 7f\n\t"
		"clrl %4\n\t"		
		"subqw #2,%1\n\t"
		"jlt 5f\n\t"
		"movew %2@+,%4\n\t"	
		"swap %4\n\t"		
		"tstw %1\n\t"		
		"jeq 6f\n"
	     "5:\t"
		"moveb %2@,%4\n\t"	
		"lslw #8,%4\n\t"	
	     "6:\t"
		"addl %4,%0\n\t"	
		"clrl %4\n\t"
		"addxl %4,%0\n"		
	     "7:\t"
		: "=d" (sum), "=d" (len), "=a" (buff),
		  "=&d" (tmp1), "=&d" (tmp2)
		: "0" (sum), "1" (len), "2" (buff)
	    );
	return(sum);
}

EXPORT_SYMBOL(csum_partial);



__wsum
csum_partial_copy_from_user(const void __user *src, void *dst,
			    int len, __wsum sum, int *csum_err)
{
	unsigned long tmp1, tmp2;

	__asm__("movel %2,%4\n\t"
		"btst #1,%4\n\t"	
		"jeq 2f\n\t"
		"subql #2,%1\n\t"	
		"jgt 1f\n\t"
		"addql #2,%1\n\t"	
		"jra 4f\n"
	     "1:\n"
	     "10:\t"
		"movesw %2@+,%4\n\t"	
		"addw %4,%0\n\t"
		"movew %4,%3@+\n\t"
		"clrl %4\n\t"
		"addxl %4,%0\n"		
	     "2:\t"
		
		"movel %1,%4\n\t"	
		"lsrl #5,%1\n\t"	
		"jeq 2f\n\t"		
		"subql #1,%1\n"
	     "1:\n"
	     "11:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "12:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "13:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "14:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "15:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "16:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "17:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
	     "18:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"dbra %1,1b\n\t"
		"clrl %5\n\t"
		"addxl %5,%0\n\t"	
		"clrw %1\n\t"
		"subql #1,%1\n\t"
		"jcc 1b\n"
	     "2:\t"
		"movel %4,%1\n\t"	
		"andw #0x1c,%4\n\t"	
		"jeq 4f\n\t"
		"lsrw #2,%4\n\t"
		"subqw #1,%4\n"
	     "3:\n"
		
	     "19:\t"
		"movesl %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"dbra %4,3b\n\t"
		"clrl %5\n\t"
		"addxl %5,%0\n"		
	     "4:\t"
		
		"andw #3,%1\n\t"
		"jeq 7f\n\t"
		"clrl %5\n\t"		
		"subqw #2,%1\n\t"
		"jlt 5f\n\t"
	     "20:\t"
		"movesw %2@+,%5\n\t"	
		"movew %5,%3@+\n\t"
		"swap %5\n\t"		
		"tstw %1\n\t"		
		"jeq 6f\n"
	     "5:\n"
	     "21:\t"
		"movesb %2@,%5\n\t"	
		"moveb %5,%3@+\n\t"
		"lslw #8,%5\n\t"	
	     "6:\t"
		"addl %5,%0\n\t"	
		"clrl %5\n\t"
		"addxl %5,%0\n\t"	
	     "7:\t"
		"clrl %5\n"		
	     "8:\n"
		".section .fixup,\"ax\"\n"
		".even\n"
	     "90:\t"
		"clrw %3@+\n\t"
		"movel %1,%4\n\t"
		"lsrl #5,%1\n\t"
		"jeq 1f\n\t"
		"subql #1,%1\n"
	     "91:\t"
		"clrl %3@+\n"
	     "92:\t"
		"clrl %3@+\n"
	     "93:\t"
		"clrl %3@+\n"
	     "94:\t"
		"clrl %3@+\n"
	     "95:\t"
		"clrl %3@+\n"
	     "96:\t"
		"clrl %3@+\n"
	     "97:\t"
		"clrl %3@+\n"
	     "98:\t"
		"clrl %3@+\n\t"
		"dbra %1,91b\n\t"
		"clrw %1\n\t"
		"subql #1,%1\n\t"
		"jcc 91b\n"
	     "1:\t"
		"movel %4,%1\n\t"
		"andw #0x1c,%4\n\t"
		"jeq 1f\n\t"
		"lsrw #2,%4\n\t"
		"subqw #1,%4\n"
	     "99:\t"
		"clrl %3@+\n\t"
		"dbra %4,99b\n\t"
	     "1:\t"
		"andw #3,%1\n\t"
		"jeq 9f\n"
	     "100:\t"
		"clrw %3@+\n\t"
		"tstw %1\n\t"
		"jeq 9f\n"
	     "101:\t"
		"clrb %3@+\n"
	     "9:\t"
#define STR(X) STR1(X)
#define STR1(X) #X
		"moveq #-" STR(EFAULT) ",%5\n\t"
		"jra 8b\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		".long 10b,90b\n"
		".long 11b,91b\n"
		".long 12b,92b\n"
		".long 13b,93b\n"
		".long 14b,94b\n"
		".long 15b,95b\n"
		".long 16b,96b\n"
		".long 17b,97b\n"
		".long 18b,98b\n"
		".long 19b,99b\n"
		".long 20b,100b\n"
		".long 21b,101b\n"
		".previous"
		: "=d" (sum), "=d" (len), "=a" (src), "=a" (dst),
		  "=&d" (tmp1), "=d" (tmp2)
		: "0" (sum), "1" (len), "2" (src), "3" (dst)
	    );

	*csum_err = tmp2;

	return(sum);
}

EXPORT_SYMBOL(csum_partial_copy_from_user);



__wsum
csum_partial_copy_nocheck(const void *src, void *dst, int len, __wsum sum)
{
	unsigned long tmp1, tmp2;
	__asm__("movel %2,%4\n\t"
		"btst #1,%4\n\t"	
		"jeq 2f\n\t"
		"subql #2,%1\n\t"	
		"jgt 1f\n\t"
		"addql #2,%1\n\t"	
		"jra 4f\n"
	     "1:\t"
		"movew %2@+,%4\n\t"	
		"addw %4,%0\n\t"
		"movew %4,%3@+\n\t"
		"clrl %4\n\t"
		"addxl %4,%0\n"		
	     "2:\t"
		
		"movel %1,%4\n\t"	
		"lsrl #5,%1\n\t"	
		"jeq 2f\n\t"		
		"subql #1,%1\n"
	     "1:\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"dbra %1,1b\n\t"
		"clrl %5\n\t"
		"addxl %5,%0\n\t"	
		"clrw %1\n\t"
		"subql #1,%1\n\t"
		"jcc 1b\n"
	     "2:\t"
		"movel %4,%1\n\t"	
		"andw #0x1c,%4\n\t"	
		"jeq 4f\n\t"
		"lsrw #2,%4\n\t"
		"subqw #1,%4\n"
	     "3:\t"
		
		"movel %2@+,%5\n\t"
		"addxl %5,%0\n\t"
		"movel %5,%3@+\n\t"
		"dbra %4,3b\n\t"
		"clrl %5\n\t"
		"addxl %5,%0\n"		
	     "4:\t"
		
		"andw #3,%1\n\t"
		"jeq 7f\n\t"
		"clrl %5\n\t"		
		"subqw #2,%1\n\t"
		"jlt 5f\n\t"
		"movew %2@+,%5\n\t"	
		"movew %5,%3@+\n\t"
		"swap %5\n\t"		
		"tstw %1\n\t"		
		"jeq 6f\n"
	     "5:\t"
		"moveb %2@,%5\n\t"	
		"moveb %5,%3@+\n\t"
		"lslw #8,%5\n"		
	     "6:\t"
		"addl %5,%0\n\t"	
		"clrl %5\n\t"
		"addxl %5,%0\n"		
	     "7:\t"
		: "=d" (sum), "=d" (len), "=a" (src), "=a" (dst),
		  "=&d" (tmp1), "=&d" (tmp2)
		: "0" (sum), "1" (len), "2" (src), "3" (dst)
	    );
    return(sum);
}
EXPORT_SYMBOL(csum_partial_copy_nocheck);
