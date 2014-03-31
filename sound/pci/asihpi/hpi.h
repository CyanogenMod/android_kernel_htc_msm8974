/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2011  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
/** \file hpi.h

 AudioScience Hardware Programming Interface (HPI)
 public API definition.

 The HPI is a low-level hardware abstraction layer to all
 AudioScience digital audio adapters

(C) Copyright AudioScience Inc. 1998-2010
*/

#ifndef _HPI_H_
#define _HPI_H_

#include <linux/types.h>
#define HPI_BUILD_KERNEL_MODE


enum HPI_FORMATS {
	HPI_FORMAT_MIXER_NATIVE = 0,
	HPI_FORMAT_PCM8_UNSIGNED = 1,
	HPI_FORMAT_PCM16_SIGNED = 2,
	HPI_FORMAT_MPEG_L1 = 3,
	HPI_FORMAT_MPEG_L2 = 4,
	HPI_FORMAT_MPEG_L3 = 5,
	HPI_FORMAT_DOLBY_AC2 = 6,
	HPI_FORMAT_DOLBY_AC3 = 7,
	HPI_FORMAT_PCM16_BIGENDIAN = 8,
	HPI_FORMAT_AA_TAGIT1_HITS = 9,
	HPI_FORMAT_AA_TAGIT1_INSERTS = 10,
	HPI_FORMAT_PCM32_SIGNED = 11,
	HPI_FORMAT_RAW_BITSTREAM = 12,
	HPI_FORMAT_AA_TAGIT1_HITS_EX1 = 13,
	HPI_FORMAT_PCM32_FLOAT = 14,
	HPI_FORMAT_PCM24_SIGNED = 15,
	HPI_FORMAT_OEM1 = 16,
	HPI_FORMAT_OEM2 = 17,
	HPI_FORMAT_UNDEFINED = 0xffff
};

enum HPI_STREAM_STATES {
	
	HPI_STATE_STOPPED = 1,
	
	HPI_STATE_PLAYING = 2,
	
	HPI_STATE_RECORDING = 3,
	
	HPI_STATE_DRAINED = 4,
	
	HPI_STATE_SINEGEN = 5,
	HPI_STATE_WAIT = 6
};
enum HPI_SOURCENODES {
	HPI_SOURCENODE_NONE = 100,
	
	HPI_SOURCENODE_OSTREAM = 101,
	
	HPI_SOURCENODE_LINEIN = 102,
	HPI_SOURCENODE_AESEBU_IN = 103,	     
	HPI_SOURCENODE_TUNER = 104,	     
	HPI_SOURCENODE_RF = 105,	     
	HPI_SOURCENODE_CLOCK_SOURCE = 106,   
	HPI_SOURCENODE_RAW_BITSTREAM = 107,  
	HPI_SOURCENODE_MICROPHONE = 108,     
	HPI_SOURCENODE_COBRANET = 109,
	HPI_SOURCENODE_ANALOG = 110,	     
	HPI_SOURCENODE_ADAPTER = 111,	     
	HPI_SOURCENODE_RTP_DESTINATION = 112,
	HPI_SOURCENODE_INTERNAL = 113,	     
	
	HPI_SOURCENODE_LAST_INDEX = 113	     
		
};

enum HPI_DESTNODES {
	HPI_DESTNODE_NONE = 200,
	
	HPI_DESTNODE_ISTREAM = 201,
	HPI_DESTNODE_LINEOUT = 202,	     
	HPI_DESTNODE_AESEBU_OUT = 203,	     
	HPI_DESTNODE_RF = 204,		     
	HPI_DESTNODE_SPEAKER = 205,	     
	HPI_DESTNODE_COBRANET = 206,
	HPI_DESTNODE_ANALOG = 207,	     
	HPI_DESTNODE_RTP_SOURCE = 208,
	
	HPI_DESTNODE_LAST_INDEX = 208	     
		
};

enum HPI_CONTROLS {
	HPI_CONTROL_GENERIC = 0,	
	HPI_CONTROL_CONNECTION = 1, 
	HPI_CONTROL_VOLUME = 2,	      
	HPI_CONTROL_METER = 3,	
	HPI_CONTROL_MUTE = 4,	
	HPI_CONTROL_MULTIPLEXER = 5,	

	HPI_CONTROL_AESEBU_TRANSMITTER = 6, 
	HPI_CONTROL_AESEBUTX = 6,	

	HPI_CONTROL_AESEBU_RECEIVER = 7, 
	HPI_CONTROL_AESEBURX = 7,	

	HPI_CONTROL_LEVEL = 8, 
	HPI_CONTROL_TUNER = 9,	
	HPI_CONTROL_VOX = 11,	
	HPI_CONTROL_CHANNEL_MODE = 15,	

	HPI_CONTROL_BITSTREAM = 16,	
	HPI_CONTROL_SAMPLECLOCK = 17,	
	HPI_CONTROL_MICROPHONE = 18,	
	HPI_CONTROL_PARAMETRIC_EQ = 19,	
	HPI_CONTROL_EQUALIZER = 19,	

	HPI_CONTROL_COMPANDER = 20,	
	HPI_CONTROL_COBRANET = 21,	
	HPI_CONTROL_TONEDETECTOR = 22,	
	HPI_CONTROL_SILENCEDETECTOR = 23,	
	HPI_CONTROL_PAD = 24,	
	HPI_CONTROL_SRC = 25,	
	HPI_CONTROL_UNIVERSAL = 26,	

	HPI_CONTROL_LAST_INDEX = 26 
};

enum HPI_ADAPTER_PROPERTIES {
	HPI_ADAPTER_PROPERTY_ERRATA_1 = 1,

