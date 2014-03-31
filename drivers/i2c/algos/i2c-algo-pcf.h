/*   Copyright (C) 1996 Simon G. Vogl
                   1999 Hans Berglund

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
    MA 02110-1301 USA.							*/


#ifndef I2C_PCF8584_H
#define I2C_PCF8584_H 1

#define I2C_PCF_PIN	0x80
#define I2C_PCF_ESO	0x40
#define I2C_PCF_ES1	0x20
#define I2C_PCF_ES2	0x10
#define I2C_PCF_ENI	0x08
#define I2C_PCF_STA	0x04
#define I2C_PCF_STO	0x02
#define I2C_PCF_ACK	0x01

#define I2C_PCF_START    (I2C_PCF_PIN | I2C_PCF_ESO | I2C_PCF_STA | I2C_PCF_ACK)
#define I2C_PCF_STOP     (I2C_PCF_PIN | I2C_PCF_ESO | I2C_PCF_STO | I2C_PCF_ACK)
#define I2C_PCF_REPSTART (              I2C_PCF_ESO | I2C_PCF_STA | I2C_PCF_ACK)
#define I2C_PCF_IDLE     (I2C_PCF_PIN | I2C_PCF_ESO               | I2C_PCF_ACK)


#define I2C_PCF_INI 0x40   
#define I2C_PCF_STS 0x20
#define I2C_PCF_BER 0x10
#define I2C_PCF_AD0 0x08
#define I2C_PCF_LRB 0x08
#define I2C_PCF_AAS 0x04
#define I2C_PCF_LAB 0x02
#define I2C_PCF_BB  0x01

#define I2C_PCF_CLK3	0x00
#define I2C_PCF_CLK443	0x10
#define I2C_PCF_CLK6	0x14
#define I2C_PCF_CLK	0x18
#define I2C_PCF_CLK12	0x1c

#define I2C_PCF_TRNS90 0x00	
#define I2C_PCF_TRNS45 0x01	
#define I2C_PCF_TRNS11 0x02	
#define I2C_PCF_TRNS15 0x03	



#define I2C_PCF_OWNADR	0
#define I2C_PCF_INTREG	I2C_PCF_ES2
#define I2C_PCF_CLKREG	I2C_PCF_ES1

#endif 
