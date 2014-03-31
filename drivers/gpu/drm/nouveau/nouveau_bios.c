/*
 * Copyright 2005-2006 Erik Waling
 * Copyright 2006 Stephane Marchesin
 * Copyright 2007-2009 Stuart Bennett
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "drmP.h"
#define NV_DEBUG_NOTRACE
#include "nouveau_drv.h"
#include "nouveau_hw.h"
#include "nouveau_encoder.h"
#include "nouveau_gpio.h"

#include <linux/io-mapping.h>

#define NV_CIO_CRE_44_HEADA 0x0
#define NV_CIO_CRE_44_HEADB 0x3
#define FEATURE_MOBILE 0x10	

#define EDID1_LEN 128

#define BIOSLOG(sip, fmt, arg...) NV_DEBUG(sip->dev, fmt, ##arg)
#define LOG_OLD_VALUE(x)

struct init_exec {
	bool execute;
	bool repeat;
};

static bool nv_cksum(const uint8_t *data, unsigned int length)
{
	int i;
	uint8_t sum = 0;

	for (i = 0; i < length; i++)
		sum += data[i];

	if (sum)
		return true;

	return false;
}

static int
score_vbios(struct nvbios *bios, const bool writeable)
{
	if (!bios->data || bios->data[0] != 0x55 || bios->data[1] != 0xAA) {
		NV_TRACEWARN(bios->dev, "... BIOS signature not found\n");
		return 0;
	}

	if (nv_cksum(bios->data, bios->data[2] * 512)) {
		NV_TRACEWARN(bios->dev, "... BIOS checksum invalid\n");
		
		return writeable ? 2 : 1;
	}

	NV_TRACE(bios->dev, "... appears to be valid\n");
	return 3;
}

static void
bios_shadow_prom(struct nvbios *bios)
{
	struct drm_device *dev = bios->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	u32 pcireg, access;
	u16 pcir;
	int i;

	
	if (dev_priv->card_type >= NV_50)
		pcireg = 0x088050;
	else
		pcireg = NV_PBUS_PCI_NV_20;
	access = nv_mask(dev, pcireg, 0x00000001, 0x00000000);

	i = 16;
	do {
		if (nv_rd08(dev, NV_PROM_OFFSET + 0) == 0x55)
			break;
	} while (i--);

	if (!i || nv_rd08(dev, NV_PROM_OFFSET + 1) != 0xaa)
		goto out;

	
	pcir = nv_rd08(dev, NV_PROM_OFFSET + 0x18) |
	       nv_rd08(dev, NV_PROM_OFFSET + 0x19) << 8;
	if (nv_rd08(dev, NV_PROM_OFFSET + pcir + 0) != 'P' ||
	    nv_rd08(dev, NV_PROM_OFFSET + pcir + 1) != 'C' ||
	    nv_rd08(dev, NV_PROM_OFFSET + pcir + 2) != 'I' ||
	    nv_rd08(dev, NV_PROM_OFFSET + pcir + 3) != 'R')
		goto out;

	
	bios->length = nv_rd08(dev, NV_PROM_OFFSET + 2) * 512;
	bios->data = kmalloc(bios->length, GFP_KERNEL);
	if (bios->data) {
		for (i = 0; i < bios->length; i++)
			bios->data[i] = nv_rd08(dev, NV_PROM_OFFSET + i);
	}

out:
	
	nv_wr32(dev, pcireg, access);
}

static void
bios_shadow_pramin(struct nvbios *bios)
{
	struct drm_device *dev = bios->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	u32 bar0 = 0;
	int i;

	if (dev_priv->card_type >= NV_50) {
		u64 addr = (u64)(nv_rd32(dev, 0x619f04) & 0xffffff00) << 8;
		if (!addr) {
			addr  = (u64)nv_rd32(dev, 0x001700) << 16;
			addr += 0xf0000;
		}

		bar0 = nv_mask(dev, 0x001700, 0xffffffff, addr >> 16);
	}

	
	if (nv_rd08(dev, NV_PRAMIN_OFFSET + 0) != 0x55 ||
	    nv_rd08(dev, NV_PRAMIN_OFFSET + 1) != 0xaa)
		goto out;

	bios->length = nv_rd08(dev, NV_PRAMIN_OFFSET + 2) * 512;
	bios->data = kmalloc(bios->length, GFP_KERNEL);
	if (bios->data) {
		for (i = 0; i < bios->length; i++)
			bios->data[i] = nv_rd08(dev, NV_PRAMIN_OFFSET + i);
	}

out:
	if (dev_priv->card_type >= NV_50)
		nv_wr32(dev, 0x001700, bar0);
}

static void
bios_shadow_pci(struct nvbios *bios)
{
	struct pci_dev *pdev = bios->dev->pdev;
	size_t length;

	if (!pci_enable_rom(pdev)) {
		void __iomem *rom = pci_map_rom(pdev, &length);
		if (rom && length) {
			bios->data = kmalloc(length, GFP_KERNEL);
			if (bios->data) {
				memcpy_fromio(bios->data, rom, length);
				bios->length = length;
			}
		}
		if (rom)
			pci_unmap_rom(pdev, rom);

		pci_disable_rom(pdev);
	}
}

static void
bios_shadow_acpi(struct nvbios *bios)
{
	struct pci_dev *pdev = bios->dev->pdev;
	int ptr, len, ret;
	u8 data[3];

	if (!nouveau_acpi_rom_supported(pdev))
		return;

	ret = nouveau_acpi_get_bios_chunk(data, 0, sizeof(data));
	if (ret != sizeof(data))
		return;

	bios->length = min(data[2] * 512, 65536);
	bios->data = kmalloc(bios->length, GFP_KERNEL);
	if (!bios->data)
		return;

	len = bios->length;
	ptr = 0;
	while (len) {
		int size = (len > ROM_BIOS_PAGE) ? ROM_BIOS_PAGE : len;

		ret = nouveau_acpi_get_bios_chunk(bios->data, ptr, size);
		if (ret != size) {
			kfree(bios->data);
			bios->data = NULL;
			return;
		}

		len -= size;
		ptr += size;
	}
}

struct methods {
	const char desc[8];
	void (*shadow)(struct nvbios *);
	const bool rw;
	int score;
	u32 size;
	u8 *data;
};

static bool
bios_shadow(struct drm_device *dev)
{
	struct methods shadow_methods[] = {
		{ "PRAMIN", bios_shadow_pramin, true, 0, 0, NULL },
		{ "PROM", bios_shadow_prom, false, 0, 0, NULL },
		{ "ACPI", bios_shadow_acpi, true, 0, 0, NULL },
		{ "PCIROM", bios_shadow_pci, true, 0, 0, NULL },
		{}
	};
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	struct methods *mthd, *best;

	if (nouveau_vbios) {
		mthd = shadow_methods;
		do {
			if (strcasecmp(nouveau_vbios, mthd->desc))
				continue;
			NV_INFO(dev, "VBIOS source: %s\n", mthd->desc);

			mthd->shadow(bios);
			mthd->score = score_vbios(bios, mthd->rw);
			if (mthd->score)
				return true;
		} while ((++mthd)->shadow);

		NV_ERROR(dev, "VBIOS source \'%s\' invalid\n", nouveau_vbios);
	}

	mthd = shadow_methods;
	do {
		NV_TRACE(dev, "Checking %s for VBIOS\n", mthd->desc);
		mthd->shadow(bios);
		mthd->score = score_vbios(bios, mthd->rw);
		mthd->size = bios->length;
		mthd->data = bios->data;
	} while (mthd->score != 3 && (++mthd)->shadow);

	mthd = shadow_methods;
	best = mthd;
	do {
		if (mthd->score > best->score) {
			kfree(best->data);
			best = mthd;
		}
	} while ((++mthd)->shadow);

	if (best->score) {
		NV_TRACE(dev, "Using VBIOS from %s\n", best->desc);
		bios->length = best->size;
		bios->data = best->data;
		return true;
	}

	NV_ERROR(dev, "No valid VBIOS image found\n");
	return false;
}

struct init_tbl_entry {
	char *name;
	uint8_t id;
	int (*handler)(struct nvbios *, uint16_t, struct init_exec *);
};

static int parse_init_table(struct nvbios *, uint16_t, struct init_exec *);

#define MACRO_INDEX_SIZE	2
#define MACRO_SIZE		8
#define CONDITION_SIZE		12
#define IO_FLAG_CONDITION_SIZE	9
#define IO_CONDITION_SIZE	5
#define MEM_INIT_SIZE		66

static void still_alive(void)
{
#if 0
	sync();
	mdelay(2);
#endif
}

static uint32_t
munge_reg(struct nvbios *bios, uint32_t reg)
{
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	struct dcb_entry *dcbent = bios->display.output;

	if (dev_priv->card_type < NV_50)
		return reg;

	if (reg & 0x80000000) {
		BUG_ON(bios->display.crtc < 0);
		reg += bios->display.crtc * 0x800;
	}

	if (reg & 0x40000000) {
		BUG_ON(!dcbent);

		reg += (ffs(dcbent->or) - 1) * 0x800;
		if ((reg & 0x20000000) && !(dcbent->sorconf.link & 1))
			reg += 0x00000080;
	}

	reg &= ~0xe0000000;
	return reg;
}

static int
valid_reg(struct nvbios *bios, uint32_t reg)
{
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	struct drm_device *dev = bios->dev;

	
	if (reg & 0x2 ||
	    (reg & 0x1 && dev_priv->vbios.chip_version != 0x51))
		NV_ERROR(dev, "======= misaligned reg 0x%08X =======\n", reg);

	
	if (reg & 0x1 && dev_priv->vbios.chip_version == 0x51 &&
	    reg != 0x130d && reg != 0x1311 && reg != 0x60081d)
		NV_WARN(dev, "=== C51 misaligned reg 0x%08X not verified ===\n",
			reg);

	if (reg >= (8*1024*1024)) {
		NV_ERROR(dev, "=== reg 0x%08x out of mapped bounds ===\n", reg);
		return 0;
	}

	return 1;
}

static bool
valid_idx_port(struct nvbios *bios, uint16_t port)
{
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	struct drm_device *dev = bios->dev;

	if (dev_priv->card_type < NV_50) {
		if (port == NV_CIO_CRX__COLOR)
			return true;
		if (port == NV_VIO_SRX)
			return true;
	} else {
		if (port == NV_CIO_CRX__COLOR)
			return true;
	}

	NV_ERROR(dev, "========== unknown indexed io port 0x%04X ==========\n",
		 port);

	return false;
}

static bool
valid_port(struct nvbios *bios, uint16_t port)
{
	struct drm_device *dev = bios->dev;

	if (port == NV_VIO_VSE2)
		return true;

	NV_ERROR(dev, "========== unknown io port 0x%04X ==========\n", port);

	return false;
}

static uint32_t
bios_rd32(struct nvbios *bios, uint32_t reg)
{
	uint32_t data;

	reg = munge_reg(bios, reg);
	if (!valid_reg(bios, reg))
		return 0;

	if (reg & 0x1)
		reg &= ~0x1;

	data = nv_rd32(bios->dev, reg);

	BIOSLOG(bios, "	Read:  Reg: 0x%08X, Data: 0x%08X\n", reg, data);

	return data;
}

static void
bios_wr32(struct nvbios *bios, uint32_t reg, uint32_t data)
{
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;

	reg = munge_reg(bios, reg);
	if (!valid_reg(bios, reg))
		return;

	
	if (reg & 0x1)
		reg &= 0xfffffffe;

	LOG_OLD_VALUE(bios_rd32(bios, reg));
	BIOSLOG(bios, "	Write: Reg: 0x%08X, Data: 0x%08X\n", reg, data);

	if (dev_priv->vbios.execute) {
		still_alive();
		nv_wr32(bios->dev, reg, data);
	}
}

static uint8_t
bios_idxprt_rd(struct nvbios *bios, uint16_t port, uint8_t index)
{
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	struct drm_device *dev = bios->dev;
	uint8_t data;

	if (!valid_idx_port(bios, port))
		return 0;

	if (dev_priv->card_type < NV_50) {
		if (port == NV_VIO_SRX)
			data = NVReadVgaSeq(dev, bios->state.crtchead, index);
		else	
			data = NVReadVgaCrtc(dev, bios->state.crtchead, index);
	} else {
		uint32_t data32;

		data32 = bios_rd32(bios, NV50_PDISPLAY_VGACRTC(index & ~3));
		data = (data32 >> ((index & 3) << 3)) & 0xff;
	}

	BIOSLOG(bios, "	Indexed IO read:  Port: 0x%04X, Index: 0x%02X, "
		      "Head: 0x%02X, Data: 0x%02X\n",
		port, index, bios->state.crtchead, data);
	return data;
}

static void
bios_idxprt_wr(struct nvbios *bios, uint16_t port, uint8_t index, uint8_t data)
{
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	struct drm_device *dev = bios->dev;

	if (!valid_idx_port(bios, port))
		return;

	/*
	 * The current head is maintained in the nvbios member  state.crtchead.
	 * We trap changes to CR44 and update the head variable and hence the
	 * register set written.
	 * As CR44 only exists on CRTC0, we update crtchead to head0 in advance
	 * of the write, and to head1 after the write
	 */
	if (port == NV_CIO_CRX__COLOR && index == NV_CIO_CRE_44 &&
	    data != NV_CIO_CRE_44_HEADB)
		bios->state.crtchead = 0;

	LOG_OLD_VALUE(bios_idxprt_rd(bios, port, index));
	BIOSLOG(bios, "	Indexed IO write: Port: 0x%04X, Index: 0x%02X, "
		      "Head: 0x%02X, Data: 0x%02X\n",
		port, index, bios->state.crtchead, data);

	if (bios->execute && dev_priv->card_type < NV_50) {
		still_alive();
		if (port == NV_VIO_SRX)
			NVWriteVgaSeq(dev, bios->state.crtchead, index, data);
		else	
			NVWriteVgaCrtc(dev, bios->state.crtchead, index, data);
	} else
	if (bios->execute) {
		uint32_t data32, shift = (index & 3) << 3;

		still_alive();

		data32  = bios_rd32(bios, NV50_PDISPLAY_VGACRTC(index & ~3));
		data32 &= ~(0xff << shift);
		data32 |= (data << shift);
		bios_wr32(bios, NV50_PDISPLAY_VGACRTC(index & ~3), data32);
	}

	if (port == NV_CIO_CRX__COLOR &&
	    index == NV_CIO_CRE_44 && data == NV_CIO_CRE_44_HEADB)
		bios->state.crtchead = 1;
}

static uint8_t
bios_port_rd(struct nvbios *bios, uint16_t port)
{
	uint8_t data, head = bios->state.crtchead;

	if (!valid_port(bios, port))
		return 0;

	data = NVReadPRMVIO(bios->dev, head, NV_PRMVIO0_OFFSET + port);

	BIOSLOG(bios, "	IO read:  Port: 0x%04X, Head: 0x%02X, Data: 0x%02X\n",
		port, head, data);

	return data;
}

static void
bios_port_wr(struct nvbios *bios, uint16_t port, uint8_t data)
{
	int head = bios->state.crtchead;

	if (!valid_port(bios, port))
		return;

	LOG_OLD_VALUE(bios_port_rd(bios, port));
	BIOSLOG(bios, "	IO write: Port: 0x%04X, Head: 0x%02X, Data: 0x%02X\n",
		port, head, data);

	if (!bios->execute)
		return;

	still_alive();
	NVWritePRMVIO(bios->dev, head, NV_PRMVIO0_OFFSET + port, data);
}

static bool
io_flag_condition_met(struct nvbios *bios, uint16_t offset, uint8_t cond)
{

	uint16_t condptr = bios->io_flag_condition_tbl_ptr + cond * IO_FLAG_CONDITION_SIZE;
	uint16_t crtcport = ROM16(bios->data[condptr]);
	uint8_t crtcindex = bios->data[condptr + 2];
	uint8_t mask = bios->data[condptr + 3];
	uint8_t shift = bios->data[condptr + 4];
	uint16_t flagarray = ROM16(bios->data[condptr + 5]);
	uint8_t flagarraymask = bios->data[condptr + 7];
	uint8_t cmpval = bios->data[condptr + 8];
	uint8_t data;

	BIOSLOG(bios, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, "
		      "Shift: 0x%02X, FlagArray: 0x%04X, FAMask: 0x%02X, "
		      "Cmpval: 0x%02X\n",
		offset, crtcport, crtcindex, mask, shift, flagarray, flagarraymask, cmpval);

	data = bios_idxprt_rd(bios, crtcport, crtcindex);

	data = bios->data[flagarray + ((data & mask) >> shift)];
	data &= flagarraymask;

	BIOSLOG(bios, "0x%04X: Checking if 0x%02X equals 0x%02X\n",
		offset, data, cmpval);

	return (data == cmpval);
}

static bool
bios_condition_met(struct nvbios *bios, uint16_t offset, uint8_t cond)
{

	uint16_t condptr = bios->condition_tbl_ptr + cond * CONDITION_SIZE;
	uint32_t reg = ROM32(bios->data[condptr]);
	uint32_t mask = ROM32(bios->data[condptr + 4]);
	uint32_t cmpval = ROM32(bios->data[condptr + 8]);
	uint32_t data;

	BIOSLOG(bios, "0x%04X: Cond: 0x%02X, Reg: 0x%08X, Mask: 0x%08X\n",
		offset, cond, reg, mask);

	data = bios_rd32(bios, reg) & mask;

	BIOSLOG(bios, "0x%04X: Checking if 0x%08X equals 0x%08X\n",
		offset, data, cmpval);

	return (data == cmpval);
}

static bool
io_condition_met(struct nvbios *bios, uint16_t offset, uint8_t cond)
{

	uint16_t condptr = bios->io_condition_tbl_ptr + cond * IO_CONDITION_SIZE;
	uint16_t io_port = ROM16(bios->data[condptr]);
	uint8_t port_index = bios->data[condptr + 2];
	uint8_t mask = bios->data[condptr + 3];
	uint8_t cmpval = bios->data[condptr + 4];

	uint8_t data = bios_idxprt_rd(bios, io_port, port_index) & mask;

	BIOSLOG(bios, "0x%04X: Checking if 0x%02X equals 0x%02X\n",
		offset, data, cmpval);

	return (data == cmpval);
}

static int
nv50_pll_set(struct drm_device *dev, uint32_t reg, uint32_t clk)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_pll_vals pll;
	struct pll_lims pll_limits;
	u32 ctrl, mask, coef;
	int ret;

	ret = get_pll_limits(dev, reg, &pll_limits);
	if (ret)
		return ret;

	clk = nouveau_calc_pll_mnp(dev, &pll_limits, clk, &pll);
	if (!clk)
		return -ERANGE;

	coef = pll.N1 << 8 | pll.M1;
	ctrl = pll.log2P << 16;
	mask = 0x00070000;
	if (reg == 0x004008) {
		mask |= 0x01f80000;
		ctrl |= (pll_limits.log2p_bias << 19);
		ctrl |= (pll.log2P << 22);
	}

	if (!dev_priv->vbios.execute)
		return 0;

	nv_mask(dev, reg + 0, mask, ctrl);
	nv_wr32(dev, reg + 4, coef);
	return 0;
}

static int
setPLL(struct nvbios *bios, uint32_t reg, uint32_t clk)
{
	struct drm_device *dev = bios->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	
	struct pll_lims pll_lim;
	struct nouveau_pll_vals pllvals;
	int ret;

	if (dev_priv->card_type >= NV_50)
		return nv50_pll_set(dev, reg, clk);

	
	ret = get_pll_limits(dev, reg > 0x405c ? reg : reg - 4, &pll_lim);
	if (ret)
		return ret;

	clk = nouveau_calc_pll_mnp(dev, &pll_lim, clk, &pllvals);
	if (!clk)
		return -ERANGE;

	if (bios->execute) {
		still_alive();
		nouveau_hw_setpll(dev, reg, &pllvals);
	}

	return 0;
}

static int dcb_entry_idx_from_crtchead(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;


	uint8_t dcb_entry = NVReadVgaCrtc5758(dev, bios->state.crtchead, 0);

	if (dcb_entry > bios->dcb.entries) {
		NV_ERROR(dev, "CR58 doesn't have a valid DCB entry currently "
				"(%02X)\n", dcb_entry);
		dcb_entry = 0x7f;	
	}

	return dcb_entry;
}

