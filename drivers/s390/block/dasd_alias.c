/*
 * PAV alias management for the DASD ECKD discipline
 *
 * Copyright IBM Corporation, 2007
 * Author(s): Stefan Weinhuber <wein@de.ibm.com>
 */

#define KMSG_COMPONENT "dasd-eckd"

#include <linux/list.h>
#include <linux/slab.h>
#include <asm/ebcdic.h>
#include "dasd_int.h"
#include "dasd_eckd.h"

#ifdef PRINTK_HEADER
#undef PRINTK_HEADER
#endif				
#define PRINTK_HEADER "dasd(eckd):"




static void summary_unit_check_handling_work(struct work_struct *);
static void lcu_update_work(struct work_struct *);
static int _schedule_lcu_update(struct alias_lcu *, struct dasd_device *);

static struct alias_root aliastree = {
	.serverlist = LIST_HEAD_INIT(aliastree.serverlist),
	.lock = __SPIN_LOCK_UNLOCKED(aliastree.lock),
};

static struct alias_server *_find_server(struct dasd_uid *uid)
{
	struct alias_server *pos;
	list_for_each_entry(pos, &aliastree.serverlist, server) {
		if (!strncmp(pos->uid.vendor, uid->vendor,
			     sizeof(uid->vendor))
		    && !strncmp(pos->uid.serial, uid->serial,
				sizeof(uid->serial)))
			return pos;
	};
	return NULL;
}

static struct alias_lcu *_find_lcu(struct alias_server *server,
				   struct dasd_uid *uid)
{
	struct alias_lcu *pos;
	list_for_each_entry(pos, &server->lculist, lcu) {
		if (pos->uid.ssid == uid->ssid)
			return pos;
	};
	return NULL;
}

static struct alias_pav_group *_find_group(struct alias_lcu *lcu,
					   struct dasd_uid *uid)
{
	struct alias_pav_group *pos;
	__u8 search_unit_addr;

	
	if (lcu->pav == HYPER_PAV) {
		if (list_empty(&lcu->grouplist))
			return NULL;
		else
			return list_first_entry(&lcu->grouplist,
						struct alias_pav_group, group);
	}

	
	if (uid->type == UA_BASE_DEVICE)
		search_unit_addr = uid->real_unit_addr;
	else
		search_unit_addr = uid->base_unit_addr;
	list_for_each_entry(pos, &lcu->grouplist, group) {
		if (pos->uid.base_unit_addr == search_unit_addr &&
		    !strncmp(pos->uid.vduit, uid->vduit, sizeof(uid->vduit)))
			return pos;
	};
	return NULL;
}

static struct alias_server *_allocate_server(struct dasd_uid *uid)
{
	struct alias_server *server;

	server = kzalloc(sizeof(*server), GFP_KERNEL);
	if (!server)
		return ERR_PTR(-ENOMEM);
	memcpy(server->uid.vendor, uid->vendor, sizeof(uid->vendor));
	memcpy(server->uid.serial, uid->serial, sizeof(uid->serial));
	INIT_LIST_HEAD(&server->server);
	INIT_LIST_HEAD(&server->lculist);
	return server;
}

static void _free_server(struct alias_server *server)
{
	kfree(server);
}

static struct alias_lcu *_allocate_lcu(struct dasd_uid *uid)
{
	struct alias_lcu *lcu;

	lcu = kzalloc(sizeof(*lcu), GFP_KERNEL);
	if (!lcu)
		return ERR_PTR(-ENOMEM);
	lcu->uac = kzalloc(sizeof(*(lcu->uac)), GFP_KERNEL | GFP_DMA);
	if (!lcu->uac)
		goto out_err1;
	lcu->rsu_cqr = kzalloc(sizeof(*lcu->rsu_cqr), GFP_KERNEL | GFP_DMA);
	if (!lcu->rsu_cqr)
		goto out_err2;
	lcu->rsu_cqr->cpaddr = kzalloc(sizeof(struct ccw1),
				       GFP_KERNEL | GFP_DMA);
	if (!lcu->rsu_cqr->cpaddr)
		goto out_err3;
	lcu->rsu_cqr->data = kzalloc(16, GFP_KERNEL | GFP_DMA);
	if (!lcu->rsu_cqr->data)
		goto out_err4;

