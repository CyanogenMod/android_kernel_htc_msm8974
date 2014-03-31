#ifndef __iop_scrc_out_defs_asm_h
#define __iop_scrc_out_defs_asm_h


#ifndef REG_FIELD
#define REG_FIELD( scope, reg, field, value ) \
  REG_FIELD_X_( value, reg_##scope##_##reg##___##field##___lsb )
#define REG_FIELD_X_( value, shift ) ((value) << shift)
#endif

#ifndef REG_STATE
#define REG_STATE( scope, reg, field, symbolic_value ) \
  REG_STATE_X_( regk_##scope##_##symbolic_value, reg_##scope##_##reg##___##field##___lsb )
#define REG_STATE_X_( k, shift ) (k << shift)
#endif

#ifndef REG_MASK
#define REG_MASK( scope, reg, field ) \
  REG_MASK_X_( reg_##scope##_##reg##___##field##___width, reg_##scope##_##reg##___##field##___lsb )
#define REG_MASK_X_( width, lsb ) (((1 << width)-1) << lsb)
#endif

#ifndef REG_LSB
#define REG_LSB( scope, reg, field ) reg_##scope##_##reg##___##field##___lsb
#endif

#ifndef REG_BIT
#define REG_BIT( scope, reg, field ) reg_##scope##_##reg##___##field##___bit
#endif

#ifndef REG_ADDR
#define REG_ADDR( scope, inst, reg ) REG_ADDR_X_(inst, reg_##scope##_##reg##_offset)
#define REG_ADDR_X_( inst, offs ) ((inst) + offs)
#endif

#ifndef REG_ADDR_VECT
#define REG_ADDR_VECT( scope, inst, reg, index ) \
         REG_ADDR_VECT_X_(inst, reg_##scope##_##reg##_offset, index, \
			 STRIDE_##scope##_##reg )
#define REG_ADDR_VECT_X_( inst, offs, index, stride ) \
                          ((inst) + offs + (index) * stride)
#endif

#define reg_iop_scrc_out_rw_cfg___trig___lsb 0
#define reg_iop_scrc_out_rw_cfg___trig___width 2
#define reg_iop_scrc_out_rw_cfg___inv_crc___lsb 2
#define reg_iop_scrc_out_rw_cfg___inv_crc___width 1
#define reg_iop_scrc_out_rw_cfg___inv_crc___bit 2
#define reg_iop_scrc_out_rw_cfg_offset 0

#define reg_iop_scrc_out_rw_ctrl___strb_src___lsb 0
#define reg_iop_scrc_out_rw_ctrl___strb_src___width 1
#define reg_iop_scrc_out_rw_ctrl___strb_src___bit 0
#define reg_iop_scrc_out_rw_ctrl___out_src___lsb 1
#define reg_iop_scrc_out_rw_ctrl___out_src___width 1
#define reg_iop_scrc_out_rw_ctrl___out_src___bit 1
#define reg_iop_scrc_out_rw_ctrl_offset 4

#define reg_iop_scrc_out_rw_init_crc_offset 8

#define reg_iop_scrc_out_rw_crc_offset 12

#define reg_iop_scrc_out_rw_data___val___lsb 0
#define reg_iop_scrc_out_rw_data___val___width 1
#define reg_iop_scrc_out_rw_data___val___bit 0
#define reg_iop_scrc_out_rw_data_offset 16

#define reg_iop_scrc_out_r_computed_crc_offset 20


#define regk_iop_scrc_out_crc                     0x00000001
#define regk_iop_scrc_out_data                    0x00000000
#define regk_iop_scrc_out_dif                     0x00000001
#define regk_iop_scrc_out_hi                      0x00000000
#define regk_iop_scrc_out_neg                     0x00000002
#define regk_iop_scrc_out_no                      0x00000000
#define regk_iop_scrc_out_pos                     0x00000001
#define regk_iop_scrc_out_pos_neg                 0x00000003
#define regk_iop_scrc_out_reg                     0x00000000
#define regk_iop_scrc_out_rw_cfg_default          0x00000000
#define regk_iop_scrc_out_rw_crc_default          0x00000000
#define regk_iop_scrc_out_rw_ctrl_default         0x00000000
#define regk_iop_scrc_out_rw_data_default         0x00000000
#define regk_iop_scrc_out_rw_init_crc_default     0x00000000
#define regk_iop_scrc_out_yes                     0x00000001
#endif 
