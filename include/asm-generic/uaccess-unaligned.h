#ifndef __ASM_GENERIC_UACCESS_UNALIGNED_H
#define __ASM_GENERIC_UACCESS_UNALIGNED_H

#define __get_user_unaligned(x, ptr)					\
({									\
	__typeof__ (*(ptr)) __x;					\
	__copy_from_user(&__x, (ptr), sizeof(*(ptr))) ? -EFAULT : 0;	\
	(x) = __x;							\
})


#define __put_user_unaligned(x, ptr)					\
({									\
	__typeof__ (*(ptr)) __x = (x);					\
	__copy_to_user((ptr), &__x, sizeof(*(ptr))) ? -EFAULT : 0;	\
})

#endif 
