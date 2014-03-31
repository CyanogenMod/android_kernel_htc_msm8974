#ifndef __iop_sw_cpu_defs_h
#define __iop_sw_cpu_defs_h

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
  unsigned int keep_owner  : 1;
  unsigned int cmd         : 2;
  unsigned int size        : 3;
  unsigned int wr_spu0_mem : 1;
  unsigned int wr_spu1_mem : 1;
  unsigned int dummy1      : 24;
} reg_iop_sw_cpu_rw_mc_ctrl;
#define REG_RD_ADDR_iop_sw_cpu_rw_mc_ctrl 0
#define REG_WR_ADDR_iop_sw_cpu_rw_mc_ctrl 0

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_cpu_rw_mc_data;
#define REG_RD_ADDR_iop_sw_cpu_rw_mc_data 4
#define REG_WR_ADDR_iop_sw_cpu_rw_mc_data 4

typedef unsigned int reg_iop_sw_cpu_rw_mc_addr;
#define REG_RD_ADDR_iop_sw_cpu_rw_mc_addr 8
#define REG_WR_ADDR_iop_sw_cpu_rw_mc_addr 8

typedef unsigned int reg_iop_sw_cpu_rs_mc_data;
#define REG_RD_ADDR_iop_sw_cpu_rs_mc_data 12

typedef unsigned int reg_iop_sw_cpu_r_mc_data;
#define REG_RD_ADDR_iop_sw_cpu_r_mc_data 16

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
} reg_iop_sw_cpu_r_mc_stat;
#define REG_RD_ADDR_iop_sw_cpu_r_mc_stat 20

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_cpu_rw_bus0_clr_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus0_clr_mask 24
#define REG_WR_ADDR_iop_sw_cpu_rw_bus0_clr_mask 24

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_cpu_rw_bus0_set_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus0_set_mask 28
#define REG_WR_ADDR_iop_sw_cpu_rw_bus0_set_mask 28

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_cpu_rw_bus0_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus0_oe_clr_mask 32
#define REG_WR_ADDR_iop_sw_cpu_rw_bus0_oe_clr_mask 32

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_cpu_rw_bus0_oe_set_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus0_oe_set_mask 36
#define REG_WR_ADDR_iop_sw_cpu_rw_bus0_oe_set_mask 36

typedef unsigned int reg_iop_sw_cpu_r_bus0_in;
#define REG_RD_ADDR_iop_sw_cpu_r_bus0_in 40

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_cpu_rw_bus1_clr_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus1_clr_mask 44
#define REG_WR_ADDR_iop_sw_cpu_rw_bus1_clr_mask 44

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_cpu_rw_bus1_set_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus1_set_mask 48
#define REG_WR_ADDR_iop_sw_cpu_rw_bus1_set_mask 48

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_cpu_rw_bus1_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus1_oe_clr_mask 52
#define REG_WR_ADDR_iop_sw_cpu_rw_bus1_oe_clr_mask 52

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_cpu_rw_bus1_oe_set_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_bus1_oe_set_mask 56
#define REG_WR_ADDR_iop_sw_cpu_rw_bus1_oe_set_mask 56

typedef unsigned int reg_iop_sw_cpu_r_bus1_in;
#define REG_RD_ADDR_iop_sw_cpu_r_bus1_in 60

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_cpu_rw_gio_clr_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_gio_clr_mask 64
#define REG_WR_ADDR_iop_sw_cpu_rw_gio_clr_mask 64

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_cpu_rw_gio_set_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_gio_set_mask 68
#define REG_WR_ADDR_iop_sw_cpu_rw_gio_set_mask 68

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_cpu_rw_gio_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_gio_oe_clr_mask 72
#define REG_WR_ADDR_iop_sw_cpu_rw_gio_oe_clr_mask 72

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_cpu_rw_gio_oe_set_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_gio_oe_set_mask 76
#define REG_WR_ADDR_iop_sw_cpu_rw_gio_oe_set_mask 76

typedef unsigned int reg_iop_sw_cpu_r_gio_in;
#define REG_RD_ADDR_iop_sw_cpu_r_gio_in 80

