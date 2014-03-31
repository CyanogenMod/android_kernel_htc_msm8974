/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/


#include <asm/octeon/octeon.h>

#include <asm/octeon/cvmx-config.h>
#include <asm/octeon/cvmx-pko.h>
#include <asm/octeon/cvmx-helper.h>



void cvmx_pko_initialize_global(void)
{
	int i;
	uint64_t priority = 8;
	union cvmx_pko_reg_cmd_buf config;

	config.u64 = 0;
	config.s.pool = CVMX_FPA_OUTPUT_BUFFER_POOL;
	config.s.size = CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE / 8 - 1;

	cvmx_write_csr(CVMX_PKO_REG_CMD_BUF, config.u64);

	for (i = 0; i < CVMX_PKO_MAX_OUTPUT_QUEUES; i++)
		cvmx_pko_config_port(CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID, i, 1,
				     &priority);

	if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)
	    || OCTEON_IS_MODEL(OCTEON_CN56XX)
	    || OCTEON_IS_MODEL(OCTEON_CN52XX)) {
		int num_interfaces = cvmx_helper_get_number_of_interfaces();
		int last_port =
		    cvmx_helper_get_last_ipd_port(num_interfaces - 1);
		int max_queues =
		    cvmx_pko_get_base_queue(last_port) +
		    cvmx_pko_get_num_queues(last_port);
		if (OCTEON_IS_MODEL(OCTEON_CN38XX)) {
			if (max_queues <= 32)
				cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 2);
			else if (max_queues <= 64)
				cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 1);
		} else {
			if (max_queues <= 64)
				cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 2);
			else if (max_queues <= 128)
				cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 1);
		}
	}
}

int cvmx_pko_initialize_local(void)
{
	
	return 0;
}

void cvmx_pko_enable(void)
{
	union cvmx_pko_reg_flags flags;

	flags.u64 = cvmx_read_csr(CVMX_PKO_REG_FLAGS);
	if (flags.s.ena_pko)
		cvmx_dprintf
		    ("Warning: Enabling PKO when PKO already enabled.\n");

	flags.s.ena_dwb = 1;
	flags.s.ena_pko = 1;
	flags.s.store_be = 1;
	cvmx_write_csr(CVMX_PKO_REG_FLAGS, flags.u64);
}

void cvmx_pko_disable(void)
{
	union cvmx_pko_reg_flags pko_reg_flags;
	pko_reg_flags.u64 = cvmx_read_csr(CVMX_PKO_REG_FLAGS);
	pko_reg_flags.s.ena_pko = 0;
	cvmx_write_csr(CVMX_PKO_REG_FLAGS, pko_reg_flags.u64);
}


static void __cvmx_pko_reset(void)
{
	union cvmx_pko_reg_flags pko_reg_flags;
	pko_reg_flags.u64 = cvmx_read_csr(CVMX_PKO_REG_FLAGS);
	pko_reg_flags.s.reset = 1;
	cvmx_write_csr(CVMX_PKO_REG_FLAGS, pko_reg_flags.u64);
}

void cvmx_pko_shutdown(void)
{
	union cvmx_pko_mem_queue_ptrs config;
	int queue;

	cvmx_pko_disable();

	for (queue = 0; queue < CVMX_PKO_MAX_OUTPUT_QUEUES; queue++) {
		config.u64 = 0;
		config.s.tail = 1;
		config.s.index = 0;
		config.s.port = CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID;
		config.s.queue = queue & 0x7f;
		config.s.qos_mask = 0;
		config.s.buf_ptr = 0;
		if (!OCTEON_IS_MODEL(OCTEON_CN3XXX)) {
			union cvmx_pko_reg_queue_ptrs1 config1;
			config1.u64 = 0;
			config1.s.qid7 = queue >> 7;
			cvmx_write_csr(CVMX_PKO_REG_QUEUE_PTRS1, config1.u64);
		}
		cvmx_write_csr(CVMX_PKO_MEM_QUEUE_PTRS, config.u64);
		cvmx_cmd_queue_shutdown(CVMX_CMD_QUEUE_PKO(queue));
	}
	__cvmx_pko_reset();
}

