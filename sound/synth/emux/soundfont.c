/*
 *  Soundfont generic routines.
 *	It is intended that these should be used by any driver that is willing
 *	to accept soundfont patches.
 *
 *  Copyright (C) 1999 Steve Ratcliffe
 *  Copyright (c) 1999-2000 Takashi Iwai <tiwai@suse.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <sound/core.h>
#include <sound/soundfont.h>
#include <sound/seq_oss_legacy.h>


static int open_patch(struct snd_sf_list *sflist, const char __user *data,
		      int count, int client);
static struct snd_soundfont *newsf(struct snd_sf_list *sflist, int type, char *name);
static int is_identical_font(struct snd_soundfont *sf, int type, unsigned char *name);
static int close_patch(struct snd_sf_list *sflist);
static int probe_data(struct snd_sf_list *sflist, int sample_id);
static void set_zone_counter(struct snd_sf_list *sflist,
			     struct snd_soundfont *sf, struct snd_sf_zone *zp);
static struct snd_sf_zone *sf_zone_new(struct snd_sf_list *sflist,
				       struct snd_soundfont *sf);
static void set_sample_counter(struct snd_sf_list *sflist,
			       struct snd_soundfont *sf, struct snd_sf_sample *sp);
static struct snd_sf_sample *sf_sample_new(struct snd_sf_list *sflist,
					   struct snd_soundfont *sf);
static void sf_sample_delete(struct snd_sf_list *sflist,
			     struct snd_soundfont *sf, struct snd_sf_sample *sp);
static int load_map(struct snd_sf_list *sflist, const void __user *data, int count);
static int load_info(struct snd_sf_list *sflist, const void __user *data, long count);
static int remove_info(struct snd_sf_list *sflist, struct snd_soundfont *sf,
		       int bank, int instr);
static void init_voice_info(struct soundfont_voice_info *avp);
static void init_voice_parm(struct soundfont_voice_parm *pp);
static struct snd_sf_sample *set_sample(struct snd_soundfont *sf,
					struct soundfont_voice_info *avp);
static struct snd_sf_sample *find_sample(struct snd_soundfont *sf, int sample_id);
static int load_data(struct snd_sf_list *sflist, const void __user *data, long count);
static void rebuild_presets(struct snd_sf_list *sflist);
static void add_preset(struct snd_sf_list *sflist, struct snd_sf_zone *cur);
static void delete_preset(struct snd_sf_list *sflist, struct snd_sf_zone *zp);
static struct snd_sf_zone *search_first_zone(struct snd_sf_list *sflist,
					     int bank, int preset, int key);
static int search_zones(struct snd_sf_list *sflist, int *notep, int vel,
			int preset, int bank, struct snd_sf_zone **table,
			int max_layers, int level);
static int get_index(int bank, int instr, int key);
static void snd_sf_init(struct snd_sf_list *sflist);
static void snd_sf_clear(struct snd_sf_list *sflist);

static void
lock_preset(struct snd_sf_list *sflist)
{
	unsigned long flags;
	mutex_lock(&sflist->presets_mutex);
	spin_lock_irqsave(&sflist->lock, flags);
	sflist->presets_locked = 1;
	spin_unlock_irqrestore(&sflist->lock, flags);
}


static void
unlock_preset(struct snd_sf_list *sflist)
{
	unsigned long flags;
	spin_lock_irqsave(&sflist->lock, flags);
	sflist->presets_locked = 0;
	spin_unlock_irqrestore(&sflist->lock, flags);
	mutex_unlock(&sflist->presets_mutex);
}


int
snd_soundfont_close_check(struct snd_sf_list *sflist, int client)
{
	unsigned long flags;
	spin_lock_irqsave(&sflist->lock, flags);
	if (sflist->open_client == client)  {
		spin_unlock_irqrestore(&sflist->lock, flags);
		return close_patch(sflist);
	}
	spin_unlock_irqrestore(&sflist->lock, flags);
	return 0;
}


int
snd_soundfont_load(struct snd_sf_list *sflist, const void __user *data,
		   long count, int client)
{
	struct soundfont_patch_info patch;
	unsigned long flags;
	int  rc;

	if (count < (long)sizeof(patch)) {
		snd_printk(KERN_ERR "patch record too small %ld\n", count);
		return -EINVAL;
	}
	if (copy_from_user(&patch, data, sizeof(patch)))
		return -EFAULT;

	count -= sizeof(patch);
	data += sizeof(patch);

	if (patch.key != SNDRV_OSS_SOUNDFONT_PATCH) {
		snd_printk(KERN_ERR "The wrong kind of patch %x\n", patch.key);
		return -EINVAL;
	}
	if (count < patch.len) {
		snd_printk(KERN_ERR "Patch too short %ld, need %d\n",
			   count, patch.len);
		return -EINVAL;
	}
	if (patch.len < 0) {
		snd_printk(KERN_ERR "poor length %d\n", patch.len);
		return -EINVAL;
	}

	if (patch.type == SNDRV_SFNT_OPEN_PATCH) {
		
		lock_preset(sflist);
		rc = open_patch(sflist, data, count, client);
		unlock_preset(sflist);
		return rc;
	}

	
	spin_lock_irqsave(&sflist->lock, flags);
	if (sflist->open_client != client) {
		spin_unlock_irqrestore(&sflist->lock, flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&sflist->lock, flags);

	lock_preset(sflist);
	rc = -EINVAL;
	switch (patch.type) {
	case SNDRV_SFNT_LOAD_INFO:
		rc = load_info(sflist, data, count);
		break;
	case SNDRV_SFNT_LOAD_DATA:
		rc = load_data(sflist, data, count);
		break;
	case SNDRV_SFNT_CLOSE_PATCH:
		rc = close_patch(sflist);
		break;
	case SNDRV_SFNT_REPLACE_DATA:
		
		break;
	case SNDRV_SFNT_MAP_PRESET:
		rc = load_map(sflist, data, count);
		break;
	case SNDRV_SFNT_PROBE_DATA:
		rc = probe_data(sflist, patch.optarg);
		break;
	case SNDRV_SFNT_REMOVE_INFO:
		
		if (!sflist->currsf) {
			snd_printk(KERN_ERR "soundfont: remove_info: "
				   "patch not opened\n");
			rc = -EINVAL;
		} else {
			int bank, instr;
			bank = ((unsigned short)patch.optarg >> 8) & 0xff;
			instr = (unsigned short)patch.optarg & 0xff;
			if (! remove_info(sflist, sflist->currsf, bank, instr))
				rc = -EINVAL;
			else
				rc = 0;
		}
		break;
	}
	unlock_preset(sflist);

	return rc;
}


static inline int
is_special_type(int type)
{
	type &= 0x0f;
	return (type == SNDRV_SFNT_PAT_TYPE_GUS ||
		type == SNDRV_SFNT_PAT_TYPE_MAP);
}


static int
open_patch(struct snd_sf_list *sflist, const char __user *data,
	   int count, int client)
{
	struct soundfont_open_parm parm;
	struct snd_soundfont *sf;
	unsigned long flags;

	spin_lock_irqsave(&sflist->lock, flags);
	if (sflist->open_client >= 0 || sflist->currsf) {
		spin_unlock_irqrestore(&sflist->lock, flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&sflist->lock, flags);

	if (copy_from_user(&parm, data, sizeof(parm)))
		return -EFAULT;

	if (is_special_type(parm.type)) {
		parm.type |= SNDRV_SFNT_PAT_SHARED;
		sf = newsf(sflist, parm.type, NULL);
	} else 
		sf = newsf(sflist, parm.type, parm.name);
	if (sf == NULL) {
		return -ENOMEM;
	}

	spin_lock_irqsave(&sflist->lock, flags);
	sflist->open_client = client;
	sflist->currsf = sf;
	spin_unlock_irqrestore(&sflist->lock, flags);

	return 0;
}

static struct snd_soundfont *
newsf(struct snd_sf_list *sflist, int type, char *name)
{
	struct snd_soundfont *sf;

	
	if (type & SNDRV_SFNT_PAT_SHARED) {
		for (sf = sflist->fonts; sf; sf = sf->next) {
			if (is_identical_font(sf, type, name)) {
				return sf;
			}
		}
	}

	
	sf = kzalloc(sizeof(*sf), GFP_KERNEL);
	if (sf == NULL)
		return NULL;
	sf->id = sflist->fonts_size;
	sflist->fonts_size++;

	
	sf->next = sflist->fonts;
	sflist->fonts = sf;

	sf->type = type;
	sf->zones = NULL;
	sf->samples = NULL;
	if (name)
		memcpy(sf->name, name, SNDRV_SFNT_PATCH_NAME_LEN);

	return sf;
}

static int
is_identical_font(struct snd_soundfont *sf, int type, unsigned char *name)
{
	return ((sf->type & SNDRV_SFNT_PAT_SHARED) &&
		(sf->type & 0x0f) == (type & 0x0f) &&
		(name == NULL ||
		 memcmp(sf->name, name, SNDRV_SFNT_PATCH_NAME_LEN) == 0));
}

static int
close_patch(struct snd_sf_list *sflist)
{
	unsigned long flags;

	spin_lock_irqsave(&sflist->lock, flags);
	sflist->currsf = NULL;
	sflist->open_client = -1;
	spin_unlock_irqrestore(&sflist->lock, flags);

	rebuild_presets(sflist);

	return 0;

}

static int
probe_data(struct snd_sf_list *sflist, int sample_id)
{
	
	if (sflist->currsf) {
		
		if (find_sample(sflist->currsf, sample_id))
			return 0;
	}
	return -EINVAL;
}

static void
set_zone_counter(struct snd_sf_list *sflist, struct snd_soundfont *sf,
		 struct snd_sf_zone *zp)
{
	zp->counter = sflist->zone_counter++;
	if (sf->type & SNDRV_SFNT_PAT_LOCKED)
		sflist->zone_locked = sflist->zone_counter;
}

static struct snd_sf_zone *
sf_zone_new(struct snd_sf_list *sflist, struct snd_soundfont *sf)
{
	struct snd_sf_zone *zp;

	if ((zp = kzalloc(sizeof(*zp), GFP_KERNEL)) == NULL)
		return NULL;
	zp->next = sf->zones;
	sf->zones = zp;

	init_voice_info(&zp->v);

	set_zone_counter(sflist, sf, zp);
	return zp;
}


static void
set_sample_counter(struct snd_sf_list *sflist, struct snd_soundfont *sf,
		   struct snd_sf_sample *sp)
{
	sp->counter = sflist->sample_counter++;
	if (sf->type & SNDRV_SFNT_PAT_LOCKED)
		sflist->sample_locked = sflist->sample_counter;
}

static struct snd_sf_sample *
sf_sample_new(struct snd_sf_list *sflist, struct snd_soundfont *sf)
{
	struct snd_sf_sample *sp;

	if ((sp = kzalloc(sizeof(*sp), GFP_KERNEL)) == NULL)
		return NULL;

	sp->next = sf->samples;
	sf->samples = sp;

	set_sample_counter(sflist, sf, sp);
	return sp;
}

static void
sf_sample_delete(struct snd_sf_list *sflist, struct snd_soundfont *sf,
		 struct snd_sf_sample *sp)
{
	
	if (sp == sf->samples) {
		sf->samples = sp->next;
		kfree(sp);
	}
}


static int
load_map(struct snd_sf_list *sflist, const void __user *data, int count)
{
	struct snd_sf_zone *zp, *prevp;
	struct snd_soundfont *sf;
	struct soundfont_voice_map map;

	
	if (count < (int)sizeof(map))
		return -EINVAL;
	if (copy_from_user(&map, data, sizeof(map)))
		return -EFAULT;

	if (map.map_instr < 0 || map.map_instr >= SF_MAX_INSTRUMENTS)
		return -EINVAL;
	
	sf = newsf(sflist, SNDRV_SFNT_PAT_TYPE_MAP|SNDRV_SFNT_PAT_SHARED, NULL);
	if (sf == NULL)
		return -ENOMEM;

	prevp = NULL;
	for (zp = sf->zones; zp; prevp = zp, zp = zp->next) {
		if (zp->mapped &&
		    zp->instr == map.map_instr &&
		    zp->bank == map.map_bank &&
		    zp->v.low == map.map_key &&
		    zp->v.start == map.src_instr &&
		    zp->v.end == map.src_bank &&
		    zp->v.fixkey == map.src_key) {
			
			
			if (prevp) {
				prevp->next = zp->next;
				zp->next = sf->zones;
				sf->zones = zp;
			}
			
			set_zone_counter(sflist, sf, zp);
			return 0;
		}
	}

	
	if ((zp = sf_zone_new(sflist, sf)) == NULL)
		return -ENOMEM;

	zp->bank = map.map_bank;
	zp->instr = map.map_instr;
	zp->mapped = 1;
	if (map.map_key >= 0) {
		zp->v.low = map.map_key;
		zp->v.high = map.map_key;
	}
	zp->v.start = map.src_instr;
	zp->v.end = map.src_bank;
	zp->v.fixkey = map.src_key;
	zp->v.sf_id = sf->id;

	add_preset(sflist, zp);

	return 0;
}


static int
remove_info(struct snd_sf_list *sflist, struct snd_soundfont *sf,
	    int bank, int instr)
{
	struct snd_sf_zone *prev, *next, *p;
	int removed = 0;

	prev = NULL;
	for (p = sf->zones; p; p = next) {
		next = p->next;
		if (! p->mapped &&
		    p->bank == bank && p->instr == instr) {
			
			if (prev)
				prev->next = next;
			else
				sf->zones = next;
			removed++;
			kfree(p);
		} else
			prev = p;
	}
	if (removed)
		rebuild_presets(sflist);
	return removed;
}


static int
load_info(struct snd_sf_list *sflist, const void __user *data, long count)
{
	struct snd_soundfont *sf;
	struct snd_sf_zone *zone;
	struct soundfont_voice_rec_hdr hdr;
	int i;

	
	if ((sf = sflist->currsf) == NULL)
		return -EINVAL;

	if (is_special_type(sf->type))
		return -EINVAL;

	if (count < (long)sizeof(hdr)) {
		printk(KERN_ERR "Soundfont error: invalid patch zone length\n");
		return -EINVAL;
	}
	if (copy_from_user((char*)&hdr, data, sizeof(hdr)))
		return -EFAULT;
	
	data += sizeof(hdr);
	count -= sizeof(hdr);

	if (hdr.nvoices <= 0 || hdr.nvoices >= 100) {
		printk(KERN_ERR "Soundfont error: Illegal voice number %d\n",
		       hdr.nvoices);
		return -EINVAL;
	}

	if (count < (long)sizeof(struct soundfont_voice_info) * hdr.nvoices) {
		printk(KERN_ERR "Soundfont Error: "
		       "patch length(%ld) is smaller than nvoices(%d)\n",
		       count, hdr.nvoices);
		return -EINVAL;
	}

	switch (hdr.write_mode) {
	case SNDRV_SFNT_WR_EXCLUSIVE:
		for (zone = sf->zones; zone; zone = zone->next) {
			if (!zone->mapped &&
			    zone->bank == hdr.bank &&
			    zone->instr == hdr.instr)
				return -EINVAL;
		}
		break;
	case SNDRV_SFNT_WR_REPLACE:
		
		remove_info(sflist, sf, hdr.bank, hdr.instr);
		break;
	}

	for (i = 0; i < hdr.nvoices; i++) {
		struct snd_sf_zone tmpzone;

		
		if (copy_from_user(&tmpzone.v, data, sizeof(tmpzone.v))) {
			return -EFAULT;
		}

		data += sizeof(tmpzone.v);
		count -= sizeof(tmpzone.v);

		tmpzone.bank = hdr.bank;
		tmpzone.instr = hdr.instr;
		tmpzone.mapped = 0;
		tmpzone.v.sf_id = sf->id;
		if (tmpzone.v.mode & SNDRV_SFNT_MODE_INIT_PARM)
			init_voice_parm(&tmpzone.v.parm);

		
		if ((zone = sf_zone_new(sflist, sf)) == NULL) {
			return -ENOMEM;
		}

		
		zone->bank = tmpzone.bank;
		zone->instr = tmpzone.instr;
		zone->v = tmpzone.v;

		
		zone->sample = set_sample(sf, &zone->v);
	}

	return 0;
}


static void
init_voice_info(struct soundfont_voice_info *avp)
{
	memset(avp, 0, sizeof(*avp));

	avp->root = 60;
	avp->high = 127;
	avp->velhigh = 127;
	avp->fixkey = -1;
	avp->fixvel = -1;
	avp->fixpan = -1;
	avp->pan = -1;
	avp->amplitude = 127;
	avp->scaleTuning = 100;

	init_voice_parm(&avp->parm);
}

static void
init_voice_parm(struct soundfont_voice_parm *pp)
{
	memset(pp, 0, sizeof(*pp));

	pp->moddelay = 0x8000;
	pp->modatkhld = 0x7f7f;
	pp->moddcysus = 0x7f7f;
	pp->modrelease = 0x807f;

	pp->voldelay = 0x8000;
	pp->volatkhld = 0x7f7f;
	pp->voldcysus = 0x7f7f;
	pp->volrelease = 0x807f;

	pp->lfo1delay = 0x8000;
	pp->lfo2delay = 0x8000;

	pp->cutoff = 0xff;
}	

static struct snd_sf_sample *
set_sample(struct snd_soundfont *sf, struct soundfont_voice_info *avp)
{
	struct snd_sf_sample *sample;

	sample = find_sample(sf, avp->sample);
	if (sample == NULL)
		return NULL;

	avp->start += sample->v.start;
	avp->end += sample->v.end;
	avp->loopstart += sample->v.loopstart;
	avp->loopend += sample->v.loopend;

	
	avp->sample_mode = sample->v.mode_flags;

	return sample;
}

static struct snd_sf_sample *
find_sample(struct snd_soundfont *sf, int sample_id)
{
	struct snd_sf_sample *p;

	if (sf == NULL)
		return NULL;

	for (p = sf->samples; p; p = p->next) {
		if (p->v.sample == sample_id)
			return p;
	}
	return NULL;
}


/*
 * Load sample information, this can include data to be loaded onto
 * the soundcard.  It can also just be a pointer into soundcard ROM.
 * If there is data it will be written to the soundcard via the callback
 * routine.
 */
