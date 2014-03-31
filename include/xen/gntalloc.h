
#ifndef __LINUX_PUBLIC_GNTALLOC_H__
#define __LINUX_PUBLIC_GNTALLOC_H__

#define IOCTL_GNTALLOC_ALLOC_GREF \
_IOC(_IOC_NONE, 'G', 5, sizeof(struct ioctl_gntalloc_alloc_gref))
struct ioctl_gntalloc_alloc_gref {
	
	
	uint16_t domid;
	
	uint16_t flags;
	
	uint32_t count;
	
	
	uint64_t index;
	
	
	uint32_t gref_ids[1];
};

#define GNTALLOC_FLAG_WRITABLE 1

#define IOCTL_GNTALLOC_DEALLOC_GREF \
_IOC(_IOC_NONE, 'G', 6, sizeof(struct ioctl_gntalloc_dealloc_gref))
struct ioctl_gntalloc_dealloc_gref {
	
	
	uint64_t index;
	
	uint32_t count;
};

#define IOCTL_GNTALLOC_SET_UNMAP_NOTIFY \
_IOC(_IOC_NONE, 'G', 7, sizeof(struct ioctl_gntalloc_unmap_notify))
struct ioctl_gntalloc_unmap_notify {
	
	uint64_t index;
	
	uint32_t action;
	
	uint32_t event_channel_port;
};

#define UNMAP_NOTIFY_CLEAR_BYTE 0x1
#define UNMAP_NOTIFY_SEND_EVENT 0x2

#endif 