	memcpy(lcu->uid.vendor, uid->vendor, sizeof(uid->vendor));
	memcpy(lcu->uid.serial, uid->serial, sizeof(uid->serial));
	lcu->uid.ssid = uid->ssid;
	lcu->pav = NO_PAV;
	lcu->flags = NEED_UAC_UPDATE | UPDATE_PENDING;
	INIT_LIST_HEAD(&lcu->lcu);
	INIT_LIST_HEAD(&lcu->inactive_devices);
	INIT_LIST_HEAD(&lcu->active_devices);
	INIT_LIST_HEAD(&lcu->grouplist);
	INIT_WORK(&lcu->suc_data.worker, summary_unit_check_handling_work);
	INIT_DELAYED_WORK(&lcu->ruac_data.dwork, lcu_update_work);
	spin_lock_init(&lcu->lock);
	init_completion(&lcu->lcu_setup);
	return lcu;

out_err4:
	kfree(lcu->rsu_cqr->cpaddr);
out_err3:
	kfree(lcu->rsu_cqr);
out_err2:
	kfree(lcu->uac);
out_err1:
	kfree(lcu);
	return ERR_PTR(-ENOMEM);
}

static void _free_lcu(struct alias_lcu *lcu)
{
	kfree(lcu->rsu_cqr->data);
	kfree(lcu->rsu_cqr->cpaddr);
	kfree(lcu->rsu_cqr);
	kfree(lcu->uac);
	kfree(lcu);
}

int dasd_alias_make_device_known_to_lcu(struct dasd_device *device)
{
	struct dasd_eckd_private *private;
	unsigned long flags;
	struct alias_server *server, *newserver;
	struct alias_lcu *lcu, *newlcu;
	struct dasd_uid uid;

	private = (struct dasd_eckd_private *) device->private;

	device->discipline->get_uid(device, &uid);
	spin_lock_irqsave(&aliastree.lock, flags);
	server = _find_server(&uid);
	if (!server) {
		spin_unlock_irqrestore(&aliastree.lock, flags);
		newserver = _allocate_server(&uid);
		if (IS_ERR(newserver))
			return PTR_ERR(newserver);
		spin_lock_irqsave(&aliastree.lock, flags);
		server = _find_server(&uid);
		if (!server) {
			list_add(&newserver->server, &aliastree.serverlist);
			server = newserver;
		} else {
			
			_free_server(newserver);
		}
	}

	lcu = _find_lcu(server, &uid);
	if (!lcu) {
		spin_unlock_irqrestore(&aliastree.lock, flags);
		newlcu = _allocate_lcu(&uid);
		if (IS_ERR(newlcu))
			return PTR_ERR(newlcu);
		spin_lock_irqsave(&aliastree.lock, flags);
		lcu = _find_lcu(server, &uid);
		if (!lcu) {
			list_add(&newlcu->lcu, &server->lculist);
			lcu = newlcu;
		} else {
			
			_free_lcu(newlcu);
		}
	}
	spin_lock(&lcu->lock);
	list_add(&device->alias_list, &lcu->inactive_devices);
	private->lcu = lcu;
	spin_unlock(&lcu->lock);
	spin_unlock_irqrestore(&aliastree.lock, flags);

	return 0;
}

void dasd_alias_disconnect_device_from_lcu(struct dasd_device *device)
{
	struct dasd_eckd_private *private;
	unsigned long flags;
	struct alias_lcu *lcu;
	struct alias_server *server;
	int was_pending;
	struct dasd_uid uid;

	private = (struct dasd_eckd_private *) device->private;
	lcu = private->lcu;
	
	if (!lcu)
		return;
	device->discipline->get_uid(device, &uid);
	spin_lock_irqsave(&lcu->lock, flags);
	list_del_init(&device->alias_list);
	
	if (device == lcu->suc_data.device) {
		spin_unlock_irqrestore(&lcu->lock, flags);
		cancel_work_sync(&lcu->suc_data.worker);
		spin_lock_irqsave(&lcu->lock, flags);
		if (device == lcu->suc_data.device)
			lcu->suc_data.device = NULL;
	}
	was_pending = 0;
	if (device == lcu->ruac_data.device) {
		spin_unlock_irqrestore(&lcu->lock, flags);
		was_pending = 1;
		cancel_delayed_work_sync(&lcu->ruac_data.dwork);
		spin_lock_irqsave(&lcu->lock, flags);
		if (device == lcu->ruac_data.device)
			lcu->ruac_data.device = NULL;
	}
	private->lcu = NULL;
	spin_unlock_irqrestore(&lcu->lock, flags);

	spin_lock_irqsave(&aliastree.lock, flags);
	spin_lock(&lcu->lock);
	if (list_empty(&lcu->grouplist) &&
	    list_empty(&lcu->active_devices) &&
	    list_empty(&lcu->inactive_devices)) {
		list_del(&lcu->lcu);
		spin_unlock(&lcu->lock);
		_free_lcu(lcu);
		lcu = NULL;
	} else {
		if (was_pending)
			_schedule_lcu_update(lcu, NULL);
		spin_unlock(&lcu->lock);
	}
	server = _find_server(&uid);
	if (server && list_empty(&server->lculist)) {
		list_del(&server->server);
		_free_server(server);
	}
	spin_unlock_irqrestore(&aliastree.lock, flags);
}


