/*
	usa28msg.h

	Copyright (C) 1998-2000 InnoSys Incorporated.  All Rights Reserved
	This file is available under a BSD-style copyright

	Keyspan USB Async Message Formats for the USA26X

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are
	met:

	1. Redistributions of source code must retain this licence text
   	without modification, this list of conditions, and the following
   	disclaimer.  The following copyright notice must appear immediately at
   	the beginning of all source files:

        	Copyright (C) 1998-2000 InnoSys Incorporated.  All Rights Reserved

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

	Note: these message formats are common to USA18, USA19, and USA28;
	(for USA28X, see usa26msg.h)

	Buffer formats for RX/TX data messages are not defined by
	a structure, but are described here:

	USB OUT (host -> USA28, transmit) messages contain a 
	REQUEST_ACK indicator (set to 0xff to request an ACK at the 
	completion of transmit; 0x00 otherwise), followed by data.
	If the port is configured for parity, the data will be an 
	alternating string of parity and data bytes, so the message
	format will be:

		RQSTACK PAR DAT PAR DAT ...

	so the maximum length is 63 bytes (1 + 62, or 31 data bytes);
	always an odd number for the total message length.

	If there is no parity, the format is simply:

		RQSTACK DAT DAT DAT ...

	with a total data length of 63.

	USB IN (USA28 -> host, receive) messages contain data and parity
	if parity is configred, thusly:
	
		DAT PAR DAT PAR DAT PAR ...

	for a total of 32 data bytes;
	
	If parity is not configured, the format is:

		DAT DAT DAT ...

	for a total of 64 data bytes.

	In the TX messages (USB OUT), the 0x01 bit of the PARity byte is 
	the parity bit.  In the RX messages (USB IN), the PARity byte is 
	the content of the 8051's status register; the parity bit 
	(RX_PARITY_BIT) is the 0x04 bit.

	revision history:

	1999may06	add resetDataToggle to control message
	2000mar21	add rs232invalid to status response message
	2000apr04	add 230.4Kb definition to setBaudRate
	2000apr13	add/remove loopbackMode switch
	2000apr13	change definition of setBaudRate to cover 115.2Kb, too
	2000jun01	add extended BSD-style copyright text
*/

#ifndef	__USA28MSG__
#define	__USA28MSG__


struct keyspan_usa28_portControlMessage
{
	u8	setBaudRate,	
		baudLo,			
		baudHi;			

	u8	parity,			
		ctsFlowControl,	        
					
		xonFlowControl,	
		rts,			
		dtr;			

	u8	forwardingLength,  
		forwardMs,		
		breakThreshold,	
		xonChar,		
		xoffChar;		

	u8	_txOn,			
		_txOff,			
		txFlush,		
		txForceXoff,	
		txBreak,		
		rxOn,			
		rxOff,			
		rxFlush,		
		rxForward,		
		returnStatus,	
		resetDataToggle;
	
};

struct keyspan_usa28_portStatusMessage
{
	u8	port,			
		cts,
		dsr,			
		dcd,

		ri,				
		_txOff,			
		_txXoff,		
		dataLost,		

		rxEnabled,		
		rxBreak,		
		rs232invalid,	
		controlResponse;
};

#define	TX_OFF			0x01	
#define	TX_XOFF			0x02	

struct keyspan_usa28_globalControlMessage
{
	u8	sendGlobalStatus,	
		resetStatusToggle,	
		resetStatusCount;	
};

struct keyspan_usa28_globalStatusMessage
{
	u8	port,				
		sendGlobalStatus,	
		resetStatusCount;	
};

struct keyspan_usa28_globalDebugMessage
{
	u8	port,				
		n,					
		b;					
};

#define	MAX_DATA_LEN			64

#define	RX_PARITY_BIT			0x04
#define	TX_PARITY_BIT			0x01

#define	STATUS_UPDATE_INTERVAL	16

#endif

