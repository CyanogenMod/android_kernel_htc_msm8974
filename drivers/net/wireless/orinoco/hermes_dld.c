/*
 * Hermes download helper.
 *
 * This helper:
 *  - is capable of writing to the volatile area of the hermes device
 *  - is currently not capable of writing to non-volatile areas
 *  - provide helpers to identify and update plugin data
 *  - is not capable of interpreting a fw image directly. That is up to
 *    the main card driver.
 *  - deals with Hermes I devices. It can probably be modified to deal
 *    with Hermes II devices
 *
 * Copyright (C) 2007, David Kilroy
 *
 * Plug data code slightly modified from spectrum_cs driver
 *    Copyright (C) 2002-2005 Pavel Roskin <proski@gnu.org>
 * Portions based on information in wl_lkm_718 Agere driver
 *    COPYRIGHT (C) 2001-2004 by Agere Systems Inc. All Rights Reserved
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include "hermes.h"
#include "hermes_dld.h"

#define PFX "hermes_dld: "

#define PDI_END		0x00000000	
#define BLOCK_END	0xFFFFFFFF	
#define TEXT_END	0x1A		


struct dblock {
	__le32 addr;		
	__le16 len;		
	char data[0];		/* data to be written */
} __packed;

/*
 * Plug Data References are located in the image after the last data
 * block.  They refer to areas in the adapter memory where the plug data
 * items with matching ID should be written.
 */
struct pdr {
	__le32 id;		
	__le32 addr;		
	__le32 len;		
	char next[0];		
} __packed;

struct pdi {
	__le16 len;		
	__le16 id;		
	char data[0];		
} __packed;


static inline u32
dblock_addr(const struct dblock *blk)
{
	return le32_to_cpu(blk->addr);
}

static inline u32
dblock_len(const struct dblock *blk)
{
	return le16_to_cpu(blk->len);
}


static inline u32
pdr_id(const struct pdr *pdr)
{
	return le32_to_cpu(pdr->id);
}

static inline u32
pdr_addr(const struct pdr *pdr)
{
	return le32_to_cpu(pdr->addr);
}

static inline u32
pdr_len(const struct pdr *pdr)
{
	return le32_to_cpu(pdr->len);
}


static inline u32
pdi_id(const struct pdi *pdi)
{
	return le16_to_cpu(pdi->id);
}

static inline u32
pdi_len(const struct pdi *pdi)
{
	return 2 * (le16_to_cpu(pdi->len) - 1);
}


static const struct pdr *
hermes_find_pdr(const struct pdr *first_pdr, u32 record_id, const void *end)
{
	const struct pdr *pdr = first_pdr;

	end -= sizeof(struct pdr);

	while (((void *) pdr <= end) &&
	       (pdr_id(pdr) != PDI_END)) {
		if (pdr_len(pdr) < 2)
			return NULL;

		
		if (pdr_id(pdr) == record_id)
			return pdr;

		pdr = (struct pdr *) pdr->next;
	}
	return NULL;
}

static const struct pdi *
hermes_find_pdi(const struct pdi *first_pdi, u32 record_id, const void *end)
{
	const struct pdi *pdi = first_pdi;

	end -= sizeof(struct pdi);

	while (((void *) pdi <= end) &&
	       (pdi_id(pdi) != PDI_END)) {

		
		if (pdi_id(pdi) == record_id)
			return pdi;

		pdi = (struct pdi *) &pdi->data[pdi_len(pdi)];
	}
	return NULL;
}

static int
hermes_plug_pdi(struct hermes *hw, const struct pdr *first_pdr,
		const struct pdi *pdi, const void *pdr_end)
{
	const struct pdr *pdr;

	
	pdr = hermes_find_pdr(first_pdr, pdi_id(pdi), pdr_end);

	
	if (!pdr)
		return 0;

	
	if (pdi_len(pdi) != pdr_len(pdr))
		return -EINVAL;

	
	hw->ops->program(hw, pdi->data, pdr_addr(pdr), pdi_len(pdi));

	return 0;
}

