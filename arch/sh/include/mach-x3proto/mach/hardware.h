#ifndef __MACH_X3PROTO_HARDWARE_H
#define __MACH_X3PROTO_HARDWARE_H

struct gpio_chip;

int x3proto_gpio_setup(void);
extern struct gpio_chip x3proto_gpio_chip;

#define NR_BASEBOARD_GPIOS	16

#endif 