static struct nouveau_i2c_chan *
init_i2c_device_find(struct drm_device *dev, int i2c_index)
{
	if (i2c_index == 0xff) {
		struct drm_nouveau_private *dev_priv = dev->dev_private;
		struct dcb_table *dcb = &dev_priv->vbios.dcb;
		
		int idx = dcb_entry_idx_from_crtchead(dev);

		i2c_index = NV_I2C_DEFAULT(0);
		if (idx != 0x7f && dcb->entry[idx].i2c_upper_default)
			i2c_index = NV_I2C_DEFAULT(1);
	}

	return nouveau_i2c_find(dev, i2c_index);
}

static uint32_t
get_tmds_index_reg(struct drm_device *dev, uint8_t mlv)
{

	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	const int pramdac_offset[13] = {
		0, 0, 0x8, 0, 0x2000, 0, 0, 0, 0x2008, 0, 0, 0, 0x2000 };
	const uint32_t pramdac_table[4] = {
		0x6808b0, 0x6808b8, 0x6828b0, 0x6828b8 };

	if (mlv >= 0x80) {
		int dcb_entry, dacoffset;

		
		dcb_entry = dcb_entry_idx_from_crtchead(dev);
		if (dcb_entry == 0x7f)
			return 0;
		dacoffset = pramdac_offset[bios->dcb.entry[dcb_entry].or];
		if (mlv == 0x81)
			dacoffset ^= 8;
		return 0x6808b0 + dacoffset;
	} else {
		if (mlv >= ARRAY_SIZE(pramdac_table)) {
			NV_ERROR(dev, "Magic Lookup Value too big (%02X)\n",
									mlv);
			return 0;
		}
		return pramdac_table[mlv];
	}
}

static int
init_io_restrict_prog(struct nvbios *bios, uint16_t offset,
		      struct init_exec *iexec)
{

	uint16_t crtcport = ROM16(bios->data[offset + 1]);
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t shift = bios->data[offset + 5];
	uint8_t count = bios->data[offset + 6];
	uint32_t reg = ROM32(bios->data[offset + 7]);
	uint8_t config;
	uint32_t configval;
	int len = 11 + count * 4;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, "
		      "Shift: 0x%02X, Count: 0x%02X, Reg: 0x%08X\n",
		offset, crtcport, crtcindex, mask, shift, count, reg);

	config = (bios_idxprt_rd(bios, crtcport, crtcindex) & mask) >> shift;
	if (config > count) {
		NV_ERROR(bios->dev,
			 "0x%04X: Config 0x%02X exceeds maximal bound 0x%02X\n",
			 offset, config, count);
		return len;
	}

	configval = ROM32(bios->data[offset + 11 + config * 4]);

	BIOSLOG(bios, "0x%04X: Writing config %02X\n", offset, config);

	bios_wr32(bios, reg, configval);

	return len;
}

static int
init_repeat(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t count = bios->data[offset + 1];
	uint8_t i;

	

	BIOSLOG(bios, "0x%04X: Repeating following segment %d times\n",
		offset, count);

	iexec->repeat = true;

	for (i = 0; i < count - 1; i++)
		parse_init_table(bios, offset + 2, iexec);

	iexec->repeat = false;

	return 2;
}

static int
init_io_restrict_pll(struct nvbios *bios, uint16_t offset,
		     struct init_exec *iexec)
{

	uint16_t crtcport = ROM16(bios->data[offset + 1]);
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t shift = bios->data[offset + 5];
	int8_t io_flag_condition_idx = bios->data[offset + 6];
	uint8_t count = bios->data[offset + 7];
	uint32_t reg = ROM32(bios->data[offset + 8]);
	uint8_t config;
	uint16_t freq;
	int len = 12 + count * 2;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, "
		      "Shift: 0x%02X, IO Flag Condition: 0x%02X, "
		      "Count: 0x%02X, Reg: 0x%08X\n",
		offset, crtcport, crtcindex, mask, shift,
		io_flag_condition_idx, count, reg);

	config = (bios_idxprt_rd(bios, crtcport, crtcindex) & mask) >> shift;
	if (config > count) {
		NV_ERROR(bios->dev,
			 "0x%04X: Config 0x%02X exceeds maximal bound 0x%02X\n",
			 offset, config, count);
		return len;
	}

	freq = ROM16(bios->data[offset + 12 + config * 2]);

	if (io_flag_condition_idx > 0) {
		if (io_flag_condition_met(bios, offset, io_flag_condition_idx)) {
			BIOSLOG(bios, "0x%04X: Condition fulfilled -- "
				      "frequency doubled\n", offset);
			freq *= 2;
		} else
			BIOSLOG(bios, "0x%04X: Condition not fulfilled -- "
				      "frequency unchanged\n", offset);
	}

	BIOSLOG(bios, "0x%04X: Reg: 0x%08X, Config: 0x%02X, Freq: %d0kHz\n",
		offset, reg, config, freq);

	setPLL(bios, reg, freq * 10);

	return len;
}

static int
init_end_repeat(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	

	if (iexec->repeat)
		return 0;

	return 1;
}

static int
init_copy(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint8_t shift = bios->data[offset + 5];
	uint8_t srcmask = bios->data[offset + 6];
	uint16_t crtcport = ROM16(bios->data[offset + 7]);
	uint8_t crtcindex = bios->data[offset + 9];
	uint8_t mask = bios->data[offset + 10];
	uint32_t data;
	uint8_t crtcdata;

	if (!iexec->execute)
		return 11;

	BIOSLOG(bios, "0x%04X: Reg: 0x%08X, Shift: 0x%02X, SrcMask: 0x%02X, "
		      "Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X\n",
		offset, reg, shift, srcmask, crtcport, crtcindex, mask);

	data = bios_rd32(bios, reg);

	if (shift < 0x80)
		data >>= shift;
	else
		data <<= (0x100 - shift);

	data &= srcmask;

	crtcdata  = bios_idxprt_rd(bios, crtcport, crtcindex) & mask;
	crtcdata |= (uint8_t)data;
	bios_idxprt_wr(bios, crtcport, crtcindex, crtcdata);

	return 11;
}

static int
init_not(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{
	if (iexec->execute)
		BIOSLOG(bios, "0x%04X: ------ Skipping following commands  ------\n", offset);
	else
		BIOSLOG(bios, "0x%04X: ------ Executing following commands ------\n", offset);

	iexec->execute = !iexec->execute;
	return 1;
}

static int
init_io_flag_condition(struct nvbios *bios, uint16_t offset,
		       struct init_exec *iexec)
{

	uint8_t cond = bios->data[offset + 1];

	if (!iexec->execute)
		return 2;

	if (io_flag_condition_met(bios, offset, cond))
		BIOSLOG(bios, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(bios, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return 2;
}

static int
init_dp_condition(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct dcb_entry *dcb = bios->display.output;
	struct drm_device *dev = bios->dev;
	uint8_t cond = bios->data[offset + 1];
	uint8_t *table, *entry;

	BIOSLOG(bios, "0x%04X: subop 0x%02X\n", offset, cond);

	if (!iexec->execute)
		return 3;

	table = nouveau_dp_bios_data(dev, dcb, &entry);
	if (!table)
		return 3;

	switch (cond) {
	case 0:
		entry = dcb_conn(dev, dcb->connector);
		if (!entry || entry[0] != DCB_CONNECTOR_eDP)
			iexec->execute = false;
		break;
	case 1:
	case 2:
		if ((table[0]  < 0x40 && !(entry[5] & cond)) ||
		    (table[0] == 0x40 && !(entry[4] & cond)))
			iexec->execute = false;
		break;
	case 5:
	{
		struct nouveau_i2c_chan *auxch;
		int ret;

		auxch = nouveau_i2c_find(dev, bios->display.output->i2c_index);
		if (!auxch) {
			NV_ERROR(dev, "0x%04X: couldn't get auxch\n", offset);
			return 3;
		}

		ret = nouveau_dp_auxch(auxch, 9, 0xd, &cond, 1);
		if (ret) {
			NV_ERROR(dev, "0x%04X: auxch rd fail: %d\n", offset, ret);
			return 3;
		}

		if (!(cond & 1))
			iexec->execute = false;
	}
		break;
	default:
		NV_WARN(dev, "0x%04X: unknown INIT_3A op: %d\n", offset, cond);
		break;
	}

	if (iexec->execute)
		BIOSLOG(bios, "0x%04X: continuing to execute\n", offset);
	else
		BIOSLOG(bios, "0x%04X: skipping following commands\n", offset);

	return 3;
}

static int
init_op_3b(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t or = ffs(bios->display.output->or) - 1;
	uint8_t index = bios->data[offset + 1];
	uint8_t data;

	if (!iexec->execute)
		return 2;

	data = bios_idxprt_rd(bios, 0x3d4, index);
	bios_idxprt_wr(bios, 0x3d4, index, data & ~(1 << or));
	return 2;
}

static int
init_op_3c(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t or = ffs(bios->display.output->or) - 1;
	uint8_t index = bios->data[offset + 1];
	uint8_t data;

	if (!iexec->execute)
		return 2;

	data = bios_idxprt_rd(bios, 0x3d4, index);
	bios_idxprt_wr(bios, 0x3d4, index, data | (1 << or));
	return 2;
}

static int
init_idx_addr_latched(struct nvbios *bios, uint16_t offset,
		      struct init_exec *iexec)
{

	uint32_t controlreg = ROM32(bios->data[offset + 1]);
	uint32_t datareg = ROM32(bios->data[offset + 5]);
	uint32_t mask = ROM32(bios->data[offset + 9]);
	uint32_t data = ROM32(bios->data[offset + 13]);
	uint8_t count = bios->data[offset + 17];
	int len = 18 + count * 2;
	uint32_t value;
	int i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: ControlReg: 0x%08X, DataReg: 0x%08X, "
		      "Mask: 0x%08X, Data: 0x%08X, Count: 0x%02X\n",
		offset, controlreg, datareg, mask, data, count);

	for (i = 0; i < count; i++) {
		uint8_t instaddress = bios->data[offset + 18 + i * 2];
		uint8_t instdata = bios->data[offset + 19 + i * 2];

		BIOSLOG(bios, "0x%04X: Address: 0x%02X, Data: 0x%02X\n",
			offset, instaddress, instdata);

		bios_wr32(bios, datareg, instdata);
		value  = bios_rd32(bios, controlreg) & mask;
		value |= data;
		value |= instaddress;
		bios_wr32(bios, controlreg, value);
	}

	return len;
}

static int
init_io_restrict_pll2(struct nvbios *bios, uint16_t offset,
		      struct init_exec *iexec)
{

	uint16_t crtcport = ROM16(bios->data[offset + 1]);
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t shift = bios->data[offset + 5];
	uint8_t count = bios->data[offset + 6];
	uint32_t reg = ROM32(bios->data[offset + 7]);
	int len = 11 + count * 4;
	uint8_t config;
	uint32_t freq;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, "
		      "Shift: 0x%02X, Count: 0x%02X, Reg: 0x%08X\n",
		offset, crtcport, crtcindex, mask, shift, count, reg);

	if (!reg)
		return len;

	config = (bios_idxprt_rd(bios, crtcport, crtcindex) & mask) >> shift;
	if (config > count) {
		NV_ERROR(bios->dev,
			 "0x%04X: Config 0x%02X exceeds maximal bound 0x%02X\n",
			 offset, config, count);
		return len;
	}

	freq = ROM32(bios->data[offset + 11 + config * 4]);

	BIOSLOG(bios, "0x%04X: Reg: 0x%08X, Config: 0x%02X, Freq: %dkHz\n",
		offset, reg, config, freq);

	setPLL(bios, reg, freq);

	return len;
}

static int
init_pll2(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint32_t freq = ROM32(bios->data[offset + 5]);

	if (!iexec->execute)
		return 9;

	BIOSLOG(bios, "0x%04X: Reg: 0x%04X, Freq: %dkHz\n",
		offset, reg, freq);

	setPLL(bios, reg, freq);
	return 9;
}

static int
init_i2c_byte(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2] >> 1;
	uint8_t count = bios->data[offset + 3];
	struct nouveau_i2c_chan *chan;
	int len = 4 + count * 3;
	int ret, i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X, "
		      "Count: 0x%02X\n",
		offset, i2c_index, i2c_address, count);

	chan = init_i2c_device_find(dev, i2c_index);
	if (!chan) {
		NV_ERROR(dev, "0x%04X: i2c bus not found\n", offset);
		return len;
	}

	for (i = 0; i < count; i++) {
		uint8_t reg = bios->data[offset + 4 + i * 3];
		uint8_t mask = bios->data[offset + 5 + i * 3];
		uint8_t data = bios->data[offset + 6 + i * 3];
		union i2c_smbus_data val;

		ret = i2c_smbus_xfer(&chan->adapter, i2c_address, 0,
				     I2C_SMBUS_READ, reg,
				     I2C_SMBUS_BYTE_DATA, &val);
		if (ret < 0) {
			NV_ERROR(dev, "0x%04X: i2c rd fail: %d\n", offset, ret);
			return len;
		}

		BIOSLOG(bios, "0x%04X: I2CReg: 0x%02X, Value: 0x%02X, "
			      "Mask: 0x%02X, Data: 0x%02X\n",
			offset, reg, val.byte, mask, data);

		if (!bios->execute)
			continue;

		val.byte &= mask;
		val.byte |= data;
		ret = i2c_smbus_xfer(&chan->adapter, i2c_address, 0,
				     I2C_SMBUS_WRITE, reg,
				     I2C_SMBUS_BYTE_DATA, &val);
		if (ret < 0) {
			NV_ERROR(dev, "0x%04X: i2c wr fail: %d\n", offset, ret);
			return len;
		}
	}

	return len;
}

static int
init_zm_i2c_byte(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2] >> 1;
	uint8_t count = bios->data[offset + 3];
	struct nouveau_i2c_chan *chan;
	int len = 4 + count * 2;
	int ret, i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X, "
		      "Count: 0x%02X\n",
		offset, i2c_index, i2c_address, count);

	chan = init_i2c_device_find(dev, i2c_index);
	if (!chan) {
		NV_ERROR(dev, "0x%04X: i2c bus not found\n", offset);
		return len;
	}

	for (i = 0; i < count; i++) {
		uint8_t reg = bios->data[offset + 4 + i * 2];
		union i2c_smbus_data val;

		val.byte = bios->data[offset + 5 + i * 2];

		BIOSLOG(bios, "0x%04X: I2CReg: 0x%02X, Data: 0x%02X\n",
			offset, reg, val.byte);

		if (!bios->execute)
			continue;

		ret = i2c_smbus_xfer(&chan->adapter, i2c_address, 0,
				     I2C_SMBUS_WRITE, reg,
				     I2C_SMBUS_BYTE_DATA, &val);
		if (ret < 0) {
			NV_ERROR(dev, "0x%04X: i2c wr fail: %d\n", offset, ret);
			return len;
		}
	}

	return len;
}

static int
init_zm_i2c(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2] >> 1;
	uint8_t count = bios->data[offset + 3];
	int len = 4 + count;
	struct nouveau_i2c_chan *chan;
	struct i2c_msg msg;
	uint8_t data[256];
	int ret, i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X, "
		      "Count: 0x%02X\n",
		offset, i2c_index, i2c_address, count);

	chan = init_i2c_device_find(dev, i2c_index);
	if (!chan) {
		NV_ERROR(dev, "0x%04X: i2c bus not found\n", offset);
		return len;
	}

	for (i = 0; i < count; i++) {
		data[i] = bios->data[offset + 4 + i];

		BIOSLOG(bios, "0x%04X: Data: 0x%02X\n", offset, data[i]);
	}

	if (bios->execute) {
		msg.addr = i2c_address;
		msg.flags = 0;
		msg.len = count;
		msg.buf = data;
		ret = i2c_transfer(&chan->adapter, &msg, 1);
		if (ret != 1) {
			NV_ERROR(dev, "0x%04X: i2c wr fail: %d\n", offset, ret);
			return len;
		}
	}

	return len;
}

static int
init_tmds(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	uint8_t mlv = bios->data[offset + 1];
	uint32_t tmdsaddr = bios->data[offset + 2];
	uint8_t mask = bios->data[offset + 3];
	uint8_t data = bios->data[offset + 4];
	uint32_t reg, value;

	if (!iexec->execute)
		return 5;

	BIOSLOG(bios, "0x%04X: MagicLookupValue: 0x%02X, TMDSAddr: 0x%02X, "
		      "Mask: 0x%02X, Data: 0x%02X\n",
		offset, mlv, tmdsaddr, mask, data);

	reg = get_tmds_index_reg(bios->dev, mlv);
	if (!reg) {
		NV_ERROR(dev, "0x%04X: no tmds_index_reg\n", offset);
		return 5;
	}

	bios_wr32(bios, reg,
		  tmdsaddr | NV_PRAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);
	value = (bios_rd32(bios, reg + 4) & mask) | data;
	bios_wr32(bios, reg + 4, value);
	bios_wr32(bios, reg, tmdsaddr);

	return 5;
}

static int
init_zm_tmds_group(struct nvbios *bios, uint16_t offset,
		   struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	uint8_t mlv = bios->data[offset + 1];
	uint8_t count = bios->data[offset + 2];
	int len = 3 + count * 2;
	uint32_t reg;
	int i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: MagicLookupValue: 0x%02X, Count: 0x%02X\n",
		offset, mlv, count);

	reg = get_tmds_index_reg(bios->dev, mlv);
	if (!reg) {
		NV_ERROR(dev, "0x%04X: no tmds_index_reg\n", offset);
		return len;
	}

	for (i = 0; i < count; i++) {
		uint8_t tmdsaddr = bios->data[offset + 3 + i * 2];
		uint8_t tmdsdata = bios->data[offset + 4 + i * 2];

		bios_wr32(bios, reg + 4, tmdsdata);
		bios_wr32(bios, reg, tmdsaddr);
	}

	return len;
}

static int
init_cr_idx_adr_latch(struct nvbios *bios, uint16_t offset,
		      struct init_exec *iexec)
{
	uint8_t crtcindex1 = bios->data[offset + 1];
	uint8_t crtcindex2 = bios->data[offset + 2];
	uint8_t baseaddr = bios->data[offset + 3];
	uint8_t count = bios->data[offset + 4];
	int len = 5 + count;
	uint8_t oldaddr, data;
	int i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: Index1: 0x%02X, Index2: 0x%02X, "
		      "BaseAddr: 0x%02X, Count: 0x%02X\n",
		offset, crtcindex1, crtcindex2, baseaddr, count);

	oldaddr = bios_idxprt_rd(bios, NV_CIO_CRX__COLOR, crtcindex1);

	for (i = 0; i < count; i++) {
		bios_idxprt_wr(bios, NV_CIO_CRX__COLOR, crtcindex1,
				     baseaddr + i);
		data = bios->data[offset + 5 + i];
		bios_idxprt_wr(bios, NV_CIO_CRX__COLOR, crtcindex2, data);
	}

	bios_idxprt_wr(bios, NV_CIO_CRX__COLOR, crtcindex1, oldaddr);

	return len;
}

static int
init_cr(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t crtcindex = bios->data[offset + 1];
	uint8_t mask = bios->data[offset + 2];
	uint8_t data = bios->data[offset + 3];
	uint8_t value;

	if (!iexec->execute)
		return 4;

	BIOSLOG(bios, "0x%04X: Index: 0x%02X, Mask: 0x%02X, Data: 0x%02X\n",
		offset, crtcindex, mask, data);

	value  = bios_idxprt_rd(bios, NV_CIO_CRX__COLOR, crtcindex) & mask;
	value |= data;
	bios_idxprt_wr(bios, NV_CIO_CRX__COLOR, crtcindex, value);

	return 4;
}

static int
init_zm_cr(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t crtcindex = ROM32(bios->data[offset + 1]);
	uint8_t data = bios->data[offset + 2];

	if (!iexec->execute)
		return 3;

	bios_idxprt_wr(bios, NV_CIO_CRX__COLOR, crtcindex, data);

	return 3;
}

static int
init_zm_cr_group(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t count = bios->data[offset + 1];
	int len = 2 + count * 2;
	int i;

	if (!iexec->execute)
		return len;

	for (i = 0; i < count; i++)
		init_zm_cr(bios, offset + 2 + 2 * i - 1, iexec);

	return len;
}

static int
init_condition_time(struct nvbios *bios, uint16_t offset,
		    struct init_exec *iexec)
{

	uint8_t cond = bios->data[offset + 1];
	uint16_t retries = bios->data[offset + 2] * 50;
	unsigned cnt;

	if (!iexec->execute)
		return 3;

	if (retries > 100)
		retries = 100;

	BIOSLOG(bios, "0x%04X: Condition: 0x%02X, Retries: 0x%02X\n",
		offset, cond, retries);

	if (!bios->execute) 
		retries = 1;