typedef struct {
  unsigned int mpu_0   : 1;
  unsigned int mpu_1   : 1;
  unsigned int mpu_2   : 1;
  unsigned int mpu_3   : 1;
  unsigned int mpu_4   : 1;
  unsigned int mpu_5   : 1;
  unsigned int mpu_6   : 1;
  unsigned int mpu_7   : 1;
  unsigned int mpu_8   : 1;
  unsigned int mpu_9   : 1;
  unsigned int mpu_10  : 1;
  unsigned int mpu_11  : 1;
  unsigned int mpu_12  : 1;
  unsigned int mpu_13  : 1;
  unsigned int mpu_14  : 1;
  unsigned int mpu_15  : 1;
  unsigned int spu0_0  : 1;
  unsigned int spu0_1  : 1;
  unsigned int spu0_2  : 1;
  unsigned int spu0_3  : 1;
  unsigned int spu0_4  : 1;
  unsigned int spu0_5  : 1;
  unsigned int spu0_6  : 1;
  unsigned int spu0_7  : 1;
  unsigned int spu1_8  : 1;
  unsigned int spu1_9  : 1;
  unsigned int spu1_10 : 1;
  unsigned int spu1_11 : 1;
  unsigned int spu1_12 : 1;
  unsigned int spu1_13 : 1;
  unsigned int spu1_14 : 1;
  unsigned int spu1_15 : 1;
} reg_iop_sw_cpu_rw_intr0_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_intr0_mask 84
#define REG_WR_ADDR_iop_sw_cpu_rw_intr0_mask 84

typedef struct {
  unsigned int mpu_0   : 1;
  unsigned int mpu_1   : 1;
  unsigned int mpu_2   : 1;
  unsigned int mpu_3   : 1;
  unsigned int mpu_4   : 1;
  unsigned int mpu_5   : 1;
  unsigned int mpu_6   : 1;
  unsigned int mpu_7   : 1;
  unsigned int mpu_8   : 1;
  unsigned int mpu_9   : 1;
  unsigned int mpu_10  : 1;
  unsigned int mpu_11  : 1;
  unsigned int mpu_12  : 1;
  unsigned int mpu_13  : 1;
  unsigned int mpu_14  : 1;
  unsigned int mpu_15  : 1;
  unsigned int spu0_0  : 1;
  unsigned int spu0_1  : 1;
  unsigned int spu0_2  : 1;
  unsigned int spu0_3  : 1;
  unsigned int spu0_4  : 1;
  unsigned int spu0_5  : 1;
  unsigned int spu0_6  : 1;
  unsigned int spu0_7  : 1;
  unsigned int spu1_8  : 1;
  unsigned int spu1_9  : 1;
  unsigned int spu1_10 : 1;
  unsigned int spu1_11 : 1;
  unsigned int spu1_12 : 1;
  unsigned int spu1_13 : 1;
  unsigned int spu1_14 : 1;
  unsigned int spu1_15 : 1;
} reg_iop_sw_cpu_rw_ack_intr0;
#define REG_RD_ADDR_iop_sw_cpu_rw_ack_intr0 88
#define REG_WR_ADDR_iop_sw_cpu_rw_ack_intr0 88

typedef struct {
  unsigned int mpu_0   : 1;
  unsigned int mpu_1   : 1;
  unsigned int mpu_2   : 1;
  unsigned int mpu_3   : 1;
  unsigned int mpu_4   : 1;
  unsigned int mpu_5   : 1;
  unsigned int mpu_6   : 1;
  unsigned int mpu_7   : 1;
  unsigned int mpu_8   : 1;
  unsigned int mpu_9   : 1;
  unsigned int mpu_10  : 1;
  unsigned int mpu_11  : 1;
  unsigned int mpu_12  : 1;
  unsigned int mpu_13  : 1;
  unsigned int mpu_14  : 1;
  unsigned int mpu_15  : 1;
  unsigned int spu0_0  : 1;
  unsigned int spu0_1  : 1;
  unsigned int spu0_2  : 1;
  unsigned int spu0_3  : 1;
  unsigned int spu0_4  : 1;
  unsigned int spu0_5  : 1;
  unsigned int spu0_6  : 1;
  unsigned int spu0_7  : 1;
  unsigned int spu1_8  : 1;
  unsigned int spu1_9  : 1;
  unsigned int spu1_10 : 1;
  unsigned int spu1_11 : 1;
  unsigned int spu1_12 : 1;
  unsigned int spu1_13 : 1;
  unsigned int spu1_14 : 1;
  unsigned int spu1_15 : 1;
} reg_iop_sw_cpu_r_intr0;
#define REG_RD_ADDR_iop_sw_cpu_r_intr0 92