static int _add_device_to_lcu(struct alias_lcu *lcu,
			      struct dasd_device *device,
			      struct dasd_device *pos)
{

	struct dasd_eckd_private *private;
	struct alias_pav_group *group;
	struct dasd_uid uid;
	unsigned long flags;

	private = (struct dasd_eckd_private *) device->private;

	
	if (device != pos)
		spin_lock_irqsave_nested(get_ccwdev_lock(device->cdev), flags,
					 CDEV_NESTED_SECOND);
	private->uid.type = lcu->uac->unit[private->uid.real_unit_addr].ua_type;
	private->uid.base_unit_addr =
		lcu->uac->unit[private->uid.real_unit_addr].base_ua;
	uid = private->uid;

	if (device != pos)
		spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);

	
	if (lcu->pav == NO_PAV) {
		list_move(&device->alias_list, &lcu->active_devices);
		return 0;
	}

	group = _find_group(lcu, &uid);
	if (!group) {
		group = kzalloc(sizeof(*group), GFP_ATOMIC);
		if (!group)
			return -ENOMEM;
		memcpy(group->uid.vendor, uid.vendor, sizeof(uid.vendor));
		memcpy(group->uid.serial, uid.serial, sizeof(uid.serial));
		group->uid.ssid = uid.ssid;
		if (uid.type == UA_BASE_DEVICE)
			group->uid.base_unit_addr = uid.real_unit_addr;
		else
			group->uid.base_unit_addr = uid.base_unit_addr;
		memcpy(group->uid.vduit, uid.vduit, sizeof(uid.vduit));
		INIT_LIST_HEAD(&group->group);
		INIT_LIST_HEAD(&group->baselist);
		INIT_LIST_HEAD(&group->aliaslist);
		list_add(&group->group, &lcu->grouplist);
	}
	if (uid.type == UA_BASE_DEVICE)
		list_move(&device->alias_list, &group->baselist);
	else
		list_move(&device->alias_list, &group->aliaslist);
	private->pavgroup = group;
	return 0;
};

static void _remove_device_from_lcu(struct alias_lcu *lcu,
				    struct dasd_device *device)
{
	struct dasd_eckd_private *private;
	struct alias_pav_group *group;

	private = (struct dasd_eckd_private *) device->private;
	list_move(&device->alias_list, &lcu->inactive_devices);
	group = private->pavgroup;
	if (!group)
		return;
	private->pavgroup = NULL;
	if (list_empty(&group->baselist) && list_empty(&group->aliaslist)) {
		list_del(&group->group);
		kfree(group);
		return;
	}
	if (group->next == device)
		group->next = NULL;
};

static int read_unit_address_configuration(struct dasd_device *device,
					   struct alias_lcu *lcu)
{
	struct dasd_psf_prssd_data *prssdp;
	struct dasd_ccw_req *cqr;
	struct ccw1 *ccw;
	int rc;
	unsigned long flags;