	HPI_ADAPTER_PROPERTY_GROUPING = 2,

	HPI_ADAPTER_PROPERTY_ENABLE_SSX2 = 3,

	HPI_ADAPTER_PROPERTY_SSX2_SETTING = 4,

	HPI_ADAPTER_PROPERTY_IRQ_RATE = 5,

	HPI_ADAPTER_PROPERTY_READONLYBASE = 256,

	HPI_ADAPTER_PROPERTY_LATENCY = 256,

	HPI_ADAPTER_PROPERTY_GRANULARITY = 257,

	HPI_ADAPTER_PROPERTY_CURCHANNELS = 258,

	HPI_ADAPTER_PROPERTY_SOFTWARE_VERSION = 259,

	HPI_ADAPTER_PROPERTY_MAC_ADDRESS_MSB = 260,

	HPI_ADAPTER_PROPERTY_MAC_ADDRESS_LSB = 261,

	HPI_ADAPTER_PROPERTY_EXTENDED_ADAPTER_TYPE = 262,

	HPI_ADAPTER_PROPERTY_LOGTABLEN = 263,
	HPI_ADAPTER_PROPERTY_LOGTABBEG = 264,

	HPI_ADAPTER_PROPERTY_IP_ADDRESS = 265,

	HPI_ADAPTER_PROPERTY_BUFFER_UPDATE_COUNT = 266,

	HPI_ADAPTER_PROPERTY_INTERVAL = 267,
	HPI_ADAPTER_PROPERTY_CAPS1 = 268,
	HPI_ADAPTER_PROPERTY_CAPS2 = 269,

	HPI_ADAPTER_PROPERTY_SYNC_HEADER_CONNECTIONS = 270,
	HPI_ADAPTER_PROPERTY_SUPPORTS_SSX2 = 271,
	HPI_ADAPTER_PROPERTY_SUPPORTS_IRQ = 272,
	HPI_ADAPTER_PROPERTY_SUPPORTS_FW_UPDATE = 273,
	HPI_ADAPTER_PROPERTY_FIRMWARE_ID = 274
};

enum HPI_ADAPTER_MODE_CMDS {
	
	HPI_ADAPTER_MODE_SET = 0,
	HPI_ADAPTER_MODE_QUERY = 1
};

enum HPI_ADAPTER_MODES {
	HPI_ADAPTER_MODE_4OSTREAM = 1,

	HPI_ADAPTER_MODE_6OSTREAM = 2,

	HPI_ADAPTER_MODE_8OSTREAM = 3,

	HPI_ADAPTER_MODE_16OSTREAM = 4,

	HPI_ADAPTER_MODE_1OSTREAM = 5,

	HPI_ADAPTER_MODE_1 = 6,

	HPI_ADAPTER_MODE_2 = 7,

	HPI_ADAPTER_MODE_3 = 8,

	HPI_ADAPTER_MODE_MULTICHANNEL = 9,

	HPI_ADAPTER_MODE_12OSTREAM = 10,

	HPI_ADAPTER_MODE_9OSTREAM = 11,

	HPI_ADAPTER_MODE_MONO = 12,

	HPI_ADAPTER_MODE_LOW_LATENCY = 13
};

#define HPI_CAPABILITY_NONE             (0)
#define HPI_CAPABILITY_MPEG_LAYER3      (1)

#define HPI_CAPABILITY_MAX                      1


enum HPI_MPEG_ANC_MODES {
	
	HPI_MPEG_ANC_HASENERGY = 0,
	HPI_MPEG_ANC_RAW = 1
};

enum HPI_ISTREAM_MPEG_ANC_ALIGNS {
	
	HPI_MPEG_ANC_ALIGN_LEFT = 0,
	
	HPI_MPEG_ANC_ALIGN_RIGHT = 1
};

enum HPI_MPEG_MODES {
	HPI_MPEG_MODE_DEFAULT = 0,
	
	HPI_MPEG_MODE_STEREO = 1,
	
	HPI_MPEG_MODE_JOINTSTEREO = 2,
	
	HPI_MPEG_MODE_DUALCHANNEL = 3
};

#define HPI_MIXER_GET_CONTROL_MULTIPLE_CHANGED  (0)
#define HPI_MIXER_GET_CONTROL_MULTIPLE_RESET    (1)

enum HPI_MIXER_STORE_COMMAND {
	HPI_MIXER_STORE_SAVE = 1,
	HPI_MIXER_STORE_RESTORE = 2,
	HPI_MIXER_STORE_DELETE = 3,
	HPI_MIXER_STORE_ENABLE = 4,
	HPI_MIXER_STORE_DISABLE = 5,
	HPI_MIXER_STORE_SAVE_SINGLE = 6
};


enum HPI_SWITCH_STATES {
	HPI_SWITCH_OFF = 0,	
	HPI_SWITCH_ON = 1	
};


#define HPI_UNITS_PER_dB                100
#define HPI_GAIN_OFF                    (-100 * HPI_UNITS_PER_dB)

#define HPI_BITMASK_ALL_CHANNELS        (0xFFFFFFFF)

#define HPI_METER_MINIMUM               (-150 * HPI_UNITS_PER_dB)

enum HPI_VOLUME_AUTOFADES {
	HPI_VOLUME_AUTOFADE_LOG = 2,
	HPI_VOLUME_AUTOFADE_LINEAR = 3
};

enum HPI_AESEBU_FORMATS {
	HPI_AESEBU_FORMAT_AESEBU = 1,
	HPI_AESEBU_FORMAT_SPDIF = 2
};

