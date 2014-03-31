
#ifndef _MPR121_TOUCHKEY_H
#define _MPR121_TOUCHKEY_H

struct mpr121_platform_data {
	const unsigned short *keymap;
	unsigned int keymap_size;
	bool wakeup;
	int vdd_uv;
};

#endif 