	for (cnt = 0; cnt < retries; cnt++) {
		if (bios_condition_met(bios, offset, cond)) {
			BIOSLOG(bios, "0x%04X: Condition met, continuing\n",
								offset);
			break;
		} else {
			BIOSLOG(bios, "0x%04X: "
				"Condition not met, sleeping for 20ms\n",
								offset);
			mdelay(20);
		}
	}

	if (!bios_condition_met(bios, offset, cond)) {
		NV_WARN(bios->dev,
			"0x%04X: Condition still not met after %dms, "
			"skipping following opcodes\n", offset, 20 * retries);
		iexec->execute = false;
	}

	return 3;
}

static int
init_ltime(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	unsigned time = ROM16(bios->data[offset + 1]);

	if (!iexec->execute)
		return 3;

	BIOSLOG(bios, "0x%04X: Sleeping for 0x%04X milliseconds\n",
		offset, time);

	mdelay(time);

	return 3;
}

static int
init_zm_reg_sequence(struct nvbios *bios, uint16_t offset,
		     struct init_exec *iexec)
{

	uint32_t basereg = ROM32(bios->data[offset + 1]);
	uint32_t count = bios->data[offset + 5];
	int len = 6 + count * 4;
	int i;

	if (!iexec->execute)
		return len;

	BIOSLOG(bios, "0x%04X: BaseReg: 0x%08X, Count: 0x%02X\n",
		offset, basereg, count);

	for (i = 0; i < count; i++) {
		uint32_t reg = basereg + i * 4;
		uint32_t data = ROM32(bios->data[offset + 6 + i * 4]);

		bios_wr32(bios, reg, data);
	}

	return len;
}

static int
init_sub_direct(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint16_t sub_offset = ROM16(bios->data[offset + 1]);

	if (!iexec->execute)
		return 3;

	BIOSLOG(bios, "0x%04X: Executing subroutine at 0x%04X\n",
		offset, sub_offset);

	parse_init_table(bios, sub_offset, iexec);

	BIOSLOG(bios, "0x%04X: End of 0x%04X subroutine\n", offset, sub_offset);

	return 3;
}

static int
init_jump(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint16_t jmp_offset = ROM16(bios->data[offset + 1]);

	if (!iexec->execute)
		return 3;

	BIOSLOG(bios, "0x%04X: Jump to 0x%04X\n", offset, jmp_offset);
	return jmp_offset - offset;
}

static int
init_i2c_if(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2] >> 1;
	uint8_t reg = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t data = bios->data[offset + 5];
	struct nouveau_i2c_chan *chan;
	union i2c_smbus_data val;
	int ret;

	

	BIOSLOG(bios, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X\n",
		offset, i2c_index, i2c_address);

	chan = init_i2c_device_find(bios->dev, i2c_index);
	if (!chan)
		return -ENODEV;

	ret = i2c_smbus_xfer(&chan->adapter, i2c_address, 0,
			     I2C_SMBUS_READ, reg,
			     I2C_SMBUS_BYTE_DATA, &val);
	if (ret < 0) {
		BIOSLOG(bios, "0x%04X: I2CReg: 0x%02X, Value: [no device], "
			      "Mask: 0x%02X, Data: 0x%02X\n",
			offset, reg, mask, data);
		iexec->execute = 0;
		return 6;
	}

	BIOSLOG(bios, "0x%04X: I2CReg: 0x%02X, Value: 0x%02X, "
		      "Mask: 0x%02X, Data: 0x%02X\n",
		offset, reg, val.byte, mask, data);

	iexec->execute = ((val.byte & mask) == data);

	return 6;
}

static int
init_copy_nv_reg(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t srcreg = *((uint32_t *)(&bios->data[offset + 1]));
	uint8_t shift = bios->data[offset + 5];
	uint32_t srcmask = *((uint32_t *)(&bios->data[offset + 6]));
	uint32_t xor = *((uint32_t *)(&bios->data[offset + 10]));
	uint32_t dstreg = *((uint32_t *)(&bios->data[offset + 14]));
	uint32_t dstmask = *((uint32_t *)(&bios->data[offset + 18]));
	uint32_t srcvalue, dstvalue;

	if (!iexec->execute)
		return 22;

	BIOSLOG(bios, "0x%04X: SrcReg: 0x%08X, Shift: 0x%02X, SrcMask: 0x%08X, "
		      "Xor: 0x%08X, DstReg: 0x%08X, DstMask: 0x%08X\n",
		offset, srcreg, shift, srcmask, xor, dstreg, dstmask);

	srcvalue = bios_rd32(bios, srcreg);

	if (shift < 0x80)
		srcvalue >>= shift;
	else
		srcvalue <<= (0x100 - shift);

	srcvalue = (srcvalue & srcmask) ^ xor;

	dstvalue = bios_rd32(bios, dstreg) & dstmask;

	bios_wr32(bios, dstreg, dstvalue | srcvalue);

	return 22;
}

