
/* Written 1995-1997 by Werner Almesberger, EPFL LRC */


#ifndef NET_ATM_PROTOCOLS_H
#define NET_ATM_PROTOCOLS_H

int atm_init_aal0(struct atm_vcc *vcc);	
int atm_init_aal34(struct atm_vcc *vcc);
int atm_init_aal5(struct atm_vcc *vcc);	

#endif
