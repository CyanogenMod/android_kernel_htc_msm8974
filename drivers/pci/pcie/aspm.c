/*
 * File:	drivers/pci/pcie/aspm.c
 * Enabling PCIe link L0s/L1 state and Clock Power Management
 *
 * Copyright (C) 2007 Intel
 * Copyright (C) Zhang Yanmin (yanmin.zhang@intel.com)
 * Copyright (C) Shaohua Li (shaohua.li@intel.com)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/pci-aspm.h>
#include "../pci.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "pcie_aspm."

#define ASPM_STATE_L0S_UP	(1)	
#define ASPM_STATE_L0S_DW	(2)	
#define ASPM_STATE_L1		(4)	
#define ASPM_STATE_L0S		(ASPM_STATE_L0S_UP | ASPM_STATE_L0S_DW)
#define ASPM_STATE_ALL		(ASPM_STATE_L0S | ASPM_STATE_L1)

struct aspm_latency {
	u32 l0s;			
	u32 l1;				
};

struct pcie_link_state {
	struct pci_dev *pdev;		
	struct pcie_link_state *root;	
	struct pcie_link_state *parent;	
	struct list_head sibling;	
	struct list_head children;	
	struct list_head link;		

	
	u32 aspm_support:3;		
	u32 aspm_enabled:3;		
	u32 aspm_capable:3;		
	u32 aspm_default:3;		
	u32 aspm_disable:3;		

	
	u32 clkpm_capable:1;		
	u32 clkpm_enabled:1;		
	u32 clkpm_default:1;		

	
	struct aspm_latency latency_up;	
	struct aspm_latency latency_dw;	
	struct aspm_latency acceptable[8];
};

static int aspm_disabled, aspm_force;
static bool aspm_support_enabled = true;
static DEFINE_MUTEX(aspm_lock);
static LIST_HEAD(link_list);

#define POLICY_DEFAULT 0	
#define POLICY_PERFORMANCE 1	
#define POLICY_POWERSAVE 2	

#ifdef CONFIG_PCIEASPM_PERFORMANCE
static int aspm_policy = POLICY_PERFORMANCE;
#elif defined CONFIG_PCIEASPM_POWERSAVE
static int aspm_policy = POLICY_POWERSAVE;
#else
static int aspm_policy;
#endif

static const char *policy_str[] = {
	[POLICY_DEFAULT] = "default",
	[POLICY_PERFORMANCE] = "performance",
	[POLICY_POWERSAVE] = "powersave"
};

#define LINK_RETRAIN_TIMEOUT HZ

static int policy_to_aspm_state(struct pcie_link_state *link)
{
	switch (aspm_policy) {
	case POLICY_PERFORMANCE:
		
		return 0;
	case POLICY_POWERSAVE:
		
		return ASPM_STATE_ALL;
	case POLICY_DEFAULT:
		return link->aspm_default;
	}
	return 0;
}

static int policy_to_clkpm_state(struct pcie_link_state *link)
{
	switch (aspm_policy) {
	case POLICY_PERFORMANCE:
		
		return 0;
	case POLICY_POWERSAVE:
		
		return 1;
	case POLICY_DEFAULT:
		return link->clkpm_default;
	}
	return 0;
}

static void pcie_set_clkpm_nocheck(struct pcie_link_state *link, int enable)
{
	int pos;
	u16 reg16;
	struct pci_dev *child;
	struct pci_bus *linkbus = link->pdev->subordinate;

	list_for_each_entry(child, &linkbus->devices, bus_list) {
		pos = pci_pcie_cap(child);
		if (!pos)
			return;
		pci_read_config_word(child, pos + PCI_EXP_LNKCTL, &reg16);
		if (enable)
			reg16 |= PCI_EXP_LNKCTL_CLKREQ_EN;
		else
			reg16 &= ~PCI_EXP_LNKCTL_CLKREQ_EN;
		pci_write_config_word(child, pos + PCI_EXP_LNKCTL, reg16);
	}
	link->clkpm_enabled = !!enable;
}

static void pcie_set_clkpm(struct pcie_link_state *link, int enable)
{
	
	if (!link->clkpm_capable && enable)
		enable = 0;
	
	if (link->clkpm_enabled == enable)
		return;
	pcie_set_clkpm_nocheck(link, enable);
}

static void pcie_clkpm_cap_init(struct pcie_link_state *link, int blacklist)
{
	int pos, capable = 1, enabled = 1;
	u32 reg32;
	u16 reg16;
	struct pci_dev *child;
	struct pci_bus *linkbus = link->pdev->subordinate;

	
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		pos = pci_pcie_cap(child);
		if (!pos)
			return;
		pci_read_config_dword(child, pos + PCI_EXP_LNKCAP, &reg32);
		if (!(reg32 & PCI_EXP_LNKCAP_CLKPM)) {
			capable = 0;
			enabled = 0;
			break;
		}
		pci_read_config_word(child, pos + PCI_EXP_LNKCTL, &reg16);
		if (!(reg16 & PCI_EXP_LNKCTL_CLKREQ_EN))
			enabled = 0;
	}
	link->clkpm_enabled = enabled;
	link->clkpm_default = enabled;
	link->clkpm_capable = (blacklist) ? 0 : capable;
}

static void pcie_aspm_configure_common_clock(struct pcie_link_state *link)
{
	int ppos, cpos, same_clock = 1;
	u16 reg16, parent_reg, child_reg[8];
	unsigned long start_jiffies;
	struct pci_dev *child, *parent = link->pdev;
	struct pci_bus *linkbus = parent->subordinate;
	child = list_entry(linkbus->devices.next, struct pci_dev, bus_list);
	BUG_ON(!pci_is_pcie(child));

	
	cpos = pci_pcie_cap(child);
	pci_read_config_word(child, cpos + PCI_EXP_LNKSTA, &reg16);
	if (!(reg16 & PCI_EXP_LNKSTA_SLC))
		same_clock = 0;

	
	ppos = pci_pcie_cap(parent);
	pci_read_config_word(parent, ppos + PCI_EXP_LNKSTA, &reg16);
	if (!(reg16 & PCI_EXP_LNKSTA_SLC))
		same_clock = 0;

	
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		cpos = pci_pcie_cap(child);
		pci_read_config_word(child, cpos + PCI_EXP_LNKCTL, &reg16);
		child_reg[PCI_FUNC(child->devfn)] = reg16;
		if (same_clock)
			reg16 |= PCI_EXP_LNKCTL_CCC;
		else
			reg16 &= ~PCI_EXP_LNKCTL_CCC;
		pci_write_config_word(child, cpos + PCI_EXP_LNKCTL, reg16);
	}

	
	pci_read_config_word(parent, ppos + PCI_EXP_LNKCTL, &reg16);
	parent_reg = reg16;
	if (same_clock)
		reg16 |= PCI_EXP_LNKCTL_CCC;
	else
		reg16 &= ~PCI_EXP_LNKCTL_CCC;
	pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL, reg16);

	
	reg16 |= PCI_EXP_LNKCTL_RL;
	pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL, reg16);

	
	start_jiffies = jiffies;
	for (;;) {
		pci_read_config_word(parent, ppos + PCI_EXP_LNKSTA, &reg16);
		if (!(reg16 & PCI_EXP_LNKSTA_LT))
			break;
		if (time_after(jiffies, start_jiffies + LINK_RETRAIN_TIMEOUT))
			break;
		msleep(1);
	}
	if (!(reg16 & PCI_EXP_LNKSTA_LT))
		return;

	
	dev_printk(KERN_ERR, &parent->dev,
		   "ASPM: Could not configure common clock\n");
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		cpos = pci_pcie_cap(child);
		pci_write_config_word(child, cpos + PCI_EXP_LNKCTL,
				      child_reg[PCI_FUNC(child->devfn)]);
	}
	pci_write_config_word(parent, ppos + PCI_EXP_LNKCTL, parent_reg);
}

static u32 calc_l0s_latency(u32 encoding)
{
	if (encoding == 0x7)
		return (5 * 1000);	
	return (64 << encoding);
}

static u32 calc_l0s_acceptable(u32 encoding)
{
	if (encoding == 0x7)
		return -1U;
	return (64 << encoding);
}

static u32 calc_l1_latency(u32 encoding)
{
	if (encoding == 0x7)
		return (65 * 1000);	
	return (1000 << encoding);
}

static u32 calc_l1_acceptable(u32 encoding)
{
	if (encoding == 0x7)
		return -1U;
	return (1000 << encoding);
}

struct aspm_register_info {
	u32 support:2;
	u32 enabled:2;
	u32 latency_encoding_l0s;
	u32 latency_encoding_l1;
};

static void pcie_get_aspm_reg(struct pci_dev *pdev,
			      struct aspm_register_info *info)
{
	int pos;
	u16 reg16;
	u32 reg32;

	pos = pci_pcie_cap(pdev);
	pci_read_config_dword(pdev, pos + PCI_EXP_LNKCAP, &reg32);
	info->support = (reg32 & PCI_EXP_LNKCAP_ASPMS) >> 10;
	info->latency_encoding_l0s = (reg32 & PCI_EXP_LNKCAP_L0SEL) >> 12;
	info->latency_encoding_l1  = (reg32 & PCI_EXP_LNKCAP_L1EL) >> 15;
	pci_read_config_word(pdev, pos + PCI_EXP_LNKCTL, &reg16);
	info->enabled = reg16 & PCI_EXP_LNKCTL_ASPMC;
}

static void pcie_aspm_check_latency(struct pci_dev *endpoint)
{
	u32 latency, l1_switch_latency = 0;
	struct aspm_latency *acceptable;
	struct pcie_link_state *link;

	
	if ((endpoint->current_state != PCI_D0) &&
	    (endpoint->current_state != PCI_UNKNOWN))
		return;

	link = endpoint->bus->self->link_state;
	acceptable = &link->acceptable[PCI_FUNC(endpoint->devfn)];

	while (link) {
		
		if ((link->aspm_capable & ASPM_STATE_L0S_UP) &&
		    (link->latency_up.l0s > acceptable->l0s))
			link->aspm_capable &= ~ASPM_STATE_L0S_UP;

		
		if ((link->aspm_capable & ASPM_STATE_L0S_DW) &&
		    (link->latency_dw.l0s > acceptable->l0s))
			link->aspm_capable &= ~ASPM_STATE_L0S_DW;
		latency = max_t(u32, link->latency_up.l1, link->latency_dw.l1);
		if ((link->aspm_capable & ASPM_STATE_L1) &&
		    (latency + l1_switch_latency > acceptable->l1))
			link->aspm_capable &= ~ASPM_STATE_L1;
		l1_switch_latency += 1000;

		link = link->parent;
	}
}

static void pcie_aspm_cap_init(struct pcie_link_state *link, int blacklist)
{
	struct pci_dev *child, *parent = link->pdev;
	struct pci_bus *linkbus = parent->subordinate;
	struct aspm_register_info upreg, dwreg;

	if (blacklist) {
		
		link->aspm_enabled = ASPM_STATE_ALL;
		link->aspm_disable = ASPM_STATE_ALL;
		return;
	}

	
	pcie_aspm_configure_common_clock(link);

	
	pcie_get_aspm_reg(parent, &upreg);
	child = list_entry(linkbus->devices.next, struct pci_dev, bus_list);
	pcie_get_aspm_reg(child, &dwreg);

	if (dwreg.support & upreg.support & PCIE_LINK_STATE_L0S)
		link->aspm_support |= ASPM_STATE_L0S;
	if (dwreg.enabled & PCIE_LINK_STATE_L0S)
		link->aspm_enabled |= ASPM_STATE_L0S_UP;
	if (upreg.enabled & PCIE_LINK_STATE_L0S)
		link->aspm_enabled |= ASPM_STATE_L0S_DW;
	link->latency_up.l0s = calc_l0s_latency(upreg.latency_encoding_l0s);
	link->latency_dw.l0s = calc_l0s_latency(dwreg.latency_encoding_l0s);

	
	if (upreg.support & dwreg.support & PCIE_LINK_STATE_L1)
		link->aspm_support |= ASPM_STATE_L1;
	if (upreg.enabled & dwreg.enabled & PCIE_LINK_STATE_L1)
		link->aspm_enabled |= ASPM_STATE_L1;
	link->latency_up.l1 = calc_l1_latency(upreg.latency_encoding_l1);
	link->latency_dw.l1 = calc_l1_latency(dwreg.latency_encoding_l1);

	
	link->aspm_default = link->aspm_enabled;

	
	link->aspm_capable = link->aspm_support;
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		if (child->pcie_type == PCI_EXP_TYPE_PCI_BRIDGE) {
			link->aspm_disable = ASPM_STATE_ALL;
			break;
		}
	}

	
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		int pos;
		u32 reg32, encoding;
		struct aspm_latency *acceptable =
			&link->acceptable[PCI_FUNC(child->devfn)];

		if (child->pcie_type != PCI_EXP_TYPE_ENDPOINT &&
		    child->pcie_type != PCI_EXP_TYPE_LEG_END)
			continue;

		pos = pci_pcie_cap(child);
		pci_read_config_dword(child, pos + PCI_EXP_DEVCAP, &reg32);
		
		encoding = (reg32 & PCI_EXP_DEVCAP_L0S) >> 6;
		acceptable->l0s = calc_l0s_acceptable(encoding);
		
		encoding = (reg32 & PCI_EXP_DEVCAP_L1) >> 9;
		acceptable->l1 = calc_l1_acceptable(encoding);

		pcie_aspm_check_latency(child);
	}
}

static void pcie_config_aspm_dev(struct pci_dev *pdev, u32 val)
{
	u16 reg16;
	int pos = pci_pcie_cap(pdev);

	pci_read_config_word(pdev, pos + PCI_EXP_LNKCTL, &reg16);
	reg16 &= ~0x3;
	reg16 |= val;
	pci_write_config_word(pdev, pos + PCI_EXP_LNKCTL, reg16);
}

static void pcie_config_aspm_link(struct pcie_link_state *link, u32 state)
{
	u32 upstream = 0, dwstream = 0;
	struct pci_dev *child, *parent = link->pdev;
	struct pci_bus *linkbus = parent->subordinate;

	
	state &= (link->aspm_capable & ~link->aspm_disable);
	if (link->aspm_enabled == state)
		return;
	
	if (state & ASPM_STATE_L0S_UP)
		dwstream |= PCIE_LINK_STATE_L0S;
	if (state & ASPM_STATE_L0S_DW)
		upstream |= PCIE_LINK_STATE_L0S;
	if (state & ASPM_STATE_L1) {
		upstream |= PCIE_LINK_STATE_L1;
		dwstream |= PCIE_LINK_STATE_L1;
	}
	if (state & ASPM_STATE_L1)
		pcie_config_aspm_dev(parent, upstream);
	list_for_each_entry(child, &linkbus->devices, bus_list)
		pcie_config_aspm_dev(child, dwstream);
	if (!(state & ASPM_STATE_L1))
		pcie_config_aspm_dev(parent, upstream);

	link->aspm_enabled = state;
}

static void pcie_config_aspm_path(struct pcie_link_state *link)
{
	while (link) {
		pcie_config_aspm_link(link, policy_to_aspm_state(link));
		link = link->parent;
	}
}

static void free_link_state(struct pcie_link_state *link)
{
	link->pdev->link_state = NULL;
	kfree(link);
}

static int pcie_aspm_sanity_check(struct pci_dev *pdev)
{
	struct pci_dev *child;
	int pos;
	u32 reg32;

	list_for_each_entry(child, &pdev->subordinate->devices, bus_list) {
		pos = pci_pcie_cap(child);
		if (!pos)
			return -EINVAL;


		if (aspm_disabled)
			continue;

		pci_read_config_dword(child, pos + PCI_EXP_DEVCAP, &reg32);
		if (!(reg32 & PCI_EXP_DEVCAP_RBER) && !aspm_force) {
			dev_printk(KERN_INFO, &child->dev, "disabling ASPM"
				" on pre-1.1 PCIe device.  You can enable it"
				" with 'pcie_aspm=force'\n");
			return -EINVAL;
		}
	}
	return 0;
}

static struct pcie_link_state *alloc_pcie_link_state(struct pci_dev *pdev)
{
	struct pcie_link_state *link;

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link)
		return NULL;
	INIT_LIST_HEAD(&link->sibling);
	INIT_LIST_HEAD(&link->children);
	INIT_LIST_HEAD(&link->link);
	link->pdev = pdev;
	if (pdev->pcie_type == PCI_EXP_TYPE_DOWNSTREAM) {
		struct pcie_link_state *parent;
		parent = pdev->bus->parent->self->link_state;
		if (!parent) {
			kfree(link);
			return NULL;
		}
		link->parent = parent;
		list_add(&link->link, &parent->children);
	}
	
	if (!link->parent)
		link->root = link;
	else
		link->root = link->parent->root;

	list_add(&link->sibling, &link_list);
	pdev->link_state = link;
	return link;
}

void pcie_aspm_init_link_state(struct pci_dev *pdev)
{
	struct pcie_link_state *link;
	int blacklist = !!pcie_aspm_sanity_check(pdev);

	if (!pci_is_pcie(pdev) || pdev->link_state)
		return;
	if (pdev->pcie_type != PCI_EXP_TYPE_ROOT_PORT &&
	    pdev->pcie_type != PCI_EXP_TYPE_DOWNSTREAM)
		return;

	
	if (pdev->pcie_type == PCI_EXP_TYPE_ROOT_PORT &&
	    pdev->bus->self)
		return;

	down_read(&pci_bus_sem);
	if (list_empty(&pdev->subordinate->devices))
		goto out;

	mutex_lock(&aspm_lock);
	link = alloc_pcie_link_state(pdev);
	if (!link)
		goto unlock;
	pcie_aspm_cap_init(link, blacklist);

	
	pcie_clkpm_cap_init(link, blacklist);

	if (aspm_policy != POLICY_POWERSAVE) {
		pcie_config_aspm_path(link);
		pcie_set_clkpm(link, policy_to_clkpm_state(link));
	}

unlock:
	mutex_unlock(&aspm_lock);
out:
	up_read(&pci_bus_sem);
}

static void pcie_update_aspm_capable(struct pcie_link_state *root)
{
	struct pcie_link_state *link;
	BUG_ON(root->parent);
	list_for_each_entry(link, &link_list, sibling) {
		if (link->root != root)
			continue;
		link->aspm_capable = link->aspm_support;
	}
	list_for_each_entry(link, &link_list, sibling) {
		struct pci_dev *child;
		struct pci_bus *linkbus = link->pdev->subordinate;
		if (link->root != root)
			continue;
		list_for_each_entry(child, &linkbus->devices, bus_list) {
			if ((child->pcie_type != PCI_EXP_TYPE_ENDPOINT) &&
			    (child->pcie_type != PCI_EXP_TYPE_LEG_END))
				continue;
			pcie_aspm_check_latency(child);
		}
	}
}

void pcie_aspm_exit_link_state(struct pci_dev *pdev)
{
	struct pci_dev *parent = pdev->bus->self;
	struct pcie_link_state *link, *root, *parent_link;

	if (!pci_is_pcie(pdev) || !parent || !parent->link_state)
		return;
	if ((parent->pcie_type != PCI_EXP_TYPE_ROOT_PORT) &&
	    (parent->pcie_type != PCI_EXP_TYPE_DOWNSTREAM))
		return;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	if (!list_is_last(&pdev->bus_list, &parent->subordinate->devices))
		goto out;

	link = parent->link_state;
	root = link->root;
	parent_link = link->parent;

	
	pcie_config_aspm_link(link, 0);
	list_del(&link->sibling);
	list_del(&link->link);
	
	free_link_state(link);

	
	if (parent_link) {
		pcie_update_aspm_capable(root);
		pcie_config_aspm_path(parent_link);
	}
out:
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
}

void pcie_aspm_pm_state_change(struct pci_dev *pdev)
{
	struct pcie_link_state *link = pdev->link_state;

	if (aspm_disabled || !pci_is_pcie(pdev) || !link)
		return;
	if ((pdev->pcie_type != PCI_EXP_TYPE_ROOT_PORT) &&
	    (pdev->pcie_type != PCI_EXP_TYPE_DOWNSTREAM))
		return;
	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	pcie_update_aspm_capable(link->root);
	pcie_config_aspm_path(link);
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
}

void pcie_aspm_powersave_config_link(struct pci_dev *pdev)
{
	struct pcie_link_state *link = pdev->link_state;

	if (aspm_disabled || !pci_is_pcie(pdev) || !link)
		return;

	if (aspm_policy != POLICY_POWERSAVE)
		return;

	if ((pdev->pcie_type != PCI_EXP_TYPE_ROOT_PORT) &&
	    (pdev->pcie_type != PCI_EXP_TYPE_DOWNSTREAM))
		return;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	pcie_config_aspm_path(link);
	pcie_set_clkpm(link, policy_to_clkpm_state(link));
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
}

static void __pci_disable_link_state(struct pci_dev *pdev, int state, bool sem,
				     bool force)
{
	struct pci_dev *parent = pdev->bus->self;
	struct pcie_link_state *link;

	if (aspm_disabled && !force)
		return;

	if (!pci_is_pcie(pdev))
		return;

	if (pdev->pcie_type == PCI_EXP_TYPE_ROOT_PORT ||
	    pdev->pcie_type == PCI_EXP_TYPE_DOWNSTREAM)
		parent = pdev;
	if (!parent || !parent->link_state)
		return;

	if (sem)
		down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	link = parent->link_state;
	if (state & PCIE_LINK_STATE_L0S)
		link->aspm_disable |= ASPM_STATE_L0S;
	if (state & PCIE_LINK_STATE_L1)
		link->aspm_disable |= ASPM_STATE_L1;
	pcie_config_aspm_link(link, policy_to_aspm_state(link));

	if (state & PCIE_LINK_STATE_CLKPM) {
		link->clkpm_capable = 0;
		pcie_set_clkpm(link, 0);
	}
	mutex_unlock(&aspm_lock);
	if (sem)
		up_read(&pci_bus_sem);
}

void pci_disable_link_state_locked(struct pci_dev *pdev, int state)
{
	__pci_disable_link_state(pdev, state, false, false);
}
EXPORT_SYMBOL(pci_disable_link_state_locked);

void pci_disable_link_state(struct pci_dev *pdev, int state)
{
	__pci_disable_link_state(pdev, state, true, false);
}
EXPORT_SYMBOL(pci_disable_link_state);

void pcie_clear_aspm(struct pci_bus *bus)
{
	struct pci_dev *child;

	list_for_each_entry(child, &bus->devices, bus_list) {
		__pci_disable_link_state(child, PCIE_LINK_STATE_L0S |
					 PCIE_LINK_STATE_L1 |
					 PCIE_LINK_STATE_CLKPM,
					 false, true);
	}
}

static int pcie_aspm_set_policy(const char *val, struct kernel_param *kp)
{
	int i;
	struct pcie_link_state *link;

	if (aspm_disabled)
		return -EPERM;
	for (i = 0; i < ARRAY_SIZE(policy_str); i++)
		if (!strncmp(val, policy_str[i], strlen(policy_str[i])))
			break;
	if (i >= ARRAY_SIZE(policy_str))
		return -EINVAL;
	if (i == aspm_policy)
		return 0;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	aspm_policy = i;
	list_for_each_entry(link, &link_list, sibling) {
		pcie_config_aspm_link(link, policy_to_aspm_state(link));
		pcie_set_clkpm(link, policy_to_clkpm_state(link));
	}
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
	return 0;
}

static int pcie_aspm_get_policy(char *buffer, struct kernel_param *kp)
{
	int i, cnt = 0;
	for (i = 0; i < ARRAY_SIZE(policy_str); i++)
		if (i == aspm_policy)
			cnt += sprintf(buffer + cnt, "[%s] ", policy_str[i]);
		else
			cnt += sprintf(buffer + cnt, "%s ", policy_str[i]);
	return cnt;
}

module_param_call(policy, pcie_aspm_set_policy, pcie_aspm_get_policy,
	NULL, 0644);

#ifdef CONFIG_PCIEASPM_DEBUG
static ssize_t link_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct pci_dev *pci_device = to_pci_dev(dev);
	struct pcie_link_state *link_state = pci_device->link_state;

	return sprintf(buf, "%d\n", link_state->aspm_enabled);
}

static ssize_t link_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t n)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pcie_link_state *link, *root = pdev->link_state->root;
	u32 val = buf[0] - '0', state = 0;

	if (aspm_disabled)
		return -EPERM;
	if (n < 1 || val > 3)
		return -EINVAL;

	
	if (val & PCIE_LINK_STATE_L0S)
		state |= ASPM_STATE_L0S;
	if (val & PCIE_LINK_STATE_L1)
		state |= ASPM_STATE_L1;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	list_for_each_entry(link, &link_list, sibling) {
		if (link->root != root)
			continue;
		pcie_config_aspm_link(link, state);
	}
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
	return n;
}

static ssize_t clk_ctl_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct pci_dev *pci_device = to_pci_dev(dev);
	struct pcie_link_state *link_state = pci_device->link_state;

	return sprintf(buf, "%d\n", link_state->clkpm_enabled);
}

static ssize_t clk_ctl_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t n)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	int state;

	if (n < 1)
		return -EINVAL;
	state = buf[0]-'0';

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	pcie_set_clkpm_nocheck(pdev->link_state, !!state);
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);

	return n;
}

static DEVICE_ATTR(link_state, 0644, link_state_show, link_state_store);
static DEVICE_ATTR(clk_ctl, 0644, clk_ctl_show, clk_ctl_store);

static char power_group[] = "power";
void pcie_aspm_create_sysfs_dev_files(struct pci_dev *pdev)
{
	struct pcie_link_state *link_state = pdev->link_state;

	if (!pci_is_pcie(pdev) ||
	    (pdev->pcie_type != PCI_EXP_TYPE_ROOT_PORT &&
	     pdev->pcie_type != PCI_EXP_TYPE_DOWNSTREAM) || !link_state)
		return;

	if (link_state->aspm_support)
		sysfs_add_file_to_group(&pdev->dev.kobj,
			&dev_attr_link_state.attr, power_group);
	if (link_state->clkpm_capable)
		sysfs_add_file_to_group(&pdev->dev.kobj,
			&dev_attr_clk_ctl.attr, power_group);
}

void pcie_aspm_remove_sysfs_dev_files(struct pci_dev *pdev)
{
	struct pcie_link_state *link_state = pdev->link_state;

	if (!pci_is_pcie(pdev) ||
	    (pdev->pcie_type != PCI_EXP_TYPE_ROOT_PORT &&
	     pdev->pcie_type != PCI_EXP_TYPE_DOWNSTREAM) || !link_state)
		return;

	if (link_state->aspm_support)
		sysfs_remove_file_from_group(&pdev->dev.kobj,
			&dev_attr_link_state.attr, power_group);
	if (link_state->clkpm_capable)
		sysfs_remove_file_from_group(&pdev->dev.kobj,
			&dev_attr_clk_ctl.attr, power_group);
}
#endif

static int __init pcie_aspm_disable(char *str)
{
	if (!strcmp(str, "off")) {
		aspm_policy = POLICY_DEFAULT;
		aspm_disabled = 1;
		aspm_support_enabled = false;
		printk(KERN_INFO "PCIe ASPM is disabled\n");
	} else if (!strcmp(str, "force")) {
		aspm_force = 1;
		printk(KERN_INFO "PCIe ASPM is forcibly enabled\n");
	}
	return 1;
}

__setup("pcie_aspm=", pcie_aspm_disable);

void pcie_no_aspm(void)
{
	if (!aspm_force) {
		aspm_policy = POLICY_DEFAULT;
		aspm_disabled = 1;
	}
}

int pcie_aspm_enabled(void)
{
       return !aspm_disabled;
}
EXPORT_SYMBOL(pcie_aspm_enabled);

bool pcie_aspm_support_enabled(void)
{
	return aspm_support_enabled;
}
EXPORT_SYMBOL(pcie_aspm_support_enabled);
