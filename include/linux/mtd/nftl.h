/*
 * Copyright © 1999-2010 David Woodhouse <dwmw2@infradead.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __MTD_NFTL_H__
#define __MTD_NFTL_H__

#include <linux/mtd/mtd.h>
#include <linux/mtd/blktrans.h>

#include <mtd/nftl-user.h>

#define BLOCK_NIL          0xffff 
#define BLOCK_FREE         0xfffe 
#define BLOCK_NOTEXPLORED  0xfffd 
#define BLOCK_RESERVED     0xfffc 

struct NFTLrecord {
	struct mtd_blktrans_dev mbd;
	__u16 MediaUnit, SpareMediaUnit;
	__u32 EraseSize;
	struct NFTLMediaHeader MediaHdr;
	int usecount;
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	__u16 numvunits;
	__u16 lastEUN;                  
	__u16 numfreeEUNs;
	__u16 LastFreeEUN; 		
	int head,sect,cyl;
	__u16 *EUNtable; 		
	__u16 *ReplUnitTable; 		
        unsigned int nb_blocks;		
        unsigned int nb_boot_blocks;	
        struct erase_info instr;
	struct nand_ecclayout oobinfo;
};

int NFTL_mount(struct NFTLrecord *s);
int NFTL_formatblock(struct NFTLrecord *s, int block);

int nftl_read_oob(struct mtd_info *mtd, loff_t offs, size_t len,
		  size_t *retlen, uint8_t *buf);
int nftl_write_oob(struct mtd_info *mtd, loff_t offs, size_t len,
		   size_t *retlen, uint8_t *buf);

#ifndef NFTL_MAJOR
#define NFTL_MAJOR 93
#endif

#define MAX_NFTLS 16
#define MAX_SECTORS_PER_UNIT 64
#define NFTL_PARTN_BITS 4

#endif 