static int
init_zm_index_io(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{
	uint16_t crtcport = ROM16(bios->data[offset + 1]);
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t data = bios->data[offset + 4];

	if (!iexec->execute)
		return 5;

	bios_idxprt_wr(bios, crtcport, crtcindex, data);

	return 5;
}

static inline void
bios_md32(struct nvbios *bios, uint32_t reg,
	  uint32_t mask, uint32_t val)
{
	bios_wr32(bios, reg, (bios_rd32(bios, reg) & ~mask) | val);
}

static uint32_t
peek_fb(struct drm_device *dev, struct io_mapping *fb,
	uint32_t off)
{
	uint32_t val = 0;

	if (off < pci_resource_len(dev->pdev, 1)) {
		uint8_t __iomem *p =
			io_mapping_map_atomic_wc(fb, off & PAGE_MASK);

		val = ioread32(p + (off & ~PAGE_MASK));

		io_mapping_unmap_atomic(p);
	}

	return val;
}

static void
poke_fb(struct drm_device *dev, struct io_mapping *fb,
	uint32_t off, uint32_t val)
{
	if (off < pci_resource_len(dev->pdev, 1)) {
		uint8_t __iomem *p =
			io_mapping_map_atomic_wc(fb, off & PAGE_MASK);

		iowrite32(val, p + (off & ~PAGE_MASK));
		wmb();

		io_mapping_unmap_atomic(p);
	}
}

static inline bool
read_back_fb(struct drm_device *dev, struct io_mapping *fb,
	     uint32_t off, uint32_t val)
{
	poke_fb(dev, fb, off, val);
	return val == peek_fb(dev, fb, off);
}

static int
nv04_init_compute_mem(struct nvbios *bios)
{
	struct drm_device *dev = bios->dev;
	uint32_t patt = 0xdeadbeef;
	struct io_mapping *fb;
	int i;

	
	fb = io_mapping_create_wc(pci_resource_start(dev->pdev, 1),
				  pci_resource_len(dev->pdev, 1));
	if (!fb)
		return -ENOMEM;

	
	NVWriteVgaSeq(dev, 0, 1, NVReadVgaSeq(dev, 0, 1) | 0x20);
	bios_md32(bios, NV04_PFB_DEBUG_0, 0, NV04_PFB_DEBUG_0_REFRESH_OFF);

	bios_md32(bios, NV04_PFB_BOOT_0, ~0,
		  NV04_PFB_BOOT_0_RAM_AMOUNT_16MB |
		  NV04_PFB_BOOT_0_RAM_WIDTH_128 |
		  NV04_PFB_BOOT_0_RAM_TYPE_SGRAM_16MBIT);

	for (i = 0; i < 4; i++)
		poke_fb(dev, fb, 4 * i, patt);

	poke_fb(dev, fb, 0x400000, patt + 1);

	if (peek_fb(dev, fb, 0) == patt + 1) {
		bios_md32(bios, NV04_PFB_BOOT_0, NV04_PFB_BOOT_0_RAM_TYPE,
			  NV04_PFB_BOOT_0_RAM_TYPE_SDRAM_16MBIT);
		bios_md32(bios, NV04_PFB_DEBUG_0,
			  NV04_PFB_DEBUG_0_REFRESH_OFF, 0);

		for (i = 0; i < 4; i++)
			poke_fb(dev, fb, 4 * i, patt);

		if ((peek_fb(dev, fb, 0xc) & 0xffff) != (patt & 0xffff))
			bios_md32(bios, NV04_PFB_BOOT_0,
				  NV04_PFB_BOOT_0_RAM_WIDTH_128 |
				  NV04_PFB_BOOT_0_RAM_AMOUNT,
				  NV04_PFB_BOOT_0_RAM_AMOUNT_8MB);

	} else if ((peek_fb(dev, fb, 0xc) & 0xffff0000) !=
		   (patt & 0xffff0000)) {
		bios_md32(bios, NV04_PFB_BOOT_0,
			  NV04_PFB_BOOT_0_RAM_WIDTH_128 |
			  NV04_PFB_BOOT_0_RAM_AMOUNT,
			  NV04_PFB_BOOT_0_RAM_AMOUNT_4MB);

	} else if (peek_fb(dev, fb, 0) != patt) {
		if (read_back_fb(dev, fb, 0x800000, patt))
			bios_md32(bios, NV04_PFB_BOOT_0,
				  NV04_PFB_BOOT_0_RAM_AMOUNT,
				  NV04_PFB_BOOT_0_RAM_AMOUNT_8MB);
		else
			bios_md32(bios, NV04_PFB_BOOT_0,
				  NV04_PFB_BOOT_0_RAM_AMOUNT,
				  NV04_PFB_BOOT_0_RAM_AMOUNT_4MB);

		bios_md32(bios, NV04_PFB_BOOT_0, NV04_PFB_BOOT_0_RAM_TYPE,
			  NV04_PFB_BOOT_0_RAM_TYPE_SGRAM_8MBIT);

	} else if (!read_back_fb(dev, fb, 0x800000, patt)) {
		bios_md32(bios, NV04_PFB_BOOT_0, NV04_PFB_BOOT_0_RAM_AMOUNT,
			  NV04_PFB_BOOT_0_RAM_AMOUNT_8MB);

	}

	
	bios_md32(bios, NV04_PFB_DEBUG_0, NV04_PFB_DEBUG_0_REFRESH_OFF, 0);
	NVWriteVgaSeq(dev, 0, 1, NVReadVgaSeq(dev, 0, 1) & ~0x20);

	io_mapping_free(fb);
	return 0;
}

static const uint8_t *
nv05_memory_config(struct nvbios *bios)
{
	
	static const uint8_t default_config_tab[][2] = {
		{ 0x24, 0x00 },
		{ 0x28, 0x00 },
		{ 0x24, 0x01 },
		{ 0x1f, 0x00 },
		{ 0x0f, 0x00 },
		{ 0x17, 0x00 },
		{ 0x06, 0x00 },
		{ 0x00, 0x00 }
	};
	int i = (bios_rd32(bios, NV_PEXTDEV_BOOT_0) &
		 NV_PEXTDEV_BOOT_0_RAMCFG) >> 2;

	if (bios->legacy.mem_init_tbl_ptr)
		return &bios->data[bios->legacy.mem_init_tbl_ptr + 2 * i];
	else
		return default_config_tab[i];
}

static int
nv05_init_compute_mem(struct nvbios *bios)
{
	struct drm_device *dev = bios->dev;
	const uint8_t *ramcfg = nv05_memory_config(bios);
	uint32_t patt = 0xdeadbeef;
	struct io_mapping *fb;
	int i, v;

	
	fb = io_mapping_create_wc(pci_resource_start(dev->pdev, 1),
				  pci_resource_len(dev->pdev, 1));
	if (!fb)
		return -ENOMEM;

	
	NVWriteVgaSeq(dev, 0, 1, NVReadVgaSeq(dev, 0, 1) | 0x20);

	if (bios_rd32(bios, NV04_PFB_BOOT_0) & NV04_PFB_BOOT_0_UMA_ENABLE)
		goto out;

	bios_md32(bios, NV04_PFB_DEBUG_0, NV04_PFB_DEBUG_0_REFRESH_OFF, 0);

	
	if (bios->legacy.mem_init_tbl_ptr) {
		uint32_t *scramble_tab = (uint32_t *)&bios->data[
			bios->legacy.mem_init_tbl_ptr + 0x10];

		for (i = 0; i < 8; i++)
			bios_wr32(bios, NV04_PFB_SCRAMBLE(i),
				  ROM32(scramble_tab[i]));
	}

	
	bios_md32(bios, NV04_PFB_BOOT_0, 0x3f, ramcfg[0]);

	if (ramcfg[1] & 0x80)
		bios_md32(bios, NV04_PFB_CFG0, 0, NV04_PFB_CFG0_SCRAMBLE);

	bios_md32(bios, NV04_PFB_CFG1, 0x700001, (ramcfg[1] & 1) << 20);
	bios_md32(bios, NV04_PFB_CFG1, 0, 1);

	
	for (i = 0; i < 4; i++)
		poke_fb(dev, fb, 4 * i, patt);

	if (peek_fb(dev, fb, 0xc) != patt)
		bios_md32(bios, NV04_PFB_BOOT_0,
			  NV04_PFB_BOOT_0_RAM_WIDTH_128, 0);

	
	v = bios_rd32(bios, NV04_PFB_BOOT_0) & NV04_PFB_BOOT_0_RAM_AMOUNT;

	if (v == NV04_PFB_BOOT_0_RAM_AMOUNT_32MB &&
	    (!read_back_fb(dev, fb, 0x1000000, ++patt) ||
	     !read_back_fb(dev, fb, 0, ++patt)))
		bios_md32(bios, NV04_PFB_BOOT_0, NV04_PFB_BOOT_0_RAM_AMOUNT,
			  NV04_PFB_BOOT_0_RAM_AMOUNT_16MB);

	if (v == NV04_PFB_BOOT_0_RAM_AMOUNT_16MB &&
	    !read_back_fb(dev, fb, 0x800000, ++patt))
		bios_md32(bios, NV04_PFB_BOOT_0, NV04_PFB_BOOT_0_RAM_AMOUNT,
			  NV04_PFB_BOOT_0_RAM_AMOUNT_8MB);

	if (!read_back_fb(dev, fb, 0x400000, ++patt))
		bios_md32(bios, NV04_PFB_BOOT_0, NV04_PFB_BOOT_0_RAM_AMOUNT,
			  NV04_PFB_BOOT_0_RAM_AMOUNT_4MB);

out:
	
	NVWriteVgaSeq(dev, 0, 1, NVReadVgaSeq(dev, 0, 1) & ~0x20);

	io_mapping_free(fb);
	return 0;
}

static int
nv10_init_compute_mem(struct nvbios *bios)
{
	struct drm_device *dev = bios->dev;
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	const int mem_width[] = { 0x10, 0x00, 0x20 };
	const int mem_width_count = (dev_priv->chipset >= 0x17 ? 3 : 2);
	uint32_t patt = 0xdeadbeef;
	struct io_mapping *fb;
	int i, j, k;

	
	fb = io_mapping_create_wc(pci_resource_start(dev->pdev, 1),
				  pci_resource_len(dev->pdev, 1));
	if (!fb)
		return -ENOMEM;

	bios_wr32(bios, NV10_PFB_REFCTRL, NV10_PFB_REFCTRL_VALID_1);

	
	for (i = 0; i < mem_width_count; i++) {
		bios_md32(bios, NV04_PFB_CFG0, 0x30, mem_width[i]);

		for (j = 0; j < 4; j++) {
			for (k = 0; k < 4; k++)
				poke_fb(dev, fb, 0x1c, 0);

			poke_fb(dev, fb, 0x1c, patt);
			poke_fb(dev, fb, 0x3c, 0);

			if (peek_fb(dev, fb, 0x1c) == patt)
				goto mem_width_found;
		}
	}

mem_width_found:
	patt <<= 1;

	
	for (i = 0; i < 4; i++) {
		int off = bios_rd32(bios, NV04_PFB_FIFO_DATA) - 0x100000;

		poke_fb(dev, fb, off, patt);
		poke_fb(dev, fb, 0, 0);

		peek_fb(dev, fb, 0);
		peek_fb(dev, fb, 0);
		peek_fb(dev, fb, 0);
		peek_fb(dev, fb, 0);

		if (peek_fb(dev, fb, off) == patt)
			goto amount_found;
	}

	
	bios_md32(bios, NV04_PFB_CFG0, 0x1000, 0);

amount_found:
	io_mapping_free(fb);
	return 0;
}

static int
nv20_init_compute_mem(struct nvbios *bios)
{
	struct drm_device *dev = bios->dev;
	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	uint32_t mask = (dev_priv->chipset >= 0x25 ? 0x300 : 0x900);
	uint32_t amount, off;
	struct io_mapping *fb;

	
	fb = io_mapping_create_wc(pci_resource_start(dev->pdev, 1),
				  pci_resource_len(dev->pdev, 1));
	if (!fb)
		return -ENOMEM;

	bios_wr32(bios, NV10_PFB_REFCTRL, NV10_PFB_REFCTRL_VALID_1);

	
	bios_md32(bios, NV04_PFB_CFG0, 0, mask);

	amount = bios_rd32(bios, NV04_PFB_FIFO_DATA);
	for (off = amount; off > 0x2000000; off -= 0x2000000)
		poke_fb(dev, fb, off - 4, off);

	amount = bios_rd32(bios, NV04_PFB_FIFO_DATA);
	if (amount != peek_fb(dev, fb, amount - 4))
		
		bios_md32(bios, NV04_PFB_CFG0, mask, 0);

	io_mapping_free(fb);
	return 0;
}

static int
init_compute_mem(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{
	/*
	 * INIT_COMPUTE_MEM   opcode: 0x63 ('c')
	 *
	 * offset      (8 bit): opcode
	 *
	 * This opcode is meant to set the PFB memory config registers
	 * appropriately so that we can correctly calculate how much VRAM it
	 * has (on nv10 and better chipsets the amount of installed VRAM is
	 * subsequently reported in NV_PFB_CSTATUS (0x10020C)).
	 *
	 * The implementation of this opcode in general consists of several
	 * parts:
	 *
	 * 1) Determination of memory type and density. Only necessary for
	 *    really old chipsets, the memory type reported by the strap bits
	 *    (0x101000) is assumed to be accurate on nv05 and newer.
	 *
	 * 2) Determination of the memory bus width. Usually done by a cunning
	 *    combination of writes to offsets 0x1c and 0x3c in the fb, and
	 *    seeing whether the written values are read back correctly.
	 *
	 *    Only necessary on nv0x-nv1x and nv34, on the other cards we can
	 *    trust the straps.
	 *
	 * 3) Determination of how many of the card's RAM pads have ICs
	 *    attached, usually done by a cunning combination of writes to an
	 *    offset slightly less than the maximum memory reported by
	 *    NV_PFB_CSTATUS, then seeing if the test pattern can be read back.
	 *
	 * This appears to be a NOP on IGPs and NV4x or newer chipsets, both io
	 * logs of the VBIOS and kmmio traces of the binary driver POSTing the
	 * card show nothing being done for this opcode. Why is it still listed
	 * in the table?!
	 */

	

	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	int ret;

	if (dev_priv->chipset >= 0x40 ||
	    dev_priv->chipset == 0x1a ||
	    dev_priv->chipset == 0x1f)
		ret = 0;
	else if (dev_priv->chipset >= 0x20 &&
		 dev_priv->chipset != 0x34)
		ret = nv20_init_compute_mem(bios);
	else if (dev_priv->chipset >= 0x10)
		ret = nv10_init_compute_mem(bios);
	else if (dev_priv->chipset >= 0x5)
		ret = nv05_init_compute_mem(bios);
	else
		ret = nv04_init_compute_mem(bios);

	if (ret)
		return ret;

	return 1;
}

static int
init_reset(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint32_t value1 = ROM32(bios->data[offset + 5]);
	uint32_t value2 = ROM32(bios->data[offset + 9]);
	uint32_t pci_nv_19, pci_nv_20;

	

	pci_nv_19 = bios_rd32(bios, NV_PBUS_PCI_NV_19);
	bios_wr32(bios, NV_PBUS_PCI_NV_19, pci_nv_19 & ~0xf00);

	bios_wr32(bios, reg, value1);

	udelay(10);

	bios_wr32(bios, reg, value2);
	bios_wr32(bios, NV_PBUS_PCI_NV_19, pci_nv_19);

	pci_nv_20 = bios_rd32(bios, NV_PBUS_PCI_NV_20);
	pci_nv_20 &= ~NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED;	
	bios_wr32(bios, NV_PBUS_PCI_NV_20, pci_nv_20);

	return 13;
}

static int
init_configure_mem(struct nvbios *bios, uint16_t offset,
		   struct init_exec *iexec)
{

	

	uint16_t meminitoffs = bios->legacy.mem_init_tbl_ptr + MEM_INIT_SIZE * (bios_idxprt_rd(bios, NV_CIO_CRX__COLOR, NV_CIO_CRE_SCRATCH4__INDEX) >> 4);
	uint16_t seqtbloffs = bios->legacy.sdr_seq_tbl_ptr, meminitdata = meminitoffs + 6;
	uint32_t reg, data;

	if (bios->major_version > 2)
		return 0;

	bios_idxprt_wr(bios, NV_VIO_SRX, NV_VIO_SR_CLOCK_INDEX, bios_idxprt_rd(
		       bios, NV_VIO_SRX, NV_VIO_SR_CLOCK_INDEX) | 0x20);

	if (bios->data[meminitoffs] & 1)
		seqtbloffs = bios->legacy.ddr_seq_tbl_ptr;

	for (reg = ROM32(bios->data[seqtbloffs]);
	     reg != 0xffffffff;
	     reg = ROM32(bios->data[seqtbloffs += 4])) {

		switch (reg) {
		case NV04_PFB_PRE:
			data = NV04_PFB_PRE_CMD_PRECHARGE;
			break;
		case NV04_PFB_PAD:
			data = NV04_PFB_PAD_CKE_NORMAL;
			break;
		case NV04_PFB_REF:
			data = NV04_PFB_REF_CMD_REFRESH;
			break;
		default:
			data = ROM32(bios->data[meminitdata]);
			meminitdata += 4;
			if (data == 0xffffffff)
				continue;
		}

		bios_wr32(bios, reg, data);
	}

	return 1;
}

static int
init_configure_clk(struct nvbios *bios, uint16_t offset,
		   struct init_exec *iexec)
{

	

	uint16_t meminitoffs = bios->legacy.mem_init_tbl_ptr + MEM_INIT_SIZE * (bios_idxprt_rd(bios, NV_CIO_CRX__COLOR, NV_CIO_CRE_SCRATCH4__INDEX) >> 4);
	int clock;

	if (bios->major_version > 2)
		return 0;

	clock = ROM16(bios->data[meminitoffs + 4]) * 10;
	setPLL(bios, NV_PRAMDAC_NVPLL_COEFF, clock);

	clock = ROM16(bios->data[meminitoffs + 2]) * 10;
	if (bios->data[meminitoffs] & 1) 
		clock *= 2;
	setPLL(bios, NV_PRAMDAC_MPLL_COEFF, clock);

	return 1;
}

static int
init_configure_preinit(struct nvbios *bios, uint16_t offset,
		       struct init_exec *iexec)
{

	

	uint32_t straps = bios_rd32(bios, NV_PEXTDEV_BOOT_0);
	uint8_t cr3c = ((straps << 2) & 0xf0) | (straps & 0x40) >> 6;

	if (bios->major_version > 2)
		return 0;

	bios_idxprt_wr(bios, NV_CIO_CRX__COLOR,
			     NV_CIO_CRE_SCRATCH4__INDEX, cr3c);

	return 1;
}

static int
init_io(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_nouveau_private *dev_priv = bios->dev->dev_private;
	uint16_t crtcport = ROM16(bios->data[offset + 1]);
	uint8_t mask = bios->data[offset + 3];
	uint8_t data = bios->data[offset + 4];

	if (!iexec->execute)
		return 5;

	BIOSLOG(bios, "0x%04X: Port: 0x%04X, Mask: 0x%02X, Data: 0x%02X\n",
		offset, crtcport, mask, data);

	if (dev_priv->card_type >= NV_50 && crtcport == 0x3c3 && data == 1) {
		int i;

		bios_wr32(bios, 0x614100, (bios_rd32(
			  bios, 0x614100) & 0x0fffffff) | 0x00800000);

		bios_wr32(bios, 0x00e18c, bios_rd32(
			  bios, 0x00e18c) | 0x00020000);

		bios_wr32(bios, 0x614900, (bios_rd32(
			  bios, 0x614900) & 0x0fffffff) | 0x00800000);

		bios_wr32(bios, 0x000200, bios_rd32(
			  bios, 0x000200) & ~0x40000000);

		mdelay(10);

		bios_wr32(bios, 0x00e18c, bios_rd32(
			  bios, 0x00e18c) & ~0x00020000);

		bios_wr32(bios, 0x000200, bios_rd32(
			  bios, 0x000200) | 0x40000000);

		bios_wr32(bios, 0x614100, 0x00800018);
		bios_wr32(bios, 0x614900, 0x00800018);

		mdelay(10);

		bios_wr32(bios, 0x614100, 0x10000018);
		bios_wr32(bios, 0x614900, 0x10000018);

		for (i = 0; i < 3; i++)
			bios_wr32(bios, 0x614280 + (i*0x800), bios_rd32(
				  bios, 0x614280 + (i*0x800)) & 0xf0f0f0f0);

		for (i = 0; i < 2; i++)
			bios_wr32(bios, 0x614300 + (i*0x800), bios_rd32(
				  bios, 0x614300 + (i*0x800)) & 0xfffff0f0);

		for (i = 0; i < 3; i++)
			bios_wr32(bios, 0x614380 + (i*0x800), bios_rd32(
				  bios, 0x614380 + (i*0x800)) & 0xfffff0f0);

		for (i = 0; i < 2; i++)
			bios_wr32(bios, 0x614200 + (i*0x800), bios_rd32(
				  bios, 0x614200 + (i*0x800)) & 0xfffffff0);

		for (i = 0; i < 2; i++)
			bios_wr32(bios, 0x614108 + (i*0x800), bios_rd32(
				  bios, 0x614108 + (i*0x800)) & 0x0fffffff);
		return 5;
	}

	bios_port_wr(bios, crtcport, (bios_port_rd(bios, crtcport) & mask) |
									data);
	return 5;
}

static int
init_sub(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t sub = bios->data[offset + 1];

	if (!iexec->execute)
		return 2;

	BIOSLOG(bios, "0x%04X: Calling script %d\n", offset, sub);

	parse_init_table(bios,
			 ROM16(bios->data[bios->init_script_tbls_ptr + sub * 2]),
			 iexec);

	BIOSLOG(bios, "0x%04X: End of script %d\n", offset, sub);

	return 2;
}

static int
init_ram_condition(struct nvbios *bios, uint16_t offset,
		   struct init_exec *iexec)
{

	uint8_t mask = bios->data[offset + 1];
	uint8_t cmpval = bios->data[offset + 2];
	uint8_t data;

	if (!iexec->execute)
		return 3;

	data = bios_rd32(bios, NV04_PFB_BOOT_0) & mask;

	BIOSLOG(bios, "0x%04X: Checking if 0x%08X equals 0x%08X\n",
		offset, data, cmpval);

	if (data == cmpval)
		BIOSLOG(bios, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(bios, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return 3;
}

static int
init_nv_reg(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint32_t mask = ROM32(bios->data[offset + 5]);
	uint32_t data = ROM32(bios->data[offset + 9]);

	if (!iexec->execute)
		return 13;

	BIOSLOG(bios, "0x%04X: Reg: 0x%08X, Mask: 0x%08X, Data: 0x%08X\n",
		offset, reg, mask, data);

	bios_wr32(bios, reg, (bios_rd32(bios, reg) & mask) | data);

	return 13;
}

static int
init_macro(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t macro_index_tbl_idx = bios->data[offset + 1];
	uint16_t tmp = bios->macro_index_tbl_ptr + (macro_index_tbl_idx * MACRO_INDEX_SIZE);
	uint8_t macro_tbl_idx = bios->data[tmp];
	uint8_t count = bios->data[tmp + 1];
	uint32_t reg, data;
	int i;

	if (!iexec->execute)
		return 2;

	BIOSLOG(bios, "0x%04X: Macro: 0x%02X, MacroTableIndex: 0x%02X, "
		      "Count: 0x%02X\n",
		offset, macro_index_tbl_idx, macro_tbl_idx, count);

	for (i = 0; i < count; i++) {
		uint16_t macroentryptr = bios->macro_tbl_ptr + (macro_tbl_idx + i) * MACRO_SIZE;

		reg = ROM32(bios->data[macroentryptr]);
		data = ROM32(bios->data[macroentryptr + 4]);

		bios_wr32(bios, reg, data);
	}

	return 2;
}

static int
init_done(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	
	return 0;
}

static int
init_resume(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	if (iexec->execute)
		return 1;

	iexec->execute = true;
	BIOSLOG(bios, "0x%04X: ---- Executing following commands ----\n", offset);

	return 1;
}

static int
init_time(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	unsigned time = ROM16(bios->data[offset + 1]);

	if (!iexec->execute)
		return 3;

	BIOSLOG(bios, "0x%04X: Sleeping for 0x%04X microseconds\n",
		offset, time);

	if (time < 1000)
		udelay(time);
	else
		mdelay((time + 900) / 1000);

	return 3;
}

static int
init_condition(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t cond = bios->data[offset + 1];

	if (!iexec->execute)
		return 2;

	BIOSLOG(bios, "0x%04X: Condition: 0x%02X\n", offset, cond);

	if (bios_condition_met(bios, offset, cond))
		BIOSLOG(bios, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(bios, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return 2;
}

static int
init_io_condition(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t cond = bios->data[offset + 1];

	if (!iexec->execute)
		return 2;

	BIOSLOG(bios, "0x%04X: IO condition: 0x%02X\n", offset, cond);

	if (io_condition_met(bios, offset, cond))
		BIOSLOG(bios, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(bios, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return 2;
}

static int
init_index_io(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint16_t crtcport = ROM16(bios->data[offset + 1]);
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t data = bios->data[offset + 5];
	uint8_t value;

	if (!iexec->execute)
		return 6;

	BIOSLOG(bios, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, "
		      "Data: 0x%02X\n",
		offset, crtcport, crtcindex, mask, data);

	value = (bios_idxprt_rd(bios, crtcport, crtcindex) & mask) | data;
	bios_idxprt_wr(bios, crtcport, crtcindex, value);

	return 6;
}

static int
init_pll(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint16_t freq = ROM16(bios->data[offset + 5]);

	if (!iexec->execute)
		return 7;

	BIOSLOG(bios, "0x%04X: Reg: 0x%08X, Freq: %d0kHz\n", offset, reg, freq);

	setPLL(bios, reg, freq * 10);

	return 7;
}

static int
init_zm_reg(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint32_t value = ROM32(bios->data[offset + 5]);

	if (!iexec->execute)
		return 9;

	if (reg == 0x000200)
		value |= 1;

	bios_wr32(bios, reg, value);

	return 9;
}

static int
init_ram_restrict_pll(struct nvbios *bios, uint16_t offset,
		      struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	uint32_t strap = (bios_rd32(bios, NV_PEXTDEV_BOOT_0) & 0x0000003c) >> 2;
	uint8_t index = bios->data[bios->ram_restrict_tbl_ptr + strap];
	uint8_t type = bios->data[offset + 1];
	uint32_t freq = ROM32(bios->data[offset + 2 + (index * 4)]);
	uint8_t *pll_limits = &bios->data[bios->pll_limit_tbl_ptr], *entry;
	int len = 2 + bios->ram_restrict_group_count * 4;
	int i;

	if (!iexec->execute)
		return len;

	if (!bios->pll_limit_tbl_ptr || (pll_limits[0] & 0xf0) != 0x30) {
		NV_ERROR(dev, "PLL limits table not version 3.x\n");
		return len; 
	}

	entry = pll_limits + pll_limits[1];
	for (i = 0; i < pll_limits[3]; i++, entry += pll_limits[2]) {
		if (entry[0] == type) {
			uint32_t reg = ROM32(entry[3]);

			BIOSLOG(bios, "0x%04X: "
				      "Type %02x Reg 0x%08x Freq %dKHz\n",
				offset, type, reg, freq);

			setPLL(bios, reg, freq);
			return len;
		}
	}

	NV_ERROR(dev, "PLL type 0x%02x not found in PLL limits table", type);
	return len;
}

static int
init_8c(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	return 1;
}

static int
init_8d(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	return 1;
}

static int
init_gpio(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	if (iexec->execute && bios->execute)
		nouveau_gpio_reset(bios->dev);

	return 1;
}

static int
init_ram_restrict_zm_reg_group(struct nvbios *bios, uint16_t offset,
			       struct init_exec *iexec)
{
	/*
	 * INIT_RAM_RESTRICT_ZM_REG_GROUP   opcode: 0x8F ('')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): reg
	 * offset + 5  (8  bit): regincrement
	 * offset + 6  (8  bit): count
	 * offset + 7  (32 bit): value 1,1
	 * ...
	 *
	 * Use the RAMCFG strap of PEXTDEV_BOOT as an index into the table at
	 * ram_restrict_table_ptr. The value read from here is 'n', and
	 * "value 1,n" gets written to "reg". This repeats "count" times and on
	 * each iteration 'm', "reg" increases by "regincrement" and
	 * "value m,n" is used. The extent of n is limited by a number read
	 * from the 'M' BIT table, herein called "blocklen"
	 */

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint8_t regincrement = bios->data[offset + 5];
	uint8_t count = bios->data[offset + 6];
	uint32_t strap_ramcfg, data;
	
	uint16_t blocklen = bios->ram_restrict_group_count * 4;
	int len = 7 + count * blocklen;
	uint8_t index;
	int i;

	;
	if (!blocklen) {
		NV_ERROR(bios->dev,
			 "0x%04X: Zero block length - has the M table "
			 "been parsed?\n", offset);
		return -EINVAL;
	}

	if (!iexec->execute)
		return len;

	strap_ramcfg = (bios_rd32(bios, NV_PEXTDEV_BOOT_0) >> 2) & 0xf;
	index = bios->data[bios->ram_restrict_tbl_ptr + strap_ramcfg];

	BIOSLOG(bios, "0x%04X: Reg: 0x%08X, RegIncrement: 0x%02X, "
		      "Count: 0x%02X, StrapRamCfg: 0x%02X, Index: 0x%02X\n",
		offset, reg, regincrement, count, strap_ramcfg, index);

	for (i = 0; i < count; i++) {
		data = ROM32(bios->data[offset + 7 + index * 4 + blocklen * i]);

		bios_wr32(bios, reg, data);

		reg += regincrement;
	}

	return len;
}

static int
init_copy_zm_reg(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t srcreg = ROM32(bios->data[offset + 1]);
	uint32_t dstreg = ROM32(bios->data[offset + 5]);

	if (!iexec->execute)
		return 9;

	bios_wr32(bios, dstreg, bios_rd32(bios, srcreg));

	return 9;
}

static int
init_zm_reg_group_addr_latched(struct nvbios *bios, uint16_t offset,
			       struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint8_t count = bios->data[offset + 5];
	int len = 6 + count * 4;
	int i;

	if (!iexec->execute)
		return len;

	for (i = 0; i < count; i++) {
		uint32_t data = ROM32(bios->data[offset + 6 + 4 * i]);
		bios_wr32(bios, reg, data);
	}

	return len;
}

static int
init_reserved(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	return 1;
}

static int
init_96(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint16_t xlatptr = bios->init96_tbl_ptr + (bios->data[offset + 7] * 2);
	uint32_t reg = ROM32(bios->data[offset + 8]);
	uint32_t mask = ROM32(bios->data[offset + 12]);
	uint32_t val;

	val = bios_rd32(bios, ROM32(bios->data[offset + 1]));
	if (bios->data[offset + 5] < 0x80)
		val >>= bios->data[offset + 5];
	else
		val <<= (0x100 - bios->data[offset + 5]);
	val &= bios->data[offset + 6];

	val   = bios->data[ROM16(bios->data[xlatptr]) + val];
	val <<= bios->data[offset + 16];

	if (!iexec->execute)
		return 17;

	bios_wr32(bios, reg, (bios_rd32(bios, reg) & mask) | val);
	return 17;
}

static int
init_97(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint32_t reg = ROM32(bios->data[offset + 1]);
	uint32_t mask = ROM32(bios->data[offset + 5]);
	uint32_t add = ROM32(bios->data[offset + 9]);
	uint32_t val;

	val = bios_rd32(bios, reg);
	val = (val & mask) | ((val + add) & ~mask);

	if (!iexec->execute)
		return 13;

	bios_wr32(bios, reg, val);
	return 13;
}

static int
init_auxch(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	struct nouveau_i2c_chan *auxch;
	uint32_t addr = ROM32(bios->data[offset + 1]);
	uint8_t count = bios->data[offset + 5];
	int len = 6 + count * 2;
	int ret, i;

	if (!bios->display.output) {
		NV_ERROR(dev, "INIT_AUXCH: no active output\n");
		return len;
	}

	auxch = init_i2c_device_find(dev, bios->display.output->i2c_index);
	if (!auxch) {
		NV_ERROR(dev, "INIT_AUXCH: couldn't get auxch %d\n",
			 bios->display.output->i2c_index);
		return len;
	}

	if (!iexec->execute)
		return len;

	offset += 6;
	for (i = 0; i < count; i++, offset += 2) {
		uint8_t data;

		ret = nouveau_dp_auxch(auxch, 9, addr, &data, 1);
		if (ret) {
			NV_ERROR(dev, "INIT_AUXCH: rd auxch fail %d\n", ret);
			return len;
		}

		data &= bios->data[offset + 0];
		data |= bios->data[offset + 1];

		ret = nouveau_dp_auxch(auxch, 8, addr, &data, 1);
		if (ret) {
			NV_ERROR(dev, "INIT_AUXCH: wr auxch fail %d\n", ret);
			return len;
		}
	}

	return len;
}

static int
init_zm_auxch(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	struct drm_device *dev = bios->dev;
	struct nouveau_i2c_chan *auxch;
	uint32_t addr = ROM32(bios->data[offset + 1]);
	uint8_t count = bios->data[offset + 5];
	int len = 6 + count;
	int ret, i;

	if (!bios->display.output) {
		NV_ERROR(dev, "INIT_ZM_AUXCH: no active output\n");
		return len;
	}

	auxch = init_i2c_device_find(dev, bios->display.output->i2c_index);
	if (!auxch) {
		NV_ERROR(dev, "INIT_ZM_AUXCH: couldn't get auxch %d\n",
			 bios->display.output->i2c_index);
		return len;
	}

	if (!iexec->execute)
		return len;

	offset += 6;
	for (i = 0; i < count; i++, offset++) {
		ret = nouveau_dp_auxch(auxch, 8, addr, &bios->data[offset], 1);
		if (ret) {
			NV_ERROR(dev, "INIT_ZM_AUXCH: wr auxch fail %d\n", ret);
			return len;
		}
	}

	return len;
}

static int
init_i2c_long_if(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2] >> 1;
	uint8_t reglo = bios->data[offset + 3];
	uint8_t reghi = bios->data[offset + 4];
	uint8_t mask = bios->data[offset + 5];
	uint8_t data = bios->data[offset + 6];
	struct nouveau_i2c_chan *chan;
	uint8_t buf0[2] = { reghi, reglo };
	uint8_t buf1[1];
	struct i2c_msg msg[2] = {
		{ i2c_address, 0, 1, buf0 },
		{ i2c_address, I2C_M_RD, 1, buf1 },
	};
	int ret;

	

	BIOSLOG(bios, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X\n",
		offset, i2c_index, i2c_address);

	chan = init_i2c_device_find(bios->dev, i2c_index);
	if (!chan)
		return -ENODEV;


	ret = i2c_transfer(&chan->adapter, msg, 2);
	if (ret < 0) {
		BIOSLOG(bios, "0x%04X: I2CReg: 0x%02X:0x%02X, Value: [no device], "
			      "Mask: 0x%02X, Data: 0x%02X\n",
			offset, reghi, reglo, mask, data);
		iexec->execute = 0;
		return 7;
	}

	BIOSLOG(bios, "0x%04X: I2CReg: 0x%02X:0x%02X, Value: 0x%02X, "
		      "Mask: 0x%02X, Data: 0x%02X\n",
		offset, reghi, reglo, buf1[0], mask, data);

	iexec->execute = ((buf1[0] & mask) == data);

	return 7;
}

static struct init_tbl_entry itbl_entry[] = {
	
	
	{ "INIT_IO_RESTRICT_PROG"             , 0x32, init_io_restrict_prog           },
	{ "INIT_REPEAT"                       , 0x33, init_repeat                     },
	{ "INIT_IO_RESTRICT_PLL"              , 0x34, init_io_restrict_pll            },
	{ "INIT_END_REPEAT"                   , 0x36, init_end_repeat                 },
	{ "INIT_COPY"                         , 0x37, init_copy                       },
	{ "INIT_NOT"                          , 0x38, init_not                        },
	{ "INIT_IO_FLAG_CONDITION"            , 0x39, init_io_flag_condition          },
	{ "INIT_DP_CONDITION"                 , 0x3A, init_dp_condition               },
	{ "INIT_OP_3B"                        , 0x3B, init_op_3b                      },
	{ "INIT_OP_3C"                        , 0x3C, init_op_3c                      },
	{ "INIT_INDEX_ADDRESS_LATCHED"        , 0x49, init_idx_addr_latched           },
	{ "INIT_IO_RESTRICT_PLL2"             , 0x4A, init_io_restrict_pll2           },
	{ "INIT_PLL2"                         , 0x4B, init_pll2                       },
	{ "INIT_I2C_BYTE"                     , 0x4C, init_i2c_byte                   },
	{ "INIT_ZM_I2C_BYTE"                  , 0x4D, init_zm_i2c_byte                },
	{ "INIT_ZM_I2C"                       , 0x4E, init_zm_i2c                     },
	{ "INIT_TMDS"                         , 0x4F, init_tmds                       },
	{ "INIT_ZM_TMDS_GROUP"                , 0x50, init_zm_tmds_group              },
	{ "INIT_CR_INDEX_ADDRESS_LATCHED"     , 0x51, init_cr_idx_adr_latch           },
	{ "INIT_CR"                           , 0x52, init_cr                         },
	{ "INIT_ZM_CR"                        , 0x53, init_zm_cr                      },
	{ "INIT_ZM_CR_GROUP"                  , 0x54, init_zm_cr_group                },
	{ "INIT_CONDITION_TIME"               , 0x56, init_condition_time             },
	{ "INIT_LTIME"                        , 0x57, init_ltime                      },
	{ "INIT_ZM_REG_SEQUENCE"              , 0x58, init_zm_reg_sequence            },
	
	{ "INIT_SUB_DIRECT"                   , 0x5B, init_sub_direct                 },
	{ "INIT_JUMP"                         , 0x5C, init_jump                       },
	{ "INIT_I2C_IF"                       , 0x5E, init_i2c_if                     },
	{ "INIT_COPY_NV_REG"                  , 0x5F, init_copy_nv_reg                },
	{ "INIT_ZM_INDEX_IO"                  , 0x62, init_zm_index_io                },
	{ "INIT_COMPUTE_MEM"                  , 0x63, init_compute_mem                },
	{ "INIT_RESET"                        , 0x65, init_reset                      },
	{ "INIT_CONFIGURE_MEM"                , 0x66, init_configure_mem              },
	{ "INIT_CONFIGURE_CLK"                , 0x67, init_configure_clk              },
	{ "INIT_CONFIGURE_PREINIT"            , 0x68, init_configure_preinit          },
	{ "INIT_IO"                           , 0x69, init_io                         },
	{ "INIT_SUB"                          , 0x6B, init_sub                        },
	{ "INIT_RAM_CONDITION"                , 0x6D, init_ram_condition              },
	{ "INIT_NV_REG"                       , 0x6E, init_nv_reg                     },
	{ "INIT_MACRO"                        , 0x6F, init_macro                      },
	{ "INIT_DONE"                         , 0x71, init_done                       },
	{ "INIT_RESUME"                       , 0x72, init_resume                     },
	
	{ "INIT_TIME"                         , 0x74, init_time                       },
	{ "INIT_CONDITION"                    , 0x75, init_condition                  },
	{ "INIT_IO_CONDITION"                 , 0x76, init_io_condition               },
	{ "INIT_INDEX_IO"                     , 0x78, init_index_io                   },
	{ "INIT_PLL"                          , 0x79, init_pll                        },
	{ "INIT_ZM_REG"                       , 0x7A, init_zm_reg                     },
	{ "INIT_RAM_RESTRICT_PLL"             , 0x87, init_ram_restrict_pll           },
	{ "INIT_8C"                           , 0x8C, init_8c                         },
	{ "INIT_8D"                           , 0x8D, init_8d                         },
	{ "INIT_GPIO"                         , 0x8E, init_gpio                       },
	{ "INIT_RAM_RESTRICT_ZM_REG_GROUP"    , 0x8F, init_ram_restrict_zm_reg_group  },
	{ "INIT_COPY_ZM_REG"                  , 0x90, init_copy_zm_reg                },
	{ "INIT_ZM_REG_GROUP_ADDRESS_LATCHED" , 0x91, init_zm_reg_group_addr_latched  },
	{ "INIT_RESERVED"                     , 0x92, init_reserved                   },
	{ "INIT_96"                           , 0x96, init_96                         },
	{ "INIT_97"                           , 0x97, init_97                         },
	{ "INIT_AUXCH"                        , 0x98, init_auxch                      },
	{ "INIT_ZM_AUXCH"                     , 0x99, init_zm_auxch                   },
	{ "INIT_I2C_LONG_IF"                  , 0x9A, init_i2c_long_if                },
	{ NULL                                , 0   , NULL                            }
};

#define MAX_TABLE_OPS 1000

static int
parse_init_table(struct nvbios *bios, uint16_t offset, struct init_exec *iexec)
{

	int count = 0, i, ret;
	uint8_t id;

	
	if (offset == 0)
		return 0;

	while ((offset < bios->length) && (count++ < MAX_TABLE_OPS)) {
		id = bios->data[offset];

		
		for (i = 0; itbl_entry[i].name && (itbl_entry[i].id != id); i++)
			;

		if (!itbl_entry[i].name) {
			NV_ERROR(bios->dev,
				 "0x%04X: Init table command not found: "
				 "0x%02X\n", offset, id);
			return -ENOENT;
		}

		BIOSLOG(bios, "0x%04X: [ (0x%02X) - %s ]\n", offset,
			itbl_entry[i].id, itbl_entry[i].name);

		
		ret = (*itbl_entry[i].handler)(bios, offset, iexec);
		if (ret < 0) {
			NV_ERROR(bios->dev, "0x%04X: Failed parsing init "
				 "table opcode: %s %d\n", offset,
				 itbl_entry[i].name, ret);
		}

		if (ret <= 0)
			break;

		offset += ret;
	}

	if (offset >= bios->length)
		NV_WARN(bios->dev,
			"Offset 0x%04X greater than known bios image length.  "
			"Corrupt image?\n", offset);
	if (count >= MAX_TABLE_OPS)
		NV_WARN(bios->dev,
			"More than %d opcodes to a table is unlikely, "
			"is the bios image corrupt?\n", MAX_TABLE_OPS);

	return 0;
}

static void
parse_init_tables(struct nvbios *bios)
{
	

	int i = 0;
	uint16_t table;
	struct init_exec iexec = {true, false};

	if (bios->old_style_init) {
		if (bios->init_script_tbls_ptr)
			parse_init_table(bios, bios->init_script_tbls_ptr, &iexec);
		if (bios->extra_init_script_tbl_ptr)
			parse_init_table(bios, bios->extra_init_script_tbl_ptr, &iexec);

		return;
	}

	while ((table = ROM16(bios->data[bios->init_script_tbls_ptr + i]))) {
		NV_INFO(bios->dev,
			"Parsing VBIOS init table %d at offset 0x%04X\n",
			i / 2, table);
		BIOSLOG(bios, "0x%04X: ------ Executing following commands ------\n", table);

		parse_init_table(bios, table, &iexec);
		i += 2;
	}
}

static uint16_t clkcmptable(struct nvbios *bios, uint16_t clktable, int pxclk)
{
	int compare_record_len, i = 0;
	uint16_t compareclk, scriptptr = 0;

	if (bios->major_version < 5) 
		compare_record_len = 3;
	else
		compare_record_len = 4;

	do {
		compareclk = ROM16(bios->data[clktable + compare_record_len * i]);
		if (pxclk >= compareclk * 10) {
			if (bios->major_version < 5) {
				uint8_t tmdssub = bios->data[clktable + 2 + compare_record_len * i];
				scriptptr = ROM16(bios->data[bios->init_script_tbls_ptr + tmdssub * 2]);
			} else
				scriptptr = ROM16(bios->data[clktable + 2 + compare_record_len * i]);
			break;
		}
		i++;
	} while (compareclk);

	return scriptptr;
}

static void
run_digital_op_script(struct drm_device *dev, uint16_t scriptptr,
		      struct dcb_entry *dcbent, int head, bool dl)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	struct init_exec iexec = {true, false};

	NV_TRACE(dev, "0x%04X: Parsing digital output script table\n",
		 scriptptr);
	bios_idxprt_wr(bios, NV_CIO_CRX__COLOR, NV_CIO_CRE_44,
		       head ? NV_CIO_CRE_44_HEADB : NV_CIO_CRE_44_HEADA);
	
	NVWriteVgaCrtc5758(dev, head, 0, dcbent->index);
	parse_init_table(bios, scriptptr, &iexec);

	nv04_dfp_bind_head(dev, dcbent, head, dl);
}

static int call_lvds_manufacturer_script(struct drm_device *dev, struct dcb_entry *dcbent, int head, enum LVDS_script script)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	uint8_t sub = bios->data[bios->fp.xlated_entry + script] + (bios->fp.link_c_increment && dcbent->or & OUTPUT_C ? 1 : 0);
	uint16_t scriptofs = ROM16(bios->data[bios->init_script_tbls_ptr + sub * 2]);

	if (!bios->fp.xlated_entry || !sub || !scriptofs)
		return -EINVAL;

	run_digital_op_script(dev, scriptofs, dcbent, head, bios->fp.dual_link);

	if (script == LVDS_PANEL_OFF) {
		
		mdelay(ROM16(bios->data[bios->fp.xlated_entry + 7]));
	}
#ifdef __powerpc__
	
	if (script == LVDS_RESET &&
	    (dev->pci_device == 0x0179 || dev->pci_device == 0x0189 ||
	     dev->pci_device == 0x0329))
		nv_write_tmds(dev, dcbent->or, 0, 0x02, 0x72);
#endif

	return 0;
}

static int run_lvds_table(struct drm_device *dev, struct dcb_entry *dcbent, int head, enum LVDS_script script, int pxclk)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	unsigned int outputset = (dcbent->or == 4) ? 1 : 0;
	uint16_t scriptptr = 0, clktable;


	switch (script) {
	case LVDS_INIT:
		return -ENOSYS;
	case LVDS_BACKLIGHT_ON:
	case LVDS_PANEL_ON:
		scriptptr = ROM16(bios->data[bios->fp.lvdsmanufacturerpointer + 7 + outputset * 2]);
		break;
	case LVDS_BACKLIGHT_OFF:
	case LVDS_PANEL_OFF:
		scriptptr = ROM16(bios->data[bios->fp.lvdsmanufacturerpointer + 11 + outputset * 2]);
		break;
	case LVDS_RESET:
		clktable = bios->fp.lvdsmanufacturerpointer + 15;
		if (dcbent->or == 4)
			clktable += 8;

		if (dcbent->lvdsconf.use_straps_for_mode) {
			if (bios->fp.dual_link)
				clktable += 4;
			if (bios->fp.if_is_24bit)
				clktable += 2;
		} else {
			
			int cmpval_24bit = (dcbent->or == 4) ? 4 : 1;

			if (bios->fp.dual_link) {
				clktable += 4;
				cmpval_24bit <<= 1;
			}

			if (bios->fp.strapless_is_24bit & cmpval_24bit)
				clktable += 2;
		}

		clktable = ROM16(bios->data[clktable]);
		if (!clktable) {
			NV_ERROR(dev, "Pixel clock comparison table not found\n");
			return -ENOENT;
		}
		scriptptr = clkcmptable(bios, clktable, pxclk);
	}

	if (!scriptptr) {
		NV_ERROR(dev, "LVDS output init script not found\n");
		return -ENOENT;
	}
	run_digital_op_script(dev, scriptptr, dcbent, head, bios->fp.dual_link);

	return 0;
}

int call_lvds_script(struct drm_device *dev, struct dcb_entry *dcbent, int head, enum LVDS_script script, int pxclk)
{

	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	uint8_t lvds_ver = bios->data[bios->fp.lvdsmanufacturerpointer];
	uint32_t sel_clk_binding, sel_clk;
	int ret;

	if (bios->fp.last_script_invoc == (script << 1 | head) || !lvds_ver ||
	    (lvds_ver >= 0x30 && script == LVDS_INIT))
		return 0;

	if (!bios->fp.lvds_init_run) {
		bios->fp.lvds_init_run = true;
		call_lvds_script(dev, dcbent, head, LVDS_INIT, pxclk);
	}

	if (script == LVDS_PANEL_ON && bios->fp.reset_after_pclk_change)
		call_lvds_script(dev, dcbent, head, LVDS_RESET, pxclk);
	if (script == LVDS_RESET && bios->fp.power_off_for_reset)
		call_lvds_script(dev, dcbent, head, LVDS_PANEL_OFF, pxclk);

	NV_TRACE(dev, "Calling LVDS script %d:\n", script);

	
	sel_clk_binding = bios_rd32(bios, NV_PRAMDAC_SEL_CLK) & 0x50000;

	if (lvds_ver < 0x30)
		ret = call_lvds_manufacturer_script(dev, dcbent, head, script);
	else
		ret = run_lvds_table(dev, dcbent, head, script, pxclk);

	bios->fp.last_script_invoc = (script << 1 | head);

	sel_clk = NVReadRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK) & ~0x50000;
	NVWriteRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK, sel_clk | sel_clk_binding);
	
	nvWriteMC(dev, NV_PBUS_POWERCTRL_2, 0);

	return ret;
}

struct lvdstableheader {
	uint8_t lvds_ver, headerlen, recordlen;
};

static int parse_lvds_manufacturer_table_header(struct drm_device *dev, struct nvbios *bios, struct lvdstableheader *lth)
{

	uint8_t lvds_ver, headerlen, recordlen;

	memset(lth, 0, sizeof(struct lvdstableheader));

	if (bios->fp.lvdsmanufacturerpointer == 0x0) {
		NV_ERROR(dev, "Pointer to LVDS manufacturer table invalid\n");
		return -EINVAL;
	}

	lvds_ver = bios->data[bios->fp.lvdsmanufacturerpointer];

	switch (lvds_ver) {
	case 0x0a:	
		headerlen = 2;
		recordlen = bios->data[bios->fp.lvdsmanufacturerpointer + 1];
		break;
	case 0x30:	
		headerlen = bios->data[bios->fp.lvdsmanufacturerpointer + 1];
		if (headerlen < 0x1f) {
			NV_ERROR(dev, "LVDS table header not understood\n");
			return -EINVAL;
		}
		recordlen = bios->data[bios->fp.lvdsmanufacturerpointer + 2];
		break;
	case 0x40:	
		headerlen = bios->data[bios->fp.lvdsmanufacturerpointer + 1];
		if (headerlen < 0x7) {
			NV_ERROR(dev, "LVDS table header not understood\n");
			return -EINVAL;
		}
		recordlen = bios->data[bios->fp.lvdsmanufacturerpointer + 2];
		break;
	default:
		NV_ERROR(dev,
			 "LVDS table revision %d.%d not currently supported\n",
			 lvds_ver >> 4, lvds_ver & 0xf);
		return -ENOSYS;
	}

	lth->lvds_ver = lvds_ver;
	lth->headerlen = headerlen;
	lth->recordlen = recordlen;

	return 0;
}

static int
get_fp_strap(struct drm_device *dev, struct nvbios *bios)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;


	if (bios->major_version < 5 && bios->data[0x48] & 0x4)
		return NVReadVgaCrtc5758(dev, 0, 0xf) & 0xf;

	if (dev_priv->card_type >= NV_50)
		return (bios_rd32(bios, NV_PEXTDEV_BOOT_0) >> 24) & 0xf;
	else
		return (bios_rd32(bios, NV_PEXTDEV_BOOT_0) >> 16) & 0xf;
}

static int parse_fp_mode_table(struct drm_device *dev, struct nvbios *bios)
{
	uint8_t *fptable;
	uint8_t fptable_ver, headerlen = 0, recordlen, fpentries = 0xf, fpindex;
	int ret, ofs, fpstrapping;
	struct lvdstableheader lth;

	if (bios->fp.fptablepointer == 0x0) {
		
		
#ifndef __powerpc__
		NV_ERROR(dev, "Pointer to flat panel table invalid\n");
#endif
		bios->digital_min_front_porch = 0x4b;
		return 0;
	}

	fptable = &bios->data[bios->fp.fptablepointer];
	fptable_ver = fptable[0];

	switch (fptable_ver) {
	case 0x05:	
		recordlen = 42;
		ofs = -1;
		break;
	case 0x10:	
		recordlen = 44;
		ofs = 0;
		break;
	case 0x20:	
		headerlen = fptable[1];
		recordlen = fptable[2];
		fpentries = fptable[3];
		bios->digital_min_front_porch = fptable[4];
		ofs = -7;
		break;
	default:
		NV_ERROR(dev,
			 "FP table revision %d.%d not currently supported\n",
			 fptable_ver >> 4, fptable_ver & 0xf);
		return -ENOSYS;
	}

	if (!bios->is_mobile) 
		return 0;

	ret = parse_lvds_manufacturer_table_header(dev, bios, &lth);
	if (ret)
		return ret;

	if (lth.lvds_ver == 0x30 || lth.lvds_ver == 0x40) {
		bios->fp.fpxlatetableptr = bios->fp.lvdsmanufacturerpointer +
							lth.headerlen + 1;
		bios->fp.xlatwidth = lth.recordlen;
	}
	if (bios->fp.fpxlatetableptr == 0x0) {
		NV_ERROR(dev, "Pointer to flat panel xlat table invalid\n");
		return -EINVAL;
	}

	fpstrapping = get_fp_strap(dev, bios);

	fpindex = bios->data[bios->fp.fpxlatetableptr +
					fpstrapping * bios->fp.xlatwidth];

	if (fpindex > fpentries) {
		NV_ERROR(dev, "Bad flat panel table index\n");
		return -ENOENT;
	}

	
	if (lth.lvds_ver > 0x10)
		bios->fp_no_ddc = fpstrapping != 0xf || fpindex != 0xf;

	if (fpstrapping == 0xf || fpindex == 0xf)
		return 0;

	bios->fp.mode_ptr = bios->fp.fptablepointer + headerlen +
			    recordlen * fpindex + ofs;

	NV_TRACE(dev, "BIOS FP mode: %dx%d (%dkHz pixel clock)\n",
		 ROM16(bios->data[bios->fp.mode_ptr + 11]) + 1,
		 ROM16(bios->data[bios->fp.mode_ptr + 25]) + 1,
		 ROM16(bios->data[bios->fp.mode_ptr + 7]) * 10);

	return 0;
}

bool nouveau_bios_fp_mode(struct drm_device *dev, struct drm_display_mode *mode)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	uint8_t *mode_entry = &bios->data[bios->fp.mode_ptr];

	if (!mode)	
		return bios->fp.mode_ptr;

	memset(mode, 0, sizeof(struct drm_display_mode));
	mode->clock = ROM16(mode_entry[7]) * 10;
	
	mode->hdisplay = ROM16(mode_entry[11]) + 1;
	mode->hsync_start = ROM16(mode_entry[17]) + 1;
	mode->hsync_end = ROM16(mode_entry[19]) + 1;
	mode->htotal = ROM16(mode_entry[21]) + 1;
	
	mode->vdisplay = ROM16(mode_entry[25]) + 1;
	mode->vsync_start = ROM16(mode_entry[31]) + 1;
	mode->vsync_end = ROM16(mode_entry[33]) + 1;
	mode->vtotal = ROM16(mode_entry[35]) + 1;
	mode->flags |= (mode_entry[37] & 0x10) ?
			DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;
	mode->flags |= (mode_entry[37] & 0x1) ?
			DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

	mode->status = MODE_OK;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	return bios->fp.mode_ptr;
}

