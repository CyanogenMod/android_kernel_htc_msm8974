#ifndef _PARISC_SUPERIO_H
#define _PARISC_SUPERIO_H

#define IC_PIC1    0x20		
#define IC_PIC2    0xA0		

#define SIO_CR     0x5A		
#define SIO_ACPIBAR 0x88	
#define SIO_FDCBAR 0x90		
#define SIO_SP1BAR 0x94		
#define SIO_SP2BAR 0x98		
#define SIO_PPBAR  0x9C		

#define TRIGGER_1  0x67		
#define TRIGGER_2  0x68		

#define CFG_IR_SER    0x69	
#define CFG_IR_PFD    0x6a	
#define CFG_IR_IDE    0x6b	
#define CFG_IR_INTAB  0x6c	
#define CFG_IR_INTCD  0x6d	
#define CFG_IR_PS2    0x6e	
#define CFG_IR_FXBUS  0x6f	
#define CFG_IR_USB    0x70	
#define CFG_IR_ACPI   0x71	

#define CFG_IR_LOW     CFG_IR_SER	
#define CFG_IR_HIGH    CFG_IR_ACPI	

#define OCW2_EOI   0x20		
#define OCW2_SEOI  0x60		
#define OCW3_IIR   0x0A		
#define OCW3_ISR   0x0B		
#define OCW3_POLL  0x0C		

#define USB_IRQ    1		
#define SP1_IRQ    3		
#define SP2_IRQ    4		
#define PAR_IRQ    5		
#define FDC_IRQ    6		
#define IDE_IRQ    7		

#define USB_REG_CR	0x1f	

#define SUPERIO_NIRQS   8

struct superio_device {
	u32 fdc_base;
	u32 sp1_base;
	u32 sp2_base;
	u32 pp_base;
	u32 acpi_base;
	int suckyio_irq_enabled;
	struct pci_dev *lio_pdev;       
	struct pci_dev *usb_pdev;       
};


#define SUPERIO_IDE_FN 0 
#define SUPERIO_LIO_FN 1 
#define SUPERIO_USB_FN 2 

#define is_superio_device(x) \
	(((x)->vendor == PCI_VENDOR_ID_NS) && \
	(  ((x)->device == PCI_DEVICE_ID_NS_87415) \
	|| ((x)->device == PCI_DEVICE_ID_NS_87560_LIO) \
	|| ((x)->device == PCI_DEVICE_ID_NS_87560_USB) ) )

extern int superio_fixup_irq(struct pci_dev *pcidev); 

#endif 
