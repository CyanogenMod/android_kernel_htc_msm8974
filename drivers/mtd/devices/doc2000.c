

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/mutex.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/doc2000.h>

#define DOC_SUPPORT_2000
#define DOC_SUPPORT_2000TSOP
#define DOC_SUPPORT_MILLENNIUM

#ifdef DOC_SUPPORT_2000
#define DoC_is_2000(doc) (doc->ChipID == DOC_ChipID_Doc2k)
#else
#define DoC_is_2000(doc) (0)
#endif

#if defined(DOC_SUPPORT_2000TSOP) || defined(DOC_SUPPORT_MILLENNIUM)
#define DoC_is_Millennium(doc) (doc->ChipID == DOC_ChipID_DocMil)
#else
#define DoC_is_Millennium(doc) (0)
#endif



static int doc_read(struct mtd_info *mtd, loff_t from, size_t len,
		    size_t *retlen, u_char *buf);
static int doc_write(struct mtd_info *mtd, loff_t to, size_t len,
		     size_t *retlen, const u_char *buf);
static int doc_read_oob(struct mtd_info *mtd, loff_t ofs,
			struct mtd_oob_ops *ops);
static int doc_write_oob(struct mtd_info *mtd, loff_t ofs,
			 struct mtd_oob_ops *ops);
static int doc_write_oob_nolock(struct mtd_info *mtd, loff_t ofs, size_t len,
			 size_t *retlen, const u_char *buf);
static int doc_erase (struct mtd_info *mtd, struct erase_info *instr);

static struct mtd_info *doc2klist = NULL;

static void DoC_Delay(struct DiskOnChip *doc, unsigned short cycles)
{
	volatile char dummy;
	int i;

	for (i = 0; i < cycles; i++) {
		if (DoC_is_Millennium(doc))
			dummy = ReadDOC(doc->virtadr, NOP);
		else
			dummy = ReadDOC(doc->virtadr, DOCStatus);
	}

}

static int _DoC_WaitReady(struct DiskOnChip *doc)
{
	void __iomem *docptr = doc->virtadr;
	unsigned long timeo = jiffies + (HZ * 10);

	pr_debug("_DoC_WaitReady called for out-of-line wait\n");

	
	while (!(ReadDOC(docptr, CDSNControl) & CDSN_CTRL_FR_B)) {
		DoC_Delay(doc, 2);

		if (time_after(jiffies, timeo)) {
			pr_debug("_DoC_WaitReady timed out.\n");
			return -EIO;
		}
		udelay(1);
		cond_resched();
	}

	return 0;
}

static inline int DoC_WaitReady(struct DiskOnChip *doc)
{
	void __iomem *docptr = doc->virtadr;

	
	int ret = 0;

	DoC_Delay(doc, 4);

	if (!(ReadDOC(docptr, CDSNControl) & CDSN_CTRL_FR_B))
		
		ret = _DoC_WaitReady(doc);

	DoC_Delay(doc, 2);

	return ret;
}


static int DoC_Command(struct DiskOnChip *doc, unsigned char command,
			      unsigned char xtraflags)
{
	void __iomem *docptr = doc->virtadr;

	if (DoC_is_2000(doc))
		xtraflags |= CDSN_CTRL_FLASH_IO;

	
	WriteDOC(xtraflags | CDSN_CTRL_CLE | CDSN_CTRL_CE, docptr, CDSNControl);
	DoC_Delay(doc, 4);	

	if (DoC_is_Millennium(doc))
		WriteDOC(command, docptr, CDSNSlowIO);

	
	WriteDOC_(command, docptr, doc->ioreg);
	if (DoC_is_Millennium(doc))
		WriteDOC(command, docptr, WritePipeTerm);

	
	WriteDOC(xtraflags | CDSN_CTRL_CE, docptr, CDSNControl);
	DoC_Delay(doc, 4);	

	
	return DoC_WaitReady(doc);
}


