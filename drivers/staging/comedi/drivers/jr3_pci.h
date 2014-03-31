
static inline u16 get_u16(volatile const u32 * p)
{
	return (u16) readl(p);
}

static inline void set_u16(volatile u32 * p, u16 val)
{
	writel(val, p);
}

static inline s16 get_s16(volatile const s32 * p)
{
	return (s16) readl(p);
}

static inline void set_s16(volatile s32 * p, s16 val)
{
	writel(val, p);
}


struct raw_channel {
	u32 raw_time;
	s32 raw_data;
	s32 reserved[2];
};

struct force_array {
	s32 fx;
	s32 fy;
	s32 fz;
	s32 mx;
	s32 my;
	s32 mz;
	s32 v1;
	s32 v2;
};

struct six_axis_array {
	s32 fx;
	s32 fy;
	s32 fz;
	s32 mx;
	s32 my;
	s32 mz;
};


enum {
	fx = 0x0001,
	fy = 0x0002,
	fz = 0x0004,
	mx = 0x0008,
	my = 0x0010,
	mz = 0x0020,
	changeV2 = 0x0040,
	changeV1 = 0x0080
} vect_bits_t;



enum {
	fx_near_sat = 0x0001,
	fy_near_sat = 0x0002,
	fz_near_sat = 0x0004,
	mx_near_sat = 0x0008,
	my_near_sat = 0x0010,
	mz_near_sat = 0x0020
} warning_bits_t;









enum error_bits_t {
	fx_sat = 0x0001,
	fy_sat = 0x0002,
	fz_sat = 0x0004,
	mx_sat = 0x0008,
	my_sat = 0x0010,
	mz_sat = 0x0020,
	memory_error = 0x0400,
	sensor_change = 0x0800,
	system_busy = 0x1000,
	cal_crc_bad = 0x2000,
	watch_dog2 = 0x4000,
	watch_dog = 0x8000
};



struct thresh_struct {
	s32 data_address;
	s32 threshold;
	s32 bit_pattern;
};


struct le_struct {
	s32 latch_bits;
	s32 number_of_ge_thresholds;
	s32 number_of_le_thresholds;
	struct thresh_struct thresholds[4];
	s32 reserved;
};


enum link_types {
	end_x_form,
	tx,
	ty,
	tz,
	rx,
	ry,
	rz,
	neg
};

struct intern_transform {
	struct {
		u32 link_type;
		s32 link_amount;
	} link[8];
};


struct jr3_channel {
	
	

	struct raw_channel raw_channels[16];	

	/*  Copyright is a null terminated ASCII string containing the JR3 */
	/*  copyright notice. */

	u32 copyright[0x0018];	
	s32 reserved1[0x0008];	


	struct six_axis_array shunts;	
	s32 reserved2[2];	

	
	

	struct six_axis_array default_FS;	
	s32 reserved3;		


	s32 load_envelope_num;	

	


	struct six_axis_array min_full_scale;	
	s32 reserved4;		


	s32 transform_num;	

	
	

	struct six_axis_array max_full_scale;	
	s32 reserved5;		


	s32 peak_address;	

	/* Full_scale is the sensor full scales which are currently in use.
	 * Decoupled and filtered data is scaled so that +/- 16384 is equal
	 * to the full scales. The engineering units used are indicated by
	 * the units value discussed on page 16. The full scales for Fx, Fy,
	 * Fz, Mx, My and Mz can be written by the user prior to calling
	 * command (10) set new full scales (pg. 38). The full scales for V1
	 * and V2 are set whenever the full scales are changed or when the
	 * axes used to calculate the vectors are changed. The full scale of
	 * V1 and V2 will always be equal to the largest full scale of the
	 * axes used for each vector respectively.
	 */

	struct force_array full_scale;	

	/* Offsets contains the sensor offsets. These values are subtracted from
	 * the sensor data to obtain the decoupled data. The offsets are set a
	 * few seconds (< 10) after the calibration data has been received.
	 * They are set so that the output data will be zero. These values
	 * can be written as well as read. The JR3 DSP will use the values
	 * written here within 2 ms of being written. To set future
	 * decoupled data to zero, add these values to the current decoupled
	 * data values and place the sum here. The JR3 DSP will change these
	 * values when a new transform is applied. So if the offsets are
	 * such that FX is 5 and all other values are zero, after rotating
	 * about Z by 90 degrees, FY would be 5 and all others would be zero.
	 */

	struct six_axis_array offsets;	


	s32 offset_num;		


	u32 vect_axes;		


	struct force_array filter[7];	


	struct force_array rate_data;	


	struct force_array minimum_data;	
	struct force_array maximum_data;	


	s32 near_sat_value;	
	s32 sat_value;		


	s32 rate_address;	
	u32 rate_divisor;	
	u32 rate_count;		


	s32 command_word2;	
	s32 command_word1;	
	s32 command_word0;	


	u32 count1;		
	u32 count2;		
	u32 count3;		
	u32 count4;		
	u32 count5;		
	u32 count6;		


	u32 error_count;	


	u32 count_x;		


	u32 warnings;		
	u32 errors;		


	s32 threshold_bits;	


	s32 last_CRC;		


	s32 eeprom_ver_no;	
	s32 software_ver_no;	


	s32 software_day;	
	s32 software_year;	


	u32 serial_no;		
	u32 model_no;		


	s32 cal_day;		
	s32 cal_year;		


	u32 units;		
	s32 bits;		
	s32 channels;		


	s32 thickness;		


	struct le_struct load_envelopes[0x10];	


	struct intern_transform transforms[0x10];	
};

struct jr3_t {
	struct {
		u32 program_low[0x4000];	
		struct jr3_channel data;	
		char pad2[0x30000 - 0x00c00];	
		u32 program_high[0x8000];	
		u32 reset;	
		char pad3[0x20000 - 0x00004];	
	} channel[4];
};
