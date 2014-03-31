/*
 *  cx18 header containing common defines.
 *
 *  Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA
 */

#ifndef CX23418_H
#define CX23418_H

#include <media/cx2341x.h>

#define MGR_CMD_MASK            		0x40000000
#define MGR_CMD_MASK_ACK        		(MGR_CMD_MASK | 0x80000000)

#define CX18_CREATE_TASK      			(MGR_CMD_MASK | 0x0001)

#define CX18_DESTROY_TASK     			(MGR_CMD_MASK | 0x0002)

#define CPU_CMD_MASK                        	0x20000000
#define CPU_CMD_MASK_DEBUG       		(CPU_CMD_MASK | 0x00000000)
#define CPU_CMD_MASK_ACK                    	(CPU_CMD_MASK | 0x80000000)
#define CPU_CMD_MASK_CAPTURE                	(CPU_CMD_MASK | 0x00020000)
#define CPU_CMD_MASK_TS                     	(CPU_CMD_MASK | 0x00040000)

#define EPU_CMD_MASK                        	0x02000000
#define EPU_CMD_MASK_DEBUG       		(EPU_CMD_MASK | 0x000000)
#define EPU_CMD_MASK_DE                     	(EPU_CMD_MASK | 0x040000)

#define APU_CMD_MASK 				0x10000000
#define APU_CMD_MASK_ACK 			(APU_CMD_MASK | 0x80000000)

#define CX18_APU_ENCODING_METHOD_MPEG		(0 << 28)
#define CX18_APU_ENCODING_METHOD_AC3		(1 << 28)

#define CX18_APU_START				(APU_CMD_MASK | 0x01)

#define CX18_APU_STOP				(APU_CMD_MASK | 0x02)

#define CX18_APU_RESETAI 			(APU_CMD_MASK | 0x05)

#define CX18_EPU_DMA_DONE              		(EPU_CMD_MASK_DE | 0x0001)

#define CX18_EPU_DEBUG 				(EPU_CMD_MASK_DEBUG | 0x0003)

#define CX18_CPU_DEBUG_PEEK32			(CPU_CMD_MASK_DEBUG | 0x0003)

#define CX18_CPU_CAPTURE_START               	(CPU_CMD_MASK_CAPTURE | 0x0002)

#define CX18_CPU_CAPTURE_STOP                	(CPU_CMD_MASK_CAPTURE | 0x0003)

#define CX18_CPU_CAPTURE_PAUSE               	(CPU_CMD_MASK_CAPTURE | 0x0007)

#define CX18_CPU_CAPTURE_RESUME              	(CPU_CMD_MASK_CAPTURE | 0x0008)

#define CAPTURE_CHANNEL_TYPE_NONE  		0
#define CAPTURE_CHANNEL_TYPE_MPEG  		1
#define CAPTURE_CHANNEL_TYPE_INDEX 		2
#define CAPTURE_CHANNEL_TYPE_YUV   		3
#define CAPTURE_CHANNEL_TYPE_PCM   		4
#define CAPTURE_CHANNEL_TYPE_VBI   		5
#define CAPTURE_CHANNEL_TYPE_SLICED_VBI		6
#define CAPTURE_CHANNEL_TYPE_TS			7
#define CAPTURE_CHANNEL_TYPE_MAX   		15

#define CX18_CPU_SET_CHANNEL_TYPE      		(CPU_CMD_MASK_CAPTURE + 1)

#define CX18_CPU_SET_STREAM_OUTPUT_TYPE		(CPU_CMD_MASK_CAPTURE | 0x0012)

#define CX18_CPU_SET_VIDEO_IN                	(CPU_CMD_MASK_CAPTURE | 0x0004)

#define CX18_CPU_SET_VIDEO_RATE              	(CPU_CMD_MASK_CAPTURE | 0x0005)

#define CX18_CPU_SET_VIDEO_RESOLUTION		(CPU_CMD_MASK_CAPTURE | 0x0006)

#define CX18_CPU_SET_FILTER_PARAM            	(CPU_CMD_MASK_CAPTURE | 0x0009)

#define CX18_CPU_SET_SPATIAL_FILTER_TYPE     	(CPU_CMD_MASK_CAPTURE | 0x000C)

#define CX18_CPU_SET_MEDIAN_CORING           	(CPU_CMD_MASK_CAPTURE | 0x000E)

#define CX18_CPU_SET_INDEXTABLE         	(CPU_CMD_MASK_CAPTURE | 0x0010)

#define CX18_CPU_SET_AUDIO_PARAMETERS		(CPU_CMD_MASK_CAPTURE | 0x0011)

#define CX18_CPU_SET_VIDEO_MUTE			(CPU_CMD_MASK_CAPTURE | 0x0013)

