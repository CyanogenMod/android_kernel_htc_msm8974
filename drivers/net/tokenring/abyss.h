
#ifndef __LINUX_MADGETR_H
#define __LINUX_MADGETR_H

#ifdef __KERNEL__

#define PCIBM2_INT_STATUS_REG          ((short)-15)
#define PCIBM2_INT_CONTROL_REG         ((short)-14)
#define PCIBM2_RESET_REG               ((short)-12)
#define PCIBM2_SEEPROM_REG             ((short)-9) 

#define PCIBM2_INT_CONTROL_REG_SINTEN           0x02
#define PCIBM2_INT_CONTROL_REG_PCI_ERR_ENABLE   0x80
#define PCIBM2_INT_STATUS_REG_PCI_ERR           0x80

#define PCIBM2_RESET_REG_CHIP_NRES              0x01
#define PCIBM2_RESET_REG_FIFO_NRES              0x02
#define PCIBM2_RESET_REG_SIF_NRES               0x04

#define PCIBM2_FIFO_THRESHOLD   0x21
#define PCIBM2_BURST_LENGTH     0x22

#define AT24_ENABLE             0x04
#define AT24_DATA               0x02
#define AT24_CLOCK              0x01

#define AT24_WRITE              0xA0
#define AT24_READ               0xA1

#define PCIBM2_SEEPROM_BIA          0x12
#define PCIBM2_SEEPROM_RING_SPEED   0x18
#define PCIBM2_SEEPROM_RAM_SIZE     0x1A
#define PCIBM2_SEEPROM_HWF1         0x1C
#define PCIBM2_SEEPROM_HWF2         0x1E


#endif 
#endif 
