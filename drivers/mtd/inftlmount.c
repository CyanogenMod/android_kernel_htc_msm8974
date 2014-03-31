/*
 * inftlmount.c -- INFTL mount code with extensive checks.
 *
 * Author: Greg Ungerer (gerg@snapgear.com)
 * Copyright © 2002-2003, Greg Ungerer (gerg@snapgear.com)
 *
 * Based heavily on the nftlmount.c code which is:
 * Author: Fabrice Bellard (fabrice.bellard@netgem.com)
 * Copyright © 2000 Netgem S.A.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nftl.h>
#include <linux/mtd/inftl.h>

static int find_boot_record(struct INFTLrecord *inftl)
{
	struct inftl_unittail h1;
	
	unsigned int i, block;
	u8 buf[SECTORSIZE];
	struct INFTLMediaHeader *mh = &inftl->MediaHdr;
	struct mtd_info *mtd = inftl->mbd.mtd;
	struct INFTLPartition *ip;
	size_t retlen;

	pr_debug("INFTL: find_boot_record(inftl=%p)\n", inftl);

	inftl->EraseSize = inftl->mbd.mtd->erasesize;
        inftl->nb_blocks = (u32)inftl->mbd.mtd->size / inftl->EraseSize;

	inftl->MediaUnit = BLOCK_NIL;

	
	for (block = 0; block < inftl->nb_blocks; block++) {
		int ret;

		ret = mtd_read(mtd, block * inftl->EraseSize, SECTORSIZE,
			       &retlen, buf);
		if (retlen != SECTORSIZE) {
			static int warncount = 5;

			if (warncount) {
				printk(KERN_WARNING "INFTL: block read at 0x%x "
					"of mtd%d failed: %d\n",
					block * inftl->EraseSize,
					inftl->mbd.mtd->index, ret);
				if (!--warncount)
					printk(KERN_WARNING "INFTL: further "
						"failures for this block will "
						"not be printed\n");
			}
			continue;
		}

		if (retlen < 6 || memcmp(buf, "BNAND", 6)) {
			
			continue;
		}

		
		ret = inftl_read_oob(mtd,
				     block * inftl->EraseSize + SECTORSIZE + 8,
				     8, &retlen,(char *)&h1);
		if (ret < 0) {
			printk(KERN_WARNING "INFTL: ANAND header found at "
				"0x%x in mtd%d, but OOB data read failed "
				"(err %d)\n", block * inftl->EraseSize,
				inftl->mbd.mtd->index, ret);
			continue;
		}


		memcpy(mh, buf, sizeof(struct INFTLMediaHeader));

		
		mtd_read(mtd, block * inftl->EraseSize + 4096, SECTORSIZE,
			 &retlen, buf);
		if (retlen != SECTORSIZE) {
			printk(KERN_WARNING "INFTL: Unable to read spare "
			       "Media Header\n");
			return -1;
		}
		
		if (memcmp(mh, buf, sizeof(struct INFTLMediaHeader))) {
			printk(KERN_WARNING "INFTL: Primary and spare Media "
			       "Headers disagree.\n");
			return -1;
		}

		mh->NoOfBootImageBlocks = le32_to_cpu(mh->NoOfBootImageBlocks);
		mh->NoOfBinaryPartitions = le32_to_cpu(mh->NoOfBinaryPartitions);
		mh->NoOfBDTLPartitions = le32_to_cpu(mh->NoOfBDTLPartitions);
		mh->BlockMultiplierBits = le32_to_cpu(mh->BlockMultiplierBits);
		mh->FormatFlags = le32_to_cpu(mh->FormatFlags);
		mh->PercentUsed = le32_to_cpu(mh->PercentUsed);

		pr_debug("INFTL: Media Header ->\n"
			 "    bootRecordID          = %s\n"
			 "    NoOfBootImageBlocks   = %d\n"
			 "    NoOfBinaryPartitions  = %d\n"
			 "    NoOfBDTLPartitions    = %d\n"
			 "    BlockMultiplerBits    = %d\n"
			 "    FormatFlgs            = %d\n"
			 "    OsakVersion           = 0x%x\n"
			 "    PercentUsed           = %d\n",
			 mh->bootRecordID, mh->NoOfBootImageBlocks,
			 mh->NoOfBinaryPartitions,
			 mh->NoOfBDTLPartitions,
			 mh->BlockMultiplierBits, mh->FormatFlags,
			 mh->OsakVersion, mh->PercentUsed);

		if (mh->NoOfBDTLPartitions == 0) {
			printk(KERN_WARNING "INFTL: Media Header sanity check "
				"failed: NoOfBDTLPartitions (%d) == 0, "
				"must be at least 1\n", mh->NoOfBDTLPartitions);
			return -1;
		}

		if ((mh->NoOfBDTLPartitions + mh->NoOfBinaryPartitions) > 4) {
			printk(KERN_WARNING "INFTL: Media Header sanity check "
				"failed: Total Partitions (%d) > 4, "
				"BDTL=%d Binary=%d\n", mh->NoOfBDTLPartitions +
				mh->NoOfBinaryPartitions,
				mh->NoOfBDTLPartitions,
				mh->NoOfBinaryPartitions);
			return -1;
		}

		if (mh->BlockMultiplierBits > 1) {
			printk(KERN_WARNING "INFTL: sorry, we don't support "
				"UnitSizeFactor 0x%02x\n",
				mh->BlockMultiplierBits);
			return -1;
		} else if (mh->BlockMultiplierBits == 1) {
			printk(KERN_WARNING "INFTL: support for INFTL with "
				"UnitSizeFactor 0x%02x is experimental\n",
				mh->BlockMultiplierBits);
			inftl->EraseSize = inftl->mbd.mtd->erasesize <<
				mh->BlockMultiplierBits;
			inftl->nb_blocks = (u32)inftl->mbd.mtd->size / inftl->EraseSize;
			block >>= mh->BlockMultiplierBits;
		}

		
		for (i = 0; (i < 4); i++) {
			ip = &mh->Partitions[i];
			ip->virtualUnits = le32_to_cpu(ip->virtualUnits);
			ip->firstUnit = le32_to_cpu(ip->firstUnit);
			ip->lastUnit = le32_to_cpu(ip->lastUnit);
			ip->flags = le32_to_cpu(ip->flags);
			ip->spareUnits = le32_to_cpu(ip->spareUnits);
			ip->Reserved0 = le32_to_cpu(ip->Reserved0);

			pr_debug("    PARTITION[%d] ->\n"
				 "        virtualUnits    = %d\n"
				 "        firstUnit       = %d\n"
				 "        lastUnit        = %d\n"
				 "        flags           = 0x%x\n"
				 "        spareUnits      = %d\n",
				 i, ip->virtualUnits, ip->firstUnit,
				 ip->lastUnit, ip->flags,
				 ip->spareUnits);

			if (ip->Reserved0 != ip->firstUnit) {
				struct erase_info *instr = &inftl->instr;

				instr->mtd = inftl->mbd.mtd;

				instr->addr = ip->Reserved0 * inftl->EraseSize;
				instr->len = inftl->EraseSize;
				mtd_erase(mtd, instr);
			}
			if ((ip->lastUnit - ip->firstUnit + 1) < ip->virtualUnits) {
				printk(KERN_WARNING "INFTL: Media Header "
					"Partition %d sanity check failed\n"
					"    firstUnit %d : lastUnit %d  >  "
					"virtualUnits %d\n", i, ip->lastUnit,
					ip->firstUnit, ip->Reserved0);
				return -1;
			}
			if (ip->Reserved1 != 0) {
				printk(KERN_WARNING "INFTL: Media Header "
					"Partition %d sanity check failed: "
					"Reserved1 %d != 0\n",
					i, ip->Reserved1);
				return -1;
			}

			if (ip->flags & INFTL_BDTL)
				break;
		}

		if (i >= 4) {
			printk(KERN_WARNING "INFTL: Media Header Partition "
				"sanity check failed:\n       No partition "
				"marked as Disk Partition\n");
			return -1;
		}

		inftl->nb_boot_blocks = ip->firstUnit;
		inftl->numvunits = ip->virtualUnits;
		if (inftl->numvunits > (inftl->nb_blocks -
		    inftl->nb_boot_blocks - 2)) {
			printk(KERN_WARNING "INFTL: Media Header sanity check "
				"failed:\n        numvunits (%d) > nb_blocks "
				"(%d) - nb_boot_blocks(%d) - 2\n",
				inftl->numvunits, inftl->nb_blocks,
				inftl->nb_boot_blocks);
			return -1;
		}

		inftl->mbd.size  = inftl->numvunits *
			(inftl->EraseSize / SECTORSIZE);

		inftl->firstEUN = ip->firstUnit;
		inftl->lastEUN = ip->lastUnit;
		inftl->nb_blocks = ip->lastUnit + 1;

		
		inftl->PUtable = kmalloc(inftl->nb_blocks * sizeof(u16), GFP_KERNEL);
		if (!inftl->PUtable) {
			printk(KERN_WARNING "INFTL: allocation of PUtable "
				"failed (%zd bytes)\n",
				inftl->nb_blocks * sizeof(u16));
			return -ENOMEM;
		}

		inftl->VUtable = kmalloc(inftl->nb_blocks * sizeof(u16), GFP_KERNEL);
		if (!inftl->VUtable) {
			kfree(inftl->PUtable);
			printk(KERN_WARNING "INFTL: allocation of VUtable "
				"failed (%zd bytes)\n",
				inftl->nb_blocks * sizeof(u16));
			return -ENOMEM;
		}

		
		for (i = 0; i < inftl->nb_boot_blocks; i++)
			inftl->PUtable[i] = BLOCK_RESERVED;
		
		for (; i < inftl->nb_blocks; i++)
			inftl->PUtable[i] = BLOCK_NOTEXPLORED;

		
		inftl->PUtable[block] = BLOCK_RESERVED;

		
		for (i = 0; i < inftl->nb_blocks; i++) {
			int physblock;
			for (physblock = 0; physblock < inftl->EraseSize; physblock += inftl->mbd.mtd->erasesize) {
				if (mtd_block_isbad(inftl->mbd.mtd,
						    i * inftl->EraseSize + physblock))
					inftl->PUtable[i] = BLOCK_RESERVED;
			}
		}

		inftl->MediaUnit = block;
		return 0;
	}

	
	return -1;
}

static int memcmpb(void *a, int c, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (c != ((unsigned char *)a)[i])
			return 1;
	}
	return 0;
}

static int check_free_sectors(struct INFTLrecord *inftl, unsigned int address,
	int len, int check_oob)
{
	u8 buf[SECTORSIZE + inftl->mbd.mtd->oobsize];
	struct mtd_info *mtd = inftl->mbd.mtd;
	size_t retlen;
	int i;

	for (i = 0; i < len; i += SECTORSIZE) {
		if (mtd_read(mtd, address, SECTORSIZE, &retlen, buf))
			return -1;
		if (memcmpb(buf, 0xff, SECTORSIZE) != 0)
			return -1;

		if (check_oob) {
			if(inftl_read_oob(mtd, address, mtd->oobsize,
					  &retlen, &buf[SECTORSIZE]) < 0)
				return -1;
			if (memcmpb(buf + SECTORSIZE, 0xff, mtd->oobsize) != 0)
				return -1;
		}
		address += SECTORSIZE;
	}

	return 0;
}

int INFTL_formatblock(struct INFTLrecord *inftl, int block)
{
	size_t retlen;
	struct inftl_unittail uci;
	struct erase_info *instr = &inftl->instr;
	struct mtd_info *mtd = inftl->mbd.mtd;
	int physblock;

	pr_debug("INFTL: INFTL_formatblock(inftl=%p,block=%d)\n", inftl, block);

	memset(instr, 0, sizeof(struct erase_info));


	
	instr->mtd = inftl->mbd.mtd;
	instr->addr = block * inftl->EraseSize;
	instr->len = inftl->mbd.mtd->erasesize;
	for (physblock = 0; physblock < inftl->EraseSize;
	     physblock += instr->len, instr->addr += instr->len) {
		mtd_erase(inftl->mbd.mtd, instr);

		if (instr->state == MTD_ERASE_FAILED) {
			printk(KERN_WARNING "INFTL: error while formatting block %d\n",
				block);
			goto fail;
		}

		if (check_free_sectors(inftl, instr->addr, instr->len, 1) != 0)
			goto fail;
	}

	uci.EraseMark = cpu_to_le16(ERASE_MARK);
	uci.EraseMark1 = cpu_to_le16(ERASE_MARK);
	uci.Reserved[0] = 0;
	uci.Reserved[1] = 0;
	uci.Reserved[2] = 0;
	uci.Reserved[3] = 0;
	instr->addr = block * inftl->EraseSize + SECTORSIZE * 2;
	if (inftl_write_oob(mtd, instr->addr + 8, 8, &retlen, (char *)&uci) < 0)
		goto fail;
	return 0;
fail:
	mtd_block_markbad(inftl->mbd.mtd, instr->addr);
	return -1;
}

static void format_chain(struct INFTLrecord *inftl, unsigned int first_block)
{
	unsigned int block = first_block, block1;

	printk(KERN_WARNING "INFTL: formatting chain at block %d\n",
		first_block);

	for (;;) {
		block1 = inftl->PUtable[block];

		printk(KERN_WARNING "INFTL: formatting block %d\n", block);
		if (INFTL_formatblock(inftl, block) < 0) {
			inftl->PUtable[block] = BLOCK_RESERVED;
		} else {
			inftl->PUtable[block] = BLOCK_FREE;
		}

		
		block = block1;

		if (block == BLOCK_NIL || block >= inftl->lastEUN)
			break;
	}
}

void INFTL_dumptables(struct INFTLrecord *s)
{
	int i;

	pr_debug("-------------------------------------------"
		"----------------------------------\n");

	pr_debug("VUtable[%d] ->", s->nb_blocks);
	for (i = 0; i < s->nb_blocks; i++) {
		if ((i % 8) == 0)
			pr_debug("\n%04x: ", i);
		pr_debug("%04x ", s->VUtable[i]);
	}

	pr_debug("\n-------------------------------------------"
		"----------------------------------\n");

	pr_debug("PUtable[%d-%d=%d] ->", s->firstEUN, s->lastEUN, s->nb_blocks);
	for (i = 0; i <= s->lastEUN; i++) {
		if ((i % 8) == 0)
			pr_debug("\n%04x: ", i);
		pr_debug("%04x ", s->PUtable[i]);
	}

	pr_debug("\n-------------------------------------------"
		"----------------------------------\n");

	pr_debug("INFTL ->\n"
		"  EraseSize       = %d\n"
		"  h/s/c           = %d/%d/%d\n"
		"  numvunits       = %d\n"
		"  firstEUN        = %d\n"
		"  lastEUN         = %d\n"
		"  numfreeEUNs     = %d\n"
		"  LastFreeEUN     = %d\n"
		"  nb_blocks       = %d\n"
		"  nb_boot_blocks  = %d",
		s->EraseSize, s->heads, s->sectors, s->cylinders,
		s->numvunits, s->firstEUN, s->lastEUN, s->numfreeEUNs,
		s->LastFreeEUN, s->nb_blocks, s->nb_boot_blocks);

	pr_debug("\n-------------------------------------------"
		"----------------------------------\n");
}

void INFTL_dumpVUchains(struct INFTLrecord *s)
{
	int logical, block, i;

	pr_debug("-------------------------------------------"
		"----------------------------------\n");

	pr_debug("INFTL Virtual Unit Chains:\n");
	for (logical = 0; logical < s->nb_blocks; logical++) {
		block = s->VUtable[logical];
		if (block > s->nb_blocks)
			continue;
		pr_debug("  LOGICAL %d --> %d ", logical, block);
		for (i = 0; i < s->nb_blocks; i++) {
			if (s->PUtable[block] == BLOCK_NIL)
				break;
			block = s->PUtable[block];
			pr_debug("%d ", block);
		}
		pr_debug("\n");
	}

	pr_debug("-------------------------------------------"
		"----------------------------------\n");
}

int INFTL_mount(struct INFTLrecord *s)
{
	struct mtd_info *mtd = s->mbd.mtd;
	unsigned int block, first_block, prev_block, last_block;
	unsigned int first_logical_block, logical_block, erase_mark;
	int chain_length, do_format_chain;
	struct inftl_unithead1 h0;
	struct inftl_unittail h1;
	size_t retlen;
	int i;
	u8 *ANACtable, ANAC;

	pr_debug("INFTL: INFTL_mount(inftl=%p)\n", s);

	
	if (find_boot_record(s) < 0) {
		printk(KERN_WARNING "INFTL: could not find valid boot record?\n");
		return -ENXIO;
	}

	
	for (i = 0; i < s->nb_blocks; i++)
		s->VUtable[i] = BLOCK_NIL;

	logical_block = block = BLOCK_NIL;

	
	ANACtable = kcalloc(s->nb_blocks, sizeof(u8), GFP_KERNEL);
	if (!ANACtable) {
		printk(KERN_WARNING "INFTL: allocation of ANACtable "
				"failed (%zd bytes)\n",
				s->nb_blocks * sizeof(u8));
		return -ENOMEM;
	}

	pr_debug("INFTL: pass 1, explore each unit\n");
	for (first_block = s->firstEUN; first_block <= s->lastEUN; first_block++) {
		if (s->PUtable[first_block] != BLOCK_NOTEXPLORED)
			continue;

		do_format_chain = 0;
		first_logical_block = BLOCK_NIL;
		last_block = BLOCK_NIL;
		block = first_block;

		for (chain_length = 0; ; chain_length++) {

			if ((chain_length == 0) &&
			    (s->PUtable[block] != BLOCK_NOTEXPLORED)) {
				
				break;
			}

			if (inftl_read_oob(mtd, block * s->EraseSize + 8,
					   8, &retlen, (char *)&h0) < 0 ||
			    inftl_read_oob(mtd, block * s->EraseSize +
					   2 * SECTORSIZE + 8, 8, &retlen,
					   (char *)&h1) < 0) {
				
				do_format_chain++;
				break;
			}

			logical_block = le16_to_cpu(h0.virtualUnitNo);
			prev_block = le16_to_cpu(h0.prevUnitNo);
			erase_mark = le16_to_cpu((h1.EraseMark | h1.EraseMark1));
			ANACtable[block] = h0.ANAC;

			
			if (prev_block < s->nb_blocks)
				prev_block += s->firstEUN;

			
			if (s->PUtable[block] != BLOCK_NOTEXPLORED) {
				
				if (logical_block == first_logical_block) {
					if (last_block != BLOCK_NIL)
						s->PUtable[last_block] = block;
				}
				break;
			}

			
			if (erase_mark != ERASE_MARK) {
				printk(KERN_WARNING "INFTL: corrupt block %d "
					"in chain %d, chain length %d, erase "
					"mark 0x%x?\n", block, first_block,
					chain_length, erase_mark);
				if (chain_length == 0)
					do_format_chain++;
				break;
			}

			
			if ((logical_block == BLOCK_FREE) ||
			    (logical_block == BLOCK_NIL)) {
				s->PUtable[block] = BLOCK_FREE;
				break;
			}

			
			if ((logical_block >= s->nb_blocks) ||
			    ((prev_block >= s->nb_blocks) &&
			     (prev_block != BLOCK_NIL))) {
				if (chain_length > 0) {
					printk(KERN_WARNING "INFTL: corrupt "
						"block %d in chain %d?\n",
						block, first_block);
					do_format_chain++;
				}
				break;
			}

			if (first_logical_block == BLOCK_NIL) {
				first_logical_block = logical_block;
			} else {
				if (first_logical_block != logical_block) {
					
					break;
				}
			}

			s->PUtable[block] = BLOCK_NIL;
			if (last_block != BLOCK_NIL)
				s->PUtable[last_block] = block;
			last_block = block;
			block = prev_block;

			
			if (block == BLOCK_NIL)
				break;

			
			if (block > s->lastEUN) {
				printk(KERN_WARNING "INFTL: invalid previous "
					"block %d in chain %d?\n", block,
					first_block);
				do_format_chain++;
				break;
			}
		}

		if (do_format_chain) {
			format_chain(s, first_block);
			continue;
		}

		s->VUtable[first_logical_block] = first_block;
		logical_block = BLOCK_NIL;
	}

	INFTL_dumptables(s);

	pr_debug("INFTL: pass 2, validate virtual chains\n");
	for (logical_block = 0; logical_block < s->numvunits; logical_block++) {
		block = s->VUtable[logical_block];
		last_block = BLOCK_NIL;

		
		if (block >= BLOCK_RESERVED)
			continue;

		ANAC = ANACtable[block];
		for (i = 0; i < s->numvunits; i++) {
			if (s->PUtable[block] == BLOCK_NIL)
				break;
			if (s->PUtable[block] > s->lastEUN) {
				printk(KERN_WARNING "INFTL: invalid prev %d, "
					"in virtual chain %d\n",
					s->PUtable[block], logical_block);
				s->PUtable[block] = BLOCK_NIL;

			}
			if (ANACtable[block] != ANAC) {
				s->VUtable[logical_block] = block;
				s->PUtable[last_block] = BLOCK_NIL;
				break;
			}

			ANAC--;
			last_block = block;
			block = s->PUtable[block];
		}

		if (i >= s->nb_blocks) {
			format_chain(s, first_block);
		}
	}

	INFTL_dumptables(s);
	INFTL_dumpVUchains(s);

	s->numfreeEUNs = 0;
	s->LastFreeEUN = BLOCK_NIL;

	pr_debug("INFTL: pass 3, format unused blocks\n");
	for (block = s->firstEUN; block <= s->lastEUN; block++) {
		if (s->PUtable[block] == BLOCK_NOTEXPLORED) {
			printk("INFTL: unreferenced block %d, formatting it\n",
				block);
			if (INFTL_formatblock(s, block) < 0)
				s->PUtable[block] = BLOCK_RESERVED;
			else
				s->PUtable[block] = BLOCK_FREE;
		}
		if (s->PUtable[block] == BLOCK_FREE) {
			s->numfreeEUNs++;
			if (s->LastFreeEUN == BLOCK_NIL)
				s->LastFreeEUN = block;
		}
	}

	kfree(ANACtable);
	return 0;
}
