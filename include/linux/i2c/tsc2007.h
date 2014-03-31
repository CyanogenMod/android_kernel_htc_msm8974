#ifndef __LINUX_I2C_TSC2007_H
#define __LINUX_I2C_TSC2007_H


struct tsc2007_platform_data {
	u16	model;				
	u16	x_plate_ohms;	
	u16	max_rt; 
	unsigned long poll_delay; 
	unsigned long poll_period; 
	int	fuzzx; 
	int	fuzzy;
	int	fuzzz;
	u16	min_x;
	u16	min_y;
	u16	max_x;
	u16	max_y;
	unsigned long irq_flags;
	bool	invert_x;
	bool	invert_y;
	bool	invert_z1;
	bool	invert_z2;

	int	(*get_pendown_state)(void);
	void	(*clear_penirq)(void);		
	int	(*init_platform_hw)(void);
	void	(*exit_platform_hw)(void);
	int	(*power_shutdown)(bool);
};

#endif
