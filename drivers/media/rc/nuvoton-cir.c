/*
 * Driver for Nuvoton Technology Corporation w83667hg/w83677hg-i CIR
 *
 * Copyright (C) 2010 Jarod Wilson <jarod@redhat.com>
 * Copyright (C) 2009 Nuvoton PS Team
 *
 * Special thanks to Nuvoton for providing hardware, spec sheets and
 * sample code upon which portions of this driver are based. Indirect
 * thanks also to Maxim Levitsky, whose ene_ir driver this driver is
 * modeled after.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pnp.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/rc-core.h>
#include <linux/pci_ids.h>

#include "nuvoton-cir.h"

static inline void nvt_cr_write(struct nvt_dev *nvt, u8 val, u8 reg)
{
	outb(reg, nvt->cr_efir);
	outb(val, nvt->cr_efdr);
}

static inline u8 nvt_cr_read(struct nvt_dev *nvt, u8 reg)
{
	outb(reg, nvt->cr_efir);
	return inb(nvt->cr_efdr);
}

static inline void nvt_set_reg_bit(struct nvt_dev *nvt, u8 val, u8 reg)
{
	u8 tmp = nvt_cr_read(nvt, reg) | val;
	nvt_cr_write(nvt, tmp, reg);
}

static inline void nvt_clear_reg_bit(struct nvt_dev *nvt, u8 val, u8 reg)
{
	u8 tmp = nvt_cr_read(nvt, reg) & ~val;
	nvt_cr_write(nvt, tmp, reg);
}

static inline void nvt_efm_enable(struct nvt_dev *nvt)
{
	
	outb(EFER_EFM_ENABLE, nvt->cr_efir);
	outb(EFER_EFM_ENABLE, nvt->cr_efir);
}

static inline void nvt_efm_disable(struct nvt_dev *nvt)
{
	outb(EFER_EFM_DISABLE, nvt->cr_efir);
}

static inline void nvt_select_logical_dev(struct nvt_dev *nvt, u8 ldev)
{
	outb(CR_LOGICAL_DEV_SEL, nvt->cr_efir);
	outb(ldev, nvt->cr_efdr);
}

static inline void nvt_cir_reg_write(struct nvt_dev *nvt, u8 val, u8 offset)
{
	outb(val, nvt->cir_addr + offset);
}

static u8 nvt_cir_reg_read(struct nvt_dev *nvt, u8 offset)
{
	u8 val;

	val = inb(nvt->cir_addr + offset);

	return val;
}

static inline void nvt_cir_wake_reg_write(struct nvt_dev *nvt,
					  u8 val, u8 offset)
{
	outb(val, nvt->cir_wake_addr + offset);
}

static u8 nvt_cir_wake_reg_read(struct nvt_dev *nvt, u8 offset)
{
	u8 val;

	val = inb(nvt->cir_wake_addr + offset);

	return val;
}

#define pr_reg(text, ...) \
	printk(KERN_INFO KBUILD_MODNAME ": " text, ## __VA_ARGS__)

static void cir_dump_regs(struct nvt_dev *nvt)
{
	nvt_efm_enable(nvt);
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);

	pr_reg("%s: Dump CIR logical device registers:\n", NVT_DRIVER_NAME);
	pr_reg(" * CR CIR ACTIVE :   0x%x\n",
	       nvt_cr_read(nvt, CR_LOGICAL_DEV_EN));
	pr_reg(" * CR CIR BASE ADDR: 0x%x\n",
	       (nvt_cr_read(nvt, CR_CIR_BASE_ADDR_HI) << 8) |
		nvt_cr_read(nvt, CR_CIR_BASE_ADDR_LO));
	pr_reg(" * CR CIR IRQ NUM:   0x%x\n",
	       nvt_cr_read(nvt, CR_CIR_IRQ_RSRC));

	nvt_efm_disable(nvt);

	pr_reg("%s: Dump CIR registers:\n", NVT_DRIVER_NAME);
	pr_reg(" * IRCON:     0x%x\n", nvt_cir_reg_read(nvt, CIR_IRCON));
	pr_reg(" * IRSTS:     0x%x\n", nvt_cir_reg_read(nvt, CIR_IRSTS));
	pr_reg(" * IREN:      0x%x\n", nvt_cir_reg_read(nvt, CIR_IREN));
	pr_reg(" * RXFCONT:   0x%x\n", nvt_cir_reg_read(nvt, CIR_RXFCONT));
	pr_reg(" * CP:        0x%x\n", nvt_cir_reg_read(nvt, CIR_CP));
	pr_reg(" * CC:        0x%x\n", nvt_cir_reg_read(nvt, CIR_CC));
	pr_reg(" * SLCH:      0x%x\n", nvt_cir_reg_read(nvt, CIR_SLCH));
	pr_reg(" * SLCL:      0x%x\n", nvt_cir_reg_read(nvt, CIR_SLCL));
	pr_reg(" * FIFOCON:   0x%x\n", nvt_cir_reg_read(nvt, CIR_FIFOCON));
	pr_reg(" * IRFIFOSTS: 0x%x\n", nvt_cir_reg_read(nvt, CIR_IRFIFOSTS));
	pr_reg(" * SRXFIFO:   0x%x\n", nvt_cir_reg_read(nvt, CIR_SRXFIFO));
	pr_reg(" * TXFCONT:   0x%x\n", nvt_cir_reg_read(nvt, CIR_TXFCONT));
	pr_reg(" * STXFIFO:   0x%x\n", nvt_cir_reg_read(nvt, CIR_STXFIFO));
	pr_reg(" * FCCH:      0x%x\n", nvt_cir_reg_read(nvt, CIR_FCCH));
	pr_reg(" * FCCL:      0x%x\n", nvt_cir_reg_read(nvt, CIR_FCCL));
	pr_reg(" * IRFSM:     0x%x\n", nvt_cir_reg_read(nvt, CIR_IRFSM));
}

static void cir_wake_dump_regs(struct nvt_dev *nvt)
{
	u8 i, fifo_len;

	nvt_efm_enable(nvt);
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR_WAKE);

	pr_reg("%s: Dump CIR WAKE logical device registers:\n",
	       NVT_DRIVER_NAME);
	pr_reg(" * CR CIR WAKE ACTIVE :   0x%x\n",
	       nvt_cr_read(nvt, CR_LOGICAL_DEV_EN));
	pr_reg(" * CR CIR WAKE BASE ADDR: 0x%x\n",
	       (nvt_cr_read(nvt, CR_CIR_BASE_ADDR_HI) << 8) |
		nvt_cr_read(nvt, CR_CIR_BASE_ADDR_LO));
	pr_reg(" * CR CIR WAKE IRQ NUM:   0x%x\n",
	       nvt_cr_read(nvt, CR_CIR_IRQ_RSRC));

	nvt_efm_disable(nvt);

	pr_reg("%s: Dump CIR WAKE registers\n", NVT_DRIVER_NAME);
	pr_reg(" * IRCON:          0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_IRCON));
	pr_reg(" * IRSTS:          0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_IRSTS));
	pr_reg(" * IREN:           0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_IREN));
	pr_reg(" * FIFO CMP DEEP:  0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFO_CMP_DEEP));
	pr_reg(" * FIFO CMP TOL:   0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFO_CMP_TOL));
	pr_reg(" * FIFO COUNT:     0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFO_COUNT));
	pr_reg(" * SLCH:           0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_SLCH));
	pr_reg(" * SLCL:           0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_SLCL));
	pr_reg(" * FIFOCON:        0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFOCON));
	pr_reg(" * SRXFSTS:        0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_SRXFSTS));
	pr_reg(" * SAMPLE RX FIFO: 0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_SAMPLE_RX_FIFO));
	pr_reg(" * WR FIFO DATA:   0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_WR_FIFO_DATA));
	pr_reg(" * RD FIFO ONLY:   0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_RD_FIFO_ONLY));
	pr_reg(" * RD FIFO ONLY IDX: 0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_RD_FIFO_ONLY_IDX));
	pr_reg(" * FIFO IGNORE:    0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFO_IGNORE));
	pr_reg(" * IRFSM:          0x%x\n",
	       nvt_cir_wake_reg_read(nvt, CIR_WAKE_IRFSM));

	fifo_len = nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFO_COUNT);
	pr_reg("%s: Dump CIR WAKE FIFO (len %d)\n", NVT_DRIVER_NAME, fifo_len);
	pr_reg("* Contents = ");
	for (i = 0; i < fifo_len; i++)
		printk(KERN_CONT "%02x ",
		       nvt_cir_wake_reg_read(nvt, CIR_WAKE_RD_FIFO_ONLY));
	printk(KERN_CONT "\n");
}

static int nvt_hw_detect(struct nvt_dev *nvt)
{
	unsigned long flags;
	u8 chip_major, chip_minor;
	int ret = 0;
	char chip_id[12];
	bool chip_unknown = false;

	nvt_efm_enable(nvt);

	
	chip_major = nvt_cr_read(nvt, CR_CHIP_ID_HI);
	if (chip_major == 0xff) {
		nvt->cr_efir = CR_EFIR2;
		nvt->cr_efdr = CR_EFDR2;
		nvt_efm_enable(nvt);
		chip_major = nvt_cr_read(nvt, CR_CHIP_ID_HI);
	}

	chip_minor = nvt_cr_read(nvt, CR_CHIP_ID_LO);

	
	switch (chip_major) {
	case CHIP_ID_HIGH_667:
		strcpy(chip_id, "w83667hg\0");
		if (chip_minor != CHIP_ID_LOW_667)
			chip_unknown = true;
		break;
	case CHIP_ID_HIGH_677B:
		strcpy(chip_id, "w83677hg\0");
		if (chip_minor != CHIP_ID_LOW_677B2 &&
		    chip_minor != CHIP_ID_LOW_677B3)
			chip_unknown = true;
		break;
	case CHIP_ID_HIGH_677C:
		strcpy(chip_id, "w83677hg-c\0");
		if (chip_minor != CHIP_ID_LOW_677C)
			chip_unknown = true;
		break;
	default:
		strcpy(chip_id, "w836x7hg\0");
		chip_unknown = true;
		break;
	}

	
	if (chip_unknown)
		nvt_pr(KERN_WARNING, "%s: unknown chip, id: 0x%02x 0x%02x, "
		       "it may not work...", chip_id, chip_major, chip_minor);
	else
		nvt_dbg("%s: chip id: 0x%02x 0x%02x",
			chip_id, chip_major, chip_minor);

	nvt_efm_disable(nvt);

	spin_lock_irqsave(&nvt->nvt_lock, flags);
	nvt->chip_major = chip_major;
	nvt->chip_minor = chip_minor;
	spin_unlock_irqrestore(&nvt->nvt_lock, flags);

	return ret;
}

static void nvt_cir_ldev_init(struct nvt_dev *nvt)
{
	u8 val, psreg, psmask, psval;

	if (nvt->chip_major == CHIP_ID_HIGH_667) {
		psreg = CR_MULTIFUNC_PIN_SEL;
		psmask = MULTIFUNC_PIN_SEL_MASK;
		psval = MULTIFUNC_ENABLE_CIR | MULTIFUNC_ENABLE_CIRWB;
	} else {
		psreg = CR_OUTPUT_PIN_SEL;
		psmask = OUTPUT_PIN_SEL_MASK;
		psval = OUTPUT_ENABLE_CIR | OUTPUT_ENABLE_CIRWB;
	}

	
	val = nvt_cr_read(nvt, psreg);
	val &= psmask;
	val |= psval;
	nvt_cr_write(nvt, val, psreg);

	
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);
	nvt_cr_write(nvt, LOGICAL_DEV_ENABLE, CR_LOGICAL_DEV_EN);

	nvt_cr_write(nvt, nvt->cir_addr >> 8, CR_CIR_BASE_ADDR_HI);
	nvt_cr_write(nvt, nvt->cir_addr & 0xff, CR_CIR_BASE_ADDR_LO);

	nvt_cr_write(nvt, nvt->cir_irq, CR_CIR_IRQ_RSRC);

	nvt_dbg("CIR initialized, base io port address: 0x%lx, irq: %d",
		nvt->cir_addr, nvt->cir_irq);
}

static void nvt_cir_wake_ldev_init(struct nvt_dev *nvt)
{
	
	nvt_select_logical_dev(nvt, LOGICAL_DEV_ACPI);
	nvt_cr_write(nvt, LOGICAL_DEV_ENABLE, CR_LOGICAL_DEV_EN);

	
	nvt_set_reg_bit(nvt, CIR_WAKE_ENABLE_BIT, CR_ACPI_CIR_WAKE);

	
	nvt_set_reg_bit(nvt, CIR_INTR_MOUSE_IRQ_BIT, CR_ACPI_IRQ_EVENTS);

	
	nvt_set_reg_bit(nvt, PME_INTR_CIR_PASS_BIT, CR_ACPI_IRQ_EVENTS2);

	
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR_WAKE);
	nvt_cr_write(nvt, LOGICAL_DEV_ENABLE, CR_LOGICAL_DEV_EN);

	nvt_cr_write(nvt, nvt->cir_wake_addr >> 8, CR_CIR_BASE_ADDR_HI);
	nvt_cr_write(nvt, nvt->cir_wake_addr & 0xff, CR_CIR_BASE_ADDR_LO);

	nvt_cr_write(nvt, nvt->cir_wake_irq, CR_CIR_IRQ_RSRC);

	nvt_dbg("CIR Wake initialized, base io port address: 0x%lx, irq: %d",
		nvt->cir_wake_addr, nvt->cir_wake_irq);
}

static void nvt_clear_cir_fifo(struct nvt_dev *nvt)
{
	u8 val;

	val = nvt_cir_reg_read(nvt, CIR_FIFOCON);
	nvt_cir_reg_write(nvt, val | CIR_FIFOCON_RXFIFOCLR, CIR_FIFOCON);
}

static void nvt_clear_cir_wake_fifo(struct nvt_dev *nvt)
{
	u8 val;

	val = nvt_cir_wake_reg_read(nvt, CIR_WAKE_FIFOCON);
	nvt_cir_wake_reg_write(nvt, val | CIR_WAKE_FIFOCON_RXFIFOCLR,
			       CIR_WAKE_FIFOCON);
}

static void nvt_clear_tx_fifo(struct nvt_dev *nvt)
{
	u8 val;

	val = nvt_cir_reg_read(nvt, CIR_FIFOCON);
	nvt_cir_reg_write(nvt, val | CIR_FIFOCON_TXFIFOCLR, CIR_FIFOCON);
}

static void nvt_set_cir_iren(struct nvt_dev *nvt)
{
	u8 iren;

	iren = CIR_IREN_RTR | CIR_IREN_PE;
	nvt_cir_reg_write(nvt, iren, CIR_IREN);
}

static void nvt_cir_regs_init(struct nvt_dev *nvt)
{
	
	nvt_cir_reg_write(nvt, CIR_RX_LIMIT_COUNT >> 8, CIR_SLCH);
	nvt_cir_reg_write(nvt, CIR_RX_LIMIT_COUNT & 0xff, CIR_SLCL);

	
	nvt_cir_reg_write(nvt, CIR_FIFOCON_TX_TRIGGER_LEV |
			  CIR_FIFOCON_RX_TRIGGER_LEV, CIR_FIFOCON);

	nvt_cir_reg_write(nvt,
			  CIR_IRCON_TXEN | CIR_IRCON_RXEN |
			  CIR_IRCON_RXINV | CIR_IRCON_SAMPLE_PERIOD_SEL,
			  CIR_IRCON);

	
	nvt_clear_cir_fifo(nvt);
	nvt_clear_tx_fifo(nvt);

	
	nvt_cir_reg_write(nvt, 0xff, CIR_IRSTS);

	
	nvt_set_cir_iren(nvt);
}

static void nvt_cir_wake_regs_init(struct nvt_dev *nvt)
{
	
	nvt_cir_wake_reg_write(nvt, CIR_WAKE_FIFO_CMP_BYTES,
			       CIR_WAKE_FIFO_CMP_DEEP);

	
	nvt_cir_wake_reg_write(nvt, CIR_WAKE_CMP_TOLERANCE,
			       CIR_WAKE_FIFO_CMP_TOL);

	
	nvt_cir_wake_reg_write(nvt, CIR_RX_LIMIT_COUNT >> 8, CIR_WAKE_SLCH);
	nvt_cir_wake_reg_write(nvt, CIR_RX_LIMIT_COUNT & 0xff, CIR_WAKE_SLCL);

	
	nvt_cir_wake_reg_write(nvt, CIR_WAKE_FIFOCON_RX_TRIGGER_LEV,
			       CIR_WAKE_FIFOCON);

	nvt_cir_wake_reg_write(nvt, CIR_WAKE_IRCON_MODE0 | CIR_WAKE_IRCON_RXEN |
			       CIR_WAKE_IRCON_R | CIR_WAKE_IRCON_RXINV |
			       CIR_WAKE_IRCON_SAMPLE_PERIOD_SEL,
			       CIR_WAKE_IRCON);

	
	nvt_clear_cir_wake_fifo(nvt);

	
	nvt_cir_wake_reg_write(nvt, 0xff, CIR_WAKE_IRSTS);
}

static void nvt_enable_wake(struct nvt_dev *nvt)
{
	nvt_efm_enable(nvt);

	nvt_select_logical_dev(nvt, LOGICAL_DEV_ACPI);
	nvt_set_reg_bit(nvt, CIR_WAKE_ENABLE_BIT, CR_ACPI_CIR_WAKE);
	nvt_set_reg_bit(nvt, CIR_INTR_MOUSE_IRQ_BIT, CR_ACPI_IRQ_EVENTS);
	nvt_set_reg_bit(nvt, PME_INTR_CIR_PASS_BIT, CR_ACPI_IRQ_EVENTS2);

	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR_WAKE);
	nvt_cr_write(nvt, LOGICAL_DEV_ENABLE, CR_LOGICAL_DEV_EN);

	nvt_efm_disable(nvt);

	nvt_cir_wake_reg_write(nvt, CIR_WAKE_IRCON_MODE0 | CIR_WAKE_IRCON_RXEN |
			       CIR_WAKE_IRCON_R | CIR_WAKE_IRCON_RXINV |
			       CIR_WAKE_IRCON_SAMPLE_PERIOD_SEL,
			       CIR_WAKE_IRCON);
	nvt_cir_wake_reg_write(nvt, 0xff, CIR_WAKE_IRSTS);
	nvt_cir_wake_reg_write(nvt, 0, CIR_WAKE_IREN);
}

static u32 nvt_rx_carrier_detect(struct nvt_dev *nvt)
{
	u32 count, carrier, duration = 0;
	int i;

	count = nvt_cir_reg_read(nvt, CIR_FCCL) |
		nvt_cir_reg_read(nvt, CIR_FCCH) << 8;

	for (i = 0; i < nvt->pkts; i++) {
		if (nvt->buf[i] & BUF_PULSE_BIT)
			duration += nvt->buf[i] & BUF_LEN_MASK;
	}

	duration *= SAMPLE_PERIOD;

	if (!count || !duration) {
		nvt_pr(KERN_NOTICE, "Unable to determine carrier! (c:%u, d:%u)",
		       count, duration);
		return 0;
	}

	carrier = MS_TO_NS(count) / duration;

	if ((carrier > MAX_CARRIER) || (carrier < MIN_CARRIER))
		nvt_dbg("WTF? Carrier frequency out of range!");

	nvt_dbg("Carrier frequency: %u (count %u, duration %u)",
		carrier, count, duration);

	return carrier;
}

static int nvt_set_tx_carrier(struct rc_dev *dev, u32 carrier)
{
	struct nvt_dev *nvt = dev->priv;
	u16 val;

	nvt_cir_reg_write(nvt, 1, CIR_CP);
	val = 3000000 / (carrier) - 1;
	nvt_cir_reg_write(nvt, val & 0xff, CIR_CC);

	nvt_dbg("cp: 0x%x cc: 0x%x\n",
		nvt_cir_reg_read(nvt, CIR_CP), nvt_cir_reg_read(nvt, CIR_CC));

	return 0;
}

static int nvt_tx_ir(struct rc_dev *dev, unsigned *txbuf, unsigned n)
{
	struct nvt_dev *nvt = dev->priv;
	unsigned long flags;
	unsigned int i;
	u8 iren;
	int ret;

	spin_lock_irqsave(&nvt->tx.lock, flags);

	ret = min((unsigned)(TX_BUF_LEN / sizeof(unsigned)), n);
	nvt->tx.buf_count = (ret * sizeof(unsigned));

	memcpy(nvt->tx.buf, txbuf, nvt->tx.buf_count);

	nvt->tx.cur_buf_num = 0;

	
	iren = nvt_cir_reg_read(nvt, CIR_IREN);

	
	nvt_cir_reg_write(nvt, CIR_IREN_TFU | CIR_IREN_TTR, CIR_IREN);

	nvt->tx.tx_state = ST_TX_REPLY;

	nvt_cir_reg_write(nvt, CIR_FIFOCON_TX_TRIGGER_LEV_8 |
			  CIR_FIFOCON_RXFIFOCLR, CIR_FIFOCON);

	
	for (i = 0; i < 9; i++)
		nvt_cir_reg_write(nvt, 0x01, CIR_STXFIFO);

	spin_unlock_irqrestore(&nvt->tx.lock, flags);

	wait_event(nvt->tx.queue, nvt->tx.tx_state == ST_TX_REQUEST);

	spin_lock_irqsave(&nvt->tx.lock, flags);
	nvt->tx.tx_state = ST_TX_NONE;
	spin_unlock_irqrestore(&nvt->tx.lock, flags);

	
	nvt_cir_reg_write(nvt, iren, CIR_IREN);

	return ret;
}

static void nvt_dump_rx_buf(struct nvt_dev *nvt)
{
	int i;

	printk(KERN_DEBUG "%s (len %d): ", __func__, nvt->pkts);
	for (i = 0; (i < nvt->pkts) && (i < RX_BUF_LEN); i++)
		printk(KERN_CONT "0x%02x ", nvt->buf[i]);
	printk(KERN_CONT "\n");
}

static void nvt_process_rx_ir_data(struct nvt_dev *nvt)
{
	DEFINE_IR_RAW_EVENT(rawir);
	u32 carrier;
	u8 sample;
	int i;

	nvt_dbg_verbose("%s firing", __func__);

	if (debug)
		nvt_dump_rx_buf(nvt);

	if (nvt->carrier_detect_enabled)
		carrier = nvt_rx_carrier_detect(nvt);

	nvt_dbg_verbose("Processing buffer of len %d", nvt->pkts);

	init_ir_raw_event(&rawir);

	for (i = 0; i < nvt->pkts; i++) {
		sample = nvt->buf[i];

		rawir.pulse = ((sample & BUF_PULSE_BIT) != 0);
		rawir.duration = US_TO_NS((sample & BUF_LEN_MASK)
					  * SAMPLE_PERIOD);

		nvt_dbg("Storing %s with duration %d",
			rawir.pulse ? "pulse" : "space", rawir.duration);

		ir_raw_event_store_with_filter(nvt->rdev, &rawir);

		if ((sample == BUF_PULSE_BIT) && (i + 1 < nvt->pkts)) {
			nvt_dbg("Calling ir_raw_event_handle (signal end)\n");
			ir_raw_event_handle(nvt->rdev);
		}
	}

	nvt->pkts = 0;

	nvt_dbg("Calling ir_raw_event_handle (buffer empty)\n");
	ir_raw_event_handle(nvt->rdev);

	nvt_dbg_verbose("%s done", __func__);
}

static void nvt_handle_rx_fifo_overrun(struct nvt_dev *nvt)
{
	nvt_pr(KERN_WARNING, "RX FIFO overrun detected, flushing data!");

	nvt->pkts = 0;
	nvt_clear_cir_fifo(nvt);
	ir_raw_event_reset(nvt->rdev);
}

static void nvt_get_rx_ir_data(struct nvt_dev *nvt)
{
	unsigned long flags;
	u8 fifocount, val;
	unsigned int b_idx;
	bool overrun = false;
	int i;

	
	fifocount = nvt_cir_reg_read(nvt, CIR_RXFCONT);
	
	if (fifocount == 0xff)
		return;
	
	else if (fifocount > RX_BUF_LEN) {
		overrun = true;
		fifocount = RX_BUF_LEN;
	}

	nvt_dbg("attempting to fetch %u bytes from hw rx fifo", fifocount);

	spin_lock_irqsave(&nvt->nvt_lock, flags);

	b_idx = nvt->pkts;

	
	if (b_idx + fifocount > RX_BUF_LEN) {
		nvt_process_rx_ir_data(nvt);
		b_idx = 0;
	}

	
	for (i = 0; i < fifocount; i++) {
		val = nvt_cir_reg_read(nvt, CIR_SRXFIFO);
		nvt->buf[b_idx + i] = val;
	}

	nvt->pkts += fifocount;
	nvt_dbg("%s: pkts now %d", __func__, nvt->pkts);

	nvt_process_rx_ir_data(nvt);

	if (overrun)
		nvt_handle_rx_fifo_overrun(nvt);

	spin_unlock_irqrestore(&nvt->nvt_lock, flags);
}

static void nvt_cir_log_irqs(u8 status, u8 iren)
{
	nvt_pr(KERN_INFO, "IRQ 0x%02x (IREN 0x%02x) :%s%s%s%s%s%s%s%s%s",
		status, iren,
		status & CIR_IRSTS_RDR	? " RDR"	: "",
		status & CIR_IRSTS_RTR	? " RTR"	: "",
		status & CIR_IRSTS_PE	? " PE"		: "",
		status & CIR_IRSTS_RFO	? " RFO"	: "",
		status & CIR_IRSTS_TE	? " TE"		: "",
		status & CIR_IRSTS_TTR	? " TTR"	: "",
		status & CIR_IRSTS_TFU	? " TFU"	: "",
		status & CIR_IRSTS_GH	? " GH"		: "",
		status & ~(CIR_IRSTS_RDR | CIR_IRSTS_RTR | CIR_IRSTS_PE |
			   CIR_IRSTS_RFO | CIR_IRSTS_TE | CIR_IRSTS_TTR |
			   CIR_IRSTS_TFU | CIR_IRSTS_GH) ? " ?" : "");
}

static bool nvt_cir_tx_inactive(struct nvt_dev *nvt)
{
	unsigned long flags;
	bool tx_inactive;
	u8 tx_state;

	spin_lock_irqsave(&nvt->tx.lock, flags);
	tx_state = nvt->tx.tx_state;
	spin_unlock_irqrestore(&nvt->tx.lock, flags);

	tx_inactive = (tx_state == ST_TX_NONE);

	return tx_inactive;
}

static irqreturn_t nvt_cir_isr(int irq, void *data)
{
	struct nvt_dev *nvt = data;
	u8 status, iren, cur_state;
	unsigned long flags;

	nvt_dbg_verbose("%s firing", __func__);

	nvt_efm_enable(nvt);
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);
	nvt_efm_disable(nvt);

	status = nvt_cir_reg_read(nvt, CIR_IRSTS);
	if (!status) {
		nvt_dbg_verbose("%s exiting, IRSTS 0x0", __func__);
		nvt_cir_reg_write(nvt, 0xff, CIR_IRSTS);
		return IRQ_RETVAL(IRQ_NONE);
	}

	
	nvt_cir_reg_write(nvt, status, CIR_IRSTS);
	nvt_cir_reg_write(nvt, 0, CIR_IRSTS);

	
	iren = nvt_cir_reg_read(nvt, CIR_IREN);
	if (!iren) {
		nvt_dbg_verbose("%s exiting, CIR not enabled", __func__);
		return IRQ_RETVAL(IRQ_NONE);
	}

	if (debug)
		nvt_cir_log_irqs(status, iren);

	if (status & CIR_IRSTS_RTR) {
		
		
		if (nvt_cir_tx_inactive(nvt))
			nvt_get_rx_ir_data(nvt);
	}

	if (status & CIR_IRSTS_PE) {
		if (nvt_cir_tx_inactive(nvt))
			nvt_get_rx_ir_data(nvt);

		spin_lock_irqsave(&nvt->nvt_lock, flags);

		cur_state = nvt->study_state;

		spin_unlock_irqrestore(&nvt->nvt_lock, flags);

		if (cur_state == ST_STUDY_NONE)
			nvt_clear_cir_fifo(nvt);
	}

	if (status & CIR_IRSTS_TE)
		nvt_clear_tx_fifo(nvt);

	if (status & CIR_IRSTS_TTR) {
		unsigned int pos, count;
		u8 tmp;

		spin_lock_irqsave(&nvt->tx.lock, flags);

		pos = nvt->tx.cur_buf_num;
		count = nvt->tx.buf_count;

		
		if (pos < count) {
			nvt_cir_reg_write(nvt, nvt->tx.buf[pos], CIR_STXFIFO);
			nvt->tx.cur_buf_num++;
		
		} else {
			tmp = nvt_cir_reg_read(nvt, CIR_IREN);
			nvt_cir_reg_write(nvt, tmp & ~CIR_IREN_TTR, CIR_IREN);
		}

		spin_unlock_irqrestore(&nvt->tx.lock, flags);

	}

	if (status & CIR_IRSTS_TFU) {
		spin_lock_irqsave(&nvt->tx.lock, flags);
		if (nvt->tx.tx_state == ST_TX_REPLY) {
			nvt->tx.tx_state = ST_TX_REQUEST;
			wake_up(&nvt->tx.queue);
		}
		spin_unlock_irqrestore(&nvt->tx.lock, flags);
	}

	nvt_dbg_verbose("%s done", __func__);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static irqreturn_t nvt_cir_wake_isr(int irq, void *data)
{
	u8 status, iren, val;
	struct nvt_dev *nvt = data;
	unsigned long flags;

	nvt_dbg_wake("%s firing", __func__);

	status = nvt_cir_wake_reg_read(nvt, CIR_WAKE_IRSTS);
	if (!status)
		return IRQ_RETVAL(IRQ_NONE);

	if (status & CIR_WAKE_IRSTS_IR_PENDING)
		nvt_clear_cir_wake_fifo(nvt);

	nvt_cir_wake_reg_write(nvt, status, CIR_WAKE_IRSTS);
	nvt_cir_wake_reg_write(nvt, 0, CIR_WAKE_IRSTS);

	
	iren = nvt_cir_wake_reg_read(nvt, CIR_WAKE_IREN);
	if (!iren) {
		nvt_dbg_wake("%s exiting, wake not enabled", __func__);
		return IRQ_RETVAL(IRQ_HANDLED);
	}

	if ((status & CIR_WAKE_IRSTS_PE) &&
	    (nvt->wake_state == ST_WAKE_START)) {
		while (nvt_cir_wake_reg_read(nvt, CIR_WAKE_RD_FIFO_ONLY_IDX)) {
			val = nvt_cir_wake_reg_read(nvt, CIR_WAKE_RD_FIFO_ONLY);
			nvt_dbg("setting wake up key: 0x%x", val);
		}

		nvt_cir_wake_reg_write(nvt, 0, CIR_WAKE_IREN);
		spin_lock_irqsave(&nvt->nvt_lock, flags);
		nvt->wake_state = ST_WAKE_FINISH;
		spin_unlock_irqrestore(&nvt->nvt_lock, flags);
	}

	nvt_dbg_wake("%s done", __func__);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void nvt_enable_cir(struct nvt_dev *nvt)
{
	
	nvt_cir_reg_write(nvt, CIR_IRCON_TXEN | CIR_IRCON_RXEN |
			  CIR_IRCON_RXINV | CIR_IRCON_SAMPLE_PERIOD_SEL,
			  CIR_IRCON);

	nvt_efm_enable(nvt);

	
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);
	nvt_cr_write(nvt, LOGICAL_DEV_ENABLE, CR_LOGICAL_DEV_EN);

	nvt_efm_disable(nvt);

	
	nvt_cir_reg_write(nvt, 0xff, CIR_IRSTS);

	
	nvt_set_cir_iren(nvt);
}

static void nvt_disable_cir(struct nvt_dev *nvt)
{
	
	nvt_cir_reg_write(nvt, 0, CIR_IREN);

	
	nvt_cir_reg_write(nvt, 0xff, CIR_IRSTS);

	
	nvt_cir_reg_write(nvt, 0, CIR_IRCON);

	
	nvt_clear_cir_fifo(nvt);
	nvt_clear_tx_fifo(nvt);

	nvt_efm_enable(nvt);

	
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);
	nvt_cr_write(nvt, LOGICAL_DEV_DISABLE, CR_LOGICAL_DEV_EN);

	nvt_efm_disable(nvt);
}

static int nvt_open(struct rc_dev *dev)
{
	struct nvt_dev *nvt = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&nvt->nvt_lock, flags);
	nvt_enable_cir(nvt);
	spin_unlock_irqrestore(&nvt->nvt_lock, flags);

	return 0;
}

static void nvt_close(struct rc_dev *dev)
{
	struct nvt_dev *nvt = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&nvt->nvt_lock, flags);
	nvt_disable_cir(nvt);
	spin_unlock_irqrestore(&nvt->nvt_lock, flags);
}

static int nvt_probe(struct pnp_dev *pdev, const struct pnp_device_id *dev_id)
{
	struct nvt_dev *nvt;
	struct rc_dev *rdev;
	int ret = -ENOMEM;

	nvt = kzalloc(sizeof(struct nvt_dev), GFP_KERNEL);
	if (!nvt)
		return ret;

	
	rdev = rc_allocate_device();
	if (!rdev)
		goto failure;

	ret = -ENODEV;
	
	if (!pnp_port_valid(pdev, 0) ||
	    pnp_port_len(pdev, 0) < CIR_IOREG_LENGTH) {
		dev_err(&pdev->dev, "IR PNP Port not valid!\n");
		goto failure;
	}

	if (!pnp_irq_valid(pdev, 0)) {
		dev_err(&pdev->dev, "PNP IRQ not valid!\n");
		goto failure;
	}

	if (!pnp_port_valid(pdev, 1) ||
	    pnp_port_len(pdev, 1) < CIR_IOREG_LENGTH) {
		dev_err(&pdev->dev, "Wake PNP Port not valid!\n");
		goto failure;
	}

	nvt->cir_addr = pnp_port_start(pdev, 0);
	nvt->cir_irq  = pnp_irq(pdev, 0);

	nvt->cir_wake_addr = pnp_port_start(pdev, 1);
	
	nvt->cir_wake_irq  = nvt->cir_irq;

	nvt->cr_efir = CR_EFIR;
	nvt->cr_efdr = CR_EFDR;

	spin_lock_init(&nvt->nvt_lock);
	spin_lock_init(&nvt->tx.lock);

	pnp_set_drvdata(pdev, nvt);
	nvt->pdev = pdev;

	init_waitqueue_head(&nvt->tx.queue);

	ret = nvt_hw_detect(nvt);
	if (ret)
		goto failure;

	
	nvt_efm_enable(nvt);
	nvt_cir_ldev_init(nvt);
	nvt_cir_wake_ldev_init(nvt);
	nvt_efm_disable(nvt);

	
	nvt_cir_regs_init(nvt);
	nvt_cir_wake_regs_init(nvt);

	
	rdev->priv = nvt;
	rdev->driver_type = RC_DRIVER_IR_RAW;
	rdev->allowed_protos = RC_TYPE_ALL;
	rdev->open = nvt_open;
	rdev->close = nvt_close;
	rdev->tx_ir = nvt_tx_ir;
	rdev->s_tx_carrier = nvt_set_tx_carrier;
	rdev->input_name = "Nuvoton w836x7hg Infrared Remote Transceiver";
	rdev->input_phys = "nuvoton/cir0";
	rdev->input_id.bustype = BUS_HOST;
	rdev->input_id.vendor = PCI_VENDOR_ID_WINBOND2;
	rdev->input_id.product = nvt->chip_major;
	rdev->input_id.version = nvt->chip_minor;
	rdev->dev.parent = &pdev->dev;
	rdev->driver_name = NVT_DRIVER_NAME;
	rdev->map_name = RC_MAP_RC6_MCE;
	rdev->timeout = MS_TO_NS(100);
	
	rdev->rx_resolution = US_TO_NS(CIR_SAMPLE_PERIOD);
#if 0
	rdev->min_timeout = XYZ;
	rdev->max_timeout = XYZ;
	
	rdev->tx_resolution = XYZ;
#endif

	ret = -EBUSY;
	
	if (!request_region(nvt->cir_addr,
			    CIR_IOREG_LENGTH, NVT_DRIVER_NAME))
		goto failure;

	if (request_irq(nvt->cir_irq, nvt_cir_isr, IRQF_SHARED,
			NVT_DRIVER_NAME, (void *)nvt))
		goto failure;

	if (!request_region(nvt->cir_wake_addr,
			    CIR_IOREG_LENGTH, NVT_DRIVER_NAME))
		goto failure;

	if (request_irq(nvt->cir_wake_irq, nvt_cir_wake_isr, IRQF_SHARED,
			NVT_DRIVER_NAME, (void *)nvt))
		goto failure;

	ret = rc_register_device(rdev);
	if (ret)
		goto failure;

	device_init_wakeup(&pdev->dev, true);
	nvt->rdev = rdev;
	nvt_pr(KERN_NOTICE, "driver has been successfully loaded\n");
	if (debug) {
		cir_dump_regs(nvt);
		cir_wake_dump_regs(nvt);
	}

	return 0;

failure:
	if (nvt->cir_irq)
		free_irq(nvt->cir_irq, nvt);
	if (nvt->cir_addr)
		release_region(nvt->cir_addr, CIR_IOREG_LENGTH);

	if (nvt->cir_wake_irq)
		free_irq(nvt->cir_wake_irq, nvt);
	if (nvt->cir_wake_addr)
		release_region(nvt->cir_wake_addr, CIR_IOREG_LENGTH);

	rc_free_device(rdev);
	kfree(nvt);

	return ret;
}

static void __devexit nvt_remove(struct pnp_dev *pdev)
{
	struct nvt_dev *nvt = pnp_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&nvt->nvt_lock, flags);
	
	nvt_cir_reg_write(nvt, 0, CIR_IREN);
	nvt_disable_cir(nvt);
	
	nvt_enable_wake(nvt);
	spin_unlock_irqrestore(&nvt->nvt_lock, flags);

	
	free_irq(nvt->cir_irq, nvt);
	free_irq(nvt->cir_wake_irq, nvt);
	release_region(nvt->cir_addr, CIR_IOREG_LENGTH);
	release_region(nvt->cir_wake_addr, CIR_IOREG_LENGTH);

	rc_unregister_device(nvt->rdev);

	kfree(nvt);
}

static int nvt_suspend(struct pnp_dev *pdev, pm_message_t state)
{
	struct nvt_dev *nvt = pnp_get_drvdata(pdev);
	unsigned long flags;

	nvt_dbg("%s called", __func__);

	
	spin_lock_irqsave(&nvt->nvt_lock, flags);
	nvt->study_state = ST_STUDY_NONE;
	nvt->wake_state = ST_WAKE_NONE;
	spin_unlock_irqrestore(&nvt->nvt_lock, flags);

	spin_lock_irqsave(&nvt->tx.lock, flags);
	nvt->tx.tx_state = ST_TX_NONE;
	spin_unlock_irqrestore(&nvt->tx.lock, flags);

	
	nvt_cir_reg_write(nvt, 0, CIR_IREN);

	nvt_efm_enable(nvt);

	
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);
	nvt_cr_write(nvt, LOGICAL_DEV_DISABLE, CR_LOGICAL_DEV_EN);

	nvt_efm_disable(nvt);

	
	nvt_enable_wake(nvt);

	return 0;
}

static int nvt_resume(struct pnp_dev *pdev)
{
	int ret = 0;
	struct nvt_dev *nvt = pnp_get_drvdata(pdev);

	nvt_dbg("%s called", __func__);

	
	nvt_set_cir_iren(nvt);

	
	nvt_efm_enable(nvt);
	nvt_select_logical_dev(nvt, LOGICAL_DEV_CIR);
	nvt_cr_write(nvt, LOGICAL_DEV_ENABLE, CR_LOGICAL_DEV_EN);

	nvt_efm_disable(nvt);

	nvt_cir_regs_init(nvt);
	nvt_cir_wake_regs_init(nvt);

	return ret;
}

static void nvt_shutdown(struct pnp_dev *pdev)
{
	struct nvt_dev *nvt = pnp_get_drvdata(pdev);
	nvt_enable_wake(nvt);
}

static const struct pnp_device_id nvt_ids[] = {
	{ "WEC0530", 0 },   
	{ "NTN0530", 0 },   
	{ "", 0 },
};

static struct pnp_driver nvt_driver = {
	.name		= NVT_DRIVER_NAME,
	.id_table	= nvt_ids,
	.flags		= PNP_DRIVER_RES_DO_NOT_CHANGE,
	.probe		= nvt_probe,
	.remove		= __devexit_p(nvt_remove),
	.suspend	= nvt_suspend,
	.resume		= nvt_resume,
	.shutdown	= nvt_shutdown,
};

int nvt_init(void)
{
	return pnp_register_driver(&nvt_driver);
}

void nvt_exit(void)
{
	pnp_unregister_driver(&nvt_driver);
}

module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debugging output");

MODULE_DEVICE_TABLE(pnp, nvt_ids);
MODULE_DESCRIPTION("Nuvoton W83667HG-A & W83677HG-I CIR driver");

MODULE_AUTHOR("Jarod Wilson <jarod@redhat.com>");
MODULE_LICENSE("GPL");

module_init(nvt_init);
module_exit(nvt_exit);