typedef struct {
  unsigned int mpu_0   : 1;
  unsigned int mpu_1   : 1;
  unsigned int mpu_2   : 1;
  unsigned int mpu_3   : 1;
  unsigned int mpu_4   : 1;
  unsigned int mpu_5   : 1;
  unsigned int mpu_6   : 1;
  unsigned int mpu_7   : 1;
  unsigned int mpu_8   : 1;
  unsigned int mpu_9   : 1;
  unsigned int mpu_10  : 1;
  unsigned int mpu_11  : 1;
  unsigned int mpu_12  : 1;
  unsigned int mpu_13  : 1;
  unsigned int mpu_14  : 1;
  unsigned int mpu_15  : 1;
  unsigned int spu0_0  : 1;
  unsigned int spu0_1  : 1;
  unsigned int spu0_2  : 1;
  unsigned int spu0_3  : 1;
  unsigned int spu0_4  : 1;
  unsigned int spu0_5  : 1;
  unsigned int spu0_6  : 1;
  unsigned int spu0_7  : 1;
  unsigned int spu1_8  : 1;
  unsigned int spu1_9  : 1;
  unsigned int spu1_10 : 1;
  unsigned int spu1_11 : 1;
  unsigned int spu1_12 : 1;
  unsigned int spu1_13 : 1;
  unsigned int spu1_14 : 1;
  unsigned int spu1_15 : 1;
} reg_iop_sw_cpu_r_masked_intr0;
#define REG_RD_ADDR_iop_sw_cpu_r_masked_intr0 96

typedef struct {
  unsigned int mpu_16  : 1;
  unsigned int mpu_17  : 1;
  unsigned int mpu_18  : 1;
  unsigned int mpu_19  : 1;
  unsigned int mpu_20  : 1;
  unsigned int mpu_21  : 1;
  unsigned int mpu_22  : 1;
  unsigned int mpu_23  : 1;
  unsigned int mpu_24  : 1;
  unsigned int mpu_25  : 1;
  unsigned int mpu_26  : 1;
  unsigned int mpu_27  : 1;
  unsigned int mpu_28  : 1;
  unsigned int mpu_29  : 1;
  unsigned int mpu_30  : 1;
  unsigned int mpu_31  : 1;
  unsigned int spu0_8  : 1;
  unsigned int spu0_9  : 1;
  unsigned int spu0_10 : 1;
  unsigned int spu0_11 : 1;
  unsigned int spu0_12 : 1;
  unsigned int spu0_13 : 1;
  unsigned int spu0_14 : 1;
  unsigned int spu0_15 : 1;
  unsigned int spu1_0  : 1;
  unsigned int spu1_1  : 1;
  unsigned int spu1_2  : 1;
  unsigned int spu1_3  : 1;
  unsigned int spu1_4  : 1;
  unsigned int spu1_5  : 1;
  unsigned int spu1_6  : 1;
  unsigned int spu1_7  : 1;
} reg_iop_sw_cpu_rw_intr1_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_intr1_mask 100
#define REG_WR_ADDR_iop_sw_cpu_rw_intr1_mask 100

typedef struct {
  unsigned int mpu_16  : 1;
  unsigned int mpu_17  : 1;
  unsigned int mpu_18  : 1;
  unsigned int mpu_19  : 1;
  unsigned int mpu_20  : 1;
  unsigned int mpu_21  : 1;
  unsigned int mpu_22  : 1;
  unsigned int mpu_23  : 1;
  unsigned int mpu_24  : 1;
  unsigned int mpu_25  : 1;
  unsigned int mpu_26  : 1;
  unsigned int mpu_27  : 1;
  unsigned int mpu_28  : 1;
  unsigned int mpu_29  : 1;
  unsigned int mpu_30  : 1;
  unsigned int mpu_31  : 1;
  unsigned int spu0_8  : 1;
  unsigned int spu0_9  : 1;
  unsigned int spu0_10 : 1;
  unsigned int spu0_11 : 1;
  unsigned int spu0_12 : 1;
  unsigned int spu0_13 : 1;
  unsigned int spu0_14 : 1;
  unsigned int spu0_15 : 1;
  unsigned int spu1_0  : 1;
  unsigned int spu1_1  : 1;
  unsigned int spu1_2  : 1;
  unsigned int spu1_3  : 1;
  unsigned int spu1_4  : 1;
  unsigned int spu1_5  : 1;
  unsigned int spu1_6  : 1;
  unsigned int spu1_7  : 1;
} reg_iop_sw_cpu_rw_ack_intr1;
#define REG_RD_ADDR_iop_sw_cpu_rw_ack_intr1 104
#define REG_WR_ADDR_iop_sw_cpu_rw_ack_intr1 104

