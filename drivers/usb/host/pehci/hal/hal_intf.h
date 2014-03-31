/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : hal
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* This is a hardware abstraction layer header file.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#ifndef HAL_INTF_H
#define HAL_INTF_H


#define HCD_PACKAGE

#define NON_PCI

#ifdef MSEC_INT_BASED
#define THREAD_BASED 
#endif

#ifndef DATABUS_WIDTH_16
#define DATABUS_WIDTH_16
#endif

#ifdef	DATABUS_WIDTH_16
#endif


#define	DMA_BUF_SIZE	(4096 * 2)

#define ISP1763_CHIPID  0x176320

#define ISP1763_HC				0	
#define ISP1763_DC				1	
#define ISP1763_OTG				2	
#define ISP1763_LAST_DEV			(ISP1763_OTG + 1)
#define ISP1763_1ST_DEV				(ISP1763_HC)

#ifdef PXA300
#define HC_SPARAMS_REG					(0x04<<1)	
#define HC_CPARAMS_REG					(0x08<<1)	

#define HC_USBCMD_REG						(0x8C<<1)	
#define HC_USBSTS_REG						(0x90<<1)	
#define HC_INTERRUPT_REG_EHCI				(0x94<<1)	
#define HC_FRINDEX_REG						(0x98<<1)	

#define HC_CONFIGFLAG_REG					(0x9C<<1)	
#define HC_PORTSC1_REG					(0xA0<<1)	

#define HC_ISO_PTD_DONEMAP_REG			(0xA4<<1)	
#define HC_ISO_PTD_SKIPMAP_REG			(0xA6<<1)	
#define HC_ISO_PTD_LASTPTD_REG				(0xA8<<1)	

#define HC_INT_PTD_DONEMAP_REG			(0xAA<<1)	
#define HC_INT_PTD_SKIPMAP_REG				(0xAC<<1)	
#define HC_INT_PTD_LASTPTD_REG				(0xAE<<1)	

#define HC_ATL_PTD_DONEMAP_REG			(0xB0<<1)	
#define HC_ATL_PTD_SKIPMAP_REG				(0xB2<<1)	
#define HC_ATL_PTD_LASTPTD_REG				(0xB4<<1)	

#define HC_HW_MODE_REG					(0x0C<<1)	
#define HC_CHIP_ID_REG						(0x70<<1)	
#define HC_SCRATCH_REG					(0x78<<1)	
#define HC_RESET_REG						(0xB8<<1)	
#define HC_HWMODECTRL_REG				(0xB6<<1)
#define HC_UNLOCK_DEVICE					(0x7C<<1)

#define HC_INTERRUPT_REG					(0xD4<<1)	
#define HC_INTENABLE_REG					(0xD6<<1)	
#define HC_ISO_IRQ_MASK_OR_REG			(0xD8<<1)	
#define HC_INT_IRQ_MASK_OR_REG			(0xDA<<1)	
#define HC_ATL_IRQ_MASK_OR_REG			(0xDC<<1)	
#define HC_ISO_IRQ_MASK_AND_REG			(0xDE<<1)	
#define HC_INT_IRQ_MASK_AND_REG			(0xE0<<1)	
#define HC_ATL_IRQ_MASK_AND_REG			(0xE2<<1)	

#define HC_POWER_DOWN_CONTROL_REG		(0xD0<<1)

#define HC_DMACONFIG_REG					(0xBC<<1)	
#define HC_MEM_READ_REG					(0xC4<<1)	
#define HC_DATA_REG						(0xC6<<1)	

#define OTG_CTRL_SET_REG					(0xE4<<1)
#define OTG_CTRL_CLEAR_REG					(0xE6<<1)
#define OTG_SOURCE_REG					(0xE8<<1)

#define OTG_INTR_EN_F_SET_REG				(0xF0<<1)
#define OTG_INTR_EN_R_SET_REG				(0xF4<<1)	

#else
#define HC_SPARAMS_REG					0x04	
#define HC_CPARAMS_REG					0x08	

#define HC_USBCMD_REG					0x8C	
#define HC_USBSTS_REG					0x90	
#define HC_INTERRUPT_REG_EHCI			0x94	
#define HC_FRINDEX_REG					0x98	

#define HC_CONFIGFLAG_REG				0x9C	
#define HC_PORTSC1_REG					0xA0	

#define HC_ISO_PTD_DONEMAP_REG			0xA4	
#define HC_ISO_PTD_SKIPMAP_REG			0xA6	
#define HC_ISO_PTD_LASTPTD_REG			0xA8	

#define HC_INT_PTD_DONEMAP_REG			0xAA	
#define HC_INT_PTD_SKIPMAP_REG			0xAC	
#define HC_INT_PTD_LASTPTD_REG			0xAE	

#define HC_ATL_PTD_DONEMAP_REG			0xB0	
#define HC_ATL_PTD_SKIPMAP_REG			0xB2	
#define HC_ATL_PTD_LASTPTD_REG			0xB4	

#define HC_HW_MODE_REG					0x0C 
#define HC_CHIP_ID_REG					0x70	
#define HC_SCRATCH_REG					0x78	
#define HC_RESET_REG					0xB8	
#define HC_HWMODECTRL_REG				0xB6 
#define HC_UNLOCK_DEVICE				0x7C

#define HC_INTERRUPT_REG				0xD4	
#define HC_INTENABLE_REG				0xD6	
#define HC_ISO_IRQ_MASK_OR_REG			0xD8	
#define HC_INT_IRQ_MASK_OR_REG			0xDA	
#define HC_ATL_IRQ_MASK_OR_REG			0xDC	
#define HC_ISO_IRQ_MASK_AND_REG			0xDE	
#define HC_INT_IRQ_MASK_AND_REG			0xE0	
#define HC_ATL_IRQ_MASK_AND_REG			0xE2	

