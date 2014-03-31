/*   Copyright (C) 1995-99 Simon G. Vogl

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.							     */


#ifndef _LINUX_I2C_ALGO_BIT_H
#define _LINUX_I2C_ALGO_BIT_H

struct i2c_algo_bit_data {
	void *data;		
	void (*setsda) (void *data, int state);
	void (*setscl) (void *data, int state);
	int  (*getsda) (void *data);
	int  (*getscl) (void *data);
	int  (*pre_xfer)  (struct i2c_adapter *);
	void (*post_xfer) (struct i2c_adapter *);

	
	int udelay;		
	int timeout;		
};

int i2c_bit_add_bus(struct i2c_adapter *);
int i2c_bit_add_numbered_bus(struct i2c_adapter *);
extern const struct i2c_algorithm i2c_bit_algo;

#endif 
