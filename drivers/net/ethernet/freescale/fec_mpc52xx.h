/*
 * drivers/net/ethernet/freescale/fec_mpc52xx.h
 *
 * Driver for the MPC5200 Fast Ethernet Controller
 *
 * Author: Dale Farnsworth <dfarnsworth@mvista.com>
 *
 * 2003-2004 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#ifndef __DRIVERS_NET_MPC52XX_FEC_H__
#define __DRIVERS_NET_MPC52XX_FEC_H__

#include <linux/phy.h>

#define FEC_RX_BUFFER_SIZE	1522	
#define FEC_RX_NUM_BD		256
#define FEC_TX_NUM_BD		64

#define FEC_RESET_DELAY		50 	

#define FEC_WATCHDOG_TIMEOUT	((400*HZ)/1000)


struct mpc52xx_fec {
	u32 fec_id;			
	u32 ievent;			
	u32 imask;			

	u32 reserved0[1];		
	u32 r_des_active;		
	u32 x_des_active;		
	u32 r_des_active_cl;		
	u32 x_des_active_cl;		
	u32 ivent_set;			
	u32 ecntrl;			

	u32 reserved1[6];		
	u32 mii_data;			
	u32 mii_speed;			
	u32 mii_status;			

	u32 reserved2[5];		
	u32 mib_data;			
	u32 mib_control;		

	u32 reserved3[6];		
	u32 r_activate;			
	u32 r_cntrl;			
	u32 r_hash;			
	u32 r_data;			
	u32 ar_done;			
	u32 r_test;			
	u32 r_mib;			
	u32 r_da_low;			
	u32 r_da_high;			

	u32 reserved4[7];		
	u32 x_activate;			
	u32 x_cntrl;			
	u32 backoff;			
	u32 x_data;			
	u32 x_status;			
	u32 x_mib;			
	u32 x_test;			
	u32 fdxfc_da1;			
	u32 fdxfc_da2;			
	u32 paddr1;			
	u32 paddr2;			
	u32 op_pause;			

	u32 reserved5[4];		
	u32 instr_reg;			
	u32 context_reg;		
	u32 test_cntrl;			
	u32 acc_reg;			
	u32 ones;			
	u32 zeros;			
	u32 iaddr1;			
	u32 iaddr2;			
	u32 gaddr1;			
	u32 gaddr2;			
	u32 random;			
	u32 rand1;			
	u32 tmp;			

	u32 reserved6[3];		
	u32 fifo_id;			
	u32 x_wmrk;			
	u32 fcntrl;			
	u32 r_bound;			
	u32 r_fstart;			
	u32 r_count;			
	u32 r_lag;			
	u32 r_read;			
	u32 r_write;			
	u32 x_count;			
	u32 x_lag;			
	u32 x_retry;			
	u32 x_write;			
	u32 x_read;			

	u32 reserved7[2];		
	u32 fm_cntrl;			
	u32 rfifo_data;			
	u32 rfifo_status;		
	u32 rfifo_cntrl;		
	u32 rfifo_lrf_ptr;		
	u32 rfifo_lwf_ptr;		
	u32 rfifo_alarm;		
	u32 rfifo_rdptr;		
	u32 rfifo_wrptr;		
	u32 tfifo_data;			
	u32 tfifo_status;		
	u32 tfifo_cntrl;		
	u32 tfifo_lrf_ptr;		
	u32 tfifo_lwf_ptr;		
	u32 tfifo_alarm;		
	u32 tfifo_rdptr;		
	u32 tfifo_wrptr;		

	u32 reset_cntrl;		
	u32 xmit_fsm;			

	u32 reserved8[3];		
	u32 rdes_data0;			
	u32 rdes_data1;			
	u32 r_length;			
	u32 x_length;			
	u32 x_addr;			
	u32 cdes_data;			
	u32 status;			
	u32 dma_control;		
	u32 des_cmnd;			
	u32 data;			

	u32 rmon_t_drop;		
	u32 rmon_t_packets;		
	u32 rmon_t_bc_pkt;		
	u32 rmon_t_mc_pkt;		
	u32 rmon_t_crc_align;		
	u32 rmon_t_undersize;		
	u32 rmon_t_oversize;		
	u32 rmon_t_frag;		
	u32 rmon_t_jab;			
	u32 rmon_t_col;			
	u32 rmon_t_p64;			
	u32 rmon_t_p65to127;		
	u32 rmon_t_p128to255;		
	u32 rmon_t_p256to511;		
	u32 rmon_t_p512to1023;		
	u32 rmon_t_p1024to2047;		
	u32 rmon_t_p_gte2048;		
	u32 rmon_t_octets;		
	u32 ieee_t_drop;		
	u32 ieee_t_frame_ok;		
	u32 ieee_t_1col;		
	u32 ieee_t_mcol;		
	u32 ieee_t_def;			
	u32 ieee_t_lcol;		
	u32 ieee_t_excol;		
	u32 ieee_t_macerr;		
	u32 ieee_t_cserr;		
	u32 ieee_t_sqe;			
	u32 t_fdxfc;			
	u32 ieee_t_octets_ok;		

	u32 reserved9[2];		
	u32 rmon_r_drop;		
	u32 rmon_r_packets;		
	u32 rmon_r_bc_pkt;		
	u32 rmon_r_mc_pkt;		
	u32 rmon_r_crc_align;		
	u32 rmon_r_undersize;		
	u32 rmon_r_oversize;		
	u32 rmon_r_frag;		
	u32 rmon_r_jab;			

	u32 rmon_r_resvd_0;		

	u32 rmon_r_p64;			
	u32 rmon_r_p65to127;		
	u32 rmon_r_p128to255;		
	u32 rmon_r_p256to511;		
	u32 rmon_r_p512to1023;		
	u32 rmon_r_p1024to2047;		
	u32 rmon_r_p_gte2048;		
	u32 rmon_r_octets;		
	u32 ieee_r_drop;		
	u32 ieee_r_frame_ok;		
	u32 ieee_r_crc;			
	u32 ieee_r_align;		
	u32 r_macerr;			
	u32 r_fdxfc;			
	u32 ieee_r_octets_ok;		

	u32 reserved10[7];		

	u32 reserved11[64];		
};

#define	FEC_MIB_DISABLE			0x80000000

#define	FEC_IEVENT_HBERR		0x80000000
#define	FEC_IEVENT_BABR			0x40000000
#define	FEC_IEVENT_BABT			0x20000000
#define	FEC_IEVENT_GRA			0x10000000
#define	FEC_IEVENT_TFINT		0x08000000
#define	FEC_IEVENT_MII			0x00800000
#define	FEC_IEVENT_LATE_COL		0x00200000
#define	FEC_IEVENT_COL_RETRY_LIM	0x00100000
#define	FEC_IEVENT_XFIFO_UN		0x00080000
#define	FEC_IEVENT_XFIFO_ERROR		0x00040000
#define	FEC_IEVENT_RFIFO_ERROR		0x00020000

#define	FEC_IMASK_HBERR			0x80000000
#define	FEC_IMASK_BABR			0x40000000
#define	FEC_IMASK_BABT			0x20000000
#define	FEC_IMASK_GRA			0x10000000
#define	FEC_IMASK_MII			0x00800000
#define	FEC_IMASK_LATE_COL		0x00200000
#define	FEC_IMASK_COL_RETRY_LIM		0x00100000
#define	FEC_IMASK_XFIFO_UN		0x00080000
#define	FEC_IMASK_XFIFO_ERROR		0x00040000
#define	FEC_IMASK_RFIFO_ERROR		0x00020000

#define FEC_IMASK_ENABLE	(FEC_IMASK_HBERR | FEC_IMASK_BABR | \
		FEC_IMASK_BABT | FEC_IMASK_GRA | FEC_IMASK_LATE_COL | \
		FEC_IMASK_COL_RETRY_LIM | FEC_IMASK_XFIFO_UN | \
		FEC_IMASK_XFIFO_ERROR | FEC_IMASK_RFIFO_ERROR)

#define	FEC_RCNTRL_MAX_FL_SHIFT		16
#define	FEC_RCNTRL_LOOP			0x01
#define	FEC_RCNTRL_DRT			0x02
#define	FEC_RCNTRL_MII_MODE		0x04
#define	FEC_RCNTRL_PROM			0x08
#define	FEC_RCNTRL_BC_REJ		0x10
#define	FEC_RCNTRL_FCE			0x20

#define	FEC_TCNTRL_GTS			0x00000001
#define	FEC_TCNTRL_HBC			0x00000002
#define	FEC_TCNTRL_FDEN			0x00000004
#define	FEC_TCNTRL_TFC_PAUSE		0x00000008
#define	FEC_TCNTRL_RFC_PAUSE		0x00000010

#define	FEC_ECNTRL_RESET		0x00000001
#define	FEC_ECNTRL_ETHER_EN		0x00000002

#define FEC_MII_DATA_ST			0x40000000	
#define FEC_MII_DATA_OP_RD		0x20000000	
#define FEC_MII_DATA_OP_WR		0x10000000	
#define FEC_MII_DATA_PA_MSK		0x0f800000	
#define FEC_MII_DATA_RA_MSK		0x007c0000	
#define FEC_MII_DATA_TA			0x00020000	
#define FEC_MII_DATA_DATAMSK		0x0000ffff	

#define FEC_MII_READ_FRAME	(FEC_MII_DATA_ST | FEC_MII_DATA_OP_RD | FEC_MII_DATA_TA)
#define FEC_MII_WRITE_FRAME	(FEC_MII_DATA_ST | FEC_MII_DATA_OP_WR | FEC_MII_DATA_TA)

#define FEC_MII_DATA_RA_SHIFT		0x12		
#define FEC_MII_DATA_PA_SHIFT		0x17		

#define FEC_PADDR2_TYPE			0x8808

#define FEC_OP_PAUSE_OPCODE		0x00010000

#define FEC_FIFO_WMRK_256B		0x3

#define FEC_FIFO_STATUS_ERR		0x00400000
#define FEC_FIFO_STATUS_UF		0x00200000
#define FEC_FIFO_STATUS_OF		0x00100000

#define FEC_FIFO_CNTRL_FRAME		0x08000000
#define FEC_FIFO_CNTRL_LTG_7		0x07000000

#define FEC_RESET_CNTRL_RESET_FIFO	0x02000000
#define FEC_RESET_CNTRL_ENABLE_IS_RESET	0x01000000

#define FEC_XMIT_FSM_APPEND_CRC		0x02000000
#define FEC_XMIT_FSM_ENABLE_CRC		0x01000000


extern struct platform_driver mpc52xx_fec_mdio_driver;

#endif	
