
struct ad7879_platform_data {
	u16	model;			
	u16	x_plate_ohms;
	u16	x_min, x_max;
	u16	y_min, y_max;
	u16	pressure_min, pressure_max;

	u8	pen_down_acc_interval;
	u8	first_conversion_delay;
	
	u8	acquisition_time;
	
	u8	averaging;
	u8	median;
	bool	gpio_export;
	s32	gpio_base;
};
