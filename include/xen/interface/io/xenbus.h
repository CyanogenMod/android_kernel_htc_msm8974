/*****************************************************************************
 * xenbus.h
 *
 * Xenbus protocol details.
 *
 * Copyright (C) 2005 XenSource Ltd.
 */

#ifndef _XEN_PUBLIC_IO_XENBUS_H
#define _XEN_PUBLIC_IO_XENBUS_H

enum xenbus_state
{
	XenbusStateUnknown      = 0,
	XenbusStateInitialising = 1,
	XenbusStateInitWait     = 2,  
	XenbusStateInitialised  = 3,  
	XenbusStateConnected    = 4,
	XenbusStateClosing      = 5,  
	XenbusStateClosed       = 6,

	XenbusStateReconfiguring = 7,

	XenbusStateReconfigured  = 8
};

#endif 