int nouveau_bios_parse_lvds_table(struct drm_device *dev, int pxclk, bool *dl, bool *if_is_24bit)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	int fpstrapping = get_fp_strap(dev, bios), lvdsmanufacturerindex = 0;
	struct lvdstableheader lth;
	uint16_t lvdsofs;
	int ret, chip_version = bios->chip_version;

	ret = parse_lvds_manufacturer_table_header(dev, bios, &lth);
	if (ret)
		return ret;

	switch (lth.lvds_ver) {
	case 0x0a:	
		lvdsmanufacturerindex = bios->data[
					bios->fp.fpxlatemanufacturertableptr +
					fpstrapping];

		
		if (!pxclk)
			break;

		if (chip_version < 0x25) {
			lvdsmanufacturerindex =
				(bios->legacy.lvds_single_a_script_ptr & 1) ?
									2 : 0;
			if (pxclk >= bios->fp.duallink_transition_clk)
				lvdsmanufacturerindex++;
		} else if (chip_version < 0x30) {
			lvdsmanufacturerindex = 0;
		} else {
			
			lvdsmanufacturerindex = 0;
			if (pxclk >= bios->fp.duallink_transition_clk)
				lvdsmanufacturerindex = 2;
			if (pxclk >= 140000)
				lvdsmanufacturerindex = 3;
		}

		break;
	case 0x30:	
	case 0x40:	
		lvdsmanufacturerindex = fpstrapping;
		break;
	default:
		NV_ERROR(dev, "LVDS table revision not currently supported\n");
		return -ENOSYS;
	}

	lvdsofs = bios->fp.xlated_entry = bios->fp.lvdsmanufacturerpointer + lth.headerlen + lth.recordlen * lvdsmanufacturerindex;
	switch (lth.lvds_ver) {
	case 0x0a:
		bios->fp.power_off_for_reset = bios->data[lvdsofs] & 1;
		bios->fp.reset_after_pclk_change = bios->data[lvdsofs] & 2;
		bios->fp.dual_link = bios->data[lvdsofs] & 4;
		bios->fp.link_c_increment = bios->data[lvdsofs] & 8;
		*if_is_24bit = bios->data[lvdsofs] & 16;
		break;
	case 0x30:
	case 0x40:
		bios->fp.power_off_for_reset = true;
		bios->fp.reset_after_pclk_change = true;

		/*
		 * It's ok lvdsofs is wrong for nv4x edid case; dual_link is
		 * over-written, and if_is_24bit isn't used
		 */
		bios->fp.dual_link = bios->data[lvdsofs] & 1;
		bios->fp.if_is_24bit = bios->data[lvdsofs] & 2;
		bios->fp.strapless_is_24bit = bios->data[bios->fp.lvdsmanufacturerpointer + 4];
		bios->fp.duallink_transition_clk = ROM16(bios->data[bios->fp.lvdsmanufacturerpointer + 5]) * 10;
		break;
	}

	
	if (pxclk && (chip_version < 0x25 || chip_version > 0x28))
		bios->fp.dual_link = (pxclk >= bios->fp.duallink_transition_clk);

	*dl = bios->fp.dual_link;

	return 0;
}

