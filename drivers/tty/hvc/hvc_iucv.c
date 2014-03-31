/*
 * hvc_iucv.c - z/VM IUCV hypervisor console (HVC) device driver
 *
 * This HVC device driver provides terminal access using
 * z/VM IUCV communication paths.
 *
 * Copyright IBM Corp. 2008, 2009
 *
 * Author(s):	Hendrik Brueckner <brueckner@linux.vnet.ibm.com>
 */
#define KMSG_COMPONENT		"hvc_iucv"
#define pr_fmt(fmt)		KMSG_COMPONENT ": " fmt

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/ebcdic.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/mempool.h>
#include <linux/moduleparam.h>
#include <linux/tty.h>
#include <linux/wait.h>
#include <net/iucv/iucv.h>

#include "hvc_console.h"


#define HVC_IUCV_MAGIC		0xc9e4c3e5
#define MAX_HVC_IUCV_LINES	HVC_ALLOC_TTY_ADAPTERS
#define MEMPOOL_MIN_NR		(PAGE_SIZE / sizeof(struct iucv_tty_buffer)/4)

#define MSG_VERSION		0x02	
#define MSG_TYPE_ERROR		0x01	
#define MSG_TYPE_TERMENV	0x02	
#define MSG_TYPE_TERMIOS	0x04	
#define MSG_TYPE_WINSIZE	0x08	
#define MSG_TYPE_DATA		0x10	

struct iucv_tty_msg {
	u8	version;		
	u8	type;			
#define MSG_MAX_DATALEN		((u16)(~0))
	u16	datalen;		
	u8	data[];			
} __attribute__((packed));
#define MSG_SIZE(s)		((s) + offsetof(struct iucv_tty_msg, data))

enum iucv_state_t {
	IUCV_DISCONN	= 0,
	IUCV_CONNECTED	= 1,
	IUCV_SEVERED	= 2,
};

enum tty_state_t {
	TTY_CLOSED	= 0,
	TTY_OPENED	= 1,
};

struct hvc_iucv_private {
	struct hvc_struct	*hvc;		
	u8			srv_name[8];	
	unsigned char		is_console;	
	enum iucv_state_t	iucv_state;	
	enum tty_state_t	tty_state;	
	struct iucv_path	*path;		
	spinlock_t		lock;		
#define SNDBUF_SIZE		(PAGE_SIZE)	
	void			*sndbuf;	
	size_t			sndbuf_len;	
#define QUEUE_SNDBUF_DELAY	(HZ / 25)
	struct delayed_work	sndbuf_work;	
	wait_queue_head_t	sndbuf_waitq;	
	struct list_head	tty_outqueue;	
	struct list_head	tty_inqueue;	
	struct device		*dev;		
};

struct iucv_tty_buffer {
	struct list_head	list;	
	struct iucv_message	msg;	
	size_t			offset;	
	struct iucv_tty_msg	*mbuf;	
};

static	int hvc_iucv_path_pending(struct iucv_path *, u8[8], u8[16]);
static void hvc_iucv_path_severed(struct iucv_path *, u8[16]);
static void hvc_iucv_msg_pending(struct iucv_path *, struct iucv_message *);
static void hvc_iucv_msg_complete(struct iucv_path *, struct iucv_message *);


static unsigned long hvc_iucv_devices = 1;

static struct hvc_iucv_private *hvc_iucv_table[MAX_HVC_IUCV_LINES];
#define IUCV_HVC_CON_IDX	(0)
#define MAX_VMID_FILTER		(500)
static size_t hvc_iucv_filter_size;
static void *hvc_iucv_filter;
static const char *hvc_iucv_filter_string;
static DEFINE_RWLOCK(hvc_iucv_filter_lock);

static struct kmem_cache *hvc_iucv_buffer_cache;
static mempool_t *hvc_iucv_mempool;

static struct iucv_handler hvc_iucv_handler = {
	.path_pending  = hvc_iucv_path_pending,
	.path_severed  = hvc_iucv_path_severed,
	.message_complete = hvc_iucv_msg_complete,
	.message_pending  = hvc_iucv_msg_pending,
};


struct hvc_iucv_private *hvc_iucv_get_private(uint32_t num)
{
	if ((num < HVC_IUCV_MAGIC) || (num - HVC_IUCV_MAGIC > hvc_iucv_devices))
		return NULL;
	return hvc_iucv_table[num - HVC_IUCV_MAGIC];
}

