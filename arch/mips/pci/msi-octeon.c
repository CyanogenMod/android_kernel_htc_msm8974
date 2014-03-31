/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2009, 2010 Cavium Networks
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/msi.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-npi-defs.h>
#include <asm/octeon/cvmx-pci-defs.h>
#include <asm/octeon/cvmx-npei-defs.h>
#include <asm/octeon/cvmx-pexp-defs.h>
#include <asm/octeon/pci-octeon.h>

static u64 msi_free_irq_bitmask[4];

static u64 msi_multiple_irq_bitmask[4];

static DEFINE_SPINLOCK(msi_free_irq_bitmask_lock);

static int msi_irq_size;

int arch_setup_msi_irq(struct pci_dev *dev, struct msi_desc *desc)
{
	struct msi_msg msg;
	u16 control;
	int configured_private_bits;
	int request_private_bits;
	int irq = 0;
	int irq_step;
	u64 search_mask;
	int index;

	pci_read_config_word(dev, desc->msi_attrib.pos + PCI_MSI_FLAGS,
			     &control);

	configured_private_bits = (control & PCI_MSI_FLAGS_QSIZE) >> 4;
	if (configured_private_bits == 0) {
		
		request_private_bits = (control & PCI_MSI_FLAGS_QMASK) >> 1;
	} else {
		request_private_bits = configured_private_bits;
	}

	if (request_private_bits > 5)
		request_private_bits = 0;

try_only_one:
	irq_step = 1 << request_private_bits;

	
	search_mask = (1 << irq_step) - 1;

	spin_lock(&msi_free_irq_bitmask_lock);
	for (index = 0; index < msi_irq_size/64; index++) {
		for (irq = 0; irq < 64; irq += irq_step) {
			if ((msi_free_irq_bitmask[index] & (search_mask << irq)) == 0) {
				msi_free_irq_bitmask[index] |= search_mask << irq;
				msi_multiple_irq_bitmask[index] |= (search_mask >> 1) << irq;
				goto msi_irq_allocated;
			}
		}
	}
msi_irq_allocated:
	spin_unlock(&msi_free_irq_bitmask_lock);

	
	if (irq >= 64) {
		if (request_private_bits) {
			pr_err("arch_setup_msi_irq: Unable to find %d free interrupts, trying just one",
			       1 << request_private_bits);
			request_private_bits = 0;
			goto try_only_one;
		} else
			panic("arch_setup_msi_irq: Unable to find a free MSI interrupt");
	}

	
	irq += index*64;
	irq += OCTEON_IRQ_MSI_BIT0;

	switch (octeon_dma_bar_type) {
	case OCTEON_DMA_BAR_TYPE_SMALL:
		
		msg.address_lo =
			((128ul << 20) + CVMX_PCI_MSI_RCV) & 0xffffffff;
		msg.address_hi = ((128ul << 20) + CVMX_PCI_MSI_RCV) >> 32;
	case OCTEON_DMA_BAR_TYPE_BIG:
		
		msg.address_lo = (0 + CVMX_PCI_MSI_RCV) & 0xffffffff;
		msg.address_hi = (0 + CVMX_PCI_MSI_RCV) >> 32;
		break;
	case OCTEON_DMA_BAR_TYPE_PCIE:
		
		
		msg.address_lo = (0 + CVMX_NPEI_PCIE_MSI_RCV) & 0xffffffff;
		msg.address_hi = (0 + CVMX_NPEI_PCIE_MSI_RCV) >> 32;
		break;
	default:
		panic("arch_setup_msi_irq: Invalid octeon_dma_bar_type");
	}
	msg.data = irq - OCTEON_IRQ_MSI_BIT0;

	
	control &= ~PCI_MSI_FLAGS_QSIZE;
	control |= request_private_bits << 4;
	pci_write_config_word(dev, desc->msi_attrib.pos + PCI_MSI_FLAGS,
			      control);

	irq_set_msi_desc(irq, desc);
	write_msi_msg(irq, &msg);
	return 0;
}

int arch_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;

	if (type == PCI_CAP_ID_MSIX)
		return -EINVAL;

	if (type == PCI_CAP_ID_MSI && nvec > 1)
		return 1;

	list_for_each_entry(entry, &dev->msi_list, list) {
		ret = arch_setup_msi_irq(dev, entry);
		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

	return 0;
}

