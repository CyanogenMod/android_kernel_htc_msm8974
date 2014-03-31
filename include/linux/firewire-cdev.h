/*
 * Char device interface.
 *
 * Copyright (C) 2005-2007  Kristian Hoegsberg <krh@bitplanet.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _LINUX_FIREWIRE_CDEV_H
#define _LINUX_FIREWIRE_CDEV_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/firewire-constants.h>

#define FW_CDEV_EVENT_BUS_RESET				0x00
#define FW_CDEV_EVENT_RESPONSE				0x01
#define FW_CDEV_EVENT_REQUEST				0x02
#define FW_CDEV_EVENT_ISO_INTERRUPT			0x03

#define FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED		0x04
#define FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED		0x05

#define FW_CDEV_EVENT_REQUEST2				0x06
#define FW_CDEV_EVENT_PHY_PACKET_SENT			0x07
#define FW_CDEV_EVENT_PHY_PACKET_RECEIVED		0x08
#define FW_CDEV_EVENT_ISO_INTERRUPT_MULTICHANNEL	0x09

struct fw_cdev_event_common {
	__u64 closure;
	__u32 type;
};

struct fw_cdev_event_bus_reset {
	__u64 closure;
	__u32 type;
	__u32 node_id;
	__u32 local_node_id;
	__u32 bm_node_id;
	__u32 irm_node_id;
	__u32 root_node_id;
	__u32 generation;
};

struct fw_cdev_event_response {
	__u64 closure;
	__u32 type;
	__u32 rcode;
	__u32 length;
	__u32 data[0];
};

struct fw_cdev_event_request {
	__u64 closure;
	__u32 type;
	__u32 tcode;
	__u64 offset;
	__u32 handle;
	__u32 length;
	__u32 data[0];
};

struct fw_cdev_event_request2 {
	__u64 closure;
	__u32 type;
	__u32 tcode;
	__u64 offset;
	__u32 source_node_id;
	__u32 destination_node_id;
	__u32 card;
	__u32 generation;
	__u32 handle;
	__u32 length;
	__u32 data[0];
};

struct fw_cdev_event_iso_interrupt {
	__u64 closure;
	__u32 type;
	__u32 cycle;
	__u32 header_length;
	__u32 header[0];
};

struct fw_cdev_event_iso_interrupt_mc {
	__u64 closure;
	__u32 type;
	__u32 completed;
};

struct fw_cdev_event_iso_resource {
	__u64 closure;
	__u32 type;
	__u32 handle;
	__s32 channel;
	__s32 bandwidth;
};

struct fw_cdev_event_phy_packet {
	__u64 closure;
	__u32 type;
	__u32 rcode;
	__u32 length;
	__u32 data[0];
};

union fw_cdev_event {
	struct fw_cdev_event_common		common;
	struct fw_cdev_event_bus_reset		bus_reset;
	struct fw_cdev_event_response		response;
	struct fw_cdev_event_request		request;
	struct fw_cdev_event_request2		request2;		
	struct fw_cdev_event_iso_interrupt	iso_interrupt;
	struct fw_cdev_event_iso_interrupt_mc	iso_interrupt_mc;	
	struct fw_cdev_event_iso_resource	iso_resource;		
	struct fw_cdev_event_phy_packet		phy_packet;		
};

#define FW_CDEV_IOC_GET_INFO           _IOWR('#', 0x00, struct fw_cdev_get_info)
#define FW_CDEV_IOC_SEND_REQUEST        _IOW('#', 0x01, struct fw_cdev_send_request)
#define FW_CDEV_IOC_ALLOCATE           _IOWR('#', 0x02, struct fw_cdev_allocate)
#define FW_CDEV_IOC_DEALLOCATE          _IOW('#', 0x03, struct fw_cdev_deallocate)
#define FW_CDEV_IOC_SEND_RESPONSE       _IOW('#', 0x04, struct fw_cdev_send_response)
#define FW_CDEV_IOC_INITIATE_BUS_RESET  _IOW('#', 0x05, struct fw_cdev_initiate_bus_reset)
#define FW_CDEV_IOC_ADD_DESCRIPTOR     _IOWR('#', 0x06, struct fw_cdev_add_descriptor)
#define FW_CDEV_IOC_REMOVE_DESCRIPTOR   _IOW('#', 0x07, struct fw_cdev_remove_descriptor)
#define FW_CDEV_IOC_CREATE_ISO_CONTEXT _IOWR('#', 0x08, struct fw_cdev_create_iso_context)
#define FW_CDEV_IOC_QUEUE_ISO          _IOWR('#', 0x09, struct fw_cdev_queue_iso)
#define FW_CDEV_IOC_START_ISO           _IOW('#', 0x0a, struct fw_cdev_start_iso)
#define FW_CDEV_IOC_STOP_ISO            _IOW('#', 0x0b, struct fw_cdev_stop_iso)

#define FW_CDEV_IOC_GET_CYCLE_TIMER     _IOR('#', 0x0c, struct fw_cdev_get_cycle_timer)

#define FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE       _IOWR('#', 0x0d, struct fw_cdev_allocate_iso_resource)
#define FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE      _IOW('#', 0x0e, struct fw_cdev_deallocate)
#define FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE   _IOW('#', 0x0f, struct fw_cdev_allocate_iso_resource)
#define FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE _IOW('#', 0x10, struct fw_cdev_allocate_iso_resource)
#define FW_CDEV_IOC_GET_SPEED                     _IO('#', 0x11) 
#define FW_CDEV_IOC_SEND_BROADCAST_REQUEST       _IOW('#', 0x12, struct fw_cdev_send_request)
#define FW_CDEV_IOC_SEND_STREAM_PACKET           _IOW('#', 0x13, struct fw_cdev_send_stream_packet)

#define FW_CDEV_IOC_GET_CYCLE_TIMER2   _IOWR('#', 0x14, struct fw_cdev_get_cycle_timer2)

#define FW_CDEV_IOC_SEND_PHY_PACKET    _IOWR('#', 0x15, struct fw_cdev_send_phy_packet)
#define FW_CDEV_IOC_RECEIVE_PHY_PACKETS _IOW('#', 0x16, struct fw_cdev_receive_phy_packets)
#define FW_CDEV_IOC_SET_ISO_CHANNELS    _IOW('#', 0x17, struct fw_cdev_set_iso_channels)

#define FW_CDEV_IOC_FLUSH_ISO           _IOW('#', 0x18, struct fw_cdev_flush_iso)


struct fw_cdev_get_info {
	__u32 version;
	__u32 rom_length;
	__u64 rom;
	__u64 bus_reset;
	__u64 bus_reset_closure;
	__u32 card;
};

struct fw_cdev_send_request {
	__u32 tcode;
	__u32 length;
	__u64 offset;
	__u64 closure;
	__u64 data;
	__u32 generation;
};

struct fw_cdev_send_response {
	__u32 rcode;
	__u32 length;
	__u64 data;
	__u32 handle;
};

/**
 * struct fw_cdev_allocate - Allocate a CSR in an address range
 * @offset:	Start offset of the address range
 * @closure:	To be passed back to userspace in request events
 * @length:	Length of the CSR, in bytes
 * @handle:	Handle to the allocation, written by the kernel
 * @region_end:	First address above the address range (added in ABI v4, 2.6.36)
 *
 * Allocate an address range in the 48-bit address space on the local node
 * (the controller).  This allows userspace to listen for requests with an
 * offset within that address range.  Every time when the kernel receives a
 * request within the range, an &fw_cdev_event_request2 event will be emitted.
 * (If the kernel or the client implements ABI version <= 3, an
 * &fw_cdev_event_request will be generated instead.)
 *
 * The @closure field is passed back to userspace in these request events.
 * The @handle field is an out parameter, returning a handle to the allocated
 * range to be used for later deallocation of the range.
 *
 * The address range is allocated on all local nodes.  The address allocation
 * is exclusive except for the FCP command and response registers.  If an
 * exclusive address region is already in use, the ioctl fails with errno set
 * to %EBUSY.
 *
 * If kernel and client implement ABI version >= 4, the kernel looks up a free
 * spot of size @length inside [@offset..@region_end) and, if found, writes
 * the start address of the new CSR back in @offset.  I.e. @offset is an
 * in and out parameter.  If this automatic placement of a CSR in a bigger
 * address range is not desired, the client simply needs to set @region_end
 * = @offset + @length.
 *
 * If the kernel or the client implements ABI version <= 3, @region_end is
 * ignored and effectively assumed to be @offset + @length.
 *
 * @region_end is only present in a kernel header >= 2.6.36.  If necessary,
 * this can for example be tested by #ifdef FW_CDEV_EVENT_REQUEST2.
 */
