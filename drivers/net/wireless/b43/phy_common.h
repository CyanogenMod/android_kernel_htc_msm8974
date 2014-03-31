#ifndef LINUX_B43_PHY_COMMON_H_
#define LINUX_B43_PHY_COMMON_H_

#include <linux/types.h>
#include <linux/nl80211.h>

struct b43_wldev;

struct b43_c32 { s32 i, q; };

#define CORDIC_CONVERT(value)	(((value) >= 0) ? \
				 ((((value) >> 15) + 1) >> 1) : \
				 -((((-(value)) >> 15) + 1) >> 1))

#define B43_PHYROUTE			0x0C00 
#define  B43_PHYROUTE_BASE		0x0000 
#define  B43_PHYROUTE_OFDM_GPHY		0x0400 
#define  B43_PHYROUTE_EXT_GPHY		0x0800 
#define  B43_PHYROUTE_N_BMODE		0x0C00 

#define B43_PHY_CCK(reg)		((reg) | B43_PHYROUTE_BASE)
#define B43_PHY_N(reg)			((reg) | B43_PHYROUTE_BASE)
#define B43_PHY_N_BMODE(reg)		((reg) | B43_PHYROUTE_N_BMODE)
#define B43_PHY_OFDM(reg)		((reg) | B43_PHYROUTE_OFDM_GPHY)
#define B43_PHY_EXTG(reg)		((reg) | B43_PHYROUTE_EXT_GPHY)


#define B43_PHYVER_ANALOG		0xF000
#define B43_PHYVER_ANALOG_SHIFT		12
#define B43_PHYVER_TYPE			0x0F00
#define B43_PHYVER_TYPE_SHIFT		8
#define B43_PHYVER_VERSION		0x00FF

#define B43_MAX_WRITES_IN_ROW		24

enum b43_interference_mitigation {
	B43_INTERFMODE_NONE,
	B43_INTERFMODE_NONWLAN,
	B43_INTERFMODE_MANUALWLAN,
	B43_INTERFMODE_AUTOWLAN,
};

enum {
	B43_ANTENNA0 = 0,	
	B43_ANTENNA1 = 1,	
	B43_ANTENNA_AUTO0 = 2,	
	B43_ANTENNA_AUTO1 = 3,	
	B43_ANTENNA2 = 4,
	B43_ANTENNA3 = 8,

	B43_ANTENNA_AUTO = B43_ANTENNA_AUTO0,
	B43_ANTENNA_DEFAULT = B43_ANTENNA_AUTO,
};

enum b43_txpwr_result {
	B43_TXPWR_RES_NEED_ADJUST,
	B43_TXPWR_RES_DONE,
};

struct b43_phy_operations {
	
	int (*allocate)(struct b43_wldev *dev);
	void (*free)(struct b43_wldev *dev);
	void (*prepare_structs)(struct b43_wldev *dev);
	int (*prepare_hardware)(struct b43_wldev *dev);
	int (*init)(struct b43_wldev *dev);
	void (*exit)(struct b43_wldev *dev);

	
	u16 (*phy_read)(struct b43_wldev *dev, u16 reg);
	void (*phy_write)(struct b43_wldev *dev, u16 reg, u16 value);
	void (*phy_maskset)(struct b43_wldev *dev, u16 reg, u16 mask, u16 set);
	u16 (*radio_read)(struct b43_wldev *dev, u16 reg);
	void (*radio_write)(struct b43_wldev *dev, u16 reg, u16 value);

	
	bool (*supports_hwpctl)(struct b43_wldev *dev);
	void (*software_rfkill)(struct b43_wldev *dev, bool blocked);
	void (*switch_analog)(struct b43_wldev *dev, bool on);
	int (*switch_channel)(struct b43_wldev *dev, unsigned int new_channel);
	unsigned int (*get_default_chan)(struct b43_wldev *dev);
	void (*set_rx_antenna)(struct b43_wldev *dev, int antenna);
	int (*interf_mitigation)(struct b43_wldev *dev,
				 enum b43_interference_mitigation new_mode);

	
	enum b43_txpwr_result (*recalc_txpower)(struct b43_wldev *dev,
						bool ignore_tssi);
	void (*adjust_txpower)(struct b43_wldev *dev);

	
	void (*pwork_15sec)(struct b43_wldev *dev);
	void (*pwork_60sec)(struct b43_wldev *dev);
};

