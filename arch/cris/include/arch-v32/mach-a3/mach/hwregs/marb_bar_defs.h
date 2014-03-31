#ifndef __marb_bar_defs_h
#define __marb_bar_defs_h

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


#define STRIDE_marb_bar_rw_ddr2_slots 4
typedef struct {
  unsigned int owner : 4;
  unsigned int dummy1 : 28;
} reg_marb_bar_rw_ddr2_slots;
#define REG_RD_ADDR_marb_bar_rw_ddr2_slots 0
#define REG_WR_ADDR_marb_bar_rw_ddr2_slots 0

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_h264_rd_burst;
#define REG_RD_ADDR_marb_bar_rw_h264_rd_burst 256
#define REG_WR_ADDR_marb_bar_rw_h264_rd_burst 256

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_h264_wr_burst;
#define REG_RD_ADDR_marb_bar_rw_h264_wr_burst 260
#define REG_WR_ADDR_marb_bar_rw_h264_wr_burst 260

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_ccd_burst;
#define REG_RD_ADDR_marb_bar_rw_ccd_burst 264
#define REG_WR_ADDR_marb_bar_rw_ccd_burst 264

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_vin_wr_burst;
#define REG_RD_ADDR_marb_bar_rw_vin_wr_burst 268
#define REG_WR_ADDR_marb_bar_rw_vin_wr_burst 268

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_vin_rd_burst;
#define REG_RD_ADDR_marb_bar_rw_vin_rd_burst 272
#define REG_WR_ADDR_marb_bar_rw_vin_rd_burst 272

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_sclr_rd_burst;
#define REG_RD_ADDR_marb_bar_rw_sclr_rd_burst 276
#define REG_WR_ADDR_marb_bar_rw_sclr_rd_burst 276

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_vout_burst;
#define REG_RD_ADDR_marb_bar_rw_vout_burst 280
#define REG_WR_ADDR_marb_bar_rw_vout_burst 280

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_sclr_fifo_burst;
#define REG_RD_ADDR_marb_bar_rw_sclr_fifo_burst 284
#define REG_WR_ADDR_marb_bar_rw_sclr_fifo_burst 284

typedef struct {
  unsigned int ddr2_bsize : 2;
  unsigned int dummy1     : 30;
} reg_marb_bar_rw_l2cache_burst;
#define REG_RD_ADDR_marb_bar_rw_l2cache_burst 288
#define REG_WR_ADDR_marb_bar_rw_l2cache_burst 288

typedef struct {
  unsigned int bp0 : 1;
  unsigned int bp1 : 1;
  unsigned int bp2 : 1;
  unsigned int bp3 : 1;
  unsigned int dummy1 : 28;
} reg_marb_bar_rw_intr_mask;
#define REG_RD_ADDR_marb_bar_rw_intr_mask 292
#define REG_WR_ADDR_marb_bar_rw_intr_mask 292

typedef struct {
  unsigned int bp0 : 1;
  unsigned int bp1 : 1;
  unsigned int bp2 : 1;
  unsigned int bp3 : 1;
  unsigned int dummy1 : 28;
} reg_marb_bar_rw_ack_intr;
#define REG_RD_ADDR_marb_bar_rw_ack_intr 296
#define REG_WR_ADDR_marb_bar_rw_ack_intr 296

typedef struct {
  unsigned int bp0 : 1;
  unsigned int bp1 : 1;
  unsigned int bp2 : 1;
  unsigned int bp3 : 1;
  unsigned int dummy1 : 28;
} reg_marb_bar_r_intr;
#define REG_RD_ADDR_marb_bar_r_intr 300

typedef struct {
  unsigned int bp0 : 1;
  unsigned int bp1 : 1;
  unsigned int bp2 : 1;
  unsigned int bp3 : 1;
  unsigned int dummy1 : 28;
} reg_marb_bar_r_masked_intr;
#define REG_RD_ADDR_marb_bar_r_masked_intr 304

