/*
    V4L2 controls framework implementation.

    Copyright (C) 2010  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-dev.h>

#define has_op(master, op) \
	(master->ops && master->ops->op)
#define call_op(master, op) \
	(has_op(master, op) ? master->ops->op(master) : 0)

struct v4l2_ctrl_helper {
	
	struct v4l2_ctrl_ref *mref;
	
	struct v4l2_ctrl *ctrl;
	u32 next;
};

static bool is_cur_manual(const struct v4l2_ctrl *master)
{
	return master->is_auto && master->cur.val == master->manual_mode_value;
}

static bool is_new_manual(const struct v4l2_ctrl *master)
{
	return master->is_auto && master->val == master->manual_mode_value;
}

const char * const *v4l2_ctrl_get_menu(u32 id)
{
	static const char * const mpeg_audio_sampling_freq[] = {
		"44.1 kHz",
		"48 kHz",
		"32 kHz",
		NULL
	};
	static const char * const mpeg_audio_encoding[] = {
		"MPEG-1/2 Layer I",
		"MPEG-1/2 Layer II",
		"MPEG-1/2 Layer III",
		"MPEG-2/4 AAC",
		"AC-3",
		NULL
	};
	static const char * const mpeg_audio_l1_bitrate[] = {
		"32 kbps",
		"64 kbps",
		"96 kbps",
		"128 kbps",
		"160 kbps",
		"192 kbps",
		"224 kbps",
		"256 kbps",
		"288 kbps",
		"320 kbps",
		"352 kbps",
		"384 kbps",
		"416 kbps",
		"448 kbps",
		NULL
	};
	static const char * const mpeg_audio_l2_bitrate[] = {
		"32 kbps",
		"48 kbps",
		"56 kbps",
		"64 kbps",
		"80 kbps",
		"96 kbps",
		"112 kbps",
		"128 kbps",
		"160 kbps",
		"192 kbps",
		"224 kbps",
		"256 kbps",
		"320 kbps",
		"384 kbps",
		NULL
	};
	static const char * const mpeg_audio_l3_bitrate[] = {
		"32 kbps",
		"40 kbps",
		"48 kbps",
		"56 kbps",
		"64 kbps",
		"80 kbps",
		"96 kbps",
		"112 kbps",
		"128 kbps",
		"160 kbps",
		"192 kbps",
		"224 kbps",
		"256 kbps",
		"320 kbps",
		NULL
	};
	static const char * const mpeg_audio_ac3_bitrate[] = {
		"32 kbps",
		"40 kbps",
		"48 kbps",
		"56 kbps",
		"64 kbps",
		"80 kbps",
		"96 kbps",
		"112 kbps",
		"128 kbps",
		"160 kbps",
		"192 kbps",
		"224 kbps",
		"256 kbps",
		"320 kbps",
		"384 kbps",
		"448 kbps",
		"512 kbps",
		"576 kbps",
		"640 kbps",
		NULL
	};
	static const char * const mpeg_audio_mode[] = {
		"Stereo",
		"Joint Stereo",
		"Dual",
		"Mono",
		NULL
	};
	static const char * const mpeg_audio_mode_extension[] = {
		"Bound 4",
		"Bound 8",
		"Bound 12",
		"Bound 16",
		NULL
	};
	static const char * const mpeg_audio_emphasis[] = {
		"No Emphasis",
		"50/15 us",
		"CCITT J17",
		NULL
	};
	static const char * const mpeg_audio_crc[] = {
		"No CRC",
		"16-bit CRC",
		NULL
	};
	static const char * const mpeg_audio_dec_playback[] = {
		"Auto",
		"Stereo",
		"Left",
		"Right",
		"Mono",
		"Swapped Stereo",
		NULL
	};
	static const char * const mpeg_video_encoding[] = {
		"MPEG-1",
		"MPEG-2",
		"MPEG-4 AVC",
		NULL
	};
	static const char * const mpeg_video_aspect[] = {
		"1x1",
		"4x3",
		"16x9",
		"2.21x1",
		NULL
	};
	static const char * const mpeg_video_bitrate_mode[] = {
		"Variable Bitrate",
		"Constant Bitrate",
		NULL
	};
	static const char * const mpeg_stream_type[] = {
		"MPEG-2 Program Stream",
		"MPEG-2 Transport Stream",
		"MPEG-1 System Stream",
		"MPEG-2 DVD-compatible Stream",
		"MPEG-1 VCD-compatible Stream",
		"MPEG-2 SVCD-compatible Stream",
		NULL
	};
	static const char * const mpeg_stream_vbi_fmt[] = {
		"No VBI",
		"Private Packet, IVTV Format",
		NULL
	};
	static const char * const camera_power_line_frequency[] = {
		"Disabled",
		"50 Hz",
		"60 Hz",
		"Auto",
		NULL
	};
	static const char * const camera_exposure_auto[] = {
		"Auto Mode",
		"Manual Mode",
		"Shutter Priority Mode",
		"Aperture Priority Mode",
		NULL
	};
	static const char * const colorfx[] = {
		"None",
		"Black & White",
		"Sepia",
		"Negative",
		"Emboss",
		"Sketch",
		"Sky Blue",
		"Grass Green",
		"Skin Whiten",
		"Vivid",
		NULL
	};
	static const char * const tune_preemphasis[] = {
		"No Preemphasis",
		"50 Microseconds",
		"75 Microseconds",
		NULL,
	};
	static const char * const header_mode[] = {
		"Separate Buffer",
		"Joined With 1st Frame",
		"Joined With I-Frames",
		NULL,
	};
	static const char * const multi_slice[] = {
		"Single",
		"Max Macroblocks",
		"Max Bytes",
		"GOB",
		NULL,
	};
	static const char * const entropy_mode[] = {
		"CAVLC",
		"CABAC",
		NULL,
	};
	static const char * const mpeg_h264_level[] = {
		"1",
		"1b",
		"1.1",
		"1.2",
		"1.3",
		"2",
		"2.1",
		"2.2",
		"3",
		"3.1",
		"3.2",
		"4",
		"4.1",
		"4.2",
		"5",
		"5.1",
		"5.2",
		NULL,
	};
	static const char * const h264_loop_filter[] = {
		"Enabled",
		"Disabled",
		"Disabled at Slice Boundary",
		NULL,
	};
	static const char * const h264_profile[] = {
		"Baseline",
		"Constrained Baseline",
		"Main",
		"Extended",
		"High",
		"High 10",
		"High 422",
		"High 444 Predictive",
		"High 10 Intra",
		"High 422 Intra",
		"High 444 Intra",
		"CAVLC 444 Intra",
		"Scalable Baseline",
		"Scalable High",
		"Scalable High Intra",
		"Multiview High",
		"Constrained High",
		NULL,
	};
	static const char * const vui_sar_idc[] = {
		"Unspecified",
		"1:1",
		"12:11",
		"10:11",
		"16:11",
		"40:33",
		"24:11",
		"20:11",
		"32:11",
		"80:33",
		"18:11",
		"15:11",
		"64:33",
		"160:99",
		"4:3",
		"3:2",
		"2:1",
		"Extended SAR",
		NULL,
	};
	static const char * const mpeg_mpeg4_level[] = {
		"0",
		"0b",
		"1",
		"2",
		"3",
		"3b",
		"4",
		"5",
		NULL,
	};
	static const char * const mpeg4_profile[] = {
		"Simple",
		"Advanced Simple",
		"Core",
		"Simple Scalable",
		"Advanced Coding Efficency",
		NULL,
	};

	static const char * const flash_led_mode[] = {
		"Off",
		"Flash",
		"Torch",
		NULL,
	};
	static const char * const flash_strobe_source[] = {
		"Software",
		"External",
		NULL,
	};

	static const char * const jpeg_chroma_subsampling[] = {
		"4:4:4",
		"4:2:2",
		"4:2:0",
		"4:1:1",
		"4:1:0",
		"Gray",
		NULL,
	};
	static const char *const mpeg_video_intra_refresh_mode[] = {
		"No Intra Refresh",
		"AIR MBS",
		"AIR REF",
		"CIR MBS",
		NULL
	};

	switch (id) {
	case V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ:
		return mpeg_audio_sampling_freq;
	case V4L2_CID_MPEG_AUDIO_ENCODING:
		return mpeg_audio_encoding;
	case V4L2_CID_MPEG_AUDIO_L1_BITRATE:
		return mpeg_audio_l1_bitrate;
	case V4L2_CID_MPEG_AUDIO_L2_BITRATE:
		return mpeg_audio_l2_bitrate;
	case V4L2_CID_MPEG_AUDIO_L3_BITRATE:
		return mpeg_audio_l3_bitrate;
	case V4L2_CID_MPEG_AUDIO_AC3_BITRATE:
		return mpeg_audio_ac3_bitrate;
	case V4L2_CID_MPEG_AUDIO_MODE:
		return mpeg_audio_mode;
	case V4L2_CID_MPEG_AUDIO_MODE_EXTENSION:
		return mpeg_audio_mode_extension;
	case V4L2_CID_MPEG_AUDIO_EMPHASIS:
		return mpeg_audio_emphasis;
	case V4L2_CID_MPEG_AUDIO_CRC:
		return mpeg_audio_crc;
	case V4L2_CID_MPEG_AUDIO_DEC_PLAYBACK:
	case V4L2_CID_MPEG_AUDIO_DEC_MULTILINGUAL_PLAYBACK:
		return mpeg_audio_dec_playback;
	case V4L2_CID_MPEG_VIDEO_ENCODING:
		return mpeg_video_encoding;
	case V4L2_CID_MPEG_VIDEO_ASPECT:
		return mpeg_video_aspect;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		return mpeg_video_bitrate_mode;
	case V4L2_CID_MPEG_STREAM_TYPE:
		return mpeg_stream_type;
	case V4L2_CID_MPEG_STREAM_VBI_FMT:
		return mpeg_stream_vbi_fmt;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		return camera_power_line_frequency;
	case V4L2_CID_EXPOSURE_AUTO:
		return camera_exposure_auto;
	case V4L2_CID_COLORFX:
		return colorfx;
	case V4L2_CID_TUNE_PREEMPHASIS:
		return tune_preemphasis;
	case V4L2_CID_FLASH_LED_MODE:
		return flash_led_mode;
	case V4L2_CID_FLASH_STROBE_SOURCE:
		return flash_strobe_source;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		return header_mode;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		return multi_slice;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		return entropy_mode;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		return mpeg_h264_level;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		return h264_loop_filter;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		return h264_profile;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC:
		return vui_sar_idc;
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		return mpeg_mpeg4_level;
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		return mpeg4_profile;
	case V4L2_CID_JPEG_CHROMA_SUBSAMPLING:
		return jpeg_chroma_subsampling;
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE:
		return mpeg_video_intra_refresh_mode;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(v4l2_ctrl_get_menu);

const char *v4l2_ctrl_get_name(u32 id)
{
	switch (id) {
	
	
	case V4L2_CID_USER_CLASS:		return "User Controls";
	case V4L2_CID_BRIGHTNESS:		return "Brightness";
	case V4L2_CID_CONTRAST:			return "Contrast";
	case V4L2_CID_SATURATION:		return "Saturation";
	case V4L2_CID_HUE:			return "Hue";
	case V4L2_CID_AUDIO_VOLUME:		return "Volume";
	case V4L2_CID_AUDIO_BALANCE:		return "Balance";
	case V4L2_CID_AUDIO_BASS:		return "Bass";
	case V4L2_CID_AUDIO_TREBLE:		return "Treble";
	case V4L2_CID_AUDIO_MUTE:		return "Mute";
	case V4L2_CID_AUDIO_LOUDNESS:		return "Loudness";
	case V4L2_CID_BLACK_LEVEL:		return "Black Level";
	case V4L2_CID_AUTO_WHITE_BALANCE:	return "White Balance, Automatic";
	case V4L2_CID_DO_WHITE_BALANCE:		return "Do White Balance";
	case V4L2_CID_RED_BALANCE:		return "Red Balance";
	case V4L2_CID_BLUE_BALANCE:		return "Blue Balance";
	case V4L2_CID_GAMMA:			return "Gamma";
	case V4L2_CID_EXPOSURE:			return "Exposure";
	case V4L2_CID_AUTOGAIN:			return "Gain, Automatic";
	case V4L2_CID_GAIN:			return "Gain";
	case V4L2_CID_HFLIP:			return "Horizontal Flip";
	case V4L2_CID_VFLIP:			return "Vertical Flip";
	case V4L2_CID_HCENTER:			return "Horizontal Center";
	case V4L2_CID_VCENTER:			return "Vertical Center";
	case V4L2_CID_POWER_LINE_FREQUENCY:	return "Power Line Frequency";
	case V4L2_CID_HUE_AUTO:			return "Hue, Automatic";
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE: return "White Balance Temperature";
	case V4L2_CID_SHARPNESS:		return "Sharpness";
	case V4L2_CID_BACKLIGHT_COMPENSATION:	return "Backlight Compensation";
	case V4L2_CID_CHROMA_AGC:		return "Chroma AGC";
	case V4L2_CID_COLOR_KILLER:		return "Color Killer";
	case V4L2_CID_COLORFX:			return "Color Effects";
	case V4L2_CID_AUTOBRIGHTNESS:		return "Brightness, Automatic";
	case V4L2_CID_BAND_STOP_FILTER:		return "Band-Stop Filter";
	case V4L2_CID_ROTATE:			return "Rotate";
	case V4L2_CID_BG_COLOR:			return "Background Color";
	case V4L2_CID_CHROMA_GAIN:		return "Chroma Gain";
	case V4L2_CID_ILLUMINATORS_1:		return "Illuminator 1";
	case V4L2_CID_ILLUMINATORS_2:		return "Illuminator 2";
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:	return "Min Number of Capture Buffers";
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:	return "Min Number of Output Buffers";
	case V4L2_CID_ALPHA_COMPONENT:		return "Alpha Component";

	
	
	case V4L2_CID_MPEG_CLASS:		return "MPEG Encoder Controls";
	case V4L2_CID_MPEG_STREAM_TYPE:		return "Stream Type";
	case V4L2_CID_MPEG_STREAM_PID_PMT:	return "Stream PMT Program ID";
	case V4L2_CID_MPEG_STREAM_PID_AUDIO:	return "Stream Audio Program ID";
	case V4L2_CID_MPEG_STREAM_PID_VIDEO:	return "Stream Video Program ID";
	case V4L2_CID_MPEG_STREAM_PID_PCR:	return "Stream PCR Program ID";
	case V4L2_CID_MPEG_STREAM_PES_ID_AUDIO: return "Stream PES Audio ID";
	case V4L2_CID_MPEG_STREAM_PES_ID_VIDEO: return "Stream PES Video ID";
	case V4L2_CID_MPEG_STREAM_VBI_FMT:	return "Stream VBI Format";
	case V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ: return "Audio Sampling Frequency";
	case V4L2_CID_MPEG_AUDIO_ENCODING:	return "Audio Encoding";
	case V4L2_CID_MPEG_AUDIO_L1_BITRATE:	return "Audio Layer I Bitrate";
	case V4L2_CID_MPEG_AUDIO_L2_BITRATE:	return "Audio Layer II Bitrate";
	case V4L2_CID_MPEG_AUDIO_L3_BITRATE:	return "Audio Layer III Bitrate";
	case V4L2_CID_MPEG_AUDIO_MODE:		return "Audio Stereo Mode";
	case V4L2_CID_MPEG_AUDIO_MODE_EXTENSION: return "Audio Stereo Mode Extension";
	case V4L2_CID_MPEG_AUDIO_EMPHASIS:	return "Audio Emphasis";
	case V4L2_CID_MPEG_AUDIO_CRC:		return "Audio CRC";
	case V4L2_CID_MPEG_AUDIO_MUTE:		return "Audio Mute";
	case V4L2_CID_MPEG_AUDIO_AAC_BITRATE:	return "Audio AAC Bitrate";
	case V4L2_CID_MPEG_AUDIO_AC3_BITRATE:	return "Audio AC-3 Bitrate";
	case V4L2_CID_MPEG_AUDIO_DEC_PLAYBACK:	return "Audio Playback";
	case V4L2_CID_MPEG_AUDIO_DEC_MULTILINGUAL_PLAYBACK: return "Audio Multilingual Playback";
	case V4L2_CID_MPEG_VIDEO_ENCODING:	return "Video Encoding";
	case V4L2_CID_MPEG_VIDEO_ASPECT:	return "Video Aspect";
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:	return "Video B Frames";
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:	return "Video GOP Size";
	case V4L2_CID_MPEG_VIDEO_GOP_CLOSURE:	return "Video GOP Closure";
	case V4L2_CID_MPEG_VIDEO_PULLDOWN:	return "Video Pulldown";
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:	return "Video Bitrate Mode";
	case V4L2_CID_MPEG_VIDEO_BITRATE:	return "Video Bitrate";
	case V4L2_CID_MPEG_VIDEO_BITRATE_PEAK:	return "Video Peak Bitrate";
	case V4L2_CID_MPEG_VIDEO_TEMPORAL_DECIMATION: return "Video Temporal Decimation";
	case V4L2_CID_MPEG_VIDEO_MUTE:		return "Video Mute";
	case V4L2_CID_MPEG_VIDEO_MUTE_YUV:	return "Video Mute YUV";
	case V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE:	return "Decoder Slice Interface";
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:	return "MPEG4 Loop Filter Enable";
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:	return "Number of Intra Refresh MBs";
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:		return "Frame Level Rate Control Enable";
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:			return "H264 MB Level Rate Control";
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:			return "Sequence Header Mode";
	case V4L2_CID_MPEG_VIDEO_MAX_REF_PIC:			return "Max Number of Reference Pics";
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:		return "H263 I-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:		return "H263 P-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP:		return "H263 B-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:			return "H263 Minimum QP Value";
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:			return "H263 Maximum QP Value";
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:		return "H264 I-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:		return "H264 P-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:		return "H264 B-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:			return "H264 Maximum QP Value";
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:			return "H264 Minimum QP Value";
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:		return "H264 8x8 Transform Enable";
	case V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE:			return "H264 CPB Buffer Size";
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:		return "H264 Entropy Mode";
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:			return "H264 I-Frame Period";
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:			return "H264 Level";
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:	return "H264 Loop Filter Alpha Offset";
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:		return "H264 Loop Filter Beta Offset";
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:		return "H264 Loop Filter Mode";
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:			return "H264 Profile";
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:	return "Vertical Size of SAR";
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:	return "Horizontal Size of SAR";
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE:		return "Aspect Ratio VUI Enable";
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC:		return "VUI Aspect Ratio IDC";
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:		return "MPEG4 I-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:		return "MPEG4 P-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:		return "MPEG4 B-Frame QP Value";
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:			return "MPEG4 Minimum QP Value";
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:			return "MPEG4 Maximum QP Value";
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:			return "MPEG4 Level";
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:			return "MPEG4 Profile";
	case V4L2_CID_MPEG_VIDEO_MPEG4_QPEL:			return "Quarter Pixel Search Enable";
	case V4L2_CID_QCOM_VIDEO_SYNC_FRAME_SEQ_HDR:
		return "CodecConfig with sync frame";
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:		return "Maximum Bytes in a Slice";
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:		return "Number of MBs in a Slice";
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:		return "Slice Partitioning Method";
	case V4L2_CID_MPEG_VIDEO_VBV_SIZE:			return "VBV Buffer Size";
	case V4L2_CID_MPEG_VIDEO_DEC_PTS:			return "Video Decoder PTS";
	case V4L2_CID_MPEG_VIDEO_DEC_FRAME:			return "Video Decoder Frame Count";
	case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION: return "Rotation";
	case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL: return "Rate Control";
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL: return "CABAC Model";
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE:
		return "Intra Refresh Mode";
	case V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS: return "Intra Refresh AIR MBS";
	case V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF: return "Intra Refresh AIR REF";
	case V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS: return "Intra Refresh CIR MBS";
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		return "VP8 Profile Level";
	case V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE:
		return "Deinterlace for encoder";

	
	
	case V4L2_CID_CAMERA_CLASS:		return "Camera Controls";
	case V4L2_CID_EXPOSURE_AUTO:		return "Auto Exposure";
	case V4L2_CID_EXPOSURE_ABSOLUTE:	return "Exposure Time, Absolute";
	case V4L2_CID_EXPOSURE_AUTO_PRIORITY:	return "Exposure, Dynamic Framerate";
	case V4L2_CID_PAN_RELATIVE:		return "Pan, Relative";
	case V4L2_CID_TILT_RELATIVE:		return "Tilt, Relative";
	case V4L2_CID_PAN_RESET:		return "Pan, Reset";
	case V4L2_CID_TILT_RESET:		return "Tilt, Reset";
	case V4L2_CID_PAN_ABSOLUTE:		return "Pan, Absolute";
	case V4L2_CID_TILT_ABSOLUTE:		return "Tilt, Absolute";
	case V4L2_CID_FOCUS_ABSOLUTE:		return "Focus, Absolute";
	case V4L2_CID_FOCUS_RELATIVE:		return "Focus, Relative";
	case V4L2_CID_FOCUS_AUTO:		return "Focus, Automatic";
	case V4L2_CID_ZOOM_ABSOLUTE:		return "Zoom, Absolute";
	case V4L2_CID_ZOOM_RELATIVE:		return "Zoom, Relative";
	case V4L2_CID_ZOOM_CONTINUOUS:		return "Zoom, Continuous";
	case V4L2_CID_PRIVACY:			return "Privacy";
	case V4L2_CID_IRIS_ABSOLUTE:		return "Iris, Absolute";
	case V4L2_CID_IRIS_RELATIVE:		return "Iris, Relative";

	
	
	case V4L2_CID_FM_TX_CLASS:		return "FM Radio Modulator Controls";
	case V4L2_CID_RDS_TX_DEVIATION:		return "RDS Signal Deviation";
	case V4L2_CID_RDS_TX_PI:		return "RDS Program ID";
	case V4L2_CID_RDS_TX_PTY:		return "RDS Program Type";
	case V4L2_CID_RDS_TX_PS_NAME:		return "RDS PS Name";
	case V4L2_CID_RDS_TX_RADIO_TEXT:	return "RDS Radio Text";
	case V4L2_CID_AUDIO_LIMITER_ENABLED:	return "Audio Limiter Feature Enabled";
	case V4L2_CID_AUDIO_LIMITER_RELEASE_TIME: return "Audio Limiter Release Time";
	case V4L2_CID_AUDIO_LIMITER_DEVIATION:	return "Audio Limiter Deviation";
	case V4L2_CID_AUDIO_COMPRESSION_ENABLED: return "Audio Compression Enabled";
	case V4L2_CID_AUDIO_COMPRESSION_GAIN:	return "Audio Compression Gain";
	case V4L2_CID_AUDIO_COMPRESSION_THRESHOLD: return "Audio Compression Threshold";
	case V4L2_CID_AUDIO_COMPRESSION_ATTACK_TIME: return "Audio Compression Attack Time";
	case V4L2_CID_AUDIO_COMPRESSION_RELEASE_TIME: return "Audio Compression Release Time";
	case V4L2_CID_PILOT_TONE_ENABLED:	return "Pilot Tone Feature Enabled";
	case V4L2_CID_PILOT_TONE_DEVIATION:	return "Pilot Tone Deviation";
	case V4L2_CID_PILOT_TONE_FREQUENCY:	return "Pilot Tone Frequency";
	case V4L2_CID_TUNE_PREEMPHASIS:		return "Pre-Emphasis";
	case V4L2_CID_TUNE_POWER_LEVEL:		return "Tune Power Level";
	case V4L2_CID_TUNE_ANTENNA_CAPACITOR:	return "Tune Antenna Capacitor";

	
	case V4L2_CID_FLASH_CLASS:		return "Flash Controls";
	case V4L2_CID_FLASH_LED_MODE:		return "LED Mode";
	case V4L2_CID_FLASH_STROBE_SOURCE:	return "Strobe Source";
	case V4L2_CID_FLASH_STROBE:		return "Strobe";
	case V4L2_CID_FLASH_STROBE_STOP:	return "Stop Strobe";
	case V4L2_CID_FLASH_STROBE_STATUS:	return "Strobe Status";
	case V4L2_CID_FLASH_TIMEOUT:		return "Strobe Timeout";
	case V4L2_CID_FLASH_INTENSITY:		return "Intensity, Flash Mode";
	case V4L2_CID_FLASH_TORCH_INTENSITY:	return "Intensity, Torch Mode";
	case V4L2_CID_FLASH_INDICATOR_INTENSITY: return "Intensity, Indicator";
	case V4L2_CID_FLASH_FAULT:		return "Faults";
	case V4L2_CID_FLASH_CHARGE:		return "Charge";
	case V4L2_CID_FLASH_READY:		return "Ready to Strobe";

	
	
	case V4L2_CID_JPEG_CLASS:		return "JPEG Compression Controls";
	case V4L2_CID_JPEG_CHROMA_SUBSAMPLING:	return "Chroma Subsampling";
	case V4L2_CID_JPEG_RESTART_INTERVAL:	return "Restart Interval";
	case V4L2_CID_JPEG_COMPRESSION_QUALITY:	return "Compression Quality";
	case V4L2_CID_JPEG_ACTIVE_MARKER:	return "Active Markers";

	default:
		return NULL;
	}
}
EXPORT_SYMBOL(v4l2_ctrl_get_name);

void v4l2_ctrl_fill(u32 id, const char **name, enum v4l2_ctrl_type *type,
		    s32 *min, s32 *max, s32 *step, s32 *def, u32 *flags)
{
	*name = v4l2_ctrl_get_name(id);
	*flags = 0;

	switch (id) {
	case V4L2_CID_AUDIO_MUTE:
	case V4L2_CID_AUDIO_LOUDNESS:
	case V4L2_CID_AUTO_WHITE_BALANCE:
	case V4L2_CID_AUTOGAIN:
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_HUE_AUTO:
	case V4L2_CID_CHROMA_AGC:
	case V4L2_CID_COLOR_KILLER:
	case V4L2_CID_MPEG_AUDIO_MUTE:
	case V4L2_CID_MPEG_VIDEO_MUTE:
	case V4L2_CID_MPEG_VIDEO_GOP_CLOSURE:
	case V4L2_CID_MPEG_VIDEO_PULLDOWN:
	case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
	case V4L2_CID_FOCUS_AUTO:
	case V4L2_CID_PRIVACY:
	case V4L2_CID_AUDIO_LIMITER_ENABLED:
	case V4L2_CID_AUDIO_COMPRESSION_ENABLED:
	case V4L2_CID_PILOT_TONE_ENABLED:
	case V4L2_CID_ILLUMINATORS_1:
	case V4L2_CID_ILLUMINATORS_2:
	case V4L2_CID_FLASH_STROBE_STATUS:
	case V4L2_CID_FLASH_CHARGE:
	case V4L2_CID_FLASH_READY:
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
	case V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE:
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE:
	case V4L2_CID_MPEG_VIDEO_MPEG4_QPEL:
		*type = V4L2_CTRL_TYPE_BOOLEAN;
		*min = 0;
		*max = *step = 1;
		break;
	case V4L2_CID_PAN_RESET:
	case V4L2_CID_TILT_RESET:
	case V4L2_CID_FLASH_STROBE:
	case V4L2_CID_FLASH_STROBE_STOP:
		*type = V4L2_CTRL_TYPE_BUTTON;
		*flags |= V4L2_CTRL_FLAG_WRITE_ONLY;
		*min = *max = *step = *def = 0;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
	case V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ:
	case V4L2_CID_MPEG_AUDIO_ENCODING:
	case V4L2_CID_MPEG_AUDIO_L1_BITRATE:
	case V4L2_CID_MPEG_AUDIO_L2_BITRATE:
	case V4L2_CID_MPEG_AUDIO_L3_BITRATE:
	case V4L2_CID_MPEG_AUDIO_AC3_BITRATE:
	case V4L2_CID_MPEG_AUDIO_MODE:
	case V4L2_CID_MPEG_AUDIO_MODE_EXTENSION:
	case V4L2_CID_MPEG_AUDIO_EMPHASIS:
	case V4L2_CID_MPEG_AUDIO_CRC:
	case V4L2_CID_MPEG_AUDIO_DEC_PLAYBACK:
	case V4L2_CID_MPEG_AUDIO_DEC_MULTILINGUAL_PLAYBACK:
	case V4L2_CID_MPEG_VIDEO_ENCODING:
	case V4L2_CID_MPEG_VIDEO_ASPECT:
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	case V4L2_CID_MPEG_STREAM_TYPE:
	case V4L2_CID_MPEG_STREAM_VBI_FMT:
	case V4L2_CID_EXPOSURE_AUTO:
	case V4L2_CID_COLORFX:
	case V4L2_CID_TUNE_PREEMPHASIS:
	case V4L2_CID_FLASH_LED_MODE:
	case V4L2_CID_FLASH_STROBE_SOURCE:
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC:
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_JPEG_CHROMA_SUBSAMPLING:
	case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION:
	case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL:
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL:
		*type = V4L2_CTRL_TYPE_MENU;
		break;
	case V4L2_CID_RDS_TX_PS_NAME:
	case V4L2_CID_RDS_TX_RADIO_TEXT:
		*type = V4L2_CTRL_TYPE_STRING;
		break;
	case V4L2_CID_USER_CLASS:
	case V4L2_CID_CAMERA_CLASS:
	case V4L2_CID_MPEG_CLASS:
	case V4L2_CID_FM_TX_CLASS:
	case V4L2_CID_FLASH_CLASS:
	case V4L2_CID_JPEG_CLASS:
		*type = V4L2_CTRL_TYPE_CTRL_CLASS;
		
		*flags |= V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY;
		*min = *max = *step = *def = 0;
		break;
	case V4L2_CID_BG_COLOR:
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	*type = V4L2_CTRL_TYPE_INTEGER;
		*step = 1;
		*min = 0;
		
		*max = 0xFFFFFF;
		break;
	case V4L2_CID_FLASH_FAULT:
	case V4L2_CID_JPEG_ACTIVE_MARKER:
		*type = V4L2_CTRL_TYPE_BITMASK;
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		*type = V4L2_CTRL_TYPE_INTEGER;
		*flags |= V4L2_CTRL_FLAG_READ_ONLY;
		break;
	case V4L2_CID_MPEG_VIDEO_DEC_FRAME:
	case V4L2_CID_MPEG_VIDEO_DEC_PTS:
		*type = V4L2_CTRL_TYPE_INTEGER64;
		*flags |= V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
		break;
	case V4L2_CID_QCOM_VIDEO_SYNC_FRAME_SEQ_HDR:
		*type = V4L2_CTRL_TYPE_BOOLEAN;
		*min = 0;
		*max = *step = 1;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE:
		*type = V4L2_CTRL_TYPE_BOOLEAN;
		*min = 0;
		*max = *step = 1;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
		*type = V4L2_CTRL_TYPE_BOOLEAN;
		*min = 0;
		*max = *step = 1;
		break;
	default:
		*type = V4L2_CTRL_TYPE_INTEGER;
		break;
	}
	switch (id) {
	case V4L2_CID_MPEG_AUDIO_ENCODING:
	case V4L2_CID_MPEG_AUDIO_MODE:
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
	case V4L2_CID_MPEG_STREAM_TYPE:
		*flags |= V4L2_CTRL_FLAG_UPDATE;
		break;
	case V4L2_CID_AUDIO_VOLUME:
	case V4L2_CID_AUDIO_BALANCE:
	case V4L2_CID_AUDIO_BASS:
	case V4L2_CID_AUDIO_TREBLE:
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_RED_BALANCE:
	case V4L2_CID_BLUE_BALANCE:
	case V4L2_CID_GAMMA:
	case V4L2_CID_SHARPNESS:
	case V4L2_CID_CHROMA_GAIN:
	case V4L2_CID_RDS_TX_DEVIATION:
	case V4L2_CID_AUDIO_LIMITER_RELEASE_TIME:
	case V4L2_CID_AUDIO_LIMITER_DEVIATION:
	case V4L2_CID_AUDIO_COMPRESSION_GAIN:
	case V4L2_CID_AUDIO_COMPRESSION_THRESHOLD:
	case V4L2_CID_AUDIO_COMPRESSION_ATTACK_TIME:
	case V4L2_CID_AUDIO_COMPRESSION_RELEASE_TIME:
	case V4L2_CID_PILOT_TONE_DEVIATION:
	case V4L2_CID_PILOT_TONE_FREQUENCY:
	case V4L2_CID_TUNE_POWER_LEVEL:
	case V4L2_CID_TUNE_ANTENNA_CAPACITOR:
		*flags |= V4L2_CTRL_FLAG_SLIDER;
		break;
	case V4L2_CID_PAN_RELATIVE:
	case V4L2_CID_TILT_RELATIVE:
	case V4L2_CID_FOCUS_RELATIVE:
	case V4L2_CID_IRIS_RELATIVE:
	case V4L2_CID_ZOOM_RELATIVE:
		*flags |= V4L2_CTRL_FLAG_WRITE_ONLY;
		break;
	case V4L2_CID_FLASH_STROBE_STATUS:
	case V4L2_CID_FLASH_READY:
		*flags |= V4L2_CTRL_FLAG_READ_ONLY;
		break;
	}
}
EXPORT_SYMBOL(v4l2_ctrl_fill);

static bool type_is_int(const struct v4l2_ctrl *ctrl)
{
	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER64:
	case V4L2_CTRL_TYPE_STRING:
		
		return false;
	default:
		return true;
	}
}

static void fill_event(struct v4l2_event *ev, struct v4l2_ctrl *ctrl, u32 changes)
{
	memset(ev->reserved, 0, sizeof(ev->reserved));
	ev->type = V4L2_EVENT_CTRL;
	ev->id = ctrl->id;
	ev->u.ctrl.changes = changes;
	ev->u.ctrl.type = ctrl->type;
	ev->u.ctrl.flags = ctrl->flags;
	if (ctrl->type == V4L2_CTRL_TYPE_STRING)
		ev->u.ctrl.value64 = 0;
	else
		ev->u.ctrl.value64 = ctrl->cur.val64;
	ev->u.ctrl.minimum = ctrl->minimum;
	ev->u.ctrl.maximum = ctrl->maximum;
	if (ctrl->type == V4L2_CTRL_TYPE_MENU)
		ev->u.ctrl.step = 1;
	else
		ev->u.ctrl.step = ctrl->step;
	ev->u.ctrl.default_value = ctrl->default_value;
}

static void send_event(struct v4l2_fh *fh, struct v4l2_ctrl *ctrl, u32 changes)
{
	struct v4l2_event ev;
	struct v4l2_subscribed_event *sev;

	if (list_empty(&ctrl->ev_subs))
		return;
	fill_event(&ev, ctrl, changes);

	list_for_each_entry(sev, &ctrl->ev_subs, node)
		if (sev->fh != fh ||
		    (sev->flags & V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK))
			v4l2_event_queue_fh(sev->fh, &ev);
}

static int cur_to_user(struct v4l2_ext_control *c,
		       struct v4l2_ctrl *ctrl)
{
	u32 len;

	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_STRING:
		len = strlen(ctrl->cur.string);
		if (c->size < len + 1) {
			c->size = len + 1;
			return -ENOSPC;
		}
		return copy_to_user(c->string, ctrl->cur.string,
						len + 1) ? -EFAULT : 0;
	case V4L2_CTRL_TYPE_INTEGER64:
		c->value64 = ctrl->cur.val64;
		break;
	default:
		c->value = ctrl->cur.val;
		break;
	}
	return 0;
}

static int user_to_new(struct v4l2_ext_control *c,
		       struct v4l2_ctrl *ctrl)
{
	int ret;
	u32 size;

	ctrl->is_new = 1;
	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER64:
		ctrl->val64 = c->value64;
		break;
	case V4L2_CTRL_TYPE_STRING:
		size = c->size;
		if (size == 0)
			return -ERANGE;
		if (size > ctrl->maximum + 1)
			size = ctrl->maximum + 1;
		ret = copy_from_user(ctrl->string, c->string, size);
		if (!ret) {
			char last = ctrl->string[size - 1];

			ctrl->string[size - 1] = 0;
			if (strlen(ctrl->string) == ctrl->maximum && last)
				return -ERANGE;
		}
		return ret ? -EFAULT : 0;
	default:
		ctrl->val = c->value;
		break;
	}
	return 0;
}

static int new_to_user(struct v4l2_ext_control *c,
		       struct v4l2_ctrl *ctrl)
{
	u32 len;

	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_STRING:
		len = strlen(ctrl->string);
		if (c->size < len + 1) {
			c->size = ctrl->maximum + 1;
			return -ENOSPC;
		}
		return copy_to_user(c->string, ctrl->string,
						len + 1) ? -EFAULT : 0;
	case V4L2_CTRL_TYPE_INTEGER64:
		c->value64 = ctrl->val64;
		break;
	default:
		c->value = ctrl->val;
		break;
	}
	return 0;
}

static void new_to_cur(struct v4l2_fh *fh, struct v4l2_ctrl *ctrl,
						bool update_inactive)
{
	bool changed = false;

	if (ctrl == NULL)
		return;
	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_BUTTON:
		changed = true;
		break;
	case V4L2_CTRL_TYPE_STRING:
		
		changed = strcmp(ctrl->string, ctrl->cur.string);
		strcpy(ctrl->cur.string, ctrl->string);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		changed = ctrl->val64 != ctrl->cur.val64;
		ctrl->cur.val64 = ctrl->val64;
		break;
	default:
		changed = ctrl->val != ctrl->cur.val;
		ctrl->cur.val = ctrl->val;
		break;
	}
	if (update_inactive) {
		
		ctrl->flags &=
			~(V4L2_CTRL_FLAG_INACTIVE | V4L2_CTRL_FLAG_VOLATILE);
		if (!is_cur_manual(ctrl->cluster[0])) {
			ctrl->flags |= V4L2_CTRL_FLAG_INACTIVE;
			if (ctrl->cluster[0]->has_volatiles)
				ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
		}
		fh = NULL;
	}
	if (changed || update_inactive) {
		if (!ctrl->is_new)
			fh = NULL;
		send_event(fh, ctrl,
			(changed ? V4L2_EVENT_CTRL_CH_VALUE : 0) |
			(update_inactive ? V4L2_EVENT_CTRL_CH_FLAGS : 0));
	}
}

static void cur_to_new(struct v4l2_ctrl *ctrl)
{
	if (ctrl == NULL)
		return;
	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_STRING:
		
		strcpy(ctrl->string, ctrl->cur.string);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		ctrl->val64 = ctrl->cur.val64;
		break;
	default:
		ctrl->val = ctrl->cur.val;
		break;
	}
}

static int cluster_changed(struct v4l2_ctrl *master)
{
	int diff = 0;
	int i;

	for (i = 0; !diff && i < master->ncontrols; i++) {
		struct v4l2_ctrl *ctrl = master->cluster[i];

		if (ctrl == NULL)
			continue;
		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_BUTTON:
			
			return 1;
		case V4L2_CTRL_TYPE_STRING:
			
			diff = strcmp(ctrl->string, ctrl->cur.string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			diff = ctrl->val64 != ctrl->cur.val64;
			break;
		default:
			diff = ctrl->val != ctrl->cur.val;
			break;
		}
	}
	return diff;
}

static int validate_new_int(const struct v4l2_ctrl *ctrl, s32 *pval)
{
	s32 val = *pval;
	u32 offset;

	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		
		val += ctrl->step / 2;
		if (val < ctrl->minimum)
			val = ctrl->minimum;
		if (val > ctrl->maximum)
			val = ctrl->maximum;
		offset = val - ctrl->minimum;
		offset = ctrl->step * (offset / ctrl->step);
		val = ctrl->minimum + offset;
		*pval = val;
		return 0;

	case V4L2_CTRL_TYPE_BOOLEAN:
		*pval = !!val;
		return 0;

	case V4L2_CTRL_TYPE_MENU:
		if (val < ctrl->minimum || val > ctrl->maximum)
			return -ERANGE;
		if (ctrl->qmenu[val][0] == '\0' ||
		    (ctrl->menu_skip_mask & (1 << val)))
			return -EINVAL;
		return 0;

	case V4L2_CTRL_TYPE_BITMASK:
		*pval &= ctrl->maximum;
		return 0;

	case V4L2_CTRL_TYPE_BUTTON:
	case V4L2_CTRL_TYPE_CTRL_CLASS:
		*pval = 0;
		return 0;

	default:
		return -EINVAL;
	}
}

static int validate_new(const struct v4l2_ctrl *ctrl, struct v4l2_ext_control *c)
{
	char *s = c->string;
	size_t len;

	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_BITMASK:
	case V4L2_CTRL_TYPE_BUTTON:
	case V4L2_CTRL_TYPE_CTRL_CLASS:
		return validate_new_int(ctrl, &c->value);

	case V4L2_CTRL_TYPE_INTEGER64:
		return 0;

	case V4L2_CTRL_TYPE_STRING:
		len = strlen(s);
		if (len < ctrl->minimum)
			return -ERANGE;
		if ((len - ctrl->minimum) % ctrl->step)
			return -ERANGE;
		return 0;

	default:
		return -EINVAL;
	}
}

static inline u32 node2id(struct list_head *node)
{
	return list_entry(node, struct v4l2_ctrl_ref, node)->ctrl->id;
}

static inline int handler_set_err(struct v4l2_ctrl_handler *hdl, int err)
{
	if (hdl->error == 0)
		hdl->error = err;
	return err;
}

int v4l2_ctrl_handler_init(struct v4l2_ctrl_handler *hdl,
			   unsigned nr_of_controls_hint)
{
	mutex_init(&hdl->lock);
	INIT_LIST_HEAD(&hdl->ctrls);
	INIT_LIST_HEAD(&hdl->ctrl_refs);
	hdl->nr_of_buckets = 1 + nr_of_controls_hint / 8;
	hdl->buckets = kcalloc(hdl->nr_of_buckets, sizeof(hdl->buckets[0]),
			       GFP_KERNEL);
	hdl->error = hdl->buckets ? 0 : -ENOMEM;
	return hdl->error;
}
EXPORT_SYMBOL(v4l2_ctrl_handler_init);

void v4l2_ctrl_handler_free(struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl_ref *ref, *next_ref;
	struct v4l2_ctrl *ctrl, *next_ctrl;
	struct v4l2_subscribed_event *sev, *next_sev;

	if (hdl == NULL || hdl->buckets == NULL)
		return;

	mutex_lock(&hdl->lock);
	
	list_for_each_entry_safe(ref, next_ref, &hdl->ctrl_refs, node) {
		list_del(&ref->node);
		kfree(ref);
	}
	
	list_for_each_entry_safe(ctrl, next_ctrl, &hdl->ctrls, node) {
		list_del(&ctrl->node);
		list_for_each_entry_safe(sev, next_sev, &ctrl->ev_subs, node)
			list_del(&sev->node);
		kfree(ctrl);
	}
	kfree(hdl->buckets);
	hdl->buckets = NULL;
	hdl->cached = NULL;
	hdl->error = 0;
	mutex_unlock(&hdl->lock);
}
EXPORT_SYMBOL(v4l2_ctrl_handler_free);

static struct v4l2_ctrl_ref *find_private_ref(
		struct v4l2_ctrl_handler *hdl, u32 id)
{
	struct v4l2_ctrl_ref *ref;

	id -= V4L2_CID_PRIVATE_BASE;
	list_for_each_entry(ref, &hdl->ctrl_refs, node) {
		if (V4L2_CTRL_ID2CLASS(ref->ctrl->id) == V4L2_CTRL_CLASS_USER &&
		    V4L2_CTRL_DRIVER_PRIV(ref->ctrl->id)) {
			if (!type_is_int(ref->ctrl))
				continue;
			if (id == 0)
				return ref;
			id--;
		}
	}
	return NULL;
}

static struct v4l2_ctrl_ref *find_ref(struct v4l2_ctrl_handler *hdl, u32 id)
{
	struct v4l2_ctrl_ref *ref;
	int bucket;

	id &= V4L2_CTRL_ID_MASK;

	
	if (id >= V4L2_CID_PRIVATE_BASE)
		return find_private_ref(hdl, id);
	bucket = id % hdl->nr_of_buckets;

	
	if (hdl->cached && hdl->cached->ctrl->id == id)
		return hdl->cached;

	
	ref = hdl->buckets ? hdl->buckets[bucket] : NULL;
	while (ref && ref->ctrl->id != id)
		ref = ref->next;

	if (ref)
		hdl->cached = ref; 
	return ref;
}

static struct v4l2_ctrl_ref *find_ref_lock(
		struct v4l2_ctrl_handler *hdl, u32 id)
{
	struct v4l2_ctrl_ref *ref = NULL;

	if (hdl) {
		mutex_lock(&hdl->lock);
		ref = find_ref(hdl, id);
		mutex_unlock(&hdl->lock);
	}
	return ref;
}

struct v4l2_ctrl *v4l2_ctrl_find(struct v4l2_ctrl_handler *hdl, u32 id)
{
	struct v4l2_ctrl_ref *ref = find_ref_lock(hdl, id);

	return ref ? ref->ctrl : NULL;
}
EXPORT_SYMBOL(v4l2_ctrl_find);

static int handler_new_ref(struct v4l2_ctrl_handler *hdl,
			   struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl_ref *ref;
	struct v4l2_ctrl_ref *new_ref;
	u32 id = ctrl->id;
	u32 class_ctrl = V4L2_CTRL_ID2CLASS(id) | 1;
	int bucket = id % hdl->nr_of_buckets;	

	
	if (id != class_ctrl && find_ref_lock(hdl, class_ctrl) == NULL)
		if (!v4l2_ctrl_new_std(hdl, NULL, class_ctrl, 0, 0, 0, 0))
			return hdl->error;

	if (hdl->error)
		return hdl->error;

	new_ref = kzalloc(sizeof(*new_ref), GFP_KERNEL);
	if (!new_ref)
		return handler_set_err(hdl, -ENOMEM);
	new_ref->ctrl = ctrl;
	if (ctrl->handler == hdl) {
		ctrl->cluster = &new_ref->ctrl;
		ctrl->ncontrols = 1;
	}

	INIT_LIST_HEAD(&new_ref->node);

	mutex_lock(&hdl->lock);

	if (list_empty(&hdl->ctrl_refs) || id > node2id(hdl->ctrl_refs.prev)) {
		list_add_tail(&new_ref->node, &hdl->ctrl_refs);
		goto insert_in_hash;
	}

	
	list_for_each_entry(ref, &hdl->ctrl_refs, node) {
		if (ref->ctrl->id < id)
			continue;
		
		if (ref->ctrl->id == id) {
			kfree(new_ref);
			goto unlock;
		}
		list_add(&new_ref->node, ref->node.prev);
		break;
	}

insert_in_hash:
	
	new_ref->next = hdl->buckets[bucket];
	hdl->buckets[bucket] = new_ref;

unlock:
	mutex_unlock(&hdl->lock);
	return 0;
}

static struct v4l2_ctrl *v4l2_ctrl_new(struct v4l2_ctrl_handler *hdl,
			const struct v4l2_ctrl_ops *ops,
			u32 id, const char *name, enum v4l2_ctrl_type type,
			s32 min, s32 max, u32 step, s32 def,
			u32 flags, const char * const *qmenu, void *priv)
{
	struct v4l2_ctrl *ctrl;
	unsigned sz_extra = 0;

	if (hdl->error)
		return NULL;

	
	if (id == 0 || name == NULL || id >= V4L2_CID_PRIVATE_BASE ||
	    (type == V4L2_CTRL_TYPE_INTEGER && step == 0) ||
	    (type == V4L2_CTRL_TYPE_BITMASK && max == 0) ||
	    (type == V4L2_CTRL_TYPE_MENU && qmenu == NULL) ||
	    (type == V4L2_CTRL_TYPE_STRING && max == 0)) {
		handler_set_err(hdl, -ERANGE);
		return NULL;
	}
	if (type != V4L2_CTRL_TYPE_BITMASK && max < min) {
		handler_set_err(hdl, -ERANGE);
		return NULL;
	}
	if ((type == V4L2_CTRL_TYPE_INTEGER ||
	     type == V4L2_CTRL_TYPE_MENU ||
	     type == V4L2_CTRL_TYPE_BOOLEAN) &&
	    (def < min || def > max)) {
		handler_set_err(hdl, -ERANGE);
		return NULL;
	}
	if (type == V4L2_CTRL_TYPE_BITMASK && ((def & ~max) || min || step)) {
		handler_set_err(hdl, -ERANGE);
		return NULL;
	}

	if (type == V4L2_CTRL_TYPE_BUTTON)
		flags |= V4L2_CTRL_FLAG_WRITE_ONLY;
	else if (type == V4L2_CTRL_TYPE_CTRL_CLASS)
		flags |= V4L2_CTRL_FLAG_READ_ONLY;
	else if (type == V4L2_CTRL_TYPE_STRING)
		sz_extra += 2 * (max + 1);

	ctrl = kzalloc(sizeof(*ctrl) + sz_extra, GFP_KERNEL);
	if (ctrl == NULL) {
		handler_set_err(hdl, -ENOMEM);
		return NULL;
	}

	INIT_LIST_HEAD(&ctrl->node);
	INIT_LIST_HEAD(&ctrl->ev_subs);
	ctrl->handler = hdl;
	ctrl->ops = ops;
	ctrl->id = id;
	ctrl->name = name;
	ctrl->type = type;
	ctrl->flags = flags;
	ctrl->minimum = min;
	ctrl->maximum = max;
	ctrl->step = step;
	ctrl->qmenu = qmenu;
	ctrl->priv = priv;
	ctrl->cur.val = ctrl->val = ctrl->default_value = def;

	if (ctrl->type == V4L2_CTRL_TYPE_STRING) {
		ctrl->cur.string = (char *)&ctrl[1] + sz_extra - (max + 1);
		ctrl->string = (char *)&ctrl[1] + sz_extra - 2 * (max + 1);
		if (ctrl->minimum)
			memset(ctrl->cur.string, ' ', ctrl->minimum);
	}
	if (handler_new_ref(hdl, ctrl)) {
		kfree(ctrl);
		return NULL;
	}
	mutex_lock(&hdl->lock);
	list_add_tail(&ctrl->node, &hdl->ctrls);
	mutex_unlock(&hdl->lock);
	return ctrl;
}

struct v4l2_ctrl *v4l2_ctrl_new_custom(struct v4l2_ctrl_handler *hdl,
			const struct v4l2_ctrl_config *cfg, void *priv)
{
	bool is_menu;
	struct v4l2_ctrl *ctrl;
	const char *name = cfg->name;
	const char * const *qmenu = cfg->qmenu;
	enum v4l2_ctrl_type type = cfg->type;
	u32 flags = cfg->flags;
	s32 min = cfg->min;
	s32 max = cfg->max;
	u32 step = cfg->step;
	s32 def = cfg->def;

	if (name == NULL)
		v4l2_ctrl_fill(cfg->id, &name, &type, &min, &max, &step,
								&def, &flags);

	is_menu = (cfg->type == V4L2_CTRL_TYPE_MENU);
	if (is_menu)
		WARN_ON(step);
	else
		WARN_ON(cfg->menu_skip_mask);
	if (is_menu && qmenu == NULL)
		qmenu = v4l2_ctrl_get_menu(cfg->id);

	ctrl = v4l2_ctrl_new(hdl, cfg->ops, cfg->id, name,
			type, min, max,
			is_menu ? cfg->menu_skip_mask : step,
			def, flags, qmenu, priv);
	if (ctrl)
		ctrl->is_private = cfg->is_private;
	return ctrl;
}
EXPORT_SYMBOL(v4l2_ctrl_new_custom);

struct v4l2_ctrl *v4l2_ctrl_new_std(struct v4l2_ctrl_handler *hdl,
			const struct v4l2_ctrl_ops *ops,
			u32 id, s32 min, s32 max, u32 step, s32 def)
{
	const char *name;
	enum v4l2_ctrl_type type;
	u32 flags;

	v4l2_ctrl_fill(id, &name, &type, &min, &max, &step, &def, &flags);
	if (type == V4L2_CTRL_TYPE_MENU) {
		handler_set_err(hdl, -EINVAL);
		return NULL;
	}
	return v4l2_ctrl_new(hdl, ops, id, name, type,
				    min, max, step, def, flags, NULL, NULL);
}
EXPORT_SYMBOL(v4l2_ctrl_new_std);

struct v4l2_ctrl *v4l2_ctrl_new_std_menu(struct v4l2_ctrl_handler *hdl,
			const struct v4l2_ctrl_ops *ops,
			u32 id, s32 max, s32 mask, s32 def)
{
	const char * const *qmenu = v4l2_ctrl_get_menu(id);
	const char *name;
	enum v4l2_ctrl_type type;
	s32 min;
	s32 step;
	u32 flags;

	v4l2_ctrl_fill(id, &name, &type, &min, &max, &step, &def, &flags);
	if (type != V4L2_CTRL_TYPE_MENU) {
		handler_set_err(hdl, -EINVAL);
		return NULL;
	}
	return v4l2_ctrl_new(hdl, ops, id, name, type,
				    0, max, mask, def, flags, qmenu, NULL);
}
EXPORT_SYMBOL(v4l2_ctrl_new_std_menu);

struct v4l2_ctrl *v4l2_ctrl_add_ctrl(struct v4l2_ctrl_handler *hdl,
					  struct v4l2_ctrl *ctrl)
{
	if (hdl == NULL || hdl->error)
		return NULL;
	if (ctrl == NULL) {
		handler_set_err(hdl, -EINVAL);
		return NULL;
	}
	if (ctrl->handler == hdl)
		return ctrl;
	return handler_new_ref(hdl, ctrl) ? NULL : ctrl;
}
EXPORT_SYMBOL(v4l2_ctrl_add_ctrl);

int v4l2_ctrl_add_handler(struct v4l2_ctrl_handler *hdl,
			  struct v4l2_ctrl_handler *add)
{
	struct v4l2_ctrl_ref *ref;
	int ret = 0;

	
	if (!hdl || !add || hdl == add)
		return 0;
	if (hdl->error)
		return hdl->error;
	mutex_lock(&add->lock);
	list_for_each_entry(ref, &add->ctrl_refs, node) {
		struct v4l2_ctrl *ctrl = ref->ctrl;

		
		if (ctrl->is_private)
			continue;
		
		if (ctrl->type == V4L2_CTRL_TYPE_CTRL_CLASS)
			continue;
		ret = handler_new_ref(hdl, ctrl);
		if (ret)
			break;
	}
	mutex_unlock(&add->lock);
	return ret;
}
EXPORT_SYMBOL(v4l2_ctrl_add_handler);

void v4l2_ctrl_cluster(unsigned ncontrols, struct v4l2_ctrl **controls)
{
	bool has_volatiles = false;
	int i;

	
	BUG_ON(ncontrols == 0 || controls[0] == NULL);

	for (i = 0; i < ncontrols; i++) {
		if (controls[i]) {
			controls[i]->cluster = controls;
			controls[i]->ncontrols = ncontrols;
			if (controls[i]->flags & V4L2_CTRL_FLAG_VOLATILE)
				has_volatiles = true;
		}
	}
	controls[0]->has_volatiles = has_volatiles;
}
EXPORT_SYMBOL(v4l2_ctrl_cluster);

void v4l2_ctrl_auto_cluster(unsigned ncontrols, struct v4l2_ctrl **controls,
			    u8 manual_val, bool set_volatile)
{
	struct v4l2_ctrl *master = controls[0];
	u32 flag = 0;
	int i;

	v4l2_ctrl_cluster(ncontrols, controls);
	WARN_ON(ncontrols <= 1);
	WARN_ON(manual_val < master->minimum || manual_val > master->maximum);
	WARN_ON(set_volatile && !has_op(master, g_volatile_ctrl));
	master->is_auto = true;
	master->has_volatiles = set_volatile;
	master->manual_mode_value = manual_val;
	master->flags |= V4L2_CTRL_FLAG_UPDATE;

	if (!is_cur_manual(master))
		flag = V4L2_CTRL_FLAG_INACTIVE |
			(set_volatile ? V4L2_CTRL_FLAG_VOLATILE : 0);

	for (i = 1; i < ncontrols; i++)
		if (controls[i])
			controls[i]->flags |= flag;
}
EXPORT_SYMBOL(v4l2_ctrl_auto_cluster);

void v4l2_ctrl_activate(struct v4l2_ctrl *ctrl, bool active)
{
	
	bool inactive = !active;
	bool old;

	if (ctrl == NULL)
		return;

	if (inactive)
		
		old = test_and_set_bit(4, &ctrl->flags);
	else
		
		old = test_and_clear_bit(4, &ctrl->flags);
	if (old != inactive)
		send_event(NULL, ctrl, V4L2_EVENT_CTRL_CH_FLAGS);
}
EXPORT_SYMBOL(v4l2_ctrl_activate);

void v4l2_ctrl_grab(struct v4l2_ctrl *ctrl, bool grabbed)
{
	bool old;

	if (ctrl == NULL)
		return;

	v4l2_ctrl_lock(ctrl);
	if (grabbed)
		
		old = test_and_set_bit(1, &ctrl->flags);
	else
		
		old = test_and_clear_bit(1, &ctrl->flags);
	if (old != grabbed)
		send_event(NULL, ctrl, V4L2_EVENT_CTRL_CH_FLAGS);
	v4l2_ctrl_unlock(ctrl);
}
EXPORT_SYMBOL(v4l2_ctrl_grab);

static void log_ctrl(const struct v4l2_ctrl *ctrl,
		     const char *prefix, const char *colon)
{
	if (ctrl->flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_WRITE_ONLY))
		return;
	if (ctrl->type == V4L2_CTRL_TYPE_CTRL_CLASS)
		return;

	printk(KERN_INFO "%s%s%s: ", prefix, colon, ctrl->name);

	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		printk(KERN_CONT "%d", ctrl->cur.val);
		break;
	case V4L2_CTRL_TYPE_BOOLEAN:
		printk(KERN_CONT "%s", ctrl->cur.val ? "true" : "false");
		break;
	case V4L2_CTRL_TYPE_MENU:
		printk(KERN_CONT "%s", ctrl->qmenu[ctrl->cur.val]);
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		printk(KERN_CONT "0x%08x", ctrl->cur.val);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		printk(KERN_CONT "%lld", ctrl->cur.val64);
		break;
	case V4L2_CTRL_TYPE_STRING:
		printk(KERN_CONT "%s", ctrl->cur.string);
		break;
	default:
		printk(KERN_CONT "unknown type %d", ctrl->type);
		break;
	}
	if (ctrl->flags & (V4L2_CTRL_FLAG_INACTIVE |
			   V4L2_CTRL_FLAG_GRABBED |
			   V4L2_CTRL_FLAG_VOLATILE)) {
		if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
			printk(KERN_CONT " inactive");
		if (ctrl->flags & V4L2_CTRL_FLAG_GRABBED)
			printk(KERN_CONT " grabbed");
		if (ctrl->flags & V4L2_CTRL_FLAG_VOLATILE)
			printk(KERN_CONT " volatile");
	}
	printk(KERN_CONT "\n");
}

void v4l2_ctrl_handler_log_status(struct v4l2_ctrl_handler *hdl,
				  const char *prefix)
{
	struct v4l2_ctrl *ctrl;
	const char *colon = "";
	int len;

	if (hdl == NULL)
		return;
	if (prefix == NULL)
		prefix = "";
	len = strlen(prefix);
	if (len && prefix[len - 1] != ' ')
		colon = ": ";
	mutex_lock(&hdl->lock);
	list_for_each_entry(ctrl, &hdl->ctrls, node)
		if (!(ctrl->flags & V4L2_CTRL_FLAG_DISABLED))
			log_ctrl(ctrl, prefix, colon);
	mutex_unlock(&hdl->lock);
}
EXPORT_SYMBOL(v4l2_ctrl_handler_log_status);

int v4l2_ctrl_handler_setup(struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	if (hdl == NULL)
		return 0;
	mutex_lock(&hdl->lock);
	list_for_each_entry(ctrl, &hdl->ctrls, node)
		ctrl->done = false;

	list_for_each_entry(ctrl, &hdl->ctrls, node) {
		struct v4l2_ctrl *master = ctrl->cluster[0];
		int i;

		
		
		if (ctrl->done || ctrl->type == V4L2_CTRL_TYPE_BUTTON ||
		    (ctrl->flags & V4L2_CTRL_FLAG_READ_ONLY))
			continue;

		for (i = 0; i < master->ncontrols; i++) {
			if (master->cluster[i]) {
				cur_to_new(master->cluster[i]);
				master->cluster[i]->is_new = 1;
				master->cluster[i]->done = true;
			}
		}
		ret = call_op(master, s_ctrl);
		if (ret)
			break;
	}
	mutex_unlock(&hdl->lock);
	return ret;
}
EXPORT_SYMBOL(v4l2_ctrl_handler_setup);

int v4l2_queryctrl(struct v4l2_ctrl_handler *hdl, struct v4l2_queryctrl *qc)
{
	u32 id = qc->id & V4L2_CTRL_ID_MASK;
	struct v4l2_ctrl_ref *ref;
	struct v4l2_ctrl *ctrl;

	if (hdl == NULL)
		return -EINVAL;

	mutex_lock(&hdl->lock);

	
	ref = find_ref(hdl, id);

	if ((qc->id & V4L2_CTRL_FLAG_NEXT_CTRL) && !list_empty(&hdl->ctrl_refs)) {
		

		
		if (id >= node2id(hdl->ctrl_refs.prev)) {
			ref = NULL; 
		} else if (ref) {
			ref = list_entry(ref->node.next, typeof(*ref), node);
		} else {
			list_for_each_entry(ref, &hdl->ctrl_refs, node)
				if (id < ref->ctrl->id)
					break;
		}
	}
	mutex_unlock(&hdl->lock);
	if (!ref)
		return -EINVAL;

	ctrl = ref->ctrl;
	memset(qc, 0, sizeof(*qc));
	if (id >= V4L2_CID_PRIVATE_BASE)
		qc->id = id;
	else
		qc->id = ctrl->id;
	strlcpy(qc->name, ctrl->name, sizeof(qc->name));
	qc->minimum = ctrl->minimum;
	qc->maximum = ctrl->maximum;
	qc->default_value = ctrl->default_value;
	if (ctrl->type == V4L2_CTRL_TYPE_MENU)
		qc->step = 1;
	else
		qc->step = ctrl->step;
	qc->flags = ctrl->flags;
	qc->type = ctrl->type;
	return 0;
}
EXPORT_SYMBOL(v4l2_queryctrl);

int v4l2_subdev_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	if (qc->id & V4L2_CTRL_FLAG_NEXT_CTRL)
		return -EINVAL;
	return v4l2_queryctrl(sd->ctrl_handler, qc);
}
EXPORT_SYMBOL(v4l2_subdev_queryctrl);

int v4l2_querymenu(struct v4l2_ctrl_handler *hdl, struct v4l2_querymenu *qm)
{
	struct v4l2_ctrl *ctrl;
	u32 i = qm->index;

	ctrl = v4l2_ctrl_find(hdl, qm->id);
	if (!ctrl)
		return -EINVAL;

	qm->reserved = 0;
	
	if (ctrl->qmenu == NULL ||
	    i < ctrl->minimum || i > ctrl->maximum)
		return -EINVAL;
	
	if (ctrl->menu_skip_mask & (1 << i))
		return -EINVAL;
	
	if (ctrl->qmenu[i] == NULL || ctrl->qmenu[i][0] == '\0')
		return -EINVAL;
	strlcpy(qm->name, ctrl->qmenu[i], sizeof(qm->name));
	return 0;
}
EXPORT_SYMBOL(v4l2_querymenu);

int v4l2_subdev_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	return v4l2_querymenu(sd->ctrl_handler, qm);
}
EXPORT_SYMBOL(v4l2_subdev_querymenu);




static int prepare_ext_ctrls(struct v4l2_ctrl_handler *hdl,
			     struct v4l2_ext_controls *cs,
			     struct v4l2_ctrl_helper *helpers)
{
	struct v4l2_ctrl_helper *h;
	bool have_clusters = false;
	u32 i;

	for (i = 0, h = helpers; i < cs->count; i++, h++) {
		struct v4l2_ext_control *c = &cs->controls[i];
		struct v4l2_ctrl_ref *ref;
		struct v4l2_ctrl *ctrl;
		u32 id = c->id & V4L2_CTRL_ID_MASK;

		cs->error_idx = i;

		if (cs->ctrl_class && V4L2_CTRL_ID2CLASS(id) != cs->ctrl_class)
			return -EINVAL;

		if (id >= V4L2_CID_PRIVATE_BASE)
			return -EINVAL;
		ref = find_ref_lock(hdl, id);
		if (ref == NULL)
			return -EINVAL;
		ctrl = ref->ctrl;
		if (ctrl->flags & V4L2_CTRL_FLAG_DISABLED)
			return -EINVAL;

		if (ctrl->cluster[0]->ncontrols > 1)
			have_clusters = true;
		if (ctrl->cluster[0] != ctrl)
			ref = find_ref_lock(hdl, ctrl->cluster[0]->id);
		
		h->mref = ref;
		h->ctrl = ctrl;
		h->next = 0;
	}

	if (!have_clusters)
		return 0;


	
	mutex_lock(&hdl->lock);

	
	for (i = 0; i < cs->count; i++)
		helpers[i].mref->helper = 0;
	for (i = 0, h = helpers; i < cs->count; i++, h++) {
		struct v4l2_ctrl_ref *mref = h->mref;

		if (mref->helper) {
			mref->helper->next = i;
			h->mref = NULL;
		}
		
		mref->helper = h;
	}
	mutex_unlock(&hdl->lock);
	return 0;
}

static int class_check(struct v4l2_ctrl_handler *hdl, u32 ctrl_class)
{
	if (ctrl_class == 0)
		return list_empty(&hdl->ctrl_refs) ? -EINVAL : 0;
	return find_ref_lock(hdl, ctrl_class | 1) ? 0 : -EINVAL;
}



int v4l2_g_ext_ctrls(struct v4l2_ctrl_handler *hdl, struct v4l2_ext_controls *cs)
{
	struct v4l2_ctrl_helper helper[4];
	struct v4l2_ctrl_helper *helpers = helper;
	int ret;
	int i, j;

	cs->error_idx = cs->count;
	cs->ctrl_class = V4L2_CTRL_ID2CLASS(cs->ctrl_class);

	if (hdl == NULL)
		return -EINVAL;

	if (cs->count == 0)
		return class_check(hdl, cs->ctrl_class);

	if (cs->count > ARRAY_SIZE(helper)) {
		helpers = kmalloc(sizeof(helper[0]) * cs->count, GFP_KERNEL);
		if (helpers == NULL)
			return -ENOMEM;
	}

	ret = prepare_ext_ctrls(hdl, cs, helpers);
	cs->error_idx = cs->count;

	for (i = 0; !ret && i < cs->count; i++)
		if (helpers[i].ctrl->flags & V4L2_CTRL_FLAG_WRITE_ONLY)
			ret = -EACCES;

	for (i = 0; !ret && i < cs->count; i++) {
		int (*ctrl_to_user)(struct v4l2_ext_control *c,
				    struct v4l2_ctrl *ctrl) = cur_to_user;
		struct v4l2_ctrl *master;

		if (helpers[i].mref == NULL)
			continue;

		master = helpers[i].mref->ctrl;
		cs->error_idx = i;

		v4l2_ctrl_lock(master);

		
		if ((master->flags & V4L2_CTRL_FLAG_VOLATILE) ||
			(master->has_volatiles && !is_cur_manual(master))) {
			for (j = 0; j < master->ncontrols; j++)
				cur_to_new(master->cluster[j]);
			ret = call_op(master, g_volatile_ctrl);
			ctrl_to_user = new_to_user;
		}
		if (!ret) {
			u32 idx = i;

			do {
				ret = ctrl_to_user(cs->controls + idx,
						   helpers[idx].ctrl);
				idx = helpers[idx].next;
			} while (!ret && idx);
		}
		v4l2_ctrl_unlock(master);
	}

	if (cs->count > ARRAY_SIZE(helper))
		kfree(helpers);
	return ret;
}
EXPORT_SYMBOL(v4l2_g_ext_ctrls);

int v4l2_subdev_g_ext_ctrls(struct v4l2_subdev *sd, struct v4l2_ext_controls *cs)
{
	return v4l2_g_ext_ctrls(sd->ctrl_handler, cs);
}
EXPORT_SYMBOL(v4l2_subdev_g_ext_ctrls);

static int get_ctrl(struct v4l2_ctrl *ctrl, s32 *val)
{
	struct v4l2_ctrl *master = ctrl->cluster[0];
	int ret = 0;
	int i;

	if (ctrl->flags & V4L2_CTRL_FLAG_WRITE_ONLY)
		return -EACCES;

	v4l2_ctrl_lock(master);
	
	if (ctrl->flags & V4L2_CTRL_FLAG_VOLATILE) {
		for (i = 0; i < master->ncontrols; i++)
			cur_to_new(master->cluster[i]);
		ret = call_op(master, g_volatile_ctrl);
		*val = ctrl->val;
	} else {
		*val = ctrl->cur.val;
	}
	v4l2_ctrl_unlock(master);
	return ret;
}

int v4l2_g_ctrl(struct v4l2_ctrl_handler *hdl, struct v4l2_control *control)
{
	struct v4l2_ctrl *ctrl = v4l2_ctrl_find(hdl, control->id);

	if (ctrl == NULL || !type_is_int(ctrl))
		return -EINVAL;
	return get_ctrl(ctrl, &control->value);
}
EXPORT_SYMBOL(v4l2_g_ctrl);

int v4l2_subdev_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control)
{
	return v4l2_g_ctrl(sd->ctrl_handler, control);
}
EXPORT_SYMBOL(v4l2_subdev_g_ctrl);

s32 v4l2_ctrl_g_ctrl(struct v4l2_ctrl *ctrl)
{
	s32 val = 0;

	
	WARN_ON(!type_is_int(ctrl));
	get_ctrl(ctrl, &val);
	return val;
}
EXPORT_SYMBOL(v4l2_ctrl_g_ctrl);


static int try_or_set_cluster(struct v4l2_fh *fh,
			      struct v4l2_ctrl *master, bool set)
{
	bool update_flag;
	int ret;
	int i;

	for (i = 0; i < master->ncontrols; i++) {
		struct v4l2_ctrl *ctrl = master->cluster[i];

		if (ctrl == NULL)
			continue;

		if (!ctrl->is_new) {
			cur_to_new(ctrl);
			continue;
		}
		if (set && (ctrl->flags & V4L2_CTRL_FLAG_GRABBED))
			return -EBUSY;
	}

	ret = call_op(master, try_ctrl);

	
	if (ret || !set || !cluster_changed(master))
		return ret;
	ret = call_op(master, s_ctrl);
	if (ret)
		return ret;

	
	update_flag = is_cur_manual(master) != is_new_manual(master);
	for (i = 0; i < master->ncontrols; i++)
		new_to_cur(fh, master->cluster[i], update_flag && i > 0);
	return 0;
}

static int validate_ctrls(struct v4l2_ext_controls *cs,
			  struct v4l2_ctrl_helper *helpers, bool set)
{
	unsigned i;
	int ret = 0;

	cs->error_idx = cs->count;
	for (i = 0; i < cs->count; i++) {
		struct v4l2_ctrl *ctrl = helpers[i].ctrl;

		cs->error_idx = i;

		if (ctrl->flags & V4L2_CTRL_FLAG_READ_ONLY)
			return -EACCES;
		if (set && (ctrl->flags & V4L2_CTRL_FLAG_GRABBED))
			return -EBUSY;
		ret = validate_new(ctrl, &cs->controls[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static void update_from_auto_cluster(struct v4l2_ctrl *master)
{
	int i;

	for (i = 0; i < master->ncontrols; i++)
		cur_to_new(master->cluster[i]);
	if (!call_op(master, g_volatile_ctrl))
		for (i = 1; i < master->ncontrols; i++)
			if (master->cluster[i])
				master->cluster[i]->is_new = 1;
}

static int try_set_ext_ctrls(struct v4l2_fh *fh, struct v4l2_ctrl_handler *hdl,
			     struct v4l2_ext_controls *cs,
			     bool set)
{
	struct v4l2_ctrl_helper helper[4];
	struct v4l2_ctrl_helper *helpers = helper;
	unsigned i, j;
	int ret;

	cs->error_idx = cs->count;
	cs->ctrl_class = V4L2_CTRL_ID2CLASS(cs->ctrl_class);

	if (hdl == NULL)
		return -EINVAL;

	if (cs->count == 0)
		return class_check(hdl, cs->ctrl_class);

	if (cs->count > ARRAY_SIZE(helper)) {
		helpers = kmalloc(sizeof(helper[0]) * cs->count, GFP_KERNEL);
		if (!helpers)
			return -ENOMEM;
	}
	ret = prepare_ext_ctrls(hdl, cs, helpers);
	if (!ret)
		ret = validate_ctrls(cs, helpers, set);
	if (ret && set)
		cs->error_idx = cs->count;
	for (i = 0; !ret && i < cs->count; i++) {
		struct v4l2_ctrl *master;
		u32 idx = i;

		if (helpers[i].mref == NULL)
			continue;

		cs->error_idx = i;
		master = helpers[i].mref->ctrl;
		v4l2_ctrl_lock(master);

		
		for (j = 0; j < master->ncontrols; j++)
			if (master->cluster[j])
				master->cluster[j]->is_new = 0;

		/* For volatile autoclusters that are currently in auto mode
		   we need to discover if it will be set to manual mode.
		   If so, then we have to copy the current volatile values
		   first since those will become the new manual values (which
		   may be overwritten by explicit new values from this set
		   of controls). */
		if (master->is_auto && master->has_volatiles &&
						!is_cur_manual(master)) {
			
			s32 new_auto_val = master->manual_mode_value + 1;
			u32 tmp_idx = idx;

			do {
				if (helpers[tmp_idx].ctrl == master)
					new_auto_val = cs->controls[tmp_idx].value;
				tmp_idx = helpers[tmp_idx].next;
			} while (tmp_idx);
			if (new_auto_val == master->manual_mode_value)
				update_from_auto_cluster(master);
		}

		do {
			ret = user_to_new(cs->controls + idx, helpers[idx].ctrl);
			idx = helpers[idx].next;
		} while (!ret && idx);

		if (!ret)
			ret = try_or_set_cluster(fh, master, set);

		
		if (!ret) {
			idx = i;
			do {
				ret = new_to_user(cs->controls + idx,
						helpers[idx].ctrl);
				idx = helpers[idx].next;
			} while (!ret && idx);
		}
		v4l2_ctrl_unlock(master);
	}

	if (cs->count > ARRAY_SIZE(helper))
		kfree(helpers);
	return ret;
}

