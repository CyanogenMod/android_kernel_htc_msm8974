/*
 * The file intends to implement the platform dependent EEH operations on pseries.
 * Actually, the pseries platform is built based on RTAS heavily. That means the
 * pseries platform dependent EEH operations will be built on RTAS calls. The functions
 * are devired from arch/powerpc/platforms/pseries/eeh.c and necessary cleanup has
 * been done.
 *
 * Copyright Benjamin Herrenschmidt & Gavin Shan, IBM Corporation 2011.
 * Copyright IBM Corporation 2001, 2005, 2006
 * Copyright Dave Engebretsen & Todd Inglett 2001
 * Copyright Linas Vepstas 2005, 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <asm/eeh.h>
#include <asm/eeh_event.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/ppc-pci.h>
#include <asm/rtas.h>

static int ibm_set_eeh_option;
static int ibm_set_slot_reset;
static int ibm_read_slot_reset_state;
static int ibm_read_slot_reset_state2;
static int ibm_slot_error_detail;
static int ibm_get_config_addr_info;
static int ibm_get_config_addr_info2;
static int ibm_configure_bridge;
static int ibm_configure_pe;

static unsigned char slot_errbuf[RTAS_ERROR_LOG_MAX];
static DEFINE_SPINLOCK(slot_errbuf_lock);
static int eeh_error_buf_size;

static int pseries_eeh_init(void)
{
	
	ibm_set_eeh_option		= rtas_token("ibm,set-eeh-option");
	ibm_set_slot_reset		= rtas_token("ibm,set-slot-reset");
	ibm_read_slot_reset_state2	= rtas_token("ibm,read-slot-reset-state2");
	ibm_read_slot_reset_state	= rtas_token("ibm,read-slot-reset-state");
	ibm_slot_error_detail		= rtas_token("ibm,slot-error-detail");
	ibm_get_config_addr_info2	= rtas_token("ibm,get-config-addr-info2");
	ibm_get_config_addr_info	= rtas_token("ibm,get-config-addr-info");
	ibm_configure_pe		= rtas_token("ibm,configure-pe");
	ibm_configure_bridge		= rtas_token ("ibm,configure-bridge");

	
	if (ibm_set_eeh_option == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: RTAS service <ibm,set-eeh-option> invalid\n",
			__func__);
		return -EINVAL;
	} else if (ibm_set_slot_reset == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: RTAS service <ibm, set-slot-reset> invalid\n",
			__func__);
		return -EINVAL;
	} else if (ibm_read_slot_reset_state2 == RTAS_UNKNOWN_SERVICE &&
		   ibm_read_slot_reset_state == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: RTAS service <ibm,read-slot-reset-state2> and "
			"<ibm,read-slot-reset-state> invalid\n",
			__func__);
		return -EINVAL;
	} else if (ibm_slot_error_detail == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: RTAS service <ibm,slot-error-detail> invalid\n",
			__func__);
		return -EINVAL;
	} else if (ibm_get_config_addr_info2 == RTAS_UNKNOWN_SERVICE &&
		   ibm_get_config_addr_info == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: RTAS service <ibm,get-config-addr-info2> and "
			"<ibm,get-config-addr-info> invalid\n",
			__func__);
		return -EINVAL;
	} else if (ibm_configure_pe == RTAS_UNKNOWN_SERVICE &&
		   ibm_configure_bridge == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: RTAS service <ibm,configure-pe> and "
			"<ibm,configure-bridge> invalid\n",
			__func__);
		return -EINVAL;
	}

	
	spin_lock_init(&slot_errbuf_lock);
	eeh_error_buf_size = rtas_token("rtas-error-log-max");
	if (eeh_error_buf_size == RTAS_UNKNOWN_SERVICE) {
		pr_warning("%s: unknown EEH error log size\n",
			__func__);
		eeh_error_buf_size = 1024;
	} else if (eeh_error_buf_size > RTAS_ERROR_LOG_MAX) {
		pr_warning("%s: EEH error log size %d exceeds the maximal %d\n",
			__func__, eeh_error_buf_size, RTAS_ERROR_LOG_MAX);
		eeh_error_buf_size = RTAS_ERROR_LOG_MAX;
	}

	return 0;
}

static int pseries_eeh_set_option(struct device_node *dn, int option)
{
	int ret = 0;
	struct eeh_dev *edev;
	const u32 *reg;
	int config_addr;

	edev = of_node_to_eeh_dev(dn);

	switch (option) {
	case EEH_OPT_DISABLE:
	case EEH_OPT_ENABLE:
		reg = of_get_property(dn, "reg", NULL);
		config_addr = reg[0];
		break;

	case EEH_OPT_THAW_MMIO:
	case EEH_OPT_THAW_DMA:
		config_addr = edev->config_addr;
		if (edev->pe_config_addr)
			config_addr = edev->pe_config_addr;
		break;

	default:
		pr_err("%s: Invalid option %d\n",
			__func__, option);
		return -EINVAL;
	}

	ret = rtas_call(ibm_set_eeh_option, 4, 1, NULL,
			config_addr, BUID_HI(edev->phb->buid),
			BUID_LO(edev->phb->buid), option);

	return ret;
}

static int pseries_eeh_get_pe_addr(struct device_node *dn)
{
	struct eeh_dev *edev;
	int ret = 0;
	int rets[3];

	edev = of_node_to_eeh_dev(dn);

	if (ibm_get_config_addr_info2 != RTAS_UNKNOWN_SERVICE) {
		ret = rtas_call(ibm_get_config_addr_info2, 4, 2, rets,
				edev->config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid), 1);
		if (ret || (rets[0] == 0))
			return 0;

		
		ret = rtas_call(ibm_get_config_addr_info2, 4, 2, rets,
				edev->config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid), 0);
		if (ret) {
			pr_warning("%s: Failed to get PE address for %s\n",
				__func__, dn->full_name);
			return 0;
		}

		return rets[0];
	}

	if (ibm_get_config_addr_info != RTAS_UNKNOWN_SERVICE) {
		ret = rtas_call(ibm_get_config_addr_info, 4, 2, rets,
				edev->config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid), 0);
		if (ret) {
			pr_warning("%s: Failed to get PE address for %s\n",
				__func__, dn->full_name);
			return 0;
		}

		return rets[0];
	}

	return ret;
}

static int pseries_eeh_get_state(struct device_node *dn, int *state)
{
	struct eeh_dev *edev;
	int config_addr;
	int ret;
	int rets[4];
	int result;

	
	edev = of_node_to_eeh_dev(dn);
	config_addr = edev->config_addr;
	if (edev->pe_config_addr)
		config_addr = edev->pe_config_addr;

	if (ibm_read_slot_reset_state2 != RTAS_UNKNOWN_SERVICE) {
		ret = rtas_call(ibm_read_slot_reset_state2, 3, 4, rets,
				config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid));
	} else if (ibm_read_slot_reset_state != RTAS_UNKNOWN_SERVICE) {
		
		rets[2] = 0;
		ret = rtas_call(ibm_read_slot_reset_state, 3, 3, rets,
				config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid));
	} else {
		return EEH_STATE_NOT_SUPPORT;
	}

	if (ret)
		return ret;

	
	result = 0;
	if (rets[1]) {
		switch(rets[0]) {
		case 0:
			result &= ~EEH_STATE_RESET_ACTIVE;
			result |= EEH_STATE_MMIO_ACTIVE;
			result |= EEH_STATE_DMA_ACTIVE;
			break;
		case 1:
			result |= EEH_STATE_RESET_ACTIVE;
			result |= EEH_STATE_MMIO_ACTIVE;
			result |= EEH_STATE_DMA_ACTIVE;
			break;
		case 2:
			result &= ~EEH_STATE_RESET_ACTIVE;
			result &= ~EEH_STATE_MMIO_ACTIVE;
			result &= ~EEH_STATE_DMA_ACTIVE;
			break;
		case 4:
			result &= ~EEH_STATE_RESET_ACTIVE;
			result &= ~EEH_STATE_MMIO_ACTIVE;
			result &= ~EEH_STATE_DMA_ACTIVE;
			result |= EEH_STATE_MMIO_ENABLED;
			break;
		case 5:
			if (rets[2]) {
				if (state) *state = rets[2];
				result = EEH_STATE_UNAVAILABLE;
			} else {
				result = EEH_STATE_NOT_SUPPORT;
			}
		default:
			result = EEH_STATE_NOT_SUPPORT;
		}
	} else {
		result = EEH_STATE_NOT_SUPPORT;
	}

	return result;
}

static int pseries_eeh_reset(struct device_node *dn, int option)
{
	struct eeh_dev *edev;
	int config_addr;
	int ret;

	
	edev = of_node_to_eeh_dev(dn);
	config_addr = edev->config_addr;
	if (edev->pe_config_addr)
		config_addr = edev->pe_config_addr;

	
	ret = rtas_call(ibm_set_slot_reset, 4, 1, NULL,
			config_addr, BUID_HI(edev->phb->buid),
			BUID_LO(edev->phb->buid), option);

	
	if (option == EEH_RESET_FUNDAMENTAL &&
	    ret == -8) {
		ret = rtas_call(ibm_set_slot_reset, 4, 1, NULL,
				config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid), EEH_RESET_HOT);
	}

	return ret;
}

static int pseries_eeh_wait_state(struct device_node *dn, int max_wait)
{
	int ret;
	int mwait;

#define EEH_STATE_MIN_WAIT_TIME	(1000)
#define EEH_STATE_MAX_WAIT_TIME	(300 * 1000)

	while (1) {
		ret = pseries_eeh_get_state(dn, &mwait);

		if (ret != EEH_STATE_UNAVAILABLE)
			return ret;

		if (max_wait <= 0) {
			pr_warning("%s: Timeout when getting PE's state (%d)\n",
				__func__, max_wait);
			return EEH_STATE_NOT_SUPPORT;
		}

		if (mwait <= 0) {
			pr_warning("%s: Firmware returned bad wait value %d\n",
				__func__, mwait);
			mwait = EEH_STATE_MIN_WAIT_TIME;
		} else if (mwait > EEH_STATE_MAX_WAIT_TIME) {
			pr_warning("%s: Firmware returned too long wait value %d\n",
				__func__, mwait);
			mwait = EEH_STATE_MAX_WAIT_TIME;
		}

		max_wait -= mwait;
		msleep(mwait);
	}

	return EEH_STATE_NOT_SUPPORT;
}

static int pseries_eeh_get_log(struct device_node *dn, int severity, char *drv_log, unsigned long len)
{
	struct eeh_dev *edev;
	int config_addr;
	unsigned long flags;
	int ret;

	edev = of_node_to_eeh_dev(dn);
	spin_lock_irqsave(&slot_errbuf_lock, flags);
	memset(slot_errbuf, 0, eeh_error_buf_size);

	
	config_addr = edev->config_addr;
	if (edev->pe_config_addr)
		config_addr = edev->pe_config_addr;

	ret = rtas_call(ibm_slot_error_detail, 8, 1, NULL, config_addr,
			BUID_HI(edev->phb->buid), BUID_LO(edev->phb->buid),
			virt_to_phys(drv_log), len,
			virt_to_phys(slot_errbuf), eeh_error_buf_size,
			severity);
	if (!ret)
		log_error(slot_errbuf, ERR_TYPE_RTAS_LOG, 0);
	spin_unlock_irqrestore(&slot_errbuf_lock, flags);

	return ret;
}

static int pseries_eeh_configure_bridge(struct device_node *dn)
{
	struct eeh_dev *edev;
	int config_addr;
	int ret;

	
	edev = of_node_to_eeh_dev(dn);
	config_addr = edev->config_addr;
	if (edev->pe_config_addr)
		config_addr = edev->pe_config_addr;

	
	if (ibm_configure_pe != RTAS_UNKNOWN_SERVICE) {
		ret = rtas_call(ibm_configure_pe, 3, 1, NULL,
				config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid));
	} else if (ibm_configure_bridge != RTAS_UNKNOWN_SERVICE) {
		ret = rtas_call(ibm_configure_bridge, 3, 1, NULL,
				config_addr, BUID_HI(edev->phb->buid),
				BUID_LO(edev->phb->buid));
	} else {
		return -EFAULT;
	}

	if (ret)
		pr_warning("%s: Unable to configure bridge %d for %s\n",
			__func__, ret, dn->full_name);

	return ret;
}

static int pseries_eeh_read_config(struct device_node *dn, int where, int size, u32 *val)
{
	struct pci_dn *pdn;

	pdn = PCI_DN(dn);

	return rtas_read_config(pdn, where, size, val);
}

/**
 * pseries_eeh_write_config - Write PCI config space
 * @dn: device node
 * @where: PCI address
 * @size: size to write
 * @val: value to be written
 *
 * Write config space to the specified device
 */
static int pseries_eeh_write_config(struct device_node *dn, int where, int size, u32 val)
{
	struct pci_dn *pdn;

	pdn = PCI_DN(dn);

	return rtas_write_config(pdn, where, size, val);
}

static struct eeh_ops pseries_eeh_ops = {
	.name			= "pseries",
	.init			= pseries_eeh_init,
	.set_option		= pseries_eeh_set_option,
	.get_pe_addr		= pseries_eeh_get_pe_addr,
	.get_state		= pseries_eeh_get_state,
	.reset			= pseries_eeh_reset,
	.wait_state		= pseries_eeh_wait_state,
	.get_log		= pseries_eeh_get_log,
	.configure_bridge       = pseries_eeh_configure_bridge,
	.read_config		= pseries_eeh_read_config,
	.write_config		= pseries_eeh_write_config
};

int __init eeh_pseries_init(void)
{
	return eeh_ops_register(&pseries_eeh_ops);
}
