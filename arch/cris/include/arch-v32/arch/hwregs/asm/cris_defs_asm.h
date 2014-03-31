#ifndef __cris_defs_asm_h
#define __cris_defs_asm_h


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

#define reg_cris_rw_gc_cfg___ic___lsb 0
#define reg_cris_rw_gc_cfg___ic___width 1
#define reg_cris_rw_gc_cfg___ic___bit 0
#define reg_cris_rw_gc_cfg___dc___lsb 1
#define reg_cris_rw_gc_cfg___dc___width 1
#define reg_cris_rw_gc_cfg___dc___bit 1
#define reg_cris_rw_gc_cfg___im___lsb 2
#define reg_cris_rw_gc_cfg___im___width 1
#define reg_cris_rw_gc_cfg___im___bit 2
#define reg_cris_rw_gc_cfg___dm___lsb 3
#define reg_cris_rw_gc_cfg___dm___width 1
#define reg_cris_rw_gc_cfg___dm___bit 3
#define reg_cris_rw_gc_cfg___gb___lsb 4
#define reg_cris_rw_gc_cfg___gb___width 1
#define reg_cris_rw_gc_cfg___gb___bit 4
#define reg_cris_rw_gc_cfg___gk___lsb 5
#define reg_cris_rw_gc_cfg___gk___width 1
#define reg_cris_rw_gc_cfg___gk___bit 5
#define reg_cris_rw_gc_cfg___gp___lsb 6
#define reg_cris_rw_gc_cfg___gp___width 1
#define reg_cris_rw_gc_cfg___gp___bit 6
#define reg_cris_rw_gc_cfg_offset 0

#define reg_cris_rw_gc_ccs_offset 4

#define reg_cris_rw_gc_srs___srs___lsb 0
#define reg_cris_rw_gc_srs___srs___width 8
#define reg_cris_rw_gc_srs_offset 8

#define reg_cris_rw_gc_nrp_offset 12

#define reg_cris_rw_gc_exs_offset 16

#define reg_cris_rw_gc_eda_offset 20

#define reg_cris_rw_gc_r0_offset 32

#define reg_cris_rw_gc_r1_offset 36

#define reg_cris_rw_gc_r2_offset 40

#define reg_cris_rw_gc_r3_offset 44


#define regk_cris_no                              0x00000000
#define regk_cris_rw_gc_cfg_default               0x00000000
#define regk_cris_yes                             0x00000001
#endif 
