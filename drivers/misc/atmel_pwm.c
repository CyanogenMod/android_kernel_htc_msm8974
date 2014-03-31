#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/atmel_pwm.h>


#define	PWM_NCHAN	4		

struct pwm {
	spinlock_t		lock;
	struct platform_device	*pdev;
	u32			mask;
	int			irq;
	void __iomem		*base;
	struct clk		*clk;
	struct pwm_channel	*channel[PWM_NCHAN];
	void			(*handler[PWM_NCHAN])(struct pwm_channel *);
};


#define PWM_MR		0x00
#define PWM_ENA		0x04
#define PWM_DIS		0x08
#define PWM_SR		0x0c
#define PWM_IER		0x10
#define PWM_IDR		0x14
#define PWM_IMR		0x18
#define PWM_ISR		0x1c

static inline void pwm_writel(const struct pwm *p, unsigned offset, u32 val)
{
	__raw_writel(val, p->base + offset);
}

static inline u32 pwm_readl(const struct pwm *p, unsigned offset)
{
	return __raw_readl(p->base + offset);
}

static inline void __iomem *pwmc_regs(const struct pwm *p, int index)
{
	return p->base + 0x200 + index * 0x20;
}

static struct pwm *pwm;

static void pwm_dumpregs(struct pwm_channel *ch, char *tag)
{
	struct device	*dev = &pwm->pdev->dev;

	dev_dbg(dev, "%s: mr %08x, sr %08x, imr %08x\n",
		tag,
		pwm_readl(pwm, PWM_MR),
		pwm_readl(pwm, PWM_SR),
		pwm_readl(pwm, PWM_IMR));
	dev_dbg(dev,
		"pwm ch%d - mr %08x, dty %u, prd %u, cnt %u\n",
		ch->index,
		pwm_channel_readl(ch, PWM_CMR),
		pwm_channel_readl(ch, PWM_CDTY),
		pwm_channel_readl(ch, PWM_CPRD),
		pwm_channel_readl(ch, PWM_CCNT));
}


int pwm_channel_alloc(int index, struct pwm_channel *ch)
{
	unsigned long	flags;
	int		status = 0;

	
	if (!pwm || !(pwm->mask & 1 << index))
		return -ENODEV;

	if (index < 0 || index >= PWM_NCHAN || !ch)
		return -EINVAL;
	memset(ch, 0, sizeof *ch);

	spin_lock_irqsave(&pwm->lock, flags);
	if (pwm->channel[index])
		status = -EBUSY;
	else {
		clk_enable(pwm->clk);

		ch->regs = pwmc_regs(pwm, index);
		ch->index = index;

		
		ch->mck = clk_get_rate(pwm->clk);

		pwm->channel[index] = ch;
		pwm->handler[index] = NULL;

		
		pwm_writel(pwm, PWM_DIS, 1 << index);
		pwm_writel(pwm, PWM_IDR, 1 << index);
	}
	spin_unlock_irqrestore(&pwm->lock, flags);
	return status;
}
EXPORT_SYMBOL(pwm_channel_alloc);

static int pwmcheck(struct pwm_channel *ch)
{
	int		index;

	if (!pwm)
		return -ENODEV;
	if (!ch)
		return -EINVAL;
	index = ch->index;
	if (index < 0 || index >= PWM_NCHAN || pwm->channel[index] != ch)
		return -EINVAL;

	return index;
}

int pwm_channel_free(struct pwm_channel *ch)
{
	unsigned long	flags;
	int		t;

	spin_lock_irqsave(&pwm->lock, flags);
	t = pwmcheck(ch);
	if (t >= 0) {
		pwm->channel[t] = NULL;
		pwm->handler[t] = NULL;

		
		pwm_writel(pwm, PWM_DIS, 1 << t);
		pwm_writel(pwm, PWM_IDR, 1 << t);

		clk_disable(pwm->clk);
		t = 0;
	}
	spin_unlock_irqrestore(&pwm->lock, flags);
	return t;
}
EXPORT_SYMBOL(pwm_channel_free);

int __pwm_channel_onoff(struct pwm_channel *ch, int enabled)
{
	unsigned long	flags;
	int		t;

	

	spin_lock_irqsave(&pwm->lock, flags);
	t = pwmcheck(ch);
	if (t >= 0) {
		pwm_writel(pwm, enabled ? PWM_ENA : PWM_DIS, 1 << t);
		t = 0;
		pwm_dumpregs(ch, enabled ? "enable" : "disable");
	}
	spin_unlock_irqrestore(&pwm->lock, flags);

	return t;
}
EXPORT_SYMBOL(__pwm_channel_onoff);