typedef struct {
  unsigned int h264_rd   : 1;
  unsigned int h264_wr   : 1;
  unsigned int ccd       : 1;
  unsigned int vin_wr    : 1;
  unsigned int vin_rd    : 1;
  unsigned int sclr_rd   : 1;
  unsigned int vout      : 1;
  unsigned int sclr_fifo : 1;
  unsigned int l2cache   : 1;
  unsigned int dummy1    : 23;
} reg_marb_bar_rw_stop_mask;
#define REG_RD_ADDR_marb_bar_rw_stop_mask 308
#define REG_WR_ADDR_marb_bar_rw_stop_mask 308

typedef struct {
  unsigned int h264_rd   : 1;
  unsigned int h264_wr   : 1;
  unsigned int ccd       : 1;
  unsigned int vin_wr    : 1;
  unsigned int vin_rd    : 1;
  unsigned int sclr_rd   : 1;
  unsigned int vout      : 1;
  unsigned int sclr_fifo : 1;
  unsigned int l2cache   : 1;
  unsigned int dummy1    : 23;
} reg_marb_bar_r_stopped;
#define REG_RD_ADDR_marb_bar_r_stopped 312

typedef struct {
  unsigned int h264_rd   : 1;
  unsigned int h264_wr   : 1;
  unsigned int ccd       : 1;
  unsigned int vin_wr    : 1;
  unsigned int vin_rd    : 1;
  unsigned int sclr_rd   : 1;
  unsigned int vout      : 1;
  unsigned int sclr_fifo : 1;
  unsigned int l2cache   : 1;
  unsigned int dummy1    : 23;
} reg_marb_bar_rw_no_snoop;
#define REG_RD_ADDR_marb_bar_rw_no_snoop 576
#define REG_WR_ADDR_marb_bar_rw_no_snoop 576


enum {
  regk_marb_bar_ccd                        = 0x00000002,
  regk_marb_bar_h264_rd                    = 0x00000000,
  regk_marb_bar_h264_wr                    = 0x00000001,
  regk_marb_bar_l2cache                    = 0x00000008,
  regk_marb_bar_no                         = 0x00000000,
  regk_marb_bar_r_stopped_default          = 0x00000000,
  regk_marb_bar_rw_ccd_burst_default       = 0x00000000,
  regk_marb_bar_rw_ddr2_slots_default      = 0x00000000,
  regk_marb_bar_rw_ddr2_slots_size         = 0x00000040,
  regk_marb_bar_rw_h264_rd_burst_default   = 0x00000000,
  regk_marb_bar_rw_h264_wr_burst_default   = 0x00000000,
  regk_marb_bar_rw_intr_mask_default       = 0x00000000,
  regk_marb_bar_rw_l2cache_burst_default   = 0x00000000,
  regk_marb_bar_rw_no_snoop_default        = 0x00000000,
  regk_marb_bar_rw_sclr_fifo_burst_default = 0x00000000,
  regk_marb_bar_rw_sclr_rd_burst_default   = 0x00000000,
  regk_marb_bar_rw_stop_mask_default       = 0x00000000,
  regk_marb_bar_rw_vin_rd_burst_default    = 0x00000000,
  regk_marb_bar_rw_vin_wr_burst_default    = 0x00000000,
  regk_marb_bar_rw_vout_burst_default      = 0x00000000,
  regk_marb_bar_sclr_fifo                  = 0x00000007,
  regk_marb_bar_sclr_rd                    = 0x00000005,
  regk_marb_bar_vin_rd                     = 0x00000004,
  regk_marb_bar_vin_wr                     = 0x00000003,
  regk_marb_bar_vout                       = 0x00000006,
  regk_marb_bar_yes                        = 0x00000001
};
#endif 
#ifndef __marb_bar_bp_defs_h
#define __marb_bar_bp_defs_h

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


typedef unsigned int reg_marb_bar_bp_rw_first_addr;
#define REG_RD_ADDR_marb_bar_bp_rw_first_addr 0
#define REG_WR_ADDR_marb_bar_bp_rw_first_addr 0

typedef unsigned int reg_marb_bar_bp_rw_last_addr;
#define REG_RD_ADDR_marb_bar_bp_rw_last_addr 4
#define REG_WR_ADDR_marb_bar_bp_rw_last_addr 4