#define HC_POWER_DOWN_CONTROL_REG		0xD0

#define HC_DMACONFIG_REG				0xBC	
#define HC_MEM_READ_REG					0xC4	
#define HC_DATA_REG						0xC6	

#define OTG_CTRL_SET_REG				0xE4
#define OTG_CTRL_CLEAR_REG				0xE6
#define OTG_SOURCE_REG					0xE8

#define OTG_INTR_EN_F_SET_REG			0xF0	
#define OTG_INTR_EN_R_SET_REG			0xF4	

#endif

#define	OTG_CTRL_DPPULLUP				0x0001
#define	OTG_CTRL_DPPULLDOWN				0x0002
#define	OTG_CTRL_DMPULLDOWN				0x0004
#define	OTG_CTRL_VBUS_DRV				0x0010
#define	OTG_CTRL_VBUS_DISCHRG			0x0020
#define	OTG_CTRL_VBUS_CHRG				0x0040
#define	OTG_CTRL_SW_SEL_HC_DC			0x0080
#define	OTG_CTRL_BDIS_ACON_EN			0x0100
#define	OTG_CTRL_OTG_SE0_EN				0x0200
#define	OTG_CTRL_OTG_DISABLE			0x0400
#define	OTG_CTRL_VBUS_DRV_PORT2			0x1000
#define	OTG_CTRL_SW_SEL_HC_2			0x8000



#ifdef PXA300
#define HC_BUFFER_STATUS_REG			(0xBA<<1)
#define HC_INT_THRESHOLD_REG			(0xC8<<1)
#else
#define HC_BUFFER_STATUS_REG			0xBA
#define HC_INT_THRESHOLD_REG			0xC8
#endif

#define HC_OTG_INTERRUPT				0x400

#ifdef PXA300
#define DC_CHIPID						(0x70<<1)
#else
#define DC_CHIPID						0x70
#endif


#ifdef PXA300
#define FPGA_CONFIG_REG				(0x100<<1)
#else
#define FPGA_CONFIG_REG					0x100
#endif

#define HC_HW_MODE_GOBAL_INTR_ENABLE	0x01
#define HC_HW_MODE_INTR_EDGE			0x02
#define HC_HW_MODE_INTR_POLARITY_HIGH	0x04
#define HC_HW_MODE_LOCK				0x08
#define HC_HW_MODE_DATABUSWIDTH_8	0x10
#define HC_HW_MODE_DREQ_POL_HIGH		0x20
#define HC_HW_MODE_DACK_POL_HIGH		0x40
#define HC_HW_MODE_COMN_INT			0x80

struct isp1763_driver;
typedef struct _isp1763_id {
	u16 idVendor;
	u16 idProduct;
	u32 driver_info;
} isp1763_id;

typedef struct isp1763_dev {
	
#ifdef  NON_PCI 
		struct platform_device *dev;
#else 
	struct pci_dev *pcidev;
#endif
	struct isp1763_driver *driver;	
	void *driver_data;	
	void *otg_driver_data;	
	unsigned char index;	
	unsigned int irq;	
	void (*handler) (struct isp1763_dev * dev, void *isr_data);	
	void *isr_data;		
	unsigned long int_reg;	
	unsigned long alt_int_reg;	
	unsigned long start;
	unsigned long length;
	struct resource *mem_res;
	unsigned long io_base;	
	unsigned long io_len;	

	unsigned long chip_id;	

	char name[80];		
	int active;		

	
	unsigned long dma;
	u8 *baseaddress;	
	u8 *dmabase;
	isp1763_id *id;
} isp1763_dev_t;


typedef struct isp1763_driver {
	char *name;
	unsigned long index;	
	isp1763_id *id;		
	int (*probe) (struct isp1763_dev * dev, isp1763_id * id);	
	void (*remove) (struct isp1763_dev * dev);	
	
	void (*suspend) (struct isp1763_dev * dev);	
	void (*resume) (struct isp1763_dev * dev);	
	void (*remotewakeup) (struct isp1763_dev *dev);  
	void (*powerup) (struct isp1763_dev *dev);  
	void (*powerdown)	(struct isp1763_dev *dev); 
} isp_1763_driver_t;

struct usb_device *phci_register_otg_device(struct isp1763_dev *dev);

int phci_suspend_otg_port(struct isp1763_dev *dev, u32 command);
int phci_enumerate_otg_port(struct isp1763_dev *dev, u32 command);

extern int isp1763_register_driver(struct isp1763_driver *drv);
extern void isp1763_unregister_driver(struct isp1763_driver *drv);
extern int isp1763_request_irq(void (*handler)(struct isp1763_dev * dev, void *isr_data),
		      struct isp1763_dev *dev, void *isr_data);
extern void isp1763_free_irq(struct isp1763_dev *dev, void *isr_data);

extern u32 isp1763_reg_read32(isp1763_dev_t * dev, u16 reg, u32 data);
extern u16 isp1763_reg_read16(isp1763_dev_t * dev, u16 reg, u16 data);
extern u8 isp1763_reg_read8(struct isp1763_dev *dev, u16 reg, u8 data);
extern void isp1763_reg_write32(isp1763_dev_t * dev, u16 reg, u32 data);
extern void isp1763_reg_write16(isp1763_dev_t * dev, u16 reg, u16 data);
extern void isp1763_reg_write8(struct isp1763_dev *dev, u16 reg, u8 data);
extern int isp1763_mem_read(isp1763_dev_t * dev, u32 start_add,
		     u32 end_add, u32 * buffer, u32 length, u16 dir);
extern int isp1763_mem_write(isp1763_dev_t * dev, u32 start_add,
		      u32 end_add, u32 * buffer, u32 length, u16 dir);
#endif 
