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


#ifndef __CVMX_HELPER_UTIL_H__
#define __CVMX_HELPER_UTIL_H__

extern const char
    *cvmx_helper_interface_mode_to_string(cvmx_helper_interface_mode_t mode);

extern int cvmx_helper_dump_packet(cvmx_wqe_t *work);

extern int cvmx_helper_setup_red_queue(int queue, int pass_thresh,
				       int drop_thresh);

extern int cvmx_helper_setup_red(int pass_thresh, int drop_thresh);

extern const char *cvmx_helper_get_version(void);

extern int __cvmx_helper_setup_gmx(int interface, int num_ports);

extern int cvmx_helper_get_ipd_port(int interface, int port);

static inline int cvmx_helper_get_first_ipd_port(int interface)
{
	return cvmx_helper_get_ipd_port(interface, 0);
}

static inline int cvmx_helper_get_last_ipd_port(int interface)
{
	extern int cvmx_helper_ports_on_interface(int interface);

	return cvmx_helper_get_first_ipd_port(interface) +
	       cvmx_helper_ports_on_interface(interface) - 1;
}

static inline void cvmx_helper_free_packet_data(cvmx_wqe_t *work)
{
	uint64_t number_buffers;
	union cvmx_buf_ptr buffer_ptr;
	union cvmx_buf_ptr next_buffer_ptr;
	uint64_t start_of_buffer;

	number_buffers = work->word2.s.bufs;
	if (number_buffers == 0)
		return;
	buffer_ptr = work->packet_ptr;

	start_of_buffer = ((buffer_ptr.s.addr >> 7) - buffer_ptr.s.back) << 7;
	if (cvmx_ptr_to_phys(work) == start_of_buffer) {
		next_buffer_ptr =
		    *(union cvmx_buf_ptr *) cvmx_phys_to_ptr(buffer_ptr.s.addr - 8);
		buffer_ptr = next_buffer_ptr;
		number_buffers--;
	}

	while (number_buffers--) {
		start_of_buffer =
		    ((buffer_ptr.s.addr >> 7) - buffer_ptr.s.back) << 7;
		next_buffer_ptr =
		    *(union cvmx_buf_ptr *) cvmx_phys_to_ptr(buffer_ptr.s.addr - 8);
		cvmx_fpa_free(cvmx_phys_to_ptr(start_of_buffer),
			      buffer_ptr.s.pool, 0);
		buffer_ptr = next_buffer_ptr;
	}
}

extern int cvmx_helper_get_interface_num(int ipd_port);

extern int cvmx_helper_get_interface_index_num(int ipd_port);

#endif 