enum HPI_AESEBU_ERRORS {
	HPI_AESEBU_ERROR_NOT_LOCKED = 0x01,
	HPI_AESEBU_ERROR_POOR_QUALITY = 0x02,
	HPI_AESEBU_ERROR_PARITY_ERROR = 0x04,
	HPI_AESEBU_ERROR_BIPHASE_VIOLATION = 0x08,
	HPI_AESEBU_ERROR_VALIDITY = 0x10,
	HPI_AESEBU_ERROR_CRC = 0x20
};

#define HPI_PAD_CHANNEL_NAME_LEN        16
#define HPI_PAD_ARTIST_LEN              64
#define HPI_PAD_TITLE_LEN               64
#define HPI_PAD_COMMENT_LEN             256
#define HPI_PAD_PROGRAM_TYPE_INVALID    0xffff

enum eHPI_RDS_type {
	HPI_RDS_DATATYPE_RDS = 0,	
	HPI_RDS_DATATYPE_RBDS = 1	
};

enum HPI_TUNER_BAND {
	HPI_TUNER_BAND_AM = 1,	 
	HPI_TUNER_BAND_FM = 2,	 
	HPI_TUNER_BAND_TV_NTSC_M = 3,	 
	HPI_TUNER_BAND_TV = 3,	
	HPI_TUNER_BAND_FM_STEREO = 4,	 
	HPI_TUNER_BAND_AUX = 5,	 
	HPI_TUNER_BAND_TV_PAL_BG = 6,	 
	HPI_TUNER_BAND_TV_PAL_I = 7,	 
	HPI_TUNER_BAND_TV_PAL_DK = 8,	 
	HPI_TUNER_BAND_TV_SECAM_L = 9,	 
	HPI_TUNER_BAND_LAST = 9	
};

enum HPI_TUNER_MODES {
	HPI_TUNER_MODE_RSS = 1,	
	HPI_TUNER_MODE_RDS = 2	
};

enum HPI_TUNER_MODE_VALUES {
	HPI_TUNER_MODE_RSS_DISABLE = 0,	
	HPI_TUNER_MODE_RSS_ENABLE = 1,	

	HPI_TUNER_MODE_RDS_DISABLE = 0,	
	HPI_TUNER_MODE_RDS_RDS = 1,  
	HPI_TUNER_MODE_RDS_RBDS = 2 
};

enum HPI_TUNER_STATUS_BITS {
	HPI_TUNER_VIDEO_COLOR_PRESENT = 0x0001,	
	HPI_TUNER_VIDEO_IS_60HZ = 0x0020, 
	HPI_TUNER_VIDEO_HORZ_SYNC_MISSING = 0x0040, 
	HPI_TUNER_VIDEO_STATUS_VALID = 0x0100, 
	HPI_TUNER_DIGITAL = 0x0200, 
	HPI_TUNER_MULTIPROGRAM = 0x0400, 
	HPI_TUNER_PLL_LOCKED = 0x1000, 
	HPI_TUNER_FM_STEREO = 0x2000 
};

enum HPI_CHANNEL_MODES {
	HPI_CHANNEL_MODE_NORMAL = 1,
	HPI_CHANNEL_MODE_SWAP = 2,
	HPI_CHANNEL_MODE_LEFT_TO_STEREO = 3,
	HPI_CHANNEL_MODE_RIGHT_TO_STEREO = 4,
	HPI_CHANNEL_MODE_STEREO_TO_LEFT = 5,
	HPI_CHANNEL_MODE_STEREO_TO_RIGHT = 6,
	HPI_CHANNEL_MODE_LAST = 6
};

enum HPI_SAMPLECLOCK_SOURCES {
	HPI_SAMPLECLOCK_SOURCE_LOCAL = 1,
	HPI_SAMPLECLOCK_SOURCE_AESEBU_SYNC = 2,
	HPI_SAMPLECLOCK_SOURCE_WORD = 3,
	HPI_SAMPLECLOCK_SOURCE_WORD_HEADER = 4,
	HPI_SAMPLECLOCK_SOURCE_SMPTE = 5,
	HPI_SAMPLECLOCK_SOURCE_AESEBU_INPUT = 6,
	HPI_SAMPLECLOCK_SOURCE_NETWORK = 8,
	HPI_SAMPLECLOCK_SOURCE_PREV_MODULE = 10,
	HPI_SAMPLECLOCK_SOURCE_LAST = 10
};

enum HPI_FILTER_TYPE {
	HPI_FILTER_TYPE_BYPASS = 0,	

	HPI_FILTER_TYPE_LOWSHELF = 1,	
	HPI_FILTER_TYPE_HIGHSHELF = 2,	
	HPI_FILTER_TYPE_EQ_BAND = 3,	

	HPI_FILTER_TYPE_LOWPASS = 4,	
	HPI_FILTER_TYPE_HIGHPASS = 5,	
	HPI_FILTER_TYPE_BANDPASS = 6,	
	HPI_FILTER_TYPE_BANDSTOP = 7	
};

enum ASYNC_EVENT_SOURCES {
	HPI_ASYNC_EVENT_GPIO = 1,	
	HPI_ASYNC_EVENT_SILENCE = 2,	
	HPI_ASYNC_EVENT_TONE = 3	
};
enum HPI_ERROR_CODES {
	
	HPI_ERROR_INVALID_TYPE = 100,
	
	HPI_ERROR_INVALID_OBJ = 101,
	
	HPI_ERROR_INVALID_FUNC = 102,
	
	HPI_ERROR_INVALID_OBJ_INDEX = 103,
	
	HPI_ERROR_OBJ_NOT_OPEN = 104,
	
	HPI_ERROR_OBJ_ALREADY_OPEN = 105,
	