static int
load_data(struct snd_sf_list *sflist, const void __user *data, long count)
{
	struct snd_soundfont *sf;
	struct soundfont_sample_info sample_info;
	struct snd_sf_sample *sp;
	long off;

	
	if ((sf = sflist->currsf) == NULL)
		return -EINVAL;

	if (is_special_type(sf->type))
		return -EINVAL;

	if (copy_from_user(&sample_info, data, sizeof(sample_info)))
		return -EFAULT;

	off = sizeof(sample_info);

	if (sample_info.size != (count-off)/2)
		return -EINVAL;

	
	if (find_sample(sf, sample_info.sample)) {
		
		if (sf->type & SNDRV_SFNT_PAT_SHARED)
			return 0;
		return -EINVAL;
	}

	
	if ((sp = sf_sample_new(sflist, sf)) == NULL)
		return -ENOMEM;

	sp->v = sample_info;
	sp->v.sf_id = sf->id;
	sp->v.dummy = 0;
	sp->v.truesize = sp->v.size;

	if (sp->v.size > 0) {
		int  rc;
		rc = sflist->callback.sample_new
			(sflist->callback.private_data, sp, sflist->memhdr,
			 data + off, count - off);
		if (rc < 0) {
			sf_sample_delete(sflist, sf, sp);
			return rc;
		}
		sflist->mem_used += sp->v.truesize;
	}

	return count;
}