struct fw_cdev_allocate {
	__u64 offset;
	__u64 closure;
	__u32 length;
	__u32 handle;
	__u64 region_end;	
};

struct fw_cdev_deallocate {
	__u32 handle;
};

#define FW_CDEV_LONG_RESET	0
#define FW_CDEV_SHORT_RESET	1

struct fw_cdev_initiate_bus_reset {
	__u32 type;
};

/**
 * struct fw_cdev_add_descriptor - Add contents to the local node's config ROM
 * @immediate:	If non-zero, immediate key to insert before pointer
 * @key:	Upper 8 bits of root directory pointer
 * @data:	Userspace pointer to contents of descriptor block
 * @length:	Length of descriptor block data, in quadlets
 * @handle:	Handle to the descriptor, written by the kernel
 *
 * Add a descriptor block and optionally a preceding immediate key to the local
 * node's Configuration ROM.
 *
 * The @key field specifies the upper 8 bits of the descriptor root directory
 * pointer and the @data and @length fields specify the contents. The @key
 * should be of the form 0xXX000000. The offset part of the root directory entry
 * will be filled in by the kernel.
 *
 * If not 0, the @immediate field specifies an immediate key which will be
 * inserted before the root directory pointer.
 *
 * @immediate, @key, and @data array elements are CPU-endian quadlets.
 *
 * If successful, the kernel adds the descriptor and writes back a @handle to
 * the kernel-side object to be used for later removal of the descriptor block
 * and immediate key.  The kernel will also generate a bus reset to signal the
 * change of the Configuration ROM to other nodes.
 *
 * This ioctl affects the Configuration ROMs of all local nodes.
 * The ioctl only succeeds on device files which represent a local node.
 */
