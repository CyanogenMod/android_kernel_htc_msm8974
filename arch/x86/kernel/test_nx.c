/*
 * test_nx.c: functional test for NX functionality
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
#include <linux/sort.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/asm.h>

extern int rodata_test_data;




static void fudze_exception_table(void *marker, void *new)
{
	struct module *mod = THIS_MODULE;
	struct exception_table_entry *extable;

	if (mod->num_exentries > 1) {
		printk(KERN_ERR "test_nx: too many exception table entries!\n");
		printk(KERN_ERR "test_nx: test results are not reliable.\n");
		return;
	}
	extable = (struct exception_table_entry *)mod->extable;
	extable[0].insn = (unsigned long)new;
}


void foo_label(void);

static noinline int test_address(void *address)
{
	unsigned long result;

	
	fudze_exception_table(&foo_label, address);
	result = 1;
	asm volatile(
		"foo_label:\n"
		"0:	call *%[fake_code]\n"
		"1:\n"
		".section .fixup,\"ax\"\n"
		"2:	mov %[zero], %[rslt]\n"
		"	ret\n"
		".previous\n"
		_ASM_EXTABLE(0b,2b)
		: [rslt] "=r" (result)
		: [fake_code] "r" (address), [zero] "r" (0UL), "0" (result)
	);
	
	fudze_exception_table(address, &foo_label);

	if (result)
		return -ENODEV;
	return 0;
}

static unsigned char test_data = 0xC3; 

static int test_NX(void)
{
	int ret = 0;
	
	char stackcode[] = {0xC3, 0x90, 0 };
	char *heap;

	test_data = 0xC3;

	printk(KERN_INFO "Testing NX protection\n");

	
	if (test_address(&stackcode)) {
		printk(KERN_ERR "test_nx: stack was executable\n");
		ret = -ENODEV;
	}


	
	heap = kmalloc(64, GFP_KERNEL);
	if (!heap)
		return -ENOMEM;
	heap[0] = 0xC3; 

	if (test_address(heap)) {
		printk(KERN_ERR "test_nx: heap was executable\n");
		ret = -ENODEV;
	}
	kfree(heap);


#ifdef CONFIG_DEBUG_RODATA
	
	if (rodata_test_data != 0xC3) {
		printk(KERN_ERR "test_nx: .rodata marker has invalid value\n");
		ret = -ENODEV;
	} else if (test_address(&rodata_test_data)) {
		printk(KERN_ERR "test_nx: .rodata section is executable\n");
		ret = -ENODEV;
	}
#endif

#if 0
	
	if (test_address(&test_data)) {
		printk(KERN_ERR "test_nx: .data section is executable\n");
		ret = -ENODEV;
	}

#endif
	return ret;
}

static void test_exit(void)
{
}

module_init(test_NX);
module_exit(test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Testcase for the NX infrastructure");
MODULE_AUTHOR("Arjan van de Ven <arjan@linux.intel.com>");
