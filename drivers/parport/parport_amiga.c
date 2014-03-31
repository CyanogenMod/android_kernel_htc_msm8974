
#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/amigahw.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/amigaints.h>

#undef DEBUG
#ifdef DEBUG
#define DPRINTK printk
#else
#define DPRINTK(x...)	do { } while (0)
#endif


static void amiga_write_data(struct parport *p, unsigned char data)
{
	DPRINTK(KERN_DEBUG "write_data %c\n",data);
	
	ciaa.prb = data;
	mb();
}

static unsigned char amiga_read_data(struct parport *p)
{
	
	return ciaa.prb;
}

#if 0
static unsigned char control_pc_to_amiga(unsigned char control)
{
	unsigned char ret = 0;

	if (control & PARPORT_CONTROL_SELECT) 
		;
	if (control & PARPORT_CONTROL_INIT) 
		;
	if (control & PARPORT_CONTROL_AUTOFD) 
		;
	if (control & PARPORT_CONTROL_STROBE) 
		;
	return ret;
}
#endif

static unsigned char control_amiga_to_pc(unsigned char control)
{
	return PARPORT_CONTROL_SELECT |
	      PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_STROBE;
}

static void amiga_write_control(struct parport *p, unsigned char control)
{
	DPRINTK(KERN_DEBUG "write_control %02x\n",control);
	
}
	
static unsigned char amiga_read_control( struct parport *p)
{
	DPRINTK(KERN_DEBUG "read_control \n");
	return control_amiga_to_pc(0);
}

static unsigned char amiga_frob_control( struct parport *p, unsigned char mask, unsigned char val)
{
	unsigned char old;

	DPRINTK(KERN_DEBUG "frob_control mask %02x, value %02x\n",mask,val);
	old = amiga_read_control(p);
	amiga_write_control(p, (old & ~mask) ^ val);
	return old;
}

#if 0 
static unsigned char status_pc_to_amiga(unsigned char status)
{
	unsigned char ret = 1;

	if (status & PARPORT_STATUS_BUSY) 
		ret &= ~1;
	if (status & PARPORT_STATUS_ACK) 
		;
	if (status & PARPORT_STATUS_PAPEROUT) 
		ret |= 2;
	if (status & PARPORT_STATUS_SELECT) 
		ret |= 4;
	if (status & PARPORT_STATUS_ERROR) 
		;
	return ret;
}
#endif

static unsigned char status_amiga_to_pc(unsigned char status)
{
	unsigned char ret = PARPORT_STATUS_BUSY | PARPORT_STATUS_ACK | PARPORT_STATUS_ERROR;

	if (status & 1) 
		ret &= ~PARPORT_STATUS_BUSY;
	if (status & 2) 
		ret |= PARPORT_STATUS_PAPEROUT;
	if (status & 4) 
		ret |= PARPORT_STATUS_SELECT;
	

	return ret;
}

static unsigned char amiga_read_status(struct parport *p)
{
	unsigned char status;

	status = status_amiga_to_pc(ciab.pra & 7);
	DPRINTK(KERN_DEBUG "read_status %02x\n", status);
	return status;
}

static void amiga_enable_irq(struct parport *p)
{
	enable_irq(IRQ_AMIGA_CIAA_FLG);
}

static void amiga_disable_irq(struct parport *p)
{
	disable_irq(IRQ_AMIGA_CIAA_FLG);
}

static void amiga_data_forward(struct parport *p)
{
	DPRINTK(KERN_DEBUG "forward\n");
	ciaa.ddrb = 0xff; 
	mb();
}

static void amiga_data_reverse(struct parport *p)
{
	DPRINTK(KERN_DEBUG "reverse\n");
	ciaa.ddrb = 0; 
	mb();
}

static void amiga_init_state(struct pardevice *dev, struct parport_state *s)
{
	s->u.amiga.data = 0;
	s->u.amiga.datadir = 255;
	s->u.amiga.status = 0;
	s->u.amiga.statusdir = 0;
}

