#ifndef __NWBUTTON_H
#define __NWBUTTON_H

/*
 * 	NetWinder Button Driver-
 *	Copyright (C) Alex Holden <alex@linuxhacker.org> 1998, 1999.
 */

#ifdef __NWBUTTON_C	


#define NUM_PRESSES_REBOOT 2	
#define BUTTON_DELAY 30 	
#define VERSION "0.3"		
#define BUTTON_MINOR 158	


struct button_callback {
	void (*callback) (void);
	int count;
};


static void button_sequence_finished (unsigned long parameters);
static irqreturn_t button_handler (int irq, void *dev_id);
int button_init (void);
int button_add_callback (void (*callback) (void), int count);
int button_del_callback (void (*callback) (void));
static void button_consume_callbacks (int bpcount);

#else 

extern int button_add_callback (void (*callback) (void), int count);
extern int button_del_callback (void (*callback) (void));

#endif 
#endif 