void arch_teardown_msi_irq(unsigned int irq)
{
	int number_irqs;
	u64 bitmask;
	int index = 0;
	int irq0;

	if ((irq < OCTEON_IRQ_MSI_BIT0)
		|| (irq > msi_irq_size + OCTEON_IRQ_MSI_BIT0))
		panic("arch_teardown_msi_irq: Attempted to teardown illegal "
		      "MSI interrupt (%d)", irq);

	irq -= OCTEON_IRQ_MSI_BIT0;
	index = irq / 64;
	irq0 = irq % 64;

	number_irqs = 0;
	while ((irq0 + number_irqs < 64) &&
	       (msi_multiple_irq_bitmask[index]
		& (1ull << (irq0 + number_irqs))))
		number_irqs++;
	number_irqs++;
	
	bitmask = (1 << number_irqs) - 1;
	
	bitmask <<= irq0;
	if ((msi_free_irq_bitmask[index] & bitmask) != bitmask)
		panic("arch_teardown_msi_irq: Attempted to teardown MSI "
		      "interrupt (%d) not in use", irq);

	
	spin_lock(&msi_free_irq_bitmask_lock);
	msi_free_irq_bitmask[index] &= ~bitmask;
	msi_multiple_irq_bitmask[index] &= ~bitmask;
	spin_unlock(&msi_free_irq_bitmask_lock);
}

static DEFINE_RAW_SPINLOCK(octeon_irq_msi_lock);

static u64 msi_rcv_reg[4];
static u64 mis_ena_reg[4];

static void octeon_irq_msi_enable_pcie(struct irq_data *data)
{
	u64 en;
	unsigned long flags;
	int msi_number = data->irq - OCTEON_IRQ_MSI_BIT0;
	int irq_index = msi_number >> 6;
	int irq_bit = msi_number & 0x3f;

	raw_spin_lock_irqsave(&octeon_irq_msi_lock, flags);
	en = cvmx_read_csr(mis_ena_reg[irq_index]);
	en |= 1ull << irq_bit;
	cvmx_write_csr(mis_ena_reg[irq_index], en);
	cvmx_read_csr(mis_ena_reg[irq_index]);
	raw_spin_unlock_irqrestore(&octeon_irq_msi_lock, flags);
}

static void octeon_irq_msi_disable_pcie(struct irq_data *data)
{
	u64 en;
	unsigned long flags;
	int msi_number = data->irq - OCTEON_IRQ_MSI_BIT0;
	int irq_index = msi_number >> 6;
	int irq_bit = msi_number & 0x3f;

	raw_spin_lock_irqsave(&octeon_irq_msi_lock, flags);
	en = cvmx_read_csr(mis_ena_reg[irq_index]);
	en &= ~(1ull << irq_bit);
	cvmx_write_csr(mis_ena_reg[irq_index], en);
	cvmx_read_csr(mis_ena_reg[irq_index]);
	raw_spin_unlock_irqrestore(&octeon_irq_msi_lock, flags);
}

static struct irq_chip octeon_irq_chip_msi_pcie = {
	.name = "MSI",
	.irq_enable = octeon_irq_msi_enable_pcie,
	.irq_disable = octeon_irq_msi_disable_pcie,
};

static void octeon_irq_msi_enable_pci(struct irq_data *data)
{
}

static void octeon_irq_msi_disable_pci(struct irq_data *data)
{
	
}

static struct irq_chip octeon_irq_chip_msi_pci = {
	.name = "MSI",
	.irq_enable = octeon_irq_msi_enable_pci,
	.irq_disable = octeon_irq_msi_disable_pci,
};

