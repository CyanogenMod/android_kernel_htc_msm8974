#ifndef BCM63XX_PCMCIA_H_
#define BCM63XX_PCMCIA_H_

#include <linux/types.h>
#include <linux/timer.h>
#include <pcmcia/ss.h>
#include <bcm63xx_dev_pcmcia.h>

#define BCM63XX_PCMCIA_POLL_RATE	500

enum {
	CARD_CARDBUS = (1 << 0),
	CARD_PCCARD = (1 << 1),
	CARD_5V = (1 << 2),
	CARD_3V = (1 << 3),
	CARD_XV = (1 << 4),
	CARD_YV = (1 << 5),
};

struct bcm63xx_pcmcia_socket {
	struct pcmcia_socket socket;

	
	struct bcm63xx_pcmcia_platform_data *pd;

	
	spinlock_t lock;

	
	struct resource *reg_res;

	
	void __iomem *base;

	
	int card_detected;

	
	u8 card_type;

	
	unsigned int old_status;

	
	socket_state_t requested_state;

	
	struct timer_list timer;

	
	struct resource *attr_res;
	struct resource *common_res;
	struct resource *io_res;

	
	void __iomem *io_base;
};

#endif 
