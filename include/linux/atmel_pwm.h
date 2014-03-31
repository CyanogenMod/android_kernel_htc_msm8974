#ifndef __LINUX_ATMEL_PWM_H
#define __LINUX_ATMEL_PWM_H

struct pwm_channel {
	void __iomem	*regs;
	unsigned	index;
	unsigned long	mck;
};

extern int pwm_channel_alloc(int index, struct pwm_channel *ch);
extern int pwm_channel_free(struct pwm_channel *ch);

extern int pwm_clk_alloc(unsigned prescale, unsigned div);
extern void pwm_clk_free(unsigned clk);

extern int __pwm_channel_onoff(struct pwm_channel *ch, int enabled);

#define pwm_channel_enable(ch)	__pwm_channel_onoff((ch), 1)
#define pwm_channel_disable(ch)	__pwm_channel_onoff((ch), 0)

extern int pwm_channel_handler(struct pwm_channel *ch,
		void (*handler)(struct pwm_channel *ch));

#define PWM_CMR		0x00		
#define		PWM_CPR_CPD	(1 << 10)	
#define		PWM_CPR_CPOL	(1 << 9)	
#define		PWM_CPR_CALG	(1 << 8)	
#define		PWM_CPR_CPRE	(0xf << 0)	
#define		PWM_CPR_CLKA	(0xb << 0)	
#define		PWM_CPR_CLKB	(0xc << 0)	
#define PWM_CDTY	0x04		
#define PWM_CPRD	0x08		
#define PWM_CCNT	0x0c		
#define PWM_CUPD	0x10		

static inline void
pwm_channel_writel(struct pwm_channel *pwmc, unsigned offset, u32 val)
{
	__raw_writel(val, pwmc->regs + offset);
}

static inline u32 pwm_channel_readl(struct pwm_channel *pwmc, unsigned offset)
{
	return __raw_readl(pwmc->regs + offset);
}

#endif 
