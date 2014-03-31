#ifndef _ASM_PARISC_FUTEX_H
#define _ASM_PARISC_FUTEX_H

#ifdef __KERNEL__

#include <linux/futex.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <asm/errno.h>


static inline void
_futex_spin_lock_irqsave(u32 __user *uaddr, unsigned long int *flags)
{
	extern u32 lws_lock_start[];
	long index = ((long)uaddr & 0xf0) >> 2;
	arch_spinlock_t *s = (arch_spinlock_t *)&lws_lock_start[index];
	local_irq_save(*flags);
	arch_spin_lock(s);
}

static inline void
_futex_spin_unlock_irqrestore(u32 __user *uaddr, unsigned long int *flags)
{
	extern u32 lws_lock_start[];
	long index = ((long)uaddr & 0xf0) >> 2;
	arch_spinlock_t *s = (arch_spinlock_t *)&lws_lock_start[index];
	arch_spin_unlock(s);
	local_irq_restore(*flags);
}

static inline int
futex_atomic_op_inuser (int encoded_op, u32 __user *uaddr)
{
	unsigned long int flags;
	u32 val;
	int op = (encoded_op >> 28) & 7;
	int cmp = (encoded_op >> 24) & 15;
	int oparg = (encoded_op << 8) >> 20;
	int cmparg = (encoded_op << 20) >> 20;
	int oldval = 0, ret;
	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
		oparg = 1 << oparg;

	if (!access_ok(VERIFY_WRITE, uaddr, sizeof(*uaddr)))
		return -EFAULT;

	pagefault_disable();

	_futex_spin_lock_irqsave(uaddr, &flags);

	switch (op) {
	case FUTEX_OP_SET:
		
		ret = get_user(oldval, uaddr);
		if (!ret)
			ret = put_user(oparg, uaddr);
		break;
	case FUTEX_OP_ADD:
		
		ret = get_user(oldval, uaddr);
		if (!ret) {
			val = oldval + oparg;
			ret = put_user(val, uaddr);
		}
		break;
	case FUTEX_OP_OR:
		
		ret = get_user(oldval, uaddr);
		if (!ret) {
			val = oldval | oparg;
			ret = put_user(val, uaddr);
		}
		break;
	case FUTEX_OP_ANDN:
		
		ret = get_user(oldval, uaddr);
		if (!ret) {
			val = oldval & ~oparg;
			ret = put_user(val, uaddr);
		}
		break;
	case FUTEX_OP_XOR:
		
		ret = get_user(oldval, uaddr);
		if (!ret) {
			val = oldval ^ oparg;
			ret = put_user(val, uaddr);
		}
		break;
	default:
		ret = -ENOSYS;
	}

	_futex_spin_unlock_irqrestore(uaddr, &flags);

	pagefault_enable();

	if (!ret) {
		switch (cmp) {
		case FUTEX_OP_CMP_EQ: ret = (oldval == cmparg); break;
		case FUTEX_OP_CMP_NE: ret = (oldval != cmparg); break;
		case FUTEX_OP_CMP_LT: ret = (oldval < cmparg); break;
		case FUTEX_OP_CMP_GE: ret = (oldval >= cmparg); break;
		case FUTEX_OP_CMP_LE: ret = (oldval <= cmparg); break;
		case FUTEX_OP_CMP_GT: ret = (oldval > cmparg); break;
		default: ret = -ENOSYS;
		}
	}
	return ret;
}

static inline int
futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
			      u32 oldval, u32 newval)
{
	int ret;
	u32 val;
	unsigned long flags;

	if (segment_eq(KERNEL_DS, get_fs()) && !uaddr)
		return -EFAULT;

	if (!access_ok(VERIFY_WRITE, uaddr, sizeof(u32)))
		return -EFAULT;


	_futex_spin_lock_irqsave(uaddr, &flags);

	ret = get_user(val, uaddr);

	if (!ret && val == oldval)
		ret = put_user(newval, uaddr);

	*uval = val;

	_futex_spin_unlock_irqrestore(uaddr, &flags);

	return ret;
}

#endif 
#endif 
