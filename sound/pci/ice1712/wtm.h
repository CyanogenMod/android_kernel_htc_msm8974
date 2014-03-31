#ifndef __SOUND_WTM_H
#define __SOUND_WTM_H

#define WTM_DEVICE_DESC		"{EGO SYS INC,WaveTerminal 192M},"
#define VT1724_SUBDEVICE_WTM	0x36495345	


#define	AK4114_ADDR		0x20	
#define STAC9460_I2C_ADDR	0x54	
#define STAC9460_2_I2C_ADDR	0x56	


extern struct snd_ice1712_card_info snd_vt1724_wtm_cards[];

#endif 

