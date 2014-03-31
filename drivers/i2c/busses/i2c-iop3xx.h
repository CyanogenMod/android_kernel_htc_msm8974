/*   Copyright (C) 2003 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */


#ifndef I2C_IOP3XX_H
#define I2C_IOP3XX_H 1

#define IOP3XX_ICR_FAST_MODE	0x8000	
#define IOP3XX_ICR_UNIT_RESET	0x4000	
#define IOP3XX_ICR_SAD_IE	0x2000	
#define IOP3XX_ICR_ALD_IE	0x1000	
#define IOP3XX_ICR_SSD_IE	0x0800	
#define IOP3XX_ICR_BERR_IE	0x0400	
#define IOP3XX_ICR_RXFULL_IE	0x0200	
#define IOP3XX_ICR_TXEMPTY_IE	0x0100	
#define IOP3XX_ICR_GCD		0x0080	
#define IOP3XX_ICR_UE		0x0040	
#define IOP3XX_ICR_SCLEN	0x0020	
#define IOP3XX_ICR_MABORT	0x0010	
#define IOP3XX_ICR_TBYTE	0x0008	
#define IOP3XX_ICR_NACK		0x0004	
#define IOP3XX_ICR_MSTOP	0x0002	
#define IOP3XX_ICR_MSTART	0x0001	


#define IOP3XX_ISR_BERRD	0x0400	
#define IOP3XX_ISR_SAD		0x0200	
#define IOP3XX_ISR_GCAD		0x0100	
#define IOP3XX_ISR_RXFULL	0x0080	
#define IOP3XX_ISR_TXEMPTY	0x0040	
#define IOP3XX_ISR_ALD		0x0020	
#define IOP3XX_ISR_SSD		0x0010	
#define IOP3XX_ISR_BBUSY	0x0008	
#define IOP3XX_ISR_UNITBUSY	0x0004	
#define IOP3XX_ISR_NACK		0x0002	
#define IOP3XX_ISR_RXREAD	0x0001	

#define IOP3XX_ISR_CLEARBITS	0x07f0

#define IOP3XX_ISAR_SAMASK	0x007f

#define IOP3XX_IDBR_MASK	0x00ff

#define IOP3XX_IBMR_SCL		0x0002
#define IOP3XX_IBMR_SDA		0x0001

#define IOP3XX_GPOD_I2C0	0x00c0	
#define IOP3XX_GPOD_I2C1	0x0030	

#define MYSAR			0	

#define I2C_ERR			321
#define I2C_ERR_BERR		(I2C_ERR+0)
#define I2C_ERR_ALD		(I2C_ERR+1)


#define	CR_OFFSET		0
#define	SR_OFFSET		0x4
#define	SAR_OFFSET		0x8
#define	DBR_OFFSET		0xc
#define	CCR_OFFSET		0x10
#define	BMR_OFFSET		0x14

#define	IOP3XX_I2C_IO_SIZE	0x18

struct i2c_algo_iop3xx_data {
	void __iomem *ioaddr;
	wait_queue_head_t waitq;
	spinlock_t lock;
	u32 SR_enabled, SR_received;
	int id;
};

#endif 