cvmx_pko_status_t cvmx_pko_config_port(uint64_t port, uint64_t base_queue,
				       uint64_t num_queues,
				       const uint64_t priority[])
{
	cvmx_pko_status_t result_code;
	uint64_t queue;
	union cvmx_pko_mem_queue_ptrs config;
	union cvmx_pko_reg_queue_ptrs1 config1;
	int static_priority_base = -1;
	int static_priority_end = -1;

	if ((port >= CVMX_PKO_NUM_OUTPUT_PORTS)
	    && (port != CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID)) {
		cvmx_dprintf("ERROR: cvmx_pko_config_port: Invalid port %llu\n",
			     (unsigned long long)port);
		return CVMX_PKO_INVALID_PORT;
	}

	if (base_queue + num_queues > CVMX_PKO_MAX_OUTPUT_QUEUES) {
		cvmx_dprintf
		    ("ERROR: cvmx_pko_config_port: Invalid queue range %llu\n",
		     (unsigned long long)(base_queue + num_queues));
		return CVMX_PKO_INVALID_QUEUE;
	}

	if (port != CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID) {
		for (queue = 0; queue < num_queues; queue++) {
			
			if (static_priority_base == -1
			    && priority[queue] ==
			    CVMX_PKO_QUEUE_STATIC_PRIORITY)
				static_priority_base = queue;
			
			if (static_priority_base != -1
			    && static_priority_end == -1
			    && priority[queue] != CVMX_PKO_QUEUE_STATIC_PRIORITY
			    && queue)
				static_priority_end = queue - 1;
			else if (static_priority_base != -1
				 && static_priority_end == -1
				 && queue == num_queues - 1)
				
				static_priority_end = queue;
			if (static_priority_end != -1
			    && (int)queue > static_priority_end
			    && priority[queue] ==
			    CVMX_PKO_QUEUE_STATIC_PRIORITY) {
				cvmx_dprintf("ERROR: cvmx_pko_config_port: "
					     "Static priority queues aren't "
					     "contiguous or don't start at "
					     "base queue. q: %d, eq: %d\n",
					(int)queue, static_priority_end);
				return CVMX_PKO_INVALID_PRIORITY;
			}
		}
		if (static_priority_base > 0) {
			cvmx_dprintf("ERROR: cvmx_pko_config_port: Static "
				     "priority queues don't start at base "
				     "queue. sq: %d\n",
				static_priority_base);
			return CVMX_PKO_INVALID_PRIORITY;
		}
#if 0
		cvmx_dprintf("Port %d: Static priority queue base: %d, "
			     "end: %d\n", port,
			static_priority_base, static_priority_end);
#endif
	}

	result_code = CVMX_PKO_SUCCESS;

#ifdef PKO_DEBUG
	cvmx_dprintf("num queues: %d (%lld,%lld)\n", num_queues,
		     CVMX_PKO_QUEUES_PER_PORT_INTERFACE0,
		     CVMX_PKO_QUEUES_PER_PORT_INTERFACE1);
#endif

	for (queue = 0; queue < num_queues; queue++) {
		uint64_t *buf_ptr = NULL;

		config1.u64 = 0;
		config1.s.idx3 = queue >> 3;
		config1.s.qid7 = (base_queue + queue) >> 7;

		config.u64 = 0;
		config.s.tail = queue == (num_queues - 1);
		config.s.index = queue;
		config.s.port = port;
		config.s.queue = base_queue + queue;

		if (!cvmx_octeon_is_pass1()) {
			config.s.static_p = static_priority_base >= 0;
			config.s.static_q = (int)queue <= static_priority_end;
			config.s.s_tail = (int)queue == static_priority_end;
		}
		switch ((int)priority[queue]) {
		case 0:
			config.s.qos_mask = 0x00;
			break;
		case 1:
			config.s.qos_mask = 0x01;
			break;
		case 2:
			config.s.qos_mask = 0x11;
			break;
		case 3:
			config.s.qos_mask = 0x49;
			break;
		case 4:
			config.s.qos_mask = 0x55;
			break;
		case 5:
			config.s.qos_mask = 0x57;
			break;
		case 6:
			config.s.qos_mask = 0x77;
			break;
		case 7:
			config.s.qos_mask = 0x7f;
			break;
		case 8:
			config.s.qos_mask = 0xff;
			break;
		case CVMX_PKO_QUEUE_STATIC_PRIORITY:
			
			if (!cvmx_octeon_is_pass1()) {
				config.s.qos_mask = 0xff;
				break;
			}
		default:
			cvmx_dprintf("ERROR: cvmx_pko_config_port: Invalid "
				     "priority %llu\n",
				(unsigned long long)priority[queue]);
			config.s.qos_mask = 0xff;
			result_code = CVMX_PKO_INVALID_PRIORITY;
			break;
		}

		if (port != CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID) {
			cvmx_cmd_queue_result_t cmd_res =
			    cvmx_cmd_queue_initialize(CVMX_CMD_QUEUE_PKO
						      (base_queue + queue),
						      CVMX_PKO_MAX_QUEUE_DEPTH,
						      CVMX_FPA_OUTPUT_BUFFER_POOL,
						      CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE
						      -
						      CVMX_PKO_COMMAND_BUFFER_SIZE_ADJUST
						      * 8);
			if (cmd_res != CVMX_CMD_QUEUE_SUCCESS) {
				switch (cmd_res) {
				case CVMX_CMD_QUEUE_NO_MEMORY:
					cvmx_dprintf("ERROR: "
						     "cvmx_pko_config_port: "
						     "Unable to allocate "
						     "output buffer.\n");
					return CVMX_PKO_NO_MEMORY;
				case CVMX_CMD_QUEUE_ALREADY_SETUP:
					cvmx_dprintf
					    ("ERROR: cvmx_pko_config_port: Port already setup.\n");
					return CVMX_PKO_PORT_ALREADY_SETUP;
				case CVMX_CMD_QUEUE_INVALID_PARAM:
				default:
					cvmx_dprintf
					    ("ERROR: cvmx_pko_config_port: Command queue initialization failed.\n");
					return CVMX_PKO_CMD_QUEUE_INIT_ERROR;
				}
			}

			buf_ptr =
			    (uint64_t *)
			    cvmx_cmd_queue_buffer(CVMX_CMD_QUEUE_PKO
						  (base_queue + queue));
			config.s.buf_ptr = cvmx_ptr_to_phys(buf_ptr);
		} else
			config.s.buf_ptr = 0;

		CVMX_SYNCWS;

		if (!OCTEON_IS_MODEL(OCTEON_CN3XXX))
			cvmx_write_csr(CVMX_PKO_REG_QUEUE_PTRS1, config1.u64);
		cvmx_write_csr(CVMX_PKO_MEM_QUEUE_PTRS, config.u64);
	}

	return result_code;
}

