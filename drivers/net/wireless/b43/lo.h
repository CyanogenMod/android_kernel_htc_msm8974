#ifndef B43_LO_H_
#define B43_LO_H_


#include "phy_g.h"

struct b43_wldev;

struct b43_loctl {
	
	s8 i;
	s8 q;
};
#define B43_LOCTL_POISON	111

struct b43_lo_calib {
	struct b43_bbatt bbatt;
	struct b43_rfatt rfatt;
	
	struct b43_loctl ctl;
	
	unsigned long calib_time;
	
	struct list_head list;
};

#define B43_DC_LT_SIZE		32

struct b43_txpower_lo_control {
	struct b43_rfatt_list rfatt_list;
	struct b43_bbatt_list bbatt_list;

	u16 dc_lt[B43_DC_LT_SIZE];

	
	struct list_head calib_list;
	
	unsigned long pwr_vec_read_time;
	
	unsigned long txctl_measured_time;

	
	u8 tx_bias;
	
	u8 tx_magn;

	
	u64 power_vector;
};

#define B43_LO_CALIB_EXPIRE	(HZ * (30 - 2))
#define B43_LO_PWRVEC_EXPIRE	(HZ * (30 - 2))
#define B43_LO_TXCTL_EXPIRE	(HZ * (180 - 4))


void b43_lo_g_adjust(struct b43_wldev *dev);
void b43_lo_g_adjust_to(struct b43_wldev *dev,
			u16 rfatt, u16 bbatt, u16 tx_control);

void b43_gphy_dc_lt_init(struct b43_wldev *dev, bool update_all);

void b43_lo_g_maintanance_work(struct b43_wldev *dev);
void b43_lo_g_cleanup(struct b43_wldev *dev);
void b43_lo_g_init(struct b43_wldev *dev);

#endif 
