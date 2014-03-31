#ifndef __gio_defs_asm_h
#define __gio_defs_asm_h


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

#define reg_gio_rw_pa_dout___data___lsb 0
#define reg_gio_rw_pa_dout___data___width 8
#define reg_gio_rw_pa_dout_offset 0

#define reg_gio_r_pa_din___data___lsb 0
#define reg_gio_r_pa_din___data___width 8
#define reg_gio_r_pa_din_offset 4

#define reg_gio_rw_pa_oe___oe___lsb 0
#define reg_gio_rw_pa_oe___oe___width 8
#define reg_gio_rw_pa_oe_offset 8

#define reg_gio_rw_intr_cfg___pa0___lsb 0
#define reg_gio_rw_intr_cfg___pa0___width 3
#define reg_gio_rw_intr_cfg___pa1___lsb 3
#define reg_gio_rw_intr_cfg___pa1___width 3
#define reg_gio_rw_intr_cfg___pa2___lsb 6
#define reg_gio_rw_intr_cfg___pa2___width 3
#define reg_gio_rw_intr_cfg___pa3___lsb 9
#define reg_gio_rw_intr_cfg___pa3___width 3
#define reg_gio_rw_intr_cfg___pa4___lsb 12
#define reg_gio_rw_intr_cfg___pa4___width 3
#define reg_gio_rw_intr_cfg___pa5___lsb 15
#define reg_gio_rw_intr_cfg___pa5___width 3
#define reg_gio_rw_intr_cfg___pa6___lsb 18
#define reg_gio_rw_intr_cfg___pa6___width 3
#define reg_gio_rw_intr_cfg___pa7___lsb 21
#define reg_gio_rw_intr_cfg___pa7___width 3
#define reg_gio_rw_intr_cfg_offset 12

#define reg_gio_rw_intr_mask___pa0___lsb 0
#define reg_gio_rw_intr_mask___pa0___width 1
#define reg_gio_rw_intr_mask___pa0___bit 0
#define reg_gio_rw_intr_mask___pa1___lsb 1
#define reg_gio_rw_intr_mask___pa1___width 1
#define reg_gio_rw_intr_mask___pa1___bit 1
#define reg_gio_rw_intr_mask___pa2___lsb 2
#define reg_gio_rw_intr_mask___pa2___width 1
#define reg_gio_rw_intr_mask___pa2___bit 2
#define reg_gio_rw_intr_mask___pa3___lsb 3
#define reg_gio_rw_intr_mask___pa3___width 1
#define reg_gio_rw_intr_mask___pa3___bit 3
#define reg_gio_rw_intr_mask___pa4___lsb 4
#define reg_gio_rw_intr_mask___pa4___width 1
#define reg_gio_rw_intr_mask___pa4___bit 4
#define reg_gio_rw_intr_mask___pa5___lsb 5
#define reg_gio_rw_intr_mask___pa5___width 1
#define reg_gio_rw_intr_mask___pa5___bit 5
#define reg_gio_rw_intr_mask___pa6___lsb 6
#define reg_gio_rw_intr_mask___pa6___width 1
#define reg_gio_rw_intr_mask___pa6___bit 6
#define reg_gio_rw_intr_mask___pa7___lsb 7
#define reg_gio_rw_intr_mask___pa7___width 1
#define reg_gio_rw_intr_mask___pa7___bit 7
#define reg_gio_rw_intr_mask_offset 16

#define reg_gio_rw_ack_intr___pa0___lsb 0
#define reg_gio_rw_ack_intr___pa0___width 1
#define reg_gio_rw_ack_intr___pa0___bit 0
#define reg_gio_rw_ack_intr___pa1___lsb 1
#define reg_gio_rw_ack_intr___pa1___width 1
#define reg_gio_rw_ack_intr___pa1___bit 1
#define reg_gio_rw_ack_intr___pa2___lsb 2
#define reg_gio_rw_ack_intr___pa2___width 1
#define reg_gio_rw_ack_intr___pa2___bit 2
#define reg_gio_rw_ack_intr___pa3___lsb 3
#define reg_gio_rw_ack_intr___pa3___width 1
#define reg_gio_rw_ack_intr___pa3___bit 3
#define reg_gio_rw_ack_intr___pa4___lsb 4
#define reg_gio_rw_ack_intr___pa4___width 1
#define reg_gio_rw_ack_intr___pa4___bit 4
#define reg_gio_rw_ack_intr___pa5___lsb 5
#define reg_gio_rw_ack_intr___pa5___width 1
#define reg_gio_rw_ack_intr___pa5___bit 5
#define reg_gio_rw_ack_intr___pa6___lsb 6
#define reg_gio_rw_ack_intr___pa6___width 1
#define reg_gio_rw_ack_intr___pa6___bit 6
#define reg_gio_rw_ack_intr___pa7___lsb 7
#define reg_gio_rw_ack_intr___pa7___width 1
#define reg_gio_rw_ack_intr___pa7___bit 7
#define reg_gio_rw_ack_intr_offset 20

