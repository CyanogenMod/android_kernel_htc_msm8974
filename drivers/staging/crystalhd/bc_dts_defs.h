/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: bc_dts_defs.h
 *
 *  Description: Common definitions for all components. Only types
 *		 is allowed to be included from this file.
 *
 *  AU
 *
 *  HISTORY:
 *
 ********************************************************************
 * This header is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License.
 *
 * This header is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this header.  If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************/

#ifndef _BC_DTS_DEFS_H_
#define _BC_DTS_DEFS_H_

#include <linux/types.h>

#define BC_BIT(_x)		(1 << (_x))

enum BC_STATUS {
	BC_STS_SUCCESS		= 0,
	BC_STS_INV_ARG		= 1,
	BC_STS_BUSY		= 2,
	BC_STS_NOT_IMPL		= 3,
	BC_STS_PGM_QUIT		= 4,
	BC_STS_NO_ACCESS	= 5,
	BC_STS_INSUFF_RES	= 6,
	BC_STS_IO_ERROR		= 7,
	BC_STS_NO_DATA		= 8,
	BC_STS_VER_MISMATCH	= 9,
	BC_STS_TIMEOUT		= 10,
	BC_STS_FW_CMD_ERR	= 11,
	BC_STS_DEC_NOT_OPEN	= 12,
	BC_STS_ERR_USAGE	= 13,
	BC_STS_IO_USER_ABORT	= 14,
	BC_STS_IO_XFR_ERROR	= 15,
	BC_STS_DEC_NOT_STARTED	= 16,
	BC_STS_FWHEX_NOT_FOUND	= 17,
	BC_STS_FMT_CHANGE	= 18,
	BC_STS_HIF_ACCESS	= 19,
	BC_STS_CMD_CANCELLED	= 20,
	BC_STS_FW_AUTH_FAILED	= 21,
	BC_STS_BOOTLOADER_FAILED = 22,
	BC_STS_CERT_VERIFY_ERROR = 23,
	BC_STS_DEC_EXIST_OPEN	= 24,
	BC_STS_PENDING		= 25,
	BC_STS_CLK_NOCHG	= 26,

	
	BC_STS_ERROR		= -1
};

#define BC_REG_KEY_MAIN_PATH	"Software\\Broadcom\\MediaPC\\70010"
#define BC_REG_KEY_FWPATH		"FirmwareFilePath"
#define BC_REG_KEY_SEC_OPT		"DbgOptions"


enum BC_SW_OPTIONS {
	BC_OPT_DOSER_OUT_ENCRYPT	= BC_BIT(3),
	BC_OPT_LINK_OUT_ENCRYPT		= BC_BIT(29),
};

struct BC_REG_CONFIG {
	uint32_t		DbgOptions;
};

#if defined(__KERNEL__) || defined(__LINUX_USER__)
#else
#define ALIGN(x)	__declspec(align(x))
#endif


enum DtsDeviceOpenMode {
	DTS_PLAYBACK_MODE = 0,
	DTS_DIAG_MODE,
	DTS_MONITOR_MODE,
	DTS_HWINIT_MODE
};

enum DtsDeviceFixMode {
	DTS_LOAD_NEW_FW		= BC_BIT(8),
	DTS_LOAD_FILE_PLAY_FW	= BC_BIT(9),
	DTS_DISK_FMT_BD		= BC_BIT(10),
	
	DTS_SKIP_TX_CHK_CPB	= BC_BIT(16),
	DTS_ADAPTIVE_OUTPUT_PER	= BC_BIT(17),
	DTS_INTELLIMAP		= BC_BIT(18),
	
	DTS_PLAYBACK_DROP_RPT_MODE = BC_BIT(22)
};

#define DTS_DFLT_RESOLUTION(x)	(x<<11)

#define DTS_DFLT_CLOCK(x) (x<<19)

enum FW_FILE_VER {
	
	BC_FW_VER_020402 = ((12<<16) | (2<<8) | (0))
};

enum DtsOpenDecStreamTypes {
	BC_STREAM_TYPE_ES		= 0,
	BC_STREAM_TYPE_PES		= 1,
	BC_STREAM_TYPE_TS		= 2,
	BC_STREAM_TYPE_ES_TSTAMP	= 6,
};

enum DtsSetVideoParamsAlgo {
	BC_VID_ALGO_H264		= 0,
	BC_VID_ALGO_MPEG2		= 1,
	BC_VID_ALGO_VC1			= 4,
	BC_VID_ALGO_VC1MP		= 7,
};

#define BC_MPEG_VALID_PANSCAN		(1)

struct BC_PIB_EXT_MPEG {
	uint32_t	valid;
	uint32_t	display_horizontal_size;
	uint32_t	display_vertical_size;

	uint32_t	offset_count;
	int32_t		horizontal_offset[3];
	int32_t		vertical_offset[3];
};

#define H264_VALID_PANSCAN		(1)
#define H264_VALID_SPS_CROP		(2)
#define H264_VALID_VUI			(4)

