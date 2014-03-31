
/* Written 2000 by Mitchell Blank Jr */

#ifndef _LINUX_ATMPPP_H
#define _LINUX_ATMPPP_H

#include <linux/atm.h>

#define PPPOATM_ENCAPS_AUTODETECT	(0)
#define PPPOATM_ENCAPS_VC		(1)
#define PPPOATM_ENCAPS_LLC		(2)

struct atm_backend_ppp {
	atm_backend_t	backend_num;	
	int		encaps;		
};

#endif	
