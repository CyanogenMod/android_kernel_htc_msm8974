/*
 * Physical mapping layer for MTD using the Axis partitiontable format
 *
 * Copyright (c) 2001-2007 Axis Communications AB
 *
 * This file is under the GPL.
 *
 * First partition is always sector 0 regardless of if we find a partitiontable
 * or not. In the start of the next sector, there can be a partitiontable that
 * tells us what other partitions to define. If there isn't, we use a default
 * partition split defined below.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <linux/mtd/concat.h>
#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/mtdram.h>
#include <linux/mtd/partitions.h>

#include <linux/cramfs_fs.h>

#include <asm/axisflashmap.h>
#include <asm/mmu.h>

#define MEM_CSE0_SIZE (0x04000000)
#define MEM_CSE1_SIZE (0x04000000)

#define FLASH_UNCACHED_ADDR  KSEG_E
#define FLASH_CACHED_ADDR    KSEG_F

#define PAGESIZE (512)

#if CONFIG_ETRAX_FLASH_BUSWIDTH==1
#define flash_data __u8
#elif CONFIG_ETRAX_FLASH_BUSWIDTH==2
#define flash_data __u16
#elif CONFIG_ETRAX_FLASH_BUSWIDTH==4
#define flash_data __u32
#endif

extern unsigned long romfs_in_flash; 
extern unsigned long romfs_start, romfs_length;
extern unsigned long nand_boot; 

struct partition_name {
	char name[6];
};

struct mtd_info* axisflash_mtd = NULL;


static map_word flash_read(struct map_info *map, unsigned long ofs)
{
	map_word tmp;
	tmp.x[0] = *(flash_data *)(map->map_priv_1 + ofs);
	return tmp;
}

static void flash_copy_from(struct map_info *map, void *to,
			    unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void flash_write(struct map_info *map, map_word d, unsigned long adr)
{
	*(flash_data *)(map->map_priv_1 + adr) = (flash_data)d.x[0];
}

static struct map_info map_cse0 = {
	.name = "cse0",
	.size = MEM_CSE0_SIZE,
	.bankwidth = CONFIG_ETRAX_FLASH_BUSWIDTH,
	.read = flash_read,
	.copy_from = flash_copy_from,
	.write = flash_write,
	.map_priv_1 = FLASH_UNCACHED_ADDR
};

static struct map_info map_cse1 = {
	.name = "cse1",
	.size = MEM_CSE1_SIZE,
	.bankwidth = CONFIG_ETRAX_FLASH_BUSWIDTH,
	.read = flash_read,
	.copy_from = flash_copy_from,
	.write = flash_write,
	.map_priv_1 = FLASH_UNCACHED_ADDR + MEM_CSE0_SIZE
};

#define MAX_PARTITIONS			7
#ifdef CONFIG_ETRAX_NANDBOOT
#define NUM_DEFAULT_PARTITIONS		4
#define DEFAULT_ROOTFS_PARTITION_NO	2
#define DEFAULT_MEDIA_SIZE              0x2000000 
#else
#define NUM_DEFAULT_PARTITIONS		3
#define DEFAULT_ROOTFS_PARTITION_NO	(-1)
#define DEFAULT_MEDIA_SIZE              0x800000 
#endif

#if (MAX_PARTITIONS < NUM_DEFAULT_PARTITIONS)
#error MAX_PARTITIONS must be >= than NUM_DEFAULT_PARTITIONS
#endif

static struct mtd_partition axis_partitions[MAX_PARTITIONS] = {
	{
		.name = "part0",
		.size = CONFIG_ETRAX_PTABLE_SECTOR,
		.offset = 0
	},
	{
		.name = "part1",
		.size = 0,
		.offset = 0
	},
	{
		.name = "part2",
		.size = 0,
		.offset = 0
	},
	{
		.name = "part3",
		.size = 0,
		.offset = 0
	},
	{
		.name = "part4",
		.size = 0,
		.offset = 0
	},
	{
		.name = "part5",
		.size = 0,
		.offset = 0
	},
	{
		.name = "part6",
		.size = 0,
		.offset = 0
	},
};


static struct mtd_partition axis_default_partitions[NUM_DEFAULT_PARTITIONS] = {
	{
		.name = "boot firmware",
		.size = CONFIG_ETRAX_PTABLE_SECTOR,
		.offset = 0
	},
	{
		.name = "kernel",
		.size = 10 * CONFIG_ETRAX_PTABLE_SECTOR,
		.offset = CONFIG_ETRAX_PTABLE_SECTOR
	},
#define FILESYSTEM_SECTOR (11 * CONFIG_ETRAX_PTABLE_SECTOR)
#ifdef CONFIG_ETRAX_NANDBOOT
	{
		.name = "rootfs",
		.size = 10 * CONFIG_ETRAX_PTABLE_SECTOR,
		.offset = FILESYSTEM_SECTOR
	},
#undef FILESYSTEM_SECTOR
#define FILESYSTEM_SECTOR (21 * CONFIG_ETRAX_PTABLE_SECTOR)
#endif
	{
		.name = "rwfs",
		.size = DEFAULT_MEDIA_SIZE - FILESYSTEM_SECTOR,
		.offset = FILESYSTEM_SECTOR
	}
};

#ifdef CONFIG_ETRAX_AXISFLASHMAP_MTD0WHOLE
static struct mtd_partition main_partition = {
	.name = "main",
	.size = 0,
	.offset = 0
};
#endif

static struct mtd_partition aux_partition = {
	.name = "aux",
	.size = 0,
	.offset = 0
};

static struct mtd_info *probe_cs(struct map_info *map_cs)
{
	struct mtd_info *mtd_cs = NULL;

	printk(KERN_INFO
	       "%s: Probing a 0x%08lx bytes large window at 0x%08lx.\n",
	       map_cs->name, map_cs->size, map_cs->map_priv_1);

#ifdef CONFIG_MTD_CFI
	mtd_cs = do_map_probe("cfi_probe", map_cs);
#endif
#ifdef CONFIG_MTD_JEDECPROBE
	if (!mtd_cs)
		mtd_cs = do_map_probe("jedec_probe", map_cs);
#endif

	return mtd_cs;
}

extern struct mtd_info* __init crisv32_nand_flash_probe (void);
static struct mtd_info *flash_probe(void)
{
	struct mtd_info *mtd_cse0;
	struct mtd_info *mtd_cse1;
	struct mtd_info *mtd_total;
	struct mtd_info *mtds[2];
	int count = 0;

	if ((mtd_cse0 = probe_cs(&map_cse0)) != NULL)
		mtds[count++] = mtd_cse0;
	if ((mtd_cse1 = probe_cs(&map_cse1)) != NULL)
		mtds[count++] = mtd_cse1;

	if (!mtd_cse0 && !mtd_cse1) {
		
		return NULL;
	}

	if (count > 1) {
		mtd_total = mtd_concat_create(mtds, count, "cse0+cse1");
		if (!mtd_total) {
			printk(KERN_ERR "%s and %s: Concatenation failed!\n",
				map_cse0.name, map_cse1.name);

			mtd_total = mtd_cse0;
			map_destroy(mtd_cse1);
		}
	} else
		mtd_total = mtd_cse0 ? mtd_cse0 : mtd_cse1;

	return mtd_total;
}

static int __init init_axis_flash(void)
{
	struct mtd_info *main_mtd;
	struct mtd_info *aux_mtd = NULL;
	int err = 0;
	int pidx = 0;
	struct partitiontable_head *ptable_head = NULL;
	struct partitiontable_entry *ptable;
	int ptable_ok = 0;
	static char page[PAGESIZE];
	size_t len;
	int ram_rootfs_partition = -1; 
	int part;

#if !defined(CONFIG_MTD_MTDRAM) || (CONFIG_MTDRAM_TOTAL_SIZE != 0) || (CONFIG_MTDRAM_ABS_POS != 0)
	if (!romfs_in_flash && !nand_boot) {
		printk(KERN_EMERG "axisflashmap: Cannot create an MTD RAM "
		       "device; configure CONFIG_MTD_MTDRAM with size = 0!\n");
		panic("This kernel cannot boot from RAM!\n");
	}
#endif

#ifndef CONFIG_ETRAX_VCS_SIM
	main_mtd = flash_probe();
	if (main_mtd)
		printk(KERN_INFO "%s: 0x%08x bytes of NOR flash memory.\n",
		       main_mtd->name, main_mtd->size);

#ifdef CONFIG_ETRAX_NANDFLASH
	aux_mtd = crisv32_nand_flash_probe();
	if (aux_mtd)
		printk(KERN_INFO "%s: 0x%08x bytes of NAND flash memory.\n",
			aux_mtd->name, aux_mtd->size);

#ifdef CONFIG_ETRAX_NANDBOOT
	{
		struct mtd_info *tmp_mtd;

		printk(KERN_INFO "axisflashmap: Set to boot from NAND flash, "
		       "making NAND flash primary device.\n");
		tmp_mtd = main_mtd;
		main_mtd = aux_mtd;
		aux_mtd = tmp_mtd;
	}
#endif 
#endif 

	if (!main_mtd && !aux_mtd) {
		printk(KERN_INFO "axisflashmap: Found no flash chip.\n");
	}

#if 0 
	if (main_mtd) {
		int sectoraddr, i;
		for (sectoraddr = 0; sectoraddr < 2*65536+4096;
				sectoraddr += PAGESIZE) {
			main_mtd->read(main_mtd, sectoraddr, PAGESIZE, &len,
				page);
			printk(KERN_INFO
			       "Sector at %d (length %d):\n",
			       sectoraddr, len);
			for (i = 0; i < PAGESIZE; i += 16) {
				printk(KERN_INFO
				       "%02x %02x %02x %02x "
				       "%02x %02x %02x %02x "
				       "%02x %02x %02x %02x "
				       "%02x %02x %02x %02x\n",
				       page[i] & 255, page[i+1] & 255,
				       page[i+2] & 255, page[i+3] & 255,
				       page[i+4] & 255, page[i+5] & 255,
				       page[i+6] & 255, page[i+7] & 255,
				       page[i+8] & 255, page[i+9] & 255,
				       page[i+10] & 255, page[i+11] & 255,
				       page[i+12] & 255, page[i+13] & 255,
				       page[i+14] & 255, page[i+15] & 255);
			}
		}
	}
#endif

	if (main_mtd) {
		main_mtd->owner = THIS_MODULE;
		axisflash_mtd = main_mtd;

		loff_t ptable_sector = CONFIG_ETRAX_PTABLE_SECTOR;

		
		pidx++;
#ifdef CONFIG_ETRAX_NANDBOOT
		int blockstat;
		do {
			blockstat = mtd_block_isbad(main_mtd, ptable_sector);
			if (blockstat < 0)
				ptable_sector = 0; 
			else if (blockstat)
				ptable_sector += main_mtd->erasesize;
		} while (blockstat && ptable_sector);
#endif
		if (ptable_sector) {
			mtd_read(main_mtd, ptable_sector, PAGESIZE, &len,
				 page);
			ptable_head = &((struct partitiontable *) page)->head;
		}

#if 0 
		printk(KERN_INFO
		       "axisflashmap: flash read %d bytes at 0x%08x, data: "
		       "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		       len, CONFIG_ETRAX_PTABLE_SECTOR,
		       page[0] & 255, page[1] & 255,
		       page[2] & 255, page[3] & 255,
		       page[4] & 255, page[5] & 255,
		       page[6] & 255, page[7] & 255);
		printk(KERN_INFO
		       "axisflashmap: partition table offset %d, data: "
		       "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		       PARTITION_TABLE_OFFSET,
		       page[PARTITION_TABLE_OFFSET+0] & 255,
		       page[PARTITION_TABLE_OFFSET+1] & 255,
		       page[PARTITION_TABLE_OFFSET+2] & 255,
		       page[PARTITION_TABLE_OFFSET+3] & 255,
		       page[PARTITION_TABLE_OFFSET+4] & 255,
		       page[PARTITION_TABLE_OFFSET+5] & 255,
		       page[PARTITION_TABLE_OFFSET+6] & 255,
		       page[PARTITION_TABLE_OFFSET+7] & 255);
#endif
	}

	if (ptable_head && (ptable_head->magic == PARTITION_TABLE_MAGIC)
	    && (ptable_head->size <
		(MAX_PARTITIONS * sizeof(struct partitiontable_entry) +
		PARTITIONTABLE_END_MARKER_SIZE))
	    && (*(unsigned long*)((void*)ptable_head + sizeof(*ptable_head) +
				  ptable_head->size -
				  PARTITIONTABLE_END_MARKER_SIZE)
		== PARTITIONTABLE_END_MARKER)) {
		struct partitiontable_entry *max_addr =
			(struct partitiontable_entry *)
			((unsigned long)ptable_head + sizeof(*ptable_head) +
			 ptable_head->size);
		unsigned long offset = CONFIG_ETRAX_PTABLE_SECTOR;
		unsigned char *p;
		unsigned long csum = 0;

		ptable = (struct partitiontable_entry *)
			((unsigned long)ptable_head + sizeof(*ptable_head));

		
		p = (unsigned char*) ptable;

		while (p <= (unsigned char*)max_addr) {
			csum += *p++;
			csum += *p++;
			csum += *p++;
			csum += *p++;
		}
		ptable_ok = (csum == ptable_head->checksum);

		
		printk(KERN_INFO "axisflashmap: "
		       "Found a%s partition table at 0x%p-0x%p.\n",
		       (ptable_ok ? " valid" : "n invalid"), ptable_head,
		       max_addr);

		while (ptable_ok
		       && ptable->offset != PARTITIONTABLE_END_MARKER
		       && ptable < max_addr
		       && pidx < MAX_PARTITIONS - 1) {

			axis_partitions[pidx].offset = offset + ptable->offset;
#ifdef CONFIG_ETRAX_NANDFLASH
			if (main_mtd->type == MTD_NANDFLASH) {
				axis_partitions[pidx].size =
					(((ptable+1)->offset ==
					  PARTITIONTABLE_END_MARKER) ?
					  main_mtd->size :
					  ((ptable+1)->offset + offset)) -
					(ptable->offset + offset);

			} else
#endif 
				axis_partitions[pidx].size = ptable->size;
#ifdef CONFIG_ETRAX_NANDBOOT
			if (!nand_boot &&
			    ram_rootfs_partition < 0 && 
			    ptable->type == PARTITION_TYPE_JFFS2 &&
			    (ptable->flags & PARTITION_FLAGS_READONLY_MASK) ==
				PARTITION_FLAGS_READONLY)
				ram_rootfs_partition = pidx;
#endif 
			pidx++;
			ptable++;
		}
	}

	
	

	struct mtd_partition *partition = &axis_partitions[0];
	if (main_mtd && !ptable_ok) {
		memcpy(axis_partitions, axis_default_partitions,
		       sizeof(axis_default_partitions));
		pidx = NUM_DEFAULT_PARTITIONS;
		ram_rootfs_partition = DEFAULT_ROOTFS_PARTITION_NO;
	}

	
	if (romfs_in_flash) {
		printk(KERN_INFO "axisflashmap: Adding partition for "
		       "overlapping root file system image\n");
		axis_partitions[pidx].size = romfs_length;
		axis_partitions[pidx].offset = romfs_start - FLASH_CACHED_ADDR;
		axis_partitions[pidx].name = "romfs";
		axis_partitions[pidx].mask_flags |= MTD_WRITEABLE;
		ram_rootfs_partition = -1;
		pidx++;
	} else if (romfs_length && !nand_boot) {
		if (ram_rootfs_partition < 0) {
			
			ram_rootfs_partition = pidx;
			pidx++;
		}
		printk(KERN_INFO "axisflashmap: Adding partition for "
		       "root file system image in RAM\n");
		axis_partitions[ram_rootfs_partition].size = romfs_length;
		axis_partitions[ram_rootfs_partition].offset = romfs_start;
		axis_partitions[ram_rootfs_partition].name = "romfs";
		axis_partitions[ram_rootfs_partition].mask_flags |=
			MTD_WRITEABLE;
	}

#ifdef CONFIG_ETRAX_AXISFLASHMAP_MTD0WHOLE
	if (main_mtd) {
		main_partition.size = main_mtd->size;
		err = mtd_device_register(main_mtd, &main_partition, 1);
		if (err)
			panic("axisflashmap: Could not initialize "
			      "partition for whole main mtd device!\n");
	}
#endif


	for (part = 0; part < pidx; part++) {
		if (part == ram_rootfs_partition) {
			
			struct mtd_info *mtd_ram;

			mtd_ram = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
			if (!mtd_ram)
				panic("axisflashmap: Couldn't allocate memory "
				      "for mtd_info!\n");
			printk(KERN_INFO "axisflashmap: Adding RAM partition "
			       "for rootfs image.\n");
			err = mtdram_init_device(mtd_ram,
						 (void *)partition[part].offset,
						 partition[part].size,
						 partition[part].name);
			if (err)
				panic("axisflashmap: Could not initialize "
				      "MTD RAM device!\n");
			mtd_ram->erasesize = (main_mtd ? main_mtd->erasesize :
				CONFIG_ETRAX_PTABLE_SECTOR);
		} else {
			err = mtd_device_register(main_mtd, &partition[part],
						  1);
			if (err)
				panic("axisflashmap: Could not add mtd "
					"partition %d\n", part);
		}
	}
#endif 

#ifdef CONFIG_ETRAX_VCS_SIM
	struct mtd_info *mtd_ram;

	mtd_ram = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!mtd_ram) {
		panic("axisflashmap: Couldn't allocate memory for "
		      "mtd_info!\n");
	}

	printk(KERN_INFO "axisflashmap: Adding RAM partition for romfs, "
	       "at %u, size %u\n",
	       (unsigned) romfs_start, (unsigned) romfs_length);

	err = mtdram_init_device(mtd_ram, (void *)romfs_start,
				 romfs_length, "romfs");
	if (err) {
		panic("axisflashmap: Could not initialize MTD RAM "
		      "device!\n");
	}
#endif 

#ifndef CONFIG_ETRAX_VCS_SIM
	if (aux_mtd) {
		aux_partition.size = aux_mtd->size;
		err = mtd_device_register(aux_mtd, &aux_partition, 1);
		if (err)
			panic("axisflashmap: Could not initialize "
			      "aux mtd device!\n");

	}
#endif 

	return err;
}

module_init(init_axis_flash);

EXPORT_SYMBOL(axisflash_mtd);
