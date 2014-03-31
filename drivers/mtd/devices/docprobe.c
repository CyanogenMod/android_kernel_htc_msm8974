




#define DOC_SINGLE_DRIVER

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/doc2000.h>


static unsigned long doc_config_location = CONFIG_MTD_DOCPROBE_ADDRESS;
module_param(doc_config_location, ulong, 0);
MODULE_PARM_DESC(doc_config_location, "Physical memory address at which to probe for DiskOnChip");

static unsigned long __initdata doc_locations[] = {
#if defined (__alpha__) || defined(__i386__) || defined(__x86_64__)
#ifdef CONFIG_MTD_DOCPROBE_HIGH
	0xfffc8000, 0xfffca000, 0xfffcc000, 0xfffce000,
	0xfffd0000, 0xfffd2000, 0xfffd4000, 0xfffd6000,
	0xfffd8000, 0xfffda000, 0xfffdc000, 0xfffde000,
	0xfffe0000, 0xfffe2000, 0xfffe4000, 0xfffe6000,
	0xfffe8000, 0xfffea000, 0xfffec000, 0xfffee000,
#else 
	0xc8000, 0xca000, 0xcc000, 0xce000,
	0xd0000, 0xd2000, 0xd4000, 0xd6000,
	0xd8000, 0xda000, 0xdc000, 0xde000,
	0xe0000, 0xe2000, 0xe4000, 0xe6000,
	0xe8000, 0xea000, 0xec000, 0xee000,
#endif 
#else
#warning Unknown architecture for DiskOnChip. No default probe locations defined
#endif
	0xffffffff };


static inline int __init doccheck(void __iomem *potential, unsigned long physadr)
{
	void __iomem *window=potential;
	unsigned char tmp, tmpb, tmpc, ChipID;
#ifndef DOC_PASSIVE_PROBE
	unsigned char tmp2;
#endif

	

#ifdef CONFIG_MTD_DOCPROBE_55AA
	if (ReadDOC(window, Sig1) != 0x55 || ReadDOC(window, Sig2) != 0xaa)
		return 0;
#endif 

#ifndef DOC_PASSIVE_PROBE
	tmp2 = ReadDOC(window, DOCControl);

	
	WriteDOC(DOC_MODE_CLR_ERR | DOC_MODE_MDWREN | DOC_MODE_RESET,
		 window, DOCControl);
	WriteDOC(DOC_MODE_CLR_ERR | DOC_MODE_MDWREN | DOC_MODE_RESET,
		 window, DOCControl);

	
	WriteDOC(DOC_MODE_CLR_ERR | DOC_MODE_MDWREN | DOC_MODE_NORMAL,
		 window, DOCControl);
	WriteDOC(DOC_MODE_CLR_ERR | DOC_MODE_MDWREN | DOC_MODE_NORMAL,
		 window, DOCControl);
#endif 

	ChipID = ReadDOC(window, ChipID);

	switch (ChipID) {
	case DOC_ChipID_Doc2k:
		
		tmp  = ReadDOC(window, 2k_ECCStatus) & DOC_TOGGLE_BIT;
		tmpb = ReadDOC(window, 2k_ECCStatus) & DOC_TOGGLE_BIT;
		tmpc = ReadDOC(window, 2k_ECCStatus) & DOC_TOGGLE_BIT;
		if (tmp != tmpb && tmp == tmpc)
				return ChipID;
		break;

	case DOC_ChipID_DocMil:
		
		ReadDOC(window, ChipID);
		ReadDOC(window, ChipID);
		if (ReadDOC(window, ChipID) != DOC_ChipID_DocMil)
			ChipID = DOC_ChipID_Doc2kTSOP;

		
		tmp  = ReadDOC(window, ECCConf) & DOC_TOGGLE_BIT;
		tmpb = ReadDOC(window, ECCConf) & DOC_TOGGLE_BIT;
		tmpc = ReadDOC(window, ECCConf) & DOC_TOGGLE_BIT;
		if (tmp != tmpb && tmp == tmpc)
				return ChipID;
		break;

	case DOC_ChipID_DocMilPlus16:
	case DOC_ChipID_DocMilPlus32:
	case 0:
		
#ifndef DOC_PASSIVE_PROBE
		
		for (tmp = 0; (tmp < 4); tmp++)
			ReadDOC(window, Mplus_Power);

		
		tmp = DOC_MODE_RESET | DOC_MODE_MDWREN | DOC_MODE_RST_LAT |
			DOC_MODE_BDECT;
		WriteDOC(tmp, window, Mplus_DOCControl);
		WriteDOC(~tmp, window, Mplus_CtrlConfirm);

		mdelay(1);
		
		tmp = DOC_MODE_NORMAL | DOC_MODE_MDWREN | DOC_MODE_RST_LAT |
			DOC_MODE_BDECT;
		WriteDOC(tmp, window, Mplus_DOCControl);
		WriteDOC(~tmp, window, Mplus_CtrlConfirm);
		mdelay(1);
#endif 

		ChipID = ReadDOC(window, ChipID);

		switch (ChipID) {
		case DOC_ChipID_DocMilPlus16:
		case DOC_ChipID_DocMilPlus32:
			
			tmp  = ReadDOC(window, Mplus_Toggle) & DOC_TOGGLE_BIT;
			tmpb = ReadDOC(window, Mplus_Toggle) & DOC_TOGGLE_BIT;
			tmpc = ReadDOC(window, Mplus_Toggle) & DOC_TOGGLE_BIT;
			if (tmp != tmpb && tmp == tmpc)
					return ChipID;
		default:
			break;
		}
		

	default:

#ifdef CONFIG_MTD_DOCPROBE_55AA
		printk(KERN_DEBUG "Possible DiskOnChip with unknown ChipID %2.2X found at 0x%lx\n",
		       ChipID, physadr);
#endif
#ifndef DOC_PASSIVE_PROBE
		WriteDOC(tmp2, window, DOCControl);
#endif
		return 0;
	}

	printk(KERN_WARNING "DiskOnChip failed TOGGLE test, dropping.\n");

#ifndef DOC_PASSIVE_PROBE
	
	WriteDOC(tmp2, window, DOCControl);
#endif
	return 0;
}

