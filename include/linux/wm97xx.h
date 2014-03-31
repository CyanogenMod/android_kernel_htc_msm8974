

#ifndef _LINUX_WM97XX_H
#define _LINUX_WM97XX_H

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/ac97_codec.h>
#include <sound/initval.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/input.h>	
#include <linux/platform_device.h>

#define	WM97xx_GENERIC			0x0000
#define	WM97xx_WM1613			0x1613

#define AC97_WM97XX_DIGITISER1		0x76
#define AC97_WM97XX_DIGITISER2		0x78
#define AC97_WM97XX_DIGITISER_RD 	0x7a
#define AC97_WM9713_DIG1		0x74
#define AC97_WM9713_DIG2		AC97_WM97XX_DIGITISER1
#define AC97_WM9713_DIG3		AC97_WM97XX_DIGITISER2

#define WM97XX_POLL		0x8000	
#define WM97XX_ADCSEL_X		0x1000	
#define WM97XX_ADCSEL_Y		0x2000	
#define WM97XX_ADCSEL_PRES	0x3000	
#define WM97XX_AUX_ID1		0x4000
#define WM97XX_AUX_ID2		0x5000
#define WM97XX_AUX_ID3		0x6000
#define WM97XX_AUX_ID4		0x7000
#define WM97XX_ADCSEL_MASK	0x7000	
#define WM97XX_COO		0x0800	
#define WM97XX_CTC		0x0400	
#define WM97XX_CM_RATE_93	0x0000	
#define WM97XX_CM_RATE_187	0x0100	
#define WM97XX_CM_RATE_375	0x0200	
#define WM97XX_CM_RATE_750	0x0300	
#define WM97XX_CM_RATE_8K	0x00f0	
#define WM97XX_CM_RATE_12K	0x01f0	
#define WM97XX_CM_RATE_24K	0x02f0	
#define WM97XX_CM_RATE_48K	0x03f0	
#define WM97XX_CM_RATE_MASK	0x03f0
#define WM97XX_RATE(i)		(((i & 3) << 8) | ((i & 4) ? 0xf0 : 0))
#define WM97XX_DELAY(i)		((i << 4) & 0x00f0)	
#define WM97XX_DELAY_MASK	0x00f0
#define WM97XX_SLEN		0x0008	
#define WM97XX_SLT(i)		((i - 5) & 0x7)	
#define WM97XX_SLT_MASK		0x0007
#define WM97XX_PRP_DETW		0x4000	
#define WM97XX_PRP_DET		0x8000	
#define WM97XX_PRP_DET_DIG	0xc000	
#define WM97XX_RPR		0x2000	
#define WM97XX_PEN_DOWN		0x8000	

#define WM9712_45W		0x1000	
#define WM9712_PDEN		0x0800	
#define WM9712_WAIT		0x0200	
#define WM9712_PIL		0x0100	
#define WM9712_MASK_HI		0x0040	
#define WM9712_MASK_EDGE	0x0080	
#define	WM9712_MASK_SYNC	0x00c0	
#define WM9712_RPU(i)		(i&0x3f)	
#define WM9712_PD(i)		(0x1 << i)	

#define AC97_WM9712_POWER	0x24
#define AC97_WM9712_REV		0x58

#define WM9705_PDEN		0x1000	
#define WM9705_PINV		0x0800	
#define WM9705_BSEN		0x0400	
#define WM9705_BINV		0x0200	
#define WM9705_WAIT		0x0100	
#define WM9705_PIL		0x0080	
#define WM9705_PHIZ		0x0040	
#define WM9705_MASK_HI		0x0010	
#define WM9705_MASK_EDGE	0x0020	
#define	WM9705_MASK_SYNC	0x0030	
#define WM9705_PDD(i)		(i & 0x000f)	


#define WM9713_PDPOL		0x0400	
#define WM9713_POLL		0x0200	
#define WM9713_CTC		0x0100	
#define WM9713_ADCSEL_X		0x0002	
#define WM9713_ADCSEL_Y		0x0004	
#define WM9713_ADCSEL_PRES	0x0008	
#define WM9713_COO		0x0001	
#define WM9713_45W		0x1000  
#define WM9713_PDEN		0x0800	
#define WM9713_ADCSEL_MASK	0x00fe	
#define WM9713_WAIT		0x0200	

#define TS_COMP1		0x0
#define TS_COMP2		0x1
#define TS_BMON			0x2
#define TS_WIPER		0x3

#define WM97XX_ID1		0x574d
#define WM9712_ID2		0x4c12
#define WM9705_ID2		0x4c05
#define WM9713_ID2		0x4c13

#define WM97XX_MAX_GPIO		16
#define WM97XX_GPIO_1		(1 << 1)
#define WM97XX_GPIO_2		(1 << 2)
#define WM97XX_GPIO_3		(1 << 3)
#define WM97XX_GPIO_4		(1 << 4)
#define WM97XX_GPIO_5		(1 << 5)
#define WM97XX_GPIO_6		(1 << 6)
#define WM97XX_GPIO_7		(1 << 7)
#define WM97XX_GPIO_8		(1 << 8)
#define WM97XX_GPIO_9		(1 << 9)
#define WM97XX_GPIO_10		(1 << 10)
#define WM97XX_GPIO_11		(1 << 11)
#define WM97XX_GPIO_12		(1 << 12)
#define WM97XX_GPIO_13		(1 << 13)
#define WM97XX_GPIO_14		(1 << 14)
#define WM97XX_GPIO_15		(1 << 15)


