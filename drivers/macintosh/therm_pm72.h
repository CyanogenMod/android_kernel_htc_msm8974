#ifndef __THERM_PMAC_7_2_H__
#define __THERM_PMAC_7_2_H__

typedef unsigned short fu16;
typedef int fs32;
typedef short fs16;

struct mpu_data
{
	u8	signature;		
	u8	bytes_used;		
	u8	size;			
	u8	version;		
	u32	data_revision;		
	u8	processor_bin_code[3];	
	u8	bin_code_expansion;	
	u8	processor_num;		
	u8	input_mul_bus_div;	
	u8	reserved1[2];		
	u32	input_clk_freq_high;	
	u8	cpu_nb_target_cycles;	
	u8	cpu_statlat;		
	u8	cpu_snooplat;		
	u8	cpu_snoopacc;		
	u8	nb_paamwin;		
	u8	nb_statlat;		
	u8	nb_snooplat;		
	u8	nb_snoopwin;		
	u8	api_bus_mode;		
	u8	reserved2[3];		
	u32	input_clk_freq_low;	
	u8	processor_card_slot;	
	u8	reserved3[2];		
	u8	padjmax;       		
	u8	ttarget;		
	u8	tmax;			
	u8	pmaxh;			
	u8	tguardband;		
	fs32	pid_gp;			
	fs32	pid_gr;			
	fs32	pid_gd;			
	fu16	voph;			
	fu16	vopl;			
	fs16	nactual_die;		
	fs16	nactual_heatsink;	
	fs16	nactual_system;		
	u16	calibration_flags;	
	fu16	mdiode;			
	fs16	bdiode;			
	fs32	theta_heat_sink;	
	u16	rminn_intake_fan;	
	u16	rmaxn_intake_fan;	
	u16	rminn_exhaust_fan;	
	u16	rmaxn_exhaust_fan;	
	u8	processor_part_num[8];	
	u32	processor_lot_num;	
	u8	orig_card_sernum[0x10];	
	u8	curr_card_sernum[0x10];	
	u8	mlb_sernum[0x18];	
	u32	checksum1;		
	u32	checksum2;			
}; 

#define FIX32TOPRINT(f)	((f) >> 16),((((f) & 0xffff) * 1000) >> 16)

#define MAX_CRITICAL_STATE			30
static char * critical_overtemp_path = "/sbin/critical_overtemp";

#define RPM_PID_USE_ACTUAL_SPEED		0

#define FAN_CTRLER_ID		0x15e
#define SUPPLY_MONITOR_ID      	0x58
#define SUPPLY_MONITORB_ID     	0x5a
#define DRIVES_DALLAS_ID	0x94
#define BACKSIDE_MAX_ID		0x98
#define XSERVE_DIMMS_LM87	0x25a
#define XSERVE_SLOTS_LM75	0x290

#define MAX6690_INT_TEMP	0
#define MAX6690_EXT_TEMP	1
#define DS1775_TEMP		0
#define LM87_INT_TEMP		0x27

#define ADC_12V_CURRENT_SCALE	0x0320	
#define ADC_CPU_VOLTAGE_SCALE	0x00a0	
#define ADC_CPU_CURRENT_SCALE	0x1f40	

#define BACKSIDE_FAN_PWM_DEFAULT_ID	1
#define BACKSIDE_FAN_PWM_INDEX		0
#define BACKSIDE_PID_U3_G_d		0x02800000
#define BACKSIDE_PID_U3H_G_d		0x01400000
#define BACKSIDE_PID_RACK_G_d		0x00500000
#define BACKSIDE_PID_G_p		0x00500000
#define BACKSIDE_PID_RACK_G_p		0x0004cccc
#define BACKSIDE_PID_G_r		0x00000000
#define BACKSIDE_PID_U3_INPUT_TARGET	0x00410000
#define BACKSIDE_PID_U3H_INPUT_TARGET	0x004b0000
#define BACKSIDE_PID_RACK_INPUT_TARGET	0x00460000
#define BACKSIDE_PID_INTERVAL		5
#define BACKSIDE_PID_RACK_INTERVAL	1
#define BACKSIDE_PID_OUTPUT_MAX		100
#define BACKSIDE_PID_U3_OUTPUT_MIN	20
#define BACKSIDE_PID_U3H_OUTPUT_MIN	20
#define BACKSIDE_PID_HISTORY_SIZE	2

struct basckside_pid_params
{
	s32			G_d;
	s32			G_p;
	s32			G_r;
	s32			input_target;
	s32			output_min;
	s32			output_max;
	s32			interval;
	int			additive;
};

struct backside_pid_state
{
	int			ticks;
	struct i2c_client *	monitor;
	s32		       	sample_history[BACKSIDE_PID_HISTORY_SIZE];
	s32			error_history[BACKSIDE_PID_HISTORY_SIZE];
	int			cur_sample;
	s32			last_temp;
	int			pwm;
	int			first;
};

