/*
 * include/linux/input/adxl34x.h
 *
 * Digital Accelerometer characteristics are highly application specific
 * and may vary between boards and models. The platform_data for the
 * device's "struct device" holds this information.
 *
 * Copyright 2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __LINUX_INPUT_ADXL34X_H__
#define __LINUX_INPUT_ADXL34X_H__

struct adxl34x_platform_data {


	s8 x_axis_offset;
	s8 y_axis_offset;
	s8 z_axis_offset;


#define ADXL_SUPPRESS	(1 << 3)
#define ADXL_TAP_X_EN	(1 << 2)
#define ADXL_TAP_Y_EN	(1 << 1)
#define ADXL_TAP_Z_EN	(1 << 0)

	u8 tap_axis_control;


	u8 tap_threshold;


	u8 tap_duration;


	u8 tap_latency;


	u8 tap_window;


#define ADXL_ACT_ACDC		(1 << 7)
#define ADXL_ACT_X_EN		(1 << 6)
#define ADXL_ACT_Y_EN		(1 << 5)
#define ADXL_ACT_Z_EN		(1 << 4)
#define ADXL_INACT_ACDC		(1 << 3)
#define ADXL_INACT_X_EN		(1 << 2)
#define ADXL_INACT_Y_EN		(1 << 1)
#define ADXL_INACT_Z_EN		(1 << 0)

	u8 act_axis_control;


	u8 activity_threshold;


	u8 inactivity_threshold;


	u8 inactivity_time;


	u8 free_fall_threshold;


	u8 free_fall_time;


	u8 data_rate;


#define ADXL_FULL_RES		(1 << 3)
#define ADXL_RANGE_PM_2g	0
#define ADXL_RANGE_PM_4g	1
#define ADXL_RANGE_PM_8g	2
#define ADXL_RANGE_PM_16g	3

	u8 data_range;


	u8 low_power_mode;


#define ADXL_LINK	(1 << 5)
#define ADXL_AUTO_SLEEP	(1 << 4)

	u8 power_mode;


#define ADXL_FIFO_BYPASS	0
#define ADXL_FIFO_FIFO		1
#define ADXL_FIFO_STREAM	2

	u8 fifo_mode;


	u8 watermark;

	u32 ev_type;	

	u32 ev_code_x;	
	u32 ev_code_y;	
	u32 ev_code_z;	


	u32 ev_code_tap[3];	


	u32 ev_code_ff;	
	u32 ev_code_act_inactivity;	

	u8 use_int2;


#define ADXL_EN_ORIENTATION_2D		1
#define ADXL_EN_ORIENTATION_3D		2
#define ADXL_EN_ORIENTATION_2D_3D	3

	u8 orientation_enable;


#define ADXL_DEADZONE_ANGLE_0p0		0	
#define ADXL_DEADZONE_ANGLE_3p6		1	
#define ADXL_DEADZONE_ANGLE_7p2		2	
#define ADXL_DEADZONE_ANGLE_10p8	3	
#define ADXL_DEADZONE_ANGLE_14p4	4	
#define ADXL_DEADZONE_ANGLE_18p0	5	
#define ADXL_DEADZONE_ANGLE_21p6	6	
#define ADXL_DEADZONE_ANGLE_25p2	7	

	u8 deadzone_angle;


#define ADXL_LP_FILTER_DIVISOR_2	0
#define ADXL_LP_FILTER_DIVISOR_4	1
#define ADXL_LP_FILTER_DIVISOR_8	2
#define ADXL_LP_FILTER_DIVISOR_16	3
#define ADXL_LP_FILTER_DIVISOR_32	4
#define ADXL_LP_FILTER_DIVISOR_64	5
#define ADXL_LP_FILTER_DIVISOR_128	6
#define ADXL_LP_FILTER_DIVISOR_256	7

	u8 divisor_length;

	u32 ev_codes_orient_2d[4];	
	u32 ev_codes_orient_3d[6];	
};
#endif