#define CX18_CPU_SET_AUDIO_MUTE			(CPU_CMD_MASK_CAPTURE | 0x0014)

#define CX18_CPU_SET_MISC_PARAMETERS		(CPU_CMD_MASK_CAPTURE | 0x0015)

#define CX18_CPU_SET_RAW_VBI_PARAM		(CPU_CMD_MASK_CAPTURE | 0x0016)

#define CX18_CPU_SET_CAPTURE_LINE_NO		(CPU_CMD_MASK_CAPTURE | 0x0017)

/* Description: Set copyright
   IN[0] - task handle. Handle of the task to start
   IN[1] - copyright
   ReturnCode - One of the ERR_CAPTURE_... */
#define CX18_CPU_SET_COPYRIGHT			(CPU_CMD_MASK_CAPTURE | 0x0018)

#define CX18_CPU_SET_AUDIO_PID			(CPU_CMD_MASK_CAPTURE | 0x0019)

#define CX18_CPU_SET_VIDEO_PID			(CPU_CMD_MASK_CAPTURE | 0x001A)

#define CX18_CPU_SET_VER_CROP_LINE		(CPU_CMD_MASK_CAPTURE | 0x001B)

#define CX18_CPU_SET_GOP_STRUCTURE		(CPU_CMD_MASK_CAPTURE | 0x001C)

#define CX18_CPU_SET_SCENE_CHANGE_DETECTION	(CPU_CMD_MASK_CAPTURE | 0x001D)

#define CX18_CPU_SET_ASPECT_RATIO		(CPU_CMD_MASK_CAPTURE | 0x001E)

#define CX18_CPU_SET_SKIP_INPUT_FRAME		(CPU_CMD_MASK_CAPTURE | 0x001F)

#define CX18_CPU_SET_SLICED_VBI_PARAM		(CPU_CMD_MASK_CAPTURE | 0x0020)

#define CX18_CPU_SET_USERDATA_PLACE_HOLDER	(CPU_CMD_MASK_CAPTURE | 0x0021)


#define CX18_CPU_GET_ENC_PTS			(CPU_CMD_MASK_CAPTURE | 0x0022)

#define CX18_CPU_SET_VFC_PARAM                  (CPU_CMD_MASK_CAPTURE | 0x0023)

#define CPU_CMD_MASK_DE 			(CPU_CMD_MASK | 0x040000)

#define CPU_CMD_DE_SetBase 			(CPU_CMD_MASK_DE | 0x0001)

#define CX18_CPU_DE_SET_MDL_ACK                	(CPU_CMD_MASK_DE | 0x0002)

#define CX18_CPU_DE_SET_MDL                   	(CPU_CMD_MASK_DE | 0x0005)

#define CX18_CPU_DE_RELEASE_MDL               	(CPU_CMD_MASK_DE | 0x0006)


#define CNXT_OK                 0x000000

#define CXERR_UNK_CMD           0x000001

#define CXERR_INVALID_PARAM1    0x000002

#define CXERR_INVALID_PARAM2    0x000003

#define CXERR_DEV_NOT_FOUND     0x000004

#define CXERR_NOTSUPPORTED      0x000005

#define CXERR_BADPTR            0x000006

#define CXERR_NOMEM             0x000007

#define CXERR_LINK              0x000008

#define CXERR_BUSY              0x000009

#define CXERR_NOT_OPEN          0x00000A

#define CXERR_OUTOFRANGE        0x00000B

#define CXERR_OVERFLOW          0x00000C

#define CXERR_BADVER            0x00000D

#define CXERR_TIMEOUT           0x00000E

#define CXERR_ABORT             0x00000F

#define CXERR_I2CDEV_NOTFOUND   0x000010

#define CXERR_I2CDEV_XFERERR    0x000011

#define CXERR_CHANNELNOTREADY   0x000012

#define CXERR_PPU_MB_CORRUPT    0x000013

#define CXERR_CPU_MB_CORRUPT    0x000014

#define CXERR_APU_MB_CORRUPT    0x000015

#define CXERR_FILE_OPEN_READ    0x000016

#define CXERR_FILE_OPEN_WRITE   0x000017

#define CXERR_I2C_BADSECTION    0x000018

#define CXERR_I2CDEV_DATALOW    0x000019

#define CXERR_I2CDEV_CLOCKLOW   0x00001A

#define CXERR_NO_HW_I2C_INTR    0x00001B

#define CXERR_RPU_NOT_READY     0x00001C

#define CXERR_RPU_NO_ACK        0x00001D

#define CXERR_NODATA_AGAIN      0x00001E

#define CXERR_STOPPING_STATUS   0x00001F

#define CXERR_DEVPOWER_OFF      0x000020

#endif 
