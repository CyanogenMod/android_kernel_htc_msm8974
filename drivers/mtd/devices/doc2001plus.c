/*
 * Linux driver for Disk-On-Chip Millennium Plus
 *
 * (c) 2002-2003 Greg Ungerer <gerg@snapgear.com>
 * (c) 2002-2003 SnapGear Inc
 * (c) 1999 Machine Vision Holdings, Inc.
 * (c) 1999, 2000 David Woodhouse <dwmw2@infradead.org>
 *
 * Released under GPL
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/bitops.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/doc2000.h>


#undef USE_MEMCPY

static int doc_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf);
static int doc_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, const u_char *buf);
static int doc_read_oob(struct mtd_info *mtd, loff_t ofs,
			struct mtd_oob_ops *ops);
static int doc_write_oob(struct mtd_info *mtd, loff_t ofs,
			 struct mtd_oob_ops *ops);
static int doc_erase (struct mtd_info *mtd, struct erase_info *instr);

static struct mtd_info *docmilpluslist = NULL;


static void DoC_Delay(void __iomem * docptr, int cycles)
{
	int i;

	for (i = 0; (i < cycles); i++)
		WriteDOC(0, docptr, Mplus_NOP);
}

#define	CDSN_CTRL_FR_B_MASK	(CDSN_CTRL_FR_B0 | CDSN_CTRL_FR_B1)

static int _DoC_WaitReady(void __iomem * docptr)
{
	unsigned int c = 0xffff;

	pr_debug("_DoC_WaitReady called for out-of-line wait\n");

	
	while (((ReadDOC(docptr, Mplus_FlashControl) & CDSN_CTRL_FR_B_MASK) != CDSN_CTRL_FR_B_MASK) && --c)
		;

	if (c == 0)
		pr_debug("_DoC_WaitReady timed out.\n");

	return (c == 0);
}

static inline int DoC_WaitReady(void __iomem * docptr)
{
	
	int ret = 0;

	DoC_Delay(docptr, 4);

	if ((ReadDOC(docptr, Mplus_FlashControl) & CDSN_CTRL_FR_B_MASK) != CDSN_CTRL_FR_B_MASK)
		
		ret = _DoC_WaitReady(docptr);

	return ret;
}

static inline void DoC_CheckASIC(void __iomem * docptr)
{
	
	if ((ReadDOC(docptr, Mplus_DOCControl) & DOC_MODE_NORMAL) == 0) {
		WriteDOC((DOC_MODE_NORMAL | DOC_MODE_MDWREN), docptr, Mplus_DOCControl);
		WriteDOC(~(DOC_MODE_NORMAL | DOC_MODE_MDWREN), docptr, Mplus_CtrlConfirm);
	}
}

static void DoC_Command(void __iomem * docptr, unsigned char command,
			       unsigned char xtraflags)
{
	WriteDOC(command, docptr, Mplus_FlashCmd);
	WriteDOC(command, docptr, Mplus_WritePipeTerm);
	WriteDOC(command, docptr, Mplus_WritePipeTerm);
}

static inline void DoC_Address(struct DiskOnChip *doc, int numbytes,
			       unsigned long ofs, unsigned char xtraflags1,
			       unsigned char xtraflags2)
{
	void __iomem * docptr = doc->virtadr;

	
	ofs >>= doc->interleave;

	switch (numbytes) {
	case 1:
		
		WriteDOC(ofs & 0xff, docptr, Mplus_FlashAddress);
		break;
	case 2:
		
		WriteDOC((ofs >> 9)  & 0xff, docptr, Mplus_FlashAddress);
		WriteDOC((ofs >> 17) & 0xff, docptr, Mplus_FlashAddress);
		break;
	case 3:
		
		WriteDOC(ofs & 0xff, docptr, Mplus_FlashAddress);
		WriteDOC((ofs >> 9)  & 0xff, docptr, Mplus_FlashAddress);
		WriteDOC((ofs >> 17) & 0xff, docptr, Mplus_FlashAddress);
		break;
	default:
		return;
	}

	WriteDOC(0x00, docptr, Mplus_WritePipeTerm);
	WriteDOC(0x00, docptr, Mplus_WritePipeTerm);
}

static int DoC_SelectChip(void __iomem * docptr, int chip)
{
	
	return 0;
}

static int DoC_SelectFloor(void __iomem * docptr, int floor)
{
	WriteDOC((floor & 0x3), docptr, Mplus_DeviceSelect);
	return 0;
}

static unsigned int DoC_GetDataOffset(struct mtd_info *mtd, loff_t *from)
{
	struct DiskOnChip *this = mtd->priv;

	if (this->interleave) {
		unsigned int ofs = *from & 0x3ff;
		unsigned int cmd;

		if (ofs < 512) {
			cmd = NAND_CMD_READ0;
			ofs &= 0x1ff;
		} else if (ofs < 1014) {
			cmd = NAND_CMD_READ1;
			ofs = (ofs & 0x1ff) + 10;
		} else {
			cmd = NAND_CMD_READOOB;
			ofs = ofs - 1014;
		}

		*from = (*from & ~0x3ff) | ofs;
		return cmd;
	} else {
		
		if ((*from) & 0x100)
			return NAND_CMD_READ1;
		return NAND_CMD_READ0;
	}
}

static unsigned int DoC_GetECCOffset(struct mtd_info *mtd, loff_t *from)
{
	unsigned int ofs, cmd;

	if (*from & 0x200) {
		cmd = NAND_CMD_READOOB;
		ofs = 10 + (*from & 0xf);
	} else {
		cmd = NAND_CMD_READ1;
		ofs = (*from & 0xf);
	}

	*from = (*from & ~0x3ff) | ofs;
	return cmd;
}

static unsigned int DoC_GetFlagsOffset(struct mtd_info *mtd, loff_t *from)
{
	unsigned int ofs, cmd;

	cmd = NAND_CMD_READ1;
	ofs = (*from & 0x200) ? 8 : 6;
	*from = (*from & ~0x3ff) | ofs;
	return cmd;
}

static unsigned int DoC_GetHdrOffset(struct mtd_info *mtd, loff_t *from)
{
	unsigned int ofs, cmd;

	cmd = NAND_CMD_READOOB;
	ofs = (*from & 0x200) ? 24 : 16;
	*from = (*from & ~0x3ff) | ofs;
	return cmd;
}

static inline void MemReadDOC(void __iomem * docptr, unsigned char *buf, int len)
{
#ifndef USE_MEMCPY
	int i;
	for (i = 0; i < len; i++)
		buf[i] = ReadDOC(docptr, Mil_CDSN_IO + i);
#else
	memcpy_fromio(buf, docptr + DoC_Mil_CDSN_IO, len);
#endif
}

static inline void MemWriteDOC(void __iomem * docptr, unsigned char *buf, int len)
{
#ifndef USE_MEMCPY
	int i;
	for (i = 0; i < len; i++)
		WriteDOC(buf[i], docptr, Mil_CDSN_IO + i);
#else
	memcpy_toio(docptr + DoC_Mil_CDSN_IO, buf, len);
#endif
}

static int DoC_IdentChip(struct DiskOnChip *doc, int floor, int chip)
{
	int mfr, id, i, j;
	volatile char dummy;
	void __iomem * docptr = doc->virtadr;

	
	DoC_SelectFloor(docptr, floor);
	DoC_SelectChip(docptr, chip);

	
	WriteDOC((DOC_FLASH_CE | DOC_FLASH_WP), docptr, Mplus_FlashSelect);

	
	DoC_Command(docptr, NAND_CMD_RESET, 0);
	DoC_WaitReady(docptr);

	
	DoC_Command(docptr, NAND_CMD_READID, 0);

	
	DoC_Address(doc, 1, 0x00, 0, 0x00);

	WriteDOC(0, docptr, Mplus_FlashControl);
	DoC_WaitReady(docptr);

	dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
	dummy = ReadDOC(docptr, Mplus_ReadPipeInit);

	mfr = ReadDOC(docptr, Mil_CDSN_IO);
	if (doc->interleave)
		dummy = ReadDOC(docptr, Mil_CDSN_IO); 

	id  = ReadDOC(docptr, Mil_CDSN_IO);
	if (doc->interleave)
		dummy = ReadDOC(docptr, Mil_CDSN_IO); 

	dummy = ReadDOC(docptr, Mplus_LastDataRead);
	dummy = ReadDOC(docptr, Mplus_LastDataRead);

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	
	if (mfr == 0xff || mfr == 0)
		return 0;

	for (i = 0; nand_flash_ids[i].name != NULL; i++) {
		if (id == nand_flash_ids[i].id) {
			
			for (j = 0; nand_manuf_ids[j].id != 0x0; j++) {
				if (nand_manuf_ids[j].id == mfr)
					break;
			}
			printk(KERN_INFO "Flash chip found: Manufacturer ID: %2.2X, "
			       "Chip ID: %2.2X (%s:%s)\n", mfr, id,
			       nand_manuf_ids[j].name, nand_flash_ids[i].name);
			doc->mfr = mfr;
			doc->id = id;
			doc->chipshift = ffs((nand_flash_ids[i].chipsize << 20)) - 1;
			doc->erasesize = nand_flash_ids[i].erasesize << doc->interleave;
			break;
		}
	}

	if (nand_flash_ids[i].name == NULL)
		return 0;
	return 1;
}

static void DoC_ScanChips(struct DiskOnChip *this)
{
	int floor, chip;
	int numchips[MAX_FLOORS_MPLUS];
	int ret;

	this->numchips = 0;
	this->mfr = 0;
	this->id = 0;

	
	this->interleave = 0;
	if (this->ChipID == DOC_ChipID_DocMilPlus32)
		this->interleave = 1;

	
	if ( (this->interleave << 2) !=
	     (ReadDOC(this->virtadr, Mplus_Configuration) & 4)) {
		u_char conf = ReadDOC(this->virtadr, Mplus_Configuration);
		printk(KERN_NOTICE "Setting DiskOnChip Millennium Plus interleave to %s\n",
		       this->interleave?"on (16-bit)":"off (8-bit)");
		conf ^= 4;
		WriteDOC(conf, this->virtadr, Mplus_Configuration);
	}

	
	for (floor = 0,ret = 1; floor < MAX_FLOORS_MPLUS; floor++) {
		numchips[floor] = 0;
		for (chip = 0; chip < MAX_CHIPS_MPLUS && ret != 0; chip++) {
			ret = DoC_IdentChip(this, floor, chip);
			if (ret) {
				numchips[floor]++;
				this->numchips++;
			}
		}
	}
	
	if (!this->numchips) {
		printk("No flash chips recognised.\n");
		return;
	}

	
	this->chips = kmalloc(sizeof(struct Nand) * this->numchips, GFP_KERNEL);
	if (!this->chips){
		printk("MTD: No memory for allocating chip info structures\n");
		return;
	}

	for (floor = 0, ret = 0; floor < MAX_FLOORS_MPLUS; floor++) {
		for (chip = 0 ; chip < numchips[floor] ; chip++) {
			this->chips[ret].floor = floor;
			this->chips[ret].chip = chip;
			this->chips[ret].curadr = 0;
			this->chips[ret].curmode = 0x50;
			ret++;
		}
	}

	
	this->totlen = this->numchips * (1 << this->chipshift);
	printk(KERN_INFO "%d flash chips found. Total DiskOnChip size: %ld MiB\n",
	       this->numchips ,this->totlen >> 20);
}

static int DoCMilPlus_is_alias(struct DiskOnChip *doc1, struct DiskOnChip *doc2)
{
	int tmp1, tmp2, retval;

	if (doc1->physadr == doc2->physadr)
		return 1;

	tmp1 = ReadDOC(doc1->virtadr, Mplus_AliasResolution);
	tmp2 = ReadDOC(doc2->virtadr, Mplus_AliasResolution);
	if (tmp1 != tmp2)
		return 0;

	WriteDOC((tmp1+1) % 0xff, doc1->virtadr, Mplus_AliasResolution);
	tmp2 = ReadDOC(doc2->virtadr, Mplus_AliasResolution);
	if (tmp2 == (tmp1+1) % 0xff)
		retval = 1;
	else
		retval = 0;

	WriteDOC(tmp1, doc1->virtadr, Mplus_AliasResolution);

	return retval;
}

void DoCMilPlus_init(struct mtd_info *mtd)
{
	struct DiskOnChip *this = mtd->priv;
	struct DiskOnChip *old = NULL;

	
	if (docmilpluslist)
		old = docmilpluslist->priv;

	while (old) {
		if (DoCMilPlus_is_alias(this, old)) {
			printk(KERN_NOTICE "Ignoring DiskOnChip Millennium "
				"Plus at 0x%lX - already configured\n",
				this->physadr);
			iounmap(this->virtadr);
			kfree(mtd);
			return;
		}
		if (old->nextdoc)
			old = old->nextdoc->priv;
		else
			old = NULL;
	}

	mtd->name = "DiskOnChip Millennium Plus";
	printk(KERN_NOTICE "DiskOnChip Millennium Plus found at "
		"address 0x%lX\n", this->physadr);

	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->writebufsize = mtd->writesize = 512;
	mtd->oobsize = 16;
	mtd->ecc_strength = 2;
	mtd->owner = THIS_MODULE;
	mtd->_erase = doc_erase;
	mtd->_read = doc_read;
	mtd->_write = doc_write;
	mtd->_read_oob = doc_read_oob;
	mtd->_write_oob = doc_write_oob;
	this->curfloor = -1;
	this->curchip = -1;

	
	DoC_ScanChips(this);

	if (!this->totlen) {
		kfree(mtd);
		iounmap(this->virtadr);
	} else {
		this->nextdoc = docmilpluslist;
		docmilpluslist = mtd;
		mtd->size  = this->totlen;
		mtd->erasesize = this->erasesize;
		mtd_device_register(mtd, NULL, 0);
		return;
	}
}
EXPORT_SYMBOL_GPL(DoCMilPlus_init);

#if 0
static int doc_dumpblk(struct mtd_info *mtd, loff_t from)
{
	int i;
	loff_t fofs;
	struct DiskOnChip *this = mtd->priv;
	void __iomem * docptr = this->virtadr;
	struct Nand *mychip = &this->chips[from >> (this->chipshift)];
	unsigned char *bp, buf[1056];
	char c[32];

	from &= ~0x3ff;

	
	if (from >= this->totlen)
		return -EINVAL;

	DoC_CheckASIC(docptr);

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC((DOC_FLASH_CE | DOC_FLASH_WP), docptr, Mplus_FlashSelect);

	
	DoC_Command(docptr, NAND_CMD_RESET, 0);
	DoC_WaitReady(docptr);

	fofs = from;
	DoC_Command(docptr, DoC_GetDataOffset(mtd, &fofs), 0);
	DoC_Address(this, 3, fofs, 0, 0x00);
	WriteDOC(0, docptr, Mplus_FlashControl);
	DoC_WaitReady(docptr);

	
	WriteDOC(DOC_ECC_RESET, docptr, Mplus_ECCConf);

	ReadDOC(docptr, Mplus_ReadPipeInit);
	ReadDOC(docptr, Mplus_ReadPipeInit);

	MemReadDOC(docptr, buf, 1054);
	buf[1054] = ReadDOC(docptr, Mplus_LastDataRead);
	buf[1055] = ReadDOC(docptr, Mplus_LastDataRead);

	memset(&c[0], 0, sizeof(c));
	printk("DUMP OFFSET=%x:\n", (int)from);

        for (i = 0, bp = &buf[0]; (i < 1056); i++) {
                if ((i % 16) == 0)
                        printk("%08x: ", i);
                printk(" %02x", *bp);
                c[(i & 0xf)] = ((*bp >= 0x20) && (*bp <= 0x7f)) ? *bp : '.';
                bp++;
                if (((i + 1) % 16) == 0)
                        printk("    %s\n", c);
        }
	printk("\n");

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	return 0;
}
#endif

static int doc_read(struct mtd_info *mtd, loff_t from, size_t len,
		    size_t *retlen, u_char *buf)
{
	int ret, i;
	volatile char dummy;
	loff_t fofs;
	unsigned char syndrome[6], eccbuf[6];
	struct DiskOnChip *this = mtd->priv;
	void __iomem * docptr = this->virtadr;
	struct Nand *mychip = &this->chips[from >> (this->chipshift)];

	
	if (from + len > ((from | 0x1ff) + 1))
		len = ((from | 0x1ff) + 1) - from;

	DoC_CheckASIC(docptr);

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC((DOC_FLASH_CE | DOC_FLASH_WP), docptr, Mplus_FlashSelect);

	
	DoC_Command(docptr, NAND_CMD_RESET, 0);
	DoC_WaitReady(docptr);

	fofs = from;
	DoC_Command(docptr, DoC_GetDataOffset(mtd, &fofs), 0);
	DoC_Address(this, 3, fofs, 0, 0x00);
	WriteDOC(0, docptr, Mplus_FlashControl);
	DoC_WaitReady(docptr);

	
	WriteDOC(DOC_ECC_RESET, docptr, Mplus_ECCConf);
	WriteDOC(DOC_ECC_EN, docptr, Mplus_ECCConf);

	
	*retlen = len;
	ret = 0;

	ReadDOC(docptr, Mplus_ReadPipeInit);
	ReadDOC(docptr, Mplus_ReadPipeInit);

	MemReadDOC(docptr, buf, len);

	
	MemReadDOC(docptr, eccbuf, 4);
	eccbuf[4] = ReadDOC(docptr, Mplus_LastDataRead);
	eccbuf[5] = ReadDOC(docptr, Mplus_LastDataRead);

	
	dummy = ReadDOC(docptr, Mplus_ECCConf);
	dummy = ReadDOC(docptr, Mplus_ECCConf);

	
	if (ReadDOC(docptr, Mplus_ECCConf) & 0x80) {
		int nb_errors;
		
#ifdef ECC_DEBUG
		printk("DiskOnChip ECC Error: Read at %lx\n", (long)from);
#endif
		for (i = 0; i < 6; i++)
			syndrome[i] = ReadDOC(docptr, Mplus_ECCSyndrome0 + i);

		nb_errors = doc_decode_ecc(buf, syndrome);
#ifdef ECC_DEBUG
		printk("ECC Errors corrected: %x\n", nb_errors);
#endif
		if (nb_errors < 0) {
#ifdef ECC_DEBUG
			printk("%s(%d): Millennium Plus ECC error (from=0x%x:\n",
				__FILE__, __LINE__, (int)from);
			printk("        syndrome= %02x:%02x:%02x:%02x:%02x:"
				"%02x\n",
				syndrome[0], syndrome[1], syndrome[2],
				syndrome[3], syndrome[4], syndrome[5]);
			printk("          eccbuf= %02x:%02x:%02x:%02x:%02x:"
				"%02x\n",
				eccbuf[0], eccbuf[1], eccbuf[2],
				eccbuf[3], eccbuf[4], eccbuf[5]);
#endif
				ret = -EIO;
		}
	}

#ifdef PSYCHO_DEBUG
	printk("ECC DATA at %lx: %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",
	       (long)from, eccbuf[0], eccbuf[1], eccbuf[2], eccbuf[3],
	       eccbuf[4], eccbuf[5]);
#endif
	
	WriteDOC(DOC_ECC_DIS, docptr , Mplus_ECCConf);

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	return ret;
}

static int doc_write(struct mtd_info *mtd, loff_t to, size_t len,
		     size_t *retlen, const u_char *buf)
{
	int i, before, ret = 0;
	loff_t fto;
	volatile char dummy;
	char eccbuf[6];
	struct DiskOnChip *this = mtd->priv;
	void __iomem * docptr = this->virtadr;
	struct Nand *mychip = &this->chips[to >> (this->chipshift)];

	
	if ((to & 0x1ff) || (len != 0x200))
		return -EINVAL;

	
	before = (this->interleave && (to & 0x200));

	DoC_CheckASIC(docptr);

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC(DOC_FLASH_CE, docptr, Mplus_FlashSelect);

	
	DoC_Command(docptr, NAND_CMD_RESET, 0);
	DoC_WaitReady(docptr);

	
	fto = to;
	WriteDOC(DoC_GetDataOffset(mtd, &fto), docptr, Mplus_FlashCmd);

	
	if (before)
		fto -= 2;

	
	DoC_Command(docptr, NAND_CMD_SEQIN, 0x00);
	DoC_Address(this, 3, fto, 0x00, 0x00);

	
	WriteDOC(DOC_ECC_RESET, docptr, Mplus_ECCConf);

	if (before) {
		
		WriteDOC(0x55, docptr, Mil_CDSN_IO);
		WriteDOC(0x55, docptr, Mil_CDSN_IO);
	}

	
	WriteDOC(DOC_ECC_EN | DOC_ECC_RW, docptr, Mplus_ECCConf);

	MemWriteDOC(docptr, (unsigned char *) buf, len);

	DoC_Delay(docptr, 3);

	
	for (i = 0; i < 6; i++)
		eccbuf[i] = ReadDOC(docptr, Mplus_ECCSyndrome0 + i);

	
	WriteDOC(DOC_ECC_DIS, docptr, Mplus_ECCConf);

	
	MemWriteDOC(docptr, eccbuf, 6);

	if (!before) {
		
		WriteDOC(0x55, docptr, Mil_CDSN_IO+6);
		WriteDOC(0x55, docptr, Mil_CDSN_IO+7);
	}

#ifdef PSYCHO_DEBUG
	printk("OOB data at %lx is %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",
	       (long) to, eccbuf[0], eccbuf[1], eccbuf[2], eccbuf[3],
	       eccbuf[4], eccbuf[5]);
#endif

	WriteDOC(0x00, docptr, Mplus_WritePipeTerm);
	WriteDOC(0x00, docptr, Mplus_WritePipeTerm);

	DoC_Command(docptr, NAND_CMD_PAGEPROG, 0x00);
	DoC_WaitReady(docptr);

	DoC_Command(docptr, NAND_CMD_STATUS, 0);
	dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
	dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
	DoC_Delay(docptr, 2);
	if ((dummy = ReadDOC(docptr, Mplus_LastDataRead)) & 1) {
		printk("MTD: Error 0x%x programming at 0x%x\n", dummy, (int)to);
		ret = -EIO;
	}
	dummy = ReadDOC(docptr, Mplus_LastDataRead);

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	
	*retlen = len;

	return ret;
}

static int doc_read_oob(struct mtd_info *mtd, loff_t ofs,
			struct mtd_oob_ops *ops)
{
	loff_t fofs, base;
	struct DiskOnChip *this = mtd->priv;
	void __iomem * docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];
	size_t i, size, got, want;
	uint8_t *buf = ops->oobbuf;
	size_t len = ops->len;

	BUG_ON(ops->mode != MTD_OPS_PLACE_OOB);

	ofs += ops->ooboffs;

	DoC_CheckASIC(docptr);

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC((DOC_FLASH_CE | DOC_FLASH_WP), docptr, Mplus_FlashSelect);

	
	WriteDOC(DOC_ECC_RESET, docptr, Mplus_ECCConf);
	DoC_WaitReady(docptr);

	
	if (len > 16)
		len = 16;
	got = 0;
	want = len;

	for (i = 0; ((i < 3) && (want > 0)); i++) {
		
		fofs = ofs;
		base = ofs & 0xf;
		if (!this->interleave) {
			DoC_Command(docptr, NAND_CMD_READOOB, 0);
			size = 16 - base;
		} else if (base < 6) {
			DoC_Command(docptr, DoC_GetECCOffset(mtd, &fofs), 0);
			size = 6 - base;
		} else if (base < 8) {
			DoC_Command(docptr, DoC_GetFlagsOffset(mtd, &fofs), 0);
			size = 8 - base;
		} else {
			DoC_Command(docptr, DoC_GetHdrOffset(mtd, &fofs), 0);
			size = 16 - base;
		}
		if (size > want)
			size = want;

		
		DoC_Address(this, 3, fofs, 0, 0x00);
		WriteDOC(0, docptr, Mplus_FlashControl);
		DoC_WaitReady(docptr);

		ReadDOC(docptr, Mplus_ReadPipeInit);
		ReadDOC(docptr, Mplus_ReadPipeInit);
		MemReadDOC(docptr, &buf[got], size - 2);
		buf[got + size - 2] = ReadDOC(docptr, Mplus_LastDataRead);
		buf[got + size - 1] = ReadDOC(docptr, Mplus_LastDataRead);

		ofs += size;
		got += size;
		want -= size;
	}

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	ops->retlen = len;
	return 0;
}

static int doc_write_oob(struct mtd_info *mtd, loff_t ofs,
			 struct mtd_oob_ops *ops)
{
	volatile char dummy;
	loff_t fofs, base;
	struct DiskOnChip *this = mtd->priv;
	void __iomem * docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];
	size_t i, size, got, want;
	int ret = 0;
	uint8_t *buf = ops->oobbuf;
	size_t len = ops->len;

	BUG_ON(ops->mode != MTD_OPS_PLACE_OOB);

	ofs += ops->ooboffs;

	DoC_CheckASIC(docptr);

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC(DOC_FLASH_CE, docptr, Mplus_FlashSelect);


	
	if (len > 16)
		len = 16;
	got = 0;
	want = len;

	for (i = 0; ((i < 3) && (want > 0)); i++) {
		
		DoC_Command(docptr, NAND_CMD_RESET, 0);
		DoC_WaitReady(docptr);

		
		fofs = ofs;
		base = ofs & 0x0f;
		if (!this->interleave) {
			WriteDOC(NAND_CMD_READOOB, docptr, Mplus_FlashCmd);
			size = 16 - base;
		} else if (base < 6) {
			WriteDOC(DoC_GetECCOffset(mtd, &fofs), docptr, Mplus_FlashCmd);
			size = 6 - base;
		} else if (base < 8) {
			WriteDOC(DoC_GetFlagsOffset(mtd, &fofs), docptr, Mplus_FlashCmd);
			size = 8 - base;
		} else {
			WriteDOC(DoC_GetHdrOffset(mtd, &fofs), docptr, Mplus_FlashCmd);
			size = 16 - base;
		}
		if (size > want)
			size = want;

		
		DoC_Command(docptr, NAND_CMD_SEQIN, 0x00);
		DoC_Address(this, 3, fofs, 0, 0x00);

		
		WriteDOC(DOC_ECC_RESET, docptr, Mplus_ECCConf);

		MemWriteDOC(docptr, (unsigned char *) &buf[got], size);
		WriteDOC(0x00, docptr, Mplus_WritePipeTerm);
		WriteDOC(0x00, docptr, Mplus_WritePipeTerm);

		DoC_Command(docptr, NAND_CMD_PAGEPROG, 0x00);
		DoC_WaitReady(docptr);

		DoC_Command(docptr, NAND_CMD_STATUS, 0x00);
		dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
		dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
		DoC_Delay(docptr, 2);
		if ((dummy = ReadDOC(docptr, Mplus_LastDataRead)) & 1) {
			printk("MTD: Error 0x%x programming oob at 0x%x\n",
				dummy, (int)ofs);
			
			ops->retlen = 0;
			ret = -EIO;
		}
		dummy = ReadDOC(docptr, Mplus_LastDataRead);

		ofs += size;
		got += size;
		want -= size;
	}

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	ops->retlen = len;
	return ret;
}

int doc_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	volatile char dummy;
	struct DiskOnChip *this = mtd->priv;
	__u32 ofs = instr->addr;
	__u32 len = instr->len;
	void __iomem * docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];

	DoC_CheckASIC(docptr);

	if (len != mtd->erasesize)
		printk(KERN_WARNING "MTD: Erase not right size (%x != %x)n",
		       len, mtd->erasesize);

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	instr->state = MTD_ERASE_PENDING;

	
	WriteDOC(DOC_FLASH_CE, docptr, Mplus_FlashSelect);

	DoC_Command(docptr, NAND_CMD_RESET, 0x00);
	DoC_WaitReady(docptr);

	DoC_Command(docptr, NAND_CMD_ERASE1, 0);
	DoC_Address(this, 2, ofs, 0, 0x00);
	DoC_Command(docptr, NAND_CMD_ERASE2, 0);
	DoC_WaitReady(docptr);
	instr->state = MTD_ERASING;

	DoC_Command(docptr, NAND_CMD_STATUS, 0);
	dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
	dummy = ReadDOC(docptr, Mplus_ReadPipeInit);
	if ((dummy = ReadDOC(docptr, Mplus_LastDataRead)) & 1) {
		printk("MTD: Error 0x%x erasing at 0x%x\n", dummy, ofs);
		
		instr->state = MTD_ERASE_FAILED;
	} else {
		instr->state = MTD_ERASE_DONE;
	}
	dummy = ReadDOC(docptr, Mplus_LastDataRead);

	
	WriteDOC(0, docptr, Mplus_FlashSelect);

	mtd_erase_callback(instr);

	return 0;
}


static void __exit cleanup_doc2001plus(void)
{
	struct mtd_info *mtd;
	struct DiskOnChip *this;

	while ((mtd=docmilpluslist)) {
		this = mtd->priv;
		docmilpluslist = this->nextdoc;

		mtd_device_unregister(mtd);

		iounmap(this->virtadr);
		kfree(this->chips);
		kfree(mtd);
	}
}

module_exit(cleanup_doc2001plus);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com> et al.");
MODULE_DESCRIPTION("Driver for DiskOnChip Millennium Plus");