	cqr = dasd_kmalloc_request(DASD_ECKD_MAGIC, 1 	+ 1 ,
				   (sizeof(struct dasd_psf_prssd_data)),
				   device);
	if (IS_ERR(cqr))
		return PTR_ERR(cqr);
	cqr->startdev = device;
	cqr->memdev = device;
	clear_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags);
	cqr->retries = 10;
	cqr->expires = 20 * HZ;

	
	prssdp = (struct dasd_psf_prssd_data *) cqr->data;
	memset(prssdp, 0, sizeof(struct dasd_psf_prssd_data));
	prssdp->order = PSF_ORDER_PRSSD;
	prssdp->suborder = 0x0e;	
	

	ccw = cqr->cpaddr;
	ccw->cmd_code = DASD_ECKD_CCW_PSF;
	ccw->count = sizeof(struct dasd_psf_prssd_data);
	ccw->flags |= CCW_FLAG_CC;
	ccw->cda = (__u32)(addr_t) prssdp;

	
	memset(lcu->uac, 0, sizeof(*(lcu->uac)));

	ccw++;
	ccw->cmd_code = DASD_ECKD_CCW_RSSD;
	ccw->count = sizeof(*(lcu->uac));
	ccw->cda = (__u32)(addr_t) lcu->uac;

	cqr->buildclk = get_clock();
	cqr->status = DASD_CQR_FILLED;

	
	spin_lock_irqsave(&lcu->lock, flags);
	lcu->flags &= ~NEED_UAC_UPDATE;
	spin_unlock_irqrestore(&lcu->lock, flags);

	do {
		rc = dasd_sleep_on(cqr);
	} while (rc && (cqr->retries > 0));
	if (rc) {
		spin_lock_irqsave(&lcu->lock, flags);
		lcu->flags |= NEED_UAC_UPDATE;
		spin_unlock_irqrestore(&lcu->lock, flags);
	}
	dasd_kfree_request(cqr, cqr->memdev);
	return rc;
}

static int _lcu_update(struct dasd_device *refdev, struct alias_lcu *lcu)
{
	unsigned long flags;
	struct alias_pav_group *pavgroup, *tempgroup;
	struct dasd_device *device, *tempdev;
	int i, rc;
	struct dasd_eckd_private *private;

	spin_lock_irqsave(&lcu->lock, flags);
	list_for_each_entry_safe(pavgroup, tempgroup, &lcu->grouplist, group) {
		list_for_each_entry_safe(device, tempdev, &pavgroup->baselist,
					 alias_list) {
			list_move(&device->alias_list, &lcu->active_devices);
			private = (struct dasd_eckd_private *) device->private;
			private->pavgroup = NULL;
		}
		list_for_each_entry_safe(device, tempdev, &pavgroup->aliaslist,
					 alias_list) {
			list_move(&device->alias_list, &lcu->active_devices);
			private = (struct dasd_eckd_private *) device->private;
			private->pavgroup = NULL;
		}
		list_del(&pavgroup->group);
		kfree(pavgroup);
	}
	spin_unlock_irqrestore(&lcu->lock, flags);

	rc = read_unit_address_configuration(refdev, lcu);
	if (rc)
		return rc;

	
	spin_lock_irqsave_nested(get_ccwdev_lock(refdev->cdev), flags,
				 CDEV_NESTED_FIRST);
	spin_lock(&lcu->lock);
	lcu->pav = NO_PAV;
	for (i = 0; i < MAX_DEVICES_PER_LCU; ++i) {
		switch (lcu->uac->unit[i].ua_type) {
		case UA_BASE_PAV_ALIAS:
			lcu->pav = BASE_PAV;
			break;
		case UA_HYPER_PAV_ALIAS:
			lcu->pav = HYPER_PAV;
			break;
		}
		if (lcu->pav != NO_PAV)
			break;
	}

	list_for_each_entry_safe(device, tempdev, &lcu->active_devices,
				 alias_list) {
		_add_device_to_lcu(lcu, device, refdev);
	}
	spin_unlock(&lcu->lock);
	spin_unlock_irqrestore(get_ccwdev_lock(refdev->cdev), flags);
	return 0;
}

static void lcu_update_work(struct work_struct *work)
{
	struct alias_lcu *lcu;
	struct read_uac_work_data *ruac_data;
	struct dasd_device *device;
	unsigned long flags;
	int rc;

	ruac_data = container_of(work, struct read_uac_work_data, dwork.work);
	lcu = container_of(ruac_data, struct alias_lcu, ruac_data);
	device = ruac_data->device;
	rc = _lcu_update(device, lcu);
	spin_lock_irqsave(&lcu->lock, flags);
	if (rc || (lcu->flags & NEED_UAC_UPDATE)) {
		DBF_DEV_EVENT(DBF_WARNING, device, "could not update"
			    " alias data in lcu (rc = %d), retry later", rc);
		schedule_delayed_work(&lcu->ruac_data.dwork, 30*HZ);
	} else {
		lcu->ruac_data.device = NULL;
		lcu->flags &= ~UPDATE_PENDING;
	}
	spin_unlock_irqrestore(&lcu->lock, flags);
}