typedef struct {
  unsigned int mpu_16  : 1;
  unsigned int mpu_17  : 1;
  unsigned int mpu_18  : 1;
  unsigned int mpu_19  : 1;
  unsigned int mpu_20  : 1;
  unsigned int mpu_21  : 1;
  unsigned int mpu_22  : 1;
  unsigned int mpu_23  : 1;
  unsigned int mpu_24  : 1;
  unsigned int mpu_25  : 1;
  unsigned int mpu_26  : 1;
  unsigned int mpu_27  : 1;
  unsigned int mpu_28  : 1;
  unsigned int mpu_29  : 1;
  unsigned int mpu_30  : 1;
  unsigned int mpu_31  : 1;
  unsigned int spu0_8  : 1;
  unsigned int spu0_9  : 1;
  unsigned int spu0_10 : 1;
  unsigned int spu0_11 : 1;
  unsigned int spu0_12 : 1;
  unsigned int spu0_13 : 1;
  unsigned int spu0_14 : 1;
  unsigned int spu0_15 : 1;
  unsigned int spu1_0  : 1;
  unsigned int spu1_1  : 1;
  unsigned int spu1_2  : 1;
  unsigned int spu1_3  : 1;
  unsigned int spu1_4  : 1;
  unsigned int spu1_5  : 1;
  unsigned int spu1_6  : 1;
  unsigned int spu1_7  : 1;
} reg_iop_sw_cpu_r_intr1;
#define REG_RD_ADDR_iop_sw_cpu_r_intr1 108

typedef struct {
  unsigned int mpu_16  : 1;
  unsigned int mpu_17  : 1;
  unsigned int mpu_18  : 1;
  unsigned int mpu_19  : 1;
  unsigned int mpu_20  : 1;
  unsigned int mpu_21  : 1;
  unsigned int mpu_22  : 1;
  unsigned int mpu_23  : 1;
  unsigned int mpu_24  : 1;
  unsigned int mpu_25  : 1;
  unsigned int mpu_26  : 1;
  unsigned int mpu_27  : 1;
  unsigned int mpu_28  : 1;
  unsigned int mpu_29  : 1;
  unsigned int mpu_30  : 1;
  unsigned int mpu_31  : 1;
  unsigned int spu0_8  : 1;
  unsigned int spu0_9  : 1;
  unsigned int spu0_10 : 1;
  unsigned int spu0_11 : 1;
  unsigned int spu0_12 : 1;
  unsigned int spu0_13 : 1;
  unsigned int spu0_14 : 1;
  unsigned int spu0_15 : 1;
  unsigned int spu1_0  : 1;
  unsigned int spu1_1  : 1;
  unsigned int spu1_2  : 1;
  unsigned int spu1_3  : 1;
  unsigned int spu1_4  : 1;
  unsigned int spu1_5  : 1;
  unsigned int spu1_6  : 1;
  unsigned int spu1_7  : 1;
} reg_iop_sw_cpu_r_masked_intr1;
#define REG_RD_ADDR_iop_sw_cpu_r_masked_intr1 112

typedef struct {
  unsigned int mpu_0           : 1;
  unsigned int mpu_1           : 1;
  unsigned int mpu_2           : 1;
  unsigned int mpu_3           : 1;
  unsigned int mpu_4           : 1;
  unsigned int mpu_5           : 1;
  unsigned int mpu_6           : 1;
  unsigned int mpu_7           : 1;
  unsigned int spu0_0          : 1;
  unsigned int spu0_1          : 1;
  unsigned int spu0_2          : 1;
  unsigned int spu0_3          : 1;
  unsigned int spu0_4          : 1;
  unsigned int spu0_5          : 1;
  unsigned int spu0_6          : 1;
  unsigned int spu0_7          : 1;
  unsigned int dmc_in0         : 1;
  unsigned int dmc_out0        : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int timer_grp1      : 1;
} reg_iop_sw_cpu_rw_intr2_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_intr2_mask 116
#define REG_WR_ADDR_iop_sw_cpu_rw_intr2_mask 116

