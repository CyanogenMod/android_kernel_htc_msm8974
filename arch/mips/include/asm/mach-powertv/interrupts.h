/*
 * Copyright (C) 2009  Cisco Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef	_ASM_MACH_POWERTV_INTERRUPTS_H_
#define _ASM_MACH_POWERTV_INTERRUPTS_H_


#define kIrq_Uart1		irq_uart1

#define ibase 0

#define irq_asc2video		(ibase+126)	
#define irq_asc1video		(ibase+125)	
#define irq_comms_block_wd	(ibase+124)	
#define irq_fdma_mailbox	(ibase+123)	
#define irq_fdma_gp		(ibase+122)	
#define irq_mips_pic		(ibase+121)	
#define irq_mips_timer		(ibase+120)	
#define irq_memory_protect	(ibase+119)	
#define irq_sbag		(ibase+117)	
#define irq_qam_b_fec		(ibase+116)	
#define irq_qam_a_fec		(ibase+115)	
#define irq_mailbox		(ibase+113)	
#define irq_fuse_stat1		(ibase+112)	
#define irq_fuse_stat2		(ibase+111)	
#define irq_fuse_stat3		(ibase+110)	
#define irq_blitter		(ibase+110)	
#define irq_avc1_pp0		(ibase+109)	
#define irq_avc1_pp1		(ibase+108)	
#define irq_avc1_mbe		(ibase+107)	
#define irq_avc2_pp0		(ibase+106)	
#define irq_avc2_pp1		(ibase+105)	
#define irq_avc2_mbe		(ibase+104)	
#define irq_zbug_spi		(ibase+103)	
#define irq_qam_mod2		(ibase+102)	
#define irq_ir_rx		(ibase+101)	
#define irq_aud_dsp2		(ibase+100)	
#define irq_aud_dsp1		(ibase+99)	
#define irq_docsis		(ibase+98)	
#define irq_sd_dvp1		(ibase+97)	
#define irq_sd_dvp2		(ibase+96)	
#define irq_hd_dvp		(ibase+95)	
#define kIrq_Prewatchdog	(ibase+94)	
#define irq_timer2		(ibase+93)	
#define irq_1394		(ibase+92)	
#define irq_usbohci		(ibase+91)	
#define irq_usbehci		(ibase+90)	
#define irq_pciexp		(ibase+89)	
#define irq_pciexp0		(ibase+89)	
#define irq_afe1		(ibase+88)	
#define irq_sata		(ibase+87)	
#define irq_sata1		(ibase+87)	
#define irq_dtcp		(ibase+86)	
#define irq_pciexp1		(ibase+85)	
#define irq_sata2		(ibase+81)	
#define irq_uart2		(ibase+80)	
#define irq_legacy_usb		(ibase+79)	
#define irq_pod			(ibase+78)	
#define irq_slave_usb		(ibase+77)	
#define irq_denc1		(ibase+76)	
#define irq_vbi_vtg		(ibase+75)	
#define irq_afe2		(ibase+74)	
#define irq_denc2		(ibase+73)	
#define irq_asc2		(ibase+72)	
#define irq_asc1		(ibase+71)	
#define irq_mod_dma		(ibase+70)	
#define irq_byte_eng1		(ibase+69)	
#define irq_byte_eng0		(ibase+68)	
#define irq_buf_dma_mem2mem	(ibase+55)	
#define irq_buf_dma_usbtransmit	(ibase+54)	
#define irq_buf_dma_qpskpodtransmit (ibase+53)	
#define irq_buf_dma_transmit_error (ibase+52)	
#define irq_buf_dma_usbrecv	(ibase+51)	
#define irq_buf_dma_qpskpodrecv	(ibase+50)	
#define irq_buf_dma_recv_error	(ibase+49)	
#define irq_qamdma_transmit_play (ibase+48)	
#define irq_qamdma_transmit_error (ibase+47)	
#define irq_qamdma_recv2high	(ibase+46)	
#define irq_qamdma_recv2low	(ibase+45)	
#define irq_qamdma_recv1high	(ibase+44)	
#define irq_qamdma_recv1low	(ibase+43)	
#define irq_qamdma_recv_error	(ibase+42)	
#define irq_mpegsplice		(ibase+41)	
#define irq_deinterlace_rdy	(ibase+40)	
#define irq_ext_in0		(ibase+39)	
#define irq_gpio3		(ibase+38)	
#define irq_gpio2		(ibase+37)	
#define irq_pcrcmplt1		(ibase+36)	
#define irq_pcrcmplt2		(ibase+35)	
#define irq_parse_peierr	(ibase+34)	
#define irq_parse_cont_err	(ibase+33)	
#define irq_ds1framer		(ibase+32)	
#define irq_gpio1		(ibase+31)	
#define irq_gpio0		(ibase+30)	
#define irq_qpsk_out_aloha	(ibase+29)	
#define irq_qpsk_out_tdma	(ibase+28)	
#define irq_qpsk_out_reserve	(ibase+27)	
#define irq_qpsk_out_aloha_err	(ibase+26)	
#define irq_qpsk_out_tdma_err	(ibase+25)	
#define irq_qpsk_out_rsrv_err	(ibase+24)	
#define irq_aloha_fail		(ibase+23)	
#define irq_timer1		(ibase+22)	
#define irq_keyboard		(ibase+21)	
#define irq_i2c			(ibase+20)	
#define irq_spi			(ibase+19)	
#define irq_irblaster		(ibase+18)	
#define irq_splice_detect	(ibase+17)	
#define irq_se_micro		(ibase+16)	
#define irq_uart1		(ibase+15)	
#define irq_irrecv		(ibase+14)	
#define irq_host_int1		(ibase+13)	
#define irq_host_int0		(ibase+12)	
#define irq_qpsk_hecerr		(ibase+11)	
#define irq_qpsk_crcerr		(ibase+10)	
#define irq_psicrcerr		(ibase+7) 	
#define irq_psilength_err	(ibase+6) 	
#define irq_esfforward		(ibase+5) 	
#define irq_esfreverse		(ibase+4) 	
#define irq_aloha_timeout	(ibase+3) 	
#define irq_reservation		(ibase+2) 	
#define irq_aloha3		(ibase+1) 	
#define irq_mpeg_d		(ibase+0) 	
#endif	
