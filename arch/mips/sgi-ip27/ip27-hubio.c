/*
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.
 * Copyright (C) 2004 Christoph Hellwig.
 *	Released under GPL v2.
 *
 * Support functions for the HUB ASIC - mostly PIO mapping related.
 */

#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/mmzone.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/hub.h>


static int force_fire_and_forget = 1;

unsigned long hub_pio_map(cnodeid_t cnode, xwidgetnum_t widget,
			  unsigned long xtalk_addr, size_t size)
{
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);
	unsigned i;

	
	if ((xtalk_addr % SWIN_SIZE) + size <= SWIN_SIZE)
		return NODE_SWIN_BASE(nasid, widget) + (xtalk_addr % SWIN_SIZE);

	if ((xtalk_addr % BWIN_SIZE) + size > BWIN_SIZE) {
		printk(KERN_WARNING "PIO mapping at hub %d widget %d addr 0x%lx"
				" too big (%ld)\n",
				nasid, widget, xtalk_addr, size);
		return 0;
	}

	xtalk_addr &= ~(BWIN_SIZE-1);
	for (i = 0; i < HUB_NUM_BIG_WINDOW; i++) {
		if (test_and_set_bit(i, hub_data(cnode)->h_bigwin_used))
			continue;

		IIO_ITTE_PUT(nasid, i, HUB_PIO_MAP_TO_MEM, widget, xtalk_addr);
		(void) HUB_L(IIO_ITTE_GET(nasid, i));

		return NODE_BWIN_BASE(nasid, widget) + (xtalk_addr % BWIN_SIZE);
	}

	printk(KERN_WARNING "unable to establish PIO mapping for at"
			" hub %d widget %d addr 0x%lx\n",
			nasid, widget, xtalk_addr);
	return 0;
}


static void hub_setup_prb(nasid_t nasid, int prbnum, int credits)
{
	iprb_t prb;
	int prb_offset;

	prb_offset = IIO_IOPRB(prbnum);
	prb.iprb_regval = REMOTE_HUB_L(nasid, prb_offset);

	prb.iprb_ovflow = 1;
	prb.iprb_bnakctr = 0;
	prb.iprb_anakctr = 0;

	prb.iprb_ff = force_fire_and_forget ? 1 : 0;

	prb.iprb_xtalkctr = credits;

	REMOTE_HUB_S(nasid, prb_offset, prb.iprb_regval);
}

static void hub_set_piomode(nasid_t nasid)
{
	hubreg_t ii_iowa;
	hubii_wcr_t ii_wcr;
	unsigned i;

	ii_iowa = REMOTE_HUB_L(nasid, IIO_OUTWIDGET_ACCESS);
	REMOTE_HUB_S(nasid, IIO_OUTWIDGET_ACCESS, 0);

	ii_wcr.wcr_reg_value = REMOTE_HUB_L(nasid, IIO_WCR);

	if (ii_wcr.iwcr_dir_con) {
		hub_setup_prb(nasid, 0, 3);
	} else {
		hub_setup_prb(nasid, 0, 1);
	}

	for (i = HUB_WIDGET_ID_MIN; i <= HUB_WIDGET_ID_MAX; i++)
		hub_setup_prb(nasid, i, 3);

	REMOTE_HUB_S(nasid, IIO_OUTWIDGET_ACCESS, ii_iowa);
}

void hub_pio_init(cnodeid_t cnode)
{
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);
	unsigned i;

	
	bitmap_zero(hub_data(cnode)->h_bigwin_used, HUB_NUM_BIG_WINDOW);
	for (i = 0; i < HUB_NUM_BIG_WINDOW; i++)
		IIO_ITTE_DISABLE(nasid, i);

	hub_set_piomode(nasid);
}
