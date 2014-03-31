#ifndef __LINUX_I2C_DM355EVM_MSP
#define __LINUX_I2C_DM355EVM_MSP

/*
 * Written against Spectrum's writeup for the A4 firmware revision,
 * and tweaked to match source and rev D2 schematics by removing CPLD
 * and NOR flash hooks (which were last appropriate in rev B boards).
 *
 * Note that the firmware supports a flavor of write posting ... to be
 * sure a write completes, issue another read or write.
 */

extern int dm355evm_msp_write(u8 value, u8 reg);
extern int dm355evm_msp_read(u8 reg);


#define DM355EVM_MSP_COMMAND		0x00
#	define MSP_COMMAND_NULL		0
#	define MSP_COMMAND_RESET_COLD	1
#	define MSP_COMMAND_RESET_WARM	2
#	define MSP_COMMAND_RESET_WARM_I	3
#	define MSP_COMMAND_POWEROFF	4
#	define MSP_COMMAND_IR_REINIT	5
#define DM355EVM_MSP_STATUS		0x01
#	define MSP_STATUS_BAD_OFFSET	BIT(0)
#	define MSP_STATUS_BAD_COMMAND	BIT(1)
#	define MSP_STATUS_POWER_ERROR	BIT(2)
#	define MSP_STATUS_RXBUF_OVERRUN	BIT(3)
#define DM355EVM_MSP_RESET		0x02	
#	define MSP_RESET_DC5		BIT(0)
#	define MSP_RESET_TVP5154	BIT(2)
#	define MSP_RESET_IMAGER		BIT(3)
#	define MSP_RESET_ETHERNET	BIT(4)
#	define MSP_RESET_SYS		BIT(5)
#	define MSP_RESET_AIC33		BIT(7)

#define DM355EVM_MSP_LED		0x03	
#define DM355EVM_MSP_SWITCH1		0x04	
#	define MSP_SWITCH1_SW6_1	BIT(0)
#	define MSP_SWITCH1_SW6_2	BIT(1)
#	define MSP_SWITCH1_SW6_3	BIT(2)
#	define MSP_SWITCH1_SW6_4	BIT(3)
#	define MSP_SWITCH1_J1		BIT(4)	
#	define MSP_SWITCH1_MSP_INT	BIT(5)	
#define DM355EVM_MSP_SWITCH2		0x05	
#	define MSP_SWITCH2_SW10		BIT(3)
#	define MSP_SWITCH2_SW11		BIT(4)
#	define MSP_SWITCH2_SW12		BIT(5)
#	define MSP_SWITCH2_SW13		BIT(6)
#	define MSP_SWITCH2_SW14		BIT(7)
#define DM355EVM_MSP_SDMMC		0x06	
#	define MSP_SDMMC_0_WP		BIT(1)
#	define MSP_SDMMC_0_CD		BIT(2)	
#	define MSP_SDMMC_1_WP		BIT(3)
#	define MSP_SDMMC_1_CD		BIT(4)	
#define DM355EVM_MSP_FIRMREV		0x07	
#define DM355EVM_MSP_VIDEO_IN		0x08	
#	define MSP_VIDEO_IMAGER		BIT(7)	


#define DM355EVM_MSP_RTC_0		0x12	
#define DM355EVM_MSP_RTC_1		0x13
#define DM355EVM_MSP_RTC_2		0x14
#define DM355EVM_MSP_RTC_3		0x15	

#define DM355EVM_MSP_INPUT_COUNT	0x16	
#define DM355EVM_MSP_INPUT_HIGH		0x17
#define DM355EVM_MSP_INPUT_LOW		0x18

#endif 