static int _schedule_lcu_update(struct alias_lcu *lcu,
				struct dasd_device *device)
{
	struct dasd_device *usedev = NULL;
	struct alias_pav_group *group;

	lcu->flags |= NEED_UAC_UPDATE;
	if (lcu->ruac_data.device) {
		
		return 0;
	}
	if (device && !list_empty(&device->alias_list))
		usedev = device;

	if (!usedev && !list_empty(&lcu->grouplist)) {
		group = list_first_entry(&lcu->grouplist,
					 struct alias_pav_group, group);
		if (!list_empty(&group->baselist))
			usedev = list_first_entry(&group->baselist,
						  struct dasd_device,
						  alias_list);
		else if (!list_empty(&group->aliaslist))
			usedev = list_first_entry(&group->aliaslist,
						  struct dasd_device,
						  alias_list);
	}
	if (!usedev && !list_empty(&lcu->active_devices)) {
		usedev = list_first_entry(&lcu->active_devices,
					  struct dasd_device, alias_list);
	}
	if (!usedev)
		return -EINVAL;
	lcu->ruac_data.device = usedev;
	schedule_delayed_work(&lcu->ruac_data.dwork, 0);
	return 0;
}

int dasd_alias_add_device(struct dasd_device *device)
{
	struct dasd_eckd_private *private;
	struct alias_lcu *lcu;
	unsigned long flags;
	int rc;

	private = (struct dasd_eckd_private *) device->private;
	lcu = private->lcu;
	rc = 0;

	
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	spin_lock(&lcu->lock);
	if (!(lcu->flags & UPDATE_PENDING)) {
		rc = _add_device_to_lcu(lcu, device, device);
		if (rc)
			lcu->flags |= UPDATE_PENDING;
	}
	if (lcu->flags & UPDATE_PENDING) {
		list_move(&device->alias_list, &lcu->active_devices);
		_schedule_lcu_update(lcu, device);
	}
	spin_unlock(&lcu->lock);
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
	return rc;
}

int dasd_alias_update_add_device(struct dasd_device *device)
{
	struct dasd_eckd_private *private;
	private = (struct dasd_eckd_private *) device->private;
	private->lcu->flags |= UPDATE_PENDING;
	return dasd_alias_add_device(device);
}

int dasd_alias_remove_device(struct dasd_device *device)
{
	struct dasd_eckd_private *private;
	struct alias_lcu *lcu;
	unsigned long flags;

	private = (struct dasd_eckd_private *) device->private;
	lcu = private->lcu;
	
	if (!lcu)
		return 0;
	spin_lock_irqsave(&lcu->lock, flags);
	_remove_device_from_lcu(lcu, device);
	spin_unlock_irqrestore(&lcu->lock, flags);
	return 0;
}

struct dasd_device *dasd_alias_get_start_dev(struct dasd_device *base_device)
{

	struct dasd_device *alias_device;
	struct alias_pav_group *group;
	struct alias_lcu *lcu;
	struct dasd_eckd_private *private, *alias_priv;
	unsigned long flags;

	private = (struct dasd_eckd_private *) base_device->private;
	group = private->pavgroup;
	lcu = private->lcu;
	if (!group || !lcu)
		return NULL;
	if (lcu->pav == NO_PAV ||
	    lcu->flags & (NEED_UAC_UPDATE | UPDATE_PENDING))
		return NULL;
	if (unlikely(!(private->features.feature[8] & 0x01))) {
		DBF_DEV_EVENT(DBF_ERR, base_device, "%s",
			      "Prefix not enabled with PAV enabled\n");
		return NULL;
	}

	spin_lock_irqsave(&lcu->lock, flags);
	alias_device = group->next;
	if (!alias_device) {
		if (list_empty(&group->aliaslist)) {
			spin_unlock_irqrestore(&lcu->lock, flags);
			return NULL;
		} else {
			alias_device = list_first_entry(&group->aliaslist,
							struct dasd_device,
							alias_list);
		}
	}
	if (list_is_last(&alias_device->alias_list, &group->aliaslist))
		group->next = list_first_entry(&group->aliaslist,
					       struct dasd_device, alias_list);
	else
		group->next = list_first_entry(&alias_device->alias_list,
					       struct dasd_device, alias_list);
	spin_unlock_irqrestore(&lcu->lock, flags);
	alias_priv = (struct dasd_eckd_private *) alias_device->private;
	if ((alias_priv->count < private->count) && !alias_device->stopped)
		return alias_device;
	else
		return NULL;
}