	HPI_ERROR_INVALID_RESOURCE = 106,
	
	
	HPI_ERROR_INVALID_RESPONSE = 108,
	HPI_ERROR_PROCESSING_MESSAGE = 109,
	
	HPI_ERROR_NETWORK_TIMEOUT = 110,
	
	HPI_ERROR_INVALID_HANDLE = 111,
	
	HPI_ERROR_UNIMPLEMENTED = 112,
	HPI_ERROR_NETWORK_TOO_MANY_CLIENTS = 113,
	HPI_ERROR_RESPONSE_BUFFER_TOO_SMALL = 114,
	
	HPI_ERROR_RESPONSE_MISMATCH = 115,
	
	HPI_ERROR_CONTROL_CACHING = 116,
	HPI_ERROR_MESSAGE_BUFFER_TOO_SMALL = 117,

	
	
	HPI_ERROR_BAD_ADAPTER = 201,
	
	HPI_ERROR_BAD_ADAPTER_NUMBER = 202,
	
	HPI_ERROR_DUPLICATE_ADAPTER_NUMBER = 203,
	
	HPI_ERROR_DSP_BOOTLOAD = 204,
	
	HPI_ERROR_DSP_FILE_NOT_FOUND = 206,
	
	HPI_ERROR_DSP_HARDWARE = 207,
	
	HPI_ERROR_MEMORY_ALLOC = 208,
	
	HPI_ERROR_PLD_LOAD = 209,
	
	HPI_ERROR_DSP_FILE_FORMAT = 210,

	
	HPI_ERROR_DSP_FILE_ACCESS_DENIED = 211,
	
	HPI_ERROR_DSP_FILE_NO_HEADER = 212,
	
	
	HPI_ERROR_DSP_SECTION_NOT_FOUND = 214,
	
	HPI_ERROR_DSP_FILE_OTHER_ERROR = 215,
	
	HPI_ERROR_DSP_FILE_SHARING_VIOLATION = 216,
	
	HPI_ERROR_DSP_FILE_NULL_HEADER = 217,

	

	
	HPI_ERROR_BAD_CHECKSUM = 221,
	HPI_ERROR_BAD_SEQUENCE = 222,
	HPI_ERROR_FLASH_ERASE = 223,
	HPI_ERROR_FLASH_PROGRAM = 224,
	HPI_ERROR_FLASH_VERIFY = 225,
	HPI_ERROR_FLASH_TYPE = 226,
	HPI_ERROR_FLASH_START = 227,
	HPI_ERROR_FLASH_READ = 228,
	HPI_ERROR_FLASH_READ_NO_FILE = 229,
	HPI_ERROR_FLASH_SIZE = 230,

	
	HPI_ERROR_RESERVED_1 = 290,

	
	
	HPI_ERROR_INVALID_FORMAT = 301,
	
	HPI_ERROR_INVALID_SAMPLERATE = 302,
	
	HPI_ERROR_INVALID_CHANNELS = 303,
	
	HPI_ERROR_INVALID_BITRATE = 304,
	
	HPI_ERROR_INVALID_DATASIZE = 305,
	
	
	
	HPI_ERROR_INVALID_DATA_POINTER = 308,
	
	HPI_ERROR_INVALID_PACKET_ORDER = 309,

	HPI_ERROR_INVALID_OPERATION = 310,

	HPI_ERROR_INCOMPATIBLE_SAMPLERATE = 311,
	
	HPI_ERROR_BAD_ADAPTER_MODE = 312,

	HPI_ERROR_TOO_MANY_CAPABILITY_CHANGE_ATTEMPTS = 313,
	
	HPI_ERROR_NO_INTERADAPTER_GROUPS = 314,
	
	HPI_ERROR_NO_INTERDSP_GROUPS = 315,
	
	HPI_ERROR_WAIT_CANCELLED = 316,
	
	HPI_ERROR_INVALID_STRING = 317,

	
	HPI_ERROR_INVALID_NODE = 400,
	
	HPI_ERROR_INVALID_CONTROL = 401,
	
	HPI_ERROR_INVALID_CONTROL_VALUE = 402,
	
	HPI_ERROR_INVALID_CONTROL_ATTRIBUTE = 403,
	
	HPI_ERROR_CONTROL_DISABLED = 404,
	
	HPI_ERROR_CONTROL_I2C_MISSING_ACK = 405,
	HPI_ERROR_I2C_MISSING_ACK = 405,
	HPI_ERROR_CONTROL_NOT_READY = 407,

	
	HPI_ERROR_NVMEM_BUSY = 450,
	HPI_ERROR_NVMEM_FULL = 451,
	HPI_ERROR_NVMEM_FAIL = 452,

	
	HPI_ERROR_I2C_BAD_ADR = 460,

	
	HPI_ERROR_ENTITY_TYPE_MISMATCH = 470,
	
	HPI_ERROR_ENTITY_ITEM_COUNT = 471,
	
	HPI_ERROR_ENTITY_TYPE_INVALID = 472,
	
	HPI_ERROR_ENTITY_ROLE_INVALID = 473,
	
	HPI_ERROR_ENTITY_SIZE_MISMATCH = 474,

	

	
	HPI_ERROR_CUSTOM = 600,

	
	HPI_ERROR_MUTEX_TIMEOUT = 700,

	HPI_ERROR_BACKEND_BASE = 900,

	
	HPI_ERROR_DSP_COMMUNICATION = 900
};

#define HPI_MAX_ADAPTERS                20
#define HPI_MAX_STREAMS                 16
#define HPI_MAX_CHANNELS                2	
#define HPI_MAX_NODES                   8	
#define HPI_MAX_CONTROLS                4	
#define HPI_MAX_ANC_BYTES_PER_FRAME     (64)
#define HPI_STRING_LEN                  16