bool
bios_encoder_match(struct dcb_entry *dcb, u32 hash)
{
	if ((hash & 0x000000f0) != (dcb->location << 4))
		return false;
	if ((hash & 0x0000000f) != dcb->type)
		return false;
	if (!(hash & (dcb->or << 16)))
		return false;

	switch (dcb->type) {
	case OUTPUT_TMDS:
	case OUTPUT_LVDS:
	case OUTPUT_DP:
		if (hash & 0x00c00000) {
			if (!(hash & (dcb->sorconf.link << 22)))
				return false;
		}
	default:
		return true;
	}
}

int
nouveau_bios_run_display_table(struct drm_device *dev, u16 type, int pclk,
			       struct dcb_entry *dcbent, int crtc)
{

	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	uint8_t *table = &bios->data[bios->display.script_table_ptr];
	uint8_t *otable = NULL;
	uint16_t script;
	int i;

	if (!bios->display.script_table_ptr) {
		NV_ERROR(dev, "No pointer to output script table\n");
		return 1;
	}

	if (table[0] < 0x20)
		return 1;

	if (table[0] != 0x20 && table[0] != 0x21) {
		NV_ERROR(dev, "Output script table version 0x%02x unknown\n",
			 table[0]);
		return 1;
	}


	NV_DEBUG_KMS(dev, "Searching for output entry for %d %d %d\n",
			dcbent->type, dcbent->location, dcbent->or);
	for (i = 0; i < table[3]; i++) {
		otable = ROMPTR(dev, table[table[1] + (i * table[2])]);
		if (otable && bios_encoder_match(dcbent, ROM32(otable[0])))
			break;
	}

	if (!otable) {
		NV_DEBUG_KMS(dev, "failed to match any output table\n");
		return 1;
	}

	if (pclk < -2 || pclk > 0) {
		
		for (i = 0; i < otable[5]; i++) {
			if (ROM16(otable[table[4] + i*6]) == type)
				break;
		}

		if (i == otable[5]) {
			NV_ERROR(dev, "Table 0x%04x not found for %d/%d, "
				      "using first\n",
				 type, dcbent->type, dcbent->or);
			i = 0;
		}
	}

	if (pclk == 0) {
		script = ROM16(otable[6]);
		if (!script) {
			NV_DEBUG_KMS(dev, "output script 0 not found\n");
			return 1;
		}

		NV_DEBUG_KMS(dev, "0x%04X: parsing output script 0\n", script);
		nouveau_bios_run_init_table(dev, script, dcbent, crtc);
	} else
	if (pclk == -1) {
		script = ROM16(otable[8]);
		if (!script) {
			NV_DEBUG_KMS(dev, "output script 1 not found\n");
			return 1;
		}

		NV_DEBUG_KMS(dev, "0x%04X: parsing output script 1\n", script);
		nouveau_bios_run_init_table(dev, script, dcbent, crtc);
	} else
	if (pclk == -2) {
		if (table[4] >= 12)
			script = ROM16(otable[10]);
		else
			script = 0;
		if (!script) {
			NV_DEBUG_KMS(dev, "output script 2 not found\n");
			return 1;
		}

		NV_DEBUG_KMS(dev, "0x%04X: parsing output script 2\n", script);
		nouveau_bios_run_init_table(dev, script, dcbent, crtc);
	} else
	if (pclk > 0) {
		script = ROM16(otable[table[4] + i*6 + 2]);
		if (script)
			script = clkcmptable(bios, script, pclk);
		if (!script) {
			NV_DEBUG_KMS(dev, "clock script 0 not found\n");
			return 1;
		}

		NV_DEBUG_KMS(dev, "0x%04X: parsing clock script 0\n", script);
		nouveau_bios_run_init_table(dev, script, dcbent, crtc);
	} else
	if (pclk < 0) {
		script = ROM16(otable[table[4] + i*6 + 4]);
		if (script)
			script = clkcmptable(bios, script, -pclk);
		if (!script) {
			NV_DEBUG_KMS(dev, "clock script 1 not found\n");
			return 1;
		}

		NV_DEBUG_KMS(dev, "0x%04X: parsing clock script 1\n", script);
		nouveau_bios_run_init_table(dev, script, dcbent, crtc);
	}

	return 0;
}


int run_tmds_table(struct drm_device *dev, struct dcb_entry *dcbent, int head, int pxclk)
{

	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	int cv = bios->chip_version;
	uint16_t clktable = 0, scriptptr;
	uint32_t sel_clk_binding, sel_clk;

	
	if (cv >= 0x17 && cv != 0x1a && cv != 0x20 &&
	    dcbent->location != DCB_LOC_ON_CHIP)
		return 0;

	switch (ffs(dcbent->or)) {
	case 1:
		clktable = bios->tmds.output0_script_ptr;
		break;
	case 2:
	case 3:
		clktable = bios->tmds.output1_script_ptr;
		break;
	}

	if (!clktable) {
		NV_ERROR(dev, "Pixel clock comparison table not found\n");
		return -EINVAL;
	}

	scriptptr = clkcmptable(bios, clktable, pxclk);

	if (!scriptptr) {
		NV_ERROR(dev, "TMDS output init script not found\n");
		return -ENOENT;
	}

	
	sel_clk_binding = bios_rd32(bios, NV_PRAMDAC_SEL_CLK) & 0x50000;
	run_digital_op_script(dev, scriptptr, dcbent, head, pxclk >= 165000);
	sel_clk = NVReadRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK) & ~0x50000;
	NVWriteRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK, sel_clk | sel_clk_binding);

	return 0;
}

struct pll_mapping {
	u8  type;
	u32 reg;
};

static struct pll_mapping nv04_pll_mapping[] = {
	{ PLL_CORE  , NV_PRAMDAC_NVPLL_COEFF },
	{ PLL_MEMORY, NV_PRAMDAC_MPLL_COEFF },
	{ PLL_VPLL0 , NV_PRAMDAC_VPLL_COEFF },
	{ PLL_VPLL1 , NV_RAMDAC_VPLL2 },
	{}
};

static struct pll_mapping nv40_pll_mapping[] = {
	{ PLL_CORE  , 0x004000 },
	{ PLL_MEMORY, 0x004020 },
	{ PLL_VPLL0 , NV_PRAMDAC_VPLL_COEFF },
	{ PLL_VPLL1 , NV_RAMDAC_VPLL2 },
	{}
};

static struct pll_mapping nv50_pll_mapping[] = {
	{ PLL_CORE  , 0x004028 },
	{ PLL_SHADER, 0x004020 },
	{ PLL_UNK03 , 0x004000 },
	{ PLL_MEMORY, 0x004008 },
	{ PLL_UNK40 , 0x00e810 },
	{ PLL_UNK41 , 0x00e818 },
	{ PLL_UNK42 , 0x00e824 },
	{ PLL_VPLL0 , 0x614100 },
	{ PLL_VPLL1 , 0x614900 },
	{}
};

static struct pll_mapping nv84_pll_mapping[] = {
	{ PLL_CORE  , 0x004028 },
	{ PLL_SHADER, 0x004020 },
	{ PLL_MEMORY, 0x004008 },
	{ PLL_VDEC  , 0x004030 },
	{ PLL_UNK41 , 0x00e818 },
	{ PLL_VPLL0 , 0x614100 },
	{ PLL_VPLL1 , 0x614900 },
	{}
};

u32
get_pll_register(struct drm_device *dev, enum pll_types type)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	struct pll_mapping *map;
	int i;

	if (dev_priv->card_type < NV_40)
		map = nv04_pll_mapping;
	else
	if (dev_priv->card_type < NV_50)
		map = nv40_pll_mapping;
	else {
		u8 *plim = &bios->data[bios->pll_limit_tbl_ptr];

		if (plim[0] >= 0x30) {
			u8 *entry = plim + plim[1];
			for (i = 0; i < plim[3]; i++, entry += plim[2]) {
				if (entry[0] == type)
					return ROM32(entry[3]);
			}

			return 0;
		}

		if (dev_priv->chipset == 0x50)
			map = nv50_pll_mapping;
		else
			map = nv84_pll_mapping;
	}

	while (map->reg) {
		if (map->type == type)
			return map->reg;
		map++;
	}

	return 0;
}

int get_pll_limits(struct drm_device *dev, uint32_t limit_match, struct pll_lims *pll_lim)
{

	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	int cv = bios->chip_version, pllindex = 0;
	uint8_t pll_lim_ver = 0, headerlen = 0, recordlen = 0, entries = 0;
	uint32_t crystal_strap_mask, crystal_straps;

	if (!bios->pll_limit_tbl_ptr) {
		if (cv == 0x30 || cv == 0x31 || cv == 0x35 || cv == 0x36 ||
		    cv >= 0x40) {
			NV_ERROR(dev, "Pointer to PLL limits table invalid\n");
			return -EINVAL;
		}
	} else
		pll_lim_ver = bios->data[bios->pll_limit_tbl_ptr];

	crystal_strap_mask = 1 << 6;
	
	if (cv > 0x10 && cv != 0x15 && cv != 0x1a && cv != 0x20)
		crystal_strap_mask |= 1 << 22;
	crystal_straps = nvReadEXTDEV(dev, NV_PEXTDEV_BOOT_0) &
							crystal_strap_mask;

	switch (pll_lim_ver) {
	case 0:
		break;
	case 0x10:
	case 0x11:
		headerlen = 1;
		recordlen = 0x18;
		entries = 1;
		pllindex = 0;
		break;
	case 0x20:
	case 0x21:
	case 0x30:
	case 0x40:
		headerlen = bios->data[bios->pll_limit_tbl_ptr + 1];
		recordlen = bios->data[bios->pll_limit_tbl_ptr + 2];
		entries = bios->data[bios->pll_limit_tbl_ptr + 3];
		break;
	default:
		NV_ERROR(dev, "PLL limits table revision 0x%X not currently "
				"supported\n", pll_lim_ver);
		return -ENOSYS;
	}

	
	memset(pll_lim, 0, sizeof(struct pll_lims));

	if (limit_match > PLL_MAX)
		pll_lim->reg = limit_match;
	else {
		pll_lim->reg = get_pll_register(dev, limit_match);
		if (!pll_lim->reg)
			return -ENOENT;
	}

	if (pll_lim_ver == 0x10 || pll_lim_ver == 0x11) {
		uint8_t *pll_rec = &bios->data[bios->pll_limit_tbl_ptr + headerlen + recordlen * pllindex];

		pll_lim->vco1.minfreq = ROM32(pll_rec[0]);
		pll_lim->vco1.maxfreq = ROM32(pll_rec[4]);
		pll_lim->vco2.minfreq = ROM32(pll_rec[8]);
		pll_lim->vco2.maxfreq = ROM32(pll_rec[12]);
		pll_lim->vco1.min_inputfreq = ROM32(pll_rec[16]);
		pll_lim->vco2.min_inputfreq = ROM32(pll_rec[20]);
		pll_lim->vco1.max_inputfreq = pll_lim->vco2.max_inputfreq = INT_MAX;

		
		pll_lim->vco1.min_n = 0x1;
		if (cv == 0x36)
			pll_lim->vco1.min_n = 0x5;
		pll_lim->vco1.max_n = 0xff;
		pll_lim->vco1.min_m = 0x1;
		pll_lim->vco1.max_m = 0xd;
		pll_lim->vco2.min_n = 0x4;
		pll_lim->vco2.max_n = 0x28;
		if (cv == 0x30 || cv == 0x35)
			
			pll_lim->vco2.max_n = 0x1f;
		pll_lim->vco2.min_m = 0x1;
		pll_lim->vco2.max_m = 0x4;
		pll_lim->max_log2p = 0x7;
		pll_lim->max_usable_log2p = 0x6;
	} else if (pll_lim_ver == 0x20 || pll_lim_ver == 0x21) {
		uint16_t plloffs = bios->pll_limit_tbl_ptr + headerlen;
		uint8_t *pll_rec;
		int i;

		if (ROM32(bios->data[plloffs]))
			NV_WARN(dev, "Default PLL limit entry has non-zero "
				       "register field\n");

		for (i = 1; i < entries; i++)
			if (ROM32(bios->data[plloffs + recordlen * i]) == pll_lim->reg) {
				pllindex = i;
				break;
			}

		if ((dev_priv->card_type >= NV_50) && (pllindex == 0)) {
			NV_ERROR(dev, "Register 0x%08x not found in PLL "
				 "limits table", pll_lim->reg);
			return -ENOENT;
		}

		pll_rec = &bios->data[plloffs + recordlen * pllindex];

		BIOSLOG(bios, "Loading PLL limits for reg 0x%08x\n",
			pllindex ? pll_lim->reg : 0);


		
		pll_lim->vco1.minfreq = ROM16(pll_rec[4]) * 1000;
		pll_lim->vco1.maxfreq = ROM16(pll_rec[6]) * 1000;
		pll_lim->vco2.minfreq = ROM16(pll_rec[8]) * 1000;
		pll_lim->vco2.maxfreq = ROM16(pll_rec[10]) * 1000;

		
		pll_lim->vco1.min_inputfreq = ROM16(pll_rec[12]) * 1000;
		pll_lim->vco2.min_inputfreq = ROM16(pll_rec[14]) * 1000;
		pll_lim->vco1.max_inputfreq = ROM16(pll_rec[16]) * 1000;
		pll_lim->vco2.max_inputfreq = ROM16(pll_rec[18]) * 1000;

		
		pll_lim->vco1.min_n = pll_rec[20];
		pll_lim->vco1.max_n = pll_rec[21];
		pll_lim->vco1.min_m = pll_rec[22];
		pll_lim->vco1.max_m = pll_rec[23];
		pll_lim->vco2.min_n = pll_rec[24];
		pll_lim->vco2.max_n = pll_rec[25];
		pll_lim->vco2.min_m = pll_rec[26];
		pll_lim->vco2.max_m = pll_rec[27];

		pll_lim->max_usable_log2p = pll_lim->max_log2p = pll_rec[29];
		if (pll_lim->max_log2p > 0x7)
			
			NV_WARN(dev, "Max log2 P value greater than 7 (%d)\n",
				pll_lim->max_log2p);
		if (cv < 0x60)
			pll_lim->max_usable_log2p = 0x6;
		pll_lim->log2p_bias = pll_rec[30];

		if (recordlen > 0x22)
			pll_lim->refclk = ROM32(pll_rec[31]);

		if (recordlen > 0x23 && pll_rec[35])
			NV_WARN(dev,
				"Bits set in PLL configuration byte (%x)\n",
				pll_rec[35]);

		
		if (cv == 0x51 && !pll_lim->refclk) {
			uint32_t sel_clk = bios_rd32(bios, NV_PRAMDAC_SEL_CLK);

			if ((pll_lim->reg == NV_PRAMDAC_VPLL_COEFF && sel_clk & 0x20) ||
			    (pll_lim->reg == NV_RAMDAC_VPLL2 && sel_clk & 0x80)) {
				if (bios_idxprt_rd(bios, NV_CIO_CRX__COLOR, NV_CIO_CRE_CHIP_ID_INDEX) < 0xa3)
					pll_lim->refclk = 200000;
				else
					pll_lim->refclk = 25000;
			}
		}
	} else if (pll_lim_ver == 0x30) { 
		uint8_t *entry = &bios->data[bios->pll_limit_tbl_ptr + headerlen];
		uint8_t *record = NULL;
		int i;

		BIOSLOG(bios, "Loading PLL limits for register 0x%08x\n",
			pll_lim->reg);

		for (i = 0; i < entries; i++, entry += recordlen) {
			if (ROM32(entry[3]) == pll_lim->reg) {
				record = &bios->data[ROM16(entry[1])];
				break;
			}
		}

		if (!record) {
			NV_ERROR(dev, "Register 0x%08x not found in PLL "
				 "limits table", pll_lim->reg);
			return -ENOENT;
		}

		pll_lim->vco1.minfreq = ROM16(record[0]) * 1000;
		pll_lim->vco1.maxfreq = ROM16(record[2]) * 1000;
		pll_lim->vco2.minfreq = ROM16(record[4]) * 1000;
		pll_lim->vco2.maxfreq = ROM16(record[6]) * 1000;
		pll_lim->vco1.min_inputfreq = ROM16(record[8]) * 1000;
		pll_lim->vco2.min_inputfreq = ROM16(record[10]) * 1000;
		pll_lim->vco1.max_inputfreq = ROM16(record[12]) * 1000;
		pll_lim->vco2.max_inputfreq = ROM16(record[14]) * 1000;
		pll_lim->vco1.min_n = record[16];
		pll_lim->vco1.max_n = record[17];
		pll_lim->vco1.min_m = record[18];
		pll_lim->vco1.max_m = record[19];
		pll_lim->vco2.min_n = record[20];
		pll_lim->vco2.max_n = record[21];
		pll_lim->vco2.min_m = record[22];
		pll_lim->vco2.max_m = record[23];
		pll_lim->max_usable_log2p = pll_lim->max_log2p = record[25];
		pll_lim->log2p_bias = record[27];
		pll_lim->refclk = ROM32(record[28]);
	} else if (pll_lim_ver) { 
		uint8_t *entry = &bios->data[bios->pll_limit_tbl_ptr + headerlen];
		uint8_t *record = NULL;
		int i;

		BIOSLOG(bios, "Loading PLL limits for register 0x%08x\n",
			pll_lim->reg);

		for (i = 0; i < entries; i++, entry += recordlen) {
			if (ROM32(entry[3]) == pll_lim->reg) {
				record = &bios->data[ROM16(entry[1])];
				break;
			}
		}

		if (!record) {
			NV_ERROR(dev, "Register 0x%08x not found in PLL "
				 "limits table", pll_lim->reg);
			return -ENOENT;
		}

		pll_lim->vco1.minfreq = ROM16(record[0]) * 1000;
		pll_lim->vco1.maxfreq = ROM16(record[2]) * 1000;
		pll_lim->vco1.min_inputfreq = ROM16(record[4]) * 1000;
		pll_lim->vco1.max_inputfreq = ROM16(record[6]) * 1000;
		pll_lim->vco1.min_m = record[8];
		pll_lim->vco1.max_m = record[9];
		pll_lim->vco1.min_n = record[10];
		pll_lim->vco1.max_n = record[11];
		pll_lim->min_p = record[12];
		pll_lim->max_p = record[13];
		pll_lim->refclk = ROM16(entry[9]) * 1000;
	}

	if (!pll_lim->vco1.maxfreq) {
		pll_lim->vco1.minfreq = bios->fminvco;
		pll_lim->vco1.maxfreq = bios->fmaxvco;
		pll_lim->vco1.min_inputfreq = 0;
		pll_lim->vco1.max_inputfreq = INT_MAX;
		pll_lim->vco1.min_n = 0x1;
		pll_lim->vco1.max_n = 0xff;
		pll_lim->vco1.min_m = 0x1;
		if (crystal_straps == 0) {
			
			if (cv < 0x11)
				pll_lim->vco1.min_m = 0x7;
			pll_lim->vco1.max_m = 0xd;
		} else {
			if (cv < 0x11)
				pll_lim->vco1.min_m = 0x8;
			pll_lim->vco1.max_m = 0xe;
		}
		if (cv < 0x17 || cv == 0x1a || cv == 0x20)
			pll_lim->max_log2p = 4;
		else
			pll_lim->max_log2p = 5;
		pll_lim->max_usable_log2p = pll_lim->max_log2p;
	}

	if (!pll_lim->refclk)
		switch (crystal_straps) {
		case 0:
			pll_lim->refclk = 13500;
			break;
		case (1 << 6):
			pll_lim->refclk = 14318;
			break;
		case (1 << 22):
			pll_lim->refclk = 27000;
			break;
		case (1 << 22 | 1 << 6):
			pll_lim->refclk = 25000;
			break;
		}

	NV_DEBUG(dev, "pll.vco1.minfreq: %d\n", pll_lim->vco1.minfreq);
	NV_DEBUG(dev, "pll.vco1.maxfreq: %d\n", pll_lim->vco1.maxfreq);
	NV_DEBUG(dev, "pll.vco1.min_inputfreq: %d\n", pll_lim->vco1.min_inputfreq);
	NV_DEBUG(dev, "pll.vco1.max_inputfreq: %d\n", pll_lim->vco1.max_inputfreq);
	NV_DEBUG(dev, "pll.vco1.min_n: %d\n", pll_lim->vco1.min_n);
	NV_DEBUG(dev, "pll.vco1.max_n: %d\n", pll_lim->vco1.max_n);
	NV_DEBUG(dev, "pll.vco1.min_m: %d\n", pll_lim->vco1.min_m);
	NV_DEBUG(dev, "pll.vco1.max_m: %d\n", pll_lim->vco1.max_m);
	if (pll_lim->vco2.maxfreq) {
		NV_DEBUG(dev, "pll.vco2.minfreq: %d\n", pll_lim->vco2.minfreq);
		NV_DEBUG(dev, "pll.vco2.maxfreq: %d\n", pll_lim->vco2.maxfreq);
		NV_DEBUG(dev, "pll.vco2.min_inputfreq: %d\n", pll_lim->vco2.min_inputfreq);
		NV_DEBUG(dev, "pll.vco2.max_inputfreq: %d\n", pll_lim->vco2.max_inputfreq);
		NV_DEBUG(dev, "pll.vco2.min_n: %d\n", pll_lim->vco2.min_n);
		NV_DEBUG(dev, "pll.vco2.max_n: %d\n", pll_lim->vco2.max_n);
		NV_DEBUG(dev, "pll.vco2.min_m: %d\n", pll_lim->vco2.min_m);
		NV_DEBUG(dev, "pll.vco2.max_m: %d\n", pll_lim->vco2.max_m);
	}
	if (!pll_lim->max_p) {
		NV_DEBUG(dev, "pll.max_log2p: %d\n", pll_lim->max_log2p);
		NV_DEBUG(dev, "pll.log2p_bias: %d\n", pll_lim->log2p_bias);
	} else {
		NV_DEBUG(dev, "pll.min_p: %d\n", pll_lim->min_p);
		NV_DEBUG(dev, "pll.max_p: %d\n", pll_lim->max_p);
	}
	NV_DEBUG(dev, "pll.refclk: %d\n", pll_lim->refclk);

	return 0;
}