struct fw_cdev_add_descriptor {
	__u32 immediate;
	__u32 key;
	__u64 data;
	__u32 length;
	__u32 handle;
};

struct fw_cdev_remove_descriptor {
	__u32 handle;
};

#define FW_CDEV_ISO_CONTEXT_TRANSMIT			0
#define FW_CDEV_ISO_CONTEXT_RECEIVE			1
#define FW_CDEV_ISO_CONTEXT_RECEIVE_MULTICHANNEL	2 

/**
 * struct fw_cdev_create_iso_context - Create a context for isochronous I/O
 * @type:	%FW_CDEV_ISO_CONTEXT_TRANSMIT or %FW_CDEV_ISO_CONTEXT_RECEIVE or
 *		%FW_CDEV_ISO_CONTEXT_RECEIVE_MULTICHANNEL
 * @header_size: Header size to strip in single-channel reception
 * @channel:	Channel to bind to in single-channel reception or transmission
 * @speed:	Transmission speed
 * @closure:	To be returned in &fw_cdev_event_iso_interrupt or
 *		&fw_cdev_event_iso_interrupt_multichannel
 * @handle:	Handle to context, written back by kernel
 *
 * Prior to sending or receiving isochronous I/O, a context must be created.
 * The context records information about the transmit or receive configuration
 * and typically maps to an underlying hardware resource.  A context is set up
 * for either sending or receiving.  It is bound to a specific isochronous
 * @channel.
 *
 * In case of multichannel reception, @header_size and @channel are ignored
 * and the channels are selected by %FW_CDEV_IOC_SET_ISO_CHANNELS.
 *
 * For %FW_CDEV_ISO_CONTEXT_RECEIVE contexts, @header_size must be at least 4
 * and must be a multiple of 4.  It is ignored in other context types.
 *
 * @speed is ignored in receive context types.
 *
 * If a context was successfully created, the kernel writes back a handle to the
 * context, which must be passed in for subsequent operations on that context.
 *
 * Limitations:
 * No more than one iso context can be created per fd.
 * The total number of contexts that all userspace and kernelspace drivers can
 * create on a card at a time is a hardware limit, typically 4 or 8 contexts per
 * direction, and of them at most one multichannel receive context.
 */
