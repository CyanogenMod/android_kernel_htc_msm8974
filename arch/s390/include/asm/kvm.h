#ifndef __LINUX_KVM_S390_H
#define __LINUX_KVM_S390_H
/*
 * asm-s390/kvm.h - KVM s390 specific structures and definitions
 *
 * Copyright IBM Corp. 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2 only)
 * as published by the Free Software Foundation.
 *
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Christian Borntraeger <borntraeger@de.ibm.com>
 */
#include <linux/types.h>

#define __KVM_S390

struct kvm_regs {
	
	__u64 gprs[16];
};

struct kvm_sregs {
	__u32 acrs[16];
	__u64 crs[16];
};

struct kvm_fpu {
	__u32 fpc;
	__u64 fprs[16];
};

struct kvm_debug_exit_arch {
};

struct kvm_guest_debug_arch {
};

#define KVM_SYNC_PREFIX (1UL << 0)
#define KVM_SYNC_GPRS   (1UL << 1)
#define KVM_SYNC_ACRS   (1UL << 2)
#define KVM_SYNC_CRS    (1UL << 3)
struct kvm_sync_regs {
	__u64 prefix;	
	__u64 gprs[16];	
	__u32 acrs[16];	
	__u64 crs[16];	
};
#endif
