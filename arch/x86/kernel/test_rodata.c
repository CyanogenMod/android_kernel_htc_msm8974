/*
 * test_rodata.c: functional test for mark_rodata_ro function
 *
 * (C) Copyright 2008 Intel Corporation
 * Author: Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <asm/sections.h>

int rodata_test(void)
{
	unsigned long result;
	unsigned long start, end;

	
	
	if (!rodata_test_data) {
		printk(KERN_ERR "rodata_test: test 1 fails (start data)\n");
		return -ENODEV;
	}

	
	/*
	 * If this test fails, we managed to overwrite the data
	 *
	 * This is written in assembly to be able to catch the
	 * exception that is supposed to happen in the correct
	 * case
	 */

	result = 1;
	asm volatile(
		"0:	mov %[zero],(%[rodata_test])\n"
		"	mov %[zero], %[rslt]\n"
		"1:\n"
		".section .fixup,\"ax\"\n"
		"2:	jmp 1b\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		"       .align 16\n"
#ifdef CONFIG_X86_32
		"	.long 0b,2b\n"
#else
		"	.quad 0b,2b\n"
#endif
		".previous"
		: [rslt] "=r" (result)
		: [rodata_test] "r" (&rodata_test_data), [zero] "r" (0UL)
	);


	if (!result) {
		printk(KERN_ERR "rodata_test: test data was not read only\n");
		return -ENODEV;
	}

	
	
	if (!rodata_test_data) {
		printk(KERN_ERR "rodata_test: Test 3 failes (end data)\n");
		return -ENODEV;
	}
	
	start = (unsigned long)__start_rodata;
	end = (unsigned long)__end_rodata;
	if (start & (PAGE_SIZE - 1)) {
		printk(KERN_ERR "rodata_test: .rodata is not 4k aligned\n");
		return -ENODEV;
	}
	if (end & (PAGE_SIZE - 1)) {
		printk(KERN_ERR "rodata_test: .rodata end is not 4k aligned\n");
		return -ENODEV;
	}

	return 0;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Testcase for the DEBUG_RODATA infrastructure");
MODULE_AUTHOR("Arjan van de Ven <arjan@linux.intel.com>");