int v4l2_try_ext_ctrls(struct v4l2_ctrl_handler *hdl, struct v4l2_ext_controls *cs)
{
	return try_set_ext_ctrls(NULL, hdl, cs, false);
}
EXPORT_SYMBOL(v4l2_try_ext_ctrls);

int v4l2_s_ext_ctrls(struct v4l2_fh *fh, struct v4l2_ctrl_handler *hdl,
					struct v4l2_ext_controls *cs)
{
	return try_set_ext_ctrls(fh, hdl, cs, true);
}
EXPORT_SYMBOL(v4l2_s_ext_ctrls);

int v4l2_subdev_try_ext_ctrls(struct v4l2_subdev *sd, struct v4l2_ext_controls *cs)
{
	return try_set_ext_ctrls(NULL, sd->ctrl_handler, cs, false);
}
EXPORT_SYMBOL(v4l2_subdev_try_ext_ctrls);

int v4l2_subdev_s_ext_ctrls(struct v4l2_subdev *sd, struct v4l2_ext_controls *cs)
{
	return try_set_ext_ctrls(NULL, sd->ctrl_handler, cs, true);
}
EXPORT_SYMBOL(v4l2_subdev_s_ext_ctrls);

static int set_ctrl(struct v4l2_fh *fh, struct v4l2_ctrl *ctrl, s32 *val)
{
	struct v4l2_ctrl *master = ctrl->cluster[0];
	int ret;
	int i;

	ret = validate_new_int(ctrl, val);
	if (ret)
		return ret;

	v4l2_ctrl_lock(ctrl);

	
	for (i = 0; i < master->ncontrols; i++)
		if (master->cluster[i])
			master->cluster[i]->is_new = 0;

	if (master->is_auto && master->has_volatiles && ctrl == master &&
	    !is_cur_manual(master) && *val == master->manual_mode_value)
		update_from_auto_cluster(master);
	ctrl->val = *val;
	ctrl->is_new = 1;
	ret = try_or_set_cluster(fh, master, true);
	*val = ctrl->cur.val;
	v4l2_ctrl_unlock(ctrl);
	return ret;
}

