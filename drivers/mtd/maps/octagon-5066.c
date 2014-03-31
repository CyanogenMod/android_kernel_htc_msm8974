
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <asm/io.h>

#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>

#define WINDOW_START 0xe8000
#define WINDOW_LENGTH 0x8000
#define WINDOW_SHIFT 27
#define WINDOW_MASK 0x7FFF
#define PAGE_IO 0x208

static volatile char page_n_dev = 0;
static unsigned long iomapadr;
static DEFINE_SPINLOCK(oct5066_spin);


static void __oct5066_page(struct map_info *map, __u8 byte)
{
	outb(byte,PAGE_IO);
	page_n_dev = byte;
}

static inline void oct5066_page(struct map_info *map, unsigned long ofs)
{
	__u8 byte = map->map_priv_1 | (ofs >> WINDOW_SHIFT);

	if (page_n_dev != byte)
		__oct5066_page(map, byte);
}


static map_word oct5066_read8(struct map_info *map, unsigned long ofs)
{
	map_word ret;
	spin_lock(&oct5066_spin);
	oct5066_page(map, ofs);
	ret.x[0] = readb(iomapadr + (ofs & WINDOW_MASK));
	spin_unlock(&oct5066_spin);
	return ret;
}

static void oct5066_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	while(len) {
		unsigned long thislen = len;
		if (len > (WINDOW_LENGTH - (from & WINDOW_MASK)))
			thislen = WINDOW_LENGTH-(from & WINDOW_MASK);

		spin_lock(&oct5066_spin);
		oct5066_page(map, from);
		memcpy_fromio(to, iomapadr + from, thislen);
		spin_unlock(&oct5066_spin);
		to += thislen;
		from += thislen;
		len -= thislen;
	}
}

static void oct5066_write8(struct map_info *map, map_word d, unsigned long adr)
{
	spin_lock(&oct5066_spin);
	oct5066_page(map, adr);
	writeb(d.x[0], iomapadr + (adr & WINDOW_MASK));
	spin_unlock(&oct5066_spin);
}

static void oct5066_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	while(len) {
		unsigned long thislen = len;
		if (len > (WINDOW_LENGTH - (to & WINDOW_MASK)))
			thislen = WINDOW_LENGTH-(to & WINDOW_MASK);

		spin_lock(&oct5066_spin);
		oct5066_page(map, to);
		memcpy_toio(iomapadr + to, from, thislen);
		spin_unlock(&oct5066_spin);
		to += thislen;
		from += thislen;
		len -= thislen;
	}
}

static struct map_info oct5066_map[2] = {
	{
		.name = "Octagon 5066 Socket",
		.phys = NO_XIP,
		.size = 512 * 1024,
		.bankwidth = 1,
		.read = oct5066_read8,
		.copy_from = oct5066_copy_from,
		.write = oct5066_write8,
		.copy_to = oct5066_copy_to,
		.map_priv_1 = 1<<6
	},
	{
		.name = "Octagon 5066 Internal Flash",
		.phys = NO_XIP,
		.size = 2 * 1024 * 1024,
		.bankwidth = 1,
		.read = oct5066_read8,
		.copy_from = oct5066_copy_from,
		.write = oct5066_write8,
		.copy_to = oct5066_copy_to,
		.map_priv_1 = 2<<6
	}
};

static struct mtd_info *oct5066_mtd[2] = {NULL, NULL};

static int __init OctProbe(void)
{
   unsigned int Base = (1 << 6);
   unsigned long I;
   unsigned long Values[10];
   for (I = 0; I != 20; I++)
   {
      outb(Base + (I%10),PAGE_IO);
      if (I < 10)
      {
	 
	 Values[I%10] = readl(iomapadr);
	 if (I > 0 && Values[I%10] == Values[0])
	    return -EAGAIN;
      }
      else
      {
	 
	 if (Values[I%10] != readl(iomapadr))
	    return -EAGAIN;
      }
   }
   return 0;
}

void cleanup_oct5066(void)
{
	int i;
	for (i=0; i<2; i++) {
		if (oct5066_mtd[i]) {
			mtd_device_unregister(oct5066_mtd[i]);
			map_destroy(oct5066_mtd[i]);
		}
	}
	iounmap((void *)iomapadr);
	release_region(PAGE_IO, 1);
}

static int __init init_oct5066(void)
{
	int i;
	int ret = 0;

	
	if (!request_region(PAGE_IO,1,"Octagon SSD")) {
		printk(KERN_NOTICE "5066: Page Register in Use\n");
		return -EAGAIN;
	}
	iomapadr = (unsigned long)ioremap(WINDOW_START, WINDOW_LENGTH);
	if (!iomapadr) {
		printk(KERN_NOTICE "Failed to ioremap memory region\n");
		ret = -EIO;
		goto out_rel;
	}
	if (OctProbe() != 0) {
		printk(KERN_NOTICE "5066: Octagon Probe Failed, is this an Octagon 5066 SBC?\n");
		iounmap((void *)iomapadr);
		ret = -EAGAIN;
		goto out_unmap;
	}

	
	printk("Octagon 5066 SSD IO:0x%x MEM:0x%x-0x%x\n",PAGE_IO,WINDOW_START,
	       WINDOW_START+WINDOW_LENGTH);

	for (i=0; i<2; i++) {
		oct5066_mtd[i] = do_map_probe("cfi_probe", &oct5066_map[i]);
		if (!oct5066_mtd[i])
			oct5066_mtd[i] = do_map_probe("jedec", &oct5066_map[i]);
		if (!oct5066_mtd[i])
			oct5066_mtd[i] = do_map_probe("map_ram", &oct5066_map[i]);
		if (!oct5066_mtd[i])
			oct5066_mtd[i] = do_map_probe("map_rom", &oct5066_map[i]);
		if (oct5066_mtd[i]) {
			oct5066_mtd[i]->owner = THIS_MODULE;
			mtd_device_register(oct5066_mtd[i], NULL, 0);
		}
	}

	if (!oct5066_mtd[0] && !oct5066_mtd[1]) {
		cleanup_oct5066();
		return -ENXIO;
	}

	return 0;

 out_unmap:
	iounmap((void *)iomapadr);
 out_rel:
	release_region(PAGE_IO, 1);
	return ret;
}

module_init(init_oct5066);
module_exit(cleanup_oct5066);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Gunthorpe <jgg@deltatee.com>, David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("MTD map driver for Octagon 5066 Single Board Computer");
