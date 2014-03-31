/*
 * Core driver for the CC770 and AN82527 CAN controllers
 *
 * Copyright (C) 2009, 2011 Wolfgang Grandegger <wg@grandegger.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the version 2 of the GNU General Public License
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef CC770_DEV_H
#define CC770_DEV_H

#include <linux/can/dev.h>

struct cc770_msgobj {
	u8 ctrl0;
	u8 ctrl1;
	u8 id[4];
	u8 config;
	u8 data[8];
	u8 dontuse;		
} __packed;

struct cc770_regs {
	union {
		struct cc770_msgobj msgobj[16]; 
		struct {
			u8 control;		
			u8 status;		
			u8 cpu_interface;	
			u8 dontuse1;
			u8 high_speed_read[2];	
			u8 global_mask_std[2];	
			u8 global_mask_ext[4];	
			u8 msg15_mask[4];	
			u8 dontuse2[15];
			u8 clkout;		
			u8 dontuse3[15];
			u8 bus_config;		
			u8 dontuse4[15];
			u8 bit_timing_0;	
			u8 dontuse5[15];
			u8 bit_timing_1;	
			u8 dontuse6[15];
			u8 interrupt;		
			u8 dontuse7[15];
			u8 rx_error_counter;	
			u8 dontuse8[15];
			u8 tx_error_counter;	
			u8 dontuse9[31];
			u8 p1_conf;
			u8 dontuse10[15];
			u8 p2_conf;
			u8 dontuse11[15];
			u8 p1_in;
			u8 dontuse12[15];
			u8 p2_in;
			u8 dontuse13[15];
			u8 p1_out;
			u8 dontuse14[15];
			u8 p2_out;
			u8 dontuse15[15];
			u8 serial_reset_addr;
		};
	};
} __packed;

#define CTRL_INI	0x01	
#define CTRL_IE		0x02	
#define CTRL_SIE	0x04	
#define CTRL_EIE	0x08	
#define CTRL_EAF	0x20	
#define CTRL_CCE	0x40	

#define STAT_LEC_STUFF	0x01	
#define STAT_LEC_FORM	0x02	
#define STAT_LEC_ACK	0x03	
#define STAT_LEC_BIT1	0x04	
#define STAT_LEC_BIT0	0x05	
#define STAT_LEC_CRC	0x06	
#define STAT_LEC_MASK	0x07	
#define STAT_TXOK	0x08	
#define STAT_RXOK	0x10	
#define STAT_WAKE	0x20	
#define STAT_WARN	0x40	
#define STAT_BOFF	0x80	


#define INTPND_RES	0x01	
#define INTPND_SET	0x02	
#define INTPND_UNC	0x03
#define RXIE_RES	0x04	
#define RXIE_SET	0x08	
#define RXIE_UNC	0x0c
#define TXIE_RES	0x10	
#define TXIE_SET	0x20	
#define TXIE_UNC	0x30
#define MSGVAL_RES	0x40	
#define MSGVAL_SET	0x80	
#define MSGVAL_UNC	0xc0

#define NEWDAT_RES	0x01	
#define NEWDAT_SET	0x02	
#define NEWDAT_UNC	0x03
#define MSGLST_RES	0x04	
#define MSGLST_SET	0x08	
#define MSGLST_UNC	0x0c
#define CPUUPD_RES	0x04	
#define CPUUPD_SET	0x08	
#define CPUUPD_UNC	0x0c
#define TXRQST_RES	0x10	
#define TXRQST_SET	0x20	
#define TXRQST_UNC	0x30
#define RMTPND_RES	0x40	
#define RMTPND_SET	0x80	
#define RMTPND_UNC	0xc0

#define MSGCFG_XTD	0x04	
#define MSGCFG_DIR	0x08	

#define MSGOBJ_FIRST	1
#define MSGOBJ_LAST	15

#define CC770_IO_SIZE	0x100
#define CC770_MAX_IRQ	20	
#define CC770_MAX_MSG	4	

#define CC770_ECHO_SKB_MAX	1

#define cc770_read_reg(priv, member)					\
	priv->read_reg(priv, offsetof(struct cc770_regs, member))

#define cc770_write_reg(priv, member, value)				\
	priv->write_reg(priv, offsetof(struct cc770_regs, member), value)

#define CC770_OBJ_FLAG_RX	0x01
#define CC770_OBJ_FLAG_RTR	0x02
#define CC770_OBJ_FLAG_EFF	0x04

enum {
	CC770_OBJ_RX0 = 0,	
	CC770_OBJ_RX1,		
	CC770_OBJ_RX_RTR0,	
	CC770_OBJ_RX_RTR1,	
	CC770_OBJ_TX,		
	CC770_OBJ_MAX
};

#define obj2msgobj(o)	(MSGOBJ_LAST - (o)) 

struct cc770_priv {
	struct can_priv can;	
	struct sk_buff *echo_skb;

	
	u8 (*read_reg)(const struct cc770_priv *priv, int reg);
	void (*write_reg)(const struct cc770_priv *priv, int reg, u8 val);
	void (*pre_irq)(const struct cc770_priv *priv);
	void (*post_irq)(const struct cc770_priv *priv);

	void *priv;		
	struct net_device *dev;

	void __iomem *reg_base;	 
	unsigned long irq_flags; 

	unsigned char obj_flags[CC770_OBJ_MAX];
	u8 control_normal_mode;	
	u8 cpu_interface;	
	u8 clkout;		
	u8 bus_config;		
};

struct net_device *alloc_cc770dev(int sizeof_priv);
void free_cc770dev(struct net_device *dev);
int register_cc770dev(struct net_device *dev);
void unregister_cc770dev(struct net_device *dev);

#endif 
