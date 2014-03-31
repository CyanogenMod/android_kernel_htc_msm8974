/*
 * (C) 2002 - 2003 Dominik Brodowski <linux@brodo.de>
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 *  Library for common functions for Intel SpeedStep v.1 and v.2 support
 *
 *  BIG FAT DISCLAIMER: Work in progress code. Possibly *dangerous*
 */



enum speedstep_processor {
	SPEEDSTEP_CPU_PIII_C_EARLY = 0x00000001,  
	SPEEDSTEP_CPU_PIII_C	   = 0x00000002,  
	SPEEDSTEP_CPU_PIII_T	   = 0x00000003,  
	SPEEDSTEP_CPU_P4M	   = 0x00000004,  
	SPEEDSTEP_CPU_PM	   = 0xFFFFFF03,  
	SPEEDSTEP_CPU_P4D	   = 0xFFFFFF04,  
	SPEEDSTEP_CPU_PCORE	   = 0xFFFFFF05,  
};


#define SPEEDSTEP_HIGH	0x00000000
#define SPEEDSTEP_LOW	0x00000001


extern enum speedstep_processor speedstep_detect_processor(void);

extern unsigned int speedstep_get_frequency(enum speedstep_processor processor);


extern unsigned int speedstep_get_freqs(enum speedstep_processor processor,
	unsigned int *low_speed,
	unsigned int *high_speed,
	unsigned int *transition_latency,
	void (*set_state) (unsigned int state));
