#ifndef __timer_defs_h
#define __timer_defs_h

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


typedef unsigned int reg_timer_rw_tmr0_div;
#define REG_RD_ADDR_timer_rw_tmr0_div 0
#define REG_WR_ADDR_timer_rw_tmr0_div 0

typedef unsigned int reg_timer_r_tmr0_data;
#define REG_RD_ADDR_timer_r_tmr0_data 4

typedef struct {
  unsigned int op   : 2;
  unsigned int freq : 3;
  unsigned int dummy1 : 27;
} reg_timer_rw_tmr0_ctrl;
#define REG_RD_ADDR_timer_rw_tmr0_ctrl 8
#define REG_WR_ADDR_timer_rw_tmr0_ctrl 8

typedef unsigned int reg_timer_rw_tmr1_div;
#define REG_RD_ADDR_timer_rw_tmr1_div 16
#define REG_WR_ADDR_timer_rw_tmr1_div 16

typedef unsigned int reg_timer_r_tmr1_data;
#define REG_RD_ADDR_timer_r_tmr1_data 20

typedef struct {
  unsigned int op   : 2;
  unsigned int freq : 3;
  unsigned int dummy1 : 27;
} reg_timer_rw_tmr1_ctrl;
#define REG_RD_ADDR_timer_rw_tmr1_ctrl 24
#define REG_WR_ADDR_timer_rw_tmr1_ctrl 24

typedef struct {
  unsigned int tmr : 24;
  unsigned int cnt : 8;
} reg_timer_rs_cnt_data;
#define REG_RD_ADDR_timer_rs_cnt_data 32

typedef struct {
  unsigned int tmr : 24;
  unsigned int cnt : 8;
} reg_timer_r_cnt_data;
#define REG_RD_ADDR_timer_r_cnt_data 36

typedef struct {
  unsigned int clk : 2;
  unsigned int dummy1 : 30;
} reg_timer_rw_cnt_cfg;
#define REG_RD_ADDR_timer_rw_cnt_cfg 40
#define REG_WR_ADDR_timer_rw_cnt_cfg 40

typedef unsigned int reg_timer_rw_trig;
#define REG_RD_ADDR_timer_rw_trig 48
#define REG_WR_ADDR_timer_rw_trig 48

typedef struct {
  unsigned int tmr : 2;
  unsigned int dummy1 : 30;
} reg_timer_rw_trig_cfg;
#define REG_RD_ADDR_timer_rw_trig_cfg 52
#define REG_WR_ADDR_timer_rw_trig_cfg 52

typedef unsigned int reg_timer_r_time;
#define REG_RD_ADDR_timer_r_time 56

typedef struct {
  unsigned int tmr : 2;
  unsigned int dummy1 : 30;
} reg_timer_rw_out;
#define REG_RD_ADDR_timer_rw_out 60
#define REG_WR_ADDR_timer_rw_out 60

typedef struct {
  unsigned int cnt : 8;
  unsigned int cmd : 1;
  unsigned int key : 7;
  unsigned int dummy1 : 16;
} reg_timer_rw_wd_ctrl;
#define REG_RD_ADDR_timer_rw_wd_ctrl 64
#define REG_WR_ADDR_timer_rw_wd_ctrl 64

typedef struct {
  unsigned int cnt : 8;
  unsigned int cmd : 1;
  unsigned int dummy1 : 23;
} reg_timer_r_wd_stat;
#define REG_RD_ADDR_timer_r_wd_stat 68

typedef struct {
  unsigned int tmr0 : 1;
  unsigned int tmr1 : 1;
  unsigned int cnt  : 1;
  unsigned int trig : 1;
  unsigned int dummy1 : 28;
} reg_timer_rw_intr_mask;
#define REG_RD_ADDR_timer_rw_intr_mask 72
#define REG_WR_ADDR_timer_rw_intr_mask 72

typedef struct {
  unsigned int tmr0 : 1;
  unsigned int tmr1 : 1;
  unsigned int cnt  : 1;
  unsigned int trig : 1;
  unsigned int dummy1 : 28;
} reg_timer_rw_ack_intr;
#define REG_RD_ADDR_timer_rw_ack_intr 76
#define REG_WR_ADDR_timer_rw_ack_intr 76

typedef struct {
  unsigned int tmr0 : 1;
  unsigned int tmr1 : 1;
  unsigned int cnt  : 1;
  unsigned int trig : 1;
  unsigned int dummy1 : 28;
} reg_timer_r_intr;
#define REG_RD_ADDR_timer_r_intr 80

typedef struct {
  unsigned int tmr0 : 1;
  unsigned int tmr1 : 1;
  unsigned int cnt  : 1;
  unsigned int trig : 1;
  unsigned int dummy1 : 28;
} reg_timer_r_masked_intr;
#define REG_RD_ADDR_timer_r_masked_intr 84

typedef struct {
  unsigned int dis : 1;
  unsigned int en  : 1;
  unsigned int dummy1 : 30;
} reg_timer_rw_test;
#define REG_RD_ADDR_timer_rw_test 88
#define REG_WR_ADDR_timer_rw_test 88


enum {
  regk_timer_ext                           = 0x00000001,
  regk_timer_f100                          = 0x00000007,
  regk_timer_f29_493                       = 0x00000004,
  regk_timer_f32                           = 0x00000005,
  regk_timer_f32_768                       = 0x00000006,
  regk_timer_hold                          = 0x00000001,
  regk_timer_ld                            = 0x00000000,
  regk_timer_no                            = 0x00000000,
  regk_timer_off                           = 0x00000000,
  regk_timer_run                           = 0x00000002,
  regk_timer_rw_cnt_cfg_default            = 0x00000000,
  regk_timer_rw_intr_mask_default          = 0x00000000,
  regk_timer_rw_out_default                = 0x00000000,
  regk_timer_rw_test_default               = 0x00000000,
  regk_timer_rw_tmr0_ctrl_default          = 0x00000000,
  regk_timer_rw_tmr1_ctrl_default          = 0x00000000,
  regk_timer_rw_trig_cfg_default           = 0x00000000,
  regk_timer_start                         = 0x00000001,
  regk_timer_stop                          = 0x00000000,
  regk_timer_time                          = 0x00000001,
  regk_timer_tmr0                          = 0x00000002,
  regk_timer_tmr1                          = 0x00000003,
  regk_timer_yes                           = 0x00000001
};
#endif 