typedef struct {
  unsigned int mpu_0  : 1;
  unsigned int mpu_1  : 1;
  unsigned int mpu_2  : 1;
  unsigned int mpu_3  : 1;
  unsigned int mpu_4  : 1;
  unsigned int mpu_5  : 1;
  unsigned int mpu_6  : 1;
  unsigned int mpu_7  : 1;
  unsigned int spu0_0 : 1;
  unsigned int spu0_1 : 1;
  unsigned int spu0_2 : 1;
  unsigned int spu0_3 : 1;
  unsigned int spu0_4 : 1;
  unsigned int spu0_5 : 1;
  unsigned int spu0_6 : 1;
  unsigned int spu0_7 : 1;
  unsigned int dummy1 : 16;
} reg_iop_sw_cpu_rw_ack_intr2;
#define REG_RD_ADDR_iop_sw_cpu_rw_ack_intr2 120
#define REG_WR_ADDR_iop_sw_cpu_rw_ack_intr2 120

typedef struct {
  unsigned int mpu_0           : 1;
  unsigned int mpu_1           : 1;
  unsigned int mpu_2           : 1;
  unsigned int mpu_3           : 1;
  unsigned int mpu_4           : 1;
  unsigned int mpu_5           : 1;
  unsigned int mpu_6           : 1;
  unsigned int mpu_7           : 1;
  unsigned int spu0_0          : 1;
  unsigned int spu0_1          : 1;
  unsigned int spu0_2          : 1;
  unsigned int spu0_3          : 1;
  unsigned int spu0_4          : 1;
  unsigned int spu0_5          : 1;
  unsigned int spu0_6          : 1;
  unsigned int spu0_7          : 1;
  unsigned int dmc_in0         : 1;
  unsigned int dmc_out0        : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int timer_grp1      : 1;
} reg_iop_sw_cpu_r_intr2;
#define REG_RD_ADDR_iop_sw_cpu_r_intr2 124

typedef struct {
  unsigned int mpu_0           : 1;
  unsigned int mpu_1           : 1;
  unsigned int mpu_2           : 1;
  unsigned int mpu_3           : 1;
  unsigned int mpu_4           : 1;
  unsigned int mpu_5           : 1;
  unsigned int mpu_6           : 1;
  unsigned int mpu_7           : 1;
  unsigned int spu0_0          : 1;
  unsigned int spu0_1          : 1;
  unsigned int spu0_2          : 1;
  unsigned int spu0_3          : 1;
  unsigned int spu0_4          : 1;
  unsigned int spu0_5          : 1;
  unsigned int spu0_6          : 1;
  unsigned int spu0_7          : 1;
  unsigned int dmc_in0         : 1;
  unsigned int dmc_out0        : 1;
  unsigned int fifo_in0        : 1;
  unsigned int fifo_out0       : 1;
  unsigned int fifo_in0_extra  : 1;
  unsigned int fifo_out0_extra : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp0      : 1;
  unsigned int timer_grp1      : 1;
} reg_iop_sw_cpu_r_masked_intr2;
#define REG_RD_ADDR_iop_sw_cpu_r_masked_intr2 128

typedef struct {
  unsigned int mpu_16          : 1;
  unsigned int mpu_17          : 1;
  unsigned int mpu_18          : 1;
  unsigned int mpu_19          : 1;
  unsigned int mpu_20          : 1;
  unsigned int mpu_21          : 1;
  unsigned int mpu_22          : 1;
  unsigned int mpu_23          : 1;
  unsigned int spu1_0          : 1;
  unsigned int spu1_1          : 1;
  unsigned int spu1_2          : 1;
  unsigned int spu1_3          : 1;
  unsigned int spu1_4          : 1;
  unsigned int spu1_5          : 1;
  unsigned int spu1_6          : 1;
  unsigned int spu1_7          : 1;
  unsigned int dmc_in1         : 1;
  unsigned int dmc_out1        : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int timer_grp3      : 1;
} reg_iop_sw_cpu_rw_intr3_mask;
#define REG_RD_ADDR_iop_sw_cpu_rw_intr3_mask 132
#define REG_WR_ADDR_iop_sw_cpu_rw_intr3_mask 132

