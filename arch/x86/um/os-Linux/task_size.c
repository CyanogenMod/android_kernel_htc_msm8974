#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include "longjmp.h"

#ifdef __i386__

static jmp_buf buf;

static void segfault(int sig)
{
	longjmp(buf, 1);
}

static int page_ok(unsigned long page)
{
	unsigned long *address = (unsigned long *) (page << UM_KERN_PAGE_SHIFT);
	unsigned long n = ~0UL;
	void *mapped = NULL;
	int ok = 0;

	if (setjmp(buf) == 0)
		n = *address;
	else {
		mapped = mmap(address, UM_KERN_PAGE_SIZE,
			      PROT_READ | PROT_WRITE,
			      MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (mapped == MAP_FAILED)
			return 0;
		if (mapped != address)
			goto out;
	}

	if (setjmp(buf) == 0) {
		*address = n;
		ok = 1;
		goto out;
	} else if (mprotect(address, UM_KERN_PAGE_SIZE,
			    PROT_READ | PROT_WRITE) != 0)
		goto out;

	if (setjmp(buf) == 0) {
		*address = n;
		ok = 1;
	}

 out:
	if (mapped != NULL)
		munmap(mapped, UM_KERN_PAGE_SIZE);
	return ok;
}

unsigned long os_get_top_address(void)
{
	struct sigaction sa, old;
	unsigned long bottom = 0;
	/*
	 * A 32-bit UML on a 64-bit host gets confused about the VDSO at
	 * 0xffffe000.  It is mapped, is readable, can be reprotected writeable
	 * and written.  However, exec discovers later that it can't be
	 * unmapped.  So, just set the highest address to be checked to just
	 * below it.  This might waste some address space on 4G/4G 32-bit
	 * hosts, but shouldn't hurt otherwise.
	 */
	unsigned long top = 0xffffd000 >> UM_KERN_PAGE_SHIFT;
	unsigned long test, original;

	printf("Locating the bottom of the address space ... ");
	fflush(stdout);

	sa.sa_handler = segfault;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NODEFER;
	if (sigaction(SIGSEGV, &sa, &old)) {
		perror("os_get_top_address");
		exit(1);
	}

	for (bottom = 0; bottom < top; bottom++) {
		if (page_ok(bottom))
			break;
	}

	
	if (bottom == top) {
		fprintf(stderr, "Unable to determine bottom of address "
			"space.\n");
		exit(1);
	}

	printf("0x%x\n", bottom << UM_KERN_PAGE_SHIFT);
	printf("Locating the top of the address space ... ");
	fflush(stdout);

	original = bottom;

	
	if (page_ok(top))
		goto out;

	do {
		test = bottom + (top - bottom) / 2;
		if (page_ok(test))
			bottom = test;
		else
			top = test;
	} while (top - bottom > 1);

out:
	
	if (sigaction(SIGSEGV, &old, NULL)) {
		perror("os_get_top_address");
		exit(1);
	}
	top <<= UM_KERN_PAGE_SHIFT;
	printf("0x%x\n", top);

	return top;
}

#else

unsigned long os_get_top_address(void)
{
	
	return 0x7fc0000000;
}

#endif