#define HPI_MIN_NETWORK_ADAPTER_IDX 100

#define HPI_OSTREAM_VELOCITY_UNITS      4096
#define HPI_OSTREAM_TIMESCALE_UNITS     10000
#define HPI_OSTREAM_TIMESCALE_PASSTHROUGH       99999


#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(push, 1)
#endif

struct hpi_format {
	u32 sample_rate;
				
	u32 bit_rate;		  
	u32 attributes;
				
	u16 mode_legacy;
				
	u16 unused;		  
	u16 channels;	  
	u16 format;	  
};

struct hpi_anc_frame {
	u32 valid_bits_in_this_frame;
	u8 b_data[HPI_MAX_ANC_BYTES_PER_FRAME];
};

struct hpi_async_event {
	u16 event_type;	
	u16 sequence; 
	u32 state; 
	u32 h_object; 
	union {
		struct {
			u16 index; 
		} gpio;
		struct {
			u16 node_index;	
			u16 node_type; 
		} control;
	} u;
};

#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(pop)
#endif


u16 hpi_stream_estimate_buffer_size(struct hpi_format *pF,
	u32 host_polling_rate_in_milli_seconds, u32 *recommended_buffer_size);


u16 hpi_subsys_get_version_ex(u32 *pversion_ex);

u16 hpi_subsys_get_num_adapters(int *pn_num_adapters);

u16 hpi_subsys_get_adapter(int iterator, u32 *padapter_index,
	u16 *pw_adapter_type);


u16 hpi_adapter_open(u16 adapter_index);

u16 hpi_adapter_close(u16 adapter_index);

u16 hpi_adapter_get_info(u16 adapter_index, u16 *pw_num_outstreams,
	u16 *pw_num_instreams, u16 *pw_version, u32 *pserial_number,
	u16 *pw_adapter_type);

u16 hpi_adapter_get_module_by_index(u16 adapter_index, u16 module_index,
	u16 *pw_num_outputs, u16 *pw_num_inputs, u16 *pw_version,
	u32 *pserial_number, u16 *pw_module_type, u32 *ph_module);

u16 hpi_adapter_set_mode(u16 adapter_index, u32 adapter_mode);

u16 hpi_adapter_set_mode_ex(u16 adapter_index, u32 adapter_mode,
	u16 query_or_set);

u16 hpi_adapter_get_mode(u16 adapter_index, u32 *padapter_mode);

u16 hpi_adapter_get_assert2(u16 adapter_index, u16 *p_assert_count,
	char *psz_assert, u32 *p_param1, u32 *p_param2,
	u32 *p_dsp_string_addr, u16 *p_processor_id);

u16 hpi_adapter_test_assert(u16 adapter_index, u16 assert_id);

u16 hpi_adapter_enable_capability(u16 adapter_index, u16 capability, u32 key);

u16 hpi_adapter_self_test(u16 adapter_index);

u16 hpi_adapter_debug_read(u16 adapter_index, u32 dsp_address, char *p_bytes,
	int *count_bytes);

u16 hpi_adapter_set_property(u16 adapter_index, u16 property, u16 paramter1,
	u16 paramter2);

u16 hpi_adapter_get_property(u16 adapter_index, u16 property,
	u16 *pw_paramter1, u16 *pw_paramter2);

u16 hpi_adapter_enumerate_property(u16 adapter_index, u16 index,
	u16 what_to_enumerate, u16 property_index, u32 *psetting);
u16 hpi_outstream_open(u16 adapter_index, u16 outstream_index,
	u32 *ph_outstream);

u16 hpi_outstream_close(u32 h_outstream);

u16 hpi_outstream_get_info_ex(u32 h_outstream, u16 *pw_state,
	u32 *pbuffer_size, u32 *pdata_to_play, u32 *psamples_played,
	u32 *pauxiliary_data_to_play);

u16 hpi_outstream_write_buf(u32 h_outstream, const u8 *pb_write_buf,
	u32 bytes_to_write, const struct hpi_format *p_format);

u16 hpi_outstream_start(u32 h_outstream);

u16 hpi_outstream_wait_start(u32 h_outstream);

u16 hpi_outstream_stop(u32 h_outstream);

u16 hpi_outstream_sinegen(u32 h_outstream);

u16 hpi_outstream_reset(u32 h_outstream);

u16 hpi_outstream_query_format(u32 h_outstream, struct hpi_format *p_format);

u16 hpi_outstream_set_format(u32 h_outstream, struct hpi_format *p_format);

u16 hpi_outstream_set_punch_in_out(u32 h_outstream, u32 punch_in_sample,
	u32 punch_out_sample);

u16 hpi_outstream_set_velocity(u32 h_outstream, short velocity);

u16 hpi_outstream_ancillary_reset(u32 h_outstream, u16 mode);

u16 hpi_outstream_ancillary_get_info(u32 h_outstream, u32 *pframes_available);

u16 hpi_outstream_ancillary_read(u32 h_outstream,
	struct hpi_anc_frame *p_anc_frame_buffer,
	u32 anc_frame_buffer_size_in_bytes,
	u32 number_of_ancillary_frames_to_read);

u16 hpi_outstream_set_time_scale(u32 h_outstream, u32 time_scaleX10000);

u16 hpi_outstream_host_buffer_allocate(u32 h_outstream, u32 size_in_bytes);

u16 hpi_outstream_host_buffer_free(u32 h_outstream);

u16 hpi_outstream_group_add(u32 h_outstream, u32 h_stream);