static struct iucv_tty_buffer *alloc_tty_buffer(size_t size, gfp_t flags)
{
	struct iucv_tty_buffer *bufp;

	bufp = mempool_alloc(hvc_iucv_mempool, flags);
	if (!bufp)
		return NULL;
	memset(bufp, 0, sizeof(*bufp));

	if (size > 0) {
		bufp->msg.length = MSG_SIZE(size);
		bufp->mbuf = kmalloc(bufp->msg.length, flags | GFP_DMA);
		if (!bufp->mbuf) {
			mempool_free(bufp, hvc_iucv_mempool);
			return NULL;
		}
		bufp->mbuf->version = MSG_VERSION;
		bufp->mbuf->type    = MSG_TYPE_DATA;
		bufp->mbuf->datalen = (u16) size;
	}
	return bufp;
}

static void destroy_tty_buffer(struct iucv_tty_buffer *bufp)
{
	kfree(bufp->mbuf);
	mempool_free(bufp, hvc_iucv_mempool);
}

static void destroy_tty_buffer_list(struct list_head *list)
{
	struct iucv_tty_buffer *ent, *next;

	list_for_each_entry_safe(ent, next, list, list) {
		list_del(&ent->list);
		destroy_tty_buffer(ent);
	}
}

/**
 * hvc_iucv_write() - Receive IUCV message & write data to HVC buffer.
 * @priv:		Pointer to struct hvc_iucv_private
 * @buf:		HVC buffer for writing received terminal data.
 * @count:		HVC buffer size.
 * @has_more_data:	Pointer to an int variable.
 *
 * The function picks up pending messages from the input queue and receives
 * the message data that is then written to the specified buffer @buf.
 * If the buffer size @count is less than the data message size, the
 * message is kept on the input queue and @has_more_data is set to 1.
 * If all message data has been written, the message is removed from
 * the input queue.
 *
 * The function returns the number of bytes written to the terminal, zero if
 * there are no pending data messages available or if there is no established
 * IUCV path.
 * If the IUCV path has been severed, then -EPIPE is returned to cause a
 * hang up (that is issued by the HVC layer).
 */
static int hvc_iucv_write(struct hvc_iucv_private *priv,
			  char *buf, int count, int *has_more_data)
{
	struct iucv_tty_buffer *rb;
	int written;
	int rc;

	
	if (priv->iucv_state == IUCV_DISCONN)
		return 0;

	if (priv->iucv_state == IUCV_SEVERED)
		return -EPIPE;

	
	if (list_empty(&priv->tty_inqueue))
		return 0;

	
	rb = list_first_entry(&priv->tty_inqueue, struct iucv_tty_buffer, list);

	written = 0;
	if (!rb->mbuf) { 
		rb->mbuf = kmalloc(rb->msg.length, GFP_ATOMIC | GFP_DMA);
		if (!rb->mbuf)
			return -ENOMEM;

		rc = __iucv_message_receive(priv->path, &rb->msg, 0,
					    rb->mbuf, rb->msg.length, NULL);
		switch (rc) {
		case 0: 
			break;
		case 2:	
		case 9: 
			break;
		default:
			written = -EIO;
		}
		if (rc || (rb->mbuf->version != MSG_VERSION) ||
			  (rb->msg.length    != MSG_SIZE(rb->mbuf->datalen)))
			goto out_remove_buffer;
	}

	switch (rb->mbuf->type) {
	case MSG_TYPE_DATA:
		written = min_t(int, rb->mbuf->datalen - rb->offset, count);
		memcpy(buf, rb->mbuf->data + rb->offset, written);
		if (written < (rb->mbuf->datalen - rb->offset)) {
			rb->offset += written;
			*has_more_data = 1;
			goto out_written;
		}
		break;

	case MSG_TYPE_WINSIZE:
		if (rb->mbuf->datalen != sizeof(struct winsize))
			break;
		__hvc_resize(priv->hvc, *((struct winsize *) rb->mbuf->data));
		break;

	case MSG_TYPE_ERROR:	
	case MSG_TYPE_TERMENV:	
	case MSG_TYPE_TERMIOS:	
		break;
	}

out_remove_buffer:
	list_del(&rb->list);
	destroy_tty_buffer(rb);
	*has_more_data = !list_empty(&priv->tty_inqueue);

out_written:
	return written;
}