int hermes_apply_pda(struct hermes *hw,
		     const char *first_pdr,
		     const void *pdr_end,
		     const __le16 *pda,
		     const void *pda_end)
{
	int ret;
	const struct pdi *pdi;
	const struct pdr *pdr;

	pdr = (const struct pdr *) first_pdr;
	pda_end -= sizeof(struct pdi);

	
	pdi = (const struct pdi *) (pda + 2);
	while (((void *) pdi <= pda_end) &&
	       (pdi_id(pdi) != PDI_END)) {
		ret = hermes_plug_pdi(hw, pdr, pdi, pdr_end);
		if (ret)
			return ret;

		
		pdi = (const struct pdi *) &pdi->data[pdi_len(pdi)];
	}
	return 0;
}

size_t
hermes_blocks_length(const char *first_block, const void *end)
{
	const struct dblock *blk = (const struct dblock *) first_block;
	int total_len = 0;
	int len;

	end -= sizeof(*blk);

	while (((void *) blk <= end) &&
	       (dblock_addr(blk) != BLOCK_END)) {
		len = dblock_len(blk);
		total_len += sizeof(*blk) + len;
		blk = (struct dblock *) &blk->data[len];
	}

	return total_len;
}


int hermes_program(struct hermes *hw, const char *first_block, const void *end)
{
	const struct dblock *blk;
	u32 blkaddr;
	u32 blklen;
	int err = 0;

	blk = (const struct dblock *) first_block;

	if ((void *) blk > (end - sizeof(*blk)))
		return -EIO;

	blkaddr = dblock_addr(blk);
	blklen = dblock_len(blk);

	while ((blkaddr != BLOCK_END) &&
	       (((void *) blk + blklen) <= end)) {
		pr_debug(PFX "Programming block of length %d "
			 "to address 0x%08x\n", blklen, blkaddr);

		err = hw->ops->program(hw, blk->data, blkaddr, blklen);
		if (err)
			break;

		blk = (const struct dblock *) &blk->data[blklen];

		if ((void *) blk > (end - sizeof(*blk)))
			return -EIO;

		blkaddr = dblock_addr(blk);
		blklen = dblock_len(blk);
	}
	return err;
}


#define DEFINE_DEFAULT_PDR(pid, length, data)				\
static const struct {							\
	__le16 len;							\
	__le16 id;							\
	u8 val[length];							\
} __packed default_pdr_data_##pid = {			\
	cpu_to_le16((sizeof(default_pdr_data_##pid)/			\
				sizeof(__le16)) - 1),			\
	cpu_to_le16(pid),						\
	data								\
}

#define DEFAULT_PDR(pid) default_pdr_data_##pid

DEFINE_DEFAULT_PDR(0x0005, 10, "\x00\x00\x06\x00\x01\x00\x01\x00\x01\x00");

DEFINE_DEFAULT_PDR(0x0108, 4, "\x00\x00\x00\x00");

DEFINE_DEFAULT_PDR(0x0109, 10, "\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00");

DEFINE_DEFAULT_PDR(0x0150, 2, "\x00\x3F");

DEFINE_DEFAULT_PDR(0x0160, 28,
		   "\x00\x00\x00\x00\x00\x00\x00\x00"
		   "\x00\x00\x00\x00\x00\x00\x00\x00"
		   "\x00\x00\x00\x00\x00\x00\x00\x00"
		   "\x00\x00\x00\x00");