int pwm_clk_alloc(unsigned prescale, unsigned div)
{
	unsigned long	flags;
	u32		mr;
	u32		val = (prescale << 8) | div;
	int		ret = -EBUSY;

	if (prescale >= 10 || div == 0 || div > 255)
		return -EINVAL;

	spin_lock_irqsave(&pwm->lock, flags);
	mr = pwm_readl(pwm, PWM_MR);
	if ((mr & 0xffff) == 0) {
		mr |= val;
		ret = PWM_CPR_CLKA;
	} else if ((mr & (0xffff << 16)) == 0) {
		mr |= val << 16;
		ret = PWM_CPR_CLKB;
	}
	if (ret > 0)
		pwm_writel(pwm, PWM_MR, mr);
	spin_unlock_irqrestore(&pwm->lock, flags);
	return ret;
}
EXPORT_SYMBOL(pwm_clk_alloc);

void pwm_clk_free(unsigned clk)
{
	unsigned long	flags;
	u32		mr;

	spin_lock_irqsave(&pwm->lock, flags);
	mr = pwm_readl(pwm, PWM_MR);
	if (clk == PWM_CPR_CLKA)
		pwm_writel(pwm, PWM_MR, mr & ~(0xffff << 0));
	if (clk == PWM_CPR_CLKB)
		pwm_writel(pwm, PWM_MR, mr & ~(0xffff << 16));
	spin_unlock_irqrestore(&pwm->lock, flags);
}
EXPORT_SYMBOL(pwm_clk_free);

int pwm_channel_handler(struct pwm_channel *ch,
		void (*handler)(struct pwm_channel *ch))
{
	unsigned long	flags;
	int		t;

	spin_lock_irqsave(&pwm->lock, flags);
	t = pwmcheck(ch);
	if (t >= 0) {
		pwm->handler[t] = handler;
		pwm_writel(pwm, handler ? PWM_IER : PWM_IDR, 1 << t);
		t = 0;
	}
	spin_unlock_irqrestore(&pwm->lock, flags);

	return t;
}
EXPORT_SYMBOL(pwm_channel_handler);

static irqreturn_t pwm_irq(int id, void *_pwm)
{
	struct pwm	*p = _pwm;
	irqreturn_t	handled = IRQ_NONE;
	u32		irqstat;
	int		index;

	spin_lock(&p->lock);

	
	irqstat = pwm_readl(pwm, PWM_ISR);

	while (irqstat) {
		struct pwm_channel *ch;
		void (*handler)(struct pwm_channel *ch);

		index = ffs(irqstat) - 1;
		irqstat &= ~(1 << index);
		ch = pwm->channel[index];
		handler = pwm->handler[index];
		if (handler && ch) {
			spin_unlock(&p->lock);
			handler(ch);
			spin_lock(&p->lock);
			handled = IRQ_HANDLED;
		}
	}

	spin_unlock(&p->lock);
	return handled;
}

static int __init pwm_probe(struct platform_device *pdev)
{
	struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int irq = platform_get_irq(pdev, 0);
	u32 *mp = pdev->dev.platform_data;
	struct pwm *p;
	int status = -EIO;

	if (pwm)
		return -EBUSY;
	if (!r || irq < 0 || !mp || !*mp)
		return -ENODEV;
	if (*mp & ~((1<<PWM_NCHAN)-1)) {
		dev_warn(&pdev->dev, "mask 0x%x ... more than %d channels\n",
			*mp, PWM_NCHAN);
		return -EINVAL;
	}

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	spin_lock_init(&p->lock);
	p->pdev = pdev;
	p->mask = *mp;
	p->irq = irq;
	p->base = ioremap(r->start, resource_size(r));
	if (!p->base)
		goto fail;
	p->clk = clk_get(&pdev->dev, "pwm_clk");
	if (IS_ERR(p->clk)) {
		status = PTR_ERR(p->clk);
		p->clk = NULL;
		goto fail;
	}

	status = request_irq(irq, pwm_irq, 0, pdev->name, p);
	if (status < 0)
		goto fail;

	pwm = p;
	platform_set_drvdata(pdev, p);

	return 0;

fail:
	if (p->clk)
		clk_put(p->clk);
	if (p->base)
		iounmap(p->base);

	kfree(p);
	return status;
}

static int __exit pwm_remove(struct platform_device *pdev)
{
	struct pwm *p = platform_get_drvdata(pdev);

	if (p != pwm)
		return -EINVAL;

	clk_enable(pwm->clk);
	pwm_writel(pwm, PWM_DIS, (1 << PWM_NCHAN) - 1);
	pwm_writel(pwm, PWM_IDR, (1 << PWM_NCHAN) - 1);
	clk_disable(pwm->clk);

	pwm = NULL;

	free_irq(p->irq, p);
	clk_put(p->clk);
	iounmap(p->base);
	kfree(p);

	return 0;
}

static struct platform_driver atmel_pwm_driver = {
	.driver = {
		.name = "atmel_pwm",
		.owner = THIS_MODULE,
	},
	.remove = __exit_p(pwm_remove),

};

static int __init pwm_init(void)
{
	return platform_driver_probe(&atmel_pwm_driver, pwm_probe);
}
module_init(pwm_init);

static void __exit pwm_exit(void)
{
	platform_driver_unregister(&atmel_pwm_driver);
}
module_exit(pwm_exit);

MODULE_DESCRIPTION("Driver for AT32/AT91 PWM module");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:atmel_pwm");
