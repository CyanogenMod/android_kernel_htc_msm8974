/*
 * NFTL mount code with extensive checks
 *
 * Author: Fabrice Bellard (fabrice.bellard@netgem.com)
 * Copyright © 2000 Netgem S.A.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <asm/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nftl.h>

#define SECTORSIZE 512

static int find_boot_record(struct NFTLrecord *nftl)
{
	struct nftl_uci1 h1;
	unsigned int block, boot_record_count = 0;
	size_t retlen;
	u8 buf[SECTORSIZE];
	struct NFTLMediaHeader *mh = &nftl->MediaHdr;
	struct mtd_info *mtd = nftl->mbd.mtd;
	unsigned int i;

	nftl->EraseSize = nftl->mbd.mtd->erasesize;
        nftl->nb_blocks = (u32)nftl->mbd.mtd->size / nftl->EraseSize;

	nftl->MediaUnit = BLOCK_NIL;
	nftl->SpareMediaUnit = BLOCK_NIL;

	
	for (block = 0; block < nftl->nb_blocks; block++) {
		int ret;

		ret = mtd_read(mtd, block * nftl->EraseSize, SECTORSIZE,
			       &retlen, buf);
		if (retlen != SECTORSIZE) {
			static int warncount = 5;

			if (warncount) {
				printk(KERN_WARNING "Block read at 0x%x of mtd%d failed: %d\n",
				       block * nftl->EraseSize, nftl->mbd.mtd->index, ret);
				if (!--warncount)
					printk(KERN_WARNING "Further failures for this block will not be printed\n");
			}
			continue;
		}

		if (retlen < 6 || memcmp(buf, "ANAND", 6)) {
			
#if 0
			printk(KERN_DEBUG "ANAND header not found at 0x%x in mtd%d\n",
			       block * nftl->EraseSize, nftl->mbd.mtd->index);
#endif
			continue;
		}

		
		if ((ret = nftl_read_oob(mtd, block * nftl->EraseSize +
					 SECTORSIZE + 8, 8, &retlen,
					 (char *)&h1) < 0)) {
			printk(KERN_WARNING "ANAND header found at 0x%x in mtd%d, but OOB data read failed (err %d)\n",
			       block * nftl->EraseSize, nftl->mbd.mtd->index, ret);
			continue;
		}

#if 0 
		if (le16_to_cpu(h1.EraseMark | h1.EraseMark1) != ERASE_MARK) {
			printk(KERN_NOTICE "ANAND header found at 0x%x in mtd%d, but erase mark not present (0x%04x,0x%04x instead)\n",
			       block * nftl->EraseSize, nftl->mbd.mtd->index,
			       le16_to_cpu(h1.EraseMark), le16_to_cpu(h1.EraseMark1));
			continue;
		}

		
		if ((ret = mtd->read(mtd, block * nftl->EraseSize, SECTORSIZE,
				     &retlen, buf) < 0)) {
			printk(KERN_NOTICE "ANAND header found at 0x%x in mtd%d, but ECC read failed (err %d)\n",
			       block * nftl->EraseSize, nftl->mbd.mtd->index, ret);
			continue;
		}

		
		if (memcmp(buf, "ANAND", 6)) {
			printk(KERN_NOTICE "ANAND header found at 0x%x in mtd%d, but went away on reread!\n",
			       block * nftl->EraseSize, nftl->mbd.mtd->index);
			printk(KERN_NOTICE "New data are: %02x %02x %02x %02x %02x %02x\n",
			       buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			continue;
		}
#endif
		

		if (boot_record_count) {
			if (memcmp(mh, buf, sizeof(struct NFTLMediaHeader))) {
				printk(KERN_NOTICE "NFTL Media Headers at 0x%x and 0x%x disagree.\n",
				       nftl->MediaUnit * nftl->EraseSize, block * nftl->EraseSize);
				
				if (boot_record_count < 2) {
					
					return -1;
				}
				continue;
			}
			if (boot_record_count == 1)
				nftl->SpareMediaUnit = block;

			
			nftl->ReplUnitTable[block] = BLOCK_RESERVED;


			boot_record_count++;
			continue;
		}

		
		memcpy(mh, buf, sizeof(struct NFTLMediaHeader));

		
#if 0
The new DiskOnChip driver scans the MediaHeader itself, and presents a virtual
erasesize based on UnitSizeFactor.  So the erasesize we read from the mtd
device is already correct.
		if (mh->UnitSizeFactor == 0) {
			printk(KERN_NOTICE "NFTL: UnitSizeFactor 0x00 detected. This violates the spec but we think we know what it means...\n");
		} else if (mh->UnitSizeFactor < 0xfc) {
			printk(KERN_NOTICE "Sorry, we don't support UnitSizeFactor 0x%02x\n",
			       mh->UnitSizeFactor);
			return -1;
		} else if (mh->UnitSizeFactor != 0xff) {
			printk(KERN_NOTICE "WARNING: Support for NFTL with UnitSizeFactor 0x%02x is experimental\n",
			       mh->UnitSizeFactor);
			nftl->EraseSize = nftl->mbd.mtd->erasesize << (0xff - mh->UnitSizeFactor);
			nftl->nb_blocks = (u32)nftl->mbd.mtd->size / nftl->EraseSize;
		}
#endif
		nftl->nb_boot_blocks = le16_to_cpu(mh->FirstPhysicalEUN);
		if ((nftl->nb_boot_blocks + 2) >= nftl->nb_blocks) {
			printk(KERN_NOTICE "NFTL Media Header sanity check failed:\n");
			printk(KERN_NOTICE "nb_boot_blocks (%d) + 2 > nb_blocks (%d)\n",
			       nftl->nb_boot_blocks, nftl->nb_blocks);
			return -1;
		}

		nftl->numvunits = le32_to_cpu(mh->FormattedSize) / nftl->EraseSize;
		if (nftl->numvunits > (nftl->nb_blocks - nftl->nb_boot_blocks - 2)) {
			printk(KERN_NOTICE "NFTL Media Header sanity check failed:\n");
			printk(KERN_NOTICE "numvunits (%d) > nb_blocks (%d) - nb_boot_blocks(%d) - 2\n",
			       nftl->numvunits, nftl->nb_blocks, nftl->nb_boot_blocks);
			return -1;
		}

		nftl->mbd.size  = nftl->numvunits * (nftl->EraseSize / SECTORSIZE);

		nftl->nb_blocks = le16_to_cpu(mh->NumEraseUnits) + le16_to_cpu(mh->FirstPhysicalEUN);

		
		nftl->lastEUN = nftl->nb_blocks - 1;

		
		nftl->EUNtable = kmalloc(nftl->nb_blocks * sizeof(u16), GFP_KERNEL);
		if (!nftl->EUNtable) {
			printk(KERN_NOTICE "NFTL: allocation of EUNtable failed\n");
			return -ENOMEM;
		}

		nftl->ReplUnitTable = kmalloc(nftl->nb_blocks * sizeof(u16), GFP_KERNEL);
		if (!nftl->ReplUnitTable) {
			kfree(nftl->EUNtable);
			printk(KERN_NOTICE "NFTL: allocation of ReplUnitTable failed\n");
			return -ENOMEM;
		}

		
		for (i = 0; i < nftl->nb_boot_blocks; i++)
			nftl->ReplUnitTable[i] = BLOCK_RESERVED;
		
		for (; i < nftl->nb_blocks; i++) {
			nftl->ReplUnitTable[i] = BLOCK_NOTEXPLORED;
		}

		
		nftl->ReplUnitTable[block] = BLOCK_RESERVED;

		
		for (i = 0; i < nftl->nb_blocks; i++) {
#if 0
The new DiskOnChip driver already scanned the bad block table.  Just query it.
			if ((i & (SECTORSIZE - 1)) == 0) {
				
				if ((ret = mtd->read(nftl->mbd.mtd, block * nftl->EraseSize +
						     i + SECTORSIZE, SECTORSIZE, &retlen,
						     buf)) < 0) {
					printk(KERN_NOTICE "Read of bad sector table failed (err %d)\n",
					       ret);
					kfree(nftl->ReplUnitTable);
					kfree(nftl->EUNtable);
					return -1;
				}
			}
			
			if (buf[i & (SECTORSIZE - 1)] != 0xff)
				nftl->ReplUnitTable[i] = BLOCK_RESERVED;
#endif
			if (mtd_block_isbad(nftl->mbd.mtd,
					    i * nftl->EraseSize))
				nftl->ReplUnitTable[i] = BLOCK_RESERVED;
		}

		nftl->MediaUnit = block;
		boot_record_count++;

	} 

	return boot_record_count?0:-1;
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

static int check_free_sectors(struct NFTLrecord *nftl, unsigned int address, int len,
			      int check_oob)
{
	u8 buf[SECTORSIZE + nftl->mbd.mtd->oobsize];
	struct mtd_info *mtd = nftl->mbd.mtd;
	size_t retlen;
	int i;

	for (i = 0; i < len; i += SECTORSIZE) {
		if (mtd_read(mtd, address, SECTORSIZE, &retlen, buf))
			return -1;
		if (memcmpb(buf, 0xff, SECTORSIZE) != 0)
			return -1;

		if (check_oob) {
			if(nftl_read_oob(mtd, address, mtd->oobsize,
					 &retlen, &buf[SECTORSIZE]) < 0)
				return -1;
			if (memcmpb(buf + SECTORSIZE, 0xff, mtd->oobsize) != 0)
				return -1;
		}
		address += SECTORSIZE;
	}

	return 0;
}

int NFTL_formatblock(struct NFTLrecord *nftl, int block)
{
	size_t retlen;
	unsigned int nb_erases, erase_mark;
	struct nftl_uci1 uci;
	struct erase_info *instr = &nftl->instr;
	struct mtd_info *mtd = nftl->mbd.mtd;

	
	if (nftl_read_oob(mtd, block * nftl->EraseSize + SECTORSIZE + 8,
			  8, &retlen, (char *)&uci) < 0)
		goto default_uci1;

	erase_mark = le16_to_cpu ((uci.EraseMark | uci.EraseMark1));
	if (erase_mark != ERASE_MARK) {
	default_uci1:
		uci.EraseMark = cpu_to_le16(ERASE_MARK);
		uci.EraseMark1 = cpu_to_le16(ERASE_MARK);
		uci.WearInfo = cpu_to_le32(0);
	}

	memset(instr, 0, sizeof(struct erase_info));

	
	instr->mtd = nftl->mbd.mtd;
	instr->addr = block * nftl->EraseSize;
	instr->len = nftl->EraseSize;
	mtd_erase(mtd, instr);

	if (instr->state == MTD_ERASE_FAILED) {
		printk("Error while formatting block %d\n", block);
		goto fail;
	}

		
		nb_erases = le32_to_cpu(uci.WearInfo);
		nb_erases++;

		
		if (nb_erases == 0)
			nb_erases = 1;

		if (check_free_sectors(nftl, instr->addr, nftl->EraseSize, 1) != 0)
			goto fail;

		uci.WearInfo = le32_to_cpu(nb_erases);
		if (nftl_write_oob(mtd, block * nftl->EraseSize + SECTORSIZE +
				   8, 8, &retlen, (char *)&uci) < 0)
			goto fail;
		return 0;
fail:
	mtd_block_markbad(nftl->mbd.mtd, instr->addr);
	return -1;
}

static void check_sectors_in_chain(struct NFTLrecord *nftl, unsigned int first_block)
{
	struct mtd_info *mtd = nftl->mbd.mtd;
	unsigned int block, i, status;
	struct nftl_bci bci;
	int sectors_per_block;
	size_t retlen;

	sectors_per_block = nftl->EraseSize / SECTORSIZE;
	block = first_block;
	for (;;) {
		for (i = 0; i < sectors_per_block; i++) {
			if (nftl_read_oob(mtd,
					  block * nftl->EraseSize + i * SECTORSIZE,
					  8, &retlen, (char *)&bci) < 0)
				status = SECTOR_IGNORE;
			else
				status = bci.Status | bci.Status1;

			switch(status) {
			case SECTOR_FREE:
				if (memcmpb(&bci, 0xff, 8) != 0 ||
				    check_free_sectors(nftl, block * nftl->EraseSize + i * SECTORSIZE,
						       SECTORSIZE, 0) != 0) {
					printk("Incorrect free sector %d in block %d: "
					       "marking it as ignored\n",
					       i, block);

					
					bci.Status = SECTOR_IGNORE;
					bci.Status1 = SECTOR_IGNORE;
					nftl_write_oob(mtd, block *
						       nftl->EraseSize +
						       i * SECTORSIZE, 8,
						       &retlen, (char *)&bci);
				}
				break;
			default:
				break;
			}
		}

		
		block = nftl->ReplUnitTable[block];
		if (!(block == BLOCK_NIL || block < nftl->nb_blocks))
			printk("incorrect ReplUnitTable[] : %d\n", block);
		if (block == BLOCK_NIL || block >= nftl->nb_blocks)
			break;
	}
}

static int calc_chain_length(struct NFTLrecord *nftl, unsigned int first_block)
{
	unsigned int length = 0, block = first_block;

	for (;;) {
		length++;
		if (length >= nftl->nb_blocks) {
			printk("nftl: length too long %d !\n", length);
			break;
		}

		block = nftl->ReplUnitTable[block];
		if (!(block == BLOCK_NIL || block < nftl->nb_blocks))
			printk("incorrect ReplUnitTable[] : %d\n", block);
		if (block == BLOCK_NIL || block >= nftl->nb_blocks)
			break;
	}
	return length;
}

static void format_chain(struct NFTLrecord *nftl, unsigned int first_block)
{
	unsigned int block = first_block, block1;

	printk("Formatting chain at block %d\n", first_block);

	for (;;) {
		block1 = nftl->ReplUnitTable[block];

		printk("Formatting block %d\n", block);
		if (NFTL_formatblock(nftl, block) < 0) {
			
			nftl->ReplUnitTable[block] = BLOCK_RESERVED;
		} else {
			nftl->ReplUnitTable[block] = BLOCK_FREE;
		}

		
		block = block1;

		if (!(block == BLOCK_NIL || block < nftl->nb_blocks))
			printk("incorrect ReplUnitTable[] : %d\n", block);
		if (block == BLOCK_NIL || block >= nftl->nb_blocks)
			break;
	}
}

static int check_and_mark_free_block(struct NFTLrecord *nftl, int block)
{
	struct mtd_info *mtd = nftl->mbd.mtd;
	struct nftl_uci1 h1;
	unsigned int erase_mark;
	size_t retlen;

	
	if (nftl_read_oob(mtd, block * nftl->EraseSize + SECTORSIZE + 8, 8,
			  &retlen, (char *)&h1) < 0)
		return -1;

	erase_mark = le16_to_cpu ((h1.EraseMark | h1.EraseMark1));
	if (erase_mark != ERASE_MARK) {
		if (check_free_sectors (nftl, block * nftl->EraseSize, nftl->EraseSize, 1) != 0)
			return -1;

		
		h1.EraseMark = cpu_to_le16(ERASE_MARK);
		h1.EraseMark1 = cpu_to_le16(ERASE_MARK);
		h1.WearInfo = cpu_to_le32(0);
		if (nftl_write_oob(mtd,
				   block * nftl->EraseSize + SECTORSIZE + 8, 8,
				   &retlen, (char *)&h1) < 0)
			return -1;
	} else {
#if 0
		
		for (i = 0; i < nftl->EraseSize; i += SECTORSIZE) {
			
			if (check_free_sectors (nftl, block * nftl->EraseSize + i,
						SECTORSIZE, 0) != 0)
				return -1;

			if (nftl_read_oob(mtd, block * nftl->EraseSize + i,
					  16, &retlen, buf) < 0)
				return -1;
			if (i == SECTORSIZE) {
				
				if (memcmpb(buf, 0xff, 8))
					return -1;
			} else {
				if (memcmpb(buf, 0xff, 16))
					return -1;
			}
		}
#endif
	}

	return 0;
}

static int get_fold_mark(struct NFTLrecord *nftl, unsigned int block)
{
	struct mtd_info *mtd = nftl->mbd.mtd;
	struct nftl_uci2 uci;
	size_t retlen;

	if (nftl_read_oob(mtd, block * nftl->EraseSize + 2 * SECTORSIZE + 8,
			  8, &retlen, (char *)&uci) < 0)
		return 0;

	return le16_to_cpu((uci.FoldMark | uci.FoldMark1));
}

int NFTL_mount(struct NFTLrecord *s)
{
	int i;
	unsigned int first_logical_block, logical_block, rep_block, nb_erases, erase_mark;
	unsigned int block, first_block, is_first_block;
	int chain_length, do_format_chain;
	struct nftl_uci0 h0;
	struct nftl_uci1 h1;
	struct mtd_info *mtd = s->mbd.mtd;
	size_t retlen;

	
	if (find_boot_record(s) < 0) {
		printk("Could not find valid boot record\n");
		return -1;
	}

	
	for (i = 0; i < s->nb_blocks; i++) {
		s->EUNtable[i] = BLOCK_NIL;
	}

	
	first_logical_block = 0;
	for (first_block = 0; first_block < s->nb_blocks; first_block++) {
		
		if (s->ReplUnitTable[first_block] == BLOCK_NOTEXPLORED) {
			block = first_block;
			chain_length = 0;
			do_format_chain = 0;

			for (;;) {
				
				if (nftl_read_oob(mtd,
						  block * s->EraseSize + 8, 8,
						  &retlen, (char *)&h0) < 0 ||
				    nftl_read_oob(mtd,
						  block * s->EraseSize +
						  SECTORSIZE + 8, 8,
						  &retlen, (char *)&h1) < 0) {
					s->ReplUnitTable[block] = BLOCK_NIL;
					do_format_chain = 1;
					break;
				}

				logical_block = le16_to_cpu ((h0.VirtUnitNum | h0.SpareVirtUnitNum));
				rep_block = le16_to_cpu ((h0.ReplUnitNum | h0.SpareReplUnitNum));
				nb_erases = le32_to_cpu (h1.WearInfo);
				erase_mark = le16_to_cpu ((h1.EraseMark | h1.EraseMark1));

				is_first_block = !(logical_block >> 15);
				logical_block = logical_block & 0x7fff;

				
				if (erase_mark != ERASE_MARK || logical_block >= s->nb_blocks) {
					if (chain_length == 0) {
						
						if (check_and_mark_free_block(s, block) < 0) {
							
							printk("Formatting block %d\n", block);
							if (NFTL_formatblock(s, block) < 0) {
								
								s->ReplUnitTable[block] = BLOCK_RESERVED;
							} else {
								s->ReplUnitTable[block] = BLOCK_FREE;
							}
						} else {
							
							s->ReplUnitTable[block] = BLOCK_FREE;
						}
						
						goto examine_ReplUnitTable;
					} else {
						printk("Block %d: free but referenced in chain %d\n",
						       block, first_block);
						s->ReplUnitTable[block] = BLOCK_NIL;
						do_format_chain = 1;
						break;
					}
				}

				
				if (chain_length == 0) {
					if (!is_first_block)
						goto examine_ReplUnitTable;
					first_logical_block = logical_block;
				} else {
					if (logical_block != first_logical_block) {
						printk("Block %d: incorrect logical block: %d expected: %d\n",
						       block, logical_block, first_logical_block);
						do_format_chain = 1;
					}
					if (is_first_block) {
						if (get_fold_mark(s, block) != FOLD_MARK_IN_PROGRESS ||
						    rep_block != 0xffff) {
							printk("Block %d: incorrectly marked as first block in chain\n",
							       block);
							do_format_chain = 1;
						} else {
							printk("Block %d: folding in progress - ignoring first block flag\n",
							       block);
						}
					}
				}
				chain_length++;
				if (rep_block == 0xffff) {
					
					s->ReplUnitTable[block] = BLOCK_NIL;
					break;
				} else if (rep_block >= s->nb_blocks) {
					printk("Block %d: referencing invalid block %d\n",
					       block, rep_block);
					do_format_chain = 1;
					s->ReplUnitTable[block] = BLOCK_NIL;
					break;
				} else if (s->ReplUnitTable[rep_block] != BLOCK_NOTEXPLORED) {
					if (s->ReplUnitTable[rep_block] == BLOCK_NIL &&
					    s->EUNtable[first_logical_block] == rep_block &&
					    get_fold_mark(s, first_block) == FOLD_MARK_IN_PROGRESS) {
						
						printk("Block %d: folding in progress - ignoring first block flag\n",
						       rep_block);
						s->ReplUnitTable[block] = rep_block;
						s->EUNtable[first_logical_block] = BLOCK_NIL;
					} else {
						printk("Block %d: referencing block %d already in another chain\n",
						       block, rep_block);
						
						do_format_chain = 1;
						s->ReplUnitTable[block] = BLOCK_NIL;
					}
					break;
				} else {
					
					s->ReplUnitTable[block] = rep_block;
					block = rep_block;
				}
			}

			if (do_format_chain) {
				
				format_chain(s, first_block);
			} else {
				unsigned int first_block1, chain_to_format, chain_length1;
				int fold_mark;

				
				fold_mark = get_fold_mark(s, first_block);
				if (fold_mark == 0) {
					
					printk("Could read foldmark at block %d\n", first_block);
					format_chain(s, first_block);
				} else {
					if (fold_mark == FOLD_MARK_IN_PROGRESS)
						check_sectors_in_chain(s, first_block);

					first_block1 = s->EUNtable[first_logical_block];
					if (first_block1 != BLOCK_NIL) {
						
						chain_length1 = calc_chain_length(s, first_block1);
						printk("Two chains at blocks %d (len=%d) and %d (len=%d)\n",
						       first_block1, chain_length1, first_block, chain_length);

						if (chain_length >= chain_length1) {
							chain_to_format = first_block1;
							s->EUNtable[first_logical_block] = first_block;
						} else {
							chain_to_format = first_block;
						}
						format_chain(s, chain_to_format);
					} else {
						s->EUNtable[first_logical_block] = first_block;
					}
				}
			}
		}
	examine_ReplUnitTable:;
	}

	
	s->numfreeEUNs = 0;
	s->LastFreeEUN = le16_to_cpu(s->MediaHdr.FirstPhysicalEUN);

	for (block = 0; block < s->nb_blocks; block++) {
		if (s->ReplUnitTable[block] == BLOCK_NOTEXPLORED) {
			printk("Unreferenced block %d, formatting it\n", block);
			if (NFTL_formatblock(s, block) < 0)
				s->ReplUnitTable[block] = BLOCK_RESERVED;
			else
				s->ReplUnitTable[block] = BLOCK_FREE;
		}
		if (s->ReplUnitTable[block] == BLOCK_FREE) {
			s->numfreeEUNs++;
			s->LastFreeEUN = block;
		}
	}

	return 0;
}
