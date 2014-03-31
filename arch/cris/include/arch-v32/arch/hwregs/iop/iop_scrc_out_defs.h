#ifndef __iop_scrc_out_defs_h
#define __iop_scrc_out_defs_h

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
  unsigned int trig    : 2;
  unsigned int inv_crc : 1;
  unsigned int dummy1  : 29;
} reg_iop_scrc_out_rw_cfg;
#define REG_RD_ADDR_iop_scrc_out_rw_cfg 0
#define REG_WR_ADDR_iop_scrc_out_rw_cfg 0

typedef struct {
  unsigned int strb_src : 1;
  unsigned int out_src  : 1;
  unsigned int dummy1   : 30;
} reg_iop_scrc_out_rw_ctrl;
#define REG_RD_ADDR_iop_scrc_out_rw_ctrl 4
#define REG_WR_ADDR_iop_scrc_out_rw_ctrl 4

typedef unsigned int reg_iop_scrc_out_rw_init_crc;
#define REG_RD_ADDR_iop_scrc_out_rw_init_crc 8
#define REG_WR_ADDR_iop_scrc_out_rw_init_crc 8

typedef unsigned int reg_iop_scrc_out_rw_crc;
#define REG_RD_ADDR_iop_scrc_out_rw_crc 12
#define REG_WR_ADDR_iop_scrc_out_rw_crc 12

typedef struct {
  unsigned int val : 1;
  unsigned int dummy1 : 31;
} reg_iop_scrc_out_rw_data;
#define REG_RD_ADDR_iop_scrc_out_rw_data 16
#define REG_WR_ADDR_iop_scrc_out_rw_data 16

typedef unsigned int reg_iop_scrc_out_r_computed_crc;
#define REG_RD_ADDR_iop_scrc_out_r_computed_crc 20


enum {
  regk_iop_scrc_out_crc                    = 0x00000001,
  regk_iop_scrc_out_data                   = 0x00000000,
  regk_iop_scrc_out_dif                    = 0x00000001,
  regk_iop_scrc_out_hi                     = 0x00000000,
  regk_iop_scrc_out_neg                    = 0x00000002,
  regk_iop_scrc_out_no                     = 0x00000000,
  regk_iop_scrc_out_pos                    = 0x00000001,
  regk_iop_scrc_out_pos_neg                = 0x00000003,
  regk_iop_scrc_out_reg                    = 0x00000000,
  regk_iop_scrc_out_rw_cfg_default         = 0x00000000,
  regk_iop_scrc_out_rw_crc_default         = 0x00000000,
  regk_iop_scrc_out_rw_ctrl_default        = 0x00000000,
  regk_iop_scrc_out_rw_data_default        = 0x00000000,
  regk_iop_scrc_out_rw_init_crc_default    = 0x00000000,
  regk_iop_scrc_out_yes                    = 0x00000001
};
#endif 