static void parse_bios_version(struct drm_device *dev, struct nvbios *bios, uint16_t offset)
{

	bios->major_version = bios->data[offset + 3];
	bios->chip_version = bios->data[offset + 2];
	NV_TRACE(dev, "Bios version %02x.%02x.%02x.%02x\n",
		 bios->data[offset + 3], bios->data[offset + 2],
		 bios->data[offset + 1], bios->data[offset]);
}

static void parse_script_table_pointers(struct nvbios *bios, uint16_t offset)
{

	bios->init_script_tbls_ptr = ROM16(bios->data[offset]);
	bios->macro_index_tbl_ptr = ROM16(bios->data[offset + 2]);
	bios->macro_tbl_ptr = ROM16(bios->data[offset + 4]);
	bios->condition_tbl_ptr = ROM16(bios->data[offset + 6]);
	bios->io_condition_tbl_ptr = ROM16(bios->data[offset + 8]);
	bios->io_flag_condition_tbl_ptr = ROM16(bios->data[offset + 10]);
	bios->init_function_tbl_ptr = ROM16(bios->data[offset + 12]);
}

static int parse_bit_A_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	uint16_t load_table_ptr;
	uint8_t version, headerlen, entrylen, num_entries;

	if (bitentry->length != 3) {
		NV_ERROR(dev, "Do not understand BIT A table\n");
		return -EINVAL;
	}

	load_table_ptr = ROM16(bios->data[bitentry->offset]);

	if (load_table_ptr == 0x0) {
		NV_DEBUG(dev, "Pointer to BIT loadval table invalid\n");
		return -EINVAL;
	}

	version = bios->data[load_table_ptr];

	if (version != 0x10) {
		NV_ERROR(dev, "BIT loadval table version %d.%d not supported\n",
			 version >> 4, version & 0xF);
		return -ENOSYS;
	}

	headerlen = bios->data[load_table_ptr + 1];
	entrylen = bios->data[load_table_ptr + 2];
	num_entries = bios->data[load_table_ptr + 3];

	if (headerlen != 4 || entrylen != 4 || num_entries != 2) {
		NV_ERROR(dev, "Do not understand BIT loadval table\n");
		return -EINVAL;
	}

	
	bios->dactestval = ROM32(bios->data[load_table_ptr + headerlen]) & 0x3ff;

	return 0;
}

static int parse_bit_C_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	if (bitentry->length < 10) {
		NV_ERROR(dev, "Do not understand BIT C table\n");
		return -EINVAL;
	}

	bios->pll_limit_tbl_ptr = ROM16(bios->data[bitentry->offset + 8]);

	return 0;
}

static int parse_bit_display_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	if (bitentry->length != 4) {
		NV_ERROR(dev, "Do not understand BIT display table\n");
		return -EINVAL;
	}

	bios->fp.fptablepointer = ROM16(bios->data[bitentry->offset + 2]);

	return 0;
}

static int parse_bit_init_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	if (bitentry->length < 14) {
		NV_ERROR(dev, "Do not understand init table\n");
		return -EINVAL;
	}

	parse_script_table_pointers(bios, bitentry->offset);

	if (bitentry->length >= 16)
		bios->some_script_ptr = ROM16(bios->data[bitentry->offset + 14]);
	if (bitentry->length >= 18)
		bios->init96_tbl_ptr = ROM16(bios->data[bitentry->offset + 16]);

	return 0;
}

static int parse_bit_i_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	uint16_t daccmpoffset;
	uint8_t dacver, dacheaderlen;

	if (bitentry->length < 6) {
		NV_ERROR(dev, "BIT i table too short for needed information\n");
		return -EINVAL;
	}

	parse_bios_version(dev, bios, bitentry->offset);

	bios->feature_byte = bios->data[bitentry->offset + 5];
	bios->is_mobile = bios->feature_byte & FEATURE_MOBILE;

	if (bitentry->length < 15) {
		NV_WARN(dev, "BIT i table not long enough for DAC load "
			       "detection comparison table\n");
		return -EINVAL;
	}

	daccmpoffset = ROM16(bios->data[bitentry->offset + 13]);

	
	if (!daccmpoffset)
		return 0;


	dacver = bios->data[daccmpoffset];
	dacheaderlen = bios->data[daccmpoffset + 1];

	if (dacver != 0x00 && dacver != 0x10) {
		NV_WARN(dev, "DAC load detection comparison table version "
			       "%d.%d not known\n", dacver >> 4, dacver & 0xf);
		return -ENOSYS;
	}

	bios->dactestval = ROM32(bios->data[daccmpoffset + dacheaderlen]);
	bios->tvdactestval = ROM32(bios->data[daccmpoffset + dacheaderlen + 4]);

	return 0;
}

static int parse_bit_lvds_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	if (bitentry->length != 2) {
		NV_ERROR(dev, "Do not understand BIT LVDS table\n");
		return -EINVAL;
	}

	bios->fp.lvdsmanufacturerpointer = ROM16(bios->data[bitentry->offset]);

	return 0;
}

static int
parse_bit_M_tbl_entry(struct drm_device *dev, struct nvbios *bios,
		      struct bit_entry *bitentry)
{

	if (bitentry->length < 0x5)
		return 0;

	if (bitentry->version < 2) {
		bios->ram_restrict_group_count = bios->data[bitentry->offset + 2];
		bios->ram_restrict_tbl_ptr = ROM16(bios->data[bitentry->offset + 3]);
	} else {
		bios->ram_restrict_group_count = bios->data[bitentry->offset + 0];
		bios->ram_restrict_tbl_ptr = ROM16(bios->data[bitentry->offset + 1]);
	}

	return 0;
}

static int parse_bit_tmds_tbl_entry(struct drm_device *dev, struct nvbios *bios, struct bit_entry *bitentry)
{

	uint16_t tmdstableptr, script1, script2;

	if (bitentry->length != 2) {
		NV_ERROR(dev, "Do not understand BIT TMDS table\n");
		return -EINVAL;
	}

	tmdstableptr = ROM16(bios->data[bitentry->offset]);
	if (!tmdstableptr) {
		NV_ERROR(dev, "Pointer to TMDS table invalid\n");
		return -EINVAL;
	}

	NV_INFO(dev, "TMDS table version %d.%d\n",
		bios->data[tmdstableptr] >> 4, bios->data[tmdstableptr] & 0xf);

	
	if (bios->data[tmdstableptr] != 0x11)
		return -ENOSYS;

	script1 = ROM16(bios->data[tmdstableptr + 7]);
	script2 = ROM16(bios->data[tmdstableptr + 9]);
	if (bios->data[script1] != 'q' || bios->data[script2] != 'q')
		NV_WARN(dev, "TMDS table script pointers not stubbed\n");

	bios->tmds.output0_script_ptr = ROM16(bios->data[tmdstableptr + 11]);
	bios->tmds.output1_script_ptr = ROM16(bios->data[tmdstableptr + 13]);

	return 0;
}

static int
parse_bit_U_tbl_entry(struct drm_device *dev, struct nvbios *bios,
		      struct bit_entry *bitentry)
{

	uint16_t outputscripttableptr;

	if (bitentry->length != 3) {
		NV_ERROR(dev, "Do not understand BIT U table\n");
		return -EINVAL;
	}

	outputscripttableptr = ROM16(bios->data[bitentry->offset]);
	bios->display.script_table_ptr = outputscripttableptr;
	return 0;
}

struct bit_table {
	const char id;
	int (* const parse_fn)(struct drm_device *, struct nvbios *, struct bit_entry *);
};

#define BIT_TABLE(id, funcid) ((struct bit_table){ id, parse_bit_##funcid##_tbl_entry })

int
bit_table(struct drm_device *dev, u8 id, struct bit_entry *bit)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	u8 entries, *entry;

	if (bios->type != NVBIOS_BIT)
		return -ENODEV;

	entries = bios->data[bios->offset + 10];
	entry   = &bios->data[bios->offset + 12];
	while (entries--) {
		if (entry[0] == id) {
			bit->id = entry[0];
			bit->version = entry[1];
			bit->length = ROM16(entry[2]);
			bit->offset = ROM16(entry[4]);
			bit->data = ROMPTR(dev, entry[4]);
			return 0;
		}

		entry += bios->data[bios->offset + 9];
	}

	return -ENOENT;
}

static int
parse_bit_table(struct nvbios *bios, const uint16_t bitoffset,
		struct bit_table *table)
{
	struct drm_device *dev = bios->dev;
	struct bit_entry bitentry;

	if (bit_table(dev, table->id, &bitentry) == 0)
		return table->parse_fn(dev, bios, &bitentry);

	NV_INFO(dev, "BIT table '%c' not found\n", table->id);
	return -ENOSYS;
}

static int
parse_bit_structure(struct nvbios *bios, const uint16_t bitoffset)
{
	int ret;

	ret = parse_bit_table(bios, bitoffset, &BIT_TABLE('i', i));
	if (ret) 
		return ret;
	if (bios->major_version >= 0x60) 
		parse_bit_table(bios, bitoffset, &BIT_TABLE('A', A));
	ret = parse_bit_table(bios, bitoffset, &BIT_TABLE('C', C));
	if (ret)
		return ret;
	parse_bit_table(bios, bitoffset, &BIT_TABLE('D', display));
	ret = parse_bit_table(bios, bitoffset, &BIT_TABLE('I', init));
	if (ret)
		return ret;
	parse_bit_table(bios, bitoffset, &BIT_TABLE('M', M)); 
	parse_bit_table(bios, bitoffset, &BIT_TABLE('L', lvds));
	parse_bit_table(bios, bitoffset, &BIT_TABLE('T', tmds));
	parse_bit_table(bios, bitoffset, &BIT_TABLE('U', U));

	return 0;
}

static int parse_bmp_structure(struct drm_device *dev, struct nvbios *bios, unsigned int offset)
{

	uint8_t *bmp = &bios->data[offset], bmp_version_major, bmp_version_minor;
	uint16_t bmplength;
	uint16_t legacy_scripts_offset, legacy_i2c_offset;

	
	bios->digital_min_front_porch = 0x4b;
	bios->fmaxvco = 256000;
	bios->fminvco = 128000;
	bios->fp.duallink_transition_clk = 90000;

	bmp_version_major = bmp[5];
	bmp_version_minor = bmp[6];

	NV_TRACE(dev, "BMP version %d.%d\n",
		 bmp_version_major, bmp_version_minor);

	if (bmp_version_major < 5)
		*(uint16_t *)&bios->data[0x36] = 0;

	if ((bmp_version_major < 5 && bmp_version_minor != 1) || bmp_version_major > 5) {
		NV_ERROR(dev, "You have an unsupported BMP version. "
				"Please send in your bios\n");
		return -ENOSYS;
	}

	if (bmp_version_major == 0)
		
		return 0;
	else if (bmp_version_major == 1)
		bmplength = 44; 
	else if (bmp_version_major == 2)
		bmplength = 48; 
	else if (bmp_version_major == 3)
		bmplength = 54;
		
	else if (bmp_version_major == 4 || bmp_version_minor < 0x1)
		
		bmplength = 62;
		
	else if (bmp_version_minor < 0x6)
		bmplength = 67; 
	else if (bmp_version_minor < 0x10)
		bmplength = 75; 
	else if (bmp_version_minor == 0x10)
		bmplength = 89; 
	else if (bmp_version_minor < 0x14)
		bmplength = 118; 
	else if (bmp_version_minor < 0x24)
		
		bmplength = 123;
	else if (bmp_version_minor < 0x27)
		bmplength = 144;
	else
		bmplength = 158;

	
	if (nv_cksum(bmp, 8)) {
		NV_ERROR(dev, "Bad BMP checksum\n");
		return -EINVAL;
	}

	bios->feature_byte = bmp[9];

	parse_bios_version(dev, bios, offset + 10);

	if (bmp_version_major < 5 || bmp_version_minor < 0x10)
		bios->old_style_init = true;
	legacy_scripts_offset = 18;
	if (bmp_version_major < 2)
		legacy_scripts_offset -= 4;
	bios->init_script_tbls_ptr = ROM16(bmp[legacy_scripts_offset]);
	bios->extra_init_script_tbl_ptr = ROM16(bmp[legacy_scripts_offset + 2]);

	if (bmp_version_major > 2) {	
		bios->legacy.mem_init_tbl_ptr = ROM16(bmp[24]);
		bios->legacy.sdr_seq_tbl_ptr = ROM16(bmp[26]);
		bios->legacy.ddr_seq_tbl_ptr = ROM16(bmp[28]);
	}

	legacy_i2c_offset = 0x48;	
	if (bmplength > 61)
		legacy_i2c_offset = offset + 54;
	bios->legacy.i2c_indices.crt = bios->data[legacy_i2c_offset];
	bios->legacy.i2c_indices.tv = bios->data[legacy_i2c_offset + 1];
	bios->legacy.i2c_indices.panel = bios->data[legacy_i2c_offset + 2];

	if (bmplength > 74) {
		bios->fmaxvco = ROM32(bmp[67]);
		bios->fminvco = ROM32(bmp[71]);
	}
	if (bmplength > 88)
		parse_script_table_pointers(bios, offset + 75);
	if (bmplength > 94) {
		bios->tmds.output0_script_ptr = ROM16(bmp[89]);
		bios->tmds.output1_script_ptr = ROM16(bmp[91]);
		bios->legacy.lvds_single_a_script_ptr = ROM16(bmp[95]);
	}
	if (bmplength > 108) {
		bios->fp.fptablepointer = ROM16(bmp[105]);
		bios->fp.fpxlatetableptr = ROM16(bmp[107]);
		bios->fp.xlatwidth = 1;
	}
	if (bmplength > 120) {
		bios->fp.lvdsmanufacturerpointer = ROM16(bmp[117]);
		bios->fp.fpxlatemanufacturertableptr = ROM16(bmp[119]);
	}
	if (bmplength > 143)
		bios->pll_limit_tbl_ptr = ROM16(bmp[142]);

	if (bmplength > 157)
		bios->fp.duallink_transition_clk = ROM16(bmp[156]) * 10;

	return 0;
}

static uint16_t findstr(uint8_t *data, int n, const uint8_t *str, int len)
{
	int i, j;

	for (i = 0; i <= (n - len); i++) {
		for (j = 0; j < len; j++)
			if (data[i + j] != str[j])
				break;
		if (j == len)
			return i;
	}

	return 0;
}

void *
dcb_table(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	u8 *dcb = NULL;

	if (dev_priv->card_type > NV_04)
		dcb = ROMPTR(dev, dev_priv->vbios.data[0x36]);
	if (!dcb) {
		NV_WARNONCE(dev, "No DCB data found in VBIOS\n");
		return NULL;
	}

	if (dcb[0] >= 0x41) {
		NV_WARNONCE(dev, "DCB version 0x%02x unknown\n", dcb[0]);
		return NULL;
	} else
	if (dcb[0] >= 0x30) {
		if (ROM32(dcb[6]) == 0x4edcbdcb)
			return dcb;
	} else
	if (dcb[0] >= 0x20) {
		if (ROM32(dcb[4]) == 0x4edcbdcb)
			return dcb;
	} else
	if (dcb[0] >= 0x15) {
		if (!memcmp(&dcb[-7], "DEV_REC", 7))
			return dcb;
	} else {
		NV_WARNONCE(dev, "No useful DCB data in VBIOS\n");
		return NULL;
	}

	NV_WARNONCE(dev, "DCB header validation failed\n");
	return NULL;
}

void *
dcb_outp(struct drm_device *dev, u8 idx)
{
	u8 *dcb = dcb_table(dev);
	if (dcb && dcb[0] >= 0x30) {
		if (idx < dcb[2])
			return dcb + dcb[1] + (idx * dcb[3]);
	} else
	if (dcb && dcb[0] >= 0x20) {
		u8 *i2c = ROMPTR(dev, dcb[2]);
		u8 *ent = dcb + 8 + (idx * 8);
		if (i2c && ent < i2c)
			return ent;
	} else
	if (dcb && dcb[0] >= 0x15) {
		u8 *i2c = ROMPTR(dev, dcb[2]);
		u8 *ent = dcb + 4 + (idx * 10);
		if (i2c && ent < i2c)
			return ent;
	}

	return NULL;
}

int
dcb_outp_foreach(struct drm_device *dev, void *data,
		 int (*exec)(struct drm_device *, void *, int idx, u8 *outp))
{
	int ret, idx = -1;
	u8 *outp = NULL;
	while ((outp = dcb_outp(dev, ++idx))) {
		if (ROM32(outp[0]) == 0x00000000)
			break; 
		if (ROM32(outp[0]) == 0xffffffff)
			break; 

		if ((outp[0] & 0x0f) == OUTPUT_UNUSED)
			continue;
		if ((outp[0] & 0x0f) == OUTPUT_EOL)
			break;

		ret = exec(dev, data, idx, outp);
		if (ret)
			return ret;
	}

	return 0;
}

u8 *
dcb_conntab(struct drm_device *dev)
{
	u8 *dcb = dcb_table(dev);
	if (dcb && dcb[0] >= 0x30 && dcb[1] >= 0x16) {
		u8 *conntab = ROMPTR(dev, dcb[0x14]);
		if (conntab && conntab[0] >= 0x30 && conntab[0] <= 0x40)
			return conntab;
	}
	return NULL;
}