u16 hpi_outstream_group_get_map(u32 h_outstream, u32 *poutstream_map,
	u32 *pinstream_map);

u16 hpi_outstream_group_reset(u32 h_outstream);

u16 hpi_instream_open(u16 adapter_index, u16 instream_index,
	u32 *ph_instream);

u16 hpi_instream_close(u32 h_instream);

u16 hpi_instream_query_format(u32 h_instream,
	const struct hpi_format *p_format);

u16 hpi_instream_set_format(u32 h_instream,
	const struct hpi_format *p_format);

u16 hpi_instream_read_buf(u32 h_instream, u8 *pb_read_buf, u32 bytes_to_read);

u16 hpi_instream_start(u32 h_instream);

u16 hpi_instream_wait_start(u32 h_instream);

u16 hpi_instream_stop(u32 h_instream);

u16 hpi_instream_reset(u32 h_instream);

u16 hpi_instream_get_info_ex(u32 h_instream, u16 *pw_state, u32 *pbuffer_size,
	u32 *pdata_recorded, u32 *psamples_recorded,
	u32 *pauxiliary_data_recorded);

u16 hpi_instream_ancillary_reset(u32 h_instream, u16 bytes_per_frame,
	u16 mode, u16 alignment, u16 idle_bit);

u16 hpi_instream_ancillary_get_info(u32 h_instream, u32 *pframe_space);

u16 hpi_instream_ancillary_write(u32 h_instream,
	const struct hpi_anc_frame *p_anc_frame_buffer,
	u32 anc_frame_buffer_size_in_bytes,
	u32 number_of_ancillary_frames_to_write);

u16 hpi_instream_host_buffer_allocate(u32 h_instream, u32 size_in_bytes);

u16 hpi_instream_host_buffer_free(u32 h_instream);

u16 hpi_instream_group_add(u32 h_instream, u32 h_stream);

u16 hpi_instream_group_get_map(u32 h_instream, u32 *poutstream_map,
	u32 *pinstream_map);

u16 hpi_instream_group_reset(u32 h_instream);

u16 hpi_mixer_open(u16 adapter_index, u32 *ph_mixer);

u16 hpi_mixer_close(u32 h_mixer);

u16 hpi_mixer_get_control(u32 h_mixer, u16 src_node_type,
	u16 src_node_type_index, u16 dst_node_type, u16 dst_node_type_index,
	u16 control_type, u32 *ph_control);

u16 hpi_mixer_get_control_by_index(u32 h_mixer, u16 control_index,
	u16 *pw_src_node_type, u16 *pw_src_node_index, u16 *pw_dst_node_type,
	u16 *pw_dst_node_index, u16 *pw_control_type, u32 *ph_control);

u16 hpi_mixer_store(u32 h_mixer, enum HPI_MIXER_STORE_COMMAND command,
	u16 index);
u16 hpi_volume_set_gain(u32 h_control, short an_gain0_01dB[HPI_MAX_CHANNELS]
	);

u16 hpi_volume_get_gain(u32 h_control,
	short an_gain0_01dB_out[HPI_MAX_CHANNELS]
	);

u16 hpi_volume_set_mute(u32 h_control, u32 mute);

u16 hpi_volume_get_mute(u32 h_control, u32 *mute);

#define hpi_volume_get_range hpi_volume_query_range
u16 hpi_volume_query_range(u32 h_control, short *min_gain_01dB,
	short *max_gain_01dB, short *step_gain_01dB);

u16 hpi_volume_query_channels(const u32 h_control, u32 *p_channels);

u16 hpi_volume_auto_fade(u32 h_control,
	short an_stop_gain0_01dB[HPI_MAX_CHANNELS], u32 duration_ms);

u16 hpi_volume_auto_fade_profile(u32 h_control,
	short an_stop_gain0_01dB[HPI_MAX_CHANNELS], u32 duration_ms,
	u16 profile);

u16 hpi_volume_query_auto_fade_profile(const u32 h_control, const u32 i,
	u16 *profile);

u16 hpi_level_query_range(u32 h_control, short *min_gain_01dB,
	short *max_gain_01dB, short *step_gain_01dB);

u16 hpi_level_set_gain(u32 h_control, short an_gain0_01dB[HPI_MAX_CHANNELS]
	);

u16 hpi_level_get_gain(u32 h_control,
	short an_gain0_01dB_out[HPI_MAX_CHANNELS]
	);

u16 hpi_meter_query_channels(const u32 h_meter, u32 *p_channels);

u16 hpi_meter_get_peak(u32 h_control,
	short an_peak0_01dB_out[HPI_MAX_CHANNELS]
	);

u16 hpi_meter_get_rms(u32 h_control, short an_peak0_01dB_out[HPI_MAX_CHANNELS]
	);

u16 hpi_meter_set_peak_ballistics(u32 h_control, u16 attack, u16 decay);

u16 hpi_meter_set_rms_ballistics(u32 h_control, u16 attack, u16 decay);

u16 hpi_meter_get_peak_ballistics(u32 h_control, u16 *attack, u16 *decay);

u16 hpi_meter_get_rms_ballistics(u32 h_control, u16 *attack, u16 *decay);

u16 hpi_channel_mode_query_mode(const u32 h_mode, const u32 index,
	u16 *pw_mode);

u16 hpi_channel_mode_set(u32 h_control, u16 mode);

u16 hpi_channel_mode_get(u32 h_control, u16 *mode);

u16 hpi_tuner_query_band(const u32 h_tuner, const u32 index, u16 *pw_band);

u16 hpi_tuner_set_band(u32 h_control, u16 band);

u16 hpi_tuner_get_band(u32 h_control, u16 *pw_band);