static int docfound;

extern void DoC2k_init(struct mtd_info *);
extern void DoCMil_init(struct mtd_info *);
extern void DoCMilPlus_init(struct mtd_info *);

static void __init DoC_Probe(unsigned long physadr)
{
	void __iomem *docptr;
	struct DiskOnChip *this;
	struct mtd_info *mtd;
	int ChipID;
	char namebuf[15];
	char *name = namebuf;
	void (*initroutine)(struct mtd_info *) = NULL;

	docptr = ioremap(physadr, DOC_IOREMAP_LEN);

	if (!docptr)
		return;

	if ((ChipID = doccheck(docptr, physadr))) {
		if (ChipID == DOC_ChipID_Doc2kTSOP) {
			
			printk(KERN_NOTICE "Refusing to drive DiskOnChip 2000 TSOP until Bad Block Table is correctly supported by INFTL\n");
			iounmap(docptr);
			return;
		}
		docfound = 1;
		mtd = kzalloc(sizeof(struct DiskOnChip) + sizeof(struct mtd_info), GFP_KERNEL);
		if (!mtd) {
			printk(KERN_WARNING "Cannot allocate memory for data structures. Dropping.\n");
			iounmap(docptr);
			return;
		}

		this = (struct DiskOnChip *)(&mtd[1]);
		mtd->priv = this;
		this->virtadr = docptr;
		this->physadr = physadr;
		this->ChipID = ChipID;
		sprintf(namebuf, "with ChipID %2.2X", ChipID);

		switch(ChipID) {
		case DOC_ChipID_Doc2kTSOP:
			name="2000 TSOP";
			initroutine = symbol_request(DoC2k_init);
			break;

		case DOC_ChipID_Doc2k:
			name="2000";
			initroutine = symbol_request(DoC2k_init);
			break;

		case DOC_ChipID_DocMil:
			name="Millennium";
#ifdef DOC_SINGLE_DRIVER
			initroutine = symbol_request(DoC2k_init);
#else
			initroutine = symbol_request(DoCMil_init);
#endif 
			break;

		case DOC_ChipID_DocMilPlus16:
		case DOC_ChipID_DocMilPlus32:
			name="MillenniumPlus";
			initroutine = symbol_request(DoCMilPlus_init);
			break;
		}

		if (initroutine) {
			(*initroutine)(mtd);
			symbol_put_addr(initroutine);
			return;
		}
		printk(KERN_NOTICE "Cannot find driver for DiskOnChip %s at 0x%lX\n", name, physadr);
		kfree(mtd);
	}
	iounmap(docptr);
}



static int __init init_doc(void)
{
	int i;

	if (doc_config_location) {
		printk(KERN_INFO "Using configured DiskOnChip probe address 0x%lx\n", doc_config_location);
		DoC_Probe(doc_config_location);
	} else {
		for (i=0; (doc_locations[i] != 0xffffffff); i++) {
			DoC_Probe(doc_locations[i]);
		}
	}
	if (!docfound)
		printk(KERN_INFO "No recognised DiskOnChip devices found\n");
	return -EAGAIN;
}

module_init(init_doc);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Probe code for DiskOnChip 2000 and Millennium devices");

