

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

static struct mtd_info *docmillist = NULL;

static void DoC_Delay(void __iomem * docptr, unsigned short cycles)
{
	volatile char dummy;
	int i;

	for (i = 0; i < cycles; i++)
		dummy = ReadDOC(docptr, NOP);
}

static int _DoC_WaitReady(void __iomem * docptr)
{
	unsigned short c = 0xffff;

	pr_debug("_DoC_WaitReady called for out-of-line wait\n");

	
	while (!(ReadDOC(docptr, CDSNControl) & CDSN_CTRL_FR_B) && --c)
		;

	if (c == 0)
		pr_debug("_DoC_WaitReady timed out.\n");

	return (c == 0);
}

static inline int DoC_WaitReady(void __iomem * docptr)
{
	
	int ret = 0;

	DoC_Delay(docptr, 4);

	if (!(ReadDOC(docptr, CDSNControl) & CDSN_CTRL_FR_B))
		
		ret = _DoC_WaitReady(docptr);

	DoC_Delay(docptr, 2);

	return ret;
}


static void DoC_Command(void __iomem * docptr, unsigned char command,
			       unsigned char xtraflags)
{
	
	WriteDOC(xtraflags | CDSN_CTRL_CLE | CDSN_CTRL_CE, docptr, CDSNControl);
	DoC_Delay(docptr, 4);

	
	WriteDOC(command, docptr, Mil_CDSN_IO);
	WriteDOC(0x00, docptr, WritePipeTerm);

	
	WriteDOC(xtraflags | CDSN_CTRL_CE, docptr, CDSNControl);
	DoC_Delay(docptr, 4);
}


static inline void DoC_Address(void __iomem * docptr, int numbytes, unsigned long ofs,
			       unsigned char xtraflags1, unsigned char xtraflags2)
{
	
	WriteDOC(xtraflags1 | CDSN_CTRL_ALE | CDSN_CTRL_CE, docptr, CDSNControl);
	DoC_Delay(docptr, 4);

	
	switch (numbytes)
	    {
	    case 1:
		    
		    WriteDOC(ofs & 0xff, docptr, Mil_CDSN_IO);
		    WriteDOC(0x00, docptr, WritePipeTerm);
		    break;
	    case 2:
		    
		    WriteDOC((ofs >> 9)  & 0xff, docptr, Mil_CDSN_IO);
		    WriteDOC((ofs >> 17) & 0xff, docptr, Mil_CDSN_IO);
		    WriteDOC(0x00, docptr, WritePipeTerm);
		break;
	    case 3:
		    
		    WriteDOC(ofs & 0xff, docptr, Mil_CDSN_IO);
		    WriteDOC((ofs >> 9)  & 0xff, docptr, Mil_CDSN_IO);
		    WriteDOC((ofs >> 17) & 0xff, docptr, Mil_CDSN_IO);
		    WriteDOC(0x00, docptr, WritePipeTerm);
		break;
	    default:
		return;
	    }

	
	WriteDOC(xtraflags1 | xtraflags2 | CDSN_CTRL_CE, docptr, CDSNControl);
	DoC_Delay(docptr, 4);
}

static int DoC_SelectChip(void __iomem * docptr, int chip)
{
	
	WriteDOC(chip, docptr, CDSNDeviceSelect);
	DoC_Delay(docptr, 4);

	
	return DoC_WaitReady(docptr);
}

static int DoC_SelectFloor(void __iomem * docptr, int floor)
{
	
	WriteDOC(floor, docptr, FloorSelect);

	
	return DoC_WaitReady(docptr);
}

static int DoC_IdentChip(struct DiskOnChip *doc, int floor, int chip)
{
	int mfr, id, i, j;
	volatile char dummy;

	DoC_SelectFloor(doc->virtadr, floor);
	DoC_SelectChip(doc->virtadr, chip);

	
	DoC_Command(doc->virtadr, NAND_CMD_RESET, CDSN_CTRL_WP);
	DoC_WaitReady(doc->virtadr);

	
	DoC_Command(doc->virtadr, NAND_CMD_READID, CDSN_CTRL_WP);

	
	DoC_Address(doc->virtadr, 1, 0x00, CDSN_CTRL_WP, 0x00);

	dummy = ReadDOC(doc->virtadr, ReadPipeInit);
	DoC_Delay(doc->virtadr, 2);
	mfr = ReadDOC(doc->virtadr, Mil_CDSN_IO);

	DoC_Delay(doc->virtadr, 2);
	id  = ReadDOC(doc->virtadr, Mil_CDSN_IO);
	dummy = ReadDOC(doc->virtadr, LastDataRead);

	
	if (mfr == 0xff || mfr == 0)
		return 0;

	
	for (i = 0; nand_flash_ids[i].name != NULL; i++) {
		if ( id == nand_flash_ids[i].id) {
			
			for (j = 0; nand_manuf_ids[j].id != 0x0; j++) {
				if (nand_manuf_ids[j].id == mfr)
					break;
			}
			printk(KERN_INFO "Flash chip found: Manufacturer ID: %2.2X, "
			       "Chip ID: %2.2X (%s:%s)\n",
			       mfr, id, nand_manuf_ids[j].name, nand_flash_ids[i].name);
			doc->mfr = mfr;
			doc->id = id;
			doc->chipshift = ffs((nand_flash_ids[i].chipsize << 20)) - 1;
			break;
		}
	}

	if (nand_flash_ids[i].name == NULL)
		return 0;
	else
		return 1;
}

