/*
 *
 * pca954x.h - I2C multiplexer/switch support
 *
 * Copyright (c) 2008-2009 Rodolfo Giometti <giometti@linux.it>
 * Copyright (c) 2008-2009 Eurotech S.p.A. <info@eurotech.it>
 * Michael Lawnick <michael.lawnick.ext@nsn.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef _LINUX_I2C_PCA954X_H
#define _LINUX_I2C_PCA954X_H


struct pca954x_platform_mode {
	int		adap_id;
	unsigned int	deselect_on_exit:1;
};

struct pca954x_platform_data {
	struct pca954x_platform_mode *modes;
	int num_modes;
};

#endif 
