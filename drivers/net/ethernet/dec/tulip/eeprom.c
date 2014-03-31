/*
	drivers/net/ethernet/dec/tulip/eeprom.c

	Copyright 2000,2001  The Linux Kernel Team
	Written/copyright 1994-2001 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	Please submit bug reports to http://bugzilla.kernel.org/.
*/

#include <linux/pci.h>
#include <linux/slab.h>
#include "tulip.h"
#include <linux/init.h>
#include <asm/unaligned.h>




static struct eeprom_fixup eeprom_fixups[] __devinitdata = {
  {"Asante", 0, 0, 0x94, {0x1e00, 0x0000, 0x0800, 0x0100, 0x018c,
			  0x0000, 0x0000, 0xe078, 0x0001, 0x0050, 0x0018 }},
  {"SMC9332DST", 0, 0, 0xC0, { 0x1e00, 0x0000, 0x0800, 0x041f,
			   0x0000, 0x009E, 
			   0x0004, 0x009E, 
			   0x0903, 0x006D, 
			   0x0905, 0x006D,  }},
  {"Cogent EM100", 0, 0, 0x92, { 0x1e00, 0x0000, 0x0800, 0x063f,
				 0x0107, 0x8021, 
				 0x0108, 0x8021, 
				 0x0100, 0x009E, 
				 0x0104, 0x009E, 
				 0x0103, 0x006D, 
				 0x0105, 0x006D,  }},
  {"Maxtech NX-110", 0, 0, 0xE8, { 0x1e00, 0x0000, 0x0800, 0x0513,
				   0x1001, 0x009E, 
				   0x0000, 0x009E, 
				   0x0004, 0x009E, 
				   0x0303, 0x006D, 
				   0x0305, 0x006D, }},
  {"Accton EN1207", 0, 0, 0xE8, { 0x1e00, 0x0000, 0x0800, 0x051F,
				  0x1B01, 0x0000, 
				  0x0B00, 0x009E, 
				  0x0B04, 0x009E, 
				  0x1B03, 0x006D, 
				  0x1B05, 0x006D, 
   }},
  {"NetWinder", 0x00, 0x10, 0x57,
	{ 0x1e00, 0x0000, 0x000b, 0x8f01, 0x0103, 0x0300, 0x0821, 0x000, 0x0001, 0x0000, 0x01e1 }
  },
  {"Cobalt Microserver", 0, 0x10, 0xE0, {0x1e00, 
					 0x0000, 
					 0x0800, 
					 0x8001, 
					 0x0003, 
					 0x0400, 
					 0x0801, 
					 0x0000, 
					 0x0800, 
					 0x0000, 
					 0x7800, 
					 0x01e0, 
					 0x5000, 
					 0x1800, 
					 0x0000, 
  }},
  {NULL}};


static const char *block_name[] __devinitdata = {
	"21140 non-MII",
	"21140 MII PHY",
	"21142 Serial PHY",
	"21142 MII PHY",
	"21143 SYM PHY",
	"21143 reset method"
};


static void __devinit tulip_build_fake_mediatable(struct tulip_private *tp)
{
#ifdef CONFIG_GSC
	if (tp->flags & NEEDS_FAKE_MEDIA_TABLE) {
		static unsigned char leafdata[] =
			{ 0x01,       
			  0x02,       
			  0x02, 0x00, 
			  0x02,       
			  0x01, 0x00, 
			  0x00, 0x78, 
			  0x00, 0xe0, 
			  0x00, 0x05, 
			  0x00, 0x06  
			};

		tp->mtable = kmalloc(sizeof(struct mediatable) +
				     sizeof(struct medialeaf), GFP_KERNEL);

		if (tp->mtable == NULL)
			return; 

		tp->mtable->defaultmedia = 0x800;
		tp->mtable->leafcount = 1;
		tp->mtable->csr12dir = 0x3f; 
		tp->mtable->has_nonmii = 0;
		tp->mtable->has_reset = 0;
		tp->mtable->has_mii = 1;
		tp->mtable->csr15dir = tp->mtable->csr15val = 0;
		tp->mtable->mleaf[0].type = 1;
		tp->mtable->mleaf[0].media = 11;
		tp->mtable->mleaf[0].leafdata = &leafdata[0];
		tp->flags |= HAS_PHY_IRQ;
		tp->csr12_shadow = -1;
	}
#endif
}

