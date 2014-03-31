/*
 * Common Flash Interface support:
 *   ST Advanced Architecture Command Set (ID 0x0020)
 *
 * (C) 2000 Red Hat. GPL'd
 *
 * 10/10/2000	Nicolas Pitre <nico@fluxnic.net>
 * 	- completely revamped method functions so they are aware and
 * 	  independent of the flash geometry (buswidth, interleave, etc.)
 * 	- scalability vs code size is completely set at compile-time
 * 	  (see include/linux/mtd/cfi.h for selection)
 *	- optimized write buffer method
 * 06/21/2002	Joern Engel <joern@wh.fh-wedel.de> and others
 *	- modified Intel Command Set 0x0001 to support ST Advanced Architecture
 *	  (command set 0x0020)
 *	- added a writev function
 * 07/13/2005	Joern Engel <joern@wh.fh-wedel.de>
 * 	- Plugged memory leak in cfi_staa_writev().
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/mtd.h>


static int cfi_staa_read(struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cfi_staa_write_buffers(struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
static int cfi_staa_writev(struct mtd_info *mtd, const struct kvec *vecs,
		unsigned long count, loff_t to, size_t *retlen);
static int cfi_staa_erase_varsize(struct mtd_info *, struct erase_info *);
static void cfi_staa_sync (struct mtd_info *);
static int cfi_staa_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
static int cfi_staa_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
static int cfi_staa_suspend (struct mtd_info *);
static void cfi_staa_resume (struct mtd_info *);

static void cfi_staa_destroy(struct mtd_info *);

struct mtd_info *cfi_cmdset_0020(struct map_info *, int);

static struct mtd_info *cfi_staa_setup (struct map_info *);

static struct mtd_chip_driver cfi_staa_chipdrv = {
	.probe		= NULL, 
	.destroy	= cfi_staa_destroy,
	.name		= "cfi_cmdset_0020",
	.module		= THIS_MODULE
};


#ifdef DEBUG_CFI_FEATURES
static void cfi_tell_features(struct cfi_pri_intelext *extp)
{
        int i;
        printk("  Feature/Command Support: %4.4X\n", extp->FeatureSupport);
	printk("     - Chip Erase:         %s\n", extp->FeatureSupport&1?"supported":"unsupported");
	printk("     - Suspend Erase:      %s\n", extp->FeatureSupport&2?"supported":"unsupported");
	printk("     - Suspend Program:    %s\n", extp->FeatureSupport&4?"supported":"unsupported");
	printk("     - Legacy Lock/Unlock: %s\n", extp->FeatureSupport&8?"supported":"unsupported");
	printk("     - Queued Erase:       %s\n", extp->FeatureSupport&16?"supported":"unsupported");
	printk("     - Instant block lock: %s\n", extp->FeatureSupport&32?"supported":"unsupported");
	printk("     - Protection Bits:    %s\n", extp->FeatureSupport&64?"supported":"unsupported");
	printk("     - Page-mode read:     %s\n", extp->FeatureSupport&128?"supported":"unsupported");
	printk("     - Synchronous read:   %s\n", extp->FeatureSupport&256?"supported":"unsupported");
	for (i=9; i<32; i++) {
		if (extp->FeatureSupport & (1<<i))
			printk("     - Unknown Bit %X:      supported\n", i);
	}

	printk("  Supported functions after Suspend: %2.2X\n", extp->SuspendCmdSupport);
	printk("     - Program after Erase Suspend: %s\n", extp->SuspendCmdSupport&1?"supported":"unsupported");
	for (i=1; i<8; i++) {
		if (extp->SuspendCmdSupport & (1<<i))
			printk("     - Unknown Bit %X:               supported\n", i);
	}

	printk("  Block Status Register Mask: %4.4X\n", extp->BlkStatusRegMask);
	printk("     - Lock Bit Active:      %s\n", extp->BlkStatusRegMask&1?"yes":"no");
	printk("     - Valid Bit Active:     %s\n", extp->BlkStatusRegMask&2?"yes":"no");
	for (i=2; i<16; i++) {
		if (extp->BlkStatusRegMask & (1<<i))
			printk("     - Unknown Bit %X Active: yes\n",i);
	}

	printk("  Vcc Logic Supply Optimum Program/Erase Voltage: %d.%d V\n",
	       extp->VccOptimal >> 8, extp->VccOptimal & 0xf);
	if (extp->VppOptimal)
		printk("  Vpp Programming Supply Optimum Program/Erase Voltage: %d.%d V\n",
		       extp->VppOptimal >> 8, extp->VppOptimal & 0xf);
}
#endif

struct mtd_info *cfi_cmdset_0020(struct map_info *map, int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int i;

	if (cfi->cfi_mode) {
		__u16 adr = primary?cfi->cfiq->P_ADR:cfi->cfiq->A_ADR;
		struct cfi_pri_intelext *extp;

		extp = (struct cfi_pri_intelext*)cfi_read_pri(map, adr, sizeof(*extp), "ST Microelectronics");
		if (!extp)
			return NULL;

		if (extp->MajorVersion != '1' ||
		    (extp->MinorVersion < '0' || extp->MinorVersion > '3')) {
			printk(KERN_ERR "  Unknown ST Microelectronics"
			       " Extended Query version %c.%c.\n",
			       extp->MajorVersion, extp->MinorVersion);
			kfree(extp);
			return NULL;
		}

		
		extp->FeatureSupport = cfi32_to_cpu(map, extp->FeatureSupport);
		extp->BlkStatusRegMask = cfi32_to_cpu(map,
						extp->BlkStatusRegMask);

#ifdef DEBUG_CFI_FEATURES
		
		cfi_tell_features(extp);
#endif

		
		cfi->cmdset_priv = extp;
	}

	for (i=0; i< cfi->numchips; i++) {
		cfi->chips[i].word_write_time = 128;
		cfi->chips[i].buffer_write_time = 128;
		cfi->chips[i].erase_time = 1024;
		cfi->chips[i].ref_point_counter = 0;
		init_waitqueue_head(&(cfi->chips[i].wq));
	}

	return cfi_staa_setup(map);
}
EXPORT_SYMBOL_GPL(cfi_cmdset_0020);

static struct mtd_info *cfi_staa_setup(struct map_info *map)
{
	struct cfi_private *cfi = map->fldrv_priv;
	struct mtd_info *mtd;
	unsigned long offset = 0;
	int i,j;
	unsigned long devsize = (1<<cfi->cfiq->DevSize) * cfi->interleave;

	mtd = kzalloc(sizeof(*mtd), GFP_KERNEL);
	

	if (!mtd) {
		printk(KERN_ERR "Failed to allocate memory for MTD device\n");
		kfree(cfi->cmdset_priv);
		return NULL;
	}

	mtd->priv = map;
	mtd->type = MTD_NORFLASH;
	mtd->size = devsize * cfi->numchips;

	mtd->numeraseregions = cfi->cfiq->NumEraseRegions * cfi->numchips;
	mtd->eraseregions = kmalloc(sizeof(struct mtd_erase_region_info)
			* mtd->numeraseregions, GFP_KERNEL);
	if (!mtd->eraseregions) {
		printk(KERN_ERR "Failed to allocate memory for MTD erase region info\n");
		kfree(cfi->cmdset_priv);
		kfree(mtd);
		return NULL;
	}

	for (i=0; i<cfi->cfiq->NumEraseRegions; i++) {
		unsigned long ernum, ersize;
		ersize = ((cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xff) * cfi->interleave;
		ernum = (cfi->cfiq->EraseRegionInfo[i] & 0xffff) + 1;

		if (mtd->erasesize < ersize) {
			mtd->erasesize = ersize;
		}
		for (j=0; j<cfi->numchips; j++) {
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].offset = (j*devsize)+offset;
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].erasesize = ersize;
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].numblocks = ernum;
		}
		offset += (ersize * ernum);
		}

		if (offset != devsize) {
			
			printk(KERN_WARNING "Sum of regions (%lx) != total size of set of interleaved chips (%lx)\n", offset, devsize);
			kfree(mtd->eraseregions);
			kfree(cfi->cmdset_priv);
			kfree(mtd);
			return NULL;
		}

		for (i=0; i<mtd->numeraseregions;i++){
			printk(KERN_DEBUG "%d: offset=0x%llx,size=0x%x,blocks=%d\n",
			       i, (unsigned long long)mtd->eraseregions[i].offset,
			       mtd->eraseregions[i].erasesize,
			       mtd->eraseregions[i].numblocks);
		}

	
	mtd->_erase = cfi_staa_erase_varsize;
	mtd->_read = cfi_staa_read;
	mtd->_write = cfi_staa_write_buffers;
	mtd->_writev = cfi_staa_writev;
	mtd->_sync = cfi_staa_sync;
	mtd->_lock = cfi_staa_lock;
	mtd->_unlock = cfi_staa_unlock;
	mtd->_suspend = cfi_staa_suspend;
	mtd->_resume = cfi_staa_resume;
	mtd->flags = MTD_CAP_NORFLASH & ~MTD_BIT_WRITEABLE;
	mtd->writesize = 8; 
	mtd->writebufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;
	map->fldrv = &cfi_staa_chipdrv;
	__module_get(THIS_MODULE);
	mtd->name = map->name;
	return mtd;
}


static inline int do_read_onechip(struct map_info *map, struct flchip *chip, loff_t adr, size_t len, u_char *buf)
{
	map_word status, status_OK;
	unsigned long timeo;
	DECLARE_WAITQUEUE(wait, current);
	int suspended = 0;
	unsigned long cmd_addr;
	struct cfi_private *cfi = map->fldrv_priv;

	adr += chip->start;

	
	cmd_addr = adr & ~(map_bankwidth(map)-1);

	
	status_OK = CMD(0x80);

	timeo = jiffies + HZ;
 retry:
	mutex_lock(&chip->mutex);

	switch (chip->state) {
	case FL_ERASING:
		if (!(((struct cfi_pri_intelext *)cfi->cmdset_priv)->FeatureSupport & 2))
			goto sleep; 

		map_write (map, CMD(0xb0), cmd_addr);
		map_write(map, CMD(0x70), cmd_addr);
		chip->oldstate = FL_ERASING;
		chip->state = FL_ERASE_SUSPENDING;
		
		for (;;) {
			status = map_read(map, cmd_addr);
			if (map_word_andequal(map, status, status_OK, status_OK))
				break;

			if (time_after(jiffies, timeo)) {
				
				map_write(map, CMD(0xd0), cmd_addr);
				
				map_write(map, CMD(0x70), cmd_addr);
				chip->state = FL_ERASING;
				wake_up(&chip->wq);
				mutex_unlock(&chip->mutex);
				printk(KERN_ERR "Chip not ready after erase "
				       "suspended: status = 0x%lx\n", status.x[0]);
				return -EIO;
			}

			mutex_unlock(&chip->mutex);
			cfi_udelay(1);
			mutex_lock(&chip->mutex);
		}

		suspended = 1;
		map_write(map, CMD(0xff), cmd_addr);
		chip->state = FL_READY;
		break;

#if 0
	case FL_WRITING:
		
#endif

	case FL_READY:
		break;

	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
		map_write(map, CMD(0x70), cmd_addr);
		chip->state = FL_STATUS;

	case FL_STATUS:
		status = map_read(map, cmd_addr);
		if (map_word_andequal(map, status, status_OK, status_OK)) {
			map_write(map, CMD(0xff), cmd_addr);
			chip->state = FL_READY;
			break;
		}

		
		if (time_after(jiffies, timeo)) {
			mutex_unlock(&chip->mutex);
			printk(KERN_ERR "waiting for chip to be ready timed out in read. WSM status = %lx\n", status.x[0]);
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		goto retry;

	default:
	sleep:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		timeo = jiffies + HZ;
		goto retry;
	}

	map_copy_from(map, buf, adr, len);

	if (suspended) {
		chip->state = chip->oldstate;
		map_write(map, CMD(0xd0), cmd_addr);
		map_write(map, CMD(0x70), cmd_addr);
	}

	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);
	return 0;
}

static int cfi_staa_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;

	
	chipnum = (from >> cfi->chipshift);
	ofs = from - (chipnum <<  cfi->chipshift);

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> cfi->chipshift)
			thislen = (1<<cfi->chipshift) - ofs;
		else
			thislen = len;

		ret = do_read_onechip(map, &cfi->chips[chipnum], ofs, thislen, buf);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}

static inline int do_write_buffer(struct map_info *map, struct flchip *chip,
				  unsigned long adr, const u_char *buf, int len)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word status, status_OK;
	unsigned long cmd_adr, timeo;
	DECLARE_WAITQUEUE(wait, current);
	int wbufsize, z;

        
        if (adr & (map_bankwidth(map)-1))
            return -EINVAL;

        wbufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;
        adr += chip->start;
	cmd_adr = adr & ~(wbufsize-1);

	
        status_OK = CMD(0x80);

	timeo = jiffies + HZ;
 retry:

#ifdef DEBUG_CFI_FEATURES
       printk("%s: chip->state[%d]\n", __func__, chip->state);
#endif
	mutex_lock(&chip->mutex);

	switch (chip->state) {
	case FL_READY:
		break;

	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
		map_write(map, CMD(0x70), cmd_adr);
                chip->state = FL_STATUS;
#ifdef DEBUG_CFI_FEATURES
	printk("%s: 1 status[%x]\n", __func__, map_read(map, cmd_adr));
#endif

	case FL_STATUS:
		status = map_read(map, cmd_adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;
		
		if (time_after(jiffies, timeo)) {
			mutex_unlock(&chip->mutex);
                        printk(KERN_ERR "waiting for chip to be ready timed out in buffer write Xstatus = %lx, status = %lx\n",
                               status.x[0], map_read(map, cmd_adr).x[0]);
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		goto retry;

	default:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		timeo = jiffies + HZ;
		goto retry;
	}

	ENABLE_VPP(map);
	map_write(map, CMD(0xe8), cmd_adr);
	chip->state = FL_WRITING_TO_BUFFER;

	z = 0;
	for (;;) {
		status = map_read(map, cmd_adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		mutex_lock(&chip->mutex);

		if (++z > 100) {
			
			DISABLE_VPP(map);
                        map_write(map, CMD(0x70), cmd_adr);
			chip->state = FL_STATUS;
			mutex_unlock(&chip->mutex);
			printk(KERN_ERR "Chip not ready for buffer write. Xstatus = %lx\n", status.x[0]);
			return -EIO;
		}
	}

	
	map_write(map, CMD(len/map_bankwidth(map)-1), cmd_adr );

	
	for (z = 0; z < len;
	     z += map_bankwidth(map), buf += map_bankwidth(map)) {
		map_word d;
		d = map_word_load(map, buf);
		map_write(map, d, adr+z);
	}
	
	map_write(map, CMD(0xd0), cmd_adr);
	chip->state = FL_WRITING;

	mutex_unlock(&chip->mutex);
	cfi_udelay(chip->buffer_write_time);
	mutex_lock(&chip->mutex);

	timeo = jiffies + (HZ/2);
	z = 0;
	for (;;) {
		if (chip->state != FL_WRITING) {
			
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			timeo = jiffies + (HZ / 2); 
			mutex_lock(&chip->mutex);
			continue;
		}

		status = map_read(map, cmd_adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
                        
                        map_write(map, CMD(0x50), cmd_adr);
                        
                        map_write(map, CMD(0x70), adr);
			chip->state = FL_STATUS;
			DISABLE_VPP(map);
			mutex_unlock(&chip->mutex);
			printk(KERN_ERR "waiting for chip to be ready timed out in bufwrite\n");
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		z++;
		mutex_lock(&chip->mutex);
	}
	if (!z) {
		chip->buffer_write_time--;
		if (!chip->buffer_write_time)
			chip->buffer_write_time++;
	}
	if (z > 1)
		chip->buffer_write_time++;

	
	DISABLE_VPP(map);
	chip->state = FL_STATUS;

        
        if (map_word_bitsset(map, status, CMD(0x3a))) {
#ifdef DEBUG_CFI_FEATURES
		printk("%s: 2 status[%lx]\n", __func__, status.x[0]);
#endif
		
		map_write(map, CMD(0x50), cmd_adr);
		
		map_write(map, CMD(0x70), adr);
		wake_up(&chip->wq);
		mutex_unlock(&chip->mutex);
		return map_word_bitsset(map, status, CMD(0x02)) ? -EROFS : -EIO;
	}
	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);

        return 0;
}

static int cfi_staa_write_buffers (struct mtd_info *mtd, loff_t to,
				       size_t len, size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int wbufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;
	int ret = 0;
	int chipnum;
	unsigned long ofs;

	chipnum = to >> cfi->chipshift;
	ofs = to  - (chipnum << cfi->chipshift);

#ifdef DEBUG_CFI_FEATURES
	printk("%s: map_bankwidth(map)[%x]\n", __func__, map_bankwidth(map));
	printk("%s: chipnum[%x] wbufsize[%x]\n", __func__, chipnum, wbufsize);
	printk("%s: ofs[%x] len[%x]\n", __func__, ofs, len);
#endif

        
        while (len > 0) {
		
		int size = wbufsize - (ofs & (wbufsize-1));

                if (size > len)
                    size = len;

                ret = do_write_buffer(map, &cfi->chips[chipnum],
				      ofs, buf, size);
		if (ret)
			return ret;

		ofs += size;
		buf += size;
		(*retlen) += size;
		len -= size;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	return 0;
}

#define ECCBUF_SIZE (mtd->writesize)
#define ECCBUF_DIV(x) ((x) & ~(ECCBUF_SIZE - 1))
#define ECCBUF_MOD(x) ((x) &  (ECCBUF_SIZE - 1))
static int
cfi_staa_writev(struct mtd_info *mtd, const struct kvec *vecs,
		unsigned long count, loff_t to, size_t *retlen)
{
	unsigned long i;
	size_t	 totlen = 0, thislen;
	int	 ret = 0;
	size_t	 buflen = 0;
	static char *buffer;

	if (!ECCBUF_SIZE) {
		/* We should fall back to a general writev implementation.
		 * Until that is written, just break.
		 */
		return -EIO;
	}
	buffer = kmalloc(ECCBUF_SIZE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	for (i=0; i<count; i++) {
		size_t elem_len = vecs[i].iov_len;
		void *elem_base = vecs[i].iov_base;
		if (!elem_len) 
			continue;
		if (buflen) { 
			if (buflen + elem_len < ECCBUF_SIZE) { 
				memcpy(buffer+buflen, elem_base, elem_len);
				buflen += elem_len;
				continue;
			}
			memcpy(buffer+buflen, elem_base, ECCBUF_SIZE-buflen);
			ret = mtd_write(mtd, to, ECCBUF_SIZE, &thislen,
					buffer);
			totlen += thislen;
			if (ret || thislen != ECCBUF_SIZE)
				goto write_error;
			elem_len -= thislen-buflen;
			elem_base += thislen-buflen;
			to += ECCBUF_SIZE;
		}
		if (ECCBUF_DIV(elem_len)) { 
			ret = mtd_write(mtd, to, ECCBUF_DIV(elem_len),
					&thislen, elem_base);
			totlen += thislen;
			if (ret || thislen != ECCBUF_DIV(elem_len))
				goto write_error;
			to += thislen;
		}
		buflen = ECCBUF_MOD(elem_len); 
		if (buflen) {
			memset(buffer, 0xff, ECCBUF_SIZE);
			memcpy(buffer, elem_base + thislen, buflen);
		}
	}
	if (buflen) { 
		
		ret = mtd_write(mtd, to, buflen, &thislen, buffer);
		totlen += thislen;
		if (ret || thislen != ECCBUF_SIZE)
			goto write_error;
	}
write_error:
	if (retlen)
		*retlen = totlen;
	kfree(buffer);
	return ret;
}