#define reg_gio_r_intr___pa0___lsb 0
#define reg_gio_r_intr___pa0___width 1
#define reg_gio_r_intr___pa0___bit 0
#define reg_gio_r_intr___pa1___lsb 1
#define reg_gio_r_intr___pa1___width 1
#define reg_gio_r_intr___pa1___bit 1
#define reg_gio_r_intr___pa2___lsb 2
#define reg_gio_r_intr___pa2___width 1
#define reg_gio_r_intr___pa2___bit 2
#define reg_gio_r_intr___pa3___lsb 3
#define reg_gio_r_intr___pa3___width 1
#define reg_gio_r_intr___pa3___bit 3
#define reg_gio_r_intr___pa4___lsb 4
#define reg_gio_r_intr___pa4___width 1
#define reg_gio_r_intr___pa4___bit 4
#define reg_gio_r_intr___pa5___lsb 5
#define reg_gio_r_intr___pa5___width 1
#define reg_gio_r_intr___pa5___bit 5
#define reg_gio_r_intr___pa6___lsb 6
#define reg_gio_r_intr___pa6___width 1
#define reg_gio_r_intr___pa6___bit 6
#define reg_gio_r_intr___pa7___lsb 7
#define reg_gio_r_intr___pa7___width 1
#define reg_gio_r_intr___pa7___bit 7
#define reg_gio_r_intr_offset 24

#define reg_gio_r_masked_intr___pa0___lsb 0
#define reg_gio_r_masked_intr___pa0___width 1
#define reg_gio_r_masked_intr___pa0___bit 0
#define reg_gio_r_masked_intr___pa1___lsb 1
#define reg_gio_r_masked_intr___pa1___width 1
#define reg_gio_r_masked_intr___pa1___bit 1
#define reg_gio_r_masked_intr___pa2___lsb 2
#define reg_gio_r_masked_intr___pa2___width 1
#define reg_gio_r_masked_intr___pa2___bit 2
#define reg_gio_r_masked_intr___pa3___lsb 3
#define reg_gio_r_masked_intr___pa3___width 1
#define reg_gio_r_masked_intr___pa3___bit 3
#define reg_gio_r_masked_intr___pa4___lsb 4
#define reg_gio_r_masked_intr___pa4___width 1
#define reg_gio_r_masked_intr___pa4___bit 4
#define reg_gio_r_masked_intr___pa5___lsb 5
#define reg_gio_r_masked_intr___pa5___width 1
#define reg_gio_r_masked_intr___pa5___bit 5
#define reg_gio_r_masked_intr___pa6___lsb 6
#define reg_gio_r_masked_intr___pa6___width 1
#define reg_gio_r_masked_intr___pa6___bit 6
#define reg_gio_r_masked_intr___pa7___lsb 7
#define reg_gio_r_masked_intr___pa7___width 1
#define reg_gio_r_masked_intr___pa7___bit 7
#define reg_gio_r_masked_intr_offset 28

#define reg_gio_rw_pb_dout___data___lsb 0
#define reg_gio_rw_pb_dout___data___width 18
#define reg_gio_rw_pb_dout_offset 32

#define reg_gio_r_pb_din___data___lsb 0
#define reg_gio_r_pb_din___data___width 18
#define reg_gio_r_pb_din_offset 36

#define reg_gio_rw_pb_oe___oe___lsb 0
#define reg_gio_rw_pb_oe___oe___width 18
#define reg_gio_rw_pb_oe_offset 40

#define reg_gio_rw_pc_dout___data___lsb 0
#define reg_gio_rw_pc_dout___data___width 18
#define reg_gio_rw_pc_dout_offset 48

#define reg_gio_r_pc_din___data___lsb 0
#define reg_gio_r_pc_din___data___width 18
#define reg_gio_r_pc_din_offset 52

#define reg_gio_rw_pc_oe___oe___lsb 0
#define reg_gio_rw_pc_oe___oe___width 18
#define reg_gio_rw_pc_oe_offset 56

#define reg_gio_rw_pd_dout___data___lsb 0
#define reg_gio_rw_pd_dout___data___width 18
#define reg_gio_rw_pd_dout_offset 64

#define reg_gio_r_pd_din___data___lsb 0
#define reg_gio_r_pd_din___data___width 18
#define reg_gio_r_pd_din_offset 68

#define reg_gio_rw_pd_oe___oe___lsb 0
#define reg_gio_rw_pd_oe___oe___width 18
#define reg_gio_rw_pd_oe_offset 72

#define reg_gio_rw_pe_dout___data___lsb 0
#define reg_gio_rw_pe_dout___data___width 18
#define reg_gio_rw_pe_dout_offset 80

#define reg_gio_r_pe_din___data___lsb 0
#define reg_gio_r_pe_din___data___width 18
#define reg_gio_r_pe_din_offset 84

#define reg_gio_rw_pe_oe___oe___lsb 0
#define reg_gio_rw_pe_oe___oe___width 18
#define reg_gio_rw_pe_oe_offset 88


#define regk_gio_anyedge                          0x00000007
#define regk_gio_hi                               0x00000001
#define regk_gio_lo                               0x00000002
#define regk_gio_negedge                          0x00000006
#define regk_gio_no                               0x00000000
#define regk_gio_off                              0x00000000
#define regk_gio_posedge                          0x00000005
#define regk_gio_rw_intr_cfg_default              0x00000000
#define regk_gio_rw_intr_mask_default             0x00000000
#define regk_gio_rw_pa_oe_default                 0x00000000
#define regk_gio_rw_pb_oe_default                 0x00000000
#define regk_gio_rw_pc_oe_default                 0x00000000
#define regk_gio_rw_pd_oe_default                 0x00000000
#define regk_gio_rw_pe_oe_default                 0x00000000
#define regk_gio_set                              0x00000003
#define regk_gio_yes                              0x00000001
#endif 
