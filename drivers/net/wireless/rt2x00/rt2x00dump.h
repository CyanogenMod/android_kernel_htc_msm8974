/*
	Copyright (C) 2004 - 2009 Ivo van Doorn <IvDoorn@gmail.com>
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef RT2X00DUMP_H
#define RT2X00DUMP_H


enum rt2x00_dump_type {
	DUMP_FRAME_RXDONE = 1,
	DUMP_FRAME_TX = 2,
	DUMP_FRAME_TXDONE = 3,
	DUMP_FRAME_BEACON = 4,
};

struct rt2x00dump_hdr {
	__le32 version;
#define DUMP_HEADER_VERSION	2

	__le32 header_length;
	__le32 desc_length;
	__le32 data_length;

	__le16 chip_rt;
	__le16 chip_rf;
	__le16 chip_rev;

	__le16 type;
	__u8 queue_index;
	__u8 entry_index;

	__le32 timestamp_sec;
	__le32 timestamp_usec;
};

#endif 
