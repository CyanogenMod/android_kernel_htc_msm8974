/*
	usa49msg.h

	Copyright (C) 1998-2000 InnoSys Incorporated.  All Rights Reserved
	This file is available under a BSD-style copyright

	Keyspan USB Async Message Formats for the USA49W

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

	4th revision: USA49W version

	Buffer formats for RX/TX data messages are not defined by
	a structure, but are described here:

	USB OUT (host -> USAxx, transmit) messages contain a 
	REQUEST_ACK indicator (set to 0xff to request an ACK at the 
	completion of transmit; 0x00 otherwise), followed by data:

		RQSTACK DAT DAT DAT ...

	with a total data length of 63.

	USB IN (USAxx -> host, receive) messages begin with a status
	byte in which the 0x80 bit is either:
				   	
		(a)	0x80 bit clear
			indicates that the bytes following it are all data
			bytes:

				STAT DATA DATA DATA DATA DATA ...

			for a total of up to 63 DATA bytes,

	or:

		(b)	0x80 bit set
			indiates that the bytes following alternate data and
			status bytes:

				STAT DATA STAT DATA STAT DATA STAT DATA ...

			for a total of up to 32 DATA bytes.

	The valid bits in the STAT bytes are:

		OVERRUN	0x02
		PARITY	0x04
		FRAMING	0x08
		BREAK	0x10

	Notes:
	
	(1) The OVERRUN bit can appear in either (a) or (b) format
		messages, but the but the PARITY/FRAMING/BREAK bits
		only appear in (b) format messages.
	(2) For the host to determine the exact point at which the
		overrun occurred (to identify the point in the data
		stream at which the data was lost), it needs to count
		128 characters, starting at the first character of the
		message in which OVERRUN was reported; the lost character(s)
		would have been received between the 128th and 129th
		characters.
	(3)	An RX data message in which the first byte has 0x80 clear
		serves as a "break off" indicator.
	(4)	a control message specifying disablePort will be answered
		with a status message, but no further status will be sent
		until a control messages with enablePort is sent

	revision history:

	1999feb10	add reportHskiaChanges to allow us to ignore them
	1999feb10	add txAckThreshold for fast+loose throughput enhancement
	1999mar30	beef up support for RX error reporting
	1999apr14	add resetDataToggle to control message
	2000jan04	merge with usa17msg.h
	2000mar08	clone from usa26msg.h -> usa49msg.h
	2000mar09	change to support 4 ports
	2000may03	change external clocking to match USA-49W hardware
	2000jun01	add extended BSD-style copyright text
	2001jul05	change message format to improve OVERRUN case
*/

#ifndef	__USA49MSG__
#define	__USA49MSG__



struct keyspan_usa49_portControlMessage
{
	u8	portNumber,

		setClocking,	
		baudLo,			
		baudHi,			
		prescaler,		
						
		txClocking,		
		rxClocking,		

		setLcr,			
		lcr,			

		setFlowControl,	
		ctsFlowControl,	
		xonFlowControl,	
		xonChar,		
		xoffChar,		

		setRts,			
		rts,			

		setDtr,			
		dtr;			


	u8	forwardingLength,  
		dsrFlowControl,	
		txAckThreshold,	
		loopbackMode;	

	u8	_txOn,			
		_txOff,			
		txFlush,		
		txBreak,		
		rxOn,			
		rxOff,			
		rxFlush,		
		rxForward,		
		returnStatus,	
		resetDataToggle,
		enablePort,		
		disablePort;	
	
};

#define	USA_DATABITS_5		0x00
#define	USA_DATABITS_6		0x01
#define	USA_DATABITS_7		0x02
#define	USA_DATABITS_8		0x03
#define	STOPBITS_5678_1		0x00	
#define	STOPBITS_5_1p5		0x04	
#define	STOPBITS_678_2		0x04	
#define	USA_PARITY_NONE		0x00
#define	USA_PARITY_ODD		0x08
#define	USA_PARITY_EVEN		0x18
#define	PARITY_1			0x28
#define	PARITY_0			0x38


struct keyspan_usa49_globalControlMessage
{
	u8	portNumber,			
		sendGlobalStatus,	
		resetStatusToggle,	
		resetStatusCount,	
		remoteWakeupEnable,		
		disableStatusMessages;	
};


struct keyspan_usa49_portStatusMessage	
{
	u8	portNumber,		
		cts,			
		dcd,			
		dsr,			
		ri,				
		_txOff,			
		_txXoff,		
		rxEnabled,		
		controlResponse,
		txAck,			
		rs232valid;		
};

#define	RXERROR_OVERRUN	0x02
#define	RXERROR_PARITY	0x04
#define	RXERROR_FRAMING	0x08
#define	RXERROR_BREAK	0x10

struct keyspan_usa49_globalStatusMessage
{
	u8	portNumber,			
		sendGlobalStatus,	
		resetStatusCount;	
};

struct keyspan_usa49_globalDebugMessage
{
	u8	portNumber,			
		n,					
		b;					
};

#define	MAX_DATA_LEN			64

#define	STATUS_UPDATE_INTERVAL	16

#define	STATUS_RATION	10

#endif
