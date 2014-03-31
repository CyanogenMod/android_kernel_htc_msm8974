/*
 *
 *  Copyright (C) 2007 Christian Pellegrin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef _LINUX_SERIAL_MAX3100_H
#define _LINUX_SERIAL_MAX3100_H 1


struct plat_max3100 {
	int loopback;
	int crystal;
	void (*max3100_hw_suspend) (int suspend);
	int poll_time;
};

#endif
