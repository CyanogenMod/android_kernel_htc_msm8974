/*
 * dvb-math provides some complex fixed-point math
 * operations shared between the dvb related stuff
 *
 * Copyright (C) 2006 Christoph Pfister (christophpfister@gmail.com)
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __DVB_MATH_H
#define __DVB_MATH_H

#include <linux/types.h>

extern unsigned int intlog2(u32 value);

extern unsigned int intlog10(u32 value);

#endif