u16 hpi_tuner_query_frequency(const u32 h_tuner, const u32 index,
	const u16 band, u32 *pfreq);

u16 hpi_tuner_set_frequency(u32 h_control, u32 freq_ink_hz);

u16 hpi_tuner_get_frequency(u32 h_control, u32 *pw_freq_ink_hz);

u16 hpi_tuner_get_rf_level(u32 h_control, short *pw_level);

u16 hpi_tuner_get_raw_rf_level(u32 h_control, short *pw_level);

u16 hpi_tuner_query_gain(const u32 h_tuner, const u32 index, u16 *pw_gain);

u16 hpi_tuner_set_gain(u32 h_control, short gain);

u16 hpi_tuner_get_gain(u32 h_control, short *pn_gain);

u16 hpi_tuner_get_status(u32 h_control, u16 *pw_status_mask, u16 *pw_status);

u16 hpi_tuner_set_mode(u32 h_control, u32 mode, u32 value);

u16 hpi_tuner_get_mode(u32 h_control, u32 mode, u32 *pn_value);

u16 hpi_tuner_get_rds(u32 h_control, char *p_rds_data);

u16 hpi_tuner_query_deemphasis(const u32 h_tuner, const u32 index,
	const u16 band, u32 *pdeemphasis);

u16 hpi_tuner_set_deemphasis(u32 h_control, u32 deemphasis);
u16 hpi_tuner_get_deemphasis(u32 h_control, u32 *pdeemphasis);

u16 hpi_tuner_query_program(const u32 h_tuner, u32 *pbitmap_program);

u16 hpi_tuner_set_program(u32 h_control, u32 program);

u16 hpi_tuner_get_program(u32 h_control, u32 *pprogram);

u16 hpi_tuner_get_hd_radio_dsp_version(u32 h_control, char *psz_dsp_version,
	const u32 string_size);

u16 hpi_tuner_get_hd_radio_sdk_version(u32 h_control, char *psz_sdk_version,
	const u32 string_size);

u16 hpi_tuner_get_hd_radio_signal_quality(u32 h_control, u32 *pquality);

u16 hpi_tuner_get_hd_radio_signal_blend(u32 h_control, u32 *pblend);

u16 hpi_tuner_set_hd_radio_signal_blend(u32 h_control, const u32 blend);


u16 hpi_pad_get_channel_name(u32 h_control, char *psz_string,
	const u32 string_length);

u16 hpi_pad_get_artist(u32 h_control, char *psz_string,
	const u32 string_length);

u16 hpi_pad_get_title(u32 h_control, char *psz_string,
	const u32 string_length);

u16 hpi_pad_get_comment(u32 h_control, char *psz_string,
	const u32 string_length);

u16 hpi_pad_get_program_type(u32 h_control, u32 *ppTY);

u16 hpi_pad_get_rdsPI(u32 h_control, u32 *ppI);

u16 hpi_pad_get_program_type_string(u32 h_control, const u32 data_type,
	const u32 pTY, char *psz_string, const u32 string_length);

u16 hpi_aesebu_receiver_query_format(const u32 h_aes_rx, const u32 index,
	u16 *pw_format);

u16 hpi_aesebu_receiver_set_format(u32 h_control, u16 source);

u16 hpi_aesebu_receiver_get_format(u32 h_control, u16 *pw_source);

u16 hpi_aesebu_receiver_get_sample_rate(u32 h_control, u32 *psample_rate);

u16 hpi_aesebu_receiver_get_user_data(u32 h_control, u16 index, u16 *pw_data);

u16 hpi_aesebu_receiver_get_channel_status(u32 h_control, u16 index,
	u16 *pw_data);

u16 hpi_aesebu_receiver_get_error_status(u32 h_control, u16 *pw_error_data);

u16 hpi_aesebu_transmitter_set_sample_rate(u32 h_control, u32 sample_rate);

u16 hpi_aesebu_transmitter_set_user_data(u32 h_control, u16 index, u16 data);

u16 hpi_aesebu_transmitter_set_channel_status(u32 h_control, u16 index,
	u16 data);

u16 hpi_aesebu_transmitter_get_channel_status(u32 h_control, u16 index,
	u16 *pw_data);

u16 hpi_aesebu_transmitter_query_format(const u32 h_aes_tx, const u32 index,
	u16 *pw_format);

u16 hpi_aesebu_transmitter_set_format(u32 h_control, u16 output_format);

u16 hpi_aesebu_transmitter_get_format(u32 h_control, u16 *pw_output_format);

u16 hpi_multiplexer_set_source(u32 h_control, u16 source_node_type,
	u16 source_node_index);

u16 hpi_multiplexer_get_source(u32 h_control, u16 *source_node_type,
	u16 *source_node_index);

u16 hpi_multiplexer_query_source(u32 h_control, u16 index,
	u16 *source_node_type, u16 *source_node_index);

u16 hpi_vox_set_threshold(u32 h_control, short an_gain0_01dB);

u16 hpi_vox_get_threshold(u32 h_control, short *an_gain0_01dB);

u16 hpi_bitstream_set_clock_edge(u32 h_control, u16 edge_type);

u16 hpi_bitstream_set_data_polarity(u32 h_control, u16 polarity);

u16 hpi_bitstream_get_activity(u32 h_control, u16 *pw_clk_activity,
	u16 *pw_data_activity);


u16 hpi_sample_clock_query_source(const u32 h_clock, const u32 index,
	u16 *pw_source);

u16 hpi_sample_clock_set_source(u32 h_control, u16 source);

u16 hpi_sample_clock_get_source(u32 h_control, u16 *pw_source);