static inline int do_erase_oneblock(struct map_info *map, struct flchip *chip, unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word status, status_OK;
	unsigned long timeo;
	int retries = 3;
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

	adr += chip->start;

	
	status_OK = CMD(0x80);

	timeo = jiffies + HZ;
retry:
	mutex_lock(&chip->mutex);

	
	switch (chip->state) {
	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
	case FL_READY:
		map_write(map, CMD(0x70), adr);
		chip->state = FL_STATUS;

	case FL_STATUS:
		status = map_read(map, adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
			mutex_unlock(&chip->mutex);
			printk(KERN_ERR "waiting for chip to be ready timed out in erase\n");
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		goto retry;

	default:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		timeo = jiffies + HZ;
		goto retry;
	}

	ENABLE_VPP(map);
	
	map_write(map, CMD(0x50), adr);

	
	map_write(map, CMD(0x20), adr);
	map_write(map, CMD(0xD0), adr);
	chip->state = FL_ERASING;

	mutex_unlock(&chip->mutex);
	msleep(1000);
	mutex_lock(&chip->mutex);

	
	

	timeo = jiffies + (HZ*20);
	for (;;) {
		if (chip->state != FL_ERASING) {
			
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			timeo = jiffies + (HZ*20); 
			mutex_lock(&chip->mutex);
			continue;
		}

		status = map_read(map, adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
			map_write(map, CMD(0x70), adr);
			chip->state = FL_STATUS;
			printk(KERN_ERR "waiting for erase to complete timed out. Xstatus = %lx, status = %lx.\n", status.x[0], map_read(map, adr).x[0]);
			DISABLE_VPP(map);
			mutex_unlock(&chip->mutex);
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		mutex_lock(&chip->mutex);
	}

	DISABLE_VPP(map);
	ret = 0;

	
	map_write(map, CMD(0x70), adr);
	chip->state = FL_STATUS;
	status = map_read(map, adr);

	
	if (map_word_bitsset(map, status, CMD(0x3a))) {
		unsigned char chipstatus = status.x[0];
		if (!map_word_equal(map, status, CMD(chipstatus))) {
			int i, w;
			for (w=0; w<map_words(map); w++) {
				for (i = 0; i<cfi_interleave(cfi); i++) {
					chipstatus |= status.x[w] >> (cfi->device_type * 8);
				}
			}
			printk(KERN_WARNING "Status is not identical for all chips: 0x%lx. Merging to give 0x%02x\n",
			       status.x[0], chipstatus);
		}
		
		map_write(map, CMD(0x50), adr);
		map_write(map, CMD(0x70), adr);

		if ((chipstatus & 0x30) == 0x30) {
			printk(KERN_NOTICE "Chip reports improper command sequence: status 0x%x\n", chipstatus);
			ret = -EIO;
		} else if (chipstatus & 0x02) {
			
			ret = -EROFS;
		} else if (chipstatus & 0x8) {
			
			printk(KERN_WARNING "Chip reports voltage low on erase: status 0x%x\n", chipstatus);
			ret = -EIO;
		} else if (chipstatus & 0x20) {
			if (retries--) {
				printk(KERN_DEBUG "Chip erase failed at 0x%08lx: status 0x%x. Retrying...\n", adr, chipstatus);
				timeo = jiffies + HZ;
				chip->state = FL_STATUS;
				mutex_unlock(&chip->mutex);
				goto retry;
			}
			printk(KERN_DEBUG "Chip erase failed at 0x%08lx: status 0x%x\n", adr, chipstatus);
			ret = -EIO;
		}
	}

	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);
	return ret;
}

static int cfi_staa_erase_varsize(struct mtd_info *mtd,
				  struct erase_info *instr)
{	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long adr, len;
	int chipnum, ret = 0;
	int i, first;
	struct mtd_erase_region_info *regions = mtd->eraseregions;


	i = 0;


	while (i < mtd->numeraseregions && instr->addr >= regions[i].offset)
	       i++;
	i--;


	if (instr->addr & (regions[i].erasesize-1))
		return -EINVAL;

	
	first = i;


	while (i<mtd->numeraseregions && (instr->addr + instr->len) >= regions[i].offset)
		i++;

	i--;

	if ((instr->addr + instr->len) & (regions[i].erasesize-1))
		return -EINVAL;

	chipnum = instr->addr >> cfi->chipshift;
	adr = instr->addr - (chipnum << cfi->chipshift);
	len = instr->len;

	i=first;

	while(len) {
		ret = do_erase_oneblock(map, &cfi->chips[chipnum], adr);

		if (ret)
			return ret;

		adr += regions[i].erasesize;
		len -= regions[i].erasesize;

		if (adr % (1<< cfi->chipshift) == (((unsigned long)regions[i].offset + (regions[i].erasesize * regions[i].numblocks)) %( 1<< cfi->chipshift)))
			i++;

		if (adr >> cfi->chipshift) {
			adr = 0;
			chipnum++;

			if (chipnum >= cfi->numchips)
			break;
		}
	}

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

static void cfi_staa_sync (struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;
	DECLARE_WAITQUEUE(wait, current);

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

	retry:
		mutex_lock(&chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_SYNCING;
		case FL_SYNCING:
			mutex_unlock(&chip->mutex);
			break;

		default:
			
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);

			mutex_unlock(&chip->mutex);
			schedule();
		        remove_wait_queue(&chip->wq, &wait);

			goto retry;
		}
	}

	

	for (i--; i >=0; i--) {
		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		if (chip->state == FL_SYNCING) {
			chip->state = chip->oldstate;
			wake_up(&chip->wq);
		}
		mutex_unlock(&chip->mutex);
	}
}

