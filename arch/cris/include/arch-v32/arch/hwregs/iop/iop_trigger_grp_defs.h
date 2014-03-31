#ifndef __iop_trigger_grp_defs_h
#define __iop_trigger_grp_defs_h

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


#define STRIDE_iop_trigger_grp_rw_cfg 4
typedef struct {
  unsigned int action          : 2;
  unsigned int once            : 1;
  unsigned int trig            : 3;
  unsigned int en_only_by_reg  : 1;
  unsigned int dis_only_by_reg : 1;
  unsigned int dummy1          : 24;
} reg_iop_trigger_grp_rw_cfg;
#define REG_RD_ADDR_iop_trigger_grp_rw_cfg 0
#define REG_WR_ADDR_iop_trigger_grp_rw_cfg 0

typedef struct {
  unsigned int dis : 4;
  unsigned int en  : 4;
  unsigned int dummy1 : 24;
} reg_iop_trigger_grp_rw_cmd;
#define REG_RD_ADDR_iop_trigger_grp_rw_cmd 16
#define REG_WR_ADDR_iop_trigger_grp_rw_cmd 16

typedef struct {
  unsigned int trig0 : 1;
  unsigned int trig1 : 1;
  unsigned int trig2 : 1;
  unsigned int trig3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_trigger_grp_rw_intr_mask;
#define REG_RD_ADDR_iop_trigger_grp_rw_intr_mask 20
#define REG_WR_ADDR_iop_trigger_grp_rw_intr_mask 20

typedef struct {
  unsigned int trig0 : 1;
  unsigned int trig1 : 1;
  unsigned int trig2 : 1;
  unsigned int trig3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_trigger_grp_rw_ack_intr;
#define REG_RD_ADDR_iop_trigger_grp_rw_ack_intr 24
#define REG_WR_ADDR_iop_trigger_grp_rw_ack_intr 24

typedef struct {
  unsigned int trig0 : 1;
  unsigned int trig1 : 1;
  unsigned int trig2 : 1;
  unsigned int trig3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_trigger_grp_r_intr;
#define REG_RD_ADDR_iop_trigger_grp_r_intr 28

typedef struct {
  unsigned int trig0 : 1;
  unsigned int trig1 : 1;
  unsigned int trig2 : 1;
  unsigned int trig3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_trigger_grp_r_masked_intr;
#define REG_RD_ADDR_iop_trigger_grp_r_masked_intr 32


enum {
  regk_iop_trigger_grp_fall                = 0x00000002,
  regk_iop_trigger_grp_fall_lo             = 0x00000006,
  regk_iop_trigger_grp_no                  = 0x00000000,
  regk_iop_trigger_grp_off                 = 0x00000000,
  regk_iop_trigger_grp_pulse               = 0x00000000,
  regk_iop_trigger_grp_rise                = 0x00000001,
  regk_iop_trigger_grp_rise_fall           = 0x00000003,
  regk_iop_trigger_grp_rise_fall_hi        = 0x00000007,
  regk_iop_trigger_grp_rise_fall_lo        = 0x00000004,
  regk_iop_trigger_grp_rise_hi             = 0x00000005,
  regk_iop_trigger_grp_rw_cfg_default      = 0x000000c0,
  regk_iop_trigger_grp_rw_cfg_size         = 0x00000004,
  regk_iop_trigger_grp_rw_intr_mask_default = 0x00000000,
  regk_iop_trigger_grp_toggle              = 0x00000003,
  regk_iop_trigger_grp_yes                 = 0x00000001
};
#endif 