static int reset_summary_unit_check(struct alias_lcu *lcu,
				    struct dasd_device *device,
				    char reason)
{
	struct dasd_ccw_req *cqr;
	int rc = 0;
	struct ccw1 *ccw;

	cqr = lcu->rsu_cqr;
	strncpy((char *) &cqr->magic, "ECKD", 4);
	ASCEBC((char *) &cqr->magic, 4);
	ccw = cqr->cpaddr;
	ccw->cmd_code = DASD_ECKD_CCW_RSCK;
	ccw->flags = 0 ;
	ccw->count = 16;
	ccw->cda = (__u32)(addr_t) cqr->data;
	((char *)cqr->data)[0] = reason;

	clear_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags);
	cqr->retries = 255;	
	cqr->startdev = device;
	cqr->memdev = device;
	cqr->block = NULL;
	cqr->expires = 5 * HZ;
	cqr->buildclk = get_clock();
	cqr->status = DASD_CQR_FILLED;

	rc = dasd_sleep_on_immediatly(cqr);
	return rc;
}

static void _restart_all_base_devices_on_lcu(struct alias_lcu *lcu)
{
	struct alias_pav_group *pavgroup;
	struct dasd_device *device;
	struct dasd_eckd_private *private;
	unsigned long flags;

	
	list_for_each_entry(device, &lcu->active_devices, alias_list) {
		private = (struct dasd_eckd_private *) device->private;
		spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
		if (private->uid.type != UA_BASE_DEVICE) {
			spin_unlock_irqrestore(get_ccwdev_lock(device->cdev),
					       flags);
			continue;
		}
		spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
		dasd_schedule_block_bh(device->block);
		dasd_schedule_device_bh(device);
	}
	list_for_each_entry(device, &lcu->inactive_devices, alias_list) {
		private = (struct dasd_eckd_private *) device->private;
		spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
		if (private->uid.type != UA_BASE_DEVICE) {
			spin_unlock_irqrestore(get_ccwdev_lock(device->cdev),
					       flags);
			continue;
		}
		spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
		dasd_schedule_block_bh(device->block);
		dasd_schedule_device_bh(device);
	}
	list_for_each_entry(pavgroup, &lcu->grouplist, group) {
		list_for_each_entry(device, &pavgroup->baselist, alias_list) {
			dasd_schedule_block_bh(device->block);
			dasd_schedule_device_bh(device);
		}
	}
}

static void flush_all_alias_devices_on_lcu(struct alias_lcu *lcu)
{
	struct alias_pav_group *pavgroup;
	struct dasd_device *device, *temp;
	struct dasd_eckd_private *private;
	int rc;
	unsigned long flags;
	LIST_HEAD(active);


	spin_lock_irqsave(&lcu->lock, flags);
	list_for_each_entry_safe(device, temp, &lcu->active_devices,
				 alias_list) {
		private = (struct dasd_eckd_private *) device->private;
		if (private->uid.type == UA_BASE_DEVICE)
			continue;
		list_move(&device->alias_list, &active);
	}

	list_for_each_entry(pavgroup, &lcu->grouplist, group) {
		list_splice_init(&pavgroup->aliaslist, &active);
	}
	while (!list_empty(&active)) {
		device = list_first_entry(&active, struct dasd_device,
					  alias_list);
		spin_unlock_irqrestore(&lcu->lock, flags);
		rc = dasd_flush_device_queue(device);
		spin_lock_irqsave(&lcu->lock, flags);
		if (device == list_first_entry(&active,
					       struct dasd_device, alias_list))
			list_move(&device->alias_list, &lcu->active_devices);
	}
	spin_unlock_irqrestore(&lcu->lock, flags);
}

static void __stop_device_on_lcu(struct dasd_device *device,
				 struct dasd_device *pos)
{
	
	if (pos == device) {
		dasd_device_set_stop_bits(pos, DASD_STOPPED_SU);
		return;
	}
	spin_lock(get_ccwdev_lock(pos->cdev));
	dasd_device_set_stop_bits(pos, DASD_STOPPED_SU);
	spin_unlock(get_ccwdev_lock(pos->cdev));
}

