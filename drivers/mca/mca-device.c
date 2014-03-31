
/*
 * MCA device support functions
 *
 * These functions support the ongoing device access API.
 *
 * (C) 2002 James Bottomley <James.Bottomley@HansenPartnership.com>
 *
**-----------------------------------------------------------------------------
**  
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/mca.h>
#include <linux/string.h>

unsigned char mca_device_read_stored_pos(struct mca_device *mca_dev, int reg)
{
	if(reg < 0 || reg >= 8)
		return 0;

	return mca_dev->pos[reg];
}
EXPORT_SYMBOL(mca_device_read_stored_pos);

unsigned char mca_device_read_pos(struct mca_device *mca_dev, int reg)
{
	struct mca_bus *mca_bus = to_mca_bus(mca_dev->dev.parent);

	return mca_bus->f.mca_read_pos(mca_dev, reg);

	return 	mca_dev->pos[reg];
}
EXPORT_SYMBOL(mca_device_read_pos);


void mca_device_write_pos(struct mca_device *mca_dev, int reg,
			  unsigned char byte)
{
	struct mca_bus *mca_bus = to_mca_bus(mca_dev->dev.parent);

	mca_bus->f.mca_write_pos(mca_dev, reg, byte);
}
EXPORT_SYMBOL(mca_device_write_pos);

int mca_device_transform_irq(struct mca_device *mca_dev, int irq)
{
	struct mca_bus *mca_bus = to_mca_bus(mca_dev->dev.parent);

	return mca_bus->f.mca_transform_irq(mca_dev, irq);
}
EXPORT_SYMBOL(mca_device_transform_irq);

int mca_device_transform_ioport(struct mca_device *mca_dev, int port)
{
	struct mca_bus *mca_bus = to_mca_bus(mca_dev->dev.parent);

	return mca_bus->f.mca_transform_ioport(mca_dev, port);
}
EXPORT_SYMBOL(mca_device_transform_ioport);

void *mca_device_transform_memory(struct mca_device *mca_dev, void *mem)
{
	struct mca_bus *mca_bus = to_mca_bus(mca_dev->dev.parent);

	return mca_bus->f.mca_transform_memory(mca_dev, mem);
}
EXPORT_SYMBOL(mca_device_transform_memory);



int mca_device_claimed(struct mca_device *mca_dev)
{
	return mca_dev->driver_loaded;
}
EXPORT_SYMBOL(mca_device_claimed);

void mca_device_set_claim(struct mca_device *mca_dev, int val)
{
	mca_dev->driver_loaded = val;
}
EXPORT_SYMBOL(mca_device_set_claim);

enum MCA_AdapterStatus mca_device_status(struct mca_device *mca_dev)
{
	return mca_dev->status;
}
EXPORT_SYMBOL(mca_device_status);

void mca_device_set_name(struct mca_device *mca_dev, const char *name)
{
	if(!mca_dev)
		return;

	strlcpy(mca_dev->name, name, sizeof(mca_dev->name));
}
EXPORT_SYMBOL(mca_device_set_name);
