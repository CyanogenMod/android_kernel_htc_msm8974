#include <dvbdev.h>
#include <dmxdev.h>
#include <dvb_demux.h>
#include <dvb_net.h>
#include <dvb_frontend.h>

#ifndef _VIDEOBUF_DVB_H_
#define	_VIDEOBUF_DVB_H_

struct videobuf_dvb {
	
	char                       *name;
	struct dvb_frontend        *frontend;
	struct videobuf_queue      dvbq;

	
	struct mutex               lock;
	struct task_struct         *thread;
	int                        nfeeds;

	
	struct dvb_demux           demux;
	struct dmxdev              dmxdev;
	struct dmx_frontend        fe_hw;
	struct dmx_frontend        fe_mem;
	struct dvb_net             net;
};

struct videobuf_dvb_frontend {
	struct list_head felist;
	int id;
	struct videobuf_dvb dvb;
};

struct videobuf_dvb_frontends {
	struct list_head felist;
	struct mutex lock;
	struct dvb_adapter adapter;
	int active_fe_id; 
	int gate; 
};

int videobuf_dvb_register_bus(struct videobuf_dvb_frontends *f,
			  struct module *module,
			  void *adapter_priv,
			  struct device *device,
			  short *adapter_nr,
			  int mfe_shared,
			  int (*fe_ioctl_override)(struct dvb_frontend *,
					unsigned int, void *, unsigned int));

void videobuf_dvb_unregister_bus(struct videobuf_dvb_frontends *f);

struct videobuf_dvb_frontend * videobuf_dvb_alloc_frontend(struct videobuf_dvb_frontends *f, int id);
void videobuf_dvb_dealloc_frontends(struct videobuf_dvb_frontends *f);

struct videobuf_dvb_frontend * videobuf_dvb_get_frontend(struct videobuf_dvb_frontends *f, int id);
int videobuf_dvb_find_frontend(struct videobuf_dvb_frontends *f, struct dvb_frontend *p);

#endif			

