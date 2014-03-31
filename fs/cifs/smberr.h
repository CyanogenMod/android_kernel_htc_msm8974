/*
 *   fs/cifs/smberr.h
 *
 *   Copyright (c) International Business Machines  Corp., 2002,2004
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   See Error Codes section of the SNIA CIFS Specification
 *   for more information
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define SUCCESS	0x00	
#define ERRDOS	0x01	
#define ERRSRV	0x02	
#define ERRHRD	0x03	
#define ERRCMD	0xFF	




#define ERRbadfunc		1	
#define ERRbadfile		2	
#define ERRbadpath		3	
#define ERRnofids		4	
#define ERRnoaccess		5	
#define ERRbadfid		6	
#define ERRbadmcb		7	
#define ERRnomem		8	
#define ERRbadmem		9	
#define ERRbadenv		10	
#define ERRbadformat		11	
#define ERRbadaccess		12	
#define ERRbaddata		13	
#define ERRbaddrive		15	
#define ERRremcd		16	
#define ERRdiffdevice		17	
#define ERRnofiles		18	
#define ERRwriteprot		19	
#define ERRgeneral		31
#define ERRbadshare		32	
#define ERRlock			33	
#define ERRunsup		50
#define ERRnosuchshare		67
#define ERRfilexists		80	
#define ERRinvparm		87
#define ERRdiskfull		112
#define ERRinvname		123
#define ERRinvlevel		124
#define ERRdirnotempty		145
#define ERRnotlocked		158
#define ERRcancelviolation	173
#define ERRalreadyexists	183
#define ERRbadpipe		230
#define ERRpipebusy		231
#define ERRpipeclosing		232
#define ERRnotconnected		233
#define ERRmoredata		234
#define ERReasnotsupported	282
#define ErrQuota		0x200	
#define ErrNotALink		0x201	

#define ERRsymlink              0xFFFD
#define ErrTooManyLinks         0xFFFE


#define ERRerror		1	
#define ERRbadpw		2	
#define ERRbadtype		3	
#define ERRaccess		4	
#define ERRinvtid		5	
#define ERRinvnetname		6	
#define ERRinvdevice		7	
#define ERRqfull		49	
#define ERRqtoobig		50	
#define ERRqeof			51	
#define ERRinvpfid		52	
#define ERRsmbcmd		64	
#define ERRsrverror		65	
#define ERRbadBID		66	
#define ERRfilespecs		67	
#define ERRbadLink		68	
#define ERRbadpermits		69	
#define ERRbadPID		70
#define ERRsetattrmode		71	
#define ERRpaused		81	
#define ERRmsgoff		82	
#define ERRnoroom		83	
#define ERRrmuns		87	
#define ERRtimeout		88	
#define ERRnoresource		89	
#define ERRtoomanyuids		90	
#define ERRbaduid		91	
#define ERRusempx		250	
#define ERRusestd		251	
#define ERR_NOTIFY_ENUM_DIR	1024
#define ERRnoSuchUser		2238	
#define ERRaccountexpired	2239
#define ERRbadclient		2240	
#define ERRbadLogonTime		2241	
#define ERRpasswordExpired	2242
#define ERRnetlogonNotStarted	2455
#define ERRnosupport		0xFFFF