int v4l2_s_ctrl(struct v4l2_fh *fh, struct v4l2_ctrl_handler *hdl,
					struct v4l2_control *control)
{
	struct v4l2_ctrl *ctrl = v4l2_ctrl_find(hdl, control->id);

	if (ctrl == NULL || !type_is_int(ctrl))
		return -EINVAL;

	if (ctrl->flags & V4L2_CTRL_FLAG_READ_ONLY)
		return -EACCES;

	return set_ctrl(fh, ctrl, &control->value);
}
EXPORT_SYMBOL(v4l2_s_ctrl);

int v4l2_subdev_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control)
{
	return v4l2_s_ctrl(NULL, sd->ctrl_handler, control);
}
EXPORT_SYMBOL(v4l2_subdev_s_ctrl);

int v4l2_ctrl_s_ctrl(struct v4l2_ctrl *ctrl, s32 val)
{
	
	WARN_ON(!type_is_int(ctrl));
	return set_ctrl(NULL, ctrl, &val);
}
EXPORT_SYMBOL(v4l2_ctrl_s_ctrl);

void v4l2_ctrl_add_event(struct v4l2_ctrl *ctrl,
				struct v4l2_subscribed_event *sev)
{
	v4l2_ctrl_lock(ctrl);
	list_add_tail(&sev->node, &ctrl->ev_subs);
	if (ctrl->type != V4L2_CTRL_TYPE_CTRL_CLASS &&
	    (sev->flags & V4L2_EVENT_SUB_FL_SEND_INITIAL)) {
		struct v4l2_event ev;
		u32 changes = V4L2_EVENT_CTRL_CH_FLAGS;

		if (!(ctrl->flags & V4L2_CTRL_FLAG_WRITE_ONLY))
			changes |= V4L2_EVENT_CTRL_CH_VALUE;
		fill_event(&ev, ctrl, changes);
		v4l2_event_queue_fh(sev->fh, &ev);
	}
	v4l2_ctrl_unlock(ctrl);
}
EXPORT_SYMBOL(v4l2_ctrl_add_event);

