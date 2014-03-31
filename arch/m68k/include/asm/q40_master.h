
#ifndef _Q40_MASTER_H
#define _Q40_MASTER_H

#include <asm/raw_io.h>


#define q40_master_addr 0xff000000

#define IIRQ_REG            0x0       
#define EIRQ_REG            0x4       
#define KEYCODE_REG         0x1c      
#define DISPLAY_CONTROL_REG 0x18
#define FRAME_CLEAR_REG     0x24
#define LED_REG             0x30

#define Q40_LED_ON()        master_outb(1,LED_REG)
#define Q40_LED_OFF()       master_outb(0,LED_REG)

#define INTERRUPT_REG       IIRQ_REG  
#define KEY_IRQ_ENABLE_REG  0x08      
#define KEYBOARD_UNLOCK_REG 0x20      

#define SAMPLE_ENABLE_REG   0x14      
#define SAMPLE_RATE_REG     0x2c
#define SAMPLE_CLEAR_REG    0x28
#define SAMPLE_LOW          0x00
#define SAMPLE_HIGH         0x01

#define FRAME_RATE_REG       0x38      

#if 0
#define SER_ENABLE_REG      0x0c      
#endif
#define EXT_ENABLE_REG      0x10      


#define master_inb(_reg_)      in_8((unsigned char *)q40_master_addr+_reg_)
#define master_outb(_b_,_reg_)  out_8((unsigned char *)q40_master_addr+_reg_,_b_)


#define Q40_RTC_BASE	    (0xff021ffc)

#define Q40_RTC_YEAR        (*(volatile unsigned char *)(Q40_RTC_BASE+0))
#define Q40_RTC_MNTH        (*(volatile unsigned char *)(Q40_RTC_BASE-4))
#define Q40_RTC_DATE        (*(volatile unsigned char *)(Q40_RTC_BASE-8))
#define Q40_RTC_DOW         (*(volatile unsigned char *)(Q40_RTC_BASE-12))
#define Q40_RTC_HOUR        (*(volatile unsigned char *)(Q40_RTC_BASE-16))
#define Q40_RTC_MINS        (*(volatile unsigned char *)(Q40_RTC_BASE-20))
#define Q40_RTC_SECS        (*(volatile unsigned char *)(Q40_RTC_BASE-24))
#define Q40_RTC_CTRL        (*(volatile unsigned char *)(Q40_RTC_BASE-28))

#define Q40_RTC_READ   64  
#define Q40_RTC_WRITE  128

#include "q40ints.h"

#define DAC_LEFT  ((unsigned char *)0xff008000)
#define DAC_RIGHT ((unsigned char *)0xff008004)

#endif 