static void DoC_ScanChips(struct DiskOnChip *this)
{
	int floor, chip;
	int numchips[MAX_FLOORS_MIL];
	int ret;

	this->numchips = 0;
	this->mfr = 0;
	this->id = 0;

	
	for (floor = 0,ret = 1; floor < MAX_FLOORS_MIL; floor++) {
		numchips[floor] = 0;
		for (chip = 0; chip < MAX_CHIPS_MIL && ret != 0; chip++) {
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
		printk("No memory for allocating chip info structures\n");
		return;
	}

	for (floor = 0, ret = 0; floor < MAX_FLOORS_MIL; floor++) {
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

static int DoCMil_is_alias(struct DiskOnChip *doc1, struct DiskOnChip *doc2)
{
	int tmp1, tmp2, retval;

	if (doc1->physadr == doc2->physadr)
		return 1;

	tmp1 = ReadDOC(doc1->virtadr, AliasResolution);
	tmp2 = ReadDOC(doc2->virtadr, AliasResolution);
	if (tmp1 != tmp2)
		return 0;

	WriteDOC((tmp1+1) % 0xff, doc1->virtadr, AliasResolution);
	tmp2 = ReadDOC(doc2->virtadr, AliasResolution);
	if (tmp2 == (tmp1+1) % 0xff)
		retval = 1;
	else
		retval = 0;

	WriteDOC(tmp1, doc1->virtadr, AliasResolution);

	return retval;
}

void DoCMil_init(struct mtd_info *mtd)
{
	struct DiskOnChip *this = mtd->priv;
	struct DiskOnChip *old = NULL;

	
	if (docmillist)
		old = docmillist->priv;

	while (old) {
		if (DoCMil_is_alias(this, old)) {
			printk(KERN_NOTICE "Ignoring DiskOnChip Millennium at "
			       "0x%lX - already configured\n", this->physadr);
			iounmap(this->virtadr);
			kfree(mtd);
			return;
		}
		if (old->nextdoc)
			old = old->nextdoc->priv;
		else
			old = NULL;
	}

	mtd->name = "DiskOnChip Millennium";
	printk(KERN_NOTICE "DiskOnChip Millennium found at address 0x%lX\n",
	       this->physadr);

	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;

	
	mtd->erasesize = 0x2000;
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
		this->nextdoc = docmillist;
		docmillist = mtd;
		mtd->size  = this->totlen;
		mtd_device_register(mtd, NULL, 0);
		return;
	}
}
EXPORT_SYMBOL_GPL(DoCMil_init);

static int doc_read (struct mtd_info *mtd, loff_t from, size_t len,
		     size_t *retlen, u_char *buf)
{
	int i, ret;
	volatile char dummy;
	unsigned char syndrome[6], eccbuf[6];
	struct DiskOnChip *this = mtd->priv;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip = &this->chips[from >> (this->chipshift)];

	
	if (from + len > ((from | 0x1ff) + 1))
		len = ((from | 0x1ff) + 1) - from;

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	DoC_Command(docptr, (from >> 8) & 1, CDSN_CTRL_WP);
	DoC_Address(docptr, 3, from, CDSN_CTRL_WP, 0x00);
	DoC_WaitReady(docptr);

	
	WriteDOC (DOC_ECC_RESET, docptr, ECCConf);
	WriteDOC (DOC_ECC_EN, docptr, ECCConf);

	dummy = ReadDOC(docptr, ReadPipeInit);
#ifndef USE_MEMCPY
	for (i = 0; i < len-1; i++) {
		buf[i] = ReadDOC(docptr, Mil_CDSN_IO + (i & 0xff));
	}
#else
	memcpy_fromio(buf, docptr + DoC_Mil_CDSN_IO, len - 1);
#endif
	buf[len - 1] = ReadDOC(docptr, LastDataRead);

	
	*retlen = len;
        ret = 0;

	dummy = ReadDOC(docptr, ReadPipeInit);
#ifndef USE_MEMCPY
	for (i = 0; i < 5; i++) {
		eccbuf[i] = ReadDOC(docptr, Mil_CDSN_IO + i);
	}
#else
	memcpy_fromio(eccbuf, docptr + DoC_Mil_CDSN_IO, 5);
#endif
	eccbuf[5] = ReadDOC(docptr, LastDataRead);

	
	dummy = ReadDOC(docptr, ECCConf);
	dummy = ReadDOC(docptr, ECCConf);

	
	if (ReadDOC(docptr, ECCConf) & 0x80) {
		int nb_errors;
		
#ifdef ECC_DEBUG
		printk("DiskOnChip ECC Error: Read at %lx\n", (long)from);
#endif
		for (i = 0; i < 6; i++) {
			syndrome[i] = ReadDOC(docptr, ECCSyndrome0 + i);
		}
		nb_errors = doc_decode_ecc(buf, syndrome);
#ifdef ECC_DEBUG
		printk("ECC Errors corrected: %x\n", nb_errors);
#endif
		if (nb_errors < 0) {
			ret = -EIO;
		}
	}

#ifdef PSYCHO_DEBUG
	printk("ECC DATA at %lx: %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",
	       (long)from, eccbuf[0], eccbuf[1], eccbuf[2], eccbuf[3],
	       eccbuf[4], eccbuf[5]);
#endif

	
	WriteDOC(DOC_ECC_DIS, docptr , ECCConf);

	return ret;
}

static int doc_write (struct mtd_info *mtd, loff_t to, size_t len,
		      size_t *retlen, const u_char *buf)
{
	int i,ret = 0;
	char eccbuf[6];
	volatile char dummy;
	struct DiskOnChip *this = mtd->priv;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip = &this->chips[to >> (this->chipshift)];

#if 0
	
	if (to + len > ( (to | 0x1ff) + 1))
		len = ((to | 0x1ff) + 1) - to;
#else
	
	if (to & 0x1ff || len != 0x200)
		return -EINVAL;
#endif

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	DoC_Command(docptr, NAND_CMD_RESET, 0x00);
	DoC_WaitReady(docptr);
	
	DoC_Command(docptr, NAND_CMD_READ0, 0x00);

	
	DoC_Command(docptr, NAND_CMD_SEQIN, 0x00);
	DoC_Address(docptr, 3, to, 0x00, 0x00);
	DoC_WaitReady(docptr);

	
	WriteDOC (DOC_ECC_RESET, docptr, ECCConf);
	WriteDOC (DOC_ECC_EN | DOC_ECC_RW, docptr, ECCConf);

#ifndef USE_MEMCPY
	for (i = 0; i < len; i++) {
		WriteDOC(buf[i], docptr, Mil_CDSN_IO + i);
	}
#else
	memcpy_toio(docptr + DoC_Mil_CDSN_IO, buf, len);
#endif
	WriteDOC(0x00, docptr, WritePipeTerm);

	WriteDOC(0, docptr, NOP);
	WriteDOC(0, docptr, NOP);
	WriteDOC(0, docptr, NOP);

	
	for (i = 0; i < 6; i++) {
		eccbuf[i] = ReadDOC(docptr, ECCSyndrome0 + i);
	}

	
	WriteDOC(DOC_ECC_DIS, docptr , ECCConf);

#ifndef USE_MEMCPY
	
	for (i = 0; i < 6; i++) {
		WriteDOC(eccbuf[i], docptr, Mil_CDSN_IO + i);
	}
#else
	memcpy_toio(docptr + DoC_Mil_CDSN_IO, eccbuf, 6);
#endif

	WriteDOC(0x55, docptr, Mil_CDSN_IO);
	WriteDOC(0x55, docptr, Mil_CDSN_IO + 1);

	WriteDOC(0x00, docptr, WritePipeTerm);

#ifdef PSYCHO_DEBUG
	printk("OOB data at %lx is %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",
	       (long) to, eccbuf[0], eccbuf[1], eccbuf[2], eccbuf[3],
	       eccbuf[4], eccbuf[5]);
#endif

	DoC_Command(docptr, NAND_CMD_PAGEPROG, 0x00);
	DoC_WaitReady(docptr);

	DoC_Command(docptr, NAND_CMD_STATUS, CDSN_CTRL_WP);
	dummy = ReadDOC(docptr, ReadPipeInit);
	DoC_Delay(docptr, 2);
	if (ReadDOC(docptr, Mil_CDSN_IO) & 1) {
		printk("Error programming flash\n");
		ret = -EIO;
	}
	dummy = ReadDOC(docptr, LastDataRead);

	
	*retlen = len;

	return ret;
}

static int doc_read_oob(struct mtd_info *mtd, loff_t ofs,
			struct mtd_oob_ops *ops)
{
#ifndef USE_MEMCPY
	int i;
#endif
	volatile char dummy;
	struct DiskOnChip *this = mtd->priv;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];
	uint8_t *buf = ops->oobbuf;
	size_t len = ops->len;

	BUG_ON(ops->mode != MTD_OPS_PLACE_OOB);

	ofs += ops->ooboffs;

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC (DOC_ECC_RESET, docptr, ECCConf);
	WriteDOC (DOC_ECC_DIS, docptr, ECCConf);

	DoC_Command(docptr, NAND_CMD_READOOB, CDSN_CTRL_WP);
	DoC_Address(docptr, 3, ofs, CDSN_CTRL_WP, 0x00);
	DoC_WaitReady(docptr);

	dummy = ReadDOC(docptr, ReadPipeInit);
#ifndef USE_MEMCPY
	for (i = 0; i < len-1; i++) {
		buf[i] = ReadDOC(docptr, Mil_CDSN_IO + i);
	}
#else
	memcpy_fromio(buf, docptr + DoC_Mil_CDSN_IO, len - 1);
#endif
	buf[len - 1] = ReadDOC(docptr, LastDataRead);

	ops->retlen = len;

	return 0;
}

static int doc_write_oob(struct mtd_info *mtd, loff_t ofs,
			 struct mtd_oob_ops *ops)
{
#ifndef USE_MEMCPY
	int i;
#endif
	volatile char dummy;
	int ret = 0;
	struct DiskOnChip *this = mtd->priv;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];
	uint8_t *buf = ops->oobbuf;
	size_t len = ops->len;

	BUG_ON(ops->mode != MTD_OPS_PLACE_OOB);

	ofs += ops->ooboffs;

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(docptr, mychip->floor);
		DoC_SelectChip(docptr, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(docptr, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC (DOC_ECC_RESET, docptr, ECCConf);
	WriteDOC (DOC_ECC_DIS, docptr, ECCConf);

	
	DoC_Command(docptr, NAND_CMD_RESET, CDSN_CTRL_WP);
	DoC_WaitReady(docptr);
	
	DoC_Command(docptr, NAND_CMD_READOOB, CDSN_CTRL_WP);

	
	DoC_Command(docptr, NAND_CMD_SEQIN, 0x00);
	DoC_Address(docptr, 3, ofs, 0x00, 0x00);

#ifndef USE_MEMCPY
	for (i = 0; i < len; i++) {
		WriteDOC(buf[i], docptr, Mil_CDSN_IO + i);
	}
#else
	memcpy_toio(docptr + DoC_Mil_CDSN_IO, buf, len);
#endif
	WriteDOC(0x00, docptr, WritePipeTerm);

	DoC_Command(docptr, NAND_CMD_PAGEPROG, 0x00);
	DoC_WaitReady(docptr);

	DoC_Command(docptr, NAND_CMD_STATUS, 0x00);
	dummy = ReadDOC(docptr, ReadPipeInit);
	DoC_Delay(docptr, 2);
	if (ReadDOC(docptr, Mil_CDSN_IO) & 1) {
		printk("Error programming oob data\n");
		
		ops->retlen = 0;
		ret = -EIO;
	}
	dummy = ReadDOC(docptr, LastDataRead);

	ops->retlen = len;

	return ret;
}

int doc_erase (struct mtd_info *mtd, struct erase_info *instr)
{
	volatile char dummy;
	struct DiskOnChip *this = mtd->priv;
	__u32 ofs = instr->addr;
	__u32 len = instr->len;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];

	if (len != mtd->erasesize)
		printk(KERN_WARNING "Erase not right size (%x != %x)n",
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

	
	DoC_Command(docptr, NAND_CMD_ERASE1, 0x00);
	DoC_Address(docptr, 2, ofs, 0x00, 0x00);

	DoC_Command(docptr, NAND_CMD_ERASE2, 0x00);
	DoC_WaitReady(docptr);

	instr->state = MTD_ERASING;

	DoC_Command(docptr, NAND_CMD_STATUS, CDSN_CTRL_WP);
	dummy = ReadDOC(docptr, ReadPipeInit);
	DoC_Delay(docptr, 2);
	if (ReadDOC(docptr, Mil_CDSN_IO) & 1) {
		printk("Error Erasing at 0x%x\n", ofs);
		instr->state = MTD_ERASE_FAILED;
	} else
		instr->state = MTD_ERASE_DONE;
	dummy = ReadDOC(docptr, LastDataRead);

	mtd_erase_callback(instr);

	return 0;
}


static void __exit cleanup_doc2001(void)
{
	struct mtd_info *mtd;
	struct DiskOnChip *this;

	while ((mtd=docmillist)) {
		this = mtd->priv;
		docmillist = this->nextdoc;

		mtd_device_unregister(mtd);

		iounmap(this->virtadr);
		kfree(this->chips);
		kfree(mtd);
	}
}

module_exit(cleanup_doc2001);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org> et al.");
MODULE_DESCRIPTION("Alternative driver for DiskOnChip Millennium");
