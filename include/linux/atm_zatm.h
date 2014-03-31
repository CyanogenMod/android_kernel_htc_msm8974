
/* Written 1995-1999 by Werner Almesberger, EPFL LRC/ICA */


#ifndef LINUX_ATM_ZATM_H
#define LINUX_ATM_ZATM_H


#include <linux/atmapi.h>
#include <linux/atmioc.h>

#define ZATM_GETPOOL	_IOW('a',ATMIOC_SARPRV+1,struct atmif_sioc)
						
#define ZATM_GETPOOLZ	_IOW('a',ATMIOC_SARPRV+2,struct atmif_sioc)
						
#define ZATM_SETPOOL	_IOW('a',ATMIOC_SARPRV+3,struct atmif_sioc)
						

struct zatm_pool_info {
	int ref_count;			
	int low_water,high_water;	
	int rqa_count,rqu_count;	
	int offset,next_off;		
	int next_cnt,next_thres;	
};

struct zatm_pool_req {
	int pool_num;			
	struct zatm_pool_info info;	
};

struct zatm_t_hist {
	struct timeval real;		
	struct timeval expected;	
};


#define ZATM_OAM_POOL		0	
#define ZATM_AAL0_POOL		1	
#define ZATM_AAL5_POOL_BASE	2	
#define ZATM_LAST_POOL	ZATM_AAL5_POOL_BASE+10 

#define ZATM_TIMER_HISTORY_SIZE	16	

#endif