static int hvc_iucv_get_chars(uint32_t vtermno, char *buf, int count)
{
	struct hvc_iucv_private *priv = hvc_iucv_get_private(vtermno);
	int written;
	int has_more_data;

	if (count <= 0)
		return 0;

	if (!priv)
		return -ENODEV;

	spin_lock(&priv->lock);
	has_more_data = 0;
	written = hvc_iucv_write(priv, buf, count, &has_more_data);
	spin_unlock(&priv->lock);

	
	if (has_more_data)
		hvc_kick();

	return written;
}

static int hvc_iucv_queue(struct hvc_iucv_private *priv, const char *buf,
			  int count)
{
	size_t len;

	if (priv->iucv_state == IUCV_DISCONN)
		return count;			

	if (priv->iucv_state == IUCV_SEVERED)
		return -EPIPE;

	len = min_t(size_t, count, SNDBUF_SIZE - priv->sndbuf_len);
	if (!len)
		return 0;

	memcpy(priv->sndbuf + priv->sndbuf_len, buf, len);
	priv->sndbuf_len += len;

	if (priv->iucv_state == IUCV_CONNECTED)
		schedule_delayed_work(&priv->sndbuf_work, QUEUE_SNDBUF_DELAY);

	return len;
}

static int hvc_iucv_send(struct hvc_iucv_private *priv)
{
	struct iucv_tty_buffer *sb;
	int rc, len;

	if (priv->iucv_state == IUCV_SEVERED)
		return -EPIPE;

	if (priv->iucv_state == IUCV_DISCONN)
		return -EIO;

	if (!priv->sndbuf_len)
		return 0;

	sb = alloc_tty_buffer(priv->sndbuf_len, GFP_ATOMIC);
	if (!sb)
		return -ENOMEM;

	memcpy(sb->mbuf->data, priv->sndbuf, priv->sndbuf_len);
	sb->mbuf->datalen = (u16) priv->sndbuf_len;
	sb->msg.length = MSG_SIZE(sb->mbuf->datalen);

	list_add_tail(&sb->list, &priv->tty_outqueue);

	rc = __iucv_message_send(priv->path, &sb->msg, 0, 0,
				 (void *) sb->mbuf, sb->msg.length);
	if (rc) {
		list_del(&sb->list);
		destroy_tty_buffer(sb);
	}
	len = priv->sndbuf_len;
	priv->sndbuf_len = 0;

	return len;
}

static void hvc_iucv_sndbuf_work(struct work_struct *work)
{
	struct hvc_iucv_private *priv;

	priv = container_of(work, struct hvc_iucv_private, sndbuf_work.work);
	if (!priv)
		return;

	spin_lock_bh(&priv->lock);
	hvc_iucv_send(priv);
	spin_unlock_bh(&priv->lock);
}

static int hvc_iucv_put_chars(uint32_t vtermno, const char *buf, int count)
{
	struct hvc_iucv_private *priv = hvc_iucv_get_private(vtermno);
	int queued;

	if (count <= 0)
		return 0;

	if (!priv)
		return -ENODEV;

	spin_lock(&priv->lock);
	queued = hvc_iucv_queue(priv, buf, count);
	spin_unlock(&priv->lock);

	return queued;
}

static int hvc_iucv_notifier_add(struct hvc_struct *hp, int id)
{
	struct hvc_iucv_private *priv;

	priv = hvc_iucv_get_private(id);
	if (!priv)
		return 0;

	spin_lock_bh(&priv->lock);
	priv->tty_state = TTY_OPENED;
	spin_unlock_bh(&priv->lock);

	return 0;
}

static void hvc_iucv_cleanup(struct hvc_iucv_private *priv)
{
	destroy_tty_buffer_list(&priv->tty_outqueue);
	destroy_tty_buffer_list(&priv->tty_inqueue);

	priv->tty_state = TTY_CLOSED;
	priv->iucv_state = IUCV_DISCONN;

	priv->sndbuf_len = 0;
}

static inline int tty_outqueue_empty(struct hvc_iucv_private *priv)
{
	int rc;

	spin_lock_bh(&priv->lock);
	rc = list_empty(&priv->tty_outqueue);
	spin_unlock_bh(&priv->lock);

	return rc;
}

