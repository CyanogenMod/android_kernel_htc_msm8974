/*****************************************************************************
 *
 * Copyright (C) 2008 Cedric Bregardis <cedric.bregardis@free.fr> and
 * Jean-Christian Hassler <jhassler@free.fr>
 * Copyright 1998 Emagic Soft- und Hardware GmbH
 * Copyright 2002 Martijn Sipkema
 *
 * This file is part of the Audiowerk2 ALSA driver
 *
 * The Audiowerk2 ALSA driver is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * The Audiowerk2 ALSA driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Audiowerk2 ALSA driver; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 *****************************************************************************/

#define TSL_WS0		(1UL << 31)
#define	TSL_WS1		(1UL << 30)
#define	TSL_WS2		(1UL << 29)
#define TSL_WS3		(1UL << 28)
#define TSL_WS4		(1UL << 27)
#define	TSL_DIS_A1	(1UL << 24)
#define TSL_SDW_A1	(1UL << 23)
#define TSL_SIB_A1	(1UL << 22)
#define TSL_SF_A1	(1UL << 21)
#define	TSL_LF_A1	(1UL << 20)
#define TSL_BSEL_A1	(1UL << 17)
#define TSL_DOD_A1	(1UL << 15)
#define TSL_LOW_A1	(1UL << 14)
#define TSL_DIS_A2	(1UL << 11)
#define TSL_SDW_A2	(1UL << 10)
#define TSL_SIB_A2	(1UL << 9)
#define TSL_SF_A2	(1UL << 8)
#define TSL_LF_A2	(1UL << 7)
#define TSL_BSEL_A2	(1UL << 4)
#define TSL_DOD_A2	(1UL << 2)
#define TSL_LOW_A2	(1UL << 1)
#define TSL_EOS		(1UL << 0)

    
    
    
    
    
    

    
    

    
    
    

    
    
    
    
    

    
    
    
    
    

static int tsl1[8] = {
	1 * TSL_SDW_A1 | 3 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1 | TSL_LF_A1,

	1 * TSL_SDW_A1 | 2 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1,

	0 * TSL_SDW_A1 | 3 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1,

	0 * TSL_SDW_A1 | 2 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1,

	1 * TSL_SDW_A1 | 1 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1 | TSL_WS1 | TSL_WS0,

	1 * TSL_SDW_A1 | 0 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1 | TSL_WS1 | TSL_WS0,

	0 * TSL_SDW_A1 | 1 * TSL_BSEL_A1 |
	0 * TSL_DIS_A1 | 0 * TSL_DOD_A1 | TSL_WS1 | TSL_WS0,

	0 * TSL_SDW_A1 | 0 * TSL_BSEL_A1 | 0 * TSL_DIS_A1 |
	0 * TSL_DOD_A1 | TSL_WS1 | TSL_WS0 | TSL_SF_A1 | TSL_EOS,
};

static int tsl2[8] = {
	0 * TSL_SDW_A2 | 3 * TSL_BSEL_A2 | 2 * TSL_DOD_A2 | TSL_LF_A2,
	0 * TSL_SDW_A2 | 2 * TSL_BSEL_A2 | 2 * TSL_DOD_A2,
	0 * TSL_SDW_A2 | 3 * TSL_BSEL_A2 | 2 * TSL_DOD_A2,
	0 * TSL_SDW_A2 | 2 * TSL_BSEL_A2 | 2 * TSL_DOD_A2,
	0 * TSL_SDW_A2 | 1 * TSL_BSEL_A2 | 2 * TSL_DOD_A2 | TSL_WS2,
	0 * TSL_SDW_A2 | 0 * TSL_BSEL_A2 | 2 * TSL_DOD_A2 | TSL_WS2,
	0 * TSL_SDW_A2 | 1 * TSL_BSEL_A2 | 2 * TSL_DOD_A2 | TSL_WS2,
	0 * TSL_SDW_A2 | 0 * TSL_BSEL_A2 | 2 * TSL_DOD_A2 | TSL_WS2 | TSL_EOS
};