static int log_tbl[129] = {
	0x70000, 0x702df, 0x705b9, 0x7088e, 0x70b5d, 0x70e26, 0x710eb, 0x713aa,
	0x71663, 0x71918, 0x71bc8, 0x71e72, 0x72118, 0x723b9, 0x72655, 0x728ed,
	0x72b80, 0x72e0e, 0x73098, 0x7331d, 0x7359e, 0x7381b, 0x73a93, 0x73d08,
	0x73f78, 0x741e4, 0x7444c, 0x746b0, 0x74910, 0x74b6c, 0x74dc4, 0x75019,
	0x75269, 0x754b6, 0x75700, 0x75946, 0x75b88, 0x75dc7, 0x76002, 0x7623a,
	0x7646e, 0x766a0, 0x768cd, 0x76af8, 0x76d1f, 0x76f43, 0x77164, 0x77382,
	0x7759d, 0x777b4, 0x779c9, 0x77bdb, 0x77dea, 0x77ff5, 0x781fe, 0x78404,
	0x78608, 0x78808, 0x78a06, 0x78c01, 0x78df9, 0x78fef, 0x791e2, 0x793d2,
	0x795c0, 0x797ab, 0x79993, 0x79b79, 0x79d5d, 0x79f3e, 0x7a11d, 0x7a2f9,
	0x7a4d3, 0x7a6ab, 0x7a880, 0x7aa53, 0x7ac24, 0x7adf2, 0x7afbe, 0x7b188,
	0x7b350, 0x7b515, 0x7b6d8, 0x7b899, 0x7ba58, 0x7bc15, 0x7bdd0, 0x7bf89,
	0x7c140, 0x7c2f5, 0x7c4a7, 0x7c658, 0x7c807, 0x7c9b3, 0x7cb5e, 0x7cd07,
	0x7ceae, 0x7d053, 0x7d1f7, 0x7d398, 0x7d538, 0x7d6d6, 0x7d872, 0x7da0c,
	0x7dba4, 0x7dd3b, 0x7ded0, 0x7e063, 0x7e1f4, 0x7e384, 0x7e512, 0x7e69f,
	0x7e829, 0x7e9b3, 0x7eb3a, 0x7ecc0, 0x7ee44, 0x7efc7, 0x7f148, 0x7f2c8,
	0x7f446, 0x7f5c2, 0x7f73d, 0x7f8b7, 0x7fa2f, 0x7fba5, 0x7fd1a, 0x7fe8d,
	0x80000,
};

