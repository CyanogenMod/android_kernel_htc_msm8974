
#ifndef __G_ZERO_H
#define __G_ZERO_H

#include <linux/usb/composite.h>

extern unsigned buflen;
extern const struct usb_descriptor_header *otg_desc[];

struct usb_request *alloc_ep_req(struct usb_ep *ep);
void free_ep_req(struct usb_ep *ep, struct usb_request *req);
void disable_endpoints(struct usb_composite_dev *cdev,
		struct usb_ep *in, struct usb_ep *out);

int sourcesink_add(struct usb_composite_dev *cdev, bool autoresume);
int loopback_add(struct usb_composite_dev *cdev, bool autoresume);

#endif 
