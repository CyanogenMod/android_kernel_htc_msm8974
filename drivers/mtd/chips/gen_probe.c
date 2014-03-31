/*
 * Routines common to all CFI-type probes.
 * (C) 2001-2003 Red Hat, Inc.
 * GPL'd
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/gen_probe.h>

static struct mtd_info *check_cmd_set(struct map_info *, int);
static struct cfi_private *genprobe_ident_chips(struct map_info *map,
						struct chip_probe *cp);
static int genprobe_new_chip(struct map_info *map, struct chip_probe *cp,
			     struct cfi_private *cfi);

struct mtd_info *mtd_do_chip_probe(struct map_info *map, struct chip_probe *cp)
{
	struct mtd_info *mtd = NULL;
	struct cfi_private *cfi;

	
	cfi = genprobe_ident_chips(map, cp);

	if (!cfi)
		return NULL;

	map->fldrv_priv = cfi;
	

	mtd = check_cmd_set(map, 1); 
	if (!mtd)
		mtd = check_cmd_set(map, 0); 

	if (mtd) {
		if (mtd->size > map->size) {
			printk(KERN_WARNING "Reducing visibility of %ldKiB chip to %ldKiB\n",
			       (unsigned long)mtd->size >> 10,
			       (unsigned long)map->size >> 10);
			mtd->size = map->size;
		}
		return mtd;
	}

	printk(KERN_WARNING"gen_probe: No supported Vendor Command Set found\n");

	kfree(cfi->cfiq);
	kfree(cfi);
	map->fldrv_priv = NULL;
	return NULL;
}
EXPORT_SYMBOL(mtd_do_chip_probe);


static struct cfi_private *genprobe_ident_chips(struct map_info *map, struct chip_probe *cp)
{
	struct cfi_private cfi;
	struct cfi_private *retcfi;
	unsigned long *chip_map;
	int i, j, mapsize;
	int max_chips;

	memset(&cfi, 0, sizeof(cfi));

	if (!genprobe_new_chip(map, cp, &cfi)) {
		
		pr_debug("%s: Found no %s device at location zero\n",
			 cp->name, map->name);
		return NULL;
	}

#if 0 
	if (cfi.cfiq->NumEraseRegions == 0) {
		printk(KERN_WARNING "Number of erase regions is zero\n");
		kfree(cfi.cfiq);
		return NULL;
	}
#endif
	cfi.chipshift = cfi.cfiq->DevSize;

	if (cfi_interleave_is_1(&cfi)) {
		;
	} else if (cfi_interleave_is_2(&cfi)) {
		cfi.chipshift++;
	} else if (cfi_interleave_is_4((&cfi))) {
		cfi.chipshift += 2;
	} else if (cfi_interleave_is_8(&cfi)) {
		cfi.chipshift += 3;
	} else {
		BUG();
	}

	cfi.numchips = 1;

	max_chips = map->size >> cfi.chipshift;
	if (!max_chips) {
		printk(KERN_WARNING "NOR chip too large to fit in mapping. Attempting to cope...\n");
		max_chips = 1;
	}

	mapsize = sizeof(long) * DIV_ROUND_UP(max_chips, BITS_PER_LONG);
	chip_map = kzalloc(mapsize, GFP_KERNEL);
	if (!chip_map) {
		printk(KERN_WARNING "%s: kmalloc failed for CFI chip map\n", map->name);
		kfree(cfi.cfiq);
		return NULL;
	}

	set_bit(0, chip_map); 


	for (i = 1; i < max_chips; i++) {
		cp->probe_chip(map, i << cfi.chipshift, chip_map, &cfi);
	}


	retcfi = kmalloc(sizeof(struct cfi_private) + cfi.numchips * sizeof(struct flchip), GFP_KERNEL);

	if (!retcfi) {
		printk(KERN_WARNING "%s: kmalloc failed for CFI private structure\n", map->name);
		kfree(cfi.cfiq);
		kfree(chip_map);
		return NULL;
	}

	memcpy(retcfi, &cfi, sizeof(cfi));
	memset(&retcfi->chips[0], 0, sizeof(struct flchip) * cfi.numchips);

	for (i = 0, j = 0; (j < cfi.numchips) && (i < max_chips); i++) {
		if(test_bit(i, chip_map)) {
			struct flchip *pchip = &retcfi->chips[j++];

			pchip->start = (i << cfi.chipshift);
			pchip->state = FL_READY;
			init_waitqueue_head(&pchip->wq);
			mutex_init(&pchip->mutex);
		}
	}

	kfree(chip_map);
	return retcfi;
}


static int genprobe_new_chip(struct map_info *map, struct chip_probe *cp,
			     struct cfi_private *cfi)
{
	int min_chips = (map_bankwidth(map)/4?:1); 
	int max_chips = map_bankwidth(map); 
	int nr_chips, type;

	for (nr_chips = max_chips; nr_chips >= min_chips; nr_chips >>= 1) {

		if (!cfi_interleave_supported(nr_chips))
		    continue;

		cfi->interleave = nr_chips;

		type = map_bankwidth(map) / nr_chips;

		for (; type <= CFI_DEVICETYPE_X32; type<<=1) {
			cfi->device_type = type;

			if (cp->probe_chip(map, 0, NULL, cfi))
				return 1;
		}
	}
	return 0;
}

typedef struct mtd_info *cfi_cmdset_fn_t(struct map_info *, int);

extern cfi_cmdset_fn_t cfi_cmdset_0001;
extern cfi_cmdset_fn_t cfi_cmdset_0002;
extern cfi_cmdset_fn_t cfi_cmdset_0020;

static inline struct mtd_info *cfi_cmdset_unknown(struct map_info *map,
						  int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	__u16 type = primary?cfi->cfiq->P_ID:cfi->cfiq->A_ID;
#ifdef CONFIG_MODULES
	char probename[16+sizeof(MODULE_SYMBOL_PREFIX)];
	cfi_cmdset_fn_t *probe_function;

	sprintf(probename, MODULE_SYMBOL_PREFIX "cfi_cmdset_%4.4X", type);

	probe_function = __symbol_get(probename);
	if (!probe_function) {
		request_module(probename + sizeof(MODULE_SYMBOL_PREFIX) - 1);
		probe_function = __symbol_get(probename);
	}

	if (probe_function) {
		struct mtd_info *mtd;

		mtd = (*probe_function)(map, primary);
		
		symbol_put_addr(probe_function);
		return mtd;
	}
#endif
	printk(KERN_NOTICE "Support for command set %04X not present\n", type);

	return NULL;
}

static struct mtd_info *check_cmd_set(struct map_info *map, int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	__u16 type = primary?cfi->cfiq->P_ID:cfi->cfiq->A_ID;

	if (type == P_ID_NONE || type == P_ID_RESERVED)
		return NULL;

	switch(type){
#ifdef CONFIG_MTD_CFI_INTELEXT
	case P_ID_INTEL_EXT:
	case P_ID_INTEL_STD:
	case P_ID_INTEL_PERFORMANCE:
		return cfi_cmdset_0001(map, primary);
#endif
#ifdef CONFIG_MTD_CFI_AMDSTD
	case P_ID_AMD_STD:
	case P_ID_SST_OLD:
	case P_ID_WINBOND:
		return cfi_cmdset_0002(map, primary);
#endif
#ifdef CONFIG_MTD_CFI_STAA
        case P_ID_ST_ADV:
		return cfi_cmdset_0020(map, primary);
#endif
	default:
		return cfi_cmdset_unknown(map, primary);
	}
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Helper routines for flash chip probe code");