static void flush_sndbuf_sync(struct hvc_iucv_private *priv)
{
	int sync_wait;

	cancel_delayed_work_sync(&priv->sndbuf_work);

	spin_lock_bh(&priv->lock);
	hvc_iucv_send(priv);		
	sync_wait = !list_empty(&priv->tty_outqueue); 
	spin_unlock_bh(&priv->lock);

	if (sync_wait)
		wait_event_timeout(priv->sndbuf_waitq,
				   tty_outqueue_empty(priv), HZ/10);
}

static void hvc_iucv_hangup(struct hvc_iucv_private *priv)
{
	struct iucv_path *path;

	path = NULL;
	spin_lock(&priv->lock);
	if (priv->iucv_state == IUCV_CONNECTED) {
		path = priv->path;
		priv->path = NULL;
		priv->iucv_state = IUCV_SEVERED;
		if (priv->tty_state == TTY_CLOSED)
			hvc_iucv_cleanup(priv);
		else
			
			if (priv->is_console) {
				hvc_iucv_cleanup(priv);
				priv->tty_state = TTY_OPENED;
			} else
				hvc_kick();
	}
	spin_unlock(&priv->lock);

	
	if (path) {
		iucv_path_sever(path, NULL);
		iucv_path_free(path);
	}
}

static void hvc_iucv_notifier_hangup(struct hvc_struct *hp, int id)
{
	struct hvc_iucv_private *priv;

	priv = hvc_iucv_get_private(id);
	if (!priv)
		return;

	flush_sndbuf_sync(priv);

	spin_lock_bh(&priv->lock);
	priv->tty_state = TTY_CLOSED;

	if (priv->iucv_state == IUCV_SEVERED)
		hvc_iucv_cleanup(priv);
	spin_unlock_bh(&priv->lock);
}

static void hvc_iucv_notifier_del(struct hvc_struct *hp, int id)
{
	struct hvc_iucv_private *priv;
	struct iucv_path	*path;

	priv = hvc_iucv_get_private(id);
	if (!priv)
		return;

	flush_sndbuf_sync(priv);

	spin_lock_bh(&priv->lock);
	path = priv->path;		
	priv->path = NULL;
	hvc_iucv_cleanup(priv);
	spin_unlock_bh(&priv->lock);

	if (path) {
		iucv_path_sever(path, NULL);
		iucv_path_free(path);
	}
}

static int hvc_iucv_filter_connreq(u8 ipvmid[8])
{
	size_t i;

	
	if (!hvc_iucv_filter_size)
		return 0;

	for (i = 0; i < hvc_iucv_filter_size; i++)
		if (0 == memcmp(ipvmid, hvc_iucv_filter + (8 * i), 8))
			return 0;
	return 1;
}

static	int hvc_iucv_path_pending(struct iucv_path *path,
				  u8 ipvmid[8], u8 ipuser[16])
{
	struct hvc_iucv_private *priv;
	u8 nuser_data[16];
	u8 vm_user_id[9];
	int i, rc;

	priv = NULL;
	for (i = 0; i < hvc_iucv_devices; i++)
		if (hvc_iucv_table[i] &&
		    (0 == memcmp(hvc_iucv_table[i]->srv_name, ipuser, 8))) {
			priv = hvc_iucv_table[i];
			break;
		}
	if (!priv)
		return -ENODEV;

	
	read_lock(&hvc_iucv_filter_lock);
	rc = hvc_iucv_filter_connreq(ipvmid);
	read_unlock(&hvc_iucv_filter_lock);
	if (rc) {
		iucv_path_sever(path, ipuser);
		iucv_path_free(path);
		memcpy(vm_user_id, ipvmid, 8);
		vm_user_id[8] = 0;
		pr_info("A connection request from z/VM user ID %s "
			"was refused\n", vm_user_id);
		return 0;
	}

	spin_lock(&priv->lock);

	if (priv->iucv_state != IUCV_DISCONN) {
		iucv_path_sever(path, ipuser);
		iucv_path_free(path);
		goto out_path_handled;
	}

	
	memcpy(nuser_data, ipuser + 8, 8);  
	memcpy(nuser_data + 8, ipuser, 8);  
	path->msglim = 0xffff;		    
	path->flags &= ~IUCV_IPRMDATA;	    
	rc = iucv_path_accept(path, &hvc_iucv_handler, nuser_data, priv);
	if (rc) {
		iucv_path_sever(path, ipuser);
		iucv_path_free(path);
		goto out_path_handled;
	}
	priv->path = path;
	priv->iucv_state = IUCV_CONNECTED;

	
	schedule_delayed_work(&priv->sndbuf_work, 5);

out_path_handled:
	spin_unlock(&priv->lock);
	return 0;
}