static inline int do_lock_oneblock(struct map_info *map, struct flchip *chip, unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word status, status_OK;
	unsigned long timeo = jiffies + HZ;
	DECLARE_WAITQUEUE(wait, current);

	adr += chip->start;

	
	status_OK = CMD(0x80);

	timeo = jiffies + HZ;
retry:
	mutex_lock(&chip->mutex);

	
	switch (chip->state) {
	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
	case FL_READY:
		map_write(map, CMD(0x70), adr);
		chip->state = FL_STATUS;

	case FL_STATUS:
		status = map_read(map, adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
			mutex_unlock(&chip->mutex);
			printk(KERN_ERR "waiting for chip to be ready timed out in lock\n");
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		goto retry;

	default:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		timeo = jiffies + HZ;
		goto retry;
	}

	ENABLE_VPP(map);
	map_write(map, CMD(0x60), adr);
	map_write(map, CMD(0x01), adr);
	chip->state = FL_LOCKING;

	mutex_unlock(&chip->mutex);
	msleep(1000);
	mutex_lock(&chip->mutex);

	
	

	timeo = jiffies + (HZ*2);
	for (;;) {

		status = map_read(map, adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
			map_write(map, CMD(0x70), adr);
			chip->state = FL_STATUS;
			printk(KERN_ERR "waiting for lock to complete timed out. Xstatus = %lx, status = %lx.\n", status.x[0], map_read(map, adr).x[0]);
			DISABLE_VPP(map);
			mutex_unlock(&chip->mutex);
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		mutex_lock(&chip->mutex);
	}

	
	chip->state = FL_STATUS;
	DISABLE_VPP(map);
	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);
	return 0;
}
static int cfi_staa_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long adr;
	int chipnum, ret = 0;
#ifdef DEBUG_LOCK_BITS
	int ofs_factor = cfi->interleave * cfi->device_type;
#endif

	if (ofs & (mtd->erasesize - 1))
		return -EINVAL;

	if (len & (mtd->erasesize -1))
		return -EINVAL;

	chipnum = ofs >> cfi->chipshift;
	adr = ofs - (chipnum << cfi->chipshift);

	while(len) {

#ifdef DEBUG_LOCK_BITS
		cfi_send_gen_cmd(0x90, 0x55, 0, map, cfi, cfi->device_type, NULL);
		printk("before lock: block status register is %x\n",cfi_read_query(map, adr+(2*ofs_factor)));
		cfi_send_gen_cmd(0xff, 0x55, 0, map, cfi, cfi->device_type, NULL);
#endif

		ret = do_lock_oneblock(map, &cfi->chips[chipnum], adr);

#ifdef DEBUG_LOCK_BITS
		cfi_send_gen_cmd(0x90, 0x55, 0, map, cfi, cfi->device_type, NULL);
		printk("after lock: block status register is %x\n",cfi_read_query(map, adr+(2*ofs_factor)));
		cfi_send_gen_cmd(0xff, 0x55, 0, map, cfi, cfi->device_type, NULL);
#endif

		if (ret)
			return ret;

		adr += mtd->erasesize;
		len -= mtd->erasesize;

		if (adr >> cfi->chipshift) {
			adr = 0;
			chipnum++;

			if (chipnum >= cfi->numchips)
			break;
		}
	}
	return 0;
}
static inline int do_unlock_oneblock(struct map_info *map, struct flchip *chip, unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	map_word status, status_OK;
	unsigned long timeo = jiffies + HZ;
	DECLARE_WAITQUEUE(wait, current);

	adr += chip->start;

	
	status_OK = CMD(0x80);

	timeo = jiffies + HZ;
retry:
	mutex_lock(&chip->mutex);

	
	switch (chip->state) {
	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
	case FL_READY:
		map_write(map, CMD(0x70), adr);
		chip->state = FL_STATUS;

	case FL_STATUS:
		status = map_read(map, adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
			mutex_unlock(&chip->mutex);
			printk(KERN_ERR "waiting for chip to be ready timed out in unlock\n");
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		goto retry;

	default:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		timeo = jiffies + HZ;
		goto retry;
	}

	ENABLE_VPP(map);
	map_write(map, CMD(0x60), adr);
	map_write(map, CMD(0xD0), adr);
	chip->state = FL_UNLOCKING;

	mutex_unlock(&chip->mutex);
	msleep(1000);
	mutex_lock(&chip->mutex);

	
	

	timeo = jiffies + (HZ*2);
	for (;;) {

		status = map_read(map, adr);
		if (map_word_andequal(map, status, status_OK, status_OK))
			break;

		
		if (time_after(jiffies, timeo)) {
			map_write(map, CMD(0x70), adr);
			chip->state = FL_STATUS;
			printk(KERN_ERR "waiting for unlock to complete timed out. Xstatus = %lx, status = %lx.\n", status.x[0], map_read(map, adr).x[0]);
			DISABLE_VPP(map);
			mutex_unlock(&chip->mutex);
			return -EIO;
		}

		
		mutex_unlock(&chip->mutex);
		cfi_udelay(1);
		mutex_lock(&chip->mutex);
	}

	
	chip->state = FL_STATUS;
	DISABLE_VPP(map);
	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);
	return 0;
}
static int cfi_staa_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long adr;
	int chipnum, ret = 0;