void __devinit tulip_parse_eeprom(struct net_device *dev)
{

	
	static struct mediatable *last_mediatable;
	static unsigned char *last_ee_data;
	static int controller_index;
	struct tulip_private *tp = netdev_priv(dev);
	unsigned char *ee_data = tp->eeprom;
	int i;

	tp->mtable = NULL;
	for (i = 0; i < 8; i ++)
		if (ee_data[i] != ee_data[16+i])
			break;
	if (i >= 8) {
		if (ee_data[0] == 0xff) {
			if (last_mediatable) {
				controller_index++;
				pr_info("%s: Controller %d of multiport board\n",
					dev->name, controller_index);
				tp->mtable = last_mediatable;
				ee_data = last_ee_data;
				goto subsequent_board;
			} else
				pr_info("%s: Missing EEPROM, this interface may not work correctly!\n",
					dev->name);
			return;
		}
	  
	  for (i = 0; eeprom_fixups[i].name; i++) {
		  if (dev->dev_addr[0] == eeprom_fixups[i].addr0 &&
		      dev->dev_addr[1] == eeprom_fixups[i].addr1 &&
		      dev->dev_addr[2] == eeprom_fixups[i].addr2) {
		  if (dev->dev_addr[2] == 0xE8 && ee_data[0x1a] == 0x55)
			  i++;			
		  memcpy(ee_data + 26, eeprom_fixups[i].newtable,
				 sizeof(eeprom_fixups[i].newtable));
		  pr_info("%s: Old format EEPROM on '%s' board.  Using substitute media control info\n",
			  dev->name, eeprom_fixups[i].name);
		  break;
		}
	  }
	  if (eeprom_fixups[i].name == NULL) { 
		  pr_info("%s: Old style EEPROM with no media selection information\n",
			  dev->name);
		return;
	  }
	}

	controller_index = 0;
	if (ee_data[19] > 1) {		
		last_ee_data = ee_data;
	}
subsequent_board:

	if (ee_data[27] == 0) {		
		tulip_build_fake_mediatable(tp);
	} else {
		unsigned char *p = (void *)ee_data + ee_data[27];
		unsigned char csr12dir = 0;
		int count, new_advertise = 0;
		struct mediatable *mtable;
		u16 media = get_u16(p);

		p += 2;
		if (tp->flags & CSR12_IN_SROM)
			csr12dir = *p++;
		count = *p++;

	        
	        if (count == 0) {
			if (tulip_debug > 0)
				pr_warn("%s: no phy info, aborting mtable build\n",
					dev->name);
		        return;
		}

		mtable = kmalloc(sizeof(struct mediatable) +
				 count * sizeof(struct medialeaf),
				 GFP_KERNEL);
		if (mtable == NULL)
			return;				
		last_mediatable = tp->mtable = mtable;
		mtable->defaultmedia = media;
		mtable->leafcount = count;
		mtable->csr12dir = csr12dir;
		mtable->has_nonmii = mtable->has_mii = mtable->has_reset = 0;
		mtable->csr15dir = mtable->csr15val = 0;

		pr_info("%s: EEPROM default media type %s\n",
			dev->name,
			media & 0x0800 ? "Autosense"
				       : medianame[media & MEDIA_MASK]);
		for (i = 0; i < count; i++) {
			struct medialeaf *leaf = &mtable->mleaf[i];

			if ((p[0] & 0x80) == 0) { 
				leaf->type = 0;
				leaf->media = p[0] & 0x3f;
				leaf->leafdata = p;
				if ((p[2] & 0x61) == 0x01)	
					mtable->has_mii = 1;
				p += 4;
			} else {
				leaf->type = p[1];
				if (p[1] == 0x05) {
					mtable->has_reset = i;
					leaf->media = p[2] & 0x0f;
				} else if (tp->chip_id == DM910X && p[1] == 0x80) {
					
					mtable->leafcount--;
					count--;
					i--;
					leaf->leafdata = p + 2;
					p += (p[0] & 0x3f) + 1;
					continue;
				} else if (p[1] & 1) {
					int gpr_len, reset_len;

					mtable->has_mii = 1;
					leaf->media = 11;
					gpr_len=p[3]*2;
					reset_len=p[4+gpr_len]*2;
					new_advertise |= get_u16(&p[7+gpr_len+reset_len]);
				} else {
					mtable->has_nonmii = 1;
					leaf->media = p[2] & MEDIA_MASK;
					
					if (tp->chip_id == DM910X && leaf->media == 1)
						leaf->media = 3;
					switch (leaf->media) {
					case 0: new_advertise |= 0x0020; break;
					case 4: new_advertise |= 0x0040; break;
					case 3: new_advertise |= 0x0080; break;
					case 5: new_advertise |= 0x0100; break;
					case 6: new_advertise |= 0x0200; break;
					}
					if (p[1] == 2  &&  leaf->media == 0) {
						if (p[2] & 0x40) {
							u32 base15 = get_unaligned((u16*)&p[7]);
							mtable->csr15dir =
								(get_unaligned((u16*)&p[9])<<16) + base15;
							mtable->csr15val =
								(get_unaligned((u16*)&p[11])<<16) + base15;
						} else {
							mtable->csr15dir = get_unaligned((u16*)&p[3])<<16;
							mtable->csr15val = get_unaligned((u16*)&p[5])<<16;
						}
					}
				}
				leaf->leafdata = p + 2;
				p += (p[0] & 0x3f) + 1;
			}
			if (tulip_debug > 1  &&  leaf->media == 11) {
				unsigned char *bp = leaf->leafdata;
				pr_info("%s: MII interface PHY %d, setup/reset sequences %d/%d long, capabilities %02x %02x\n",
					dev->name,
					bp[0], bp[1], bp[2 + bp[1]*2],
					bp[5 + bp[2 + bp[1]*2]*2],
					bp[4 + bp[2 + bp[1]*2]*2]);
			}
			pr_info("%s: Index #%d - Media %s (#%d) described by a %s (%d) block\n",
				dev->name,
				i, medianame[leaf->media & 15], leaf->media,
				leaf->type < ARRAY_SIZE(block_name) ? block_name[leaf->type] : "<unknown>",
				leaf->type);
		}
		if (new_advertise)
			tp->sym_advertise = new_advertise;
	}
}

