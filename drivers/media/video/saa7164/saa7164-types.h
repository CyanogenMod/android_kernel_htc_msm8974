/*
 *  Driver for the NXP SAA7164 PCIe bridge
 *
 *  Copyright (c) 2010 Steven Toth <stoth@kernellabs.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


struct tmComResHWDescr {
	u8	bLength;
	u8	bDescriptorType;
	u8	bDescriptorSubtype;
	u16	bcdSpecVersion;
	u32	dwClockFrequency;
	u32	dwClockUpdateRes;
	u8	bCapabilities;
	u32	dwDeviceRegistersLocation;
	u32	dwHostMemoryRegion;
	u32	dwHostMemoryRegionSize;
	u32	dwHostHibernatMemRegion;
	u32	dwHostHibernatMemRegionSize;
} __attribute__((packed));

struct tmComResInterfaceDescr {
	u8	bLength;
	u8	bDescriptorType;
	u8	bDescriptorSubtype;
	u8	bFlags;
	u8	bInterfaceType;
	u8	bInterfaceId;
	u8	bBaseInterface;
	u8	bInterruptId;
	u8	bDebugInterruptId;
	u8	BARLocation;
	u8	Reserved[3];
};

struct tmComResBusDescr {
	u64	CommandRing;
	u64	ResponseRing;
	u32	CommandWrite;
	u32	CommandRead;
	u32	ResponseWrite;
	u32	ResponseRead;
};

enum tmBusType {
	NONE		= 0,
	TYPE_BUS_PCI	= 1,
	TYPE_BUS_PCIe	= 2,
	TYPE_BUS_USB	= 3,
	TYPE_BUS_I2C	= 4
};

struct tmComResBusInfo {
	enum tmBusType Type;
	u16	m_wMaxReqSize;
	u8	*m_pdwSetRing;
	u32	m_dwSizeSetRing;
	u8	*m_pdwGetRing;
	u32	m_dwSizeGetRing;
	u32	m_dwSetWritePos;
	u32	m_dwSetReadPos;
	u32	m_dwGetWritePos;
	u32	m_dwGetReadPos;

	
	struct mutex lock;

};

struct tmComResInfo {
	u8	id;
	u8	flags;
	u16	size;
	u32	command;
	u16	controlselector;
	u8	seqno;
} __attribute__((packed));

enum tmComResCmd {
	SET_CUR  = 0x01,
	GET_CUR  = 0x81,
	GET_MIN  = 0x82,
	GET_MAX  = 0x83,
	GET_RES  = 0x84,
	GET_LEN  = 0x85,
	GET_INFO = 0x86,
	GET_DEF  = 0x87
};

struct cmd {
	u8 seqno;
	u32 inuse;
	u32 timeout;
	u32 signalled;
	struct mutex lock;
	wait_queue_head_t wait;
};

struct tmDescriptor {
	u32	pathid;
	u32	size;
	void	*descriptor;
};

struct tmComResDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
} __attribute__((packed));

struct tmComResExtDevDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
	u32	devicetype;
	u16	deviceid;
	u32	numgpiopins;
	u8	numgpiogroups;
	u8	controlsize;
} __attribute__((packed));

struct tmComResGPIO {
	u32	pin;
	u8	state;
} __attribute__((packed));

struct tmComResPathDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	pathid;
} __attribute__((packed));

enum tmComResTermType {
	ITT_ANTENNA              = 0x0203,
	LINE_CONNECTOR           = 0x0603,
	SPDIF_CONNECTOR          = 0x0605,
	COMPOSITE_CONNECTOR      = 0x0401,
	SVIDEO_CONNECTOR         = 0x0402,
	COMPONENT_CONNECTOR      = 0x0403,
	STANDARD_DMA             = 0xF101
};

struct tmComResAntTermDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	terminalid;
	u16	terminaltype;
	u8	assocterminal;
	u8	iterminal;
	u8	controlsize;
} __attribute__((packed));

struct tmComResTunerDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
	u8	sourceid;
	u8	iunit;
	u32	tuningstandards;
	u8	controlsize;
	u32	controls;
} __attribute__((packed));

enum tmBufferFlag {
	
	TM_BUFFER_FLAG_EMPTY,

	
	TM_BUFFER_FLAG_DONE,

	
	TM_BUFFER_FLAG_DUMMY_BUFFER
};

struct tmBuffer {
	u64		*pagetablevirt;
	u64		pagetablephys;
	u16		offset;
	u8		*context;
	u64		timestamp;
	enum tmBufferFlag BufferFlag;
	u32		lostbuffers;
	u32		validbuffers;
	u64		*dummypagevirt;
	u64		dummypagephys;
	u64		*addressvirt;
};

struct tmHWStreamParameters {
	u32	bitspersample;
	u32	samplesperline;
	u32	numberoflines;
	u32	pitch;
	u32	linethreshold;
	u64	**pagetablelistvirt;
	u64	*pagetablelistphys;
	u32	numpagetables;
	u32	numpagetableentries;
};

struct tmStreamParameters {
	struct tmHWStreamParameters	HWStreamParameters;
	u64				qwDummyPageTablePhys;
	u64				*pDummyPageTableVirt;
};

struct tmComResDMATermDescrHeader {
	u8	len;
	u8	type;
	u8	subtyle;
	u8	unitid;
	u16	terminaltype;
	u8	assocterminal;
	u8	sourceid;
	u8	iterminal;
	u32	BARLocation;
	u8	flags;
	u8	interruptid;
	u8	buffercount;
	u8	metadatasize;
	u8	numformats;
	u8	controlsize;
} __attribute__((packed));

struct tmComResTSFormatDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	bFormatIndex;
	u8	bDataOffset;
	u8	bPacketLength;
	u8	bStrideLength;
	u8	guidStrideFormat[16];
} __attribute__((packed));


struct tmComResSelDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
	u8	nrinpins;
	u8	sourceid;
} __attribute__((packed));

struct tmComResProcDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
	u8	sourceid;
	u16	wreserved;
	u8	controlsize;
} __attribute__((packed));

#define EU_VIDEO_BIT_RATE_MODE_CONSTANT		(0)
#define EU_VIDEO_BIT_RATE_MODE_VARIABLE_AVERAGE (1)
#define EU_VIDEO_BIT_RATE_MODE_VARIABLE_PEAK	(2)
struct tmComResEncVideoBitRate {
	u8	ucVideoBitRateMode;
	u32	dwVideoBitRate;
	u32	dwVideoBitRatePeak;
} __attribute__((packed));

struct tmComResEncVideoInputAspectRatio {
	u8	width;
	u8	height;
} __attribute__((packed));

#define SAA7164_ENCODER_DEFAULT_GOP_DIST (1)
#define SAA7164_ENCODER_DEFAULT_GOP_SIZE (15)
struct tmComResEncVideoGopStructure {
	u8	ucGOPSize;	
	u8	ucRefFrameDist; 
} __attribute__((packed));

struct tmComResEncoderDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
	u8	vsourceid;
	u8	asourceid;
	u8	iunit;
	u32	dwmControlCap;
	u32	dwmProfileCap;
	u32	dwmVidFormatCap;
	u8	bmVidBitrateCap;
	u16	wmVidResolutionsCap;
	u16	wmVidFrmRateCap;
	u32	dwmAudFormatCap;
	u8	bmAudBitrateCap;
} __attribute__((packed));

struct tmComResAFeatureDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	unitid;
	u8	sourceid;
	u8	controlsize;
} __attribute__((packed));

struct tmComResAudioDefaults {
	u8	ucDecoderLevel;
	u8	ucDecoderFM_Level;
	u8	ucMonoLevel;
	u8	ucNICAM_Level;
	u8	ucSAP_Level;
	u8	ucADC_Level;
} __attribute__((packed));

struct tmComResEncAudioBitRate {
	u8	ucAudioBitRateMode;
	u32	dwAudioBitRate;
	u32	dwAudioBitRatePeak;
} __attribute__((packed));

struct tmComResTunerStandard {
	u8	std;
	u32	country;
} __attribute__((packed));

struct tmComResTunerStandardAuto {
	u8	mode;
} __attribute__((packed));

struct tmComResPSFormatDescrHeader {
	u8	len;
	u8	type;
	u8	subtype;
	u8	bFormatIndex;
	u16	wPacketLength;
	u16	wPackLength;
	u8	bPackDataType;
} __attribute__((packed));

struct tmComResVBIFormatDescrHeader {
	u8	len;
	u8	type;
	u8	subtype; 
	u8	bFormatIndex;
	u32	VideoStandard; 
	u8	StartLine; 
	u8	EndLine; 
	u8	FieldRate; 
	u8	bNumLines; 
} __attribute__((packed));

struct tmComResProbeCommit {
	u16	bmHint;
	u8	bFormatIndex;
	u8	bFrameIndex;
} __attribute__((packed));

struct tmComResDebugSetLevel {
	u32	dwDebugLevel;
} __attribute__((packed));

struct tmComResDebugGetData {
	u32	dwResult;
	u8	ucDebugData[256];
} __attribute__((packed));

struct tmFwInfoStruct {
	u32	status;
	u32	mode;
	u32	devicespec;
	u32	deviceinst;
	u32	CPULoad;
	u32	RemainHeap;
	u32	CPUClock;
	u32	RAMSpeed;
} __attribute__((packed));
