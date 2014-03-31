
/* NCR Dual 700 MCA SCSI Driver
 *
 * Copyright (C) 2001 by James.Bottomley@HansenPartnership.com
 */

#ifndef _NCR_D700_H
#define _NCR_D700_H

#undef NCR_D700_DEBUG

#define NCR_D700_MCA_ID		0x0092

#define	BOARD_RESET		0x80	
#define ADD_PARENB		0x04	
#define DAT_PARENB		0x01	
#define SFBK_ENB		0x10	
#define LED0GREEN		0x20	
#define LED1GREEN		0x40	
#define LED0RED			0xDF	
#define LED1RED			0xBF	

#define NCR_D700_CLOCK_MHZ	50

#endif