static int DoC_Address(struct DiskOnChip *doc, int numbytes, unsigned long ofs,
		       unsigned char xtraflags1, unsigned char xtraflags2)
{
	int i;
	void __iomem *docptr = doc->virtadr;

	if (DoC_is_2000(doc))
		xtraflags1 |= CDSN_CTRL_FLASH_IO;

	
	WriteDOC(xtraflags1 | CDSN_CTRL_ALE | CDSN_CTRL_CE, docptr, CDSNControl);

	DoC_Delay(doc, 4);	

	

	if (numbytes == ADDR_COLUMN || numbytes == ADDR_COLUMN_PAGE) {
		if (DoC_is_Millennium(doc))
			WriteDOC(ofs & 0xff, docptr, CDSNSlowIO);
		WriteDOC_(ofs & 0xff, docptr, doc->ioreg);
	}

	if (doc->page256) {
		ofs = ofs >> 8;
	} else {
		ofs = ofs >> 9;
	}

	if (numbytes == ADDR_PAGE || numbytes == ADDR_COLUMN_PAGE) {
		for (i = 0; i < doc->pageadrlen; i++, ofs = ofs >> 8) {
			if (DoC_is_Millennium(doc))
				WriteDOC(ofs & 0xff, docptr, CDSNSlowIO);
			WriteDOC_(ofs & 0xff, docptr, doc->ioreg);
		}
	}

	if (DoC_is_Millennium(doc))
		WriteDOC(ofs & 0xff, docptr, WritePipeTerm);

	DoC_Delay(doc, 2);	


	
	WriteDOC(xtraflags1 | xtraflags2 | CDSN_CTRL_CE, docptr,
		 CDSNControl);

	DoC_Delay(doc, 4);	

	
	return DoC_WaitReady(doc);
}

static void DoC_ReadBuf(struct DiskOnChip *doc, u_char * buf, int len)
{
	volatile int dummy;
	int modulus = 0xffff;
	void __iomem *docptr = doc->virtadr;
	int i;

	if (len <= 0)
		return;

	if (DoC_is_Millennium(doc)) {
		dummy = ReadDOC(docptr, ReadPipeInit);

		
		len--;

		
		modulus = 0xff;
	}

	for (i = 0; i < len; i++)
		buf[i] = ReadDOC_(docptr, doc->ioreg + (i & modulus));

	if (DoC_is_Millennium(doc)) {
		buf[i] = ReadDOC(docptr, LastDataRead);
	}
}

static void DoC_WriteBuf(struct DiskOnChip *doc, const u_char * buf, int len)
{
	void __iomem *docptr = doc->virtadr;
	int i;

	if (len <= 0)
		return;

	for (i = 0; i < len; i++)
		WriteDOC_(buf[i], docptr, doc->ioreg + i);

	if (DoC_is_Millennium(doc)) {
		WriteDOC(0x00, docptr, WritePipeTerm);
	}
}



static inline int DoC_SelectChip(struct DiskOnChip *doc, int chip)
{
	void __iomem *docptr = doc->virtadr;

	
	
	WriteDOC(CDSN_CTRL_WP, docptr, CDSNControl);
	DoC_Delay(doc, 4);	

	
	WriteDOC(chip, docptr, CDSNDeviceSelect);
	DoC_Delay(doc, 4);

	
	WriteDOC(CDSN_CTRL_CE | CDSN_CTRL_FLASH_IO | CDSN_CTRL_WP, docptr,
		 CDSNControl);
	DoC_Delay(doc, 4);	

	
	return DoC_WaitReady(doc);
}


static inline int DoC_SelectFloor(struct DiskOnChip *doc, int floor)
{
	void __iomem *docptr = doc->virtadr;

	
	WriteDOC(floor, docptr, FloorSelect);

	
	return DoC_WaitReady(doc);
}


