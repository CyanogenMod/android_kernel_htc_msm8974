/*
 * Copyright (C) 2001 Dave Engebretsen IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/reboot.h>

#include <asm/machdep.h>
#include <asm/rtas.h>
#include <asm/firmware.h>

#include "pseries.h"

static unsigned char ras_log_buf[RTAS_ERROR_LOG_MAX];
static DEFINE_SPINLOCK(ras_log_buf_lock);

static char global_mce_data_buf[RTAS_ERROR_LOG_MAX];
static DEFINE_PER_CPU(__u64, mce_data_buf);

static int ras_check_exception_token;

#define EPOW_SENSOR_TOKEN	9
#define EPOW_SENSOR_INDEX	0

static irqreturn_t ras_epow_interrupt(int irq, void *dev_id);
static irqreturn_t ras_error_interrupt(int irq, void *dev_id);


static int __init init_ras_IRQ(void)
{
	struct device_node *np;

	ras_check_exception_token = rtas_token("check-exception");

	
	np = of_find_node_by_path("/event-sources/internal-errors");
	if (np != NULL) {
		request_event_sources_irqs(np, ras_error_interrupt,
					   "RAS_ERROR");
		of_node_put(np);
	}

	
	np = of_find_node_by_path("/event-sources/epow-events");
	if (np != NULL) {
		request_event_sources_irqs(np, ras_epow_interrupt, "RAS_EPOW");
		of_node_put(np);
	}

	return 0;
}
subsys_initcall(init_ras_IRQ);

#define EPOW_SHUTDOWN_NORMAL				1
#define EPOW_SHUTDOWN_ON_UPS				2
#define EPOW_SHUTDOWN_LOSS_OF_CRITICAL_FUNCTIONS	3
#define EPOW_SHUTDOWN_AMBIENT_TEMPERATURE_TOO_HIGH	4

static void handle_system_shutdown(char event_modifier)
{
	switch (event_modifier) {
	case EPOW_SHUTDOWN_NORMAL:
		pr_emerg("Firmware initiated power off");
		orderly_poweroff(1);
		break;

	case EPOW_SHUTDOWN_ON_UPS:
		pr_emerg("Loss of power reported by firmware, system is "
			"running on UPS/battery");
		break;

	case EPOW_SHUTDOWN_LOSS_OF_CRITICAL_FUNCTIONS:
		pr_emerg("Loss of system critical functions reported by "
			"firmware");
		pr_emerg("Check RTAS error log for details");
		orderly_poweroff(1);
		break;

	case EPOW_SHUTDOWN_AMBIENT_TEMPERATURE_TOO_HIGH:
		pr_emerg("Ambient temperature too high reported by firmware");
		pr_emerg("Check RTAS error log for details");
		orderly_poweroff(1);
		break;

	default:
		pr_err("Unknown power/cooling shutdown event (modifier %d)",
			event_modifier);
	}
}

struct epow_errorlog {
	unsigned char sensor_value;
	unsigned char event_modifier;
	unsigned char extended_modifier;
	unsigned char reserved;
	unsigned char platform_reason;
};

#define EPOW_RESET			0
#define EPOW_WARN_COOLING		1
#define EPOW_WARN_POWER			2
#define EPOW_SYSTEM_SHUTDOWN		3
#define EPOW_SYSTEM_HALT		4
#define EPOW_MAIN_ENCLOSURE		5
#define EPOW_POWER_OFF			7

void rtas_parse_epow_errlog(struct rtas_error_log *log)
{
	struct pseries_errorlog *pseries_log;
	struct epow_errorlog *epow_log;
	char action_code;
	char modifier;

	pseries_log = get_pseries_errorlog(log, PSERIES_ELOG_SECT_ID_EPOW);
	if (pseries_log == NULL)
		return;

	epow_log = (struct epow_errorlog *)pseries_log->data;
	action_code = epow_log->sensor_value & 0xF;	
	modifier = epow_log->event_modifier & 0xF;	

	switch (action_code) {
	case EPOW_RESET:
		pr_err("Non critical power or cooling issue cleared");
		break;

	case EPOW_WARN_COOLING:
		pr_err("Non critical cooling issue reported by firmware");
		pr_err("Check RTAS error log for details");
		break;

	case EPOW_WARN_POWER:
		pr_err("Non critical power issue reported by firmware");
		pr_err("Check RTAS error log for details");
		break;

	case EPOW_SYSTEM_SHUTDOWN:
		handle_system_shutdown(epow_log->event_modifier);
		break;

	case EPOW_SYSTEM_HALT:
		pr_emerg("Firmware initiated power off");
		orderly_poweroff(1);
		break;

	case EPOW_MAIN_ENCLOSURE:
	case EPOW_POWER_OFF:
		pr_emerg("Critical power/cooling issue reported by firmware");
		pr_emerg("Check RTAS error log for details");
		pr_emerg("Immediate power off");
		emergency_sync();
		kernel_power_off();
		break;

	default:
		pr_err("Unknown power/cooling event (action code %d)",
			action_code);
	}
}

static irqreturn_t ras_epow_interrupt(int irq, void *dev_id)
{
	int status;
	int state;
	int critical;

	status = rtas_get_sensor(EPOW_SENSOR_TOKEN, EPOW_SENSOR_INDEX, &state);

	if (state > 3)
		critical = 1;		
	else
		critical = 0;

	spin_lock(&ras_log_buf_lock);

	status = rtas_call(ras_check_exception_token, 6, 1, NULL,
			   RTAS_VECTOR_EXTERNAL_INTERRUPT,
			   virq_to_hw(irq),
			   RTAS_EPOW_WARNING,
			   critical, __pa(&ras_log_buf),
				rtas_get_error_log_max());

	log_error(ras_log_buf, ERR_TYPE_RTAS_LOG, 0);

	rtas_parse_epow_errlog((struct rtas_error_log *)ras_log_buf);

	spin_unlock(&ras_log_buf_lock);
	return IRQ_HANDLED;
}

static irqreturn_t ras_error_interrupt(int irq, void *dev_id)
{
	struct rtas_error_log *rtas_elog;
	int status;
	int fatal;

	spin_lock(&ras_log_buf_lock);

	status = rtas_call(ras_check_exception_token, 6, 1, NULL,
			   RTAS_VECTOR_EXTERNAL_INTERRUPT,
			   virq_to_hw(irq),
			   RTAS_INTERNAL_ERROR, 1 ,
			   __pa(&ras_log_buf),
				rtas_get_error_log_max());

	rtas_elog = (struct rtas_error_log *)ras_log_buf;

	if ((status == 0) && (rtas_elog->severity >= RTAS_SEVERITY_ERROR_SYNC))
		fatal = 1;
	else
		fatal = 0;

	
	log_error(ras_log_buf, ERR_TYPE_RTAS_LOG, fatal);

	if (fatal) {
		pr_emerg("Fatal hardware error reported by firmware");
		pr_emerg("Check RTAS error log for details");
		pr_emerg("Immediate power off");
		emergency_sync();
		kernel_power_off();
	} else {
		pr_err("Recoverable hardware error reported by firmware");
	}

	spin_unlock(&ras_log_buf_lock);
	return IRQ_HANDLED;
}

#define VALID_FWNMI_BUFFER(A) \
	((((A) >= 0x7000) && ((A) < 0x7ff0)) || \
	(((A) >= rtas.base) && ((A) < (rtas.base + rtas.size - 16))))

static struct rtas_error_log *fwnmi_get_errinfo(struct pt_regs *regs)
{
	unsigned long *savep;
	struct rtas_error_log *h, *errhdr = NULL;

	if (!VALID_FWNMI_BUFFER(regs->gpr[3])) {
		printk(KERN_ERR "FWNMI: corrupt r3 0x%016lx\n", regs->gpr[3]);
		return NULL;
	}

	savep = __va(regs->gpr[3]);
	regs->gpr[3] = savep[0];	

	
	h = (struct rtas_error_log *)&savep[1];
	if (!h->extended) {
		memcpy(&__get_cpu_var(mce_data_buf), h, sizeof(__u64));
		errhdr = (struct rtas_error_log *)&__get_cpu_var(mce_data_buf);
	} else {
		int len;

		len = max_t(int, 8+h->extended_log_length, RTAS_ERROR_LOG_MAX);
		memset(global_mce_data_buf, 0, RTAS_ERROR_LOG_MAX);
		memcpy(global_mce_data_buf, h, len);
		errhdr = (struct rtas_error_log *)global_mce_data_buf;
	}

	return errhdr;
}

static void fwnmi_release_errinfo(void)
{
	int ret = rtas_call(rtas_token("ibm,nmi-interlock"), 0, 1, NULL);
	if (ret != 0)
		printk(KERN_ERR "FWNMI: nmi-interlock failed: %d\n", ret);
}

int pSeries_system_reset_exception(struct pt_regs *regs)
{
	if (fwnmi_active) {
		struct rtas_error_log *errhdr = fwnmi_get_errinfo(regs);
		if (errhdr) {
			
		}
		fwnmi_release_errinfo();
	}
	return 0; 
}

static int recover_mce(struct pt_regs *regs, struct rtas_error_log *err)
{
	int recovered = 0;

	if (!(regs->msr & MSR_RI)) {
		
		recovered = 0;

	} else if (err->disposition == RTAS_DISP_FULLY_RECOVERED) {
		
		recovered = 1;

	} else if (err->disposition == RTAS_DISP_LIMITED_RECOVERY) {
		
		printk(KERN_ERR "MCE: limited recovery, system may "
		       "be degraded\n");
		recovered = 1;

	} else if (user_mode(regs) && !is_global_init(current) &&
		   err->severity == RTAS_SEVERITY_ERROR_SYNC) {

		printk(KERN_ERR "MCE: uncorrectable error, killing task "
		       "%s:%d\n", current->comm, current->pid);

		_exception(SIGBUS, regs, BUS_MCEERR_AR, regs->nip);
		recovered = 1;
	}

	log_error((char *)err, ERR_TYPE_RTAS_LOG, 0);

	return recovered;
}

int pSeries_machine_check_exception(struct pt_regs *regs)
{
	struct rtas_error_log *errp;

	if (fwnmi_active) {
		errp = fwnmi_get_errinfo(regs);
		fwnmi_release_errinfo();
		if (errp && recover_mce(regs, errp))
			return 1;
	}

	return 0;
}
