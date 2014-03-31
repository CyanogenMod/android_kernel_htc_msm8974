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

#ifndef __CVMX_SPI_H__
#define __CVMX_SPI_H__

#include "cvmx-gmxx-defs.h"


typedef enum {
	CVMX_SPI_MODE_UNKNOWN = 0,
	CVMX_SPI_MODE_TX_HALFPLEX = 1,
	CVMX_SPI_MODE_RX_HALFPLEX = 2,
	CVMX_SPI_MODE_DUPLEX = 3
} cvmx_spi_mode_t;

typedef struct {
    
	int (*reset_cb) (int interface, cvmx_spi_mode_t mode);

    
	int (*calendar_setup_cb) (int interface, cvmx_spi_mode_t mode,
				  int num_ports);

    
	int (*clock_detect_cb) (int interface, cvmx_spi_mode_t mode,
				int timeout);

    
	int (*training_cb) (int interface, cvmx_spi_mode_t mode, int timeout);

    
	int (*calendar_sync_cb) (int interface, cvmx_spi_mode_t mode,
				 int timeout);

    
	int (*interface_up_cb) (int interface, cvmx_spi_mode_t mode);

} cvmx_spi_callbacks_t;

static inline int cvmx_spi_is_spi_interface(int interface)
{
	uint64_t gmxState = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));
	return (gmxState & 0x2) && (gmxState & 0x1);
}

extern int cvmx_spi_start_interface(int interface, cvmx_spi_mode_t mode,
				    int timeout, int num_ports);

extern int cvmx_spi_restart_interface(int interface, cvmx_spi_mode_t mode,
				      int timeout);

static inline int cvmx_spi4000_is_present(int interface)
{
	return 0;
}

static inline int cvmx_spi4000_initialize(int interface)
{
	return 0;
}

static inline union cvmx_gmxx_rxx_rx_inbnd cvmx_spi4000_check_speed(
	int interface,
	int port)
{
	union cvmx_gmxx_rxx_rx_inbnd r;
	r.u64 = 0;
	return r;
}

extern void cvmx_spi_get_callbacks(cvmx_spi_callbacks_t *callbacks);

extern void cvmx_spi_set_callbacks(cvmx_spi_callbacks_t *new_callbacks);

extern int cvmx_spi_reset_cb(int interface, cvmx_spi_mode_t mode);

extern int cvmx_spi_calendar_setup_cb(int interface, cvmx_spi_mode_t mode,
				      int num_ports);

extern int cvmx_spi_clock_detect_cb(int interface, cvmx_spi_mode_t mode,
				    int timeout);

extern int cvmx_spi_training_cb(int interface, cvmx_spi_mode_t mode,
				int timeout);

extern int cvmx_spi_calendar_sync_cb(int interface, cvmx_spi_mode_t mode,
				     int timeout);

extern int cvmx_spi_interface_up_cb(int interface, cvmx_spi_mode_t mode);

#endif 
