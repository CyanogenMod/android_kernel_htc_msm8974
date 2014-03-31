

#include <linux/module.h>

int hwreg_present( volatile void *regp )
{
    int	ret = 0;
    long	save_sp, save_vbr;
    long	tmp_vectors[3];

    __asm__ __volatile__
	(	"movec	%/vbr,%2\n\t"
		"movel	#Lberr1,%4@(8)\n\t"
                "movec	%4,%/vbr\n\t"
		"movel	%/sp,%1\n\t"
		"moveq	#0,%0\n\t"
		"tstb	%3@\n\t"
		"nop\n\t"
		"moveq	#1,%0\n"
                "Lberr1:\n\t"
		"movel	%1,%/sp\n\t"
		"movec	%2,%/vbr"
		: "=&d" (ret), "=&r" (save_sp), "=&r" (save_vbr)
		: "a" (regp), "a" (tmp_vectors)
                );

    return( ret );
}
EXPORT_SYMBOL(hwreg_present);


int hwreg_write( volatile void *regp, unsigned short val )
{
	int		ret;
	long	save_sp, save_vbr;
	long	tmp_vectors[3];

	__asm__ __volatile__
	(	"movec	%/vbr,%2\n\t"
		"movel	#Lberr2,%4@(8)\n\t"
		"movec	%4,%/vbr\n\t"
		"movel	%/sp,%1\n\t"
		"moveq	#0,%0\n\t"
		"movew	%5,%3@\n\t"
		"nop	\n\t"	
		"moveq	#1,%0\n"
	"Lberr2:\n\t"
		"movel	%1,%/sp\n\t"
		"movec	%2,%/vbr"
		: "=&d" (ret), "=&r" (save_sp), "=&r" (save_vbr)
		: "a" (regp), "a" (tmp_vectors), "g" (val)
	);

	return( ret );
}
EXPORT_SYMBOL(hwreg_write);

