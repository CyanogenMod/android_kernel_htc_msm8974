/*
 *  linux/zorro.h -- Amiga AutoConfig (Zorro) Bus Definitions
 *
 *  Copyright (C) 1995--2003 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#ifndef _LINUX_ZORRO_H
#define _LINUX_ZORRO_H

#include <linux/device.h>




#define ZORRO_MANUF(id)		((id) >> 16)
#define ZORRO_PROD(id)		(((id) >> 8) & 0xff)
#define ZORRO_EPC(id)		((id) & 0xff)

#define ZORRO_ID(manuf, prod, epc) \
    ((ZORRO_MANUF_##manuf << 16) | ((prod) << 8) | (epc))

typedef __u32 zorro_id;


#include <linux/zorro_ids.h>



#define GVP_PRODMASK			(0xf8)
#define GVP_SCSICLKMASK			(0x01)

enum GVP_flags {
    GVP_IO		= 0x01,
    GVP_ACCEL		= 0x02,
    GVP_SCSI		= 0x04,
    GVP_24BITDMA	= 0x08,
    GVP_25BITDMA	= 0x10,
    GVP_NOBANK		= 0x20,
    GVP_14MHZ		= 0x40,
};


struct Node {
    struct  Node *ln_Succ;	
    struct  Node *ln_Pred;	
    __u8    ln_Type;
    __s8    ln_Pri;		
    __s8    *ln_Name;		
} __attribute__ ((packed));

struct ExpansionRom {
    
    __u8  er_Type;		
    __u8  er_Product;		
    __u8  er_Flags;		
    __u8  er_Reserved03;	
    __u16 er_Manufacturer;	
    __u32 er_SerialNumber;	
    __u16 er_InitDiagVec;	
    __u8  er_Reserved0c;
    __u8  er_Reserved0d;
    __u8  er_Reserved0e;
    __u8  er_Reserved0f;
} __attribute__ ((packed));

#define ERT_TYPEMASK	0xc0
#define ERT_ZORROII	0xc0
#define ERT_ZORROIII	0x80

#define ERTB_MEMLIST	5		
#define ERTF_MEMLIST	(1<<5)

struct ConfigDev {
    struct Node		cd_Node;
    __u8		cd_Flags;	
    __u8		cd_Pad;		
    struct ExpansionRom cd_Rom;		
    void		*cd_BoardAddr;	
    __u32		cd_BoardSize;	
    __u16		cd_SlotAddr;	
    __u16		cd_SlotSize;	
    void		*cd_Driver;	
    struct ConfigDev	*cd_NextCD;	
    __u32		cd_Unused[4];	
} __attribute__ ((packed));

#define ZORRO_NUM_AUTO		16

#ifdef __KERNEL__

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/mod_devicetable.h>

#include <asm/zorro.h>



struct zorro_dev {
    struct ExpansionRom rom;
    zorro_id id;
    struct zorro_driver *driver;	
    struct device dev;			
    u16 slotaddr;
    u16 slotsize;
    char name[64];
    struct resource resource;
};

#define	to_zorro_dev(n)	container_of(n, struct zorro_dev, dev)



extern struct bus_type zorro_bus_type;



struct zorro_driver {
    struct list_head node;
    char *name;
    const struct zorro_device_id *id_table;	
    int (*probe)(struct zorro_dev *z, const struct zorro_device_id *id);	
    void (*remove)(struct zorro_dev *z);	
    struct device_driver driver;
};

#define	to_zorro_driver(drv)	container_of(drv, struct zorro_driver, driver)


#define zorro_for_each_dev(dev)	\
	for (dev = &zorro_autocon[0]; dev < zorro_autocon+zorro_num_autocon; dev++)


extern int zorro_register_driver(struct zorro_driver *);
extern void zorro_unregister_driver(struct zorro_driver *);
extern const struct zorro_device_id *zorro_match_device(const struct zorro_device_id *ids, const struct zorro_dev *z);
static inline struct zorro_driver *zorro_dev_driver(const struct zorro_dev *z)
{
    return z->driver;
}


extern unsigned int zorro_num_autocon;	
extern struct zorro_dev zorro_autocon[ZORRO_NUM_AUTO];



extern struct zorro_dev *zorro_find_device(zorro_id id,
					   struct zorro_dev *from);

#define zorro_resource_start(z)	((z)->resource.start)
#define zorro_resource_end(z)	((z)->resource.end)
#define zorro_resource_len(z)	(resource_size(&(z)->resource))
#define zorro_resource_flags(z)	((z)->resource.flags)

#define zorro_request_device(z, name) \
    request_mem_region(zorro_resource_start(z), zorro_resource_len(z), name)
#define zorro_release_device(z) \
    release_mem_region(zorro_resource_start(z), zorro_resource_len(z))

static inline void *zorro_get_drvdata (struct zorro_dev *z)
{
	return dev_get_drvdata(&z->dev);
}

static inline void zorro_set_drvdata (struct zorro_dev *z, void *data)
{
	dev_set_drvdata(&z->dev, data);
}



extern DECLARE_BITMAP(zorro_unused_z2ram, 128);

#define Z2RAM_START		(0x00200000)
#define Z2RAM_END		(0x00a00000)
#define Z2RAM_SIZE		(0x00800000)
#define Z2RAM_CHUNKSIZE		(0x00010000)
#define Z2RAM_CHUNKMASK		(0x0000ffff)
#define Z2RAM_CHUNKSHIFT	(16)


#endif 

#endif 
