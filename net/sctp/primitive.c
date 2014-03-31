/* SCTP kernel implementation
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 *
 * This file is part of the SCTP kernel implementation
 *
 * These functions implement the SCTP primitive functions from Section 10.
 *
 * Note that the descriptions from the specification are USER level
 * functions--this file is the functions which populate the struct proto
 * for SCTP which is the BOTTOM of the sockets interface.
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Narasimha Budihal     <narasimha@refcode.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Kevin Gao             <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#include <linux/list.h> 
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/time.h> 
#include <linux/gfp.h>
#include <net/sock.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

#define DECLARE_PRIMITIVE(name) \
 \
int sctp_primitive_ ## name(struct sctp_association *asoc, \
			    void *arg) { \
	int error = 0; \
	sctp_event_t event_type; sctp_subtype_t subtype; \
	sctp_state_t state; \
	struct sctp_endpoint *ep; \
	\
	event_type = SCTP_EVENT_T_PRIMITIVE; \
	subtype = SCTP_ST_PRIMITIVE(SCTP_PRIMITIVE_ ## name); \
	state = asoc ? asoc->state : SCTP_STATE_CLOSED; \
	ep = asoc ? asoc->ep : NULL; \
	\
	error = sctp_do_sm(event_type, subtype, state, ep, asoc, \
			   arg, GFP_KERNEL); \
	return error; \
}



DECLARE_PRIMITIVE(ASSOCIATE)


DECLARE_PRIMITIVE(SHUTDOWN);


DECLARE_PRIMITIVE(ABORT);


DECLARE_PRIMITIVE(SEND);


DECLARE_PRIMITIVE(REQUESTHEARTBEAT);


DECLARE_PRIMITIVE(ASCONF);
