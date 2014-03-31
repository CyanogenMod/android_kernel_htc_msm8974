#ifndef _ASM_ETRAXGPIO_H
#define _ASM_ETRAXGPIO_H

#define GPIO_MINOR_FIRST 0

#define ETRAXGPIO_IOCTYPE 43

#ifdef CONFIG_ETRAX_ARCH_V10
#define GPIO_MINOR_A 0
#define GPIO_MINOR_B 1
#define GPIO_MINOR_LEDS 2
#define GPIO_MINOR_G 3
#define GPIO_MINOR_LAST 3
#define GPIO_MINOR_LAST_REAL GPIO_MINOR_LAST
#endif

#ifdef CONFIG_ETRAXFS
#define GPIO_MINOR_A 0
#define GPIO_MINOR_B 1
#define GPIO_MINOR_LEDS 2
#define GPIO_MINOR_C 3
#define GPIO_MINOR_D 4
#define GPIO_MINOR_E 5
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
#define GPIO_MINOR_V 6
#define GPIO_MINOR_LAST 6
#else
#define GPIO_MINOR_LAST 5
#endif
#define GPIO_MINOR_LAST_REAL GPIO_MINOR_LAST
#endif

#ifdef CONFIG_CRIS_MACH_ARTPEC3
#define GPIO_MINOR_A 0
#define GPIO_MINOR_B 1
#define GPIO_MINOR_LEDS 2
#define GPIO_MINOR_C 3
#define GPIO_MINOR_D 4
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
#define GPIO_MINOR_V 6
#define GPIO_MINOR_LAST 6
#else
#define GPIO_MINOR_LAST 4
#endif
#define GPIO_MINOR_FIRST_PWM 16
#define GPIO_MINOR_PWM0 (GPIO_MINOR_FIRST_PWM+0)
#define GPIO_MINOR_PWM1 (GPIO_MINOR_FIRST_PWM+1)
#define GPIO_MINOR_PWM2 (GPIO_MINOR_FIRST_PWM+2)
#define GPIO_MINOR_PPWM (GPIO_MINOR_FIRST_PWM+3)
#define GPIO_MINOR_LAST_PWM GPIO_MINOR_PPWM
#define GPIO_MINOR_LAST_REAL GPIO_MINOR_LAST_PWM
#endif




#define IO_READBITS  0x1  
#define IO_SETBITS   0x2  
#define IO_CLRBITS   0x3  


#define IO_HIGHALARM 0x4  
#define IO_LOWALARM  0x5  
#define IO_CLRALARM  0x6  

#define IO_LEDACTIVE_SET 0x7 

#define IO_READDIR    0x8  
#define IO_SETINPUT   0x9  
#define IO_SETOUTPUT  0xA  

#define IO_LED_SETBIT 0xB
#define IO_LED_CLRBIT 0xC

#define IO_SHUTDOWN   0xD
#define IO_GET_PWR_BT 0xE

#define IO_CFG_WRITE_MODE 0xF
#define IO_CFG_WRITE_MODE_VALUE(msb, data_mask, clk_mask) \
	( (((msb)&1) << 16) | (((data_mask) &0xFF) << 8) | ((clk_mask) & 0xFF) )

#define IO_READ_INBITS   0x10 
#define IO_READ_OUTBITS  0x11 
#define IO_SETGET_INPUT  0x12 
			      
#define IO_SETGET_OUTPUT 0x13 
			      


#define IO_PWM_SET_MODE     0x20

enum io_pwm_mode {
	PWM_OFF = 0,		
	PWM_STANDARD = 1,	
	PWM_FAST = 2,		
	PWM_VARFREQ = 3,	
	PWM_SOFT = 4		
};

struct io_pwm_set_mode {
	enum io_pwm_mode mode;
};

#define IO_PWM_SET_PERIOD   0x21

struct io_pwm_set_period {
	unsigned int lo;		
	unsigned int hi;		
};

#define IO_PWM_SET_DUTY     0x22

struct io_pwm_set_duty {
	int duty;		
};

#define IO_PWM_GET_PERIOD   0x23

struct io_pwm_get_period {
	unsigned int lo;
	unsigned int hi;
	unsigned int cnt;
};

#define IO_PWM_SET_INPUT_SRC   0x24
struct io_pwm_set_input_src {
	unsigned int src;	
};

#define IO_PPWM_SET_DUTY     0x25

struct io_ppwm_set_duty {
	int duty;		
};

#define IO_PWMCLK_SETGET_CONFIG 0x26
struct gpio_pwmclk_conf {
  unsigned int gpiopin; 
  unsigned int baseclk; 
  unsigned int low;     
  unsigned int high;    
};


#endif