#define AC97_LINK_FRAME		21	



#define RC_AGAIN			0x00000001
#define RC_VALID			0x00000002
#define RC_PENUP			0x00000004
#define RC_PENDOWN			0x00000008


struct wm97xx_data {
    int x;
    int y;
    int p;
};

enum wm97xx_gpio_status {
    WM97XX_GPIO_HIGH,
    WM97XX_GPIO_LOW
};

enum wm97xx_gpio_dir {
    WM97XX_GPIO_IN,
    WM97XX_GPIO_OUT
};

enum wm97xx_gpio_pol {
    WM97XX_GPIO_POL_HIGH,
    WM97XX_GPIO_POL_LOW
};

enum wm97xx_gpio_sticky {
    WM97XX_GPIO_STICKY,
    WM97XX_GPIO_NOTSTICKY
};

enum wm97xx_gpio_wake {
    WM97XX_GPIO_WAKE,
    WM97XX_GPIO_NOWAKE
};

#define WM97XX_DIG_START	0x1
#define WM97XX_DIG_STOP		0x2
#define WM97XX_PHY_INIT		0x3
#define WM97XX_AUX_PREPARE	0x4
#define WM97XX_DIG_RESTORE	0x5

struct wm97xx;

extern struct wm97xx_codec_drv wm9705_codec;
extern struct wm97xx_codec_drv wm9712_codec;
extern struct wm97xx_codec_drv wm9713_codec;

struct wm97xx_codec_drv {
	u16 id;
	char *name;

	
	int (*poll_sample) (struct wm97xx *, int adcsel, int *sample);

	
	int (*poll_touch) (struct wm97xx *, struct wm97xx_data *);

	int (*acc_enable) (struct wm97xx *, int enable);
	void (*phy_init) (struct wm97xx *);
	void (*dig_enable) (struct wm97xx *, int enable);
	void (*dig_restore) (struct wm97xx *);
	void (*aux_prepare) (struct wm97xx *);
};


struct wm97xx_mach_ops {

	
	int acc_enabled;
	void (*acc_pen_up) (struct wm97xx *);
	int (*acc_pen_down) (struct wm97xx *);
	int (*acc_startup) (struct wm97xx *);
	void (*acc_shutdown) (struct wm97xx *);

	
	void (*irq_enable) (struct wm97xx *, int enable);

	
	int irq_gpio;

	
	void (*pre_sample) (int);  
	void (*post_sample) (int);  
};

struct wm97xx {
	u16 dig[3], id, gpio[6], misc;	
	u16 dig_save[3];		
	struct wm97xx_codec_drv *codec;	
	struct input_dev *input_dev;	
	struct snd_ac97 *ac97;		
	struct device *dev;		
	struct platform_device *battery_dev;
	struct platform_device *touch_dev;
	struct wm97xx_mach_ops *mach_ops;
	struct mutex codec_mutex;
	struct delayed_work ts_reader;  
	unsigned long ts_reader_interval; 
	unsigned long ts_reader_min_interval; 
	unsigned int pen_irq;		
	struct workqueue_struct *ts_workq;
	struct work_struct pen_event_work;
	u16 acc_slot;			
	u16 acc_rate;			
	unsigned pen_is_down:1;		
	unsigned aux_waiting:1;		
	unsigned pen_probably_down:1;	
	u16 variant;			
	u16 suspend_mode;               
};

struct wm97xx_batt_pdata {
	int	batt_aux;
	int	temp_aux;
	int	charge_gpio;
	int	min_voltage;
	int	max_voltage;
	int	batt_div;
	int	batt_mult;
	int	temp_div;
	int	temp_mult;
	int	batt_tech;
	char	*batt_name;
};

struct wm97xx_pdata {
	struct wm97xx_batt_pdata	*batt_pdata;	
};

enum wm97xx_gpio_status wm97xx_get_gpio(struct wm97xx *wm, u32 gpio);
void wm97xx_set_gpio(struct wm97xx *wm, u32 gpio,
			  enum wm97xx_gpio_status status);
void wm97xx_config_gpio(struct wm97xx *wm, u32 gpio,
				     enum wm97xx_gpio_dir dir,
				     enum wm97xx_gpio_pol pol,
				     enum wm97xx_gpio_sticky sticky,
				     enum wm97xx_gpio_wake wake);

void wm97xx_set_suspend_mode(struct wm97xx *wm, u16 mode);

int wm97xx_reg_read(struct wm97xx *wm, u16 reg);
void wm97xx_reg_write(struct wm97xx *wm, u16 reg, u16 val);

int wm97xx_read_aux_adc(struct wm97xx *wm, u16 adcsel);

int wm97xx_register_mach_ops(struct wm97xx *, struct wm97xx_mach_ops *);
void wm97xx_unregister_mach_ops(struct wm97xx *);

#endif
