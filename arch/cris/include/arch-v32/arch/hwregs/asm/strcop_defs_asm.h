#ifndef __strcop_defs_asm_h
#define __strcop_defs_asm_h


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

#define reg_strcop_rw_cfg___td3___lsb 0
#define reg_strcop_rw_cfg___td3___width 1
#define reg_strcop_rw_cfg___td3___bit 0
#define reg_strcop_rw_cfg___td2___lsb 1
#define reg_strcop_rw_cfg___td2___width 1
#define reg_strcop_rw_cfg___td2___bit 1
#define reg_strcop_rw_cfg___td1___lsb 2
#define reg_strcop_rw_cfg___td1___width 1
#define reg_strcop_rw_cfg___td1___bit 2
#define reg_strcop_rw_cfg___ipend___lsb 3
#define reg_strcop_rw_cfg___ipend___width 1
#define reg_strcop_rw_cfg___ipend___bit 3
#define reg_strcop_rw_cfg___ignore_sync___lsb 4
#define reg_strcop_rw_cfg___ignore_sync___width 1
#define reg_strcop_rw_cfg___ignore_sync___bit 4
#define reg_strcop_rw_cfg___en___lsb 5
#define reg_strcop_rw_cfg___en___width 1
#define reg_strcop_rw_cfg___en___bit 5
#define reg_strcop_rw_cfg_offset 0


#define regk_strcop_big                           0x00000001
#define regk_strcop_d                             0x00000001
#define regk_strcop_e                             0x00000000
#define regk_strcop_little                        0x00000000
#define regk_strcop_rw_cfg_default                0x00000002
#endif 