struct b43_phy_a;
struct b43_phy_g;
struct b43_phy_n;
struct b43_phy_lp;
struct b43_phy_ht;
struct b43_phy_lcn;

struct b43_phy {
	
	const struct b43_phy_operations *ops;

#ifdef CONFIG_B43_DEBUG
	
	struct {
#else
	union {
#endif
		
		struct b43_phy_a *a;
		
		struct b43_phy_g *g;
		
		struct b43_phy_n *n;
		
		struct b43_phy_lp *lp;
		
		struct b43_phy_ht *ht;
		
		struct b43_phy_lcn *lcn;
	};

	
	bool supports_2ghz;
	bool supports_5ghz;

	
	bool is_40mhz;

	
	bool gmode;

	
	u8 analog;
	
	u8 type;
	
	u8 rev;

	
	u8 writes_counter;

	
	u16 radio_manuf;	
	u16 radio_ver;		
	u8 radio_rev;		

	
	bool radio_on;

	int desired_txpower;

	
	bool hardware_power_control;

	unsigned long next_txpwr_check_time;

	
	unsigned int channel;
	u16 channel_freq;
	enum nl80211_channel_type channel_type;

	
	atomic_t txerr_cnt;

#ifdef CONFIG_B43_DEBUG
	
	bool phy_locked;
	
	bool radio_locked;
#endif 
};


int b43_phy_allocate(struct b43_wldev *dev);

void b43_phy_free(struct b43_wldev *dev);

int b43_phy_init(struct b43_wldev *dev);

void b43_phy_exit(struct b43_wldev *dev);

bool b43_has_hardware_pctl(struct b43_wldev *dev);

u16 b43_phy_read(struct b43_wldev *dev, u16 reg);

void b43_phy_write(struct b43_wldev *dev, u16 reg, u16 value);

void b43_phy_copy(struct b43_wldev *dev, u16 destreg, u16 srcreg);

void b43_phy_mask(struct b43_wldev *dev, u16 offset, u16 mask);

void b43_phy_set(struct b43_wldev *dev, u16 offset, u16 set);

void b43_phy_maskset(struct b43_wldev *dev, u16 offset, u16 mask, u16 set);

u16 b43_radio_read(struct b43_wldev *dev, u16 reg);
#define b43_radio_read16	b43_radio_read 

void b43_radio_write(struct b43_wldev *dev, u16 reg, u16 value);
#define b43_radio_write16	b43_radio_write 

void b43_radio_mask(struct b43_wldev *dev, u16 offset, u16 mask);

void b43_radio_set(struct b43_wldev *dev, u16 offset, u16 set);

void b43_radio_maskset(struct b43_wldev *dev, u16 offset, u16 mask, u16 set);

void b43_radio_lock(struct b43_wldev *dev);

void b43_radio_unlock(struct b43_wldev *dev);

void b43_phy_lock(struct b43_wldev *dev);

void b43_phy_unlock(struct b43_wldev *dev);

int b43_switch_channel(struct b43_wldev *dev, unsigned int new_channel);
#define B43_DEFAULT_CHANNEL	UINT_MAX

void b43_software_rfkill(struct b43_wldev *dev, bool blocked);

void b43_phy_txpower_check(struct b43_wldev *dev, unsigned int flags);
enum b43_phy_txpower_check_flags {
	B43_TXPWR_IGNORE_TIME		= (1 << 0),
	B43_TXPWR_IGNORE_TSSI		= (1 << 1),
};

struct work_struct;
void b43_phy_txpower_adjust_work(struct work_struct *work);

int b43_phy_shm_tssi_read(struct b43_wldev *dev, u16 shm_offset);

void b43_phyop_switch_analog_generic(struct b43_wldev *dev, bool on);

bool b43_channel_type_is_40mhz(enum nl80211_channel_type channel_type);

void b43_phy_force_clock(struct b43_wldev *dev, bool force);

struct b43_c32 b43_cordic(int theta);

#endif 
