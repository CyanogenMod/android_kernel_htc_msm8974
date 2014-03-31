/******************************************************************************
 * grant_table.h
 *
 * Interface for granting foreign access to page frames, and receiving
 * page-ownership transfers.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2004, K A Fraser
 */

#ifndef __XEN_PUBLIC_GRANT_TABLE_H__
#define __XEN_PUBLIC_GRANT_TABLE_H__

#include <xen/interface/xen.h>


/* Some rough guidelines on accessing and updating grant-table entries
 * in a concurrency-safe manner. For more information, Linux contains a
 * reference implementation for guest OSes (arch/xen/kernel/grant_table.c).
 *
 * NB. WMB is a no-op on current-generation x86 processors. However, a
 *     compiler barrier will still be required.
 *
 * Introducing a valid entry into the grant table:
 *  1. Write ent->domid.
 *  2. Write ent->frame:
 *      GTF_permit_access:   Frame to which access is permitted.
 *      GTF_accept_transfer: Pseudo-phys frame slot being filled by new
 *                           frame, or zero if none.
 *  3. Write memory barrier (WMB).
 *  4. Write ent->flags, inc. valid type.
 *
 * Invalidating an unused GTF_permit_access entry:
 *  1. flags = ent->flags.
 *  2. Observe that !(flags & (GTF_reading|GTF_writing)).
 *  3. Check result of SMP-safe CMPXCHG(&ent->flags, flags, 0).
 *  NB. No need for WMB as reuse of entry is control-dependent on success of
 *      step 3, and all architectures guarantee ordering of ctrl-dep writes.
 *
 * Invalidating an in-use GTF_permit_access entry:
 *  This cannot be done directly. Request assistance from the domain controller
 *  which can set a timeout on the use of a grant entry and take necessary
 *  action. (NB. This is not yet implemented!).
 *
 * Invalidating an unused GTF_accept_transfer entry:
 *  1. flags = ent->flags.
 *  2. Observe that !(flags & GTF_transfer_committed). [*]
 *  3. Check result of SMP-safe CMPXCHG(&ent->flags, flags, 0).
 *  NB. No need for WMB as reuse of entry is control-dependent on success of
 *      step 3, and all architectures guarantee ordering of ctrl-dep writes.
 *  [*] If GTF_transfer_committed is set then the grant entry is 'committed'.
 *      The guest must /not/ modify the grant entry until the address of the
 *      transferred frame is written. It is safe for the guest to spin waiting
 *      for this to occur (detect by observing GTF_transfer_completed in
 *      ent->flags).
 *
 * Invalidating a committed GTF_accept_transfer entry:
 *  1. Wait for (ent->flags & GTF_transfer_completed).
 *
 * Changing a GTF_permit_access from writable to read-only:
 *  Use SMP-safe CMPXCHG to set GTF_readonly, while checking !GTF_writing.
 *
 * Changing a GTF_permit_access from read-only to writable:
 *  Use SMP-safe bit-setting instruction.
 */

typedef uint32_t grant_ref_t;

/*
 * A grant table comprises a packed array of grant entries in one or more
 * page frames shared between Xen and a guest.
 * [XEN]: This field is written by Xen and read by the sharing guest.
 * [GST]: This field is written by the guest and read by Xen.
 */

struct grant_entry_v1 {
    
    uint16_t flags;
    
    domid_t  domid;
    uint32_t frame;
};

#define GTF_invalid         (0U<<0)
#define GTF_permit_access   (1U<<0)
#define GTF_accept_transfer (2U<<0)
#define GTF_transitive      (3U<<0)
#define GTF_type_mask       (3U<<0)

#define _GTF_readonly       (2)
#define GTF_readonly        (1U<<_GTF_readonly)
#define _GTF_reading        (3)
#define GTF_reading         (1U<<_GTF_reading)
#define _GTF_writing        (4)
#define GTF_writing         (1U<<_GTF_writing)
#define _GTF_sub_page       (8)
#define GTF_sub_page        (1U<<_GTF_sub_page)

#define _GTF_transfer_committed (2)
#define GTF_transfer_committed  (1U<<_GTF_transfer_committed)
#define _GTF_transfer_completed (3)
#define GTF_transfer_completed  (1U<<_GTF_transfer_completed)


struct grant_entry_header {
    uint16_t flags;
    domid_t  domid;
};

union grant_entry_v2 {
    struct grant_entry_header hdr;

    struct {
	struct grant_entry_header hdr;
	uint32_t pad0;
	uint64_t frame;
    } full_page;

    struct {
	struct grant_entry_header hdr;
	uint16_t page_off;
	uint16_t length;
	uint64_t frame;
    } sub_page;

    struct {
	struct grant_entry_header hdr;
	domid_t trans_domid;
	uint16_t pad0;
	grant_ref_t gref;
    } transitive;

    uint32_t __spacer[4]; 
};

typedef uint16_t grant_status_t;


typedef uint32_t grant_handle_t;