int
snd_sf_linear_to_log(unsigned int amount, int offset, int ratio)
{
	int v;
	int s, low, bit;
	
	if (amount < 2)
		return 0;
	for (bit = 0; ! (amount & 0x80000000L); bit++)
		amount <<= 1;
	s = (amount >> 24) & 0x7f;
	low = (amount >> 16) & 0xff;
	
	v = (log_tbl[s + 1] * low + log_tbl[s] * (0x100 - low)) >> 8;
	v -= offset;
	v = (v * ratio) >> 16;
	v += (24 - bit) * ratio;
	return v;
}

EXPORT_SYMBOL(snd_sf_linear_to_log);


#define OFFSET_MSEC		653117		
#define OFFSET_ABSCENT		851781		
#define OFFSET_SAMPLERATE	1011119		

#define ABSCENT_RATIO		1200
#define TIMECENT_RATIO		1200
#define SAMPLERATE_RATIO	4096

static int
freq_to_note(int mhz)
{
	return snd_sf_linear_to_log(mhz, OFFSET_ABSCENT, ABSCENT_RATIO);
}

static int
calc_rate_offset(int hz)
{
	return snd_sf_linear_to_log(hz, OFFSET_SAMPLERATE, SAMPLERATE_RATIO);
}


static int
calc_gus_envelope_time(int rate, int start, int end)
{
	int r, p, t;
	r = (3 - ((rate >> 6) & 3)) * 3;
	p = rate & 0x3f;
	t = end - start;
	if (t < 0) t = -t;
	if (13 > r)
		t = t << (13 - r);
	else
		t = t >> (r - 13);
	return (t * 10) / (p * 441);
}