static irqreturn_t __octeon_msi_do_interrupt(int index, u64 msi_bits)
{
	int irq;
	int bit;

	bit = fls64(msi_bits);
	if (bit) {
		bit--;
		
		cvmx_write_csr(msi_rcv_reg[index], 1ull << bit);

		irq = bit + OCTEON_IRQ_MSI_BIT0 + 64 * index;
		do_IRQ(irq);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

#define OCTEON_MSI_INT_HANDLER_X(x)					\
static irqreturn_t octeon_msi_interrupt##x(int cpl, void *dev_id)	\
{									\
	u64 msi_bits = cvmx_read_csr(msi_rcv_reg[(x)]);			\
	return __octeon_msi_do_interrupt((x), msi_bits);		\
}

OCTEON_MSI_INT_HANDLER_X(0);
OCTEON_MSI_INT_HANDLER_X(1);
OCTEON_MSI_INT_HANDLER_X(2);
OCTEON_MSI_INT_HANDLER_X(3);

int __init octeon_msi_initialize(void)
{
	int irq;
	struct irq_chip *msi;

	if (octeon_dma_bar_type == OCTEON_DMA_BAR_TYPE_PCIE) {
		msi_rcv_reg[0] = CVMX_PEXP_NPEI_MSI_RCV0;
		msi_rcv_reg[1] = CVMX_PEXP_NPEI_MSI_RCV1;
		msi_rcv_reg[2] = CVMX_PEXP_NPEI_MSI_RCV2;
		msi_rcv_reg[3] = CVMX_PEXP_NPEI_MSI_RCV3;
		mis_ena_reg[0] = CVMX_PEXP_NPEI_MSI_ENB0;
		mis_ena_reg[1] = CVMX_PEXP_NPEI_MSI_ENB1;
		mis_ena_reg[2] = CVMX_PEXP_NPEI_MSI_ENB2;
		mis_ena_reg[3] = CVMX_PEXP_NPEI_MSI_ENB3;
		msi = &octeon_irq_chip_msi_pcie;
	} else {
		msi_rcv_reg[0] = CVMX_NPI_NPI_MSI_RCV;
#define INVALID_GENERATE_ADE 0x8700000000000000ULL;
		msi_rcv_reg[1] = INVALID_GENERATE_ADE;
		msi_rcv_reg[2] = INVALID_GENERATE_ADE;
		msi_rcv_reg[3] = INVALID_GENERATE_ADE;
		mis_ena_reg[0] = INVALID_GENERATE_ADE;
		mis_ena_reg[1] = INVALID_GENERATE_ADE;
		mis_ena_reg[2] = INVALID_GENERATE_ADE;
		mis_ena_reg[3] = INVALID_GENERATE_ADE;
		msi = &octeon_irq_chip_msi_pci;
	}

	for (irq = OCTEON_IRQ_MSI_BIT0; irq <= OCTEON_IRQ_MSI_LAST; irq++)
		irq_set_chip_and_handler(irq, msi, handle_simple_irq);

	if (octeon_has_feature(OCTEON_FEATURE_PCIE)) {
		if (request_irq(OCTEON_IRQ_PCI_MSI0, octeon_msi_interrupt0,
				0, "MSI[0:63]", octeon_msi_interrupt0))
			panic("request_irq(OCTEON_IRQ_PCI_MSI0) failed");

		if (request_irq(OCTEON_IRQ_PCI_MSI1, octeon_msi_interrupt1,
				0, "MSI[64:127]", octeon_msi_interrupt1))
			panic("request_irq(OCTEON_IRQ_PCI_MSI1) failed");

		if (request_irq(OCTEON_IRQ_PCI_MSI2, octeon_msi_interrupt2,
				0, "MSI[127:191]", octeon_msi_interrupt2))
			panic("request_irq(OCTEON_IRQ_PCI_MSI2) failed");

		if (request_irq(OCTEON_IRQ_PCI_MSI3, octeon_msi_interrupt3,
				0, "MSI[192:255]", octeon_msi_interrupt3))
			panic("request_irq(OCTEON_IRQ_PCI_MSI3) failed");

		msi_irq_size = 256;
	} else if (octeon_is_pci_host()) {
		if (request_irq(OCTEON_IRQ_PCI_MSI0, octeon_msi_interrupt0,
				0, "MSI[0:15]", octeon_msi_interrupt0))
			panic("request_irq(OCTEON_IRQ_PCI_MSI0) failed");

		if (request_irq(OCTEON_IRQ_PCI_MSI1, octeon_msi_interrupt0,
				0, "MSI[16:31]", octeon_msi_interrupt0))
			panic("request_irq(OCTEON_IRQ_PCI_MSI1) failed");

		if (request_irq(OCTEON_IRQ_PCI_MSI2, octeon_msi_interrupt0,
				0, "MSI[32:47]", octeon_msi_interrupt0))
			panic("request_irq(OCTEON_IRQ_PCI_MSI2) failed");

		if (request_irq(OCTEON_IRQ_PCI_MSI3, octeon_msi_interrupt0,
				0, "MSI[48:63]", octeon_msi_interrupt0))
			panic("request_irq(OCTEON_IRQ_PCI_MSI3) failed");
		msi_irq_size = 64;
	}
	return 0;
}
subsys_initcall(octeon_msi_initialize);