#ifdef DEBUG_LOCK_BITS
	int ofs_factor = cfi->interleave * cfi->device_type;
#endif

	chipnum = ofs >> cfi->chipshift;
	adr = ofs - (chipnum << cfi->chipshift);

#ifdef DEBUG_LOCK_BITS
	{
		unsigned long temp_adr = adr;
		unsigned long temp_len = len;

		cfi_send_gen_cmd(0x90, 0x55, 0, map, cfi, cfi->device_type, NULL);
                while (temp_len) {
			printk("before unlock %x: block status register is %x\n",temp_adr,cfi_read_query(map, temp_adr+(2*ofs_factor)));
			temp_adr += mtd->erasesize;
			temp_len -= mtd->erasesize;
		}
		cfi_send_gen_cmd(0xff, 0x55, 0, map, cfi, cfi->device_type, NULL);
	}
#endif

	ret = do_unlock_oneblock(map, &cfi->chips[chipnum], adr);

#ifdef DEBUG_LOCK_BITS
	cfi_send_gen_cmd(0x90, 0x55, 0, map, cfi, cfi->device_type, NULL);
	printk("after unlock: block status register is %x\n",cfi_read_query(map, adr+(2*ofs_factor)));
	cfi_send_gen_cmd(0xff, 0x55, 0, map, cfi, cfi->device_type, NULL);
#endif

	return ret;
}

static int cfi_staa_suspend(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_PM_SUSPENDED;
		case FL_PM_SUSPENDED:
			break;

		default:
			ret = -EAGAIN;
			break;
		}
		mutex_unlock(&chip->mutex);
	}

	

	if (ret) {
		for (i--; i >=0; i--) {
			chip = &cfi->chips[i];

			mutex_lock(&chip->mutex);

			if (chip->state == FL_PM_SUSPENDED) {
				chip->state = chip->oldstate;
				wake_up(&chip->wq);
			}
			mutex_unlock(&chip->mutex);
		}
	}

	return ret;
}

static void cfi_staa_resume(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;

	for (i=0; i<cfi->numchips; i++) {

		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		
		if (chip->state == FL_PM_SUSPENDED) {
			map_write(map, CMD(0xFF), 0);
			chip->state = FL_READY;
			wake_up(&chip->wq);
		}

		mutex_unlock(&chip->mutex);
	}
}

static void cfi_staa_destroy(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	kfree(cfi->cmdset_priv);
	kfree(cfi);
}

MODULE_LICENSE("GPL");
