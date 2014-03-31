

#include <linux/kernel.h>

typedef void __iomem *virt_addr_t;

#define CYCLE_DELAY 5

#define osp_MicroDelay(microsec) {unsigned long useconds = (microsec); \
                                  udelay((useconds));}


#define CS_HIGH		0x0002	
#define CS_LOW		0x0000	
#define CLK_HIGH	0x0004	
#define CLK_LOW		0x0000	
#define SI_HIGH		0x0001	
#define SI_LOW		0x0000	

#if 0
static u_int32_t rdsrtab[] = {
	CS_HIGH | CLK_HIGH,
	CS_LOW | CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW | SI_HIGH,
	CLK_HIGH | SI_HIGH,	
	CLK_LOW | SI_LOW,
	CLK_HIGH,		
	CLK_LOW | SI_HIGH,
	CLK_HIGH | SI_HIGH	
};
#endif 

static u_int32_t readtab[] = {
	CS_LOW | CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW,
	CLK_HIGH,		
	CLK_LOW | SI_HIGH,
	CLK_HIGH | SI_HIGH,	
	CLK_LOW | SI_HIGH,
	CLK_HIGH | SI_HIGH	
};

static u_int32_t clocktab[] = {
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW,
	CLK_HIGH,
	CLK_LOW
};

#define NICSTAR_REG_WRITE(bs, reg, val) \
	while ( readl(bs + STAT) & 0x0200 ) ; \
	writel((val),(base)+(reg))
#define NICSTAR_REG_READ(bs, reg) \
	readl((base)+(reg))
#define NICSTAR_REG_GENERAL_PURPOSE GP

#if 0
u_int32_t nicstar_read_eprom_status(virt_addr_t base)
{
	u_int32_t val;
	u_int32_t rbyte;
	int32_t i, j;

	
	val = NICSTAR_REG_READ(base, NICSTAR_REG_GENERAL_PURPOSE) & 0xFFFFFFF0;

	for (i = 0; i < ARRAY_SIZE(rdsrtab); i++) {
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | rdsrtab[i]));
		osp_MicroDelay(CYCLE_DELAY);
	}

	
	

	rbyte = 0;
	for (i = 7, j = 0; i >= 0; i--) {
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | clocktab[j++]));
		rbyte |= (((NICSTAR_REG_READ(base, NICSTAR_REG_GENERAL_PURPOSE)
			    & 0x00010000) >> 16) << i);
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | clocktab[j++]));
		osp_MicroDelay(CYCLE_DELAY);
	}
	NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE, 2);
	osp_MicroDelay(CYCLE_DELAY);
	return rbyte;
}
#endif 


static u_int8_t read_eprom_byte(virt_addr_t base, u_int8_t offset)
{
	u_int32_t val = 0;
	int i, j = 0;
	u_int8_t tempread = 0;

	val = NICSTAR_REG_READ(base, NICSTAR_REG_GENERAL_PURPOSE) & 0xFFFFFFF0;

	
	for (i = 0; i < ARRAY_SIZE(readtab); i++) {
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | readtab[i]));
		osp_MicroDelay(CYCLE_DELAY);
	}

	
	for (i = 7; i >= 0; i--) {
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | clocktab[j++] | ((offset >> i) & 1)));
		osp_MicroDelay(CYCLE_DELAY);
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | clocktab[j++] | ((offset >> i) & 1)));
		osp_MicroDelay(CYCLE_DELAY);
	}

	j = 0;

	
	for (i = 7; i >= 0; i--) {
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | clocktab[j++]));
		osp_MicroDelay(CYCLE_DELAY);
		tempread |=
		    (((NICSTAR_REG_READ(base, NICSTAR_REG_GENERAL_PURPOSE)
		       & 0x00010000) >> 16) << i);
		NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
				  (val | clocktab[j++]));
		osp_MicroDelay(CYCLE_DELAY);
	}

	NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE, 2);
	osp_MicroDelay(CYCLE_DELAY);
	return tempread;
}

static void nicstar_init_eprom(virt_addr_t base)
{
	u_int32_t val;

	val = NICSTAR_REG_READ(base, NICSTAR_REG_GENERAL_PURPOSE) & 0xFFFFFFF0;

	NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
			  (val | CS_HIGH | CLK_HIGH));
	osp_MicroDelay(CYCLE_DELAY);

	NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
			  (val | CS_HIGH | CLK_LOW));
	osp_MicroDelay(CYCLE_DELAY);

	NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
			  (val | CS_HIGH | CLK_HIGH));
	osp_MicroDelay(CYCLE_DELAY);

	NICSTAR_REG_WRITE(base, NICSTAR_REG_GENERAL_PURPOSE,
			  (val | CS_HIGH | CLK_LOW));
	osp_MicroDelay(CYCLE_DELAY);
}


static void
nicstar_read_eprom(virt_addr_t base,
		   u_int8_t prom_offset, u_int8_t * buffer, u_int32_t nbytes)
{
	u_int i;

	for (i = 0; i < nbytes; i++) {
		buffer[i] = read_eprom_byte(base, prom_offset);
		++prom_offset;
		osp_MicroDelay(CYCLE_DELAY);
	}
}
