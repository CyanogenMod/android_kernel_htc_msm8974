/*
 *  (c) 2003-2006 Advanced Micro Devices, Inc.
 *  Your use of this code is subject to the terms and conditions of the
 *  GNU general public license version 2. See "COPYING" or
 *  http://www.gnu.org/licenses/gpl.html
 */

enum pstate {
	HW_PSTATE_INVALID = 0xff,
	HW_PSTATE_0 = 0,
	HW_PSTATE_1 = 1,
	HW_PSTATE_2 = 2,
	HW_PSTATE_3 = 3,
	HW_PSTATE_4 = 4,
	HW_PSTATE_5 = 5,
	HW_PSTATE_6 = 6,
	HW_PSTATE_7 = 7,
};

struct powernow_k8_data {
	unsigned int cpu;

	u32 numps;  
	u32 batps;  
	u32 max_hw_pstate; 

	u32 rvo;     
	u32 irt;     
	u32 vidmvs;  
	u32 vstable; 
	u32 plllock; 
        u32 exttype; 

	
	u32 currvid;
	u32 currfid;
	enum pstate currpstate;

	struct cpufreq_frequency_table  *powernow_table;

	struct acpi_processor_performance acpi_data;

	struct cpumask *available_cores;
};

#define CPUID_PROCESSOR_SIGNATURE	1	
#define CPUID_XFAM			0x0ff00000	
#define CPUID_XFAM_K8			0
#define CPUID_XMOD			0x000f0000	
#define CPUID_XMOD_REV_MASK		0x000c0000
#define CPUID_XFAM_10H			0x00100000	
#define CPUID_USE_XFAM_XMOD		0x00000f00
#define CPUID_GET_MAX_CAPABILITIES	0x80000000
#define CPUID_FREQ_VOLT_CAPABILITIES	0x80000007
#define P_STATE_TRANSITION_CAPABLE	6


#define MSR_FIDVID_CTL      0xc0010041
#define MSR_FIDVID_STATUS   0xc0010042

#define MSR_C_LO_INIT_FID_VID     0x00010000
#define MSR_C_LO_NEW_VID          0x00003f00
#define MSR_C_LO_NEW_FID          0x0000003f
#define MSR_C_LO_VID_SHIFT        8

#define MSR_C_HI_STP_GNT_TO	  0x000fffff

#define MSR_S_LO_CHANGE_PENDING   0x80000000   
#define MSR_S_LO_MAX_RAMP_VID     0x3f000000
#define MSR_S_LO_MAX_FID          0x003f0000
#define MSR_S_LO_START_FID        0x00003f00
#define MSR_S_LO_CURRENT_FID      0x0000003f

#define MSR_S_HI_MIN_WORKING_VID  0x3f000000
#define MSR_S_HI_MAX_WORKING_VID  0x003f0000
#define MSR_S_HI_START_VID        0x00003f00
#define MSR_S_HI_CURRENT_VID      0x0000003f
#define MSR_C_HI_STP_GNT_BENIGN	  0x00000001


#define USE_HW_PSTATE		0x00000080
#define HW_PSTATE_MASK 		0x00000007
#define HW_PSTATE_VALID_MASK 	0x80000000
#define HW_PSTATE_MAX_MASK	0x000000f0
#define HW_PSTATE_MAX_SHIFT	4
#define MSR_PSTATE_DEF_BASE 	0xc0010064 
#define MSR_PSTATE_STATUS 	0xc0010063 
#define MSR_PSTATE_CTRL 	0xc0010062 
#define MSR_PSTATE_CUR_LIMIT	0xc0010061 

#define CPU_OPTERON 0
#define CPU_HW_PSTATE 1



#define LO_FID_TABLE_TOP     7	
#define HI_FID_TABLE_BOTTOM  8	

#define LO_VCOFREQ_TABLE_TOP    1400	
#define HI_VCOFREQ_TABLE_BOTTOM 1600

#define MIN_FREQ_RESOLUTION  200 

#define MAX_FID 0x2a	
#define LEAST_VID 0x3e	

#define MIN_FREQ 800	
#define MAX_FREQ 5000

#define INVALID_FID_MASK 0xffffffc0  
#define INVALID_VID_MASK 0xffffffc0  

#define VID_OFF 0x3f

#define STOP_GRANT_5NS 1 

#define PLL_LOCK_CONVERSION (1000/5) 

#define MAXIMUM_VID_STEPS 1  
#define VST_UNITS_20US 20   


#define IRT_SHIFT      30
#define RVO_SHIFT      28
#define EXT_TYPE_SHIFT 27
#define PLL_L_SHIFT    20
#define MVS_SHIFT      18
#define VST_SHIFT      11
#define VID_SHIFT       6
#define IRT_MASK        3
#define RVO_MASK        3
#define EXT_TYPE_MASK   1
#define PLL_L_MASK   0x7f
#define MVS_MASK        3
#define VST_MASK     0x7f
#define VID_MASK     0x1f
#define FID_MASK     0x1f
#define EXT_VID_MASK 0x3f
#define EXT_FID_MASK 0x3f



#define PSB_ID_STRING      "AMDK7PNOW!"
#define PSB_ID_STRING_LEN  10

#define PSB_VERSION_1_4  0x14

struct psb_s {
	u8 signature[10];
	u8 tableversion;
	u8 flags1;
	u16 vstable;
	u8 flags2;
	u8 num_tables;
	u32 cpuid;
	u8 plllocktime;
	u8 maxfid;
	u8 maxvid;
	u8 numps;
};

struct pst_s {
	u8 fid;
	u8 vid;
};

static int core_voltage_pre_transition(struct powernow_k8_data *data,
	u32 reqvid, u32 regfid);
static int core_voltage_post_transition(struct powernow_k8_data *data, u32 reqvid);
static int core_frequency_transition(struct powernow_k8_data *data, u32 reqfid);

static void powernow_k8_acpi_pst_values(struct powernow_k8_data *data, unsigned int index);

static int fill_powernow_table_pstate(struct powernow_k8_data *data, struct cpufreq_frequency_table *powernow_table);
static int fill_powernow_table_fidvid(struct powernow_k8_data *data, struct cpufreq_frequency_table *powernow_table);
