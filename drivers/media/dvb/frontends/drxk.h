#ifndef _DRXK_H_
#define _DRXK_H_

#include <linux/types.h>
#include <linux/i2c.h>

struct drxk_config {
	u8	adr;
	bool	single_master;
	bool	no_i2c_bridge;
	bool	parallel_ts;
	bool	dynamic_clk;
	bool	enable_merr_cfg;

	bool	antenna_dvbt;
	u16	antenna_gpio;

	u8	mpeg_out_clk_strength;
	int	chunk_size;

	const char *microcode_name;
};

#if defined(CONFIG_DVB_DRXK) || (defined(CONFIG_DVB_DRXK_MODULE) \
        && defined(MODULE))
extern struct dvb_frontend *drxk_attach(const struct drxk_config *config,
					struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *drxk_attach(const struct drxk_config *config,
					struct i2c_adapter *i2c)
{
        printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
        return NULL;
}
#endif

#endif
