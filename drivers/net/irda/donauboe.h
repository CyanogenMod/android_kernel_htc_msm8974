/*********************************************************************
 *                
 * Filename:      toshoboe.h
 * Version:       2.16
 * Description:   Driver for the Toshiba OBOE (or type-O or 701)
 *                FIR Chipset, also supports the DONAUOBOE (type-DO
 *                or d01) FIR chipset which as far as I know is
 *                register compatible.
 * Status:        Experimental.
 * Author:        James McKenzie <james@fishsoup.dhs.org>
 * Created at:    Sat May 8  12:35:27 1999
 * Modified: 2.16 Martin Lucina <mato@kotelna.sk>
 * Modified: 2.16 Sat Jun 22 18:54:29 2002 (sync headers)
 * Modified: 2.17 Christian Gennerat <christian.gennerat@polytechnique.org>
 * Modified: 2.17 jeu sep 12 08:50:20 2002 (add lock to be used by spinlocks)
 * 
 *     Copyright (c) 1999 James McKenzie, All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither James McKenzie nor Cambridge University admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 * 
 *     Applicable Models : Libretto 100/110CT and many more.
 *     Toshiba refers to this chip as the type-O IR port,
 *     or the type-DO IR port.
 *
 * IrDA chip set list from Toshiba Computer Engineering Corp.
 * model			method	maker	controller		Version 
 * Portege 320CT	FIR,SIR Toshiba Oboe(Triangle) 
 * Portege 3010CT	FIR,SIR Toshiba Oboe(Sydney) 
 * Portege 3015CT	FIR,SIR Toshiba Oboe(Sydney) 
 * Portege 3020CT	FIR,SIR Toshiba Oboe(Sydney) 
 * Portege 7020CT	FIR,SIR ?		?
 * 
 * Satell. 4090XCDT	FIR,SIR ?		?
 * 
 * Libretto 100CT	FIR,SIR Toshiba Oboe 
 * Libretto 1000CT	FIR,SIR Toshiba Oboe 
 * 
 * TECRA750DVD		FIR,SIR Toshiba Oboe(Triangle)	REV ID=14h 
 * TECRA780			FIR,SIR Toshiba Oboe(Sandlot)	REV ID=32h,33h 
 * TECRA750CDT		FIR,SIR Toshiba Oboe(Triangle)	REV ID=13h,14h 
 * TECRA8000		FIR,SIR Toshiba Oboe(ISKUR)		REV ID=23h 
 * 
 ********************************************************************/







#ifndef TOSHOBOE_H
#define TOSHOBOE_H


#define OBOE_IO_EXTENT	0x1f

#define OBOE_REG(i)	(i+(self->base))
#define OBOE_RXSLOT	OBOE_REG(0x0)
#define OBOE_TXSLOT	OBOE_REG(0x1)
#define OBOE_SLOT_MASK	0x3f

#define OBOE_TXRING_OFFSET		0x200
#define OBOE_TXRING_OFFSET_IN_SLOTS	0x40

#define OBOE_RING_BASE0	OBOE_REG(0x4)
#define OBOE_RING_BASE1	OBOE_REG(0x5)
#define OBOE_RING_BASE2	OBOE_REG(0x2)
#define OBOE_RING_BASE3	OBOE_REG(0x3)

#define OBOE_RING_SIZE  OBOE_REG(0x7)
#define OBOE_RING_SIZE_RX4	0x00
#define OBOE_RING_SIZE_RX8	0x01
#define OBOE_RING_SIZE_RX16	0x03
#define OBOE_RING_SIZE_RX32	0x07
#define OBOE_RING_SIZE_RX64	0x0f
#define OBOE_RING_SIZE_TX4	0x00
#define OBOE_RING_SIZE_TX8	0x10
#define OBOE_RING_SIZE_TX16	0x30
#define OBOE_RING_SIZE_TX32	0x70
#define OBOE_RING_SIZE_TX64	0xf0

#define OBOE_RING_MAX_SIZE	64

#define OBOE_PROMPT	OBOE_REG(0x9)
#define OBOE_PROMPT_BIT		0x1

#define OBOE_ISR	OBOE_REG(0xc)
#define OBOE_IER	OBOE_REG(0xd)
#define OBOE_INT_TXDONE		0x80
#define OBOE_INT_RXDONE		0x40
#define OBOE_INT_TXUNDER	0x20
#define OBOE_INT_RXOVER		0x10
#define OBOE_INT_SIP		0x08
#define OBOE_INT_MASK		0xf8

#define OBOE_CONFIG1	OBOE_REG(0xe)
#define OBOE_CONFIG1_RST	0x01
#define OBOE_CONFIG1_DISABLE	0x02
#define OBOE_CONFIG1_4		0x08
#define OBOE_CONFIG1_8		0x08

#define OBOE_CONFIG1_ON		0x8
#define OBOE_CONFIG1_RESET	0xf
#define OBOE_CONFIG1_OFF	0xe

#define OBOE_STATUS	OBOE_REG(0xf)
#define OBOE_STATUS_RXBUSY	0x10
#define OBOE_STATUS_FIRRX	0x04
#define OBOE_STATUS_MIRRX	0x02
#define OBOE_STATUS_SIRRX	0x01


#define OBOE_CONFIG0L	OBOE_REG(0x10)
#define OBOE_CONFIG0H	OBOE_REG(0x11)

