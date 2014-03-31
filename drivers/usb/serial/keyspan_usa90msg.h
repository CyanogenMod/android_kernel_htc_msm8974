/*
	usa90msg.h

	Copyright (c) 1998-2003 InnoSys Incorporated.  All Rights Reserved
	This file is available under a BSD-style copyright

	Keyspan USB Async Message Formats for the USA19HS

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are
	met:

	1. Redistributions of source code must retain this licence text
   	without modification, this list of conditions, and the following
   	disclaimer.  The following copyright notice must appear immediately at
   	the beginning of all source files:

        	Copyright (c) 1998-2003 InnoSys Incorporated.  All Rights Reserved

        	This file is available under a BSD-style copyright

	2. The name of InnoSys Incorporated may not be used to endorse or promote
   	products derived from this software without specific prior written
   	permission.

	THIS SOFTWARE IS PROVIDED BY INNOSYS CORP. ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
	NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
	SUCH DAMAGE.    

	Revisions:

	2003feb14		add setTxMode/txMode  and cancelRxXoff to portControl
	2003mar21		change name of PARITY_0/1 to add MARK/SPACE
*/

#ifndef	__USA90MSG__
#define	__USA90MSG__

struct keyspan_usa90_portControlMessage
{

	u8	setClocking,	
		baudLo,			
		baudHi,			
		
		setLcr,			
		lcr,			
		
		setRxMode,		
		rxMode,			

		setTxMode,		
		txMode,			

		setTxFlowControl,	
		txFlowControl	,	
		setRxFlowControl,	
		rxFlowControl,	
		sendXoff,		
		sendXon,		
		xonChar,		
		xoffChar,		

		sendChar,		
		txChar,			

		setRts,			
		rts,			
		setDtr, 		
		dtr;			

	

	u8	rxForwardingLength,  
		rxForwardingTimeout, 
		txAckSetting;	   

	u8	portEnabled,	
		txFlush,		
		txBreak,		
		loopbackMode;	


	u8	rxFlush,		
		rxForward,		
		cancelRxXoff,	
		returnStatus;	
};

#define		USA_DATABITS_5		0x00
#define		USA_DATABITS_6		0x01
#define		USA_DATABITS_7		0x02
#define		USA_DATABITS_8		0x03
#define		STOPBITS_5678_1		0x00	
#define		STOPBITS_5_1p5		0x04	
#define		STOPBITS_678_2		0x04	
#define		USA_PARITY_NONE		0x00
#define		USA_PARITY_ODD		0x08
#define		USA_PARITY_EVEN		0x18
#define		PARITY_MARK_1  		0x28   	
#define		PARITY_SPACE_0 		0x38	

#define		TXFLOW_CTS			0x04	
#define		TXFLOW_DSR			0x08
#define		TXFLOW_XOFF			0x01	
#define		TXFLOW_XOFF_ANY		0x02	
#define		TXFLOW_XOFF_BITS	(TXFLOW_XOFF | TXFLOW_XOFF_ANY)

#define		RXFLOW_XOFF			0x10	
#define		RXFLOW_RTS			0x20	
#define		RXFLOW_DTR			0x40
#define		RXFLOW_DSR_SENSITIVITY	0x80

#define		RXMODE_BYHAND		0x00	
#define		RXMODE_DMA			0x02	

#define		TXMODE_BYHAND		0x00	
#define		TXMODE_DMA			0x02	



struct keyspan_usa90_portStatusMessage	
{
	u8	msr,			
		cts,			
		dcd,			
		dsr,			
		ri,				
		_txXoff,		
		rxBreak,		
		rxOverrun,		
		rxParity,		
		rxFrame,		
		portState,		
		messageAck,		
		charAck,		
		controlResponse;	
};


#define	RXERROR_OVERRUN		0x02
#define	RXERROR_PARITY		0x04
#define	RXERROR_FRAMING		0x08
#define	RXERROR_BREAK		0x10

#define	PORTSTATE_ENABLED	0x80
#define	PORTSTATE_TXFLUSH	0x01
#define	PORTSTATE_TXBREAK	0x02
#define	PORTSTATE_LOOPBACK 	0x04


#define USA_MSR_dCTS	  		0x01		
#define USA_MSR_dDSR	  		0x02
#define USA_MSR_dRI			0x04
#define USA_MSR_dDCD	  		0x08

#define USA_MSR_CTS			0x10	  	
#define USA_MSR_DSR			0x20
#define USA_USA_MSR_RI			0x40
#define MSR_DCD				0x80

#define		MAX_DATA_LEN			64

#endif
