#ifndef _S390_RWSEM_H
#define _S390_RWSEM_H

/*
 *  include/asm-s390/rwsem.h
 *
 *  S390 version
 *    Copyright (C) 2002 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Based on asm-alpha/semaphore.h and asm-i386/rwsem.h
 */


#ifndef _LINUX_RWSEM_H
#error "please don't include asm/rwsem.h directly, use linux/rwsem.h instead"
#endif

#ifdef __KERNEL__

#ifndef __s390x__
#define RWSEM_UNLOCKED_VALUE	0x00000000
#define RWSEM_ACTIVE_BIAS	0x00000001
#define RWSEM_ACTIVE_MASK	0x0000ffff
#define RWSEM_WAITING_BIAS	(-0x00010000)
#else 
#define RWSEM_UNLOCKED_VALUE	0x0000000000000000L
#define RWSEM_ACTIVE_BIAS	0x0000000000000001L
#define RWSEM_ACTIVE_MASK	0x00000000ffffffffL
#define RWSEM_WAITING_BIAS	(-0x0000000100000000L)
#endif 
#define RWSEM_ACTIVE_READ_BIAS	RWSEM_ACTIVE_BIAS
#define RWSEM_ACTIVE_WRITE_BIAS	(RWSEM_WAITING_BIAS + RWSEM_ACTIVE_BIAS)

static inline void __down_read(struct rw_semaphore *sem)
{
	signed long old, new;

	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	ahi	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	aghi	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "i" (RWSEM_ACTIVE_READ_BIAS)
		: "cc", "memory");
	if (old < 0)
		rwsem_down_read_failed(sem);
}

static inline int __down_read_trylock(struct rw_semaphore *sem)
{
	signed long old, new;

	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	ltr	%1,%0\n"
		"	jm	1f\n"
		"	ahi	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b\n"
		"1:"
#else 
		"	lg	%0,%2\n"
		"0:	ltgr	%1,%0\n"
		"	jm	1f\n"
		"	aghi	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b\n"
		"1:"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "i" (RWSEM_ACTIVE_READ_BIAS)
		: "cc", "memory");
	return old >= 0 ? 1 : 0;
}

static inline void __down_write_nested(struct rw_semaphore *sem, int subclass)
{
	signed long old, new, tmp;

	tmp = RWSEM_ACTIVE_WRITE_BIAS;
	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	a	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	ag	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "m" (tmp)
		: "cc", "memory");
	if (old != 0)
		rwsem_down_write_failed(sem);
}

static inline void __down_write(struct rw_semaphore *sem)
{
	__down_write_nested(sem, 0);
}

static inline int __down_write_trylock(struct rw_semaphore *sem)
{
	signed long old;

	asm volatile(
#ifndef __s390x__
		"	l	%0,%1\n"
		"0:	ltr	%0,%0\n"
		"	jnz	1f\n"
		"	cs	%0,%3,%1\n"
		"	jl	0b\n"
#else 
		"	lg	%0,%1\n"
		"0:	ltgr	%0,%0\n"
		"	jnz	1f\n"
		"	csg	%0,%3,%1\n"
		"	jl	0b\n"
#endif 
		"1:"
		: "=&d" (old), "=Q" (sem->count)
		: "Q" (sem->count), "d" (RWSEM_ACTIVE_WRITE_BIAS)
		: "cc", "memory");
	return (old == RWSEM_UNLOCKED_VALUE) ? 1 : 0;
}

static inline void __up_read(struct rw_semaphore *sem)
{
	signed long old, new;

	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	ahi	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	aghi	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "i" (-RWSEM_ACTIVE_READ_BIAS)
		: "cc", "memory");
	if (new < 0)
		if ((new & RWSEM_ACTIVE_MASK) == 0)
			rwsem_wake(sem);
}

static inline void __up_write(struct rw_semaphore *sem)
{
	signed long old, new, tmp;

	tmp = -RWSEM_ACTIVE_WRITE_BIAS;
	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	a	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	ag	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "m" (tmp)
		: "cc", "memory");
	if (new < 0)
		if ((new & RWSEM_ACTIVE_MASK) == 0)
			rwsem_wake(sem);
}

static inline void __downgrade_write(struct rw_semaphore *sem)
{
	signed long old, new, tmp;

	tmp = -RWSEM_WAITING_BIAS;
	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	a	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	ag	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "m" (tmp)
		: "cc", "memory");
	if (new > 1)
		rwsem_downgrade_wake(sem);
}

static inline void rwsem_atomic_add(long delta, struct rw_semaphore *sem)
{
	signed long old, new;

	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	ar	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	agr	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "d" (delta)
		: "cc", "memory");
}

static inline long rwsem_atomic_update(long delta, struct rw_semaphore *sem)
{
	signed long old, new;

	asm volatile(
#ifndef __s390x__
		"	l	%0,%2\n"
		"0:	lr	%1,%0\n"
		"	ar	%1,%4\n"
		"	cs	%0,%1,%2\n"
		"	jl	0b"
#else 
		"	lg	%0,%2\n"
		"0:	lgr	%1,%0\n"
		"	agr	%1,%4\n"
		"	csg	%0,%1,%2\n"
		"	jl	0b"
#endif 
		: "=&d" (old), "=&d" (new), "=Q" (sem->count)
		: "Q" (sem->count), "d" (delta)
		: "cc", "memory");
	return new;
}

#endif 
#endif 
