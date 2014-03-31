#ifndef _LINUX_VHOST_H
#define _LINUX_VHOST_H


#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/ioctl.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ring.h>

struct vhost_vring_state {
	unsigned int index;
	unsigned int num;
};

struct vhost_vring_file {
	unsigned int index;
	int fd; 

};

struct vhost_vring_addr {
	unsigned int index;
	
	unsigned int flags;
	
	
#define VHOST_VRING_F_LOG 0

	
	__u64 desc_user_addr;
	
	__u64 used_user_addr;
	
	__u64 avail_user_addr;
	
	__u64 log_guest_addr;
};

struct vhost_memory_region {
	__u64 guest_phys_addr;
	__u64 memory_size; 
	__u64 userspace_addr;
	__u64 flags_padding; 
};

#define VHOST_PAGE_SIZE 0x1000

struct vhost_memory {
	__u32 nregions;
	__u32 padding;
	struct vhost_memory_region regions[0];
};


#define VHOST_VIRTIO 0xAF

#define VHOST_GET_FEATURES	_IOR(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_FEATURES	_IOW(VHOST_VIRTIO, 0x00, __u64)

#define VHOST_SET_OWNER _IO(VHOST_VIRTIO, 0x01)
#define VHOST_RESET_OWNER _IO(VHOST_VIRTIO, 0x02)

#define VHOST_SET_MEM_TABLE	_IOW(VHOST_VIRTIO, 0x03, struct vhost_memory)

#define VHOST_SET_LOG_BASE _IOW(VHOST_VIRTIO, 0x04, __u64)
#define VHOST_SET_LOG_FD _IOW(VHOST_VIRTIO, 0x07, int)

#define VHOST_SET_VRING_NUM _IOW(VHOST_VIRTIO, 0x10, struct vhost_vring_state)
#define VHOST_SET_VRING_ADDR _IOW(VHOST_VIRTIO, 0x11, struct vhost_vring_addr)
#define VHOST_SET_VRING_BASE _IOW(VHOST_VIRTIO, 0x12, struct vhost_vring_state)
#define VHOST_GET_VRING_BASE _IOWR(VHOST_VIRTIO, 0x12, struct vhost_vring_state)


#define VHOST_SET_VRING_KICK _IOW(VHOST_VIRTIO, 0x20, struct vhost_vring_file)
#define VHOST_SET_VRING_CALL _IOW(VHOST_VIRTIO, 0x21, struct vhost_vring_file)
#define VHOST_SET_VRING_ERR _IOW(VHOST_VIRTIO, 0x22, struct vhost_vring_file)


#define VHOST_NET_SET_BACKEND _IOW(VHOST_VIRTIO, 0x30, struct vhost_vring_file)

#define VHOST_F_LOG_ALL 26
#define VHOST_NET_F_VIRTIO_NET_HDR 27

#endif