static short attack_time_tbl[128] = {
32767, 32767, 5989, 4235, 2994, 2518, 2117, 1780, 1497, 1373, 1259, 1154, 1058, 970, 890, 816,
707, 691, 662, 634, 607, 581, 557, 533, 510, 489, 468, 448, 429, 411, 393, 377,
361, 345, 331, 317, 303, 290, 278, 266, 255, 244, 234, 224, 214, 205, 196, 188,
180, 172, 165, 158, 151, 145, 139, 133, 127, 122, 117, 112, 107, 102, 98, 94,
90, 86, 82, 79, 75, 72, 69, 66, 63, 61, 58, 56, 53, 51, 49, 47,
45, 43, 41, 39, 37, 36, 34, 33, 31, 30, 29, 28, 26, 25, 24, 23,
22, 21, 20, 19, 19, 18, 17, 16, 16, 15, 15, 14, 13, 13, 12, 12,
11, 11, 10, 10, 10, 9, 9, 8, 8, 8, 8, 7, 7, 7, 6, 0,
};

static short decay_time_tbl[128] = {
32767, 32767, 22614, 15990, 11307, 9508, 7995, 6723, 5653, 5184, 4754, 4359, 3997, 3665, 3361, 3082,
2828, 2765, 2648, 2535, 2428, 2325, 2226, 2132, 2042, 1955, 1872, 1793, 1717, 1644, 1574, 1507,
1443, 1382, 1324, 1267, 1214, 1162, 1113, 1066, 978, 936, 897, 859, 822, 787, 754, 722,
691, 662, 634, 607, 581, 557, 533, 510, 489, 468, 448, 429, 411, 393, 377, 361,
345, 331, 317, 303, 290, 278, 266, 255, 244, 234, 224, 214, 205, 196, 188, 180,
172, 165, 158, 151, 145, 139, 133, 127, 122, 117, 112, 107, 102, 98, 94, 90,
86, 82, 79, 75, 72, 69, 66, 63, 61, 58, 56, 53, 51, 49, 47, 45,
43, 41, 39, 37, 36, 34, 33, 31, 30, 29, 28, 26, 25, 24, 23, 22,
};

int
snd_sf_calc_parm_hold(int msec)
{
	int val = (0x7f * 92 - msec) / 92;
	if (val < 1) val = 1;
	if (val >= 126) val = 126;
	return val;
}

static int
calc_parm_search(int msec, short *table)
{
	int left = 1, right = 127, mid;
	while (left < right) {
		mid = (left + right) / 2;
		if (msec < (int)table[mid])
			left = mid + 1;
		else
			right = mid;
	}
	return left;
}

int
snd_sf_calc_parm_attack(int msec)
{
	return calc_parm_search(msec, attack_time_tbl);
}

int
snd_sf_calc_parm_decay(int msec)
{
	return calc_parm_search(msec, decay_time_tbl);
}