struct BC_PIB_EXT_H264 {
	uint32_t	valid;

	
	uint32_t	pan_scan_count;
	int32_t		pan_scan_left[3];
	int32_t		pan_scan_right[3];
	int32_t		pan_scan_top[3];
	int32_t		pan_scan_bottom[3];

	
	int32_t		sps_crop_left;
	int32_t		sps_crop_right;
	int32_t		sps_crop_top;
	int32_t		sps_crop_bottom;

	
	uint32_t	chroma_top;
	uint32_t	chroma_bottom;
};

#define VC1_VALID_PANSCAN		(1)

struct BC_PIB_EXT_VC1 {
	uint32_t	valid;

	uint32_t	display_horizontal_size;
	uint32_t	display_vertical_size;

	
	uint32_t	num_panscan_windows;
	int32_t		ps_horiz_offset[4];
	int32_t		ps_vert_offset[4];
	int32_t		ps_width[4];
	int32_t		ps_height[4];
};

#if defined(__LINUX_USER__)
enum {
	vdecNoPulldownInfo	= 0,
	vdecTop			= 1,
	vdecBottom		= 2,
	vdecTopBottom		= 3,
	vdecBottomTop		= 4,
	vdecTopBottomTop	= 5,
	vdecBottomTopBottom	= 6,
	vdecFrame_X2		= 7,
	vdecFrame_X3		= 8,
	vdecFrame_X1		= 9,
	vdecFrame_X4		= 10,
};

enum {
	vdecFrameRateUnknown = 0,
	vdecFrameRate23_97,
	vdecFrameRate24,
	vdecFrameRate25,
	vdecFrameRate29_97,
	vdecFrameRate30,
	vdecFrameRate50,
	vdecFrameRate59_94,
	vdecFrameRate60,
};

enum {
	vdecAspectRatioUnknown = 0,
	vdecAspectRatioSquare,
	vdecAspectRatio12_11,
	vdecAspectRatio10_11,
	vdecAspectRatio16_11,
	vdecAspectRatio40_33,
	vdecAspectRatio24_11,
	vdecAspectRatio20_11,
	vdecAspectRatio32_11,
	vdecAspectRatio80_33,
	vdecAspectRatio18_11,
	vdecAspectRatio15_11,
	vdecAspectRatio64_33,
	vdecAspectRatio160_99,
	vdecAspectRatio4_3,
	vdecAspectRatio16_9,
	vdecAspectRatio221_1,
	vdecAspectRatioOther = 255,
};

enum {
	vdecColourPrimariesUnknown = 0,
	vdecColourPrimariesBT709,
	vdecColourPrimariesUnspecified,
	vdecColourPrimariesReserved,
	vdecColourPrimariesBT470_2M = 4,
	vdecColourPrimariesBT470_2BG,
	vdecColourPrimariesSMPTE170M,
	vdecColourPrimariesSMPTE240M,
	vdecColourPrimariesGenericFilm,
};
enum {
	vdecRESOLUTION_CUSTOM	= 0x00000000,
	vdecRESOLUTION_480i	= 0x00000001,
	vdecRESOLUTION_1080i	= 0x00000002,
	vdecRESOLUTION_NTSC	= 0x00000003,
	vdecRESOLUTION_480p	= 0x00000004,
	vdecRESOLUTION_720p	= 0x00000005,
	vdecRESOLUTION_PAL1	= 0x00000006,
	vdecRESOLUTION_1080i25	= 0x00000007,
	vdecRESOLUTION_720p50	= 0x00000008,
	vdecRESOLUTION_576p	= 0x00000009,
	vdecRESOLUTION_1080i29_97 = 0x0000000A,
	vdecRESOLUTION_720p59_94  = 0x0000000B,
	vdecRESOLUTION_SD_DVD	= 0x0000000C,
	vdecRESOLUTION_480p656	= 0x0000000D,
	vdecRESOLUTION_1080p23_976 = 0x0000000E,
	vdecRESOLUTION_720p23_976  = 0x0000000F,
	vdecRESOLUTION_240p29_97   = 0x00000010,
	vdecRESOLUTION_240p30	= 0x00000011,
	vdecRESOLUTION_288p25	= 0x00000012,
	vdecRESOLUTION_1080p29_97 = 0x00000013,
	vdecRESOLUTION_1080p30	= 0x00000014,
	vdecRESOLUTION_1080p24	= 0x00000015,
	vdecRESOLUTION_1080p25	= 0x00000016,
	vdecRESOLUTION_720p24	= 0x00000017,
	vdecRESOLUTION_720p29_97  = 0x00000018,
	vdecRESOLUTION_480p23_976 = 0x00000019,
	vdecRESOLUTION_480p29_97  = 0x0000001A,
	vdecRESOLUTION_576p25	= 0x0000001B,
	
	vdecRESOLUTION_480p0	= 0x0000001C,
	vdecRESOLUTION_480i0	= 0x0000001D,
	vdecRESOLUTION_576p0	= 0x0000001E,
	vdecRESOLUTION_720p0	= 0x0000001F,
	vdecRESOLUTION_1080p0	= 0x00000020,
	vdecRESOLUTION_1080i0	= 0x00000021,
};

