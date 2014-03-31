#ifndef __PMAC_PFUNC_H__
#define __PMAC_PFUNC_H__

#include <linux/types.h>
#include <linux/list.h>

#define PMF_FLAGS_ON_INIT		0x80000000u
#define PMF_FLGAS_ON_TERM		0x40000000u
#define PMF_FLAGS_ON_SLEEP		0x20000000u
#define PMF_FLAGS_ON_WAKE		0x10000000u
#define PMF_FLAGS_ON_DEMAND		0x08000000u
#define PMF_FLAGS_INT_GEN		0x04000000u
#define PMF_FLAGS_HIGH_SPEED		0x02000000u
#define PMF_FLAGS_LOW_SPEED		0x01000000u
#define PMF_FLAGS_SIDE_EFFECTS		0x00800000u

struct pmf_args {
	union {
		u32 v;
		u32 *p;
	} u[4];
	unsigned int count;
};


#define PMF_STD_ARGS	struct pmf_function *func, void *instdata, \
		        struct pmf_args *args

struct pmf_function;

struct pmf_handlers {
	void * (*begin)(struct pmf_function *func, struct pmf_args *args);
	void (*end)(struct pmf_function *func, void *instdata);

	int (*irq_enable)(struct pmf_function *func);
	int (*irq_disable)(struct pmf_function *func);

	int (*write_gpio)(PMF_STD_ARGS, u8 value, u8 mask);
	int (*read_gpio)(PMF_STD_ARGS, u8 mask, int rshift, u8 xor);

	int (*write_reg32)(PMF_STD_ARGS, u32 offset, u32 value, u32 mask);
	int (*read_reg32)(PMF_STD_ARGS, u32 offset);
	int (*write_reg16)(PMF_STD_ARGS, u32 offset, u16 value, u16 mask);
	int (*read_reg16)(PMF_STD_ARGS, u32 offset);
	int (*write_reg8)(PMF_STD_ARGS, u32 offset, u8 value, u8 mask);
	int (*read_reg8)(PMF_STD_ARGS, u32 offset);

	int (*delay)(PMF_STD_ARGS, u32 duration);

	int (*wait_reg32)(PMF_STD_ARGS, u32 offset, u32 value, u32 mask);
	int (*wait_reg16)(PMF_STD_ARGS, u32 offset, u16 value, u16 mask);
	int (*wait_reg8)(PMF_STD_ARGS, u32 offset, u8 value, u8 mask);

	int (*read_i2c)(PMF_STD_ARGS, u32 len);
	int (*write_i2c)(PMF_STD_ARGS, u32 len, const u8 *data);
	int (*rmw_i2c)(PMF_STD_ARGS, u32 masklen, u32 valuelen, u32 totallen,
		       const u8 *maskdata, const u8 *valuedata);

	int (*read_cfg)(PMF_STD_ARGS, u32 offset, u32 len);
	int (*write_cfg)(PMF_STD_ARGS, u32 offset, u32 len, const u8 *data);
	int (*rmw_cfg)(PMF_STD_ARGS, u32 offset, u32 masklen, u32 valuelen,
		       u32 totallen, const u8 *maskdata, const u8 *valuedata);

	int (*read_i2c_sub)(PMF_STD_ARGS, u8 subaddr, u32 len);
	int (*write_i2c_sub)(PMF_STD_ARGS, u8 subaddr, u32 len, const u8 *data);
	int (*set_i2c_mode)(PMF_STD_ARGS, int mode);
	int (*rmw_i2c_sub)(PMF_STD_ARGS, u8 subaddr, u32 masklen, u32 valuelen,
			   u32 totallen, const u8 *maskdata,
			   const u8 *valuedata);

	int (*read_reg32_msrx)(PMF_STD_ARGS, u32 offset, u32 mask, u32 shift,
			       u32 xor);
	int (*read_reg16_msrx)(PMF_STD_ARGS, u32 offset, u32 mask, u32 shift,
			       u32 xor);
	int (*read_reg8_msrx)(PMF_STD_ARGS, u32 offset, u32 mask, u32 shift,
			      u32 xor);

	int (*write_reg32_slm)(PMF_STD_ARGS, u32 offset, u32 shift, u32 mask);
	int (*write_reg16_slm)(PMF_STD_ARGS, u32 offset, u32 shift, u32 mask);
	int (*write_reg8_slm)(PMF_STD_ARGS, u32 offset, u32 shift, u32 mask);

	int (*mask_and_compare)(PMF_STD_ARGS, u32 len, const u8 *maskdata,
				const u8 *valuedata);

	struct module *owner;
};


struct pmf_device;

struct pmf_function {
	
	struct list_head	link;

	
	struct device_node	*node;
	void			*driver_data;

	
	struct pmf_device	*dev;

	const char		*name;
	u32			phandle;

	u32			flags;

	
	const void		*data;
	unsigned int		length;

	
	struct list_head	irq_clients;

	
	struct kref		ref;
};

struct pmf_irq_client {
	void			(*handler)(void *data);
	void			*data;
	struct module		*owner;
	struct list_head	link;
	struct pmf_function	*func;
};


extern int pmf_register_driver(struct device_node *np,
			      struct pmf_handlers *handlers,
			      void *driverdata);

extern void pmf_unregister_driver(struct device_node *np);


extern int pmf_register_irq_client(struct device_node *np,
				   const char *name,
				   struct pmf_irq_client *client);

extern void pmf_unregister_irq_client(struct pmf_irq_client *client);

extern void pmf_do_irq(struct pmf_function *func);


extern int pmf_do_functions(struct device_node *np, const char *name,
			    u32 phandle, u32 flags, struct pmf_args *args);




extern int pmf_call_function(struct device_node *target, const char *name,
			     struct pmf_args *args);



extern struct pmf_function *pmf_find_function(struct device_node *target,
					      const char *name);

extern struct pmf_function * pmf_get_function(struct pmf_function *func);
extern void pmf_put_function(struct pmf_function *func);

extern int pmf_call_one(struct pmf_function *func, struct pmf_args *args);


extern void pmac_pfunc_base_suspend(void);
extern void pmac_pfunc_base_resume(void);

#endif 
