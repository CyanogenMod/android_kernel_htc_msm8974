#ifndef LINUX_B43_PHY_G_H_
#define LINUX_B43_PHY_G_H_

#include "phy_a.h"

#define B43_PHY_VERSION_CCK		B43_PHY_CCK(0x00)	
#define B43_PHY_CCKBBANDCFG		B43_PHY_CCK(0x01)	
#define B43_PHY_PGACTL			B43_PHY_CCK(0x15)	
#define  B43_PHY_PGACTL_LPF		0x1000	
#define  B43_PHY_PGACTL_LOWBANDW	0x0040	
#define  B43_PHY_PGACTL_UNKNOWN		0xEFA0
#define B43_PHY_FBCTL1			B43_PHY_CCK(0x18)	
#define B43_PHY_ITSSI			B43_PHY_CCK(0x29)	
#define B43_PHY_LO_LEAKAGE		B43_PHY_CCK(0x2D)	
#define B43_PHY_ENERGY			B43_PHY_CCK(0x33)	
#define B43_PHY_SYNCCTL			B43_PHY_CCK(0x35)
#define B43_PHY_FBCTL2			B43_PHY_CCK(0x38)	
#define B43_PHY_DACCTL			B43_PHY_CCK(0x60)	
#define B43_PHY_RCCALOVER		B43_PHY_CCK(0x78)	

#define B43_PHY_CLASSCTL		B43_PHY_EXTG(0x02)	
#define B43_PHY_GTABCTL			B43_PHY_EXTG(0x03)	
#define  B43_PHY_GTABOFF		0x03FF	
#define  B43_PHY_GTABNR			0xFC00	
#define  B43_PHY_GTABNR_SHIFT		10
#define B43_PHY_GTABDATA		B43_PHY_EXTG(0x04)	
#define B43_PHY_LO_MASK			B43_PHY_EXTG(0x0F)	
#define B43_PHY_LO_CTL			B43_PHY_EXTG(0x10)	
#define B43_PHY_RFOVER			B43_PHY_EXTG(0x11)	
#define B43_PHY_RFOVERVAL		B43_PHY_EXTG(0x12)	
#define  B43_PHY_RFOVERVAL_EXTLNA	0x8000
#define  B43_PHY_RFOVERVAL_LNA		0x7000
#define  B43_PHY_RFOVERVAL_LNA_SHIFT	12
#define  B43_PHY_RFOVERVAL_PGA		0x0F00
#define  B43_PHY_RFOVERVAL_PGA_SHIFT	8
#define  B43_PHY_RFOVERVAL_UNK		0x0010	
#define  B43_PHY_RFOVERVAL_TRSWRX	0x00E0
#define  B43_PHY_RFOVERVAL_BW		0x0003	
#define   B43_PHY_RFOVERVAL_BW_LPF	0x0001	
#define   B43_PHY_RFOVERVAL_BW_LBW	0x0002	
#define B43_PHY_ANALOGOVER		B43_PHY_EXTG(0x14)	
#define B43_PHY_ANALOGOVERVAL		B43_PHY_EXTG(0x15)	


#define B43_GTAB(number, offset)	(((number) << B43_PHY_GTABNR_SHIFT) | (offset))
#define B43_GTAB_NRSSI			B43_GTAB(0x00, 0)
#define B43_GTAB_TRFEMW			B43_GTAB(0x0C, 0x120)
#define B43_GTAB_ORIGTR			B43_GTAB(0x2E, 0x298)

u16 b43_gtab_read(struct b43_wldev *dev, u16 table, u16 offset);
void b43_gtab_write(struct b43_wldev *dev, u16 table, u16 offset, u16 value);


#define has_tx_magnification(phy) \
	(((phy)->rev >= 2) &&			\
	 ((phy)->radio_ver == 0x2050) &&	\
	 ((phy)->radio_rev == 8))
#define has_loopback_gain(phy) \
	(((phy)->rev > 1) || ((phy)->gmode))

struct b43_rfatt {
	u8 att;			
	bool with_padmix;	
};
struct b43_rfatt_list {
	
	const struct b43_rfatt *list;
	u8 len;
	
	u8 min_val;
	u8 max_val;
};

static inline bool b43_compare_rfatt(const struct b43_rfatt *a,
				     const struct b43_rfatt *b)
{
	return ((a->att == b->att) &&
		(a->with_padmix == b->with_padmix));
}

struct b43_bbatt {
	u8 att;			
};
struct b43_bbatt_list {
	
	const struct b43_bbatt *list;
	u8 len;
	
	u8 min_val;
	u8 max_val;
};

static inline bool b43_compare_bbatt(const struct b43_bbatt *a,
				     const struct b43_bbatt *b)
{
	return (a->att == b->att);
}

#define B43_TXCTL_PA3DB		0x40	
#define B43_TXCTL_PA2DB		0x20	
#define B43_TXCTL_TXMIX		0x10	

struct b43_txpower_lo_control;

struct b43_phy_g {
	
	bool aci_enable;
	bool aci_wlan_automatic;
	bool aci_hw_rssi;

	
	bool radio_on;
	struct {
		bool valid;
		u16 rfover;
		u16 rfoverval;
	} radio_off_context;

	u16 minlowsig[2];
	u16 minlowsigpos[2];

	const s8 *tssi2dbm;
	
	bool dyn_tssi_tbl;
	
	int tgt_idle_tssi;
	
	int cur_idle_tssi;
	
	u8 average_tssi;
	
	struct b43_bbatt bbatt;
	struct b43_rfatt rfatt;
	u8 tx_control;		
	int bbatt_delta;
	int rfatt_delta;

	
	struct b43_txpower_lo_control *lo_control;
	
	s16 max_lb_gain;	
	s16 trsw_rx_gain;	
	s16 lna_lod_gain;	
	s16 lna_gain;		
	s16 pga_gain;		

	
	int interfmode;
#define B43_INTERFSTACK_SIZE	26
	u32 interfstack[B43_INTERFSTACK_SIZE];	

	
	s16 nrssi[2];
	s32 nrssislope;
	
	s8 nrssi_lt[64];

	u16 lofcal;

	u16 initval;		

	u16 ofdmtab_addr; 
	enum { 
		B43_OFDMTAB_DIRECTION_UNKNOWN = 0,
		B43_OFDMTAB_DIRECTION_READ,
		B43_OFDMTAB_DIRECTION_WRITE,
	} ofdmtab_addr_direction;
};

void b43_gphy_set_baseband_attenuation(struct b43_wldev *dev,
				       u16 baseband_attenuation);
void b43_gphy_channel_switch(struct b43_wldev *dev,
			     unsigned int channel,
			     bool synthetic_pu_workaround);
u8 * b43_generate_dyn_tssi2dbm_tab(struct b43_wldev *dev,
				   s16 pab0, s16 pab1, s16 pab2);

struct b43_phy_operations;
extern const struct b43_phy_operations b43_phyops_g;

#endif 
