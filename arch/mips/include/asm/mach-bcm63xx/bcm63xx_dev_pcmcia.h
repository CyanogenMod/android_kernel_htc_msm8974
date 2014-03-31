#ifndef BCM63XX_DEV_PCMCIA_H_
#define BCM63XX_DEV_PCMCIA_H_

struct bcm63xx_pcmcia_platform_data {
	unsigned int ready_gpio;
};

int bcm63xx_pcmcia_register(void);

#endif 
