
/*
 *	stallion.h  -- stallion multiport serial driver.
 *
 *	Copyright (C) 1996-1998  Stallion Technologies
 *	Copyright (C) 1994-1996  Greg Ungerer.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	_STALLION_H
#define	_STALLION_H

#define	STL_MAXBRDS		4
#define	STL_MAXPANELS		4
#define	STL_MAXBANKS		8
#define	STL_PORTSPERPANEL	16
#define	STL_MAXPORTS		64
#define	STL_MAXDEVS		(STL_MAXBRDS * STL_MAXPORTS)



struct stlrq {
	char	*buf;
	char	*head;
	char	*tail;
};

struct stlport {
	unsigned long		magic;
	struct tty_port		port;
	unsigned int		portnr;
	unsigned int		panelnr;
	unsigned int		brdnr;
	int			ioaddr;
	int			uartaddr;
	unsigned int		pagenr;
	unsigned long		istate;
	int			baud_base;
	int			custom_divisor;
	int			close_delay;
	int			closing_wait;
	int			openwaitcnt;
	int			brklen;
	unsigned int		sigs;
	unsigned int		rxignoremsk;
	unsigned int		rxmarkmsk;
	unsigned int		imr;
	unsigned int		crenable;
	unsigned long		clk;
	unsigned long		hwid;
	void			*uartp;
	comstats_t		stats;
	struct stlrq		tx;
};

struct stlpanel {
	unsigned long	magic;
	unsigned int	panelnr;
	unsigned int	brdnr;
	unsigned int	pagenr;
	unsigned int	nrports;
	int		iobase;
	void		*uartp;
	void		(*isr)(struct stlpanel *panelp, unsigned int iobase);
	unsigned int	hwid;
	unsigned int	ackmask;
	struct stlport	*ports[STL_PORTSPERPANEL];
};

struct stlbrd {
	unsigned long	magic;
	unsigned int	brdnr;
	unsigned int	brdtype;
	unsigned int	state;
	unsigned int	nrpanels;
	unsigned int	nrports;
	unsigned int	nrbnks;
	int		irq;
	int		irqtype;
	int		(*isr)(struct stlbrd *brdp);
	unsigned int	ioaddr1;
	unsigned int	ioaddr2;
	unsigned int	iosize1;
	unsigned int	iosize2;
	unsigned int	iostatus;
	unsigned int	ioctrl;
	unsigned int	ioctrlval;
	unsigned int	hwid;
	unsigned long	clk;
	unsigned int	bnkpageaddr[STL_MAXBANKS];
	unsigned int	bnkstataddr[STL_MAXBANKS];
	struct stlpanel	*bnk2panel[STL_MAXBANKS];
	struct stlpanel	*panels[STL_MAXPANELS];
};


#define	STL_PORTMAGIC	0x5a7182c9
#define	STL_PANELMAGIC	0x7ef621a1
#define	STL_BOARDMAGIC	0xa2267f52

#endif
