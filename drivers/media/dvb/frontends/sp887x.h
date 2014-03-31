
#ifndef SP887X_H
#define SP887X_H

#include <linux/dvb/frontend.h>
#include <linux/firmware.h>

struct sp887x_config
{
	
	u8 demod_address;

	
	int (*request_firmware)(struct dvb_frontend* fe, const struct firmware **fw, char* name);
};

#if defined(CONFIG_DVB_SP887X) || (defined(CONFIG_DVB_SP887X_MODULE) && defined(MODULE))
extern struct dvb_frontend* sp887x_attach(const struct sp887x_config* config,
					  struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* sp887x_attach(const struct sp887x_config* config,
					  struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