#define GNTTABOP_map_grant_ref        0
struct gnttab_map_grant_ref {
    
    uint64_t host_addr;
    uint32_t flags;               
    grant_ref_t ref;
    domid_t  dom;
    
    int16_t  status;              
    grant_handle_t handle;
    uint64_t dev_bus_addr;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_map_grant_ref);

#define GNTTABOP_unmap_grant_ref      1
struct gnttab_unmap_grant_ref {
    
    uint64_t host_addr;
    uint64_t dev_bus_addr;
    grant_handle_t handle;
    
    int16_t  status;              
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_unmap_grant_ref);

/*
 * GNTTABOP_setup_table: Set up a grant table for <dom> comprising at least
 * <nr_frames> pages. The frame addresses are written to the <frame_list>.
 * Only <nr_frames> addresses are written, even if the table is larger.
 * NOTES:
 *  1. <dom> may be specified as DOMID_SELF.
 *  2. Only a sufficiently-privileged domain may specify <dom> != DOMID_SELF.
 *  3. Xen may not support more than a single grant-table page per domain.
 */
#define GNTTABOP_setup_table          2
struct gnttab_setup_table {
    
    domid_t  dom;
    uint32_t nr_frames;
    
    int16_t  status;              
    GUEST_HANDLE(ulong) frame_list;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_setup_table);

#define GNTTABOP_dump_table           3
struct gnttab_dump_table {
    
    domid_t dom;
    
    int16_t status;               
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_dump_table);

#define GNTTABOP_transfer                4
struct gnttab_transfer {
    
    unsigned long mfn;
    domid_t       domid;
    grant_ref_t   ref;
    
    int16_t       status;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_transfer);


#define _GNTCOPY_source_gref      (0)
#define GNTCOPY_source_gref       (1<<_GNTCOPY_source_gref)
#define _GNTCOPY_dest_gref        (1)
#define GNTCOPY_dest_gref         (1<<_GNTCOPY_dest_gref)

#define GNTTABOP_copy                 5
struct gnttab_copy {
	
	struct {
		union {
			grant_ref_t ref;
			unsigned long   gmfn;
		} u;
		domid_t  domid;
		uint16_t offset;
	} source, dest;
	uint16_t      len;
	uint16_t      flags;          
	
	int16_t       status;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_copy);

#define GNTTABOP_query_size           6
struct gnttab_query_size {
    
    domid_t  dom;
    
    uint32_t nr_frames;
    uint32_t max_nr_frames;
    int16_t  status;              
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_query_size);

#define GNTTABOP_unmap_and_replace    7
struct gnttab_unmap_and_replace {
    
    uint64_t host_addr;
    uint64_t new_addr;
    grant_handle_t handle;
    
    int16_t  status;              
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_unmap_and_replace);

#define GNTTABOP_set_version          8
struct gnttab_set_version {
    
    uint32_t version;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_set_version);

#define GNTTABOP_get_status_frames     9
struct gnttab_get_status_frames {
    
    uint32_t nr_frames;
    domid_t  dom;
    
    int16_t  status;              
    GUEST_HANDLE(uint64_t) frame_list;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_get_status_frames);

#define GNTTABOP_get_version          10
struct gnttab_get_version {
    
    domid_t dom;
    uint16_t pad;
    
    uint32_t version;
};
DEFINE_GUEST_HANDLE_STRUCT(gnttab_get_version);

 
#define _GNTMAP_device_map      (0)
#define GNTMAP_device_map       (1<<_GNTMAP_device_map)
 
#define _GNTMAP_host_map        (1)
#define GNTMAP_host_map         (1<<_GNTMAP_host_map)
 
#define _GNTMAP_readonly        (2)
#define GNTMAP_readonly         (1<<_GNTMAP_readonly)
#define _GNTMAP_application_map (3)
#define GNTMAP_application_map  (1<<_GNTMAP_application_map)

#define _GNTMAP_contains_pte    (4)
#define GNTMAP_contains_pte     (1<<_GNTMAP_contains_pte)

#define GNTST_okay             (0)  
#define GNTST_general_error    (-1) 
#define GNTST_bad_domain       (-2) 
#define GNTST_bad_gntref       (-3) 
#define GNTST_bad_handle       (-4) 
#define GNTST_bad_virt_addr    (-5) 
#define GNTST_bad_dev_addr     (-6) 
#define GNTST_no_device_space  (-7) 
#define GNTST_permission_denied (-8) 
#define GNTST_bad_page         (-9) 
#define GNTST_bad_copy_arg    (-10) 

#define GNTTABOP_error_msgs {                   \
    "okay",                                     \
    "undefined error",                          \
    "unrecognised domain id",                   \
    "invalid grant reference",                  \
    "invalid mapping handle",                   \
    "invalid virtual address",                  \
    "invalid device address",                   \
    "no spare translation slot in the I/O MMU", \
    "permission denied",                        \
    "bad page",                                 \
    "copy arguments cross page boundary"        \
}

#endif 
