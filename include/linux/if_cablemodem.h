#ifndef _LINUX_CABLEMODEM_H_
#define _LINUX_CABLEMODEM_H_
/*
 *		Author: Franco Venturi <fventuri@mediaone.net>
 *		Copyright 1998 Franco Venturi
 *
 *		This program is free software; you can redistribute it
 *		and/or  modify it under  the terms of  the GNU General
 *		Public  License as  published  by  the  Free  Software
 *		Foundation;  either  version 2 of the License, or  (at
 *		your option) any later version.
 */

#define SIOCGCMSTATS		SIOCDEVPRIVATE+0	
#define SIOCGCMFIRMWARE		SIOCDEVPRIVATE+1	
#define SIOCGCMFREQUENCY	SIOCDEVPRIVATE+2	
#define SIOCSCMFREQUENCY	SIOCDEVPRIVATE+3	
#define SIOCGCMPIDS			SIOCDEVPRIVATE+4	
#define SIOCSCMPIDS			SIOCDEVPRIVATE+5	

#endif