static void hvc_iucv_path_severed(struct iucv_path *path, u8 ipuser[16])
{
	struct hvc_iucv_private *priv = path->private;

	hvc_iucv_hangup(priv);
}

static void hvc_iucv_msg_pending(struct iucv_path *path,
				 struct iucv_message *msg)
{
	struct hvc_iucv_private *priv = path->private;
	struct iucv_tty_buffer *rb;

	
	if (msg->length > MSG_SIZE(MSG_MAX_DATALEN)) {
		iucv_message_reject(path, msg);
		return;
	}

	spin_lock(&priv->lock);

	
	if (priv->tty_state == TTY_CLOSED) {
		iucv_message_reject(path, msg);
		goto unlock_return;
	}

	
	rb = alloc_tty_buffer(0, GFP_ATOMIC);
	if (!rb) {
		iucv_message_reject(path, msg);
		goto unlock_return;	
	}
	rb->msg = *msg;

	list_add_tail(&rb->list, &priv->tty_inqueue);

	hvc_kick();	

unlock_return:
	spin_unlock(&priv->lock);
}

static void hvc_iucv_msg_complete(struct iucv_path *path,
				  struct iucv_message *msg)
{
	struct hvc_iucv_private *priv = path->private;
	struct iucv_tty_buffer	*ent, *next;
	LIST_HEAD(list_remove);

	spin_lock(&priv->lock);
	list_for_each_entry_safe(ent, next, &priv->tty_outqueue, list)
		if (ent->msg.id == msg->id) {
			list_move(&ent->list, &list_remove);
			break;
		}
	wake_up(&priv->sndbuf_waitq);
	spin_unlock(&priv->lock);
	destroy_tty_buffer_list(&list_remove);
}

static int hvc_iucv_pm_freeze(struct device *dev)
{
	struct hvc_iucv_private *priv = dev_get_drvdata(dev);

	local_bh_disable();
	hvc_iucv_hangup(priv);
	local_bh_enable();

	return 0;
}

static int hvc_iucv_pm_restore_thaw(struct device *dev)
{
	hvc_kick();
	return 0;
}


static const struct hv_ops hvc_iucv_ops = {
	.get_chars = hvc_iucv_get_chars,
	.put_chars = hvc_iucv_put_chars,
	.notifier_add = hvc_iucv_notifier_add,
	.notifier_del = hvc_iucv_notifier_del,
	.notifier_hangup = hvc_iucv_notifier_hangup,
};

static const struct dev_pm_ops hvc_iucv_pm_ops = {
	.freeze	  = hvc_iucv_pm_freeze,
	.thaw	  = hvc_iucv_pm_restore_thaw,
	.restore  = hvc_iucv_pm_restore_thaw,
};

static struct device_driver hvc_iucv_driver = {
	.name = KMSG_COMPONENT,
	.bus  = &iucv_bus,
	.pm   = &hvc_iucv_pm_ops,
};