u8 *
dcb_conn(struct drm_device *dev, u8 idx)
{
	u8 *conntab = dcb_conntab(dev);
	if (conntab && idx < conntab[2])
		return conntab + conntab[1] + (idx * conntab[3]);
	return NULL;
}

static struct dcb_entry *new_dcb_entry(struct dcb_table *dcb)
{
	struct dcb_entry *entry = &dcb->entry[dcb->entries];

	memset(entry, 0, sizeof(struct dcb_entry));
	entry->index = dcb->entries++;

	return entry;
}

static void fabricate_dcb_output(struct dcb_table *dcb, int type, int i2c,
				 int heads, int or)
{
	struct dcb_entry *entry = new_dcb_entry(dcb);

	entry->type = type;
	entry->i2c_index = i2c;
	entry->heads = heads;
	if (type != OUTPUT_ANALOG)
		entry->location = !DCB_LOC_ON_CHIP; 
	entry->or = or;
}

static bool
parse_dcb20_entry(struct drm_device *dev, struct dcb_table *dcb,
		  uint32_t conn, uint32_t conf, struct dcb_entry *entry)
{
	entry->type = conn & 0xf;
	entry->i2c_index = (conn >> 4) & 0xf;
	entry->heads = (conn >> 8) & 0xf;
	entry->connector = (conn >> 12) & 0xf;
	entry->bus = (conn >> 16) & 0xf;
	entry->location = (conn >> 20) & 0x3;
	entry->or = (conn >> 24) & 0xf;

	switch (entry->type) {
	case OUTPUT_ANALOG:
		entry->crtconf.maxfreq = (dcb->version < 0x30) ?
					 (conf & 0xffff) * 10 :
					 (conf & 0xff) * 10000;
		break;
	case OUTPUT_LVDS:
		{
		uint32_t mask;
		if (conf & 0x1)
			entry->lvdsconf.use_straps_for_mode = true;
		if (dcb->version < 0x22) {
			mask = ~0xd;
			entry->lvdsconf.use_straps_for_mode = true;
			if (conf & 0x4 || conf & 0x8)
				entry->lvdsconf.use_power_scripts = true;
		} else {
			mask = ~0x7;
			if (conf & 0x2)
				entry->lvdsconf.use_acpi_for_edid = true;
			if (conf & 0x4)
				entry->lvdsconf.use_power_scripts = true;
			entry->lvdsconf.sor.link = (conf & 0x00000030) >> 4;
		}
		if (conf & mask) {
			if (dcb->version >= 0x40)
				break;

			NV_ERROR(dev, "Unknown LVDS configuration bits, "
				      "please report\n");
		}
		break;
		}
	case OUTPUT_TV:
	{
		if (dcb->version >= 0x30)
			entry->tvconf.has_component_output = conf & (0x8 << 4);
		else
			entry->tvconf.has_component_output = false;

		break;
	}
	case OUTPUT_DP:
		entry->dpconf.sor.link = (conf & 0x00000030) >> 4;
		switch ((conf & 0x00e00000) >> 21) {
		case 0:
			entry->dpconf.link_bw = 162000;
			break;
		default:
			entry->dpconf.link_bw = 270000;
			break;
		}
		switch ((conf & 0x0f000000) >> 24) {
		case 0xf:
			entry->dpconf.link_nr = 4;
			break;
		case 0x3:
			entry->dpconf.link_nr = 2;
			break;
		default:
			entry->dpconf.link_nr = 1;
			break;
		}
		break;
	case OUTPUT_TMDS:
		if (dcb->version >= 0x40)
			entry->tmdsconf.sor.link = (conf & 0x00000030) >> 4;
		else if (dcb->version >= 0x30)
			entry->tmdsconf.slave_addr = (conf & 0x00000700) >> 8;
		else if (dcb->version >= 0x22)
			entry->tmdsconf.slave_addr = (conf & 0x00000070) >> 4;

		break;
	case OUTPUT_EOL:
		
		dcb->entries--;
		return false;
	default:
		break;
	}

	if (dcb->version < 0x40) {
		entry->duallink_possible =
			((1 << (ffs(entry->or) - 1)) * 3 == entry->or);
	} else {
		entry->duallink_possible = (entry->sorconf.link == 3);
	}

	
	if (conf & 0x100000)
		entry->i2c_upper_default = true;

	return true;
}

static bool
parse_dcb15_entry(struct drm_device *dev, struct dcb_table *dcb,
		  uint32_t conn, uint32_t conf, struct dcb_entry *entry)
{
	switch (conn & 0x0000000f) {
	case 0:
		entry->type = OUTPUT_ANALOG;
		break;
	case 1:
		entry->type = OUTPUT_TV;
		break;
	case 2:
	case 4:
		if (conn & 0x10)
			entry->type = OUTPUT_LVDS;
		else
			entry->type = OUTPUT_TMDS;
		break;
	case 3:
		entry->type = OUTPUT_LVDS;
		break;
	default:
		NV_ERROR(dev, "Unknown DCB type %d\n", conn & 0x0000000f);
		return false;
	}

	entry->i2c_index = (conn & 0x0003c000) >> 14;
	entry->heads = ((conn & 0x001c0000) >> 18) + 1;
	entry->or = entry->heads; 
	entry->location = (conn & 0x01e00000) >> 21;
	entry->bus = (conn & 0x0e000000) >> 25;
	entry->duallink_possible = false;

	switch (entry->type) {
	case OUTPUT_ANALOG:
		entry->crtconf.maxfreq = (conf & 0xffff) * 10;
		break;
	case OUTPUT_TV:
		entry->tvconf.has_component_output = false;
		break;
	case OUTPUT_LVDS:
		if ((conn & 0x00003f00) >> 8 != 0x10)
			entry->lvdsconf.use_straps_for_mode = true;
		entry->lvdsconf.use_power_scripts = true;
		break;
	default:
		break;
	}

	return true;
}

static
void merge_like_dcb_entries(struct drm_device *dev, struct dcb_table *dcb)
{

	int i, newentries = 0;

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *ient = &dcb->entry[i];
		int j;

		for (j = i + 1; j < dcb->entries; j++) {
			struct dcb_entry *jent = &dcb->entry[j];

			if (jent->type == 100) 
				continue;

			
			if (jent->i2c_index == ient->i2c_index &&
			    jent->type == ient->type &&
			    jent->location == ient->location &&
			    jent->or == ient->or) {
				NV_TRACE(dev, "Merging DCB entries %d and %d\n",
					 i, j);
				ient->heads |= jent->heads;
				jent->type = 100; 
			}
		}
	}

	
	for (i = 0; i < dcb->entries; i++) {
		if (dcb->entry[i].type == 100)
			continue;

		if (newentries != i) {
			dcb->entry[newentries] = dcb->entry[i];
			dcb->entry[newentries].index = newentries;
		}
		newentries++;
	}

	dcb->entries = newentries;
}

static bool
apply_dcb_encoder_quirks(struct drm_device *dev, int idx, u32 *conn, u32 *conf)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct dcb_table *dcb = &dev_priv->vbios.dcb;

	if (nv_match_device(dev, 0x040d, 0x1028, 0x019b)) {
		if (*conn == 0x02026312 && *conf == 0x00000020)
			return false;
	}

	if (nv_match_device(dev, 0x0201, 0x1462, 0x8851)) {
		if (*conn == 0xf2005014 && *conf == 0xffffffff) {
			fabricate_dcb_output(dcb, OUTPUT_TMDS, 1, 1, 1);
			return false;
		}
	}

	if (nv_match_device(dev, 0x0ca3, 0x1682, 0x3003)) {
		if (idx == 0) {
			*conn = 0x02001300; 
			*conf = 0x00000028;
		} else
		if (idx == 1) {
			*conn = 0x01010312; 
			*conf = 0x00020030;
		} else
		if (idx == 2) {
			*conn = 0x01010310; 
			*conf = 0x00000028;
		} else
		if (idx == 3) {
			*conn = 0x02022362; 
			*conf = 0x00020010;
		} else {
			*conn = 0x0000000e; 
			*conf = 0x00000000;
		}
	}

	if (nv_match_device(dev, 0x0615, 0x1682, 0x2605)) {
		if (idx == 0) {
			*conn = 0x02002300; 
			*conf = 0x00000028;
		} else
		if (idx == 1) {
			*conn = 0x01010312; 
			*conf = 0x00020030;
		} else
		if (idx == 2) {
			*conn = 0x04020310; 
			*conf = 0x00000028;
		} else
		if (idx == 3) {
			*conn = 0x02021322; 
			*conf = 0x00020010;
		} else {
			*conn = 0x0000000e; 
			*conf = 0x00000000;
		}
	}

	return true;
}

static void
fabricate_dcb_encoder_table(struct drm_device *dev, struct nvbios *bios)
{
	struct dcb_table *dcb = &bios->dcb;
	int all_heads = (nv_two_heads(dev) ? 3 : 1);

#ifdef __powerpc__
	
	if (of_machine_is_compatible("PowerMac4,5")) {
		fabricate_dcb_output(dcb, OUTPUT_TMDS, 0, all_heads, 1);
		fabricate_dcb_output(dcb, OUTPUT_ANALOG, 1, all_heads, 2);
		return;
	}
#endif

	
	fabricate_dcb_output(dcb, OUTPUT_ANALOG,
			     bios->legacy.i2c_indices.crt, 1, 1);

	if (nv04_tv_identify(dev, bios->legacy.i2c_indices.tv) >= 0)
		fabricate_dcb_output(dcb, OUTPUT_TV,
				     bios->legacy.i2c_indices.tv,
				     all_heads, 0);

	else if (bios->tmds.output0_script_ptr ||
		 bios->tmds.output1_script_ptr)
		fabricate_dcb_output(dcb, OUTPUT_TMDS,
				     bios->legacy.i2c_indices.panel,
				     all_heads, 1);
}

static int
parse_dcb_entry(struct drm_device *dev, void *data, int idx, u8 *outp)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct dcb_table *dcb = &dev_priv->vbios.dcb;
	u32 conf = (dcb->version >= 0x20) ? ROM32(outp[4]) : ROM32(outp[6]);
	u32 conn = ROM32(outp[0]);
	bool ret;

	if (apply_dcb_encoder_quirks(dev, idx, &conn, &conf)) {
		struct dcb_entry *entry = new_dcb_entry(dcb);

		NV_TRACEWARN(dev, "DCB outp %02d: %08x %08x\n", idx, conn, conf);

		if (dcb->version >= 0x20)
			ret = parse_dcb20_entry(dev, dcb, conn, conf, entry);
		else
			ret = parse_dcb15_entry(dev, dcb, conn, conf, entry);
		if (!ret)
			return 1; 

		if (entry->type == OUTPUT_TV &&
		    entry->location == DCB_LOC_ON_CHIP)
			entry->i2c_index = 0x0f;
	}

	return 0;
}

static void
dcb_fake_connectors(struct nvbios *bios)
{
	struct dcb_table *dcbt = &bios->dcb;
	u8 map[16] = { };
	int i, idx = 0;

	if (!nv_match_device(bios->dev, 0x0392, 0x107d, 0x20a2)) {
		for (i = 0; i < dcbt->entries; i++) {
			if (dcbt->entry[i].connector)
				return;
		}
	}

	for (i = 0; i < dcbt->entries; i++) {
		u8 i2c = dcbt->entry[i].i2c_index;
		if (i2c == 0x0f) {
			dcbt->entry[i].connector = idx++;
		} else {
			if (!map[i2c])
				map[i2c] = ++idx;
			dcbt->entry[i].connector = map[i2c] - 1;
		}
	}

	if (i > 1) {
		u8 *conntab = dcb_conntab(bios->dev);
		if (conntab)
			conntab[0] = 0x00;
	}
}

static int
parse_dcb_table(struct drm_device *dev, struct nvbios *bios)
{
	struct dcb_table *dcb = &bios->dcb;
	u8 *dcbt, *conn;
	int idx;

	dcbt = dcb_table(dev);
	if (!dcbt) {
		
		if (bios->type == NVBIOS_BMP) {
			fabricate_dcb_encoder_table(dev, bios);
			return 0;
		}

		return -EINVAL;
	}

	NV_TRACE(dev, "DCB version %d.%d\n", dcbt[0] >> 4, dcbt[0] & 0xf);

	dcb->version = dcbt[0];
	dcb_outp_foreach(dev, NULL, parse_dcb_entry);

	if (dcb->version < 0x21)
		merge_like_dcb_entries(dev, dcb);

	if (!dcb->entries)
		return -ENXIO;

	
	idx = -1;
	while ((conn = dcb_conn(dev, ++idx))) {
		if (conn[0] != 0xff) {
			NV_TRACE(dev, "DCB conn %02d: ", idx);
			if (dcb_conntab(dev)[3] < 4)
				printk("%04x\n", ROM16(conn[0]));
			else
				printk("%08x\n", ROM32(conn[0]));
		}
	}
	dcb_fake_connectors(bios);
	return 0;
}

static int load_nv17_hwsq_ucode_entry(struct drm_device *dev, struct nvbios *bios, uint16_t hwsq_offset, int entry)
{
	/*
	 * The header following the "HWSQ" signature has the number of entries,
	 * and the entry size
	 *
	 * An entry consists of a dword to write to the sequencer control reg
	 * (0x00001304), followed by the ucode bytes, written sequentially,
	 * starting at reg 0x00001400
	 */

	uint8_t bytes_to_write;
	uint16_t hwsq_entry_offset;
	int i;

	if (bios->data[hwsq_offset] <= entry) {
		NV_ERROR(dev, "Too few entries in HW sequencer table for "
				"requested entry\n");
		return -ENOENT;
	}

	bytes_to_write = bios->data[hwsq_offset + 1];

	if (bytes_to_write != 36) {
		NV_ERROR(dev, "Unknown HW sequencer entry size\n");
		return -EINVAL;
	}

	NV_TRACE(dev, "Loading NV17 power sequencing microcode\n");

	hwsq_entry_offset = hwsq_offset + 2 + entry * bytes_to_write;

	
	bios_wr32(bios, 0x00001304, ROM32(bios->data[hwsq_entry_offset]));
	bytes_to_write -= 4;

	
	for (i = 0; i < bytes_to_write; i += 4)
		bios_wr32(bios, 0x00001400 + i, ROM32(bios->data[hwsq_entry_offset + i + 4]));

	
	bios_wr32(bios, NV_PBUS_DEBUG_4, bios_rd32(bios, NV_PBUS_DEBUG_4) | 0x18);

	return 0;
}

static int load_nv17_hw_sequencer_ucode(struct drm_device *dev,
					struct nvbios *bios)
{

	const uint8_t hwsq_signature[] = { 'H', 'W', 'S', 'Q' };
	const int sz = sizeof(hwsq_signature);
	int hwsq_offset;

	hwsq_offset = findstr(bios->data, bios->length, hwsq_signature, sz);
	if (!hwsq_offset)
		return 0;

	
	return load_nv17_hwsq_ucode_entry(dev, bios, hwsq_offset + sz, 0);
}

uint8_t *nouveau_bios_embedded_edid(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	const uint8_t edid_sig[] = {
			0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
	uint16_t offset = 0;
	uint16_t newoffset;
	int searchlen = NV_PROM_SIZE;

	if (bios->fp.edid)
		return bios->fp.edid;

	while (searchlen) {
		newoffset = findstr(&bios->data[offset], searchlen,
								edid_sig, 8);
		if (!newoffset)
			return NULL;
		offset += newoffset;
		if (!nv_cksum(&bios->data[offset], EDID1_LEN))
			break;

		searchlen -= offset;
		offset++;
	}

	NV_TRACE(dev, "Found EDID in BIOS\n");

	return bios->fp.edid = &bios->data[offset];
}

void
nouveau_bios_run_init_table(struct drm_device *dev, uint16_t table,
			    struct dcb_entry *dcbent, int crtc)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	struct init_exec iexec = { true, false };

	spin_lock_bh(&bios->lock);
	bios->display.output = dcbent;
	bios->display.crtc = crtc;
	parse_init_table(bios, table, &iexec);
	bios->display.output = NULL;
	spin_unlock_bh(&bios->lock);
}

void
nouveau_bios_init_exec(struct drm_device *dev, uint16_t table)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	struct init_exec iexec = { true, false };

	parse_init_table(bios, table, &iexec);
}

static bool NVInitVBIOS(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;

	memset(bios, 0, sizeof(struct nvbios));
	spin_lock_init(&bios->lock);
	bios->dev = dev;

	return bios_shadow(dev);
}

static int nouveau_parse_vbios_struct(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	const uint8_t bit_signature[] = { 0xff, 0xb8, 'B', 'I', 'T' };
	const uint8_t bmp_signature[] = { 0xff, 0x7f, 'N', 'V', 0x0 };
	int offset;

	offset = findstr(bios->data, bios->length,
					bit_signature, sizeof(bit_signature));
	if (offset) {
		NV_TRACE(dev, "BIT BIOS found\n");
		bios->type = NVBIOS_BIT;
		bios->offset = offset;
		return parse_bit_structure(bios, offset + 6);
	}

	offset = findstr(bios->data, bios->length,
					bmp_signature, sizeof(bmp_signature));
	if (offset) {
		NV_TRACE(dev, "BMP BIOS found\n");
		bios->type = NVBIOS_BMP;
		bios->offset = offset;
		return parse_bmp_structure(dev, bios, offset);
	}

	NV_ERROR(dev, "No known BIOS signature found\n");
	return -ENODEV;
}

int
nouveau_run_vbios_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	int i, ret = 0;

	
	bios->state.crtchead = 0;

	if (bios->major_version < 5)	
		load_nv17_hw_sequencer_ucode(dev, bios);

	if (bios->execute) {
		bios->fp.last_script_invoc = 0;
		bios->fp.lvds_init_run = false;
	}

	parse_init_tables(bios);

	if (bios->some_script_ptr) {
		struct init_exec iexec = {true, false};

		NV_INFO(dev, "Parsing VBIOS init table at offset 0x%04X\n",
			bios->some_script_ptr);
		parse_init_table(bios, bios->some_script_ptr, &iexec);
	}

	if (dev_priv->card_type >= NV_50) {
		for (i = 0; i < bios->dcb.entries; i++) {
			nouveau_bios_run_display_table(dev, 0, 0,
						       &bios->dcb.entry[i], -1);
		}
	}

	return ret;
}

static bool
nouveau_bios_posted(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	unsigned htotal;

	if (dev_priv->card_type >= NV_50) {
		if (NVReadVgaCrtc(dev, 0, 0x00) == 0 &&
		    NVReadVgaCrtc(dev, 0, 0x1a) == 0)
			return false;
		return true;
	}

	htotal  = NVReadVgaCrtc(dev, 0, 0x06);
	htotal |= (NVReadVgaCrtc(dev, 0, 0x07) & 0x01) << 8;
	htotal |= (NVReadVgaCrtc(dev, 0, 0x07) & 0x20) << 4;
	htotal |= (NVReadVgaCrtc(dev, 0, 0x25) & 0x01) << 10;
	htotal |= (NVReadVgaCrtc(dev, 0, 0x41) & 0x01) << 11;

	return (htotal != 0);
}

int
nouveau_bios_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nvbios *bios = &dev_priv->vbios;
	int ret;

	if (!NVInitVBIOS(dev))
		return -ENODEV;

	ret = nouveau_parse_vbios_struct(dev);
	if (ret)
		return ret;

	ret = nouveau_i2c_init(dev);
	if (ret)
		return ret;

	ret = nouveau_mxm_init(dev);
	if (ret)
		return ret;

	ret = parse_dcb_table(dev, bios);
	if (ret)
		return ret;

	if (!bios->major_version)	
		return 0;

	
	bios->execute = false;

	
	if (!nouveau_bios_posted(dev)) {
		NV_INFO(dev, "Adaptor not initialised, "
			"running VBIOS init tables.\n");
		bios->execute = true;
	}
	if (nouveau_force_post)
		bios->execute = true;

	ret = nouveau_run_vbios_init(dev);
	if (ret)
		return ret;

	
	if (bios->major_version < 5)
		bios->is_mobile = NVReadVgaCrtc(dev, 0, NV_CIO_CRE_4B) & 0x40;

	
	if (bios->is_mobile || bios->major_version >= 5)
		ret = parse_fp_mode_table(dev, bios);

	
	bios->execute = true;

	return 0;
}

void
nouveau_bios_takedown(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	nouveau_mxm_fini(dev);
	nouveau_i2c_fini(dev);

	kfree(dev_priv->vbios.data);
}