u16 hpi_sample_clock_query_source_index(const u32 h_clock, const u32 index,
	const u32 source, u16 *pw_source_index);

u16 hpi_sample_clock_set_source_index(u32 h_control, u16 source_index);

u16 hpi_sample_clock_get_source_index(u32 h_control, u16 *pw_source_index);

u16 hpi_sample_clock_get_sample_rate(u32 h_control, u32 *psample_rate);

u16 hpi_sample_clock_query_local_rate(const u32 h_clock, const u32 index,
	u32 *psource);

u16 hpi_sample_clock_set_local_rate(u32 h_control, u32 sample_rate);

u16 hpi_sample_clock_get_local_rate(u32 h_control, u32 *psample_rate);

u16 hpi_sample_clock_set_auto(u32 h_control, u32 enable);

u16 hpi_sample_clock_get_auto(u32 h_control, u32 *penable);

u16 hpi_sample_clock_set_local_rate_lock(u32 h_control, u32 lock);

u16 hpi_sample_clock_get_local_rate_lock(u32 h_control, u32 *plock);

u16 hpi_microphone_set_phantom_power(u32 h_control, u16 on_off);

u16 hpi_microphone_get_phantom_power(u32 h_control, u16 *pw_on_off);

u16 hpi_parametric_eq_get_info(u32 h_control, u16 *pw_number_of_bands,
	u16 *pw_enabled);

u16 hpi_parametric_eq_set_state(u32 h_control, u16 on_off);

u16 hpi_parametric_eq_set_band(u32 h_control, u16 index, u16 type,
	u32 frequency_hz, short q100, short gain0_01dB);

u16 hpi_parametric_eq_get_band(u32 h_control, u16 index, u16 *pn_type,
	u32 *pfrequency_hz, short *pnQ100, short *pn_gain0_01dB);

u16 hpi_parametric_eq_get_coeffs(u32 h_control, u16 index, short coeffs[5]
	);


u16 hpi_compander_set_enable(u32 h_control, u32 on);

u16 hpi_compander_get_enable(u32 h_control, u32 *pon);

u16 hpi_compander_set_makeup_gain(u32 h_control, short makeup_gain0_01dB);

u16 hpi_compander_get_makeup_gain(u32 h_control, short *pn_makeup_gain0_01dB);

u16 hpi_compander_set_attack_time_constant(u32 h_control, u32 index,
	u32 attack);

u16 hpi_compander_get_attack_time_constant(u32 h_control, u32 index,
	u32 *pw_attack);

u16 hpi_compander_set_decay_time_constant(u32 h_control, u32 index,
	u32 decay);

u16 hpi_compander_get_decay_time_constant(u32 h_control, u32 index,
	u32 *pw_decay);

u16 hpi_compander_set_threshold(u32 h_control, u32 index,
	short threshold0_01dB);

u16 hpi_compander_get_threshold(u32 h_control, u32 index,
	short *pn_threshold0_01dB);

u16 hpi_compander_set_ratio(u32 h_control, u32 index, u32 ratio100);

u16 hpi_compander_get_ratio(u32 h_control, u32 index, u32 *pw_ratio100);

u16 hpi_cobranet_hmi_write(u32 h_control, u32 hmi_address, u32 byte_count,
	u8 *pb_data);

u16 hpi_cobranet_hmi_read(u32 h_control, u32 hmi_address, u32 max_byte_count,
	u32 *pbyte_count, u8 *pb_data);

u16 hpi_cobranet_hmi_get_status(u32 h_control, u32 *pstatus,
	u32 *preadable_size, u32 *pwriteable_size);

u16 hpi_cobranet_get_ip_address(u32 h_control, u32 *pdw_ip_address);

u16 hpi_cobranet_set_ip_address(u32 h_control, u32 dw_ip_address);

u16 hpi_cobranet_get_static_ip_address(u32 h_control, u32 *pdw_ip_address);

u16 hpi_cobranet_set_static_ip_address(u32 h_control, u32 dw_ip_address);

u16 hpi_cobranet_get_macaddress(u32 h_control, u32 *p_mac_msbs,
	u32 *p_mac_lsbs);

u16 hpi_tone_detector_get_state(u32 hC, u32 *state);

u16 hpi_tone_detector_set_enable(u32 hC, u32 enable);

u16 hpi_tone_detector_get_enable(u32 hC, u32 *enable);

u16 hpi_tone_detector_set_event_enable(u32 hC, u32 event_enable);

u16 hpi_tone_detector_get_event_enable(u32 hC, u32 *event_enable);

u16 hpi_tone_detector_set_threshold(u32 hC, int threshold);

u16 hpi_tone_detector_get_threshold(u32 hC, int *threshold);

u16 hpi_tone_detector_get_frequency(u32 hC, u32 index, u32 *frequency);

u16 hpi_silence_detector_get_state(u32 hC, u32 *state);

u16 hpi_silence_detector_set_enable(u32 hC, u32 enable);

u16 hpi_silence_detector_get_enable(u32 hC, u32 *enable);

u16 hpi_silence_detector_set_event_enable(u32 hC, u32 event_enable);

u16 hpi_silence_detector_get_event_enable(u32 hC, u32 *event_enable);

u16 hpi_silence_detector_set_delay(u32 hC, u32 delay);

u16 hpi_silence_detector_get_delay(u32 hC, u32 *delay);

u16 hpi_silence_detector_set_threshold(u32 hC, int threshold);

u16 hpi_silence_detector_get_threshold(u32 hC, int *threshold);

u16 hpi_format_create(struct hpi_format *p_format, u16 channels, u16 format,
	u32 sample_rate, u32 bit_rate, u32 attributes);

#endif	 