static int __init hvc_iucv_alloc(int id, unsigned int is_console)
{
	struct hvc_iucv_private *priv;
	char name[9];
	int rc;

	priv = kzalloc(sizeof(struct hvc_iucv_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	spin_lock_init(&priv->lock);
	INIT_LIST_HEAD(&priv->tty_outqueue);
	INIT_LIST_HEAD(&priv->tty_inqueue);
	INIT_DELAYED_WORK(&priv->sndbuf_work, hvc_iucv_sndbuf_work);
	init_waitqueue_head(&priv->sndbuf_waitq);

	priv->sndbuf = (void *) get_zeroed_page(GFP_KERNEL);
	if (!priv->sndbuf) {
		kfree(priv);
		return -ENOMEM;
	}

	
	priv->is_console = is_console;

	
	priv->hvc = hvc_alloc(HVC_IUCV_MAGIC + id, 
			      HVC_IUCV_MAGIC + id, &hvc_iucv_ops, 256);
	if (IS_ERR(priv->hvc)) {
		rc = PTR_ERR(priv->hvc);
		goto out_error_hvc;
	}

	
	priv->hvc->irq_requested = 1;

	
	snprintf(name, 9, "lnxhvc%-2d", id);
	memcpy(priv->srv_name, name, 8);
	ASCEBC(priv->srv_name, 8);

	
	priv->dev = kzalloc(sizeof(*priv->dev), GFP_KERNEL);
	if (!priv->dev) {
		rc = -ENOMEM;
		goto out_error_dev;
	}
	dev_set_name(priv->dev, "hvc_iucv%d", id);
	dev_set_drvdata(priv->dev, priv);
	priv->dev->bus = &iucv_bus;
	priv->dev->parent = iucv_root;
	priv->dev->driver = &hvc_iucv_driver;
	priv->dev->release = (void (*)(struct device *)) kfree;
	rc = device_register(priv->dev);
	if (rc) {
		put_device(priv->dev);
		goto out_error_dev;
	}

	hvc_iucv_table[id] = priv;
	return 0;

out_error_dev:
	hvc_remove(priv->hvc);
out_error_hvc:
	free_page((unsigned long) priv->sndbuf);
	kfree(priv);

	return rc;
}

static void __init hvc_iucv_destroy(struct hvc_iucv_private *priv)
{
	hvc_remove(priv->hvc);
	device_unregister(priv->dev);
	free_page((unsigned long) priv->sndbuf);
	kfree(priv);
}

static const char *hvc_iucv_parse_filter(const char *filter, char *dest)
{
	const char *nextdelim, *residual;
	size_t len;

	nextdelim = strchr(filter, ',');
	if (nextdelim) {
		len = nextdelim - filter;
		residual = nextdelim + 1;
	} else {
		len = strlen(filter);
		residual = filter + len;
	}

	if (len == 0)
		return ERR_PTR(-EINVAL);

	
	if (filter[len - 1] == '\n')
		len--;

	if (len > 8)
		return ERR_PTR(-EINVAL);

	
	memset(dest, ' ', 8);
	while (len--)
		dest[len] = toupper(filter[len]);
	return residual;
}

static int hvc_iucv_setup_filter(const char *val)
{
	const char *residual;
	int err;
	size_t size, count;
	void *array, *old_filter;

	count = strlen(val);
	if (count == 0 || (count == 1 && val[0] == '\n')) {
		size  = 0;
		array = NULL;
		goto out_replace_filter;	
	}

	
	size = 1;
	residual = val;
	while ((residual = strchr(residual, ',')) != NULL) {
		residual++;
		size++;
	}

	
	if (size > MAX_VMID_FILTER)
		return -ENOSPC;

	array = kzalloc(size * 8, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	count = size;
	residual = val;
	while (*residual && count) {
		residual = hvc_iucv_parse_filter(residual,
						 array + ((size - count) * 8));
		if (IS_ERR(residual)) {
			err = PTR_ERR(residual);
			kfree(array);
			goto out_err;
		}
		count--;
	}

out_replace_filter:
	write_lock_bh(&hvc_iucv_filter_lock);
	old_filter = hvc_iucv_filter;
	hvc_iucv_filter_size = size;
	hvc_iucv_filter = array;
	write_unlock_bh(&hvc_iucv_filter_lock);
	kfree(old_filter);

	err = 0;
out_err:
	return err;
}

static int param_set_vmidfilter(const char *val, const struct kernel_param *kp)
{
	int rc;

	if (!MACHINE_IS_VM || !hvc_iucv_devices)
		return -ENODEV;

	if (!val)
		return -EINVAL;

	rc = 0;
	if (slab_is_available())
		rc = hvc_iucv_setup_filter(val);
	else
		hvc_iucv_filter_string = val;	
	return rc;
}

static int param_get_vmidfilter(char *buffer, const struct kernel_param *kp)
{
	int rc;
	size_t index, len;
	void *start, *end;

	if (!MACHINE_IS_VM || !hvc_iucv_devices)
		return -ENODEV;

	rc = 0;
	read_lock_bh(&hvc_iucv_filter_lock);
	for (index = 0; index < hvc_iucv_filter_size; index++) {
		start = hvc_iucv_filter + (8 * index);
		end   = memchr(start, ' ', 8);
		len   = (end) ? end - start : 8;
		memcpy(buffer + rc, start, len);
		rc += len;
		buffer[rc++] = ',';
	}
	read_unlock_bh(&hvc_iucv_filter_lock);
	if (rc)
		buffer[--rc] = '\0';	
	return rc;
}

#define param_check_vmidfilter(name, p) __param_check(name, p, void)

static struct kernel_param_ops param_ops_vmidfilter = {
	.set = param_set_vmidfilter,
	.get = param_get_vmidfilter,
};

static int __init hvc_iucv_init(void)
{
	int rc;
	unsigned int i;

	if (!hvc_iucv_devices)
		return -ENODEV;

	if (!MACHINE_IS_VM) {
		pr_notice("The z/VM IUCV HVC device driver cannot "
			   "be used without z/VM\n");
		rc = -ENODEV;
		goto out_error;
	}

	if (hvc_iucv_devices > MAX_HVC_IUCV_LINES) {
		pr_err("%lu is not a valid value for the hvc_iucv= "
			"kernel parameter\n", hvc_iucv_devices);
		rc = -EINVAL;
		goto out_error;
	}

	
	rc = driver_register(&hvc_iucv_driver);
	if (rc)
		goto out_error;

	
	if (hvc_iucv_filter_string) {
		rc = hvc_iucv_setup_filter(hvc_iucv_filter_string);
		switch (rc) {
		case 0:
			break;
		case -ENOMEM:
			pr_err("Allocating memory failed with "
				"reason code=%d\n", 3);
			goto out_error;
		case -EINVAL:
			pr_err("hvc_iucv_allow= does not specify a valid "
				"z/VM user ID list\n");
			goto out_error;
		case -ENOSPC:
			pr_err("hvc_iucv_allow= specifies too many "
				"z/VM user IDs\n");
			goto out_error;
		default:
			goto out_error;
		}
	}

	hvc_iucv_buffer_cache = kmem_cache_create(KMSG_COMPONENT,
					   sizeof(struct iucv_tty_buffer),
					   0, 0, NULL);
	if (!hvc_iucv_buffer_cache) {
		pr_err("Allocating memory failed with reason code=%d\n", 1);
		rc = -ENOMEM;
		goto out_error;
	}

	hvc_iucv_mempool = mempool_create_slab_pool(MEMPOOL_MIN_NR,
						    hvc_iucv_buffer_cache);
	if (!hvc_iucv_mempool) {
		pr_err("Allocating memory failed with reason code=%d\n", 2);
		kmem_cache_destroy(hvc_iucv_buffer_cache);
		rc = -ENOMEM;
		goto out_error;
	}

	rc = hvc_instantiate(HVC_IUCV_MAGIC, IUCV_HVC_CON_IDX, &hvc_iucv_ops);
	if (rc) {
		pr_err("Registering HVC terminal device as "
		       "Linux console failed\n");
		goto out_error_memory;
	}

	
	for (i = 0; i < hvc_iucv_devices; i++) {
		rc = hvc_iucv_alloc(i, (i == IUCV_HVC_CON_IDX) ? 1 : 0);
		if (rc) {
			pr_err("Creating a new HVC terminal device "
				"failed with error code=%d\n", rc);
			goto out_error_hvc;
		}
	}

	
	rc = iucv_register(&hvc_iucv_handler, 0);
	if (rc) {
		pr_err("Registering IUCV handlers failed with error code=%d\n",
			rc);
		goto out_error_hvc;
	}

	return 0;

out_error_hvc:
	for (i = 0; i < hvc_iucv_devices; i++)
		if (hvc_iucv_table[i])
			hvc_iucv_destroy(hvc_iucv_table[i]);
out_error_memory:
	mempool_destroy(hvc_iucv_mempool);
	kmem_cache_destroy(hvc_iucv_buffer_cache);
out_error:
	if (hvc_iucv_filter)
		kfree(hvc_iucv_filter);
	hvc_iucv_devices = 0; 
	return rc;
}

static	int __init hvc_iucv_config(char *val)
{
	 return strict_strtoul(val, 10, &hvc_iucv_devices);
}


device_initcall(hvc_iucv_init);
__setup("hvc_iucv=", hvc_iucv_config);
core_param(hvc_iucv_allow, hvc_iucv_filter, vmidfilter, 0640);