#define EE_SHIFT_CLK	0x02	
#define EE_CS		0x01	
#define EE_DATA_WRITE	0x04	
#define EE_WRITE_0	0x01
#define EE_WRITE_1	0x05
#define EE_DATA_READ	0x08	
#define EE_ENB		(0x4800 | EE_CS)

#define eeprom_delay()	ioread32(ee_addr)

#define EE_READ_CMD		(6)

int __devinit tulip_read_eeprom(struct net_device *dev, int location, int addr_len)
{
	int i;
	unsigned retval = 0;
	struct tulip_private *tp = netdev_priv(dev);
	void __iomem *ee_addr = tp->base_addr + CSR9;
	int read_cmd = location | (EE_READ_CMD << addr_len);

	if (location > (1 << addr_len) - 1)
		return 0;

	iowrite32(EE_ENB & ~EE_CS, ee_addr);
	iowrite32(EE_ENB, ee_addr);

	
	for (i = 4 + addr_len; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		iowrite32(EE_ENB | dataval, ee_addr);
		eeprom_delay();
		iowrite32(EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((ioread32(ee_addr) & EE_DATA_READ) ? 1 : 0);
	}
	iowrite32(EE_ENB, ee_addr);
	eeprom_delay();

	for (i = 16; i > 0; i--) {
		iowrite32(EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((ioread32(ee_addr) & EE_DATA_READ) ? 1 : 0);
		iowrite32(EE_ENB, ee_addr);
		eeprom_delay();
	}

	
	iowrite32(EE_ENB & ~EE_CS, ee_addr);
	return (tp->flags & HAS_SWAPPED_SEEPROM) ? swab16(retval) : retval;
}

