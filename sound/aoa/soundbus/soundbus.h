/*
 * soundbus generic definitions
 *
 * Copyright 2006 Johannes Berg <johannes@sipsolutions.net>
 *
 * GPL v2, can be found in COPYING.
 */
#ifndef __SOUNDBUS_H
#define __SOUNDBUS_H

#include <linux/of_device.h>
#include <sound/pcm.h>
#include <linux/list.h>


enum clock_switch {
	CLOCK_SWITCH_PREPARE_SLAVE,
	CLOCK_SWITCH_PREPARE_MASTER,
	CLOCK_SWITCH_SLAVE,
	CLOCK_SWITCH_MASTER,
	CLOCK_SWITCH_NOTIFY,
};

struct transfer_info {
	u64 formats;		
	unsigned int rates;	
	
	u32 transfer_in:1, 
	    must_be_clock_source:1;
	
	int tag;
};

struct codec_info_item {
	struct codec_info *codec;
	void *codec_data;
	struct soundbus_dev *sdev;
	
	struct list_head list;
};

struct bus_info {
	
	int sysclock_factor;
	int bus_factor;
};

struct codec_info {
	
	struct module *owner;

	struct transfer_info *transfers;

	int sysclock_factor;

	int bus_factor;

	
	
	int (*switch_clock)(struct codec_info_item *cii,
			    enum clock_switch clock);

	int (*usable)(struct codec_info_item *cii,
		      struct transfer_info *ti,
		      struct transfer_info *out);

	int (*open)(struct codec_info_item *cii,
		    struct snd_pcm_substream *substream);

	int (*close)(struct codec_info_item *cii,
		     struct snd_pcm_substream *substream);

	int (*prepare)(struct codec_info_item *cii,
		       struct bus_info *bi,
		       struct snd_pcm_substream *substream);

	int (*start)(struct codec_info_item *cii,
		     struct snd_pcm_substream *substream);

	int (*stop)(struct codec_info_item *cii,
		    struct snd_pcm_substream *substream);

	int (*suspend)(struct codec_info_item *cii, pm_message_t state);
	int (*resume)(struct codec_info_item *cii);
};

struct soundbus_dev {
	
	struct list_head onbuslist;

	
	struct platform_device ofdev;

	
	char modalias[32];

	char *pcmname;
	int pcmid;

	
	struct snd_pcm *pcm;

	
	int (*attach_codec)(struct soundbus_dev *dev, struct snd_card *card,
			    struct codec_info *ci, void *data);
	void (*detach_codec)(struct soundbus_dev *dev, void *data);
	

	
	struct list_head codec_list;
	u32 have_out:1, have_in:1;
};
#define to_soundbus_device(d) container_of(d, struct soundbus_dev, ofdev.dev)
#define of_to_soundbus_device(d) container_of(d, struct soundbus_dev, ofdev)

extern int soundbus_add_one(struct soundbus_dev *dev);
extern void soundbus_remove_one(struct soundbus_dev *dev);

extern struct soundbus_dev *soundbus_dev_get(struct soundbus_dev *dev);
extern void soundbus_dev_put(struct soundbus_dev *dev);

struct soundbus_driver {
	char *name;
	struct module *owner;

	

	int	(*probe)(struct soundbus_dev* dev);
	int	(*remove)(struct soundbus_dev* dev);

	int	(*suspend)(struct soundbus_dev* dev, pm_message_t state);
	int	(*resume)(struct soundbus_dev* dev);
	int	(*shutdown)(struct soundbus_dev* dev);

	struct device_driver driver;
};
#define to_soundbus_driver(drv) container_of(drv,struct soundbus_driver, driver)

extern int soundbus_register_driver(struct soundbus_driver *drv);
extern void soundbus_unregister_driver(struct soundbus_driver *drv);

extern struct device_attribute soundbus_dev_attrs[];

#endif 