static void _stop_all_devices_on_lcu(struct alias_lcu *lcu,
				     struct dasd_device *device)
{
	struct alias_pav_group *pavgroup;
	struct dasd_device *pos;

	list_for_each_entry(pos, &lcu->active_devices, alias_list)
		__stop_device_on_lcu(device, pos);
	list_for_each_entry(pos, &lcu->inactive_devices, alias_list)
		__stop_device_on_lcu(device, pos);
	list_for_each_entry(pavgroup, &lcu->grouplist, group) {
		list_for_each_entry(pos, &pavgroup->baselist, alias_list)
			__stop_device_on_lcu(device, pos);
		list_for_each_entry(pos, &pavgroup->aliaslist, alias_list)
			__stop_device_on_lcu(device, pos);
	}
}

static void _unstop_all_devices_on_lcu(struct alias_lcu *lcu)
{
	struct alias_pav_group *pavgroup;
	struct dasd_device *device;
	unsigned long flags;

	list_for_each_entry(device, &lcu->active_devices, alias_list) {
		spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
		dasd_device_remove_stop_bits(device, DASD_STOPPED_SU);
		spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
	}

	list_for_each_entry(device, &lcu->inactive_devices, alias_list) {
		spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
		dasd_device_remove_stop_bits(device, DASD_STOPPED_SU);
		spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
	}

	list_for_each_entry(pavgroup, &lcu->grouplist, group) {
		list_for_each_entry(device, &pavgroup->baselist, alias_list) {
			spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
			dasd_device_remove_stop_bits(device, DASD_STOPPED_SU);
			spin_unlock_irqrestore(get_ccwdev_lock(device->cdev),
					       flags);
		}
		list_for_each_entry(device, &pavgroup->aliaslist, alias_list) {
			spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
			dasd_device_remove_stop_bits(device, DASD_STOPPED_SU);
			spin_unlock_irqrestore(get_ccwdev_lock(device->cdev),
					       flags);
		}
	}
}

static void summary_unit_check_handling_work(struct work_struct *work)
{
	struct alias_lcu *lcu;
	struct summary_unit_check_work_data *suc_data;
	unsigned long flags;
	struct dasd_device *device;

	suc_data = container_of(work, struct summary_unit_check_work_data,
				worker);
	lcu = container_of(suc_data, struct alias_lcu, suc_data);
	device = suc_data->device;

	
	flush_all_alias_devices_on_lcu(lcu);

	
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	dasd_device_remove_stop_bits(device,
				     (DASD_STOPPED_SU | DASD_STOPPED_PENDING));
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
	reset_summary_unit_check(lcu, device, suc_data->reason);

	spin_lock_irqsave(&lcu->lock, flags);
	_unstop_all_devices_on_lcu(lcu);
	_restart_all_base_devices_on_lcu(lcu);
	
	_schedule_lcu_update(lcu, device);
	lcu->suc_data.device = NULL;
	spin_unlock_irqrestore(&lcu->lock, flags);
}

void dasd_alias_handle_summary_unit_check(struct dasd_device *device,
					  struct irb *irb)
{
	struct alias_lcu *lcu;
	char reason;
	struct dasd_eckd_private *private;
	char *sense;

	private = (struct dasd_eckd_private *) device->private;

	sense = dasd_get_sense(irb);
	if (sense) {
		reason = sense[8];
		DBF_DEV_EVENT(DBF_NOTICE, device, "%s %x",
			    "eckd handle summary unit check: reason", reason);
	} else {
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			    "eckd handle summary unit check:"
			    " no reason code available");
		return;
	}

	lcu = private->lcu;
	if (!lcu) {
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			    "device not ready to handle summary"
			    " unit check (no lcu structure)");
		return;
	}
	spin_lock(&lcu->lock);
	_stop_all_devices_on_lcu(lcu, device);
	
	private->lcu->flags |= NEED_UAC_UPDATE | UPDATE_PENDING;
	if (list_empty(&device->alias_list)) {
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			    "device is in offline processing,"
			    " don't do summary unit check handling");
		spin_unlock(&lcu->lock);
		return;
	}
	if (lcu->suc_data.device) {
		
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			    "previous instance of summary unit check worker"
			    " still pending");
		spin_unlock(&lcu->lock);
		return ;
	}
	lcu->suc_data.reason = reason;
	lcu->suc_data.device = device;
	spin_unlock(&lcu->lock);
	schedule_work(&lcu->suc_data.worker);
};
