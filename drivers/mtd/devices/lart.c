
/*
 * MTD driver for the 28F160F3 Flash Memory (non-CFI) on LART.
 *
 * Author: Abraham vd Merwe <abraham@2d3d.co.za>
 *
 * Copyright (c) 2001, 2d3D, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * References:
 *
 *    [1] 3 Volt Fast Boot Block Flash Memory" Intel Datasheet
 *           - Order Number: 290644-005
 *           - January 2000
 *
 *    [2] MTD internal API documentation
 *           - http://www.linux-mtd.infradead.org/ 
 *
 * Limitations:
 *
 *    Even though this driver is written for 3 Volt Fast Boot
 *    Block Flash Memory, it is rather specific to LART. With
 *    Minor modifications, notably the without data/address line
 *    mangling and different bus settings, etc. it should be
 *    trivial to adapt to other platforms.
 *
 *    If somebody would sponsor me a different board, I'll
 *    adapt the driver (:
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#ifndef CONFIG_SA1100_LART
#error This is for LART architecture only
#endif

static char module_name[] = "lart";

#define FLASH_BLOCKSIZE_PARAM		(4096 * BUSWIDTH)
#define FLASH_NUMBLOCKS_16m_PARAM	8
#define FLASH_NUMBLOCKS_8m_PARAM	8

#define FLASH_BLOCKSIZE_MAIN		(32768 * BUSWIDTH)
#define FLASH_NUMBLOCKS_16m_MAIN	31
#define FLASH_NUMBLOCKS_8m_MAIN		15


#define BUSWIDTH			4				
#define FLASH_OFFSET		0xe8000000		

#define NUM_BLOB_BLOCKS		FLASH_NUMBLOCKS_16m_PARAM
#define BLOB_START			0x00000000
#define BLOB_LEN			(NUM_BLOB_BLOCKS * FLASH_BLOCKSIZE_PARAM)

#define NUM_KERNEL_BLOCKS	7
#define KERNEL_START		(BLOB_START + BLOB_LEN)
#define KERNEL_LEN			(NUM_KERNEL_BLOCKS * FLASH_BLOCKSIZE_MAIN)

#define NUM_INITRD_BLOCKS	24
#define INITRD_START		(KERNEL_START + KERNEL_LEN)
#define INITRD_LEN			(NUM_INITRD_BLOCKS * FLASH_BLOCKSIZE_MAIN)

#define READ_ARRAY			0x00FF00FF		
#define READ_ID_CODES		0x00900090		
#define ERASE_SETUP			0x00200020		
#define ERASE_CONFIRM		0x00D000D0		
#define PGM_SETUP			0x00400040		
#define STATUS_READ			0x00700070		
#define STATUS_CLEAR		0x00500050		
#define STATUS_BUSY			0x00800080		
#define STATUS_ERASE_ERR	0x00200020		
#define STATUS_PGM_ERR		0x00100010		

#define FLASH_MANUFACTURER			0x00890089
#define FLASH_DEVICE_8mbit_TOP		0x88f188f1
#define FLASH_DEVICE_8mbit_BOTTOM	0x88f288f2
#define FLASH_DEVICE_16mbit_TOP		0x88f388f3
#define FLASH_DEVICE_16mbit_BOTTOM	0x88f488f4



#define DATA_TO_FLASH(x)				\
	(									\
		(((x) & 0x08009000) >> 11)	+	\
		(((x) & 0x00002000) >> 10)	+	\
		(((x) & 0x04004000) >> 8)	+	\
		(((x) & 0x00000010) >> 4)	+	\
		(((x) & 0x91000820) >> 3)	+	\
		(((x) & 0x22080080) >> 2)	+	\
		((x) & 0x40000400)			+	\
		(((x) & 0x00040040) << 1)	+	\
		(((x) & 0x00110000) << 4)	+	\
		(((x) & 0x00220100) << 5)	+	\
		(((x) & 0x00800208) << 6)	+	\
		(((x) & 0x00400004) << 9)	+	\
		(((x) & 0x00000001) << 12)	+	\
		(((x) & 0x00000002) << 13)		\
	)

#define FLASH_TO_DATA(x)				\
	(									\
		(((x) & 0x00010012) << 11)	+	\
		(((x) & 0x00000008) << 10)	+	\
		(((x) & 0x00040040) << 8)	+	\
		(((x) & 0x00000001) << 4)	+	\
		(((x) & 0x12200104) << 3)	+	\
		(((x) & 0x08820020) << 2)	+	\
		((x) & 0x40000400)			+	\
		(((x) & 0x00080080) >> 1)	+	\
		(((x) & 0x01100000) >> 4)	+	\
		(((x) & 0x04402000) >> 5)	+	\
		(((x) & 0x20008200) >> 6)	+	\
		(((x) & 0x80000800) >> 9)	+	\
		(((x) & 0x00001000) >> 12)	+	\
		(((x) & 0x00004000) >> 13)		\
	)


#define ADDR_TO_FLASH_U2(x)				\
	(									\
		(((x) & 0x00000f00) >> 4)	+	\
		(((x) & 0x00042000) << 1)	+	\
		(((x) & 0x0009c003) << 2)	+	\
		(((x) & 0x00021080) << 3)	+	\
		(((x) & 0x00000010) << 4)	+	\
		(((x) & 0x00000040) << 5)	+	\
		(((x) & 0x00000024) << 7)	+	\
		(((x) & 0x00000008) << 10)		\
	)

#define FLASH_U2_TO_ADDR(x)				\
	(									\
		(((x) << 4) & 0x00000f00)	+	\
		(((x) >> 1) & 0x00042000)	+	\
		(((x) >> 2) & 0x0009c003)	+	\
		(((x) >> 3) & 0x00021080)	+	\
		(((x) >> 4) & 0x00000010)	+	\
		(((x) >> 5) & 0x00000040)	+	\
		(((x) >> 7) & 0x00000024)	+	\
		(((x) >> 10) & 0x00000008)		\
	)

#define ADDR_TO_FLASH_U3(x)				\
	(									\
		(((x) & 0x00000080) >> 3)	+	\
		(((x) & 0x00000040) >> 1)	+	\
		(((x) & 0x00052020) << 1)	+	\
		(((x) & 0x00084f03) << 2)	+	\
		(((x) & 0x00029010) << 3)	+	\
		(((x) & 0x00000008) << 5)	+	\
		(((x) & 0x00000004) << 7)		\
	)

#define FLASH_U3_TO_ADDR(x)				\
	(									\
		(((x) << 3) & 0x00000080)	+	\
		(((x) << 1) & 0x00000040)	+	\
		(((x) >> 1) & 0x00052020)	+	\
		(((x) >> 2) & 0x00084f03)	+	\
		(((x) >> 3) & 0x00029010)	+	\
		(((x) >> 5) & 0x00000008)	+	\
		(((x) >> 7) & 0x00000004)		\
	)


static __u8 read8 (__u32 offset)
{
   volatile __u8 *data = (__u8 *) (FLASH_OFFSET + offset);
#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x -> 0x%.2x\n", __func__, offset, *data);
#endif
   return (*data);
}

static __u32 read32 (__u32 offset)
{
   volatile __u32 *data = (__u32 *) (FLASH_OFFSET + offset);
#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x -> 0x%.8x\n", __func__, offset, *data);
#endif
   return (*data);
}

static void write32 (__u32 x,__u32 offset)
{
   volatile __u32 *data = (__u32 *) (FLASH_OFFSET + offset);
   *data = x;
#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x <- 0x%.8x\n", __func__, offset, *data);
#endif
}


static int flash_probe (void)
{
   __u32 manufacturer,devtype;

   
   write32 (DATA_TO_FLASH (READ_ID_CODES),0x00000000);

   manufacturer = FLASH_TO_DATA (read32 (ADDR_TO_FLASH_U2 (0x00000000)));
   devtype = FLASH_TO_DATA (read32 (ADDR_TO_FLASH_U2 (0x00000001)));

   
   write32 (DATA_TO_FLASH (READ_ARRAY),0x00000000);

   return (manufacturer == FLASH_MANUFACTURER && (devtype == FLASH_DEVICE_16mbit_TOP || devtype == FLASH_DEVICE_16mbit_BOTTOM));
}

static inline int erase_block (__u32 offset)
{
   __u32 status;

#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x\n", __func__, offset);
#endif

   
   write32 (DATA_TO_FLASH (ERASE_SETUP),offset);
   write32 (DATA_TO_FLASH (ERASE_CONFIRM),offset);

   
   do
	 {
		write32 (DATA_TO_FLASH (STATUS_READ),offset);
		status = FLASH_TO_DATA (read32 (offset));
	 }
   while ((~status & STATUS_BUSY) != 0);

   
   write32 (DATA_TO_FLASH (READ_ARRAY),offset);

   
   if ((status & STATUS_ERASE_ERR))
	 {
		printk (KERN_WARNING "%s: erase error at address 0x%.8x.\n",module_name,offset);
		return (0);
	 }

   return (1);
}

static int flash_erase (struct mtd_info *mtd,struct erase_info *instr)
{
   __u32 addr,len;
   int i,first;

#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(addr = 0x%.8x, len = %d)\n", __func__, instr->addr, instr->len);
#endif

   for (i = 0; i < mtd->numeraseregions && instr->addr >= mtd->eraseregions[i].offset; i++) ;
   i--;

   if (i < 0 || (instr->addr & (mtd->eraseregions[i].erasesize - 1)))
      return -EINVAL;

   
   first = i;

   for (; i < mtd->numeraseregions && instr->addr + instr->len >= mtd->eraseregions[i].offset; i++) ;
   i--;

   
   if (i < 0 || ((instr->addr + instr->len) & (mtd->eraseregions[i].erasesize - 1)))
      return -EINVAL;

   addr = instr->addr;
   len = instr->len;

   i = first;

   
   while (len)
	 {
		if (!erase_block (addr))
		  {
			 instr->state = MTD_ERASE_FAILED;
			 return (-EIO);
		  }

		addr += mtd->eraseregions[i].erasesize;
		len -= mtd->eraseregions[i].erasesize;

		if (addr == mtd->eraseregions[i].offset + (mtd->eraseregions[i].erasesize * mtd->eraseregions[i].numblocks)) i++;
	 }

   instr->state = MTD_ERASE_DONE;
   mtd_erase_callback(instr);

   return (0);
}

static int flash_read (struct mtd_info *mtd,loff_t from,size_t len,size_t *retlen,u_char *buf)
{
#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(from = 0x%.8x, len = %d)\n", __func__, (__u32)from, len);
#endif

   
   *retlen = len;

   
   if (from & (BUSWIDTH - 1))
	 {
		int gap = BUSWIDTH - (from & (BUSWIDTH - 1));

		while (len && gap--) *buf++ = read8 (from++), len--;
	 }

   
   while (len >= BUSWIDTH)
	 {
		*((__u32 *) buf) = read32 (from);

		buf += BUSWIDTH;
		from += BUSWIDTH;
		len -= BUSWIDTH;
	 }

   
   if (len & (BUSWIDTH - 1))
	 while (len--) *buf++ = read8 (from++);

   return (0);
}

static inline int write_dword (__u32 offset,__u32 x)
{
   __u32 status;

#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x <- 0x%.8x\n", __func__, offset, x);
#endif

   
   write32 (DATA_TO_FLASH (PGM_SETUP),offset);

   
   write32 (x,offset);

   
   do
	 {
		write32 (DATA_TO_FLASH (STATUS_READ),offset);
		status = FLASH_TO_DATA (read32 (offset));
	 }
   while ((~status & STATUS_BUSY) != 0);

   
   write32 (DATA_TO_FLASH (READ_ARRAY),offset);

   
   if ((status & STATUS_PGM_ERR) || read32 (offset) != x)
	 {
		printk (KERN_WARNING "%s: write error at address 0x%.8x.\n",module_name,offset);
		return (0);
	 }

   return (1);
}

static int flash_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,const u_char *buf)
{
   __u8 tmp[4];
   int i,n;

#ifdef LART_DEBUG
   printk (KERN_DEBUG "%s(to = 0x%.8x, len = %d)\n", __func__, (__u32)to, len);
#endif

   
   if (!len) return (0);

   
   if (to & (BUSWIDTH - 1))
	 {
		__u32 aligned = to & ~(BUSWIDTH - 1);
		int gap = to - aligned;

		i = n = 0;

		while (gap--) tmp[i++] = 0xFF;
		while (len && i < BUSWIDTH) tmp[i++] = buf[n++], len--;
		while (i < BUSWIDTH) tmp[i++] = 0xFF;

		if (!write_dword (aligned,*((__u32 *) tmp))) return (-EIO);

		to += n;
		buf += n;
		*retlen += n;
	 }

   
   while (len >= BUSWIDTH)
	 {
		if (!write_dword (to,*((__u32 *) buf))) return (-EIO);

		to += BUSWIDTH;
		buf += BUSWIDTH;
		*retlen += BUSWIDTH;
		len -= BUSWIDTH;
	 }

   
   if (len & (BUSWIDTH - 1))
	 {
		i = n = 0;

		while (len--) tmp[i++] = buf[n++];
		while (i < BUSWIDTH) tmp[i++] = 0xFF;

		if (!write_dword (to,*((__u32 *) tmp))) return (-EIO);

		*retlen += n;
	 }

   return (0);
}


static struct mtd_info mtd;

static struct mtd_erase_region_info erase_regions[] = {
	
	{
		.offset		= 0x00000000,
		.erasesize	= FLASH_BLOCKSIZE_PARAM,
		.numblocks	= FLASH_NUMBLOCKS_16m_PARAM,
	},
	
	{
		.offset	 = FLASH_BLOCKSIZE_PARAM * FLASH_NUMBLOCKS_16m_PARAM,
		.erasesize	= FLASH_BLOCKSIZE_MAIN,
		.numblocks	= FLASH_NUMBLOCKS_16m_MAIN,
	}
};

static struct mtd_partition lart_partitions[] = {
	
	{
		.name	= "blob",
		.offset	= BLOB_START,
		.size	= BLOB_LEN,
	},
	
	{
		.name	= "kernel",
		.offset	= KERNEL_START,		
		.size	= KERNEL_LEN,
	},
	
	{
		.name	= "file system",
		.offset	= INITRD_START,		
		.size	= INITRD_LEN,		
	}
};
#define NUM_PARTITIONS ARRAY_SIZE(lart_partitions)

static int __init lart_flash_init (void)
{
   int result;
   memset (&mtd,0,sizeof (mtd));
   printk ("MTD driver for LART. Written by Abraham vd Merwe <abraham@2d3d.co.za>\n");
   printk ("%s: Probing for 28F160x3 flash on LART...\n",module_name);
   if (!flash_probe ())
	 {
		printk (KERN_WARNING "%s: Found no LART compatible flash device\n",module_name);
		return (-ENXIO);
	 }
   printk ("%s: This looks like a LART board to me.\n",module_name);
   mtd.name = module_name;
   mtd.type = MTD_NORFLASH;
   mtd.writesize = 1;
   mtd.writebufsize = 4;
   mtd.flags = MTD_CAP_NORFLASH;
   mtd.size = FLASH_BLOCKSIZE_PARAM * FLASH_NUMBLOCKS_16m_PARAM + FLASH_BLOCKSIZE_MAIN * FLASH_NUMBLOCKS_16m_MAIN;
   mtd.erasesize = FLASH_BLOCKSIZE_MAIN;
   mtd.numeraseregions = ARRAY_SIZE(erase_regions);
   mtd.eraseregions = erase_regions;
   mtd._erase = flash_erase;
   mtd._read = flash_read;
   mtd._write = flash_write;
   mtd.owner = THIS_MODULE;

#ifdef LART_DEBUG
   printk (KERN_DEBUG
		   "mtd.name = %s\n"
		   "mtd.size = 0x%.8x (%uM)\n"
		   "mtd.erasesize = 0x%.8x (%uK)\n"
		   "mtd.numeraseregions = %d\n",
		   mtd.name,
		   mtd.size,mtd.size / (1024*1024),
		   mtd.erasesize,mtd.erasesize / 1024,
		   mtd.numeraseregions);

   if (mtd.numeraseregions)
	 for (result = 0; result < mtd.numeraseregions; result++)
	   printk (KERN_DEBUG
			   "\n\n"
			   "mtd.eraseregions[%d].offset = 0x%.8x\n"
			   "mtd.eraseregions[%d].erasesize = 0x%.8x (%uK)\n"
			   "mtd.eraseregions[%d].numblocks = %d\n",
			   result,mtd.eraseregions[result].offset,
			   result,mtd.eraseregions[result].erasesize,mtd.eraseregions[result].erasesize / 1024,
			   result,mtd.eraseregions[result].numblocks);

   printk ("\npartitions = %d\n", ARRAY_SIZE(lart_partitions));

   for (result = 0; result < ARRAY_SIZE(lart_partitions); result++)
	 printk (KERN_DEBUG
			 "\n\n"
			 "lart_partitions[%d].name = %s\n"
			 "lart_partitions[%d].offset = 0x%.8x\n"
			 "lart_partitions[%d].size = 0x%.8x (%uK)\n",
			 result,lart_partitions[result].name,
			 result,lart_partitions[result].offset,
			 result,lart_partitions[result].size,lart_partitions[result].size / 1024);
#endif

   result = mtd_device_register(&mtd, lart_partitions,
                                ARRAY_SIZE(lart_partitions));

   return (result);
}

static void __exit lart_flash_exit (void)
{
   mtd_device_unregister(&mtd);
}

module_init (lart_flash_init);
module_exit (lart_flash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abraham vd Merwe <abraham@2d3d.co.za>");
MODULE_DESCRIPTION("MTD driver for Intel 28F160F3 on LART board");