int snd_sf_vol_table[128] = {
	255,111,95,86,79,74,70,66,63,61,58,56,54,52,50,49,
	47,46,45,43,42,41,40,39,38,37,36,35,34,34,33,32,
	31,31,30,29,29,28,27,27,26,26,25,24,24,23,23,22,
	22,21,21,21,20,20,19,19,18,18,18,17,17,16,16,16,
	15,15,15,14,14,14,13,13,13,12,12,12,11,11,11,10,
	10,10,10,9,9,9,8,8,8,8,7,7,7,7,6,6,
	6,6,5,5,5,5,5,4,4,4,4,3,3,3,3,3,
	2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0,
};


#define calc_gus_sustain(val)  (0x7f - snd_sf_vol_table[(val)/2])
#define calc_gus_attenuation(val)	snd_sf_vol_table[(val)/2]

static int
load_guspatch(struct snd_sf_list *sflist, const char __user *data,
	      long count, int client)
{
	struct patch_info patch;
	struct snd_soundfont *sf;
	struct snd_sf_zone *zone;
	struct snd_sf_sample *smp;
	int note, sample_id;
	int rc;

	if (count < (long)sizeof(patch)) {
		snd_printk(KERN_ERR "patch record too small %ld\n", count);
		return -EINVAL;
	}
	if (copy_from_user(&patch, data, sizeof(patch)))
		return -EFAULT;
	
	count -= sizeof(patch);
	data += sizeof(patch);

	sf = newsf(sflist, SNDRV_SFNT_PAT_TYPE_GUS|SNDRV_SFNT_PAT_SHARED, NULL);
	if (sf == NULL)
		return -ENOMEM;
	if ((smp = sf_sample_new(sflist, sf)) == NULL)
		return -ENOMEM;
	sample_id = sflist->sample_counter;
	smp->v.sample = sample_id;
	smp->v.start = 0;
	smp->v.end = patch.len;
	smp->v.loopstart = patch.loop_start;
	smp->v.loopend = patch.loop_end;
	smp->v.size = patch.len;

	
	smp->v.mode_flags = 0;
	if (!(patch.mode & WAVE_16_BITS))
		smp->v.mode_flags |= SNDRV_SFNT_SAMPLE_8BITS;
	if (patch.mode & WAVE_UNSIGNED)
		smp->v.mode_flags |= SNDRV_SFNT_SAMPLE_UNSIGNED;
	smp->v.mode_flags |= SNDRV_SFNT_SAMPLE_NO_BLANK;
	if (!(patch.mode & (WAVE_LOOPING|WAVE_BIDIR_LOOP|WAVE_LOOP_BACK)))
		smp->v.mode_flags |= SNDRV_SFNT_SAMPLE_SINGLESHOT;
	if (patch.mode & WAVE_BIDIR_LOOP)
		smp->v.mode_flags |= SNDRV_SFNT_SAMPLE_BIDIR_LOOP;
	if (patch.mode & WAVE_LOOP_BACK)
		smp->v.mode_flags |= SNDRV_SFNT_SAMPLE_REVERSE_LOOP;

	if (patch.mode & WAVE_16_BITS) {
		
		smp->v.size /= 2;
		smp->v.end /= 2;
		smp->v.loopstart /= 2;
		smp->v.loopend /= 2;
	}
	

	smp->v.dummy = 0;
	smp->v.truesize = 0;
	smp->v.sf_id = sf->id;

	
	if ((zone = sf_zone_new(sflist, sf)) == NULL) {
		sf_sample_delete(sflist, sf, smp);
		return -ENOMEM;
	}

	if (sflist->callback.sample_new) {
		rc = sflist->callback.sample_new
			(sflist->callback.private_data, smp, sflist->memhdr,
			 data, count);
		if (rc < 0) {
			sf_sample_delete(sflist, sf, smp);
			return rc;
		}
		
	}

	
	sflist->mem_used += smp->v.truesize;

	zone->v.sample = sample_id; 
	zone->v.rate_offset = calc_rate_offset(patch.base_freq);
	note = freq_to_note(patch.base_note);
	zone->v.root = note / 100;
	zone->v.tune = -(note % 100);
	zone->v.low = (freq_to_note(patch.low_note) + 99) / 100;
	zone->v.high = freq_to_note(patch.high_note) / 100;
	
	zone->v.pan = (patch.panning + 128) / 2;
#if 0
	snd_printk(KERN_DEBUG
		   "gus: basefrq=%d (ofs=%d) root=%d,tune=%d, range:%d-%d\n",
		   (int)patch.base_freq, zone->v.rate_offset,
		   zone->v.root, zone->v.tune, zone->v.low, zone->v.high);
#endif

	
	
	if (patch.mode & WAVE_ENVELOPES) {
		int attack, hold, decay, release;
		attack = calc_gus_envelope_time
			(patch.env_rate[0], 0, patch.env_offset[0]);
		hold = calc_gus_envelope_time
			(patch.env_rate[1], patch.env_offset[0],
			 patch.env_offset[1]);
		decay = calc_gus_envelope_time
			(patch.env_rate[2], patch.env_offset[1],
			 patch.env_offset[2]);
		release = calc_gus_envelope_time
			(patch.env_rate[3], patch.env_offset[1],
			 patch.env_offset[4]);
		release += calc_gus_envelope_time
			(patch.env_rate[4], patch.env_offset[3],
			 patch.env_offset[4]);
		release += calc_gus_envelope_time
			(patch.env_rate[5], patch.env_offset[4],
			 patch.env_offset[5]);
		zone->v.parm.volatkhld = 
			(snd_sf_calc_parm_hold(hold) << 8) |
			snd_sf_calc_parm_attack(attack);
		zone->v.parm.voldcysus = (calc_gus_sustain(patch.env_offset[2]) << 8) |
			snd_sf_calc_parm_decay(decay);
		zone->v.parm.volrelease = 0x8000 | snd_sf_calc_parm_decay(release);
		zone->v.attenuation = calc_gus_attenuation(patch.env_offset[0]);
#if 0
		snd_printk(KERN_DEBUG
			   "gus: atkhld=%x, dcysus=%x, volrel=%x, att=%d\n",
			   zone->v.parm.volatkhld,
			   zone->v.parm.voldcysus,
			   zone->v.parm.volrelease,
			   zone->v.attenuation);
#endif
	}

	
	if (patch.mode & WAVE_FAST_RELEASE) {
		zone->v.parm.volrelease = 0x807f;
	}

	
	if (patch.mode & WAVE_TREMOLO) {
		int rate = (patch.tremolo_rate * 1000 / 38) / 42;
		zone->v.parm.tremfrq = ((patch.tremolo_depth / 2) << 8) | rate;
	}
	
	if (patch.mode & WAVE_VIBRATO) {
		int rate = (patch.vibrato_rate * 1000 / 38) / 42;
		zone->v.parm.fm2frq2 = ((patch.vibrato_depth / 6) << 8) | rate;
	}
	
	

	if (!(smp->v.mode_flags & SNDRV_SFNT_SAMPLE_SINGLESHOT))
		zone->v.mode = SNDRV_SFNT_MODE_LOOPING;
	else
		zone->v.mode = 0;

	
	
	zone->bank = 0;
	zone->instr = patch.instr_no;
	zone->mapped = 0;
	zone->v.sf_id = sf->id;

	zone->sample = set_sample(sf, &zone->v);

	
	add_preset(sflist, zone);

	return 0;
}

