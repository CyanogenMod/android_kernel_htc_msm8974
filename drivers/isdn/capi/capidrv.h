/* $Id: capidrv.h,v 1.2.8.2 2001/09/23 22:24:33 kai Exp $
 *
 * ISDN4Linux Driver, using capi20 interface (kernelcapi)
 *
 * Copyright 1997 by Carsten Paeth <calle@calle.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef __CAPIDRV_H__
#define __CAPIDRV_H__

#define ST_LISTEN_NONE			0	
#define ST_LISTEN_WAIT_CONF		1	
#define ST_LISTEN_ACTIVE		2	
#define ST_LISTEN_ACTIVE_WAIT_CONF	3	


#define EV_LISTEN_REQ			1	
#define EV_LISTEN_CONF_ERROR		2	
#define EV_LISTEN_CONF_EMPTY		3	
#define EV_LISTEN_CONF_OK		4	

#define ST_PLCI_NONE			0	
#define ST_PLCI_OUTGOING		1	
#define ST_PLCI_ALLOCATED		2	
#define ST_PLCI_ACTIVE			3	
#define ST_PLCI_INCOMING		4	
#define ST_PLCI_FACILITY_IND		5	
#define ST_PLCI_ACCEPTING		6	
#define ST_PLCI_DISCONNECTING		7	
#define ST_PLCI_DISCONNECTED		8	
#define ST_PLCI_RESUMEING		9	
#define ST_PLCI_RESUME			10	
#define ST_PLCI_HELD			11	

#define EV_PLCI_CONNECT_REQ		1	
#define EV_PLCI_CONNECT_CONF_ERROR	2	
#define EV_PLCI_CONNECT_CONF_OK		3	
#define EV_PLCI_FACILITY_IND_UP		4	
#define EV_PLCI_CONNECT_IND		5	
#define EV_PLCI_CONNECT_ACTIVE_IND	6	
#define EV_PLCI_CONNECT_REJECT		7	
#define EV_PLCI_DISCONNECT_REQ		8	
#define EV_PLCI_DISCONNECT_IND		9	
#define EV_PLCI_FACILITY_IND_DOWN	10	
#define EV_PLCI_DISCONNECT_RESP		11	
#define EV_PLCI_CONNECT_RESP		12	

#define EV_PLCI_RESUME_REQ		13	
#define EV_PLCI_RESUME_CONF_OK		14	
#define EV_PLCI_RESUME_CONF_ERROR	15	
#define EV_PLCI_RESUME_IND		16	
#define EV_PLCI_HOLD_IND		17	
#define EV_PLCI_RETRIEVE_IND		18	
#define EV_PLCI_SUSPEND_IND		19	
#define EV_PLCI_CD_IND			20	

#define ST_NCCI_PREVIOUS			-1
#define ST_NCCI_NONE				0	
#define ST_NCCI_OUTGOING			1	
#define ST_NCCI_INCOMING			2	
#define ST_NCCI_ALLOCATED			3	
#define ST_NCCI_ACTIVE				4	
#define ST_NCCI_RESETING			5	
#define ST_NCCI_DISCONNECTING			6	
#define ST_NCCI_DISCONNECTED			7	

#define EV_NCCI_CONNECT_B3_REQ			1	
#define EV_NCCI_CONNECT_B3_IND			2	
#define EV_NCCI_CONNECT_B3_CONF_OK		3	
#define EV_NCCI_CONNECT_B3_CONF_ERROR		4	
#define EV_NCCI_CONNECT_B3_REJECT		5	
#define EV_NCCI_CONNECT_B3_RESP			6	
#define EV_NCCI_CONNECT_B3_ACTIVE_IND		7	
#define EV_NCCI_RESET_B3_REQ			8	
#define EV_NCCI_RESET_B3_IND			9	
#define EV_NCCI_DISCONNECT_B3_IND		10	
#define EV_NCCI_DISCONNECT_B3_CONF_ERROR	11	
#define EV_NCCI_DISCONNECT_B3_REQ		12	
#define EV_NCCI_DISCONNECT_B3_RESP		13	

#endif				