static void amiga_save_state(struct parport *p, struct parport_state *s)
{
	mb();
	s->u.amiga.data = ciaa.prb;
	s->u.amiga.datadir = ciaa.ddrb;
	s->u.amiga.status = ciab.pra & 7;
	s->u.amiga.statusdir = ciab.ddra & 7;
	mb();
}

static void amiga_restore_state(struct parport *p, struct parport_state *s)
{
	mb();
	ciaa.prb = s->u.amiga.data;
	ciaa.ddrb = s->u.amiga.datadir;
	ciab.pra |= (ciab.pra & 0xf8) | s->u.amiga.status;
	ciab.ddra |= (ciab.ddra & 0xf8) | s->u.amiga.statusdir;
	mb();
}

static struct parport_operations pp_amiga_ops = {
	.write_data	= amiga_write_data,
	.read_data	= amiga_read_data,

	.write_control	= amiga_write_control,
	.read_control	= amiga_read_control,
	.frob_control	= amiga_frob_control,

	.read_status	= amiga_read_status,

	.enable_irq	= amiga_enable_irq,
	.disable_irq	= amiga_disable_irq,

	.data_forward	= amiga_data_forward,
	.data_reverse	= amiga_data_reverse,

	.init_state	= amiga_init_state,
	.save_state	= amiga_save_state,
	.restore_state	= amiga_restore_state,

	.epp_write_data	= parport_ieee1284_epp_write_data,
	.epp_read_data	= parport_ieee1284_epp_read_data,
	.epp_write_addr	= parport_ieee1284_epp_write_addr,
	.epp_read_addr	= parport_ieee1284_epp_read_addr,

	.ecp_write_data	= parport_ieee1284_ecp_write_data,
	.ecp_read_data	= parport_ieee1284_ecp_read_data,
	.ecp_write_addr	= parport_ieee1284_ecp_write_addr,

	.compat_write_data	= parport_ieee1284_write_compat,
	.nibble_read_data	= parport_ieee1284_read_nibble,
	.byte_read_data		= parport_ieee1284_read_byte,

	.owner		= THIS_MODULE,
};


static int __init amiga_parallel_probe(struct platform_device *pdev)
{
	struct parport *p;
	int err;

	ciaa.ddrb = 0xff;
	ciab.ddra &= 0xf8;
	mb();

	p = parport_register_port((unsigned long)&ciaa.prb, IRQ_AMIGA_CIAA_FLG,
				   PARPORT_DMA_NONE, &pp_amiga_ops);
	if (!p)
		return -EBUSY;

	err = request_irq(IRQ_AMIGA_CIAA_FLG, parport_irq_handler, 0, p->name,
			  p);
	if (err)
		goto out_irq;

	printk(KERN_INFO "%s: Amiga built-in port using irq\n", p->name);
	
	parport_announce_port(p);

	platform_set_drvdata(pdev, p);

	return 0;

out_irq:
	parport_put_port(p);
	return err;
}

static int __exit amiga_parallel_remove(struct platform_device *pdev)
{
	struct parport *port = platform_get_drvdata(pdev);

	parport_remove_port(port);
	if (port->irq != PARPORT_IRQ_NONE)
		free_irq(IRQ_AMIGA_CIAA_FLG, port);
	parport_put_port(port);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver amiga_parallel_driver = {
	.remove = __exit_p(amiga_parallel_remove),
	.driver   = {
		.name	= "amiga-parallel",
		.owner	= THIS_MODULE,
	},
};

static int __init amiga_parallel_init(void)
{
	return platform_driver_probe(&amiga_parallel_driver,
				     amiga_parallel_probe);
}

module_init(amiga_parallel_init);

static void __exit amiga_parallel_exit(void)
{
	platform_driver_unregister(&amiga_parallel_driver);
}

module_exit(amiga_parallel_exit);

MODULE_AUTHOR("Joerg Dorchain <joerg@dorchain.net>");
MODULE_DESCRIPTION("Parport Driver for Amiga builtin Port");
MODULE_SUPPORTED_DEVICE("Amiga builtin Parallel Port");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:amiga-parallel");