struct fw_cdev_create_iso_context {
	__u32 type;
	__u32 header_size;
	__u32 channel;
	__u32 speed;
	__u64 closure;
	__u32 handle;
};

struct fw_cdev_set_iso_channels {
	__u64 channels;
	__u32 handle;
};

#define FW_CDEV_ISO_PAYLOAD_LENGTH(v)	(v)
#define FW_CDEV_ISO_INTERRUPT		(1 << 16)
#define FW_CDEV_ISO_SKIP		(1 << 17)
#define FW_CDEV_ISO_SYNC		(1 << 17)
#define FW_CDEV_ISO_TAG(v)		((v) << 18)
#define FW_CDEV_ISO_SY(v)		((v) << 20)
#define FW_CDEV_ISO_HEADER_LENGTH(v)	((v) << 24)

/**
 * struct fw_cdev_iso_packet - Isochronous packet
 * @control:	Contains the header length (8 uppermost bits),
 *		the sy field (4 bits), the tag field (2 bits), a sync flag
 *		or a skip flag (1 bit), an interrupt flag (1 bit), and the
 *		payload length (16 lowermost bits)
 * @header:	Header and payload in case of a transmit context.
 *
 * &struct fw_cdev_iso_packet is used to describe isochronous packet queues.
 * Use the FW_CDEV_ISO_ macros to fill in @control.
 * The @header array is empty in case of receive contexts.
 *
 * Context type %FW_CDEV_ISO_CONTEXT_TRANSMIT:
 *
 * @control.HEADER_LENGTH must be a multiple of 4.  It specifies the numbers of
 * bytes in @header that will be prepended to the packet's payload.  These bytes
 * are copied into the kernel and will not be accessed after the ioctl has
 * returned.
 *
 * The @control.SY and TAG fields are copied to the iso packet header.  These
 * fields are specified by IEEE 1394a and IEC 61883-1.
 *
 * The @control.SKIP flag specifies that no packet is to be sent in a frame.
 * When using this, all other fields except @control.INTERRUPT must be zero.
 *
 * When a packet with the @control.INTERRUPT flag set has been completed, an
 * &fw_cdev_event_iso_interrupt event will be sent.
 *
 * Context type %FW_CDEV_ISO_CONTEXT_RECEIVE:
 *
 * @control.HEADER_LENGTH must be a multiple of the context's header_size.
 * If the HEADER_LENGTH is larger than the context's header_size, multiple
 * packets are queued for this entry.
 *
 * The @control.SY and TAG fields are ignored.
 *
 * If the @control.SYNC flag is set, the context drops all packets until a
 * packet with a sy field is received which matches &fw_cdev_start_iso.sync.
 *
 * @control.PAYLOAD_LENGTH defines how many payload bytes can be received for
 * one packet (in addition to payload quadlets that have been defined as headers
 * and are stripped and returned in the &fw_cdev_event_iso_interrupt structure).
 * If more bytes are received, the additional bytes are dropped.  If less bytes
 * are received, the remaining bytes in this part of the payload buffer will not
 * be written to, not even by the next packet.  I.e., packets received in
 * consecutive frames will not necessarily be consecutive in memory.  If an
 * entry has queued multiple packets, the PAYLOAD_LENGTH is divided equally
 * among them.
 *
 * When a packet with the @control.INTERRUPT flag set has been completed, an
 * &fw_cdev_event_iso_interrupt event will be sent.  An entry that has queued
 * multiple receive packets is completed when its last packet is completed.
 *
 * Context type %FW_CDEV_ISO_CONTEXT_RECEIVE_MULTICHANNEL:
 *
 * Here, &fw_cdev_iso_packet would be more aptly named _iso_buffer_chunk since
 * it specifies a chunk of the mmap()'ed buffer, while the number and alignment
 * of packets to be placed into the buffer chunk is not known beforehand.
 *
 * @control.PAYLOAD_LENGTH is the size of the buffer chunk and specifies room
 * for header, payload, padding, and trailer bytes of one or more packets.
 * It must be a multiple of 4.
 *
 * @control.HEADER_LENGTH, TAG and SY are ignored.  SYNC is treated as described
 * for single-channel reception.
 *
 * When a buffer chunk with the @control.INTERRUPT flag set has been filled
 * entirely, an &fw_cdev_event_iso_interrupt_mc event will be sent.
 */
