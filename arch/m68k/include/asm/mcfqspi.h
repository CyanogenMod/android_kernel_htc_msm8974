/*
 * Definitions for Freescale Coldfire QSPI module
 *
 * Copyright 2010 Steven King <sfking@fdwdc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
*/

#ifndef mcfqspi_h
#define mcfqspi_h

struct mcfqspi_cs_control {
	int 	(*setup)(struct mcfqspi_cs_control *);
	void	(*teardown)(struct mcfqspi_cs_control *);
	void	(*select)(struct mcfqspi_cs_control *, u8, bool);
	void	(*deselect)(struct mcfqspi_cs_control *, u8, bool);
};

struct mcfqspi_platform_data {
	s16	bus_num;
	u16	num_chipselect;
	struct mcfqspi_cs_control *cs_control;
};

#endif 