int
snd_soundfont_load_guspatch(struct snd_sf_list *sflist, const char __user *data,
			    long count, int client)
{
	int rc;
	lock_preset(sflist);
	rc = load_guspatch(sflist, data, count, client);
	unlock_preset(sflist);
	return rc;
}


static void
rebuild_presets(struct snd_sf_list *sflist)
{
	struct snd_soundfont *sf;
	struct snd_sf_zone *cur;

	
	memset(sflist->presets, 0, sizeof(sflist->presets));

	
	for (sf = sflist->fonts; sf; sf = sf->next) {
		for (cur = sf->zones; cur; cur = cur->next) {
			if (! cur->mapped && cur->sample == NULL) {
				
				cur->sample = set_sample(sf, &cur->v);
				if (cur->sample == NULL)
					continue;
			}

			add_preset(sflist, cur);
		}
	}
}


static void
add_preset(struct snd_sf_list *sflist, struct snd_sf_zone *cur)
{
	struct snd_sf_zone *zone;
	int index;

	zone = search_first_zone(sflist, cur->bank, cur->instr, cur->v.low);
	if (zone && zone->v.sf_id != cur->v.sf_id) {
		
		struct snd_sf_zone *p;
		
		for (p = zone; p; p = p->next_zone) {
			if (p->counter > cur->counter)
				
				return;
		}
		
		delete_preset(sflist, zone);
		zone = NULL; 
	}

	
	if ((index = get_index(cur->bank, cur->instr, cur->v.low)) < 0)
		return;
	cur->next_zone = zone; 
	cur->next_instr = sflist->presets[index]; 
	sflist->presets[index] = cur;
}

static void
delete_preset(struct snd_sf_list *sflist, struct snd_sf_zone *zp)
{
	int index;
	struct snd_sf_zone *p;

	if ((index = get_index(zp->bank, zp->instr, zp->v.low)) < 0)
		return;
	for (p = sflist->presets[index]; p; p = p->next_instr) {
		while (p->next_instr == zp) {
			p->next_instr = zp->next_instr;
			zp = zp->next_zone;
			if (zp == NULL)
				return;
		}
	}
}


/*
 * Search matching zones from preset table.
 * The note can be rewritten by preset mapping (alias).
 * The found zones are stored on 'table' array.  max_layers defines
 * the maximum number of elements in this array.
 * This function returns the number of found zones.  0 if not found.
 */
int
snd_soundfont_search_zone(struct snd_sf_list *sflist, int *notep, int vel,
			  int preset, int bank,
			  int def_preset, int def_bank,
			  struct snd_sf_zone **table, int max_layers)
{
	int nvoices;
	unsigned long flags;

	spin_lock_irqsave(&sflist->lock, flags);
	if (sflist->presets_locked) {
		spin_unlock_irqrestore(&sflist->lock, flags);
		return 0;
	}
	nvoices = search_zones(sflist, notep, vel, preset, bank,
			       table, max_layers, 0);
	if (! nvoices) {
		if (preset != def_preset || bank != def_bank)
			nvoices = search_zones(sflist, notep, vel,
					       def_preset, def_bank,
					       table, max_layers, 0);
	}
	spin_unlock_irqrestore(&sflist->lock, flags);
	return nvoices;
}