static int DoC_IdentChip(struct DiskOnChip *doc, int floor, int chip)
{
	int mfr, id, i, j;
	volatile char dummy;

	
	DoC_SelectFloor(doc, floor);
	DoC_SelectChip(doc, chip);

	
	if (DoC_Command(doc, NAND_CMD_RESET, CDSN_CTRL_WP)) {
		pr_debug("DoC_Command (reset) for %d,%d returned true\n",
		      floor, chip);
		return 0;
	}


	
	if (DoC_Command(doc, NAND_CMD_READID, CDSN_CTRL_WP)) {
		pr_debug("DoC_Command (ReadID) for %d,%d returned true\n",
		      floor, chip);
		return 0;
	}

	
	DoC_Address(doc, ADDR_COLUMN, 0, CDSN_CTRL_WP, 0);

	

	if (DoC_is_Millennium(doc)) {
		DoC_Delay(doc, 2);
		dummy = ReadDOC(doc->virtadr, ReadPipeInit);
		mfr = ReadDOC(doc->virtadr, LastDataRead);

		DoC_Delay(doc, 2);
		dummy = ReadDOC(doc->virtadr, ReadPipeInit);
		id = ReadDOC(doc->virtadr, LastDataRead);
	} else {
		
		dummy = ReadDOC(doc->virtadr, CDSNSlowIO);
		DoC_Delay(doc, 2);
		mfr = ReadDOC_(doc->virtadr, doc->ioreg);

		
		dummy = ReadDOC(doc->virtadr, CDSNSlowIO);
		DoC_Delay(doc, 2);
		id = ReadDOC_(doc->virtadr, doc->ioreg);
	}

	
	if (mfr == 0xff || mfr == 0)
		return 0;

	if (doc->mfr) {
		if (doc->mfr == mfr && doc->id == id)
			return 1;	
		else
			printk(KERN_WARNING
			       "Flash chip at floor %d, chip %d is different:\n",
			       floor, chip);
	}

	
	for (i = 0; nand_flash_ids[i].name != NULL; i++) {
		if (id == nand_flash_ids[i].id) {
			
			for (j = 0; nand_manuf_ids[j].id != 0x0; j++) {
				if (nand_manuf_ids[j].id == mfr)
					break;
			}
			printk(KERN_INFO
			       "Flash chip found: Manufacturer ID: %2.2X, "
			       "Chip ID: %2.2X (%s:%s)\n", mfr, id,
			       nand_manuf_ids[j].name, nand_flash_ids[i].name);
			if (!doc->mfr) {
				doc->mfr = mfr;
				doc->id = id;
				doc->chipshift =
					ffs((nand_flash_ids[i].chipsize << 20)) - 1;
				doc->page256 = (nand_flash_ids[i].pagesize == 256) ? 1 : 0;
				doc->pageadrlen = doc->chipshift > 25 ? 3 : 2;
				doc->erasesize =
				    nand_flash_ids[i].erasesize;
				return 1;
			}
			return 0;
		}
	}


	
	printk(KERN_WARNING "Unknown flash chip found: %2.2X %2.2X\n",
	       id, mfr);

	printk(KERN_WARNING "Please report to dwmw2@infradead.org\n");
	return 0;
}


static void DoC_ScanChips(struct DiskOnChip *this, int maxchips)
{
	int floor, chip;
	int numchips[MAX_FLOORS];
	int ret = 1;

	this->numchips = 0;
	this->mfr = 0;
	this->id = 0;

	
	for (floor = 0; floor < MAX_FLOORS; floor++) {
		ret = 1;
		numchips[floor] = 0;
		for (chip = 0; chip < maxchips && ret != 0; chip++) {

			ret = DoC_IdentChip(this, floor, chip);
			if (ret) {
				numchips[floor]++;
				this->numchips++;
			}
		}
	}

	
	if (!this->numchips) {
		printk(KERN_NOTICE "No flash chips recognised.\n");
		return;
	}

	
	this->chips = kmalloc(sizeof(struct Nand) * this->numchips, GFP_KERNEL);
	if (!this->chips) {
		printk(KERN_NOTICE "No memory for allocating chip info structures\n");
		return;
	}

	ret = 0;

	for (floor = 0; floor < MAX_FLOORS; floor++) {
		for (chip = 0; chip < numchips[floor]; chip++) {
			this->chips[ret].floor = floor;
			this->chips[ret].chip = chip;
			this->chips[ret].curadr = 0;
			this->chips[ret].curmode = 0x50;
			ret++;
		}
	}

	
	this->totlen = this->numchips * (1 << this->chipshift);

	printk(KERN_INFO "%d flash chips found. Total DiskOnChip size: %ld MiB\n",
	       this->numchips, this->totlen >> 20);
}

