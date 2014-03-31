#ifndef __iop_sw_mpu_defs_h
#define __iop_sw_mpu_defs_h

#ifndef REG_RD
#define REG_RD( scope, inst, reg ) \
  REG_READ( reg_##scope##_##reg, \
            (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_WR
#define REG_WR( scope, inst, reg, val ) \
  REG_WRITE( reg_##scope##_##reg, \
             (inst) + REG_WR_ADDR_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_VECT
#define REG_RD_VECT( scope, inst, reg, index ) \
  REG_READ( reg_##scope##_##reg, \
            (inst) + REG_RD_ADDR_##scope##_##reg + \
	    (index) * STRIDE_##scope##_##reg )
#endif

#ifndef REG_WR_VECT
#define REG_WR_VECT( scope, inst, reg, index, val ) \
  REG_WRITE( reg_##scope##_##reg, \
             (inst) + REG_WR_ADDR_##scope##_##reg + \
	     (index) * STRIDE_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_INT
#define REG_RD_INT( scope, inst, reg ) \
  REG_READ( int, (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_WR_INT
#define REG_WR_INT( scope, inst, reg, val ) \
  REG_WRITE( int, (inst) + REG_WR_ADDR_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_INT_VECT
#define REG_RD_INT_VECT( scope, inst, reg, index ) \
  REG_READ( int, (inst) + REG_RD_ADDR_##scope##_##reg + \
	    (index) * STRIDE_##scope##_##reg )
#endif

#ifndef REG_WR_INT_VECT
#define REG_WR_INT_VECT( scope, inst, reg, index, val ) \
  REG_WRITE( int, (inst) + REG_WR_ADDR_##scope##_##reg + \
	     (index) * STRIDE_##scope##_##reg, (val) )
#endif

#ifndef REG_TYPE_CONV
#define REG_TYPE_CONV( type, orgtype, val ) \
  ( { union { orgtype o; type n; } r; r.o = val; r.n; } )
#endif

#ifndef reg_page_size
#define reg_page_size 8192
#endif

#ifndef REG_ADDR
#define REG_ADDR( scope, inst, reg ) \
  ( (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_ADDR_VECT
#define REG_ADDR_VECT( scope, inst, reg, index ) \
  ( (inst) + REG_RD_ADDR_##scope##_##reg + \
    (index) * STRIDE_##scope##_##reg )
#endif


typedef struct {
  unsigned int cfg : 2;
  unsigned int dummy1 : 30;
} reg_iop_sw_mpu_rw_sw_cfg_owner;
#define REG_RD_ADDR_iop_sw_mpu_rw_sw_cfg_owner 0
#define REG_WR_ADDR_iop_sw_mpu_rw_sw_cfg_owner 0

typedef struct {
  unsigned int keep_owner  : 1;
  unsigned int cmd         : 2;
  unsigned int size        : 3;
  unsigned int wr_spu0_mem : 1;
  unsigned int wr_spu1_mem : 1;
  unsigned int dummy1      : 24;
} reg_iop_sw_mpu_rw_mc_ctrl;
#define REG_RD_ADDR_iop_sw_mpu_rw_mc_ctrl 4
#define REG_WR_ADDR_iop_sw_mpu_rw_mc_ctrl 4

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_mc_data;
#define REG_RD_ADDR_iop_sw_mpu_rw_mc_data 8
#define REG_WR_ADDR_iop_sw_mpu_rw_mc_data 8

typedef unsigned int reg_iop_sw_mpu_rw_mc_addr;
#define REG_RD_ADDR_iop_sw_mpu_rw_mc_addr 12
#define REG_WR_ADDR_iop_sw_mpu_rw_mc_addr 12

typedef unsigned int reg_iop_sw_mpu_rs_mc_data;
#define REG_RD_ADDR_iop_sw_mpu_rs_mc_data 16

typedef unsigned int reg_iop_sw_mpu_r_mc_data;
#define REG_RD_ADDR_iop_sw_mpu_r_mc_data 20

typedef struct {
  unsigned int busy_cpu      : 1;
  unsigned int busy_mpu      : 1;
  unsigned int busy_spu0     : 1;
  unsigned int busy_spu1     : 1;
  unsigned int owned_by_cpu  : 1;
  unsigned int owned_by_mpu  : 1;
  unsigned int owned_by_spu0 : 1;
  unsigned int owned_by_spu1 : 1;
  unsigned int dummy1        : 24;
} reg_iop_sw_mpu_r_mc_stat;
#define REG_RD_ADDR_iop_sw_mpu_r_mc_stat 24

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_mpu_rw_bus0_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus0_clr_mask 28
#define REG_WR_ADDR_iop_sw_mpu_rw_bus0_clr_mask 28

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_mpu_rw_bus0_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus0_set_mask 32
#define REG_WR_ADDR_iop_sw_mpu_rw_bus0_set_mask 32

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_mpu_rw_bus0_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus0_oe_clr_mask 36
#define REG_WR_ADDR_iop_sw_mpu_rw_bus0_oe_clr_mask 36

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_mpu_rw_bus0_oe_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus0_oe_set_mask 40
#define REG_WR_ADDR_iop_sw_mpu_rw_bus0_oe_set_mask 40

typedef unsigned int reg_iop_sw_mpu_r_bus0_in;
#define REG_RD_ADDR_iop_sw_mpu_r_bus0_in 44

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_mpu_rw_bus1_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus1_clr_mask 48
#define REG_WR_ADDR_iop_sw_mpu_rw_bus1_clr_mask 48

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_mpu_rw_bus1_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus1_set_mask 52
#define REG_WR_ADDR_iop_sw_mpu_rw_bus1_set_mask 52

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_mpu_rw_bus1_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus1_oe_clr_mask 56
#define REG_WR_ADDR_iop_sw_mpu_rw_bus1_oe_clr_mask 56

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_mpu_rw_bus1_oe_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus1_oe_set_mask 60
#define REG_WR_ADDR_iop_sw_mpu_rw_bus1_oe_set_mask 60

typedef unsigned int reg_iop_sw_mpu_r_bus1_in;
#define REG_RD_ADDR_iop_sw_mpu_r_bus1_in 64

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_clr_mask 68
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_clr_mask 68

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_set_mask 72
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_set_mask 72

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_oe_clr_mask 76
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_oe_clr_mask 76

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_oe_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_oe_set_mask 80
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_oe_set_mask 80

typedef unsigned int reg_iop_sw_mpu_r_gio_in;
#define REG_RD_ADDR_iop_sw_mpu_r_gio_in 84

typedef struct {
  unsigned int intr0  : 1;
  unsigned int intr1  : 1;
  unsigned int intr2  : 1;
  unsigned int intr3  : 1;
  unsigned int intr4  : 1;
  unsigned int intr5  : 1;
  unsigned int intr6  : 1;
  unsigned int intr7  : 1;
  unsigned int intr8  : 1;
  unsigned int intr9  : 1;
  unsigned int intr10 : 1;
  unsigned int intr11 : 1;
  unsigned int intr12 : 1;
  unsigned int intr13 : 1;
  unsigned int intr14 : 1;
  unsigned int intr15 : 1;
  unsigned int intr16 : 1;
  unsigned int intr17 : 1;
  unsigned int intr18 : 1;
  unsigned int intr19 : 1;
  unsigned int intr20 : 1;
  unsigned int intr21 : 1;
  unsigned int intr22 : 1;
  unsigned int intr23 : 1;
  unsigned int intr24 : 1;
  unsigned int intr25 : 1;
  unsigned int intr26 : 1;
  unsigned int intr27 : 1;
  unsigned int intr28 : 1;
  unsigned int intr29 : 1;
  unsigned int intr30 : 1;
  unsigned int intr31 : 1;
} reg_iop_sw_mpu_rw_cpu_intr;
#define REG_RD_ADDR_iop_sw_mpu_rw_cpu_intr 88
#define REG_WR_ADDR_iop_sw_mpu_rw_cpu_intr 88

typedef struct {
  unsigned int intr0  : 1;
  unsigned int intr1  : 1;
  unsigned int intr2  : 1;
  unsigned int intr3  : 1;
  unsigned int intr4  : 1;
  unsigned int intr5  : 1;
  unsigned int intr6  : 1;
  unsigned int intr7  : 1;
  unsigned int intr8  : 1;
  unsigned int intr9  : 1;
  unsigned int intr10 : 1;
  unsigned int intr11 : 1;
  unsigned int intr12 : 1;
  unsigned int intr13 : 1;
  unsigned int intr14 : 1;
  unsigned int intr15 : 1;
  unsigned int intr16 : 1;
  unsigned int intr17 : 1;
  unsigned int intr18 : 1;
  unsigned int intr19 : 1;
  unsigned int intr20 : 1;
  unsigned int intr21 : 1;
  unsigned int intr22 : 1;
  unsigned int intr23 : 1;
  unsigned int intr24 : 1;
  unsigned int intr25 : 1;
  unsigned int intr26 : 1;
  unsigned int intr27 : 1;
  unsigned int intr28 : 1;
  unsigned int intr29 : 1;
  unsigned int intr30 : 1;
  unsigned int intr31 : 1;
} reg_iop_sw_mpu_r_cpu_intr;
#define REG_RD_ADDR_iop_sw_mpu_r_cpu_intr 92

typedef struct {
  unsigned int spu0_intr0      : 1;
  unsigned int spu1_intr0      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr1      : 1;
  unsigned int spu1_intr1      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr2      : 1;
  unsigned int spu1_intr2      : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr3      : 1;
  unsigned int spu1_intr3      : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_rw_intr_grp0_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp0_mask 96
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp0_mask 96

typedef struct {
  unsigned int spu0_intr0 : 1;
  unsigned int spu1_intr0 : 1;
  unsigned int dummy1     : 6;
  unsigned int spu0_intr1 : 1;
  unsigned int spu1_intr1 : 1;
  unsigned int dummy2     : 6;
  unsigned int spu0_intr2 : 1;
  unsigned int spu1_intr2 : 1;
  unsigned int dummy3     : 6;
  unsigned int spu0_intr3 : 1;
  unsigned int spu1_intr3 : 1;
  unsigned int dummy4     : 6;
} reg_iop_sw_mpu_rw_ack_intr_grp0;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp0 100
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp0 100

typedef struct {
  unsigned int spu0_intr0      : 1;
  unsigned int spu1_intr0      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr1      : 1;
  unsigned int spu1_intr1      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr2      : 1;
  unsigned int spu1_intr2      : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr3      : 1;
  unsigned int spu1_intr3      : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_intr_grp0;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp0 104

typedef struct {
  unsigned int spu0_intr0      : 1;
  unsigned int spu1_intr0      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr1      : 1;
  unsigned int spu1_intr1      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr2      : 1;
  unsigned int spu1_intr2      : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr3      : 1;
  unsigned int spu1_intr3      : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_masked_intr_grp0;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp0 108

typedef struct {
  unsigned int spu0_intr4      : 1;
  unsigned int spu1_intr4      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr5      : 1;
  unsigned int spu1_intr5      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr6      : 1;
  unsigned int spu1_intr6      : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr7      : 1;
  unsigned int spu1_intr7      : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_rw_intr_grp1_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp1_mask 112
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp1_mask 112

typedef struct {
  unsigned int spu0_intr4 : 1;
  unsigned int spu1_intr4 : 1;
  unsigned int dummy1     : 6;
  unsigned int spu0_intr5 : 1;
  unsigned int spu1_intr5 : 1;
  unsigned int dummy2     : 6;
  unsigned int spu0_intr6 : 1;
  unsigned int spu1_intr6 : 1;
  unsigned int dummy3     : 6;
  unsigned int spu0_intr7 : 1;
  unsigned int spu1_intr7 : 1;
  unsigned int dummy4     : 6;
} reg_iop_sw_mpu_rw_ack_intr_grp1;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp1 116
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp1 116

typedef struct {
  unsigned int spu0_intr4      : 1;
  unsigned int spu1_intr4      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr5      : 1;
  unsigned int spu1_intr5      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr6      : 1;
  unsigned int spu1_intr6      : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr7      : 1;
  unsigned int spu1_intr7      : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_intr_grp1;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp1 120

typedef struct {
  unsigned int spu0_intr4      : 1;
  unsigned int spu1_intr4      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr5      : 1;
  unsigned int spu1_intr5      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr6      : 1;
  unsigned int spu1_intr6      : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr7      : 1;
  unsigned int spu1_intr7      : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_masked_intr_grp1;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp1 124

typedef struct {
  unsigned int spu0_intr8      : 1;
  unsigned int spu1_intr8      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr9      : 1;
  unsigned int spu1_intr9      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr10     : 1;
  unsigned int spu1_intr10     : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr11     : 1;
  unsigned int spu1_intr11     : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_rw_intr_grp2_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp2_mask 128
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp2_mask 128

typedef struct {
  unsigned int spu0_intr8  : 1;
  unsigned int spu1_intr8  : 1;
  unsigned int dummy1      : 6;
  unsigned int spu0_intr9  : 1;
  unsigned int spu1_intr9  : 1;
  unsigned int dummy2      : 6;
  unsigned int spu0_intr10 : 1;
  unsigned int spu1_intr10 : 1;
  unsigned int dummy3      : 6;
  unsigned int spu0_intr11 : 1;
  unsigned int spu1_intr11 : 1;
  unsigned int dummy4      : 6;
} reg_iop_sw_mpu_rw_ack_intr_grp2;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp2 132
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp2 132

typedef struct {
  unsigned int spu0_intr8      : 1;
  unsigned int spu1_intr8      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr9      : 1;
  unsigned int spu1_intr9      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr10     : 1;
  unsigned int spu1_intr10     : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr11     : 1;
  unsigned int spu1_intr11     : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_intr_grp2;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp2 136

typedef struct {
  unsigned int spu0_intr8      : 1;
  unsigned int spu1_intr8      : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr9      : 1;
  unsigned int spu1_intr9      : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr10     : 1;
  unsigned int spu1_intr10     : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr11     : 1;
  unsigned int spu1_intr11     : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_masked_intr_grp2;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp2 140

typedef struct {
  unsigned int spu0_intr12     : 1;
  unsigned int spu1_intr12     : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr13     : 1;
  unsigned int spu1_intr13     : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr14     : 1;
  unsigned int spu1_intr14     : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr15     : 1;
  unsigned int spu1_intr15     : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_rw_intr_grp3_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp3_mask 144
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp3_mask 144

typedef struct {
  unsigned int spu0_intr12 : 1;
  unsigned int spu1_intr12 : 1;
  unsigned int dummy1      : 6;
  unsigned int spu0_intr13 : 1;
  unsigned int spu1_intr13 : 1;
  unsigned int dummy2      : 6;
  unsigned int spu0_intr14 : 1;
  unsigned int spu1_intr14 : 1;
  unsigned int dummy3      : 6;
  unsigned int spu0_intr15 : 1;
  unsigned int spu1_intr15 : 1;
  unsigned int dummy4      : 6;
} reg_iop_sw_mpu_rw_ack_intr_grp3;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp3 148
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp3 148

typedef struct {
  unsigned int spu0_intr12     : 1;
  unsigned int spu1_intr12     : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr13     : 1;
  unsigned int spu1_intr13     : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr14     : 1;
  unsigned int spu1_intr14     : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr15     : 1;
  unsigned int spu1_intr15     : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_intr_grp3;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp3 152

typedef struct {
  unsigned int spu0_intr12     : 1;
  unsigned int spu1_intr12     : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int dmc_out0        : 1;
  unsigned int spu0_intr13     : 1;
  unsigned int spu1_intr13     : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int timer_grp1      : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int dmc_in0         : 1;
  unsigned int spu0_intr14     : 1;
  unsigned int spu1_intr14     : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int dmc_out1        : 1;
  unsigned int spu0_intr15     : 1;
  unsigned int spu1_intr15     : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int timer_grp3      : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int dmc_in1         : 1;
} reg_iop_sw_mpu_r_masked_intr_grp3;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp3 156


enum {
  regk_iop_sw_mpu_copy                     = 0x00000000,
  regk_iop_sw_mpu_cpu                      = 0x00000000,
  regk_iop_sw_mpu_mpu                      = 0x00000001,
  regk_iop_sw_mpu_no                       = 0x00000000,
  regk_iop_sw_mpu_nop                      = 0x00000000,
  regk_iop_sw_mpu_rd                       = 0x00000002,
  regk_iop_sw_mpu_reg_copy                 = 0x00000001,
  regk_iop_sw_mpu_rw_bus0_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus0_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus0_oe_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus0_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus1_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus1_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus1_oe_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus1_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_gio_clr_mask_default  = 0x00000000,
  regk_iop_sw_mpu_rw_gio_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_gio_oe_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_gio_set_mask_default  = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp0_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp1_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp2_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp3_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_sw_cfg_owner_default  = 0x00000000,
  regk_iop_sw_mpu_set                      = 0x00000001,
  regk_iop_sw_mpu_spu0                     = 0x00000002,
  regk_iop_sw_mpu_spu1                     = 0x00000003,
  regk_iop_sw_mpu_wr                       = 0x00000003,
  regk_iop_sw_mpu_yes                      = 0x00000001
};
#endif 
