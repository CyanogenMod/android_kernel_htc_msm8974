/*
* cycx_drv.c	Cyclom 2X Support Module.
*
*		This module is a library of common hardware specific
*		functions used by the Cyclades Cyclom 2X sync card.
*
* Author:	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
*
* Copyright:	(c) 1998-2003 Arnaldo Carvalho de Melo
*
* Based on sdladrv.c by Gene Kozin <genek@compuserve.com>
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* 1999/11/11	acme		set_current_state(TASK_INTERRUPTIBLE), code
*				cleanup
* 1999/11/08	acme		init_cyc2x deleted, doing nothing
* 1999/11/06	acme		back to read[bw], write[bw] and memcpy_to and
*				fromio to use dpmbase ioremaped
* 1999/10/26	acme		use isa_read[bw], isa_write[bw] & isa_memcpy_to
*				& fromio
* 1999/10/23	acme		cleanup to only supports cyclom2x: all the other
*				boards are no longer manufactured by cyclades,
*				if someone wants to support them... be my guest!
* 1999/05/28    acme		cycx_intack & cycx_intde gone for good
* 1999/05/18	acme		lots of unlogged work, submitting to Linus...
* 1999/01/03	acme		more judicious use of data types
* 1999/01/03	acme		judicious use of data types :>
*				cycx_inten trying to reset pending interrupts
*				from cyclom 2x - I think this isn't the way to
*				go, but for now...
* 1999/01/02	acme		cycx_intack ok, I think there's nothing to do
*				to ack an int in cycx_drv.c, only handle it in
*				cyx_isr (or in the other protocols: cyp_isr,
*				cyf_isr, when they get implemented.
* Dec 31, 1998	acme		cycx_data_boot & cycx_code_boot fixed, crossing
*				fingers to see x25_configure in cycx_x25.c
*				work... :)
* Dec 26, 1998	acme		load implementation fixed, seems to work! :)
*				cycx_2x_dpmbase_options with all the possible
*				DPM addresses (20).
*				cycx_intr implemented (test this!)
*				general code cleanup
* Dec  8, 1998	Ivan Passos	Cyclom-2X firmware load implementation.
* Aug  8, 1998	acme		Initial version.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>		
#include <linux/module.h>
#include <linux/kernel.h>	
#include <linux/stddef.h>	
#include <linux/errno.h>	
#include <linux/cycx_drv.h>	
#include <linux/cycx_cfm.h>	
#include <linux/delay.h>	
#include <asm/io.h>		

#define	MOD_VERSION	0
#define	MOD_RELEASE	6

MODULE_AUTHOR("Arnaldo Carvalho de Melo");
MODULE_DESCRIPTION("Cyclom 2x Sync Card Driver");
MODULE_LICENSE("GPL");

static int load_cyc2x(struct cycx_hw *hw, struct cycx_firmware *cfm, u32 len);
static void cycx_bootcfg(struct cycx_hw *hw);

static int reset_cyc2x(void __iomem *addr);
static int detect_cyc2x(void __iomem *addr);

static int get_option_index(const long *optlist, long optval);
static u16 checksum(u8 *buf, u32 len);

#define wait_cyc(addr) cycx_exec(addr + CMD_OFFSET)


static const char fullname[] = "Cyclom 2X Support Module";
static const char copyright[] =
	"(c) 1998-2003 Arnaldo Carvalho de Melo <acme@conectiva.com.br>";

static const long cyc2x_dpmbase_options[] = {
	20,
	0xA0000, 0xA4000, 0xA8000, 0xAC000, 0xB0000, 0xB4000, 0xB8000,
	0xBC000, 0xC0000, 0xC4000, 0xC8000, 0xCC000, 0xD0000, 0xD4000,
	0xD8000, 0xDC000, 0xE0000, 0xE4000, 0xE8000, 0xEC000
};

static const long cycx_2x_irq_options[]  = { 7, 3, 5, 9, 10, 11, 12, 15 };


static int __init cycx_drv_init(void)
{
	pr_info("%s v%u.%u %s\n",
		fullname, MOD_VERSION, MOD_RELEASE, copyright);

	return 0;
}

static void cycx_drv_cleanup(void)
{
}

EXPORT_SYMBOL(cycx_setup);
int cycx_setup(struct cycx_hw *hw, void *cfm, u32 len, unsigned long dpmbase)
{
	int err;

	
	if (!get_option_index(cycx_2x_irq_options, hw->irq)) {
		pr_err("IRQ %d is invalid!\n", hw->irq);
		return -EINVAL;
	}

	
	if (!dpmbase) {
		pr_err("you must specify the dpm address!\n");
 		return -EINVAL;
	} else if (!get_option_index(cyc2x_dpmbase_options, dpmbase)) {
		pr_err("memory address 0x%lX is invalid!\n", dpmbase);
		return -EINVAL;
	}

	hw->dpmbase = ioremap(dpmbase, CYCX_WINDOWSIZE);
	hw->dpmsize = CYCX_WINDOWSIZE;

	if (!detect_cyc2x(hw->dpmbase)) {
		pr_err("adapter Cyclom 2X not found at address 0x%lX!\n",
		       dpmbase);
		return -EINVAL;
	}

	pr_info("found Cyclom 2X card at address 0x%lX\n", dpmbase);

	
	err = load_cyc2x(hw, cfm, len);

	if (err)
		cycx_down(hw);         

	return err;
}

EXPORT_SYMBOL(cycx_down);
int cycx_down(struct cycx_hw *hw)
{
	iounmap(hw->dpmbase);
	return 0;
}

static void cycx_inten(struct cycx_hw *hw)
{
	writeb(0, hw->dpmbase);
}

EXPORT_SYMBOL(cycx_intr);
void cycx_intr(struct cycx_hw *hw)
{
	writew(0, hw->dpmbase + GEN_CYCX_INTR);
}

EXPORT_SYMBOL(cycx_exec);
int cycx_exec(void __iomem *addr)
{
	u16 i = 0;
	

	while (readw(addr)) {
		udelay(1000);

		if (++i > 50)
			return -1;
	}

	return 0;
}

EXPORT_SYMBOL(cycx_peek);
int cycx_peek(struct cycx_hw *hw, u32 addr, void *buf, u32 len)
{
	if (len == 1)
		*(u8*)buf = readb(hw->dpmbase + addr);
	else
		memcpy_fromio(buf, hw->dpmbase + addr, len);

	return 0;
}

EXPORT_SYMBOL(cycx_poke);
int cycx_poke(struct cycx_hw *hw, u32 addr, void *buf, u32 len)
{
	if (len == 1)
		writeb(*(u8*)buf, hw->dpmbase + addr);
	else
		memcpy_toio(hw->dpmbase + addr, buf, len);

	return 0;
}


static int memory_exists(void __iomem *addr)
{
	int tries = 0;

	for (; tries < 3 ; tries++) {
		writew(TEST_PATTERN, addr + 0x10);

		if (readw(addr + 0x10) == TEST_PATTERN)
			if (readw(addr + 0x10) == TEST_PATTERN)
				return 1;

		msleep_interruptible(1 * 1000);
	}

	return 0;
}

static void reset_load(void __iomem *addr, u8 *buffer, u32 cnt)
{
	void __iomem *pt_code = addr + RESET_OFFSET;
	u16 i; 

	for (i = 0 ; i < cnt ; i++) {
		writeb(*buffer++, pt_code++);
	}
}

static int buffer_load(void __iomem *addr, u8 *buffer, u32 cnt)
{
	memcpy_toio(addr + DATA_OFFSET, buffer, cnt);
	writew(GEN_BOOT_DAT, addr + CMD_OFFSET);

	return wait_cyc(addr);
}

static void cycx_start(void __iomem *addr)
{
	
	writeb(0xea, addr + 0x30);
	writeb(0x00, addr + 0x31);
	writeb(0xc4, addr + 0x32);
	writeb(0x00, addr + 0x33);
	writeb(0x00, addr + 0x34);

	
	writew(GEN_START, addr + CMD_OFFSET);
}

static void cycx_reset_boot(void __iomem *addr, u8 *code, u32 len)
{
	void __iomem *pt_start = addr + START_OFFSET;

	writeb(0xea, pt_start++); 
	writeb(0x00, pt_start++);
	writeb(0xfc, pt_start++);
	writeb(0x00, pt_start++);
	writeb(0xf0, pt_start);
	reset_load(addr, code, len);

	
	writeb(0, addr + START_CPU);
	msleep_interruptible(1 * 1000);
}

static int cycx_data_boot(void __iomem *addr, u8 *code, u32 len)
{
	void __iomem *pt_boot_cmd = addr + CMD_OFFSET;
	u32 i;

	
	writew(CFM_LOAD_BUFSZ, pt_boot_cmd + sizeof(u16));
	writew(GEN_DEFPAR, pt_boot_cmd);

	if (wait_cyc(addr) < 0)
		return -1;

	writew(0, pt_boot_cmd + sizeof(u16));
	writew(0x4000, pt_boot_cmd + 2 * sizeof(u16));
	writew(GEN_SET_SEG, pt_boot_cmd);

	if (wait_cyc(addr) < 0)
		return -1;

	for (i = 0 ; i < len ; i += CFM_LOAD_BUFSZ)
		if (buffer_load(addr, code + i,
				min_t(u32, CFM_LOAD_BUFSZ, (len - i))) < 0) {
			pr_err("Error !!\n");
			return -1;
		}

	return 0;
}


static int cycx_code_boot(void __iomem *addr, u8 *code, u32 len)
{
	void __iomem *pt_boot_cmd = addr + CMD_OFFSET;
	u32 i;

	
	writew(CFM_LOAD_BUFSZ, pt_boot_cmd + sizeof(u16));
	writew(GEN_DEFPAR, pt_boot_cmd);

	if (wait_cyc(addr) < 0)
		return -1;

	writew(0x0000, pt_boot_cmd + sizeof(u16));
	writew(0xc400, pt_boot_cmd + 2 * sizeof(u16));
	writew(GEN_SET_SEG, pt_boot_cmd);

	if (wait_cyc(addr) < 0)
		return -1;

	for (i = 0 ; i < len ; i += CFM_LOAD_BUFSZ)
		if (buffer_load(addr, code + i,
				min_t(u32, CFM_LOAD_BUFSZ, (len - i)))) {
			pr_err("Error !!\n");
			return -1;
		}

	return 0;
}

static int load_cyc2x(struct cycx_hw *hw, struct cycx_firmware *cfm, u32 len)
{
	int i, j;
	struct cycx_fw_header *img_hdr;
	u8 *reset_image,
	   *data_image,
	   *code_image;
	void __iomem *pt_cycld = hw->dpmbase + 0x400;
	u16 cksum;

	
	pr_info("firmware signature=\"%s\"\n", cfm->signature);

	
	if (strcmp(cfm->signature, CFM_SIGNATURE)) {
		pr_err("load_cyc2x: not Cyclom-2X firmware!\n");
		return -EINVAL;
	}

	pr_info("firmware version=%u\n", cfm->version);

	
	if (cfm->version != CFM_VERSION) {
		pr_err("%s: firmware format %u rejected! Expecting %u.\n",
		       __func__, cfm->version, CFM_VERSION);
		return -EINVAL;
	}

	
	cksum = checksum((u8*)&cfm->info, sizeof(struct cycx_fw_info) +
					  cfm->info.codesize);
	if (cksum != cfm->checksum) {
		pr_err("%s: firmware corrupted!\n", __func__);
		pr_err(" cdsize = 0x%x (expected 0x%lx)\n",
		       len - (int)sizeof(struct cycx_firmware) - 1,
		       cfm->info.codesize);
		pr_err(" chksum = 0x%x (expected 0x%x)\n",
		       cksum, cfm->checksum);
		return -EINVAL;
	}

	
	img_hdr = (struct cycx_fw_header *)&cfm->image;
#ifdef FIRMWARE_DEBUG
	pr_info("%s: image sizes\n", __func__);
	pr_info(" reset=%lu\n", img_hdr->reset_size);
	pr_info("  data=%lu\n", img_hdr->data_size);
	pr_info("  code=%lu\n", img_hdr->code_size);
#endif
	reset_image = ((u8 *)img_hdr) + sizeof(struct cycx_fw_header);
	data_image = reset_image + img_hdr->reset_size;
	code_image = data_image + img_hdr->data_size;

	
	
	pr_info("loading firmware %s (ID=%u)...\n",
		cfm->descr[0] ? cfm->descr : "unknown firmware",
		cfm->info.codeid);

	for (i = 0 ; i < 5 ; i++) {
		
		if (!reset_cyc2x(hw->dpmbase)) {
			pr_err("dpm problem or board not found\n");
			return -EINVAL;
		}

		
		cycx_reset_boot(hw->dpmbase, reset_image, img_hdr->reset_size);
		
		writew(GEN_POWER_ON, pt_cycld);
		msleep_interruptible(1 * 1000);

		for (j = 0 ; j < 3 ; j++)
			if (!readw(pt_cycld))
				goto reset_loaded;
			else
				msleep_interruptible(1 * 1000);
	}

	pr_err("reset not started\n");
	return -EINVAL;

reset_loaded:
	
	if (cycx_data_boot(hw->dpmbase, data_image, img_hdr->data_size)) {
		pr_err("cannot load data file\n");
		return -EINVAL;
	}

	
	if (cycx_code_boot(hw->dpmbase, code_image, img_hdr->code_size)) {
		pr_err("cannot load code file\n");
		return -EINVAL;
	}

	
	cycx_bootcfg(hw);

	
	cycx_start(hw->dpmbase);

	msleep_interruptible(7 * 1000);
	pr_info("firmware loaded!\n");

	
	cycx_inten(hw);

	return 0;
}

static void cycx_bootcfg(struct cycx_hw *hw)
{
	
	writeb(FIXED_BUFFERS, hw->dpmbase + CONF_OFFSET);
}

static int detect_cyc2x(void __iomem *addr)
{
	reset_cyc2x(addr);

	return memory_exists(addr);
}

static int get_option_index(const long *optlist, long optval)
{
	int i = 1;

	for (; i <= optlist[0]; ++i)
		if (optlist[i] == optval)
			return i;

	return 0;
}

static int reset_cyc2x(void __iomem *addr)
{
	writeb(0, addr + RST_ENABLE);
	msleep_interruptible(2 * 1000);
	writeb(0, addr + RST_DISABLE);
	msleep_interruptible(2 * 1000);

	return memory_exists(addr);
}

static u16 checksum(u8 *buf, u32 len)
{
	u16 crc = 0;
	u16 mask, flag;

	for (; len; --len, ++buf)
		for (mask = 0x80; mask; mask >>= 1) {
			flag = (crc & 0x8000);
			crc <<= 1;
			crc |= ((*buf & mask) ? 1 : 0);

			if (flag)
				crc ^= 0x1021;
		}

	return crc;
}

module_init(cycx_drv_init);
module_exit(cycx_drv_cleanup);

