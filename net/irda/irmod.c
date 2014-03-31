/*********************************************************************
 *
 * Filename:      irmod.c
 * Version:       0.9
 * Description:   IrDA stack main entry points
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Dec 15 13:55:39 1997
 * Modified at:   Wed Jan  5 15:12:41 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1997, 1999-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2004 Jean Tourrilhes <jt@hpl.hp.com>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/


#include <linux/module.h>
#include <linux/moduleparam.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>		
#include <net/irda/irlap.h>		
#include <net/irda/irlmp.h>		
#include <net/irda/iriap.h>		
#include <net/irda/irttp.h>		
#include <net/irda/irda_device.h>	

#ifdef CONFIG_IRDA_DEBUG
unsigned int irda_debug = IRDA_DEBUG_LEVEL;
module_param_named(debug, irda_debug, uint, 0);
MODULE_PARM_DESC(debug, "IRDA debugging level");
EXPORT_SYMBOL(irda_debug);
#endif

static struct packet_type irda_packet_type __read_mostly = {
	.type	= cpu_to_be16(ETH_P_IRDA),
	.func	= irlap_driver_rcv,	
};

void irda_notify_init(notify_t *notify)
{
	notify->data_indication = NULL;
	notify->udata_indication = NULL;
	notify->connect_confirm = NULL;
	notify->connect_indication = NULL;
	notify->disconnect_indication = NULL;
	notify->flow_indication = NULL;
	notify->status_indication = NULL;
	notify->instance = NULL;
	strlcpy(notify->name, "Unknown", sizeof(notify->name));
}
EXPORT_SYMBOL(irda_notify_init);

static int __init irda_init(void)
{
	int ret = 0;

	IRDA_DEBUG(0, "%s()\n", __func__);

	
	irlmp_init();
	irlap_init();

	
	irda_device_init();

	
	iriap_init();
	irttp_init();
	ret = irsock_init();
	if (ret < 0)
		goto out_err_1;

	
	dev_add_pack(&irda_packet_type);

	
#ifdef CONFIG_PROC_FS
	irda_proc_register();
#endif
#ifdef CONFIG_SYSCTL
	ret = irda_sysctl_register();
	if (ret < 0)
		goto out_err_2;
#endif

	ret = irda_nl_register();
	if (ret < 0)
		goto out_err_3;

	return 0;

 out_err_3:
#ifdef CONFIG_SYSCTL
	irda_sysctl_unregister();
 out_err_2:
#endif
#ifdef CONFIG_PROC_FS
	irda_proc_unregister();
#endif

	
	dev_remove_pack(&irda_packet_type);

	
	irsock_cleanup();
 out_err_1:
	irttp_cleanup();
	iriap_cleanup();

	
	irda_device_cleanup();
	irlap_cleanup(); 

	
	irlmp_cleanup();


	return ret;
}

static void __exit irda_cleanup(void)
{
	
	irda_nl_unregister();

#ifdef CONFIG_SYSCTL
	irda_sysctl_unregister();
#endif
#ifdef CONFIG_PROC_FS
	irda_proc_unregister();
#endif

	
	dev_remove_pack(&irda_packet_type);

	
	irsock_cleanup();
	irttp_cleanup();
	iriap_cleanup();

	
	irda_device_cleanup();
	irlap_cleanup(); 

	
	irlmp_cleanup();
}

subsys_initcall(irda_init);
module_exit(irda_cleanup);

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no> & Jean Tourrilhes <jt@hpl.hp.com>");
MODULE_DESCRIPTION("The Linux IrDA Protocol Stack");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NETPROTO(PF_IRDA);
