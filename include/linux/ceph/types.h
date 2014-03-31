#ifndef _FS_CEPH_TYPES_H
#define _FS_CEPH_TYPES_H

#include <linux/in.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/string.h>

#include "ceph_fs.h"
#include "ceph_frag.h"
#include "ceph_hash.h"

struct ceph_vino {
	u64 ino;
	u64 snap;
};


struct ceph_cap_reservation {
	int count;
};


#endif