static struct snd_sf_zone *
search_first_zone(struct snd_sf_list *sflist, int bank, int preset, int key)
{
	int index;
	struct snd_sf_zone *zp;

	if ((index = get_index(bank, preset, key)) < 0)
		return NULL;
	for (zp = sflist->presets[index]; zp; zp = zp->next_instr) {
		if (zp->instr == preset && zp->bank == bank)
			return zp;
	}
	return NULL;
}


static int
search_zones(struct snd_sf_list *sflist, int *notep, int vel,
	     int preset, int bank, struct snd_sf_zone **table,
	     int max_layers, int level)
{
	struct snd_sf_zone *zp;
	int nvoices;

	zp = search_first_zone(sflist, bank, preset, *notep);
	nvoices = 0;
	for (; zp; zp = zp->next_zone) {
		if (*notep >= zp->v.low && *notep <= zp->v.high &&
		    vel >= zp->v.vellow && vel <= zp->v.velhigh) {
			if (zp->mapped) {
				
				int key = zp->v.fixkey;
				preset = zp->v.start;
				bank = zp->v.end;

				if (level > 5) 
					return 0;
				if (key < 0)
					key = *notep;
				nvoices = search_zones(sflist, &key, vel,
						       preset, bank, table,
						       max_layers, level + 1);
				if (nvoices > 0)
					*notep = key;
				break;
			}
			table[nvoices++] = zp;
			if (nvoices >= max_layers)
				break;
		}
	}

	return nvoices;
}


static int
get_index(int bank, int instr, int key)
{
	int index;
	if (SF_IS_DRUM_BANK(bank))
		index = key + SF_MAX_INSTRUMENTS;
	else
		index = instr;
	index = index % SF_MAX_PRESETS;
	if (index < 0)
		return -1;
	return index;
}

static void
snd_sf_init(struct snd_sf_list *sflist)
{
	memset(sflist->presets, 0, sizeof(sflist->presets));

	sflist->mem_used = 0;
	sflist->currsf = NULL;
	sflist->open_client = -1;
	sflist->fonts = NULL;
	sflist->fonts_size = 0;
	sflist->zone_counter = 0;
	sflist->sample_counter = 0;
	sflist->zone_locked = 0;
	sflist->sample_locked = 0;
}

static void
snd_sf_clear(struct snd_sf_list *sflist)
{
	struct snd_soundfont *sf, *nextsf;
	struct snd_sf_zone *zp, *nextzp;
	struct snd_sf_sample *sp, *nextsp;

	for (sf = sflist->fonts; sf; sf = nextsf) {
		nextsf = sf->next;
		for (zp = sf->zones; zp; zp = nextzp) {
			nextzp = zp->next;
			kfree(zp);
		}
		for (sp = sf->samples; sp; sp = nextsp) {
			nextsp = sp->next;
			if (sflist->callback.sample_free)
				sflist->callback.sample_free(sflist->callback.private_data,
							     sp, sflist->memhdr);
			kfree(sp);
		}
		kfree(sf);
	}

	snd_sf_init(sflist);
}


struct snd_sf_list *
snd_sf_new(struct snd_sf_callback *callback, struct snd_util_memhdr *hdr)
{
	struct snd_sf_list *sflist;

	if ((sflist = kzalloc(sizeof(*sflist), GFP_KERNEL)) == NULL)
		return NULL;

	mutex_init(&sflist->presets_mutex);
	spin_lock_init(&sflist->lock);
	sflist->memhdr = hdr;

	if (callback)
		sflist->callback = *callback;

	snd_sf_init(sflist);
	return sflist;
}


void
snd_sf_free(struct snd_sf_list *sflist)
{
	if (sflist == NULL)
		return;
	
	lock_preset(sflist);
	if (sflist->callback.sample_reset)
		sflist->callback.sample_reset(sflist->callback.private_data);
	snd_sf_clear(sflist);
	unlock_preset(sflist);

	kfree(sflist);
}

int
snd_soundfont_remove_samples(struct snd_sf_list *sflist)
{
	lock_preset(sflist);
	if (sflist->callback.sample_reset)
		sflist->callback.sample_reset(sflist->callback.private_data);
	snd_sf_clear(sflist);
	unlock_preset(sflist);

	return 0;
}

int
snd_soundfont_remove_unlocked(struct snd_sf_list *sflist)
{
	struct snd_soundfont *sf;
	struct snd_sf_zone *zp, *nextzp;
	struct snd_sf_sample *sp, *nextsp;

	lock_preset(sflist);

	if (sflist->callback.sample_reset)
		sflist->callback.sample_reset(sflist->callback.private_data);

	
	memset(sflist->presets, 0, sizeof(sflist->presets));

	for (sf = sflist->fonts; sf; sf = sf->next) {
		for (zp = sf->zones; zp; zp = nextzp) {
			if (zp->counter < sflist->zone_locked)
				break;
			nextzp = zp->next;
			sf->zones = nextzp;
			kfree(zp);
		}

		for (sp = sf->samples; sp; sp = nextsp) {
			if (sp->counter < sflist->sample_locked)
				break;
			nextsp = sp->next;
			sf->samples = nextsp;
			sflist->mem_used -= sp->v.truesize;
			if (sflist->callback.sample_free)
				sflist->callback.sample_free(sflist->callback.private_data,
							     sp, sflist->memhdr);
			kfree(sp);
		}
	}

	sflist->zone_counter = sflist->zone_locked;
	sflist->sample_counter = sflist->sample_locked;

	rebuild_presets(sflist);

	unlock_preset(sflist);
	return 0;
}
