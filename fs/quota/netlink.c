
#include <linux/cred.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/quotaops.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <net/netlink.h>
#include <net/genetlink.h>

static struct genl_family quota_genl_family = {
	.id = GENL_ID_GENERATE,
	.hdrsize = 0,
	.name = "VFS_DQUOT",
	.version = 1,
	.maxattr = QUOTA_NL_A_MAX,
};


void quota_send_warning(short type, unsigned int id, dev_t dev,
			const char warntype)
{
	static atomic_t seq;
	struct sk_buff *skb;
	void *msg_head;
	int ret;
	int msg_size = 4 * nla_total_size(sizeof(u32)) +
		       2 * nla_total_size(sizeof(u64));

	skb = genlmsg_new(msg_size, GFP_NOFS);
	if (!skb) {
		printk(KERN_ERR
		  "VFS: Not enough memory to send quota warning.\n");
		return;
	}
	msg_head = genlmsg_put(skb, 0, atomic_add_return(1, &seq),
			&quota_genl_family, 0, QUOTA_NL_C_WARNING);
	if (!msg_head) {
		printk(KERN_ERR
		  "VFS: Cannot store netlink header in quota warning.\n");
		goto err_out;
	}
	ret = nla_put_u32(skb, QUOTA_NL_A_QTYPE, type);
	if (ret)
		goto attr_err_out;
	ret = nla_put_u64(skb, QUOTA_NL_A_EXCESS_ID, id);
	if (ret)
		goto attr_err_out;
	ret = nla_put_u32(skb, QUOTA_NL_A_WARNING, warntype);
	if (ret)
		goto attr_err_out;
	ret = nla_put_u32(skb, QUOTA_NL_A_DEV_MAJOR, MAJOR(dev));
	if (ret)
		goto attr_err_out;
	ret = nla_put_u32(skb, QUOTA_NL_A_DEV_MINOR, MINOR(dev));
	if (ret)
		goto attr_err_out;
	ret = nla_put_u64(skb, QUOTA_NL_A_CAUSED_ID, current_uid());
	if (ret)
		goto attr_err_out;
	genlmsg_end(skb, msg_head);

	genlmsg_multicast(skb, 0, quota_genl_family.id, GFP_NOFS);
	return;
attr_err_out:
	printk(KERN_ERR "VFS: Not enough space to compose quota message!\n");
err_out:
	kfree_skb(skb);
}
EXPORT_SYMBOL(quota_send_warning);

static int __init quota_init(void)
{
	if (genl_register_family(&quota_genl_family) != 0)
		printk(KERN_ERR
		       "VFS: Failed to create quota netlink interface.\n");
	return 0;
};

module_init(quota_init);
