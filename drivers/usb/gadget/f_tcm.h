#ifndef __TARGET_USB_GADGET_H__
#define __TARGET_USB_GADGET_H__

#include <linux/kref.h>
#include <linux/usb/composite.h>
#include <linux/usb/uas.h>
#include <linux/usb/storage.h>
#include <scsi/scsi.h>
#include <target/target_core_base.h>
#include <target/target_core_fabric.h>

#define USBG_NAMELEN 32

#define fuas_to_gadget(f)	(f->function.config->cdev->gadget)
#define UASP_SS_EP_COMP_LOG_STREAMS 4
#define UASP_SS_EP_COMP_NUM_STREAMS (1 << UASP_SS_EP_COMP_LOG_STREAMS)

#define USB_G_ALT_INT_BBB       0
#define USB_G_ALT_INT_UAS       1

struct usbg_nacl {
	
	u64 iport_wwpn;
	
	char iport_name[USBG_NAMELEN];
	
	struct se_node_acl se_node_acl;
};

struct tcm_usbg_nexus {
	struct se_session *tvn_se_sess;
};

struct usbg_tpg {
	struct mutex tpg_mutex;
	
	u16 tport_tpgt;
	
	struct usbg_tport *tport;
	struct workqueue_struct *workqueue;
	
	struct se_portal_group se_tpg;
	u32 gadget_connect;
	struct tcm_usbg_nexus *tpg_nexus;
	atomic_t tpg_port_count;
};

struct usbg_tport {
	
	u8 tport_proto_id;
	
	u64 tport_wwpn;
	
	char tport_name[USBG_NAMELEN];
	
	struct se_wwn tport_wwn;
};

enum uas_state {
	UASP_SEND_DATA,
	UASP_RECEIVE_DATA,
	UASP_SEND_STATUS,
	UASP_QUEUE_COMMAND,
};

#define USBG_MAX_CMD    64
struct usbg_cmd {
	
	u8 cmd_buf[USBG_MAX_CMD];
	u32 data_len;
	struct work_struct work;
	int unpacked_lun;
	struct se_cmd se_cmd;
	void *data_buf; 
	struct f_uas *fu;
	struct completion write_complete;
	struct kref ref;

	
	u16 tag;
	u16 prio_attr;
	struct sense_iu sense_iu;
	enum uas_state state;
	struct uas_stream *stream;

	
	__le32 bot_tag;
	unsigned int csw_code;
	unsigned is_read:1;

};

struct uas_stream {
	struct usb_request	*req_in;
	struct usb_request	*req_out;
	struct usb_request	*req_status;
};

struct usbg_cdb {
	struct usb_request	*req;
	void			*buf;
};

struct bot_status {
	struct usb_request	*req;
	struct bulk_cs_wrap	csw;
};

struct f_uas {
	struct usbg_tpg		*tpg;
	struct usb_function	function;
	u16			iface;

	u32			flags;
#define USBG_ENABLED		(1 << 0)
#define USBG_IS_UAS		(1 << 1)
#define USBG_USE_STREAMS	(1 << 2)
#define USBG_IS_BOT		(1 << 3)
#define USBG_BOT_CMD_PEND	(1 << 4)

	struct usbg_cdb		cmd;
	struct usb_ep		*ep_in;
	struct usb_ep		*ep_out;

	
	struct usb_ep		*ep_status;
	struct usb_ep		*ep_cmd;
	struct uas_stream	stream[UASP_SS_EP_COMP_NUM_STREAMS];

	
	struct bot_status	bot_status;
	struct usb_request	*bot_req_in;
	struct usb_request	*bot_req_out;
};

extern struct usbg_tpg *the_only_tpg_I_currently_have;

#endif