#define VDEC_FLAG_EOS				(0x0004)

#define VDEC_FLAG_FRAME				(0x0000)
#define VDEC_FLAG_FIELDPAIR			(0x0008)
#define VDEC_FLAG_TOPFIELD			(0x0010)
#define VDEC_FLAG_BOTTOMFIELD			(0x0018)

#define VDEC_FLAG_PROGRESSIVE_SRC		(0x0000)
#define VDEC_FLAG_INTERLACED_SRC		(0x0020)
#define VDEC_FLAG_UNKNOWN_SRC			(0x0040)

#define VDEC_FLAG_BOTTOM_FIRST			(0x0080)
#define VDEC_FLAG_LAST_PICTURE			(0x0100)

#define VDEC_FLAG_PICTURE_META_DATA_PRESENT	(0x40000)

#endif 

enum _BC_OUTPUT_FORMAT {
	MODE420				= 0x0,
	MODE422_YUY2			= 0x1,
	MODE422_UYVY			= 0x2,
};
struct BC_PIC_INFO_BLOCK {
	
	uint64_t	timeStamp;
	uint32_t	picture_number;
	uint32_t	width;
	uint32_t	height;
	uint32_t	chroma_format;
	uint32_t	pulldown;
	uint32_t	flags;
	uint32_t	frame_rate;
	uint32_t	aspect_ratio;
	uint32_t	colour_primaries;
	uint32_t	picture_meta_payload;
	uint32_t	sess_num;
	uint32_t	ycom;
	uint32_t	custom_aspect_ratio_width_height;
	uint32_t	n_drop;	

	
	union {
		struct BC_PIB_EXT_H264	h264;
		struct BC_PIB_EXT_MPEG	mpeg;
		struct BC_PIB_EXT_VC1	 vc1;
	} other;

};


enum POUT_OPTIONAL_IN_FLAGS_ {
	
	BC_POUT_FLAGS_YV12	  = 0x01,
	BC_POUT_FLAGS_STRIDE	  = 0x02,
	BC_POUT_FLAGS_SIZE	  = 0x04,
	BC_POUT_FLAGS_INTERLACED  = 0x08,
	BC_POUT_FLAGS_INTERLEAVED = 0x10,

	
	BC_POUT_FLAGS_FMT_CHANGE  = 0x10000,
	BC_POUT_FLAGS_PIB_VALID	  = 0x20000,
	BC_POUT_FLAGS_ENCRYPTED	  = 0x40000,
	BC_POUT_FLAGS_FLD_BOT	  = 0x80000,
};

typedef enum BC_STATUS(*dts_pout_callback)(void  *shnd, uint32_t width,
			uint32_t height, uint32_t stride, void *pOut);

#define MAX_UD_SIZE		1792	

struct BC_DTS_PROC_OUT {
	uint8_t		*Ybuff;
	uint32_t	YbuffSz;
	uint32_t	YBuffDoneSz;

	uint8_t		*UVbuff;
	uint32_t	UVbuffSz;
	uint32_t	UVBuffDoneSz;

	uint32_t	StrideSz;
	uint32_t	PoutFlags;

	uint32_t	discCnt;

	struct BC_PIC_INFO_BLOCK PicInfo;

	
	
	uint32_t	UserDataSz;
	uint8_t		UserData[MAX_UD_SIZE];

	void		*hnd;
	dts_pout_callback AppCallBack;
	uint8_t		DropFrames;
	uint8_t		b422Mode;
	uint8_t		bPibEnc;
	uint8_t		bRevertScramble;

};
struct BC_DTS_STATUS {
	uint8_t		ReadyListCount;
	uint8_t		FreeListCount;
	uint8_t		PowerStateChange;
	uint8_t		reserved_[1];
	uint32_t	FramesDropped;
	uint32_t	FramesCaptured;
	uint32_t	FramesRepeated;
	uint32_t	InputCount;
	uint64_t	InputTotalSize;
	uint32_t	InputBusyCount;
	uint32_t	PIBMissCount;
	uint32_t	cpbEmptySize;
	uint64_t	NextTimeStamp;
	uint8_t		reserved__[16];
};

#define BC_SWAP32(_v)			\
	((((_v) & 0xFF000000)>>24)|	\
	  (((_v) & 0x00FF0000)>>8)|	\
	  (((_v) & 0x0000FF00)<<8)|	\
	  (((_v) & 0x000000FF)<<24))

#define WM_AGENT_TRAYICON_DECODER_OPEN	10001
#define WM_AGENT_TRAYICON_DECODER_CLOSE	10002
#define WM_AGENT_TRAYICON_DECODER_START	10003
#define WM_AGENT_TRAYICON_DECODER_STOP	10004
#define WM_AGENT_TRAYICON_DECODER_RUN	10005
#define WM_AGENT_TRAYICON_DECODER_PAUSE	10006


#endif	