DEFINE_DEFAULT_PDR(0x0161, 256,
		   "\x3F\x01\x3F\01\x3F\x01\x3F\x01"
		   "\x3F\x01\x3F\01\x3F\x01\x3F\x01"
		   "\x3F\x01\x3F\01\x3F\x01\x3F\x01"
		   "\x3F\x01\x3F\01\x3F\x01\x3F\x01"
		   "\x3F\x01\x3E\01\x3E\x01\x3D\x01"
		   "\x3D\x01\x3C\01\x3C\x01\x3B\x01"
		   "\x3B\x01\x3A\01\x3A\x01\x39\x01"
		   "\x39\x01\x38\01\x38\x01\x37\x01"
		   "\x37\x01\x36\01\x36\x01\x35\x01"
		   "\x35\x01\x34\01\x34\x01\x33\x01"
		   "\x33\x01\x32\x01\x32\x01\x31\x01"
		   "\x31\x01\x30\x01\x30\x01\x7B\x01"
		   "\x7B\x01\x7A\x01\x7A\x01\x79\x01"
		   "\x79\x01\x78\x01\x78\x01\x77\x01"
		   "\x77\x01\x76\x01\x76\x01\x75\x01"
		   "\x75\x01\x74\x01\x74\x01\x73\x01"
		   "\x73\x01\x72\x01\x72\x01\x71\x01"
		   "\x71\x01\x70\x01\x70\x01\x68\x01"
		   "\x68\x01\x67\x01\x67\x01\x66\x01"
		   "\x66\x01\x65\x01\x65\x01\x57\x01"
		   "\x57\x01\x56\x01\x56\x01\x55\x01"
		   "\x55\x01\x54\x01\x54\x01\x53\x01"
		   "\x53\x01\x52\x01\x52\x01\x51\x01"
		   "\x51\x01\x50\x01\x50\x01\x48\x01"
		   "\x48\x01\x47\x01\x47\x01\x46\x01"
		   "\x46\x01\x45\x01\x45\x01\x44\x01"
		   "\x44\x01\x43\x01\x43\x01\x42\x01"
		   "\x42\x01\x41\x01\x41\x01\x40\x01"
		   "\x40\x01\x40\x01\x40\x01\x40\x01"
		   "\x40\x01\x40\x01\x40\x01\x40\x01"
		   "\x40\x01\x40\x01\x40\x01\x40\x01"
		   "\x40\x01\x40\x01\x40\x01\x40\x01");

int hermes_apply_pda_with_defaults(struct hermes *hw,
				   const char *first_pdr,
				   const void *pdr_end,
				   const __le16 *pda,
				   const void *pda_end)
{
	const struct pdr *pdr = (const struct pdr *) first_pdr;
	const struct pdi *first_pdi = (const struct pdi *) &pda[2];
	const struct pdi *pdi;
	const struct pdi *default_pdi = NULL;
	const struct pdi *outdoor_pdi;
	int record_id;

	pdr_end -= sizeof(struct pdr);

	while (((void *) pdr <= pdr_end) &&
	       (pdr_id(pdr) != PDI_END)) {
		if (pdr_len(pdr) < 2)
			break;
		record_id = pdr_id(pdr);

		pdi = hermes_find_pdi(first_pdi, record_id, pda_end);
		if (pdi)
			pr_debug(PFX "Found record 0x%04x at %p\n",
				 record_id, pdi);

		switch (record_id) {
		case 0x110: 
		case 0x120: 
			outdoor_pdi = hermes_find_pdi(first_pdi, record_id + 1,
						      pda_end);
			default_pdi = NULL;
			if (outdoor_pdi) {
				pdi = outdoor_pdi;
				pr_debug(PFX
					 "Using outdoor record 0x%04x at %p\n",
					 record_id + 1, pdi);
			}
			break;
		case 0x5: 
			default_pdi = (struct pdi *) &DEFAULT_PDR(0x0005);
			break;
		case 0x108: 
			default_pdi = (struct pdi *) &DEFAULT_PDR(0x0108);
			break;
		case 0x109: 
			default_pdi = (struct pdi *) &DEFAULT_PDR(0x0109);
			break;
		case 0x150: 
			default_pdi = (struct pdi *) &DEFAULT_PDR(0x0150);
			break;
		case 0x160: 
			default_pdi = (struct pdi *) &DEFAULT_PDR(0x0160);
			break;
		case 0x161: 
			default_pdi = (struct pdi *) &DEFAULT_PDR(0x0161);
			break;
		default:
			default_pdi = NULL;
			break;
		}
		if (!pdi && default_pdi) {
			
			pdi = default_pdi;
			pr_debug(PFX "Using default record 0x%04x at %p\n",
				 record_id, pdi);
		}

		if (pdi) {
			
			if ((pdi_len(pdi) == pdr_len(pdr)) &&
			    ((void *) pdi->data + pdi_len(pdi) < pda_end)) {
				
				hw->ops->program(hw, pdi->data, pdr_addr(pdr),
						 pdi_len(pdi));
			}
		}

		pdr++;
	}
	return 0;
}