#define DRIVES_FAN_RPM_DEFAULT_ID	2
#define DRIVES_FAN_RPM_INDEX		1
#define DRIVES_PID_G_d			0x01e00000
#define DRIVES_PID_G_p			0x00500000
#define DRIVES_PID_G_r			0x00000000
#define DRIVES_PID_INPUT_TARGET		0x00280000
#define DRIVES_PID_INTERVAL    		5
#define DRIVES_PID_OUTPUT_MAX		4000
#define DRIVES_PID_OUTPUT_MIN		300
#define DRIVES_PID_HISTORY_SIZE		2

struct drives_pid_state
{
	int			ticks;
	struct i2c_client *	monitor;
	s32	       		sample_history[BACKSIDE_PID_HISTORY_SIZE];
	s32			error_history[BACKSIDE_PID_HISTORY_SIZE];
	int			cur_sample;
	s32			last_temp;
	int			rpm;
	int			first;
};

#define SLOTS_FAN_PWM_DEFAULT_ID	2
#define SLOTS_FAN_PWM_INDEX		2
#define	SLOTS_FAN_DEFAULT_PWM		40 


#define DIMM_PID_G_d			0
#define DIMM_PID_G_p			0
#define DIMM_PID_G_r			0x06553600
#define DIMM_PID_INPUT_TARGET		3276800
#define DIMM_PID_INTERVAL    		1
#define DIMM_PID_OUTPUT_MAX		14000
#define DIMM_PID_OUTPUT_MIN		4000
#define DIMM_PID_HISTORY_SIZE		20

struct dimm_pid_state
{
	int			ticks;
	struct i2c_client *	monitor;
	s32	       		sample_history[DIMM_PID_HISTORY_SIZE];
	s32			error_history[DIMM_PID_HISTORY_SIZE];
	int			cur_sample;
	s32			last_temp;
	int			first;
	int			output;
};


#define SLOTS_PID_G_d			0
#define SLOTS_PID_G_p			0
#define SLOTS_PID_G_r			0x00100000
#define SLOTS_PID_INPUT_TARGET		3200000
#define SLOTS_PID_INTERVAL    		1
#define SLOTS_PID_OUTPUT_MAX		100
#define SLOTS_PID_OUTPUT_MIN		20
#define SLOTS_PID_HISTORY_SIZE		20

struct slots_pid_state
{
	int			ticks;
	struct i2c_client *	monitor;
	s32	       		sample_history[SLOTS_PID_HISTORY_SIZE];
	s32			error_history[SLOTS_PID_HISTORY_SIZE];
	int			cur_sample;
	s32			last_temp;
	int			first;
	int			pwm;
};




#define CPUA_INTAKE_FAN_RPM_DEFAULT_ID	3
#define CPUA_EXHAUST_FAN_RPM_DEFAULT_ID	4
#define CPUB_INTAKE_FAN_RPM_DEFAULT_ID	5
#define CPUB_EXHAUST_FAN_RPM_DEFAULT_ID	6

#define CPUA_INTAKE_FAN_RPM_INDEX	3
#define CPUA_EXHAUST_FAN_RPM_INDEX	4
#define CPUB_INTAKE_FAN_RPM_INDEX	5
#define CPUB_EXHAUST_FAN_RPM_INDEX	6

#define CPU_INTAKE_SCALE		0x0000f852
#define CPU_TEMP_HISTORY_SIZE		2
#define CPU_POWER_HISTORY_SIZE		10
#define CPU_PID_INTERVAL		1
#define CPU_MAX_OVERTEMP		90

#define CPUA_PUMP_RPM_INDEX		7
#define CPUB_PUMP_RPM_INDEX		8
#define CPU_PUMP_OUTPUT_MAX		3200
#define CPU_PUMP_OUTPUT_MIN		1250

#define CPU_A1_FAN_RPM_INDEX		9
#define CPU_A2_FAN_RPM_INDEX		10
#define CPU_A3_FAN_RPM_INDEX		11
#define CPU_B1_FAN_RPM_INDEX		12
#define CPU_B2_FAN_RPM_INDEX		13
#define CPU_B3_FAN_RPM_INDEX		14


struct cpu_pid_state
{
	int			index;
	struct i2c_client *	monitor;
	struct mpu_data		mpu;
	int			overtemp;
	s32	       		temp_history[CPU_TEMP_HISTORY_SIZE];
	int			cur_temp;
	s32			power_history[CPU_POWER_HISTORY_SIZE];
	s32			error_history[CPU_POWER_HISTORY_SIZE];
	int			cur_power;
	int			count_power;
	int			rpm;
	int			intake_rpm;
	s32			voltage;
	s32			current_a;
	s32			last_temp;
	s32			last_power;
	int			first;
	u8			adc_config;
	s32			pump_min;
	s32			pump_max;
};

#define FCU_TICKLE_TICKS	10

enum {
	state_detached,
	state_attaching,
	state_attached,
	state_detaching,
};


#endif 
