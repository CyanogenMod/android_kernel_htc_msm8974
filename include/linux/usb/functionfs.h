#ifndef __LINUX_FUNCTIONFS_H__
#define __LINUX_FUNCTIONFS_H__ 1


#include <linux/types.h>
#include <linux/ioctl.h>

#include <linux/usb/ch9.h>


enum {
	FUNCTIONFS_DESCRIPTORS_MAGIC = 1,
	FUNCTIONFS_STRINGS_MAGIC     = 2
};


#ifndef __KERNEL__

struct usb_endpoint_descriptor_no_audio {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bEndpointAddress;
	__u8  bmAttributes;
	__le16 wMaxPacketSize;
	__u8  bInterval;
} __attribute__((packed));



struct usb_functionfs_descs_head {
	__le32 magic;
	__le32 length;
	__le32 fs_count;
	__le32 hs_count;
} __attribute__((packed));


struct usb_functionfs_strings_head {
	__le32 magic;
	__le32 length;
	__le32 str_count;
	__le32 lang_count;
} __attribute__((packed));


#endif



enum usb_functionfs_event_type {
	FUNCTIONFS_BIND,
	FUNCTIONFS_UNBIND,

	FUNCTIONFS_ENABLE,
	FUNCTIONFS_DISABLE,

	FUNCTIONFS_SETUP,

	FUNCTIONFS_SUSPEND,
	FUNCTIONFS_RESUME
};

struct usb_functionfs_event {
	union {
		struct usb_ctrlrequest	setup;
	} __attribute__((packed)) u;

	
	__u8				type;
	__u8				_pad[3];
} __attribute__((packed));



#define	FUNCTIONFS_FIFO_STATUS	_IO('g', 1)

#define	FUNCTIONFS_FIFO_FLUSH	_IO('g', 2)

#define	FUNCTIONFS_CLEAR_HALT	_IO('g', 3)


#define	FUNCTIONFS_INTERFACE_REVMAP	_IO('g', 128)

#define	FUNCTIONFS_ENDPOINT_REVMAP	_IO('g', 129)


#ifdef __KERNEL__

struct ffs_data;
struct usb_composite_dev;
struct usb_configuration;


static int  functionfs_init(void) __attribute__((warn_unused_result));
static void functionfs_cleanup(void);

static int functionfs_bind(struct ffs_data *ffs, struct usb_composite_dev *cdev)
	__attribute__((warn_unused_result, nonnull));
static void functionfs_unbind(struct ffs_data *ffs)
	__attribute__((nonnull));

static int functionfs_bind_config(struct usb_composite_dev *cdev,
				  struct usb_configuration *c,
				  struct ffs_data *ffs)
	__attribute__((warn_unused_result, nonnull));


static int functionfs_ready_callback(struct ffs_data *ffs)
	__attribute__((warn_unused_result, nonnull));
static void functionfs_closed_callback(struct ffs_data *ffs)
	__attribute__((nonnull));
static int functionfs_check_dev_callback(const char *dev_name)
	__attribute__((warn_unused_result, nonnull));


#endif

#endif
