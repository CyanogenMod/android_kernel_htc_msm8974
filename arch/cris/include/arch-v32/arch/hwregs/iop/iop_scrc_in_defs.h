#ifndef __iop_scrc_in_defs_h
#define __iop_scrc_in_defs_h

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
  unsigned int trig : 2;
  unsigned int dummy1 : 30;
} reg_iop_scrc_in_rw_cfg;
#define REG_RD_ADDR_iop_scrc_in_rw_cfg 0
#define REG_WR_ADDR_iop_scrc_in_rw_cfg 0

typedef struct {
  unsigned int dif_in_en : 1;
  unsigned int dummy1    : 31;
} reg_iop_scrc_in_rw_ctrl;
#define REG_RD_ADDR_iop_scrc_in_rw_ctrl 4
#define REG_WR_ADDR_iop_scrc_in_rw_ctrl 4

typedef struct {
  unsigned int err : 1;
  unsigned int dummy1 : 31;
} reg_iop_scrc_in_r_stat;
#define REG_RD_ADDR_iop_scrc_in_r_stat 8

typedef unsigned int reg_iop_scrc_in_rw_init_crc;
#define REG_RD_ADDR_iop_scrc_in_rw_init_crc 12
#define REG_WR_ADDR_iop_scrc_in_rw_init_crc 12

typedef unsigned int reg_iop_scrc_in_rs_computed_crc;
#define REG_RD_ADDR_iop_scrc_in_rs_computed_crc 16

typedef unsigned int reg_iop_scrc_in_r_computed_crc;
#define REG_RD_ADDR_iop_scrc_in_r_computed_crc 20

typedef unsigned int reg_iop_scrc_in_rw_crc;
#define REG_RD_ADDR_iop_scrc_in_rw_crc 24
#define REG_WR_ADDR_iop_scrc_in_rw_crc 24

typedef unsigned int reg_iop_scrc_in_rw_correct_crc;
#define REG_RD_ADDR_iop_scrc_in_rw_correct_crc 28
#define REG_WR_ADDR_iop_scrc_in_rw_correct_crc 28

typedef struct {
  unsigned int data : 2;
  unsigned int last : 2;
  unsigned int dummy1 : 28;
} reg_iop_scrc_in_rw_wr1bit;
#define REG_RD_ADDR_iop_scrc_in_rw_wr1bit 32
#define REG_WR_ADDR_iop_scrc_in_rw_wr1bit 32


enum {
  regk_iop_scrc_in_dif_in                  = 0x00000002,
  regk_iop_scrc_in_hi                      = 0x00000000,
  regk_iop_scrc_in_neg                     = 0x00000002,
  regk_iop_scrc_in_no                      = 0x00000000,
  regk_iop_scrc_in_pos                     = 0x00000001,
  regk_iop_scrc_in_pos_neg                 = 0x00000003,
  regk_iop_scrc_in_r_computed_crc_default  = 0x00000000,
  regk_iop_scrc_in_rs_computed_crc_default = 0x00000000,
  regk_iop_scrc_in_rw_cfg_default          = 0x00000000,
  regk_iop_scrc_in_rw_ctrl_default         = 0x00000000,
  regk_iop_scrc_in_rw_init_crc_default     = 0x00000000,
  regk_iop_scrc_in_set0                    = 0x00000000,
  regk_iop_scrc_in_set1                    = 0x00000001,
  regk_iop_scrc_in_yes                     = 0x00000001
};
#endif 