static int DoC2k_is_alias(struct DiskOnChip *doc1, struct DiskOnChip *doc2)
{
	int tmp1, tmp2, retval;
	if (doc1->physadr == doc2->physadr)
		return 1;

	tmp1 = ReadDOC(doc1->virtadr, AliasResolution);
	tmp2 = ReadDOC(doc2->virtadr, AliasResolution);
	if (tmp1 != tmp2)
		return 0;

	WriteDOC((tmp1 + 1) % 0xff, doc1->virtadr, AliasResolution);
	tmp2 = ReadDOC(doc2->virtadr, AliasResolution);
	if (tmp2 == (tmp1 + 1) % 0xff)
		retval = 1;
	else
		retval = 0;

	WriteDOC(tmp1, doc1->virtadr, AliasResolution);

	return retval;
}

void DoC2k_init(struct mtd_info *mtd)
{
	struct DiskOnChip *this = mtd->priv;
	struct DiskOnChip *old = NULL;
	int maxchips;

	

	if (doc2klist)
		old = doc2klist->priv;

	while (old) {
		if (DoC2k_is_alias(old, this)) {
			printk(KERN_NOTICE
			       "Ignoring DiskOnChip 2000 at 0x%lX - already configured\n",
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


	switch (this->ChipID) {
	case DOC_ChipID_Doc2kTSOP:
		mtd->name = "DiskOnChip 2000 TSOP";
		this->ioreg = DoC_Mil_CDSN_IO;
		
		this->ChipID = DOC_ChipID_DocMil;
		maxchips = MAX_CHIPS;
		break;
	case DOC_ChipID_Doc2k:
		mtd->name = "DiskOnChip 2000";
		this->ioreg = DoC_2k_CDSN_IO;
		maxchips = MAX_CHIPS;
		break;
	case DOC_ChipID_DocMil:
		mtd->name = "DiskOnChip Millennium";
		this->ioreg = DoC_Mil_CDSN_IO;
		maxchips = MAX_CHIPS_MIL;
		break;
	default:
		printk("Unknown ChipID 0x%02x\n", this->ChipID);
		kfree(mtd);
		iounmap(this->virtadr);
		return;
	}

	printk(KERN_NOTICE "%s found at address 0x%lX\n", mtd->name,
	       this->physadr);

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
	mutex_init(&this->lock);

	
	DoC_ScanChips(this, maxchips);

	if (!this->totlen) {
		kfree(mtd);
		iounmap(this->virtadr);
	} else {
		this->nextdoc = doc2klist;
		doc2klist = mtd;
		mtd->size = this->totlen;
		mtd->erasesize = this->erasesize;
		mtd_device_register(mtd, NULL, 0);
		return;
	}
}
EXPORT_SYMBOL_GPL(DoC2k_init);

static int doc_read(struct mtd_info *mtd, loff_t from, size_t len,
		    size_t * retlen, u_char * buf)
{
	struct DiskOnChip *this = mtd->priv;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip;
	unsigned char syndrome[6], eccbuf[6];
	volatile char dummy;
	int i, len256 = 0, ret=0;
	size_t left = len;

	mutex_lock(&this->lock);
	while (left) {
		len = left;

		
		if (from + len > ((from | 0x1ff) + 1))
			len = ((from | 0x1ff) + 1) - from;

		
		if (len != 0x200)
			printk(KERN_WARNING
			       "ECC needs a full sector read (adr: %lx size %lx)\n",
			       (long) from, (long) len);

		


		
		mychip = &this->chips[from >> (this->chipshift)];

		if (this->curfloor != mychip->floor) {
			DoC_SelectFloor(this, mychip->floor);
			DoC_SelectChip(this, mychip->chip);
		} else if (this->curchip != mychip->chip) {
			DoC_SelectChip(this, mychip->chip);
		}

		this->curfloor = mychip->floor;
		this->curchip = mychip->chip;

		DoC_Command(this,
			    (!this->page256
			     && (from & 0x100)) ? NAND_CMD_READ1 : NAND_CMD_READ0,
			    CDSN_CTRL_WP);
		DoC_Address(this, ADDR_COLUMN_PAGE, from, CDSN_CTRL_WP,
			    CDSN_CTRL_ECC_IO);

		
		WriteDOC(DOC_ECC_RESET, docptr, ECCConf);
		WriteDOC(DOC_ECC_EN, docptr, ECCConf);

		
		if (this->page256 && from + len > (from | 0xff) + 1) {
			len256 = (from | 0xff) + 1 - from;
			DoC_ReadBuf(this, buf, len256);

			DoC_Command(this, NAND_CMD_READ0, CDSN_CTRL_WP);
			DoC_Address(this, ADDR_COLUMN_PAGE, from + len256,
				    CDSN_CTRL_WP, CDSN_CTRL_ECC_IO);
		}

		DoC_ReadBuf(this, &buf[len256], len - len256);

		
		*retlen += len;

		
		
		
		DoC_ReadBuf(this, eccbuf, 6);

		
		if (DoC_is_Millennium(this)) {
			dummy = ReadDOC(docptr, ECCConf);
			dummy = ReadDOC(docptr, ECCConf);
			i = ReadDOC(docptr, ECCConf);
		} else {
			dummy = ReadDOC(docptr, 2k_ECCStatus);
			dummy = ReadDOC(docptr, 2k_ECCStatus);
			i = ReadDOC(docptr, 2k_ECCStatus);
		}

		
		if (i & 0x80) {
			int nb_errors;
			
#ifdef ECC_DEBUG
			printk(KERN_ERR "DiskOnChip ECC Error: Read at %lx\n", (long)from);
#endif
			for (i = 0; i < 6; i++) {
				syndrome[i] =
					ReadDOC(docptr, ECCSyndrome0 + i);
			}
			nb_errors = doc_decode_ecc(buf, syndrome);

#ifdef ECC_DEBUG
			printk(KERN_ERR "Errors corrected: %x\n", nb_errors);
#endif
			if (nb_errors < 0) {
				ret = -EIO;
			}
		}

#ifdef PSYCHO_DEBUG
		printk(KERN_DEBUG "ECC DATA at %lxB: %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",
		       (long)from, eccbuf[0], eccbuf[1], eccbuf[2],
		       eccbuf[3], eccbuf[4], eccbuf[5]);
#endif

		
		WriteDOC(DOC_ECC_DIS, docptr , ECCConf);

		if(0 == ((from + len) & 0x1ff))
		{
		    DoC_WaitReady(this);
		}

		from += len;
		left -= len;
		buf += len;
	}

	mutex_unlock(&this->lock);

	return ret;
}

static int doc_write(struct mtd_info *mtd, loff_t to, size_t len,
		     size_t * retlen, const u_char * buf)
{
	struct DiskOnChip *this = mtd->priv;
	int di; 
	void __iomem *docptr = this->virtadr;
	unsigned char eccbuf[6];
	volatile char dummy;
	int len256 = 0;
	struct Nand *mychip;
	size_t left = len;
	int status;

	mutex_lock(&this->lock);
	while (left) {
		len = left;

		
		if (to + len > ((to | 0x1ff) + 1))
			len = ((to | 0x1ff) + 1) - to;

		/* The ECC will not be calculated correctly if less than 512 is written */

		

		
		mychip = &this->chips[to >> (this->chipshift)];

		if (this->curfloor != mychip->floor) {
			DoC_SelectFloor(this, mychip->floor);
			DoC_SelectChip(this, mychip->chip);
		} else if (this->curchip != mychip->chip) {
			DoC_SelectChip(this, mychip->chip);
		}

		this->curfloor = mychip->floor;
		this->curchip = mychip->chip;

		
		DoC_Command(this, NAND_CMD_RESET, CDSN_CTRL_WP);
		DoC_Command(this,
			    (!this->page256
			     && (to & 0x100)) ? NAND_CMD_READ1 : NAND_CMD_READ0,
			    CDSN_CTRL_WP);

		DoC_Command(this, NAND_CMD_SEQIN, 0);
		DoC_Address(this, ADDR_COLUMN_PAGE, to, 0, CDSN_CTRL_ECC_IO);

		
		WriteDOC(DOC_ECC_RESET, docptr, ECCConf);
		WriteDOC(DOC_ECC_EN | DOC_ECC_RW, docptr, ECCConf);

		
		if (this->page256 && to + len > (to | 0xff) + 1) {
			len256 = (to | 0xff) + 1 - to;
			DoC_WriteBuf(this, buf, len256);

			DoC_Command(this, NAND_CMD_PAGEPROG, 0);

			DoC_Command(this, NAND_CMD_STATUS, CDSN_CTRL_WP);
			

			dummy = ReadDOC(docptr, CDSNSlowIO);
			DoC_Delay(this, 2);

			if (ReadDOC_(docptr, this->ioreg) & 1) {
				printk(KERN_ERR "Error programming flash\n");
				
				*retlen = 0;
				mutex_unlock(&this->lock);
				return -EIO;
			}

			DoC_Command(this, NAND_CMD_SEQIN, 0);
			DoC_Address(this, ADDR_COLUMN_PAGE, to + len256, 0,
				    CDSN_CTRL_ECC_IO);
		}

		DoC_WriteBuf(this, &buf[len256], len - len256);

		WriteDOC(CDSN_CTRL_ECC_IO | CDSN_CTRL_CE, docptr, CDSNControl);

		if (DoC_is_Millennium(this)) {
			WriteDOC(0, docptr, NOP);
			WriteDOC(0, docptr, NOP);
			WriteDOC(0, docptr, NOP);
		} else {
			WriteDOC_(0, docptr, this->ioreg);
			WriteDOC_(0, docptr, this->ioreg);
			WriteDOC_(0, docptr, this->ioreg);
		}

		WriteDOC(CDSN_CTRL_ECC_IO | CDSN_CTRL_FLASH_IO | CDSN_CTRL_CE, docptr,
			 CDSNControl);

		
		for (di = 0; di < 6; di++) {
			eccbuf[di] = ReadDOC(docptr, ECCSyndrome0 + di);
		}

		
		WriteDOC(DOC_ECC_DIS, docptr, ECCConf);

#ifdef PSYCHO_DEBUG
		printk
			("OOB data at %lx is %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",
			 (long) to, eccbuf[0], eccbuf[1], eccbuf[2], eccbuf[3],
			 eccbuf[4], eccbuf[5]);
#endif
		DoC_Command(this, NAND_CMD_PAGEPROG, 0);

		DoC_Command(this, NAND_CMD_STATUS, CDSN_CTRL_WP);
		

		if (DoC_is_Millennium(this)) {
			ReadDOC(docptr, ReadPipeInit);
			status = ReadDOC(docptr, LastDataRead);
		} else {
			dummy = ReadDOC(docptr, CDSNSlowIO);
			DoC_Delay(this, 2);
			status = ReadDOC_(docptr, this->ioreg);
		}

		if (status & 1) {
			printk(KERN_ERR "Error programming flash\n");
			
			*retlen = 0;
			mutex_unlock(&this->lock);
			return -EIO;
		}

		
		*retlen += len;

		{
			unsigned char x[8];
			size_t dummy;
			int ret;

			
			for (di=0; di<6; di++)
				x[di] = eccbuf[di];

			x[6]=0x55;
			x[7]=0x55;

			ret = doc_write_oob_nolock(mtd, to, 8, &dummy, x);
			if (ret) {
				mutex_unlock(&this->lock);
				return ret;
			}
		}

		to += len;
		left -= len;
		buf += len;
	}

	mutex_unlock(&this->lock);
	return 0;
}

static int doc_read_oob(struct mtd_info *mtd, loff_t ofs,
			struct mtd_oob_ops *ops)
{
	struct DiskOnChip *this = mtd->priv;
	int len256 = 0, ret;
	struct Nand *mychip;
	uint8_t *buf = ops->oobbuf;
	size_t len = ops->len;

	BUG_ON(ops->mode != MTD_OPS_PLACE_OOB);

	ofs += ops->ooboffs;

	mutex_lock(&this->lock);

	mychip = &this->chips[ofs >> this->chipshift];

	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(this, mychip->floor);
		DoC_SelectChip(this, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(this, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	
	if (this->page256) {
		if (!(ofs & 0x8))
			ofs += 0x100;
		else
			ofs -= 0x8;
	}

	DoC_Command(this, NAND_CMD_READOOB, CDSN_CTRL_WP);
	DoC_Address(this, ADDR_COLUMN_PAGE, ofs, CDSN_CTRL_WP, 0);

	
	
	
	if (this->page256 && ofs + len > (ofs | 0x7) + 1) {
		len256 = (ofs | 0x7) + 1 - ofs;
		DoC_ReadBuf(this, buf, len256);

		DoC_Command(this, NAND_CMD_READOOB, CDSN_CTRL_WP);
		DoC_Address(this, ADDR_COLUMN_PAGE, ofs & (~0x1ff),
			    CDSN_CTRL_WP, 0);
	}

	DoC_ReadBuf(this, &buf[len256], len - len256);

	ops->retlen = len;

	ret = DoC_WaitReady(this);

	mutex_unlock(&this->lock);
	return ret;

}

static int doc_write_oob_nolock(struct mtd_info *mtd, loff_t ofs, size_t len,
				size_t * retlen, const u_char * buf)
{
	struct DiskOnChip *this = mtd->priv;
	int len256 = 0;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip = &this->chips[ofs >> this->chipshift];
	volatile int dummy;
	int status;

	
	

	
	if (this->curfloor != mychip->floor) {
		DoC_SelectFloor(this, mychip->floor);
		DoC_SelectChip(this, mychip->chip);
	} else if (this->curchip != mychip->chip) {
		DoC_SelectChip(this, mychip->chip);
	}
	this->curfloor = mychip->floor;
	this->curchip = mychip->chip;

	
	WriteDOC (DOC_ECC_RESET, docptr, ECCConf);
	WriteDOC (DOC_ECC_DIS, docptr, ECCConf);

	
	DoC_Command(this, NAND_CMD_RESET, CDSN_CTRL_WP);

	
	DoC_Command(this, NAND_CMD_READOOB, CDSN_CTRL_WP);

	
	
	if (this->page256) {
		if (!(ofs & 0x8))
			ofs += 0x100;
		else
			ofs -= 0x8;
	}

	
	DoC_Command(this, NAND_CMD_SEQIN, 0);
	DoC_Address(this, ADDR_COLUMN_PAGE, ofs, 0, 0);

	
	
	
	if (this->page256 && ofs + len > (ofs | 0x7) + 1) {
		len256 = (ofs | 0x7) + 1 - ofs;
		DoC_WriteBuf(this, buf, len256);

		DoC_Command(this, NAND_CMD_PAGEPROG, 0);
		DoC_Command(this, NAND_CMD_STATUS, 0);
		

		if (DoC_is_Millennium(this)) {
			ReadDOC(docptr, ReadPipeInit);
			status = ReadDOC(docptr, LastDataRead);
		} else {
			dummy = ReadDOC(docptr, CDSNSlowIO);
			DoC_Delay(this, 2);
			status = ReadDOC_(docptr, this->ioreg);
		}

		if (status & 1) {
			printk(KERN_ERR "Error programming oob data\n");
			
			*retlen = 0;
			return -EIO;
		}
		DoC_Command(this, NAND_CMD_SEQIN, 0);
		DoC_Address(this, ADDR_COLUMN_PAGE, ofs & (~0x1ff), 0, 0);
	}

	DoC_WriteBuf(this, &buf[len256], len - len256);

	DoC_Command(this, NAND_CMD_PAGEPROG, 0);
	DoC_Command(this, NAND_CMD_STATUS, 0);
	

	if (DoC_is_Millennium(this)) {
		ReadDOC(docptr, ReadPipeInit);
		status = ReadDOC(docptr, LastDataRead);
	} else {
		dummy = ReadDOC(docptr, CDSNSlowIO);
		DoC_Delay(this, 2);
		status = ReadDOC_(docptr, this->ioreg);
	}

	if (status & 1) {
		printk(KERN_ERR "Error programming oob data\n");
		
		*retlen = 0;
		return -EIO;
	}

	*retlen = len;
	return 0;

}

static int doc_write_oob(struct mtd_info *mtd, loff_t ofs,
			 struct mtd_oob_ops *ops)
{
	struct DiskOnChip *this = mtd->priv;
	int ret;

	BUG_ON(ops->mode != MTD_OPS_PLACE_OOB);

	mutex_lock(&this->lock);
	ret = doc_write_oob_nolock(mtd, ofs + ops->ooboffs, ops->len,
				   &ops->retlen, ops->oobbuf);

	mutex_unlock(&this->lock);
	return ret;
}

static int doc_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct DiskOnChip *this = mtd->priv;
	__u32 ofs = instr->addr;
	__u32 len = instr->len;
	volatile int dummy;
	void __iomem *docptr = this->virtadr;
	struct Nand *mychip;
	int status;

 	mutex_lock(&this->lock);

	if (ofs & (mtd->erasesize-1) || len & (mtd->erasesize-1)) {
		mutex_unlock(&this->lock);
		return -EINVAL;
	}

	instr->state = MTD_ERASING;

	
	while(len) {
		mychip = &this->chips[ofs >> this->chipshift];

		if (this->curfloor != mychip->floor) {
			DoC_SelectFloor(this, mychip->floor);
			DoC_SelectChip(this, mychip->chip);
		} else if (this->curchip != mychip->chip) {
			DoC_SelectChip(this, mychip->chip);
		}
		this->curfloor = mychip->floor;
		this->curchip = mychip->chip;

		DoC_Command(this, NAND_CMD_ERASE1, 0);
		DoC_Address(this, ADDR_PAGE, ofs, 0, 0);
		DoC_Command(this, NAND_CMD_ERASE2, 0);

		DoC_Command(this, NAND_CMD_STATUS, CDSN_CTRL_WP);

		if (DoC_is_Millennium(this)) {
			ReadDOC(docptr, ReadPipeInit);
			status = ReadDOC(docptr, LastDataRead);
		} else {
			dummy = ReadDOC(docptr, CDSNSlowIO);
			DoC_Delay(this, 2);
			status = ReadDOC_(docptr, this->ioreg);
		}

		if (status & 1) {
			printk(KERN_ERR "Error erasing at 0x%x\n", ofs);
			
			instr->state = MTD_ERASE_FAILED;
			goto callback;
		}
		ofs += mtd->erasesize;
		len -= mtd->erasesize;
	}
	instr->state = MTD_ERASE_DONE;

 callback:
	mtd_erase_callback(instr);

	mutex_unlock(&this->lock);
	return 0;
}



static void __exit cleanup_doc2000(void)
{
	struct mtd_info *mtd;
	struct DiskOnChip *this;

	while ((mtd = doc2klist)) {
		this = mtd->priv;
		doc2klist = this->nextdoc;

		mtd_device_unregister(mtd);

		iounmap(this->virtadr);
		kfree(this->chips);
		kfree(mtd);
	}
}

module_exit(cleanup_doc2000);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org> et al.");
MODULE_DESCRIPTION("MTD driver for DiskOnChip 2000 and Millennium");

