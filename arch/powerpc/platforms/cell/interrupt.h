#ifndef ASM_CELL_PIC_H
#define ASM_CELL_PIC_H
#ifdef __KERNEL__

enum {
	IIC_IRQ_INVALID		= 0x80000000u,
	IIC_IRQ_NODE_MASK	= 0x100,
	IIC_IRQ_NODE_SHIFT	= 8,
	IIC_IRQ_MAX		= 0x1ff,
	IIC_IRQ_TYPE_MASK	= 0xc0,
	IIC_IRQ_TYPE_NORMAL	= 0x00,
	IIC_IRQ_TYPE_IOEXC	= 0x40,
	IIC_IRQ_TYPE_IPI	= 0x80,
	IIC_IRQ_CLASS_SHIFT	= 4,
	IIC_IRQ_CLASS_0		= 0x00,
	IIC_IRQ_CLASS_1		= 0x10,
	IIC_IRQ_CLASS_2		= 0x20,
	IIC_SOURCE_COUNT	= 0x200,

	IIC_UNIT_SPU_0		= 0x4,
	IIC_UNIT_SPU_1		= 0x7,
	IIC_UNIT_SPU_2		= 0x3,
	IIC_UNIT_SPU_3		= 0x8,
	IIC_UNIT_SPU_4		= 0x2,
	IIC_UNIT_SPU_5		= 0x9,
	IIC_UNIT_SPU_6		= 0x1,
	IIC_UNIT_SPU_7		= 0xa,
	IIC_UNIT_IOC_0		= 0x0,
	IIC_UNIT_IOC_1		= 0xb,
	IIC_UNIT_THREAD_0	= 0xe, 
	IIC_UNIT_THREAD_1	= 0xf, 
	IIC_UNIT_IIC		= 0xe, 

	
	IIC_IRQ_EXT_IOIF0	=
		IIC_IRQ_TYPE_NORMAL | IIC_IRQ_CLASS_2 | IIC_UNIT_IOC_0,
	IIC_IRQ_EXT_IOIF1	=
		IIC_IRQ_TYPE_NORMAL | IIC_IRQ_CLASS_2 | IIC_UNIT_IOC_1,

	
	IIC_IRQ_IOEX_TMI	= IIC_IRQ_TYPE_IOEXC | IIC_IRQ_CLASS_1 | 63,
	IIC_IRQ_IOEX_PMI	= IIC_IRQ_TYPE_IOEXC | IIC_IRQ_CLASS_1 | 62,
	IIC_IRQ_IOEX_ATI	= IIC_IRQ_TYPE_IOEXC | IIC_IRQ_CLASS_1 | 61,
	IIC_IRQ_IOEX_MATBFI	= IIC_IRQ_TYPE_IOEXC | IIC_IRQ_CLASS_1 | 60,
	IIC_IRQ_IOEX_ELDI	= IIC_IRQ_TYPE_IOEXC | IIC_IRQ_CLASS_1 | 59,

	
	IIC_ISR_EDGE_MASK	= 0x4ul,
};

extern void iic_init_IRQ(void);
extern void iic_message_pass(int cpu, int msg);
extern void iic_request_IPIs(void);
extern void iic_setup_cpu(void);

extern u8 iic_get_target_id(int cpu);

extern void spider_init_IRQ(void);

extern void iic_set_interrupt_routing(int cpu, int thread, int priority);

#endif
#endif 
