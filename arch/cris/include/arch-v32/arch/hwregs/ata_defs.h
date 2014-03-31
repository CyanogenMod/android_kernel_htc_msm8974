#ifndef __ata_defs_h
#define __ata_defs_h

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
  unsigned int pio_hold  : 6;
  unsigned int pio_strb  : 6;
  unsigned int pio_setup : 6;
  unsigned int dma_hold  : 6;
  unsigned int dma_strb  : 6;
  unsigned int rst       : 1;
  unsigned int en        : 1;
} reg_ata_rw_ctrl0;
#define REG_RD_ADDR_ata_rw_ctrl0 12
#define REG_WR_ADDR_ata_rw_ctrl0 12

typedef struct {
  unsigned int udma_tcyc : 4;
  unsigned int udma_tdvs : 4;
  unsigned int dummy1    : 24;
} reg_ata_rw_ctrl1;
#define REG_RD_ADDR_ata_rw_ctrl1 16
#define REG_WR_ADDR_ata_rw_ctrl1 16

typedef struct {
  unsigned int data     : 16;
  unsigned int dummy1   : 3;
  unsigned int dma_size : 1;
  unsigned int multi    : 1;
  unsigned int hsh      : 2;
  unsigned int trf_mode : 1;
  unsigned int rw       : 1;
  unsigned int addr     : 3;
  unsigned int cs0      : 1;
  unsigned int cs1      : 1;
  unsigned int sel      : 2;
} reg_ata_rw_ctrl2;
#define REG_RD_ADDR_ata_rw_ctrl2 0
#define REG_WR_ADDR_ata_rw_ctrl2 0

typedef struct {
  unsigned int data : 16;
  unsigned int dav  : 1;
  unsigned int busy : 1;
  unsigned int dummy1 : 14;
} reg_ata_rs_stat_data;
#define REG_RD_ADDR_ata_rs_stat_data 4

typedef struct {
  unsigned int data : 16;
  unsigned int dav  : 1;
  unsigned int busy : 1;
  unsigned int dummy1 : 14;
} reg_ata_r_stat_data;
#define REG_RD_ADDR_ata_r_stat_data 8

typedef struct {
  unsigned int cnt : 17;
  unsigned int dummy1 : 15;
} reg_ata_rw_trf_cnt;
#define REG_RD_ADDR_ata_rw_trf_cnt 20
#define REG_WR_ADDR_ata_rw_trf_cnt 20

typedef struct {
  unsigned int crc : 16;
  unsigned int dummy1 : 16;
} reg_ata_r_stat_misc;
#define REG_RD_ADDR_ata_r_stat_misc 24

typedef struct {
  unsigned int bus0 : 1;
  unsigned int bus1 : 1;
  unsigned int bus2 : 1;
  unsigned int bus3 : 1;
  unsigned int dummy1 : 28;
} reg_ata_rw_intr_mask;
#define REG_RD_ADDR_ata_rw_intr_mask 28
#define REG_WR_ADDR_ata_rw_intr_mask 28

typedef struct {
  unsigned int bus0 : 1;
  unsigned int bus1 : 1;
  unsigned int bus2 : 1;
  unsigned int bus3 : 1;
  unsigned int dummy1 : 28;
} reg_ata_rw_ack_intr;
#define REG_RD_ADDR_ata_rw_ack_intr 32
#define REG_WR_ADDR_ata_rw_ack_intr 32

typedef struct {
  unsigned int bus0 : 1;
  unsigned int bus1 : 1;
  unsigned int bus2 : 1;
  unsigned int bus3 : 1;
  unsigned int dummy1 : 28;
} reg_ata_r_intr;
#define REG_RD_ADDR_ata_r_intr 36

typedef struct {
  unsigned int bus0 : 1;
  unsigned int bus1 : 1;
  unsigned int bus2 : 1;
  unsigned int bus3 : 1;
  unsigned int dummy1 : 28;
} reg_ata_r_masked_intr;
#define REG_RD_ADDR_ata_r_masked_intr 40


enum {
  regk_ata_active                          = 0x00000001,
  regk_ata_byte                            = 0x00000001,
  regk_ata_data                            = 0x00000001,
  regk_ata_dma                             = 0x00000001,
  regk_ata_inactive                        = 0x00000000,
  regk_ata_no                              = 0x00000000,
  regk_ata_nodata                          = 0x00000000,
  regk_ata_pio                             = 0x00000000,
  regk_ata_rd                              = 0x00000001,
  regk_ata_reg                             = 0x00000000,
  regk_ata_rw_ctrl0_default                = 0x00000000,
  regk_ata_rw_ctrl2_default                = 0x00000000,
  regk_ata_rw_intr_mask_default            = 0x00000000,
  regk_ata_udma                            = 0x00000002,
  regk_ata_word                            = 0x00000000,
  regk_ata_wr                              = 0x00000000,
  regk_ata_yes                             = 0x00000001
};
#endif 