typedef struct {
  unsigned int mpu_16 : 1;
  unsigned int mpu_17 : 1;
  unsigned int mpu_18 : 1;
  unsigned int mpu_19 : 1;
  unsigned int mpu_20 : 1;
  unsigned int mpu_21 : 1;
  unsigned int mpu_22 : 1;
  unsigned int mpu_23 : 1;
  unsigned int spu1_0 : 1;
  unsigned int spu1_1 : 1;
  unsigned int spu1_2 : 1;
  unsigned int spu1_3 : 1;
  unsigned int spu1_4 : 1;
  unsigned int spu1_5 : 1;
  unsigned int spu1_6 : 1;
  unsigned int spu1_7 : 1;
  unsigned int dummy1 : 16;
} reg_iop_sw_cpu_rw_ack_intr3;
#define REG_RD_ADDR_iop_sw_cpu_rw_ack_intr3 136
#define REG_WR_ADDR_iop_sw_cpu_rw_ack_intr3 136

typedef struct {
  unsigned int mpu_16          : 1;
  unsigned int mpu_17          : 1;
  unsigned int mpu_18          : 1;
  unsigned int mpu_19          : 1;
  unsigned int mpu_20          : 1;
  unsigned int mpu_21          : 1;
  unsigned int mpu_22          : 1;
  unsigned int mpu_23          : 1;
  unsigned int spu1_0          : 1;
  unsigned int spu1_1          : 1;
  unsigned int spu1_2          : 1;
  unsigned int spu1_3          : 1;
  unsigned int spu1_4          : 1;
  unsigned int spu1_5          : 1;
  unsigned int spu1_6          : 1;
  unsigned int spu1_7          : 1;
  unsigned int dmc_in1         : 1;
  unsigned int dmc_out1        : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int timer_grp3      : 1;
} reg_iop_sw_cpu_r_intr3;
#define REG_RD_ADDR_iop_sw_cpu_r_intr3 140

typedef struct {
  unsigned int mpu_16          : 1;
  unsigned int mpu_17          : 1;
  unsigned int mpu_18          : 1;
  unsigned int mpu_19          : 1;
  unsigned int mpu_20          : 1;
  unsigned int mpu_21          : 1;
  unsigned int mpu_22          : 1;
  unsigned int mpu_23          : 1;
  unsigned int spu1_0          : 1;
  unsigned int spu1_1          : 1;
  unsigned int spu1_2          : 1;
  unsigned int spu1_3          : 1;
  unsigned int spu1_4          : 1;
  unsigned int spu1_5          : 1;
  unsigned int spu1_6          : 1;
  unsigned int spu1_7          : 1;
  unsigned int dmc_in1         : 1;
  unsigned int dmc_out1        : 1;
  unsigned int fifo_in1        : 1;
  unsigned int fifo_out1       : 1;
  unsigned int fifo_in1_extra  : 1;
  unsigned int fifo_out1_extra : 1;
  unsigned int trigger_grp0    : 1;
  unsigned int trigger_grp1    : 1;
  unsigned int trigger_grp2    : 1;
  unsigned int trigger_grp3    : 1;
  unsigned int trigger_grp4    : 1;
  unsigned int trigger_grp5    : 1;
  unsigned int trigger_grp6    : 1;
  unsigned int trigger_grp7    : 1;
  unsigned int timer_grp2      : 1;
  unsigned int timer_grp3      : 1;
} reg_iop_sw_cpu_r_masked_intr3;
#define REG_RD_ADDR_iop_sw_cpu_r_masked_intr3 144


enum {
  regk_iop_sw_cpu_copy                     = 0x00000000,
  regk_iop_sw_cpu_no                       = 0x00000000,
  regk_iop_sw_cpu_rd                       = 0x00000002,
  regk_iop_sw_cpu_reg_copy                 = 0x00000001,
  regk_iop_sw_cpu_rw_bus0_clr_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus0_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus0_oe_set_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus0_set_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus1_clr_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus1_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus1_oe_set_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_bus1_set_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_gio_clr_mask_default  = 0x00000000,
  regk_iop_sw_cpu_rw_gio_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_gio_oe_set_mask_default = 0x00000000,
  regk_iop_sw_cpu_rw_gio_set_mask_default  = 0x00000000,
  regk_iop_sw_cpu_rw_intr0_mask_default    = 0x00000000,
  regk_iop_sw_cpu_rw_intr1_mask_default    = 0x00000000,
  regk_iop_sw_cpu_rw_intr2_mask_default    = 0x00000000,
  regk_iop_sw_cpu_rw_intr3_mask_default    = 0x00000000,
  regk_iop_sw_cpu_wr                       = 0x00000003,
  regk_iop_sw_cpu_yes                      = 0x00000001
};
#endif 
