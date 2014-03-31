/*
 * kvm_virtio.h - definition for virtio for kvm on s390
 *
 * Copyright IBM Corp. 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2 only)
 * as published by the Free Software Foundation.
 *
 *    Author(s): Christian Borntraeger <borntraeger@de.ibm.com>
 */

#ifndef __KVM_S390_VIRTIO_H
#define __KVM_S390_VIRTIO_H

#include <linux/types.h>

struct kvm_device_desc {
	
	__u8 type;
	
	__u8 num_vq;
	__u8 feature_len;
	
	__u8 config_len;
	/* A status byte, written by the Guest. */
	__u8 status;
	__u8 config[0];
};

struct kvm_vqconfig {
	
	__u64 token;
	
	__u64 address;
	
	__u16 num;

};

#define KVM_S390_VIRTIO_NOTIFY		0
#define KVM_S390_VIRTIO_RESET		1
#define KVM_S390_VIRTIO_SET_STATUS	2

#define KVM_S390_VIRTIO_RING_ALIGN	4096


#define VIRTIO_PARAM_MASK		0xff
#define VIRTIO_PARAM_VRING_INTERRUPT	0x0
#define VIRTIO_PARAM_CONFIG_CHANGED	0x1
#define VIRTIO_PARAM_DEV_ADD		0x2

#endif
