/*
 * Functions for registration of I/O interruption subclasses on s390.
 *
 * Copyright IBM Corp. 2008
 * Authors: Sebastian Ott <sebott@linux.vnet.ibm.com>
 */

#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/isc.h>

static unsigned int isc_refs[MAX_ISC + 1];
static DEFINE_SPINLOCK(isc_ref_lock);


void isc_register(unsigned int isc)
{
	if (isc > MAX_ISC) {
		WARN_ON(1);
		return;
	}

	spin_lock(&isc_ref_lock);
	if (isc_refs[isc] == 0)
		ctl_set_bit(6, 31 - isc);
	isc_refs[isc]++;
	spin_unlock(&isc_ref_lock);
}
EXPORT_SYMBOL_GPL(isc_register);

void isc_unregister(unsigned int isc)
{
	spin_lock(&isc_ref_lock);
	
	if (isc > MAX_ISC || isc_refs[isc] == 0) {
		WARN_ON(1);
		goto out_unlock;
	}
	if (isc_refs[isc] == 1)
		ctl_clear_bit(6, 31 - isc);
	isc_refs[isc]--;
out_unlock:
	spin_unlock(&isc_ref_lock);
}
EXPORT_SYMBOL_GPL(isc_unregister);
