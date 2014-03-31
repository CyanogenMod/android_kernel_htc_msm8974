#ifndef __iop_mpu_defs_asm_h
#define __iop_mpu_defs_asm_h


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

#define STRIDE_iop_mpu_rw_r 4
#define reg_iop_mpu_rw_r_offset 0

#define reg_iop_mpu_rw_ctrl___en___lsb 0
#define reg_iop_mpu_rw_ctrl___en___width 1
#define reg_iop_mpu_rw_ctrl___en___bit 0
#define reg_iop_mpu_rw_ctrl_offset 128

#define reg_iop_mpu_r_pc___addr___lsb 0
#define reg_iop_mpu_r_pc___addr___width 12
#define reg_iop_mpu_r_pc_offset 132

#define reg_iop_mpu_r_stat___instr_reg_busy___lsb 0
#define reg_iop_mpu_r_stat___instr_reg_busy___width 1
#define reg_iop_mpu_r_stat___instr_reg_busy___bit 0
#define reg_iop_mpu_r_stat___intr_busy___lsb 1
#define reg_iop_mpu_r_stat___intr_busy___width 1
#define reg_iop_mpu_r_stat___intr_busy___bit 1
#define reg_iop_mpu_r_stat___intr_vect___lsb 2
#define reg_iop_mpu_r_stat___intr_vect___width 16
#define reg_iop_mpu_r_stat_offset 136

#define reg_iop_mpu_rw_instr_offset 140

#define reg_iop_mpu_rw_immediate_offset 144

#define reg_iop_mpu_r_trace___intr_vect___lsb 0
#define reg_iop_mpu_r_trace___intr_vect___width 16
#define reg_iop_mpu_r_trace___pc___lsb 16
#define reg_iop_mpu_r_trace___pc___width 12
#define reg_iop_mpu_r_trace___en___lsb 28
#define reg_iop_mpu_r_trace___en___width 1
#define reg_iop_mpu_r_trace___en___bit 28
#define reg_iop_mpu_r_trace___instr_reg_busy___lsb 29
#define reg_iop_mpu_r_trace___instr_reg_busy___width 1
#define reg_iop_mpu_r_trace___instr_reg_busy___bit 29
#define reg_iop_mpu_r_trace___intr_busy___lsb 30
#define reg_iop_mpu_r_trace___intr_busy___width 1
#define reg_iop_mpu_r_trace___intr_busy___bit 30
#define reg_iop_mpu_r_trace_offset 148

#define reg_iop_mpu_r_wr_stat___r0___lsb 0
#define reg_iop_mpu_r_wr_stat___r0___width 1
#define reg_iop_mpu_r_wr_stat___r0___bit 0
#define reg_iop_mpu_r_wr_stat___r1___lsb 1
#define reg_iop_mpu_r_wr_stat___r1___width 1
#define reg_iop_mpu_r_wr_stat___r1___bit 1
#define reg_iop_mpu_r_wr_stat___r2___lsb 2
#define reg_iop_mpu_r_wr_stat___r2___width 1
#define reg_iop_mpu_r_wr_stat___r2___bit 2
#define reg_iop_mpu_r_wr_stat___r3___lsb 3
#define reg_iop_mpu_r_wr_stat___r3___width 1
#define reg_iop_mpu_r_wr_stat___r3___bit 3
#define reg_iop_mpu_r_wr_stat___r4___lsb 4
#define reg_iop_mpu_r_wr_stat___r4___width 1
#define reg_iop_mpu_r_wr_stat___r4___bit 4
#define reg_iop_mpu_r_wr_stat___r5___lsb 5
#define reg_iop_mpu_r_wr_stat___r5___width 1
#define reg_iop_mpu_r_wr_stat___r5___bit 5
#define reg_iop_mpu_r_wr_stat___r6___lsb 6
#define reg_iop_mpu_r_wr_stat___r6___width 1
#define reg_iop_mpu_r_wr_stat___r6___bit 6
#define reg_iop_mpu_r_wr_stat___r7___lsb 7
#define reg_iop_mpu_r_wr_stat___r7___width 1
#define reg_iop_mpu_r_wr_stat___r7___bit 7
#define reg_iop_mpu_r_wr_stat___r8___lsb 8
#define reg_iop_mpu_r_wr_stat___r8___width 1
#define reg_iop_mpu_r_wr_stat___r8___bit 8
#define reg_iop_mpu_r_wr_stat___r9___lsb 9
#define reg_iop_mpu_r_wr_stat___r9___width 1
#define reg_iop_mpu_r_wr_stat___r9___bit 9
#define reg_iop_mpu_r_wr_stat___r10___lsb 10
#define reg_iop_mpu_r_wr_stat___r10___width 1
#define reg_iop_mpu_r_wr_stat___r10___bit 10
#define reg_iop_mpu_r_wr_stat___r11___lsb 11
#define reg_iop_mpu_r_wr_stat___r11___width 1
#define reg_iop_mpu_r_wr_stat___r11___bit 11
#define reg_iop_mpu_r_wr_stat___r12___lsb 12
#define reg_iop_mpu_r_wr_stat___r12___width 1
#define reg_iop_mpu_r_wr_stat___r12___bit 12
#define reg_iop_mpu_r_wr_stat___r13___lsb 13
#define reg_iop_mpu_r_wr_stat___r13___width 1
#define reg_iop_mpu_r_wr_stat___r13___bit 13
#define reg_iop_mpu_r_wr_stat___r14___lsb 14
#define reg_iop_mpu_r_wr_stat___r14___width 1
#define reg_iop_mpu_r_wr_stat___r14___bit 14
#define reg_iop_mpu_r_wr_stat___r15___lsb 15
#define reg_iop_mpu_r_wr_stat___r15___width 1
#define reg_iop_mpu_r_wr_stat___r15___bit 15
#define reg_iop_mpu_r_wr_stat_offset 152

#define STRIDE_iop_mpu_rw_thread 4
#define reg_iop_mpu_rw_thread___addr___lsb 0
#define reg_iop_mpu_rw_thread___addr___width 12
#define reg_iop_mpu_rw_thread_offset 156

#define STRIDE_iop_mpu_rw_intr 4
#define reg_iop_mpu_rw_intr___addr___lsb 0
#define reg_iop_mpu_rw_intr___addr___width 12
#define reg_iop_mpu_rw_intr_offset 196


#define regk_iop_mpu_no                           0x00000000
#define regk_iop_mpu_r_pc_default                 0x00000000
#define regk_iop_mpu_rw_ctrl_default              0x00000000
#define regk_iop_mpu_rw_intr_size                 0x00000010
#define regk_iop_mpu_rw_r_size                    0x00000010
#define regk_iop_mpu_rw_thread_default            0x00000000
#define regk_iop_mpu_rw_thread_size               0x00000004
#define regk_iop_mpu_yes                          0x00000001
#endif 
