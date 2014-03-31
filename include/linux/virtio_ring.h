#ifndef _LINUX_VIRTIO_RING_H
#define _LINUX_VIRTIO_RING_H
/* An interface for efficient virtio implementation, currently for use by KVM
 * and lguest, but hopefully others soon.  Do NOT change this since it will
 * break existing servers and clients.
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
 *
 * Copyright Rusty Russell IBM Corporation 2007. */
#include <linux/types.h>

#define VRING_DESC_F_NEXT	1
#define VRING_DESC_F_WRITE	2
#define VRING_DESC_F_INDIRECT	4

#define VRING_USED_F_NO_NOTIFY	1
#define VRING_AVAIL_F_NO_INTERRUPT	1

#define VIRTIO_RING_F_INDIRECT_DESC	28

#define VIRTIO_RING_F_EVENT_IDX		29

struct vring_desc {
	
	__u64 addr;
	
	__u32 len;
	
	__u16 flags;
	
	__u16 next;
};

struct vring_avail {
	__u16 flags;
	__u16 idx;
	__u16 ring[];
};

struct vring_used_elem {
	
	__u32 id;
	/* Total length of the descriptor chain which was used (written to) */
	__u32 len;
};

struct vring_used {
	__u16 flags;
	__u16 idx;
	struct vring_used_elem ring[];
};

struct vring {
	unsigned int num;

	struct vring_desc *desc;

	struct vring_avail *avail;

	struct vring_used *used;
};

#define vring_used_event(vr) ((vr)->avail->ring[(vr)->num])
#define vring_avail_event(vr) (*(__u16 *)&(vr)->used->ring[(vr)->num])

static inline void vring_init(struct vring *vr, unsigned int num, void *p,
			      unsigned long align)
{
	vr->num = num;
	vr->desc = p;
	vr->avail = p + num*sizeof(struct vring_desc);
	vr->used = (void *)(((unsigned long)&vr->avail->ring[num] + sizeof(__u16)
		+ align-1) & ~(align - 1));
}

static inline unsigned vring_size(unsigned int num, unsigned long align)
{
	return ((sizeof(struct vring_desc) * num + sizeof(__u16) * (3 + num)
		 + align - 1) & ~(align - 1))
		+ sizeof(__u16) * 3 + sizeof(struct vring_used_elem) * num;
}

static inline int vring_need_event(__u16 event_idx, __u16 new_idx, __u16 old)
{
	return (__u16)(new_idx - event_idx - 1) < (__u16)(new_idx - old);
}

#ifdef __KERNEL__
#include <linux/irqreturn.h>
struct virtio_device;
struct virtqueue;

struct virtqueue *vring_new_virtqueue(unsigned int num,
				      unsigned int vring_align,
				      struct virtio_device *vdev,
				      bool weak_barriers,
				      void *pages,
				      void (*notify)(struct virtqueue *vq),
				      void (*callback)(struct virtqueue *vq),
				      const char *name);
void vring_del_virtqueue(struct virtqueue *vq);
void vring_transport_features(struct virtio_device *vdev);

irqreturn_t vring_interrupt(int irq, void *_vq);
#endif 
#endif 