typedef struct {
  unsigned int rd         : 1;
  unsigned int wr         : 1;
  unsigned int rd_excl    : 1;
  unsigned int pri_wr     : 1;
  unsigned int us_rd      : 1;
  unsigned int us_wr      : 1;
  unsigned int us_rd_excl : 1;
  unsigned int us_pri_wr  : 1;
  unsigned int dummy1     : 24;
} reg_marb_bar_bp_rw_op;
#define REG_RD_ADDR_marb_bar_bp_rw_op 8
#define REG_WR_ADDR_marb_bar_bp_rw_op 8

typedef struct {
  unsigned int h264_rd   : 1;
  unsigned int h264_wr   : 1;
  unsigned int ccd       : 1;
  unsigned int vin_wr    : 1;
  unsigned int vin_rd    : 1;
  unsigned int sclr_rd   : 1;
  unsigned int vout      : 1;
  unsigned int sclr_fifo : 1;
  unsigned int l2cache   : 1;
  unsigned int dummy1    : 23;
} reg_marb_bar_bp_rw_clients;
#define REG_RD_ADDR_marb_bar_bp_rw_clients 12
#define REG_WR_ADDR_marb_bar_bp_rw_clients 12

typedef struct {
  unsigned int wrap : 1;
  unsigned int dummy1 : 31;
} reg_marb_bar_bp_rw_options;
#define REG_RD_ADDR_marb_bar_bp_rw_options 16
#define REG_WR_ADDR_marb_bar_bp_rw_options 16

typedef unsigned int reg_marb_bar_bp_r_brk_addr;
#define REG_RD_ADDR_marb_bar_bp_r_brk_addr 20

typedef struct {
  unsigned int rd         : 1;
  unsigned int wr         : 1;
  unsigned int rd_excl    : 1;
  unsigned int pri_wr     : 1;
  unsigned int us_rd      : 1;
  unsigned int us_wr      : 1;
  unsigned int us_rd_excl : 1;
  unsigned int us_pri_wr  : 1;
  unsigned int dummy1     : 24;
} reg_marb_bar_bp_r_brk_op;
#define REG_RD_ADDR_marb_bar_bp_r_brk_op 24

typedef struct {
  unsigned int h264_rd   : 1;
  unsigned int h264_wr   : 1;
  unsigned int ccd       : 1;
  unsigned int vin_wr    : 1;
  unsigned int vin_rd    : 1;
  unsigned int sclr_rd   : 1;
  unsigned int vout      : 1;
  unsigned int sclr_fifo : 1;
  unsigned int l2cache   : 1;
  unsigned int dummy1    : 23;
} reg_marb_bar_bp_r_brk_clients;
#define REG_RD_ADDR_marb_bar_bp_r_brk_clients 28

typedef struct {
  unsigned int h264_rd   : 1;
  unsigned int h264_wr   : 1;
  unsigned int ccd       : 1;
  unsigned int vin_wr    : 1;
  unsigned int vin_rd    : 1;
  unsigned int sclr_rd   : 1;
  unsigned int vout      : 1;
  unsigned int sclr_fifo : 1;
  unsigned int l2cache   : 1;
  unsigned int dummy1    : 23;
} reg_marb_bar_bp_r_brk_first_client;
#define REG_RD_ADDR_marb_bar_bp_r_brk_first_client 32

typedef unsigned int reg_marb_bar_bp_r_brk_size;
#define REG_RD_ADDR_marb_bar_bp_r_brk_size 36

typedef unsigned int reg_marb_bar_bp_rw_ack;
#define REG_RD_ADDR_marb_bar_bp_rw_ack 40
#define REG_WR_ADDR_marb_bar_bp_rw_ack 40


enum {
  regk_marb_bar_bp_no                      = 0x00000000,
  regk_marb_bar_bp_rw_op_default           = 0x00000000,
  regk_marb_bar_bp_rw_options_default      = 0x00000000,
  regk_marb_bar_bp_yes                     = 0x00000001
};
#endif 
