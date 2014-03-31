#ifndef __ISDN_SC_SCIOC_H__
#define __ISDN_SC_SCIOC_H__

/*
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#define SCIOCLOAD	0x01	
#define SCIOCRESET	0x02	
#define SCIOCDEBUG	0x03	
#define SCIOCREV	0x04	
#define SCIOCSTART	0x05	
#define SCIOCGETSWITCH	0x06	
#define SCIOCSETSWITCH	0x07	
#define SCIOCGETSPID	0x08	
#define SCIOCSETSPID	0x09	
#define SCIOCGETDN	0x0A	
#define SCIOCSETDN	0x0B	
#define SCIOCTRACE	0x0C	
#define SCIOCSTAT	0x0D	
#define SCIOCGETSPEED	0x0E	
#define SCIOCSETSPEED	0x0F	
#define SCIOCLOOPTST	0x10	

typedef struct {
	int device;
	int channel;
	unsigned long command;
	void __user *dataptr;
} scs_ioctl;

#define SCIOC_SPIDSIZE		49
#define SCIOC_DNSIZE		SCIOC_SPIDSIZE
#define SCIOC_REVSIZE		SCIOC_SPIDSIZE
#define SCIOC_SRECSIZE		49

typedef struct {
	unsigned long tx_good;
	unsigned long tx_bad;
	unsigned long rx_good;
	unsigned long rx_bad;
} ChLinkStats;

typedef struct {
	char spid[49];
	char dn[49];
	char call_type;
	char phy_stat;
	ChLinkStats link_stats;
} BRIStat;

typedef BRIStat POTStat;

typedef struct {
	char call_type;
	char call_state;
	char serv_state;
	char phy_stat;
	ChLinkStats link_stats;
} PRIStat;

typedef char PRIInfo;
typedef char BRIInfo;
typedef char POTInfo;


typedef struct {
	char acfa_nos;
	char acfa_ais;
	char acfa_los;
	char acfa_rra;
	char acfa_slpp;
	char acfa_slpn;
	char acfa_fsrf;
} ACFAStat;

typedef struct {
	unsigned char modelid;
	char serial_no[13];
	char part_no[13];
	char load_ver[11];
	char proc_ver[11];
	int iobase;
	long rambase;
	char irq;
	long ramsize;
	char interface;
	char switch_type;
	char l1_status;
	char l2_status;
	ChLinkStats dch_stats;
	ACFAStat AcfaStats;
	union {
		PRIStat pristats[23];
		BRIStat bristats[2];
		POTStat potsstats[2];
	} status;
	union {
		PRIInfo priinfo;
		BRIInfo briinfo;
		POTInfo potsinfo;
	} info;
} boardInfo;

#endif  