struct fw_cdev_iso_packet {
	__u32 control;
	__u32 header[0];
};

struct fw_cdev_queue_iso {
	__u64 packets;
	__u64 data;
	__u32 size;
	__u32 handle;
};

#define FW_CDEV_ISO_CONTEXT_MATCH_TAG0		 1
#define FW_CDEV_ISO_CONTEXT_MATCH_TAG1		 2
#define FW_CDEV_ISO_CONTEXT_MATCH_TAG2		 4
#define FW_CDEV_ISO_CONTEXT_MATCH_TAG3		 8
#define FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS	15

struct fw_cdev_start_iso {
	__s32 cycle;
	__u32 sync;
	__u32 tags;
	__u32 handle;
};

struct fw_cdev_stop_iso {
	__u32 handle;
};

struct fw_cdev_flush_iso {
	__u32 handle;
};

struct fw_cdev_get_cycle_timer {
	__u64 local_time;
	__u32 cycle_timer;
};

struct fw_cdev_get_cycle_timer2 {
	__s64 tv_sec;
	__s32 tv_nsec;
	__s32 clk_id;
	__u32 cycle_timer;
};

/**
 * struct fw_cdev_allocate_iso_resource - (De)allocate a channel or bandwidth
 * @closure:	Passed back to userspace in corresponding iso resource events
 * @channels:	Isochronous channels of which one is to be (de)allocated
 * @bandwidth:	Isochronous bandwidth units to be (de)allocated
 * @handle:	Handle to the allocation, written by the kernel (only valid in
 *		case of %FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE ioctls)
 *
 * The %FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE ioctl initiates allocation of an
 * isochronous channel and/or of isochronous bandwidth at the isochronous
 * resource manager (IRM).  Only one of the channels specified in @channels is
 * allocated.  An %FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED is sent after
 * communication with the IRM, indicating success or failure in the event data.
 * The kernel will automatically reallocate the resources after bus resets.
 * Should a reallocation fail, an %FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED event
 * will be sent.  The kernel will also automatically deallocate the resources
 * when the file descriptor is closed.
 *
 * The %FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE ioctl can be used to initiate
 * deallocation of resources which were allocated as described above.
 * An %FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED event concludes this operation.
 *
 * The %FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE ioctl is a variant of allocation
 * without automatic re- or deallocation.
 * An %FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED event concludes this operation,
 * indicating success or failure in its data.
 *
 * The %FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE ioctl works like
 * %FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE except that resources are freed
 * instead of allocated.
 * An %FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED event concludes this operation.
 *
 * To summarize, %FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE allocates iso resources
 * for the lifetime of the fd or @handle.
 * In contrast, %FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE allocates iso resources
 * for the duration of a bus generation.
 *
 * @channels is a host-endian bitfield with the least significant bit
 * representing channel 0 and the most significant bit representing channel 63:
 * 1ULL << c for each channel c that is a candidate for (de)allocation.
 *
 * @bandwidth is expressed in bandwidth allocation units, i.e. the time to send
 * one quadlet of data (payload or header data) at speed S1600.
 */
struct fw_cdev_allocate_iso_resource {
	__u64 closure;
	__u64 channels;
	__u32 bandwidth;
	__u32 handle;
};

struct fw_cdev_send_stream_packet {
	__u32 length;
	__u32 tag;
	__u32 channel;
	__u32 sy;
	__u64 closure;
	__u64 data;
	__u32 generation;
	__u32 speed;
};

struct fw_cdev_send_phy_packet {
	__u64 closure;
	__u32 data[2];
	__u32 generation;
};

struct fw_cdev_receive_phy_packets {
	__u64 closure;
};

#define FW_CDEV_VERSION 3 

#endif 