void v4l2_ctrl_del_event(struct v4l2_ctrl *ctrl,
				struct v4l2_subscribed_event *sev)
{
	v4l2_ctrl_lock(ctrl);
	list_del(&sev->node);
	v4l2_ctrl_unlock(ctrl);
}
EXPORT_SYMBOL(v4l2_ctrl_del_event);

int v4l2_ctrl_log_status(struct file *file, void *fh)
{
	struct video_device *vfd = video_devdata(file);
	struct v4l2_fh *vfh = file->private_data;

	if (test_bit(V4L2_FL_USES_V4L2_FH, &vfd->flags) && vfd->v4l2_dev)
		v4l2_ctrl_handler_log_status(vfh->ctrl_handler,
			vfd->v4l2_dev->name);
	return 0;
}
EXPORT_SYMBOL(v4l2_ctrl_log_status);

int v4l2_ctrl_subscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	if (sub->type == V4L2_EVENT_CTRL)
		return v4l2_event_subscribe(fh, sub, 0);
	return -EINVAL;
}
EXPORT_SYMBOL(v4l2_ctrl_subscribe_event);

unsigned int v4l2_ctrl_poll(struct file *file, struct poll_table_struct *wait)
{
	struct v4l2_fh *fh = file->private_data;

	if (v4l2_event_pending(fh))
		return POLLPRI;
	poll_wait(file, &fh->wait, wait);
	return 0;
}
EXPORT_SYMBOL(v4l2_ctrl_poll);