#ifdef PKO_DEBUG
void cvmx_pko_show_queue_map()
{
	int core, port;
	int pko_output_ports = 36;

	cvmx_dprintf("port");
	for (port = 0; port < pko_output_ports; port++)
		cvmx_dprintf("%3d ", port);
	cvmx_dprintf("\n");

	for (core = 0; core < CVMX_MAX_CORES; core++) {
		cvmx_dprintf("\n%2d: ", core);
		for (port = 0; port < pko_output_ports; port++) {
			cvmx_dprintf("%3d ",
				     cvmx_pko_get_base_queue_per_core(port,
								      core));
		}
	}
	cvmx_dprintf("\n");
}
#endif

int cvmx_pko_rate_limit_packets(int port, int packets_s, int burst)
{
	union cvmx_pko_mem_port_rate0 pko_mem_port_rate0;
	union cvmx_pko_mem_port_rate1 pko_mem_port_rate1;

	pko_mem_port_rate0.u64 = 0;
	pko_mem_port_rate0.s.pid = port;
	pko_mem_port_rate0.s.rate_pkt =
	    cvmx_sysinfo_get()->cpu_clock_hz / packets_s / 16;
	
	pko_mem_port_rate0.s.rate_word = 0;

	pko_mem_port_rate1.u64 = 0;
	pko_mem_port_rate1.s.pid = port;
	pko_mem_port_rate1.s.rate_lim =
	    ((uint64_t) pko_mem_port_rate0.s.rate_pkt * burst) >> 8;

	cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE0, pko_mem_port_rate0.u64);
	cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE1, pko_mem_port_rate1.u64);
	return 0;
}

int cvmx_pko_rate_limit_bits(int port, uint64_t bits_s, int burst)
{
	union cvmx_pko_mem_port_rate0 pko_mem_port_rate0;
	union cvmx_pko_mem_port_rate1 pko_mem_port_rate1;
	uint64_t clock_rate = cvmx_sysinfo_get()->cpu_clock_hz;
	uint64_t tokens_per_bit = clock_rate * 16 / bits_s;

	pko_mem_port_rate0.u64 = 0;
	pko_mem_port_rate0.s.pid = port;
	pko_mem_port_rate0.s.rate_pkt = (12 + 8 + 4) * 8 * tokens_per_bit / 256;
	
	pko_mem_port_rate0.s.rate_word = 64 * tokens_per_bit;

	pko_mem_port_rate1.u64 = 0;
	pko_mem_port_rate1.s.pid = port;
	pko_mem_port_rate1.s.rate_lim = tokens_per_bit * burst / 256;

	cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE0, pko_mem_port_rate0.u64);
	cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE1, pko_mem_port_rate1.u64);
	return 0;
}