#define OBOE_CONFIG0H_TXONLOOP  0x80 
#define OBOE_CONFIG0H_LOOP	0x40 
#define OBOE_CONFIG0H_ENTX	0x10 
#define OBOE_CONFIG0H_ENRX	0x08 
#define OBOE_CONFIG0H_ENDMAC	0x04 
#define OBOE_CONFIG0H_RCVANY	0x02 

#define OBOE_CONFIG0L_CRC16	0x80 
#define OBOE_CONFIG0L_ENFIR	0x40 
#define OBOE_CONFIG0L_ENMIR	0x20 
#define OBOE_CONFIG0L_ENSIR	0x10 
#define OBOE_CONFIG0L_ENSIRF	0x08 
#define OBOE_CONFIG0L_SIRTEST	0x04 
#define OBOE_CONFIG0L_INVERTTX  0x02 
#define OBOE_CONFIG0L_INVERTRX  0x01 

#define OBOE_BOF	OBOE_REG(0x12)
#define OBOE_EOF	OBOE_REG(0x13)

#define OBOE_ENABLEL	OBOE_REG(0x14)
#define OBOE_ENABLEH	OBOE_REG(0x15)

#define OBOE_ENABLEH_PHYANDCLOCK	0x80 
#define OBOE_ENABLEH_CONFIGERR		0x40
#define OBOE_ENABLEH_FIRON		0x20
#define OBOE_ENABLEH_MIRON		0x10
#define OBOE_ENABLEH_SIRON		0x08
#define OBOE_ENABLEH_ENTX		0x04
#define OBOE_ENABLEH_ENRX		0x02
#define OBOE_ENABLEH_CRC16		0x01

#define OBOE_ENABLEL_BROADCAST		0x01

#define OBOE_CURR_PCONFIGL		OBOE_REG(0x16) 
#define OBOE_CURR_PCONFIGH		OBOE_REG(0x17)

#define OBOE_NEW_PCONFIGL		OBOE_REG(0x18)
#define OBOE_NEW_PCONFIGH		OBOE_REG(0x19)

#define OBOE_PCONFIGH_BAUDMASK		0xfc
#define OBOE_PCONFIGH_WIDTHMASK		0x04
#define OBOE_PCONFIGL_WIDTHMASK		0xe0
#define OBOE_PCONFIGL_PREAMBLEMASK	0x1f

#define OBOE_PCONFIG_BAUDMASK		0xfc00
#define OBOE_PCONFIG_BAUDSHIFT		10
#define OBOE_PCONFIG_WIDTHMASK		0x04e0
#define OBOE_PCONFIG_WIDTHSHIFT		5
#define OBOE_PCONFIG_PREAMBLEMASK	0x001f
#define OBOE_PCONFIG_PREAMBLESHIFT	0

#define OBOE_MAXLENL			OBOE_REG(0x1a)
#define OBOE_MAXLENH			OBOE_REG(0x1b)

#define OBOE_RXCOUNTH			OBOE_REG(0x1c) 
#define OBOE_RXCOUNTL			OBOE_REG(0x1d) 

#ifndef PCI_DEVICE_ID_FIR701
#define PCI_DEVICE_ID_FIR701 	0x0701
#endif

#ifndef PCI_DEVICE_ID_FIRD01
#define PCI_DEVICE_ID_FIRD01 	0x0d01
#endif

struct OboeSlot
{
  __u16 len;                    
  __u8 unused;
  __u8 control;                 
  __u32 address;                
}
__packed;

#define OBOE_NTASKS OBOE_TXRING_OFFSET_IN_SLOTS

struct OboeRing
{
  struct OboeSlot rx[OBOE_NTASKS];
  struct OboeSlot tx[OBOE_NTASKS];
};

#define OBOE_RING_LEN (sizeof(struct OboeRing))


#define OBOE_CTL_TX_HW_OWNS	0x80 
#define OBOE_CTL_TX_DISTX_CRC	0x40 
#define OBOE_CTL_TX_BAD_CRC     0x20 
#define OBOE_CTL_TX_SIP		0x10   
#define OBOE_CTL_TX_MKUNDER	0x08 
#define OBOE_CTL_TX_RTCENTX	0x04 
     
#define OBOE_CTL_TX_UNDER	0x01  


#define OBOE_CTL_RX_HW_OWNS	0x80 
#define OBOE_CTL_RX_PHYERR	0x40 
#define OBOE_CTL_RX_CRCERR	0x20 
#define OBOE_CTL_RX_LENGTH	0x10 
#define OBOE_CTL_RX_OVER	0x08   
#define OBOE_CTL_RX_SIRBAD	0x04 
#define OBOE_CTL_RX_RXEOF	0x02  


struct toshoboe_cb
{
  struct net_device *netdev;    
  struct tty_driver ttydev;

  struct irlap_cb *irlap;       

  chipio_t io;                  
  struct qos_info qos;          

  __u32 flags;                  

  struct pci_dev *pdev;         
  int base;                     


  int txpending;                
  int txs, rxs;                 

  int irdad;                    
  int async;                    


  int stopped;                  

  int filter;                   

  void *ringbuf;                
  struct OboeRing *ring;        

  void *tx_bufs[OBOE_RING_MAX_SIZE]; 
  void *rx_bufs[OBOE_RING_MAX_SIZE];


  int speed;                    
  int new_speed;                

  spinlock_t spinlock;		
  
  int int_rx;
  int int_tx;
  int int_txunder;
  int int_rxover;
  int int_sip;
};


#endif
