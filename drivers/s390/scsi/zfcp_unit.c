/*
 * zfcp device driver
 *
 * Tracking of manually configured LUNs and helper functions to
 * register the LUNs with the SCSI midlayer.
 *
 * Copyright IBM Corporation 2010
 */

#include "zfcp_def.h"
#include "zfcp_ext.h"

void zfcp_unit_scsi_scan(struct zfcp_unit *unit)
{
	struct fc_rport *rport = unit->port->rport;
	unsigned int lun;

	lun = scsilun_to_int((struct scsi_lun *) &unit->fcp_lun);

	if (rport && rport->port_state == FC_PORTSTATE_ONLINE)
		scsi_scan_target(&rport->dev, 0, rport->scsi_target_id, lun, 1);
}

static void zfcp_unit_scsi_scan_work(struct work_struct *work)
{
	struct zfcp_unit *unit = container_of(work, struct zfcp_unit,
					      scsi_work);

	zfcp_unit_scsi_scan(unit);
	put_device(&unit->dev);
}

void zfcp_unit_queue_scsi_scan(struct zfcp_port *port)
{
	struct zfcp_unit *unit;

	read_lock_irq(&port->unit_list_lock);
	list_for_each_entry(unit, &port->unit_list, list) {
		get_device(&unit->dev);
		if (scsi_queue_work(port->adapter->scsi_host,
				    &unit->scsi_work) <= 0)
			put_device(&unit->dev);
	}
	read_unlock_irq(&port->unit_list_lock);
}

static struct zfcp_unit *_zfcp_unit_find(struct zfcp_port *port, u64 fcp_lun)
{
	struct zfcp_unit *unit;

	list_for_each_entry(unit, &port->unit_list, list)
		if (unit->fcp_lun == fcp_lun) {
			get_device(&unit->dev);
			return unit;
		}

	return NULL;
}

struct zfcp_unit *zfcp_unit_find(struct zfcp_port *port, u64 fcp_lun)
{
	struct zfcp_unit *unit;

	read_lock_irq(&port->unit_list_lock);
	unit = _zfcp_unit_find(port, fcp_lun);
	read_unlock_irq(&port->unit_list_lock);
	return unit;
}

static void zfcp_unit_release(struct device *dev)
{
	struct zfcp_unit *unit = container_of(dev, struct zfcp_unit, dev);

	put_device(&unit->port->dev);
	kfree(unit);
}

int zfcp_unit_add(struct zfcp_port *port, u64 fcp_lun)
{
	struct zfcp_unit *unit;

	unit = zfcp_unit_find(port, fcp_lun);
	if (unit) {
		put_device(&unit->dev);
		return -EEXIST;
	}

	unit = kzalloc(sizeof(struct zfcp_unit), GFP_KERNEL);
	if (!unit)
		return -ENOMEM;

	unit->port = port;
	unit->fcp_lun = fcp_lun;
	unit->dev.parent = &port->dev;
	unit->dev.release = zfcp_unit_release;
	INIT_WORK(&unit->scsi_work, zfcp_unit_scsi_scan_work);

	if (dev_set_name(&unit->dev, "0x%016llx",
			 (unsigned long long) fcp_lun)) {
		kfree(unit);
		return -ENOMEM;
	}

	get_device(&port->dev);

	if (device_register(&unit->dev)) {
		put_device(&unit->dev);
		return -ENOMEM;
	}

	if (sysfs_create_group(&unit->dev.kobj, &zfcp_sysfs_unit_attrs)) {
		device_unregister(&unit->dev);
		return -EINVAL;
	}

	write_lock_irq(&port->unit_list_lock);
	list_add_tail(&unit->list, &port->unit_list);
	write_unlock_irq(&port->unit_list_lock);

	zfcp_unit_scsi_scan(unit);

	return 0;
}

struct scsi_device *zfcp_unit_sdev(struct zfcp_unit *unit)
{
	struct Scsi_Host *shost;
	struct zfcp_port *port;
	unsigned int lun;

	lun = scsilun_to_int((struct scsi_lun *) &unit->fcp_lun);
	port = unit->port;
	shost = port->adapter->scsi_host;
	return scsi_device_lookup(shost, 0, port->starget_id, lun);
}

unsigned int zfcp_unit_sdev_status(struct zfcp_unit *unit)
{
	unsigned int status = 0;
	struct scsi_device *sdev;
	struct zfcp_scsi_dev *zfcp_sdev;

	sdev = zfcp_unit_sdev(unit);
	if (sdev) {
		zfcp_sdev = sdev_to_zfcp(sdev);
		status = atomic_read(&zfcp_sdev->status);
		scsi_device_put(sdev);
	}

	return status;
}

int zfcp_unit_remove(struct zfcp_port *port, u64 fcp_lun)
{
	struct zfcp_unit *unit;
	struct scsi_device *sdev;

	write_lock_irq(&port->unit_list_lock);
	unit = _zfcp_unit_find(port, fcp_lun);
	if (unit)
		list_del(&unit->list);
	write_unlock_irq(&port->unit_list_lock);

	if (!unit)
		return -EINVAL;

	sdev = zfcp_unit_sdev(unit);
	if (sdev) {
		scsi_remove_device(sdev);
		scsi_device_put(sdev);
	}

	put_device(&unit->dev);

	zfcp_device_unregister(&unit->dev, &zfcp_sysfs_unit_attrs);

	return 0;
}
