/*
 * Virtio PCI driver
 *
 * This module allows virtio devices to be used over a virtual PCI device.
 * This can be used with QEMU based VMMs like KVM or Xen.
 *
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Anthony Liguori  <aliguori@us.ibm.com>
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUX_VIRTIO_PCI_H
#define _LINUX_VIRTIO_PCI_H

#include <linux/virtio_config.h>

#define VIRTIO_PCI_HOST_FEATURES	0

#define VIRTIO_PCI_GUEST_FEATURES	4

#define VIRTIO_PCI_QUEUE_PFN		8

#define VIRTIO_PCI_QUEUE_NUM		12

#define VIRTIO_PCI_QUEUE_SEL		14

#define VIRTIO_PCI_QUEUE_NOTIFY		16

#define VIRTIO_PCI_STATUS		18

#define VIRTIO_PCI_ISR			19

#define VIRTIO_PCI_ISR_CONFIG		0x2

#define VIRTIO_MSI_CONFIG_VECTOR        20
#define VIRTIO_MSI_QUEUE_VECTOR         22
#define VIRTIO_MSI_NO_VECTOR            0xffff

#define VIRTIO_PCI_CONFIG(dev)		((dev)->msix_enabled ? 24 : 20)

#define VIRTIO_PCI_ABI_VERSION		0

/* How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size. */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT	12

#define VIRTIO_PCI_VRING_ALIGN		4096
#endif
