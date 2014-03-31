#ifndef LINUX_IFX_MODEM_H
#define LINUX_IFX_MODEM_H

struct ifx_modem_platform_data {
	unsigned short rst_out;		
	unsigned short pwr_on;		
	unsigned short rst_pmu;		
	unsigned short tx_pwr;		
	unsigned short srdy;		
	unsigned short mrdy;		
	unsigned char modem_type;	
	unsigned long max_hz;		
	unsigned short use_dma:1;	
};
#define IFX_MODEM_6160	1
#define IFX_MODEM_6260	2

#endif
