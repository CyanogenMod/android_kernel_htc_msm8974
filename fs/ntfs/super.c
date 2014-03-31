/*
 * super.c - NTFS kernel super block handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2012 Anton Altaparmakov and Tuxera Inc.
 * Copyright (c) 2001,2002 Richard Russon
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>	
#include <linux/backing-dev.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/moduleparam.h>
#include <linux/bitmap.h>

#include "sysctl.h"
#include "logfile.h"
#include "quota.h"
#include "usnjrnl.h"
#include "dir.h"
#include "debug.h"
#include "index.h"
#include "inode.h"
#include "aops.h"
#include "layout.h"
#include "malloc.h"
#include "ntfs.h"

static unsigned long ntfs_nr_compression_users;

static ntfschar *default_upcase = NULL;
static unsigned long ntfs_nr_upcase_users = 0;

typedef enum {
	
	ON_ERRORS_PANIC			= 0x01,
	ON_ERRORS_REMOUNT_RO		= 0x02,
	ON_ERRORS_CONTINUE		= 0x04,
	
	ON_ERRORS_RECOVER		= 0x10,
} ON_ERRORS_ACTIONS;

const option_t on_errors_arr[] = {
	{ ON_ERRORS_PANIC,	"panic" },
	{ ON_ERRORS_REMOUNT_RO,	"remount-ro", },
	{ ON_ERRORS_CONTINUE,	"continue", },
	{ ON_ERRORS_RECOVER,	"recover" },
	{ 0,			NULL }
};

static int simple_getbool(char *s, bool *setval)
{
	if (s) {
		if (!strcmp(s, "1") || !strcmp(s, "yes") || !strcmp(s, "true"))
			*setval = true;
		else if (!strcmp(s, "0") || !strcmp(s, "no") ||
							!strcmp(s, "false"))
			*setval = false;
		else
			return 0;
	} else
		*setval = true;
	return 1;
}

static bool parse_options(ntfs_volume *vol, char *opt)
{
	char *p, *v, *ov;
	static char *utf8 = "utf8";
	int errors = 0, sloppy = 0;
	uid_t uid = (uid_t)-1;
	gid_t gid = (gid_t)-1;
	umode_t fmask = (umode_t)-1, dmask = (umode_t)-1;
	int mft_zone_multiplier = -1, on_errors = -1;
	int show_sys_files = -1, case_sensitive = -1, disable_sparse = -1;
	struct nls_table *nls_map = NULL, *old_nls;

	
#define NTFS_GETOPT_WITH_DEFAULT(option, variable, default_value)	\
	if (!strcmp(p, option)) {					\
		if (!v || !*v)						\
			variable = default_value;			\
		else {							\
			variable = simple_strtoul(ov = v, &v, 0);	\
			if (*v)						\
				goto needs_val;				\
		}							\
	}
#define NTFS_GETOPT(option, variable)					\
	if (!strcmp(p, option)) {					\
		if (!v || !*v)						\
			goto needs_arg;					\
		variable = simple_strtoul(ov = v, &v, 0);		\
		if (*v)							\
			goto needs_val;					\
	}
#define NTFS_GETOPT_OCTAL(option, variable)				\
	if (!strcmp(p, option)) {					\
		if (!v || !*v)						\
			goto needs_arg;					\
		variable = simple_strtoul(ov = v, &v, 8);		\
		if (*v)							\
			goto needs_val;					\
	}
#define NTFS_GETOPT_BOOL(option, variable)				\
	if (!strcmp(p, option)) {					\
		bool val;						\
		if (!simple_getbool(v, &val))				\
			goto needs_bool;				\
		variable = val;						\
	}
#define NTFS_GETOPT_OPTIONS_ARRAY(option, variable, opt_array)		\
	if (!strcmp(p, option)) {					\
		int _i;							\
		if (!v || !*v)						\
			goto needs_arg;					\
		ov = v;							\
		if (variable == -1)					\
			variable = 0;					\
		for (_i = 0; opt_array[_i].str && *opt_array[_i].str; _i++) \
			if (!strcmp(opt_array[_i].str, v)) {		\
				variable |= opt_array[_i].val;		\
				break;					\
			}						\
		if (!opt_array[_i].str || !*opt_array[_i].str)		\
			goto needs_val;					\
	}
	if (!opt || !*opt)
		goto no_mount_options;
	ntfs_debug("Entering with mount options string: %s", opt);
	while ((p = strsep(&opt, ","))) {
		if ((v = strchr(p, '=')))
			*v++ = 0;
		NTFS_GETOPT("uid", uid)
		else NTFS_GETOPT("gid", gid)
		else NTFS_GETOPT_OCTAL("umask", fmask = dmask)
		else NTFS_GETOPT_OCTAL("fmask", fmask)
		else NTFS_GETOPT_OCTAL("dmask", dmask)
		else NTFS_GETOPT("mft_zone_multiplier", mft_zone_multiplier)
		else NTFS_GETOPT_WITH_DEFAULT("sloppy", sloppy, true)
		else NTFS_GETOPT_BOOL("show_sys_files", show_sys_files)
		else NTFS_GETOPT_BOOL("case_sensitive", case_sensitive)
		else NTFS_GETOPT_BOOL("disable_sparse", disable_sparse)
		else NTFS_GETOPT_OPTIONS_ARRAY("errors", on_errors,
				on_errors_arr)
		else if (!strcmp(p, "posix") || !strcmp(p, "show_inodes"))
			ntfs_warning(vol->sb, "Ignoring obsolete option %s.",
					p);
		else if (!strcmp(p, "nls") || !strcmp(p, "iocharset")) {
			if (!strcmp(p, "iocharset"))
				ntfs_warning(vol->sb, "Option iocharset is "
						"deprecated. Please use "
						"option nls=<charsetname> in "
						"the future.");
			if (!v || !*v)
				goto needs_arg;
use_utf8:
			old_nls = nls_map;
			nls_map = load_nls(v);
			if (!nls_map) {
				if (!old_nls) {
					ntfs_error(vol->sb, "NLS character set "
							"%s not found.", v);
					return false;
				}
				ntfs_error(vol->sb, "NLS character set %s not "
						"found. Using previous one %s.",
						v, old_nls->charset);
				nls_map = old_nls;
			} else  {
				unload_nls(old_nls);
			}
		} else if (!strcmp(p, "utf8")) {
			bool val = false;
			ntfs_warning(vol->sb, "Option utf8 is no longer "
				   "supported, using option nls=utf8. Please "
				   "use option nls=utf8 in the future and "
				   "make sure utf8 is compiled either as a "
				   "module or into the kernel.");
			if (!v || !*v)
				val = true;
			else if (!simple_getbool(v, &val))
				goto needs_bool;
			if (val) {
				v = utf8;
				goto use_utf8;
			}
		} else {
			ntfs_error(vol->sb, "Unrecognized mount option %s.", p);
			if (errors < INT_MAX)
				errors++;
		}
#undef NTFS_GETOPT_OPTIONS_ARRAY
#undef NTFS_GETOPT_BOOL
#undef NTFS_GETOPT
#undef NTFS_GETOPT_WITH_DEFAULT
	}
no_mount_options:
	if (errors && !sloppy)
		return false;
	if (sloppy)
		ntfs_warning(vol->sb, "Sloppy option given. Ignoring "
				"unrecognized mount option(s) and continuing.");
	
	if (on_errors != -1) {
		if (!on_errors) {
			ntfs_error(vol->sb, "Invalid errors option argument "
					"or bug in options parser.");
			return false;
		}
	}
	if (nls_map) {
		if (vol->nls_map && vol->nls_map != nls_map) {
			ntfs_error(vol->sb, "Cannot change NLS character set "
					"on remount.");
			return false;
		} 
		ntfs_debug("Using NLS character set %s.", nls_map->charset);
		vol->nls_map = nls_map;
	} else  {
		if (!vol->nls_map) {
			vol->nls_map = load_nls_default();
			if (!vol->nls_map) {
				ntfs_error(vol->sb, "Failed to load default "
						"NLS character set.");
				return false;
			}
			ntfs_debug("Using default NLS character set (%s).",
					vol->nls_map->charset);
		}
	}
	if (mft_zone_multiplier != -1) {
		if (vol->mft_zone_multiplier && vol->mft_zone_multiplier !=
				mft_zone_multiplier) {
			ntfs_error(vol->sb, "Cannot change mft_zone_multiplier "
					"on remount.");
			return false;
		}
		if (mft_zone_multiplier < 1 || mft_zone_multiplier > 4) {
			ntfs_error(vol->sb, "Invalid mft_zone_multiplier. "
					"Using default value, i.e. 1.");
			mft_zone_multiplier = 1;
		}
		vol->mft_zone_multiplier = mft_zone_multiplier;
	}
	if (!vol->mft_zone_multiplier)
		vol->mft_zone_multiplier = 1;
	if (on_errors != -1)
		vol->on_errors = on_errors;
	if (!vol->on_errors || vol->on_errors == ON_ERRORS_RECOVER)
		vol->on_errors |= ON_ERRORS_CONTINUE;
	if (uid != (uid_t)-1)
		vol->uid = uid;
	if (gid != (gid_t)-1)
		vol->gid = gid;
	if (fmask != (umode_t)-1)
		vol->fmask = fmask;
	if (dmask != (umode_t)-1)
		vol->dmask = dmask;
	if (show_sys_files != -1) {
		if (show_sys_files)
			NVolSetShowSystemFiles(vol);
		else
			NVolClearShowSystemFiles(vol);
	}
	if (case_sensitive != -1) {
		if (case_sensitive)
			NVolSetCaseSensitive(vol);
		else
			NVolClearCaseSensitive(vol);
	}
	if (disable_sparse != -1) {
		if (disable_sparse)
			NVolClearSparseEnabled(vol);
		else {
			if (!NVolSparseEnabled(vol) &&
					vol->major_ver && vol->major_ver < 3)
				ntfs_warning(vol->sb, "Not enabling sparse "
						"support due to NTFS volume "
						"version %i.%i (need at least "
						"version 3.0).", vol->major_ver,
						vol->minor_ver);
			else
				NVolSetSparseEnabled(vol);
		}
	}
	return true;
needs_arg:
	ntfs_error(vol->sb, "The %s option requires an argument.", p);
	return false;
needs_bool:
	ntfs_error(vol->sb, "The %s option requires a boolean argument.", p);
	return false;
needs_val:
	ntfs_error(vol->sb, "Invalid %s option argument: %s", p, ov);
	return false;
}

#ifdef NTFS_RW

static int ntfs_write_volume_flags(ntfs_volume *vol, const VOLUME_FLAGS flags)
{
	ntfs_inode *ni = NTFS_I(vol->vol_ino);
	MFT_RECORD *m;
	VOLUME_INFORMATION *vi;
	ntfs_attr_search_ctx *ctx;
	int err;

	ntfs_debug("Entering, old flags = 0x%x, new flags = 0x%x.",
			le16_to_cpu(vol->vol_flags), le16_to_cpu(flags));
	if (vol->vol_flags == flags)
		goto done;
	BUG_ON(!ni);
	m = map_mft_record(ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(ni, m);
	if (!ctx) {
		err = -ENOMEM;
		goto put_unm_err_out;
	}
	err = ntfs_attr_lookup(AT_VOLUME_INFORMATION, NULL, 0, 0, 0, NULL, 0,
			ctx);
	if (err)
		goto put_unm_err_out;
	vi = (VOLUME_INFORMATION*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset));
	vol->vol_flags = vi->flags = flags;
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(ni);
done:
	ntfs_debug("Done.");
	return 0;
put_unm_err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(ni);
err_out:
	ntfs_error(vol->sb, "Failed with error code %i.", -err);
	return err;
}

static inline int ntfs_set_volume_flags(ntfs_volume *vol, VOLUME_FLAGS flags)
{
	flags &= VOLUME_FLAGS_MASK;
	return ntfs_write_volume_flags(vol, vol->vol_flags | flags);
}

static inline int ntfs_clear_volume_flags(ntfs_volume *vol, VOLUME_FLAGS flags)
{
	flags &= VOLUME_FLAGS_MASK;
	flags = vol->vol_flags & cpu_to_le16(~le16_to_cpu(flags));
	return ntfs_write_volume_flags(vol, flags);
}

#endif 

static int ntfs_remount(struct super_block *sb, int *flags, char *opt)
{
	ntfs_volume *vol = NTFS_SB(sb);

	ntfs_debug("Entering with remount options string: %s", opt);

#ifndef NTFS_RW
	
	*flags |= MS_RDONLY;
#else 
	/*
	 * For the read-write compiled driver, if we are remounting read-write,
	 * make sure there are no volume errors and that no unsupported volume
	 * flags are set.  Also, empty the logfile journal as it would become
	 * stale as soon as something is written to the volume and mark the
	 * volume dirty so that chkdsk is run if the volume is not umounted
	 * cleanly.  Finally, mark the quotas out of date so Windows rescans
	 * the volume on boot and updates them.
	 *
	 * When remounting read-only, mark the volume clean if no volume errors
	 * have occurred.
	 */
	if ((sb->s_flags & MS_RDONLY) && !(*flags & MS_RDONLY)) {
		static const char *es = ".  Cannot remount read-write.";

		
		if (NVolErrors(vol)) {
			ntfs_error(sb, "Volume has errors and is read-only%s",
					es);
			return -EROFS;
		}
		if (vol->vol_flags & VOLUME_IS_DIRTY) {
			ntfs_error(sb, "Volume is dirty and read-only%s", es);
			return -EROFS;
		}
		if (vol->vol_flags & VOLUME_MODIFIED_BY_CHKDSK) {
			ntfs_error(sb, "Volume has been modified by chkdsk "
					"and is read-only%s", es);
			return -EROFS;
		}
		if (vol->vol_flags & VOLUME_MUST_MOUNT_RO_MASK) {
			ntfs_error(sb, "Volume has unsupported flags set "
					"(0x%x) and is read-only%s",
					(unsigned)le16_to_cpu(vol->vol_flags),
					es);
			return -EROFS;
		}
		if (ntfs_set_volume_flags(vol, VOLUME_IS_DIRTY)) {
			ntfs_error(sb, "Failed to set dirty bit in volume "
					"information flags%s", es);
			return -EROFS;
		}
#if 0
		
		
		
		if ((vol->major_ver > 1)) {
			if (ntfs_set_volume_flags(vol, VOLUME_MOUNTED_ON_NT4)) {
				ntfs_error(sb, "Failed to set NT4 "
						"compatibility flag%s", es);
				NVolSetErrors(vol);
				return -EROFS;
			}
		}
#endif
		if (!ntfs_empty_logfile(vol->logfile_ino)) {
			ntfs_error(sb, "Failed to empty journal $LogFile%s",
					es);
			NVolSetErrors(vol);
			return -EROFS;
		}
		if (!ntfs_mark_quotas_out_of_date(vol)) {
			ntfs_error(sb, "Failed to mark quotas out of date%s",
					es);
			NVolSetErrors(vol);
			return -EROFS;
		}
		if (!ntfs_stamp_usnjrnl(vol)) {
			ntfs_error(sb, "Failed to stamp transation log "
					"($UsnJrnl)%s", es);
			NVolSetErrors(vol);
			return -EROFS;
		}
	} else if (!(sb->s_flags & MS_RDONLY) && (*flags & MS_RDONLY)) {
		
		if (!NVolErrors(vol)) {
			if (ntfs_clear_volume_flags(vol, VOLUME_IS_DIRTY))
				ntfs_warning(sb, "Failed to clear dirty bit "
						"in volume information "
						"flags.  Run chkdsk.");
		}
	}
#endif 

	

	if (!parse_options(vol, opt))
		return -EINVAL;

	ntfs_debug("Done.");
	return 0;
}

static bool is_boot_sector_ntfs(const struct super_block *sb,
		const NTFS_BOOT_SECTOR *b, const bool silent)
{
	if ((void*)b < (void*)&b->checksum && b->checksum && !silent) {
		le32 *u;
		u32 i;

		for (i = 0, u = (le32*)b; u < (le32*)(&b->checksum); ++u)
			i += le32_to_cpup(u);
		if (le32_to_cpu(b->checksum) != i)
			ntfs_warning(sb, "Invalid boot sector checksum.");
	}
	
	if (b->oem_id != magicNTFS)
		goto not_ntfs;
	
	if (le16_to_cpu(b->bpb.bytes_per_sector) < 0x100 ||
			le16_to_cpu(b->bpb.bytes_per_sector) > 0x1000)
		goto not_ntfs;
	
	switch (b->bpb.sectors_per_cluster) {
	case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128:
		break;
	default:
		goto not_ntfs;
	}
	
	if ((u32)le16_to_cpu(b->bpb.bytes_per_sector) *
			b->bpb.sectors_per_cluster > NTFS_MAX_CLUSTER_SIZE)
		goto not_ntfs;
	
	if (le16_to_cpu(b->bpb.reserved_sectors) ||
			le16_to_cpu(b->bpb.root_entries) ||
			le16_to_cpu(b->bpb.sectors) ||
			le16_to_cpu(b->bpb.sectors_per_fat) ||
			le32_to_cpu(b->bpb.large_sectors) || b->bpb.fats)
		goto not_ntfs;
	
	if ((u8)b->clusters_per_mft_record < 0xe1 ||
			(u8)b->clusters_per_mft_record > 0xf7)
		switch (b->clusters_per_mft_record) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			goto not_ntfs;
		}
	
	if ((u8)b->clusters_per_index_record < 0xe1 ||
			(u8)b->clusters_per_index_record > 0xf7)
		switch (b->clusters_per_index_record) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			goto not_ntfs;
		}
	if (!silent && b->end_of_sector_marker != cpu_to_le16(0xaa55))
		ntfs_warning(sb, "Invalid end of sector marker.");
	return true;
not_ntfs:
	return false;
}

static struct buffer_head *read_ntfs_boot_sector(struct super_block *sb,
		const int silent)
{
	const char *read_err_str = "Unable to read %s boot sector.";
	struct buffer_head *bh_primary, *bh_backup;
	sector_t nr_blocks = NTFS_SB(sb)->nr_blocks;

	
	if ((bh_primary = sb_bread(sb, 0))) {
		if (is_boot_sector_ntfs(sb, (NTFS_BOOT_SECTOR*)
				bh_primary->b_data, silent))
			return bh_primary;
		if (!silent)
			ntfs_error(sb, "Primary boot sector is invalid.");
	} else if (!silent)
		ntfs_error(sb, read_err_str, "primary");
	if (!(NTFS_SB(sb)->on_errors & ON_ERRORS_RECOVER)) {
		if (bh_primary)
			brelse(bh_primary);
		if (!silent)
			ntfs_error(sb, "Mount option errors=recover not used. "
					"Aborting without trying to recover.");
		return NULL;
	}
	
	if ((bh_backup = sb_bread(sb, nr_blocks - 1))) {
		if (is_boot_sector_ntfs(sb, (NTFS_BOOT_SECTOR*)
				bh_backup->b_data, silent))
			goto hotfix_primary_boot_sector;
		brelse(bh_backup);
	} else if (!silent)
		ntfs_error(sb, read_err_str, "backup");
	
	if ((bh_backup = sb_bread(sb, nr_blocks >> 1))) {
		if (is_boot_sector_ntfs(sb, (NTFS_BOOT_SECTOR*)
				bh_backup->b_data, silent))
			goto hotfix_primary_boot_sector;
		if (!silent)
			ntfs_error(sb, "Could not find a valid backup boot "
					"sector.");
		brelse(bh_backup);
	} else if (!silent)
		ntfs_error(sb, read_err_str, "backup");
	
	if (bh_primary)
		brelse(bh_primary);
	return NULL;
hotfix_primary_boot_sector:
	if (bh_primary) {
		if (!(sb->s_flags & MS_RDONLY)) {
			ntfs_warning(sb, "Hot-fix: Recovering invalid primary "
					"boot sector from backup copy.");
			memcpy(bh_primary->b_data, bh_backup->b_data,
					NTFS_BLOCK_SIZE);
			mark_buffer_dirty(bh_primary);
			sync_dirty_buffer(bh_primary);
			if (buffer_uptodate(bh_primary)) {
				brelse(bh_backup);
				return bh_primary;
			}
			ntfs_error(sb, "Hot-fix: Device write error while "
					"recovering primary boot sector.");
		} else {
			ntfs_warning(sb, "Hot-fix: Recovery of primary boot "
					"sector failed: Read-only mount.");
		}
		brelse(bh_primary);
	}
	ntfs_warning(sb, "Using backup boot sector.");
	return bh_backup;
}

static bool parse_ntfs_boot_sector(ntfs_volume *vol, const NTFS_BOOT_SECTOR *b)
{
	unsigned int sectors_per_cluster_bits, nr_hidden_sects;
	int clusters_per_mft_record, clusters_per_index_record;
	s64 ll;

	vol->sector_size = le16_to_cpu(b->bpb.bytes_per_sector);
	vol->sector_size_bits = ffs(vol->sector_size) - 1;
	ntfs_debug("vol->sector_size = %i (0x%x)", vol->sector_size,
			vol->sector_size);
	ntfs_debug("vol->sector_size_bits = %i (0x%x)", vol->sector_size_bits,
			vol->sector_size_bits);
	if (vol->sector_size < vol->sb->s_blocksize) {
		ntfs_error(vol->sb, "Sector size (%i) is smaller than the "
				"device block size (%lu).  This is not "
				"supported.  Sorry.", vol->sector_size,
				vol->sb->s_blocksize);
		return false;
	}
	ntfs_debug("sectors_per_cluster = 0x%x", b->bpb.sectors_per_cluster);
	sectors_per_cluster_bits = ffs(b->bpb.sectors_per_cluster) - 1;
	ntfs_debug("sectors_per_cluster_bits = 0x%x",
			sectors_per_cluster_bits);
	nr_hidden_sects = le32_to_cpu(b->bpb.hidden_sectors);
	ntfs_debug("number of hidden sectors = 0x%x", nr_hidden_sects);
	vol->cluster_size = vol->sector_size << sectors_per_cluster_bits;
	vol->cluster_size_mask = vol->cluster_size - 1;
	vol->cluster_size_bits = ffs(vol->cluster_size) - 1;
	ntfs_debug("vol->cluster_size = %i (0x%x)", vol->cluster_size,
			vol->cluster_size);
	ntfs_debug("vol->cluster_size_mask = 0x%x", vol->cluster_size_mask);
	ntfs_debug("vol->cluster_size_bits = %i", vol->cluster_size_bits);
	if (vol->cluster_size < vol->sector_size) {
		ntfs_error(vol->sb, "Cluster size (%i) is smaller than the "
				"sector size (%i).  This is not supported.  "
				"Sorry.", vol->cluster_size, vol->sector_size);
		return false;
	}
	clusters_per_mft_record = b->clusters_per_mft_record;
	ntfs_debug("clusters_per_mft_record = %i (0x%x)",
			clusters_per_mft_record, clusters_per_mft_record);
	if (clusters_per_mft_record > 0)
		vol->mft_record_size = vol->cluster_size <<
				(ffs(clusters_per_mft_record) - 1);
	else
		vol->mft_record_size = 1 << -clusters_per_mft_record;
	vol->mft_record_size_mask = vol->mft_record_size - 1;
	vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;
	ntfs_debug("vol->mft_record_size = %i (0x%x)", vol->mft_record_size,
			vol->mft_record_size);
	ntfs_debug("vol->mft_record_size_mask = 0x%x",
			vol->mft_record_size_mask);
	ntfs_debug("vol->mft_record_size_bits = %i (0x%x)",
			vol->mft_record_size_bits, vol->mft_record_size_bits);
	if (vol->mft_record_size > PAGE_CACHE_SIZE) {
		ntfs_error(vol->sb, "Mft record size (%i) exceeds the "
				"PAGE_CACHE_SIZE on your system (%lu).  "
				"This is not supported.  Sorry.",
				vol->mft_record_size, PAGE_CACHE_SIZE);
		return false;
	}
	
	if (vol->mft_record_size < vol->sector_size) {
		ntfs_error(vol->sb, "Mft record size (%i) is smaller than the "
				"sector size (%i).  This is not supported.  "
				"Sorry.", vol->mft_record_size,
				vol->sector_size);
		return false;
	}
	clusters_per_index_record = b->clusters_per_index_record;
	ntfs_debug("clusters_per_index_record = %i (0x%x)",
			clusters_per_index_record, clusters_per_index_record);
	if (clusters_per_index_record > 0)
		vol->index_record_size = vol->cluster_size <<
				(ffs(clusters_per_index_record) - 1);
	else
		vol->index_record_size = 1 << -clusters_per_index_record;
	vol->index_record_size_mask = vol->index_record_size - 1;
	vol->index_record_size_bits = ffs(vol->index_record_size) - 1;
	ntfs_debug("vol->index_record_size = %i (0x%x)",
			vol->index_record_size, vol->index_record_size);
	ntfs_debug("vol->index_record_size_mask = 0x%x",
			vol->index_record_size_mask);
	ntfs_debug("vol->index_record_size_bits = %i (0x%x)",
			vol->index_record_size_bits,
			vol->index_record_size_bits);
	
	if (vol->index_record_size < vol->sector_size) {
		ntfs_error(vol->sb, "Index record size (%i) is smaller than "
				"the sector size (%i).  This is not "
				"supported.  Sorry.", vol->index_record_size,
				vol->sector_size);
		return false;
	}
	ll = sle64_to_cpu(b->number_of_sectors) >> sectors_per_cluster_bits;
	if ((u64)ll >= 1ULL << 32) {
		ntfs_error(vol->sb, "Cannot handle 64-bit clusters.  Sorry.");
		return false;
	}
	vol->nr_clusters = ll;
	ntfs_debug("vol->nr_clusters = 0x%llx", (long long)vol->nr_clusters);
	if (sizeof(unsigned long) < 8) {
		if ((ll << vol->cluster_size_bits) >= (1ULL << 41)) {
			ntfs_error(vol->sb, "Volume size (%lluTiB) is too "
					"large for this architecture.  "
					"Maximum supported is 2TiB.  Sorry.",
					(unsigned long long)ll >> (40 -
					vol->cluster_size_bits));
			return false;
		}
	}
	ll = sle64_to_cpu(b->mft_lcn);
	if (ll >= vol->nr_clusters) {
		ntfs_error(vol->sb, "MFT LCN (%lli, 0x%llx) is beyond end of "
				"volume.  Weird.", (unsigned long long)ll,
				(unsigned long long)ll);
		return false;
	}
	vol->mft_lcn = ll;
	ntfs_debug("vol->mft_lcn = 0x%llx", (long long)vol->mft_lcn);
	ll = sle64_to_cpu(b->mftmirr_lcn);
	if (ll >= vol->nr_clusters) {
		ntfs_error(vol->sb, "MFTMirr LCN (%lli, 0x%llx) is beyond end "
				"of volume.  Weird.", (unsigned long long)ll,
				(unsigned long long)ll);
		return false;
	}
	vol->mftmirr_lcn = ll;
	ntfs_debug("vol->mftmirr_lcn = 0x%llx", (long long)vol->mftmirr_lcn);
#ifdef NTFS_RW
	if (vol->cluster_size <= (4 << vol->mft_record_size_bits))
		vol->mftmirr_size = 4;
	else
		vol->mftmirr_size = vol->cluster_size >>
				vol->mft_record_size_bits;
	ntfs_debug("vol->mftmirr_size = %i", vol->mftmirr_size);
#endif 
	vol->serial_no = le64_to_cpu(b->volume_serial_number);
	ntfs_debug("vol->serial_no = 0x%llx",
			(unsigned long long)vol->serial_no);
	return true;
}

static void ntfs_setup_allocators(ntfs_volume *vol)
{
#ifdef NTFS_RW
	LCN mft_zone_size, mft_lcn;
#endif 

	ntfs_debug("vol->mft_zone_multiplier = 0x%x",
			vol->mft_zone_multiplier);
#ifdef NTFS_RW
	
	mft_zone_size = vol->nr_clusters;
	switch (vol->mft_zone_multiplier) {  
	case 4:
		mft_zone_size >>= 1;			
		break;
	case 3:
		mft_zone_size = (mft_zone_size +
				(mft_zone_size >> 1)) >> 2;	
		break;
	case 2:
		mft_zone_size >>= 2;			
		break;
	
	default:
		mft_zone_size >>= 3;			
		break;
	}
	
	vol->mft_zone_start = vol->mft_zone_pos = vol->mft_lcn;
	ntfs_debug("vol->mft_zone_pos = 0x%llx",
			(unsigned long long)vol->mft_zone_pos);
	mft_lcn = (8192 + 2 * vol->cluster_size - 1) / vol->cluster_size;
	if (mft_lcn * vol->cluster_size < 16 * 1024)
		mft_lcn = (16 * 1024 + vol->cluster_size - 1) /
				vol->cluster_size;
	if (vol->mft_zone_start <= mft_lcn)
		vol->mft_zone_start = 0;
	ntfs_debug("vol->mft_zone_start = 0x%llx",
			(unsigned long long)vol->mft_zone_start);
	vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	while (vol->mft_zone_end >= vol->nr_clusters) {
		mft_zone_size >>= 1;
		vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	}
	ntfs_debug("vol->mft_zone_end = 0x%llx",
			(unsigned long long)vol->mft_zone_end);
	vol->data1_zone_pos = vol->mft_zone_end;
	ntfs_debug("vol->data1_zone_pos = 0x%llx",
			(unsigned long long)vol->data1_zone_pos);
	vol->data2_zone_pos = 0;
	ntfs_debug("vol->data2_zone_pos = 0x%llx",
			(unsigned long long)vol->data2_zone_pos);

	
	vol->mft_data_pos = 24;
	ntfs_debug("vol->mft_data_pos = 0x%llx",
			(unsigned long long)vol->mft_data_pos);
#endif 
}

#ifdef NTFS_RW

static bool load_and_init_mft_mirror(ntfs_volume *vol)
{
	struct inode *tmp_ino;
	ntfs_inode *tmp_ni;

	ntfs_debug("Entering.");
	
	tmp_ino = ntfs_iget(vol->sb, FILE_MFTMirr);
	if (IS_ERR(tmp_ino) || is_bad_inode(tmp_ino)) {
		if (!IS_ERR(tmp_ino))
			iput(tmp_ino);
		
		return false;
	}
	
	tmp_ino->i_uid = tmp_ino->i_gid = 0;
	
	tmp_ino->i_mode = S_IFREG;
	
	tmp_ino->i_op = &ntfs_empty_inode_ops;
	tmp_ino->i_fop = &ntfs_empty_file_ops;
	
	tmp_ino->i_mapping->a_ops = &ntfs_mst_aops;
	tmp_ni = NTFS_I(tmp_ino);
	
	NInoSetMstProtected(tmp_ni);
	NInoSetSparseDisabled(tmp_ni);
	tmp_ni->itype.index.block_size = vol->mft_record_size;
	tmp_ni->itype.index.block_size_bits = vol->mft_record_size_bits;
	vol->mftmirr_ino = tmp_ino;
	ntfs_debug("Done.");
	return true;
}

static bool check_mft_mirror(ntfs_volume *vol)
{
	struct super_block *sb = vol->sb;
	ntfs_inode *mirr_ni;
	struct page *mft_page, *mirr_page;
	u8 *kmft, *kmirr;
	runlist_element *rl, rl2[2];
	pgoff_t index;
	int mrecs_per_page, i;

	ntfs_debug("Entering.");
	
	mrecs_per_page = PAGE_CACHE_SIZE / vol->mft_record_size;
	BUG_ON(!mrecs_per_page);
	BUG_ON(!vol->mftmirr_size);
	mft_page = mirr_page = NULL;
	kmft = kmirr = NULL;
	index = i = 0;
	do {
		u32 bytes;

		
		if (!(i % mrecs_per_page)) {
			if (index) {
				ntfs_unmap_page(mft_page);
				ntfs_unmap_page(mirr_page);
			}
			
			mft_page = ntfs_map_page(vol->mft_ino->i_mapping,
					index);
			if (IS_ERR(mft_page)) {
				ntfs_error(sb, "Failed to read $MFT.");
				return false;
			}
			kmft = page_address(mft_page);
			
			mirr_page = ntfs_map_page(vol->mftmirr_ino->i_mapping,
					index);
			if (IS_ERR(mirr_page)) {
				ntfs_error(sb, "Failed to read $MFTMirr.");
				goto mft_unmap_out;
			}
			kmirr = page_address(mirr_page);
			++index;
		}
		
		if (((MFT_RECORD*)kmft)->flags & MFT_RECORD_IN_USE) {
			
			if (ntfs_is_baad_recordp((le32*)kmft)) {
				ntfs_error(sb, "Incomplete multi sector "
						"transfer detected in mft "
						"record %i.", i);
mm_unmap_out:
				ntfs_unmap_page(mirr_page);
mft_unmap_out:
				ntfs_unmap_page(mft_page);
				return false;
			}
		}
		
		if (((MFT_RECORD*)kmirr)->flags & MFT_RECORD_IN_USE) {
			if (ntfs_is_baad_recordp((le32*)kmirr)) {
				ntfs_error(sb, "Incomplete multi sector "
						"transfer detected in mft "
						"mirror record %i.", i);
				goto mm_unmap_out;
			}
		}
		
		bytes = le32_to_cpu(((MFT_RECORD*)kmft)->bytes_in_use);
		if (bytes < sizeof(MFT_RECORD_OLD) ||
				bytes > vol->mft_record_size ||
				ntfs_is_baad_recordp((le32*)kmft)) {
			bytes = le32_to_cpu(((MFT_RECORD*)kmirr)->bytes_in_use);
			if (bytes < sizeof(MFT_RECORD_OLD) ||
					bytes > vol->mft_record_size ||
					ntfs_is_baad_recordp((le32*)kmirr))
				bytes = vol->mft_record_size;
		}
		
		if (memcmp(kmft, kmirr, bytes)) {
			ntfs_error(sb, "$MFT and $MFTMirr (record %i) do not "
					"match.  Run ntfsfix or chkdsk.", i);
			goto mm_unmap_out;
		}
		kmft += vol->mft_record_size;
		kmirr += vol->mft_record_size;
	} while (++i < vol->mftmirr_size);
	
	ntfs_unmap_page(mft_page);
	ntfs_unmap_page(mirr_page);

	
	rl2[0].vcn = 0;
	rl2[0].lcn = vol->mftmirr_lcn;
	rl2[0].length = (vol->mftmirr_size * vol->mft_record_size +
			vol->cluster_size - 1) / vol->cluster_size;
	rl2[1].vcn = rl2[0].length;
	rl2[1].lcn = LCN_ENOENT;
	rl2[1].length = 0;
	mirr_ni = NTFS_I(vol->mftmirr_ino);
	down_read(&mirr_ni->runlist.lock);
	rl = mirr_ni->runlist.rl;
	
	i = 0;
	do {
		if (rl2[i].vcn != rl[i].vcn || rl2[i].lcn != rl[i].lcn ||
				rl2[i].length != rl[i].length) {
			ntfs_error(sb, "$MFTMirr location mismatch.  "
					"Run chkdsk.");
			up_read(&mirr_ni->runlist.lock);
			return false;
		}
	} while (rl2[i++].length);
	up_read(&mirr_ni->runlist.lock);
	ntfs_debug("Done.");
	return true;
}

static bool load_and_check_logfile(ntfs_volume *vol,
		RESTART_PAGE_HEADER **rp)
{
	struct inode *tmp_ino;

	ntfs_debug("Entering.");
	tmp_ino = ntfs_iget(vol->sb, FILE_LogFile);
	if (IS_ERR(tmp_ino) || is_bad_inode(tmp_ino)) {
		if (!IS_ERR(tmp_ino))
			iput(tmp_ino);
		
		return false;
	}
	if (!ntfs_check_logfile(tmp_ino, rp)) {
		iput(tmp_ino);
		
		return false;
	}
	NInoSetSparseDisabled(NTFS_I(tmp_ino));
	vol->logfile_ino = tmp_ino;
	ntfs_debug("Done.");
	return true;
}

#define NTFS_HIBERFIL_HEADER_SIZE	4096

static int check_windows_hibernation_status(ntfs_volume *vol)
{
	MFT_REF mref;
	struct inode *vi;
	struct page *page;
	u32 *kaddr, *kend;
	ntfs_name *name = NULL;
	int ret = 1;
	static const ntfschar hiberfil[13] = { cpu_to_le16('h'),
			cpu_to_le16('i'), cpu_to_le16('b'),
			cpu_to_le16('e'), cpu_to_le16('r'),
			cpu_to_le16('f'), cpu_to_le16('i'),
			cpu_to_le16('l'), cpu_to_le16('.'),
			cpu_to_le16('s'), cpu_to_le16('y'),
			cpu_to_le16('s'), 0 };

	ntfs_debug("Entering.");
	mutex_lock(&vol->root_ino->i_mutex);
	mref = ntfs_lookup_inode_by_name(NTFS_I(vol->root_ino), hiberfil, 12,
			&name);
	mutex_unlock(&vol->root_ino->i_mutex);
	if (IS_ERR_MREF(mref)) {
		ret = MREF_ERR(mref);
		
		if (ret == -ENOENT) {
			ntfs_debug("hiberfil.sys not present.  Windows is not "
					"hibernated on the volume.");
			return 0;
		}
		
		ntfs_error(vol->sb, "Failed to find inode number for "
				"hiberfil.sys.");
		return ret;
	}
	
	kfree(name);
	
	vi = ntfs_iget(vol->sb, MREF(mref));
	if (IS_ERR(vi) || is_bad_inode(vi)) {
		if (!IS_ERR(vi))
			iput(vi);
		ntfs_error(vol->sb, "Failed to load hiberfil.sys.");
		return IS_ERR(vi) ? PTR_ERR(vi) : -EIO;
	}
	if (unlikely(i_size_read(vi) < NTFS_HIBERFIL_HEADER_SIZE)) {
		ntfs_debug("hiberfil.sys is smaller than 4kiB (0x%llx).  "
				"Windows is hibernated on the volume.  This "
				"is not the system volume.", i_size_read(vi));
		goto iput_out;
	}
	page = ntfs_map_page(vi->i_mapping, 0);
	if (IS_ERR(page)) {
		ntfs_error(vol->sb, "Failed to read from hiberfil.sys.");
		ret = PTR_ERR(page);
		goto iput_out;
	}
	kaddr = (u32*)page_address(page);
	if (*(le32*)kaddr == cpu_to_le32(0x72626968)) {
		ntfs_debug("Magic \"hibr\" found in hiberfil.sys.  Windows is "
				"hibernated on the volume.  This is the "
				"system volume.");
		goto unm_iput_out;
	}
	kend = kaddr + NTFS_HIBERFIL_HEADER_SIZE/sizeof(*kaddr);
	do {
		if (unlikely(*kaddr)) {
			ntfs_debug("hiberfil.sys is larger than 4kiB "
					"(0x%llx), does not contain the "
					"\"hibr\" magic, and does not have a "
					"zero header.  Windows is hibernated "
					"on the volume.  This is not the "
					"system volume.", i_size_read(vi));
			goto unm_iput_out;
		}
	} while (++kaddr < kend);
	ntfs_debug("hiberfil.sys contains a zero header.  Windows is not "
			"hibernated on the volume.  This is the system "
			"volume.");
	ret = 0;
unm_iput_out:
	ntfs_unmap_page(page);
iput_out:
	iput(vi);
	return ret;
}

static bool load_and_init_quota(ntfs_volume *vol)
{
	MFT_REF mref;
	struct inode *tmp_ino;
	ntfs_name *name = NULL;
	static const ntfschar Quota[7] = { cpu_to_le16('$'),
			cpu_to_le16('Q'), cpu_to_le16('u'),
			cpu_to_le16('o'), cpu_to_le16('t'),
			cpu_to_le16('a'), 0 };
	static ntfschar Q[3] = { cpu_to_le16('$'),
			cpu_to_le16('Q'), 0 };

	ntfs_debug("Entering.");
	mutex_lock(&vol->extend_ino->i_mutex);
	mref = ntfs_lookup_inode_by_name(NTFS_I(vol->extend_ino), Quota, 6,
			&name);
	mutex_unlock(&vol->extend_ino->i_mutex);
	if (IS_ERR_MREF(mref)) {
		if (MREF_ERR(mref) == -ENOENT) {
			ntfs_debug("$Quota not present.  Volume does not have "
					"quotas enabled.");
			NVolSetQuotaOutOfDate(vol);
			return true;
		}
		
		ntfs_error(vol->sb, "Failed to find inode number for $Quota.");
		return false;
	}
	
	kfree(name);
	
	tmp_ino = ntfs_iget(vol->sb, MREF(mref));
	if (IS_ERR(tmp_ino) || is_bad_inode(tmp_ino)) {
		if (!IS_ERR(tmp_ino))
			iput(tmp_ino);
		ntfs_error(vol->sb, "Failed to load $Quota.");
		return false;
	}
	vol->quota_ino = tmp_ino;
	
	tmp_ino = ntfs_index_iget(vol->quota_ino, Q, 2);
	if (IS_ERR(tmp_ino)) {
		ntfs_error(vol->sb, "Failed to load $Quota/$Q index.");
		return false;
	}
	vol->quota_q_ino = tmp_ino;
	ntfs_debug("Done.");
	return true;
}

static bool load_and_init_usnjrnl(ntfs_volume *vol)
{
	MFT_REF mref;
	struct inode *tmp_ino;
	ntfs_inode *tmp_ni;
	struct page *page;
	ntfs_name *name = NULL;
	USN_HEADER *uh;
	static const ntfschar UsnJrnl[9] = { cpu_to_le16('$'),
			cpu_to_le16('U'), cpu_to_le16('s'),
			cpu_to_le16('n'), cpu_to_le16('J'),
			cpu_to_le16('r'), cpu_to_le16('n'),
			cpu_to_le16('l'), 0 };
	static ntfschar Max[5] = { cpu_to_le16('$'),
			cpu_to_le16('M'), cpu_to_le16('a'),
			cpu_to_le16('x'), 0 };
	static ntfschar J[3] = { cpu_to_le16('$'),
			cpu_to_le16('J'), 0 };

	ntfs_debug("Entering.");
	mutex_lock(&vol->extend_ino->i_mutex);
	mref = ntfs_lookup_inode_by_name(NTFS_I(vol->extend_ino), UsnJrnl, 8,
			&name);
	mutex_unlock(&vol->extend_ino->i_mutex);
	if (IS_ERR_MREF(mref)) {
		if (MREF_ERR(mref) == -ENOENT) {
			ntfs_debug("$UsnJrnl not present.  Volume does not "
					"have transaction logging enabled.");
not_enabled:
			NVolSetUsnJrnlStamped(vol);
			return true;
		}
		
		ntfs_error(vol->sb, "Failed to find inode number for "
				"$UsnJrnl.");
		return false;
	}
	
	kfree(name);
	
	tmp_ino = ntfs_iget(vol->sb, MREF(mref));
	if (unlikely(IS_ERR(tmp_ino) || is_bad_inode(tmp_ino))) {
		if (!IS_ERR(tmp_ino))
			iput(tmp_ino);
		ntfs_error(vol->sb, "Failed to load $UsnJrnl.");
		return false;
	}
	vol->usnjrnl_ino = tmp_ino;
	if (unlikely(vol->vol_flags & VOLUME_DELETE_USN_UNDERWAY)) {
		ntfs_debug("$UsnJrnl in the process of being disabled.  "
				"Volume does not have transaction logging "
				"enabled.");
		goto not_enabled;
	}
	
	tmp_ino = ntfs_attr_iget(vol->usnjrnl_ino, AT_DATA, Max, 4);
	if (IS_ERR(tmp_ino)) {
		ntfs_error(vol->sb, "Failed to load $UsnJrnl/$DATA/$Max "
				"attribute.");
		return false;
	}
	vol->usnjrnl_max_ino = tmp_ino;
	if (unlikely(i_size_read(tmp_ino) < sizeof(USN_HEADER))) {
		ntfs_error(vol->sb, "Found corrupt $UsnJrnl/$DATA/$Max "
				"attribute (size is 0x%llx but should be at "
				"least 0x%zx bytes).", i_size_read(tmp_ino),
				sizeof(USN_HEADER));
		return false;
	}
	
	tmp_ino = ntfs_attr_iget(vol->usnjrnl_ino, AT_DATA, J, 2);
	if (IS_ERR(tmp_ino)) {
		ntfs_error(vol->sb, "Failed to load $UsnJrnl/$DATA/$J "
				"attribute.");
		return false;
	}
	vol->usnjrnl_j_ino = tmp_ino;
	
	tmp_ni = NTFS_I(vol->usnjrnl_j_ino);
	if (unlikely(!NInoNonResident(tmp_ni) || !NInoSparse(tmp_ni))) {
		ntfs_error(vol->sb, "$UsnJrnl/$DATA/$J attribute is resident "
				"and/or not sparse.");
		return false;
	}
	
	page = ntfs_map_page(vol->usnjrnl_max_ino->i_mapping, 0);
	if (IS_ERR(page)) {
		ntfs_error(vol->sb, "Failed to read from $UsnJrnl/$DATA/$Max "
				"attribute.");
		return false;
	}
	uh = (USN_HEADER*)page_address(page);
	
	if (unlikely(sle64_to_cpu(uh->allocation_delta) >
			sle64_to_cpu(uh->maximum_size))) {
		ntfs_error(vol->sb, "Allocation delta (0x%llx) exceeds "
				"maximum size (0x%llx).  $UsnJrnl is corrupt.",
				(long long)sle64_to_cpu(uh->allocation_delta),
				(long long)sle64_to_cpu(uh->maximum_size));
		ntfs_unmap_page(page);
		return false;
	}
	/*
	 * If the transaction log has been stamped and nothing has been written
	 * to it since, we do not need to stamp it.
	 */
	if (unlikely(sle64_to_cpu(uh->lowest_valid_usn) >=
			i_size_read(vol->usnjrnl_j_ino))) {
		if (likely(sle64_to_cpu(uh->lowest_valid_usn) ==
				i_size_read(vol->usnjrnl_j_ino))) {
			ntfs_unmap_page(page);
			ntfs_debug("$UsnJrnl is enabled but nothing has been "
					"logged since it was last stamped.  "
					"Treating this as if the volume does "
					"not have transaction logging "
					"enabled.");
			goto not_enabled;
		}
		ntfs_error(vol->sb, "$UsnJrnl has lowest valid usn (0x%llx) "
				"which is out of bounds (0x%llx).  $UsnJrnl "
				"is corrupt.",
				(long long)sle64_to_cpu(uh->lowest_valid_usn),
				i_size_read(vol->usnjrnl_j_ino));
		ntfs_unmap_page(page);
		return false;
	}
	ntfs_unmap_page(page);
	ntfs_debug("Done.");
	return true;
}

static bool load_and_init_attrdef(ntfs_volume *vol)
{
	loff_t i_size;
	struct super_block *sb = vol->sb;
	struct inode *ino;
	struct page *page;
	pgoff_t index, max_index;
	unsigned int size;

	ntfs_debug("Entering.");
	
	ino = ntfs_iget(sb, FILE_AttrDef);
	if (IS_ERR(ino) || is_bad_inode(ino)) {
		if (!IS_ERR(ino))
			iput(ino);
		goto failed;
	}
	NInoSetSparseDisabled(NTFS_I(ino));
	
	i_size = i_size_read(ino);
	if (i_size <= 0 || i_size > 0x7fffffff)
		goto iput_failed;
	vol->attrdef = (ATTR_DEF*)ntfs_malloc_nofs(i_size);
	if (!vol->attrdef)
		goto iput_failed;
	index = 0;
	max_index = i_size >> PAGE_CACHE_SHIFT;
	size = PAGE_CACHE_SIZE;
	while (index < max_index) {
		
read_partial_attrdef_page:
		page = ntfs_map_page(ino->i_mapping, index);
		if (IS_ERR(page))
			goto free_iput_failed;
		memcpy((u8*)vol->attrdef + (index++ << PAGE_CACHE_SHIFT),
				page_address(page), size);
		ntfs_unmap_page(page);
	};
	if (size == PAGE_CACHE_SIZE) {
		size = i_size & ~PAGE_CACHE_MASK;
		if (size)
			goto read_partial_attrdef_page;
	}
	vol->attrdef_size = i_size;
	ntfs_debug("Read %llu bytes from $AttrDef.", i_size);
	iput(ino);
	return true;
free_iput_failed:
	ntfs_free(vol->attrdef);
	vol->attrdef = NULL;
iput_failed:
	iput(ino);
failed:
	ntfs_error(sb, "Failed to initialize attribute definition table.");
	return false;
}

#endif 

static bool load_and_init_upcase(ntfs_volume *vol)
{
	loff_t i_size;
	struct super_block *sb = vol->sb;
	struct inode *ino;
	struct page *page;
	pgoff_t index, max_index;
	unsigned int size;
	int i, max;

	ntfs_debug("Entering.");
	
	ino = ntfs_iget(sb, FILE_UpCase);
	if (IS_ERR(ino) || is_bad_inode(ino)) {
		if (!IS_ERR(ino))
			iput(ino);
		goto upcase_failed;
	}
	i_size = i_size_read(ino);
	if (!i_size || i_size & (sizeof(ntfschar) - 1) ||
			i_size > 64ULL * 1024 * sizeof(ntfschar))
		goto iput_upcase_failed;
	vol->upcase = (ntfschar*)ntfs_malloc_nofs(i_size);
	if (!vol->upcase)
		goto iput_upcase_failed;
	index = 0;
	max_index = i_size >> PAGE_CACHE_SHIFT;
	size = PAGE_CACHE_SIZE;
	while (index < max_index) {
		
read_partial_upcase_page:
		page = ntfs_map_page(ino->i_mapping, index);
		if (IS_ERR(page))
			goto iput_upcase_failed;
		memcpy((char*)vol->upcase + (index++ << PAGE_CACHE_SHIFT),
				page_address(page), size);
		ntfs_unmap_page(page);
	};
	if (size == PAGE_CACHE_SIZE) {
		size = i_size & ~PAGE_CACHE_MASK;
		if (size)
			goto read_partial_upcase_page;
	}
	vol->upcase_len = i_size >> UCHAR_T_SIZE_BITS;
	ntfs_debug("Read %llu bytes from $UpCase (expected %zu bytes).",
			i_size, 64 * 1024 * sizeof(ntfschar));
	iput(ino);
	mutex_lock(&ntfs_lock);
	if (!default_upcase) {
		ntfs_debug("Using volume specified $UpCase since default is "
				"not present.");
		mutex_unlock(&ntfs_lock);
		return true;
	}
	max = default_upcase_len;
	if (max > vol->upcase_len)
		max = vol->upcase_len;
	for (i = 0; i < max; i++)
		if (vol->upcase[i] != default_upcase[i])
			break;
	if (i == max) {
		ntfs_free(vol->upcase);
		vol->upcase = default_upcase;
		vol->upcase_len = max;
		ntfs_nr_upcase_users++;
		mutex_unlock(&ntfs_lock);
		ntfs_debug("Volume specified $UpCase matches default. Using "
				"default.");
		return true;
	}
	mutex_unlock(&ntfs_lock);
	ntfs_debug("Using volume specified $UpCase since it does not match "
			"the default.");
	return true;
iput_upcase_failed:
	iput(ino);
	ntfs_free(vol->upcase);
	vol->upcase = NULL;
upcase_failed:
	mutex_lock(&ntfs_lock);
	if (default_upcase) {
		vol->upcase = default_upcase;
		vol->upcase_len = default_upcase_len;
		ntfs_nr_upcase_users++;
		mutex_unlock(&ntfs_lock);
		ntfs_error(sb, "Failed to load $UpCase from the volume. Using "
				"default.");
		return true;
	}
	mutex_unlock(&ntfs_lock);
	ntfs_error(sb, "Failed to initialize upcase table.");
	return false;
}

static struct lock_class_key
	lcnbmp_runlist_lock_key, lcnbmp_mrec_lock_key,
	mftbmp_runlist_lock_key, mftbmp_mrec_lock_key;

static bool load_system_files(ntfs_volume *vol)
{
	struct super_block *sb = vol->sb;
	MFT_RECORD *m;
	VOLUME_INFORMATION *vi;
	ntfs_attr_search_ctx *ctx;
#ifdef NTFS_RW
	RESTART_PAGE_HEADER *rp;
	int err;
#endif 

	ntfs_debug("Entering.");
#ifdef NTFS_RW
	
	if (!load_and_init_mft_mirror(vol) || !check_mft_mirror(vol)) {
		static const char *es1 = "Failed to load $MFTMirr";
		static const char *es2 = "$MFTMirr does not match $MFT";
		static const char *es3 = ".  Run ntfsfix and/or chkdsk.";

		
		if (!(sb->s_flags & MS_RDONLY)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(sb, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						!vol->mftmirr_ino ? es1 : es2,
						es3);
				goto iput_mirr_err_out;
			}
			sb->s_flags |= MS_RDONLY;
			ntfs_error(sb, "%s.  Mounting read-only%s",
					!vol->mftmirr_ino ? es1 : es2, es3);
		} else
			ntfs_warning(sb, "%s.  Will not be able to remount "
					"read-write%s",
					!vol->mftmirr_ino ? es1 : es2, es3);
		
		NVolSetErrors(vol);
	}
#endif 
	
	vol->mftbmp_ino = ntfs_attr_iget(vol->mft_ino, AT_BITMAP, NULL, 0);
	if (IS_ERR(vol->mftbmp_ino)) {
		ntfs_error(sb, "Failed to load $MFT/$BITMAP attribute.");
		goto iput_mirr_err_out;
	}
	lockdep_set_class(&NTFS_I(vol->mftbmp_ino)->runlist.lock,
			   &mftbmp_runlist_lock_key);
	lockdep_set_class(&NTFS_I(vol->mftbmp_ino)->mrec_lock,
			   &mftbmp_mrec_lock_key);
	
	if (!load_and_init_upcase(vol))
		goto iput_mftbmp_err_out;
#ifdef NTFS_RW
	if (!load_and_init_attrdef(vol))
		goto iput_upcase_err_out;
#endif 
	vol->lcnbmp_ino = ntfs_iget(sb, FILE_Bitmap);
	if (IS_ERR(vol->lcnbmp_ino) || is_bad_inode(vol->lcnbmp_ino)) {
		if (!IS_ERR(vol->lcnbmp_ino))
			iput(vol->lcnbmp_ino);
		goto bitmap_failed;
	}
	lockdep_set_class(&NTFS_I(vol->lcnbmp_ino)->runlist.lock,
			   &lcnbmp_runlist_lock_key);
	lockdep_set_class(&NTFS_I(vol->lcnbmp_ino)->mrec_lock,
			   &lcnbmp_mrec_lock_key);

	NInoSetSparseDisabled(NTFS_I(vol->lcnbmp_ino));
	if ((vol->nr_clusters + 7) >> 3 > i_size_read(vol->lcnbmp_ino)) {
		iput(vol->lcnbmp_ino);
bitmap_failed:
		ntfs_error(sb, "Failed to load $Bitmap.");
		goto iput_attrdef_err_out;
	}
	vol->vol_ino = ntfs_iget(sb, FILE_Volume);
	if (IS_ERR(vol->vol_ino) || is_bad_inode(vol->vol_ino)) {
		if (!IS_ERR(vol->vol_ino))
			iput(vol->vol_ino);
volume_failed:
		ntfs_error(sb, "Failed to load $Volume.");
		goto iput_lcnbmp_err_out;
	}
	m = map_mft_record(NTFS_I(vol->vol_ino));
	if (IS_ERR(m)) {
iput_volume_failed:
		iput(vol->vol_ino);
		goto volume_failed;
	}
	if (!(ctx = ntfs_attr_get_search_ctx(NTFS_I(vol->vol_ino), m))) {
		ntfs_error(sb, "Failed to get attribute search context.");
		goto get_ctx_vol_failed;
	}
	if (ntfs_attr_lookup(AT_VOLUME_INFORMATION, NULL, 0, 0, 0, NULL, 0,
			ctx) || ctx->attr->non_resident || ctx->attr->flags) {
err_put_vol:
		ntfs_attr_put_search_ctx(ctx);
get_ctx_vol_failed:
		unmap_mft_record(NTFS_I(vol->vol_ino));
		goto iput_volume_failed;
	}
	vi = (VOLUME_INFORMATION*)((char*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset));
	
	if ((u8*)vi < (u8*)ctx->attr || (u8*)vi +
			le32_to_cpu(ctx->attr->data.resident.value_length) >
			(u8*)ctx->attr + le32_to_cpu(ctx->attr->length))
		goto err_put_vol;
	
	vol->vol_flags = vi->flags;
	vol->major_ver = vi->major_ver;
	vol->minor_ver = vi->minor_ver;
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(NTFS_I(vol->vol_ino));
	printk(KERN_INFO "NTFS volume version %i.%i.\n", vol->major_ver,
			vol->minor_ver);
	if (vol->major_ver < 3 && NVolSparseEnabled(vol)) {
		ntfs_warning(vol->sb, "Disabling sparse support due to NTFS "
				"volume version %i.%i (need at least version "
				"3.0).", vol->major_ver, vol->minor_ver);
		NVolClearSparseEnabled(vol);
	}
#ifdef NTFS_RW
	
	if (vol->vol_flags & VOLUME_MUST_MOUNT_RO_MASK) {
		static const char *es1a = "Volume is dirty";
		static const char *es1b = "Volume has been modified by chkdsk";
		static const char *es1c = "Volume has unsupported flags set";
		static const char *es2a = ".  Run chkdsk and mount in Windows.";
		static const char *es2b = ".  Mount in Windows.";
		const char *es1, *es2;

		es2 = es2a;
		if (vol->vol_flags & VOLUME_IS_DIRTY)
			es1 = es1a;
		else if (vol->vol_flags & VOLUME_MODIFIED_BY_CHKDSK) {
			es1 = es1b;
			es2 = es2b;
		} else {
			es1 = es1c;
			ntfs_warning(sb, "Unsupported volume flags 0x%x "
					"encountered.",
					(unsigned)le16_to_cpu(vol->vol_flags));
		}
		
		if (!(sb->s_flags & MS_RDONLY)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(sb, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto iput_vol_err_out;
			}
			sb->s_flags |= MS_RDONLY;
			ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		} else
			ntfs_warning(sb, "%s.  Will not be able to remount "
					"read-write%s", es1, es2);
	}
	rp = NULL;
	if (!load_and_check_logfile(vol, &rp) ||
			!ntfs_is_logfile_clean(vol->logfile_ino, rp)) {
		static const char *es1a = "Failed to load $LogFile";
		static const char *es1b = "$LogFile is not clean";
		static const char *es2 = ".  Mount in Windows.";
		const char *es1;

		es1 = !vol->logfile_ino ? es1a : es1b;
		
		if (!(sb->s_flags & MS_RDONLY)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(sb, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				if (vol->logfile_ino) {
					BUG_ON(!rp);
					ntfs_free(rp);
				}
				goto iput_logfile_err_out;
			}
			sb->s_flags |= MS_RDONLY;
			ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		} else
			ntfs_warning(sb, "%s.  Will not be able to remount "
					"read-write%s", es1, es2);
		
		NVolSetErrors(vol);
	}
	ntfs_free(rp);
#endif 
	
	vol->root_ino = ntfs_iget(sb, FILE_root);
	if (IS_ERR(vol->root_ino) || is_bad_inode(vol->root_ino)) {
		if (!IS_ERR(vol->root_ino))
			iput(vol->root_ino);
		ntfs_error(sb, "Failed to load root directory.");
		goto iput_logfile_err_out;
	}
#ifdef NTFS_RW
	err = check_windows_hibernation_status(vol);
	if (unlikely(err)) {
		static const char *es1a = "Failed to determine if Windows is "
				"hibernated";
		static const char *es1b = "Windows is hibernated";
		static const char *es2 = ".  Run chkdsk.";
		const char *es1;

		es1 = err < 0 ? es1a : es1b;
		
		if (!(sb->s_flags & MS_RDONLY)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(sb, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto iput_root_err_out;
			}
			sb->s_flags |= MS_RDONLY;
			ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		} else
			ntfs_warning(sb, "%s.  Will not be able to remount "
					"read-write%s", es1, es2);
		
		NVolSetErrors(vol);
	}
	
	if (!(sb->s_flags & MS_RDONLY) &&
			ntfs_set_volume_flags(vol, VOLUME_IS_DIRTY)) {
		static const char *es1 = "Failed to set dirty bit in volume "
				"information flags";
		static const char *es2 = ".  Run chkdsk.";

		
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(sb, "%s and neither on_errors=continue nor "
					"on_errors=remount-ro was specified%s",
					es1, es2);
			goto iput_root_err_out;
		}
		ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		sb->s_flags |= MS_RDONLY;
	}
#if 0
	
	
	if (!(sb->s_flags & MS_RDONLY) && (vol->major_ver > 1) &&
			ntfs_set_volume_flags(vol, VOLUME_MOUNTED_ON_NT4)) {
		static const char *es1 = "Failed to set NT4 compatibility flag";
		static const char *es2 = ".  Run chkdsk.";

		
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(sb, "%s and neither on_errors=continue nor "
					"on_errors=remount-ro was specified%s",
					es1, es2);
			goto iput_root_err_out;
		}
		ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		sb->s_flags |= MS_RDONLY;
		NVolSetErrors(vol);
	}
#endif
	
	if (!(sb->s_flags & MS_RDONLY) &&
			!ntfs_empty_logfile(vol->logfile_ino)) {
		static const char *es1 = "Failed to empty $LogFile";
		static const char *es2 = ".  Mount in Windows.";

		
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(sb, "%s and neither on_errors=continue nor "
					"on_errors=remount-ro was specified%s",
					es1, es2);
			goto iput_root_err_out;
		}
		ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		sb->s_flags |= MS_RDONLY;
		NVolSetErrors(vol);
	}
#endif 
	
	if (unlikely(vol->major_ver < 3))
		return true;
	
	
	vol->secure_ino = ntfs_iget(sb, FILE_Secure);
	if (IS_ERR(vol->secure_ino) || is_bad_inode(vol->secure_ino)) {
		if (!IS_ERR(vol->secure_ino))
			iput(vol->secure_ino);
		ntfs_error(sb, "Failed to load $Secure.");
		goto iput_root_err_out;
	}
	
	
	vol->extend_ino = ntfs_iget(sb, FILE_Extend);
	if (IS_ERR(vol->extend_ino) || is_bad_inode(vol->extend_ino)) {
		if (!IS_ERR(vol->extend_ino))
			iput(vol->extend_ino);
		ntfs_error(sb, "Failed to load $Extend.");
		goto iput_sec_err_out;
	}
#ifdef NTFS_RW
	
	if (!load_and_init_quota(vol)) {
		static const char *es1 = "Failed to load $Quota";
		static const char *es2 = ".  Run chkdsk.";

		
		if (!(sb->s_flags & MS_RDONLY)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(sb, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto iput_quota_err_out;
			}
			sb->s_flags |= MS_RDONLY;
			ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		} else
			ntfs_warning(sb, "%s.  Will not be able to remount "
					"read-write%s", es1, es2);
		
		NVolSetErrors(vol);
	}
	
	if (!(sb->s_flags & MS_RDONLY) &&
			!ntfs_mark_quotas_out_of_date(vol)) {
		static const char *es1 = "Failed to mark quotas out of date";
		static const char *es2 = ".  Run chkdsk.";

		
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(sb, "%s and neither on_errors=continue nor "
					"on_errors=remount-ro was specified%s",
					es1, es2);
			goto iput_quota_err_out;
		}
		ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		sb->s_flags |= MS_RDONLY;
		NVolSetErrors(vol);
	}
	if (!load_and_init_usnjrnl(vol)) {
		static const char *es1 = "Failed to load $UsnJrnl";
		static const char *es2 = ".  Run chkdsk.";

		
		if (!(sb->s_flags & MS_RDONLY)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(sb, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto iput_usnjrnl_err_out;
			}
			sb->s_flags |= MS_RDONLY;
			ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		} else
			ntfs_warning(sb, "%s.  Will not be able to remount "
					"read-write%s", es1, es2);
		
		NVolSetErrors(vol);
	}
	
	if (!(sb->s_flags & MS_RDONLY) && !ntfs_stamp_usnjrnl(vol)) {
		static const char *es1 = "Failed to stamp transaction log "
				"($UsnJrnl)";
		static const char *es2 = ".  Run chkdsk.";

		
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(sb, "%s and neither on_errors=continue nor "
					"on_errors=remount-ro was specified%s",
					es1, es2);
			goto iput_usnjrnl_err_out;
		}
		ntfs_error(sb, "%s.  Mounting read-only%s", es1, es2);
		sb->s_flags |= MS_RDONLY;
		NVolSetErrors(vol);
	}
#endif 
	return true;
#ifdef NTFS_RW
iput_usnjrnl_err_out:
	if (vol->usnjrnl_j_ino)
		iput(vol->usnjrnl_j_ino);
	if (vol->usnjrnl_max_ino)
		iput(vol->usnjrnl_max_ino);
	if (vol->usnjrnl_ino)
		iput(vol->usnjrnl_ino);
iput_quota_err_out:
	if (vol->quota_q_ino)
		iput(vol->quota_q_ino);
	if (vol->quota_ino)
		iput(vol->quota_ino);
	iput(vol->extend_ino);
#endif 
iput_sec_err_out:
	iput(vol->secure_ino);
iput_root_err_out:
	iput(vol->root_ino);
iput_logfile_err_out:
#ifdef NTFS_RW
	if (vol->logfile_ino)
		iput(vol->logfile_ino);
iput_vol_err_out:
#endif 
	iput(vol->vol_ino);
iput_lcnbmp_err_out:
	iput(vol->lcnbmp_ino);
iput_attrdef_err_out:
	vol->attrdef_size = 0;
	if (vol->attrdef) {
		ntfs_free(vol->attrdef);
		vol->attrdef = NULL;
	}
#ifdef NTFS_RW
iput_upcase_err_out:
#endif 
	vol->upcase_len = 0;
	mutex_lock(&ntfs_lock);
	if (vol->upcase == default_upcase) {
		ntfs_nr_upcase_users--;
		vol->upcase = NULL;
	}
	mutex_unlock(&ntfs_lock);
	if (vol->upcase) {
		ntfs_free(vol->upcase);
		vol->upcase = NULL;
	}
iput_mftbmp_err_out:
	iput(vol->mftbmp_ino);
iput_mirr_err_out:
#ifdef NTFS_RW
	if (vol->mftmirr_ino)
		iput(vol->mftmirr_ino);
#endif 
	return false;
}

static void ntfs_put_super(struct super_block *sb)
{
	ntfs_volume *vol = NTFS_SB(sb);

	ntfs_debug("Entering.");

#ifdef NTFS_RW
	ntfs_commit_inode(vol->vol_ino);

	
	if (vol->major_ver >= 3) {
		if (vol->usnjrnl_j_ino)
			ntfs_commit_inode(vol->usnjrnl_j_ino);
		if (vol->usnjrnl_max_ino)
			ntfs_commit_inode(vol->usnjrnl_max_ino);
		if (vol->usnjrnl_ino)
			ntfs_commit_inode(vol->usnjrnl_ino);
		if (vol->quota_q_ino)
			ntfs_commit_inode(vol->quota_q_ino);
		if (vol->quota_ino)
			ntfs_commit_inode(vol->quota_ino);
		if (vol->extend_ino)
			ntfs_commit_inode(vol->extend_ino);
		if (vol->secure_ino)
			ntfs_commit_inode(vol->secure_ino);
	}

	ntfs_commit_inode(vol->root_ino);

	down_write(&vol->lcnbmp_lock);
	ntfs_commit_inode(vol->lcnbmp_ino);
	up_write(&vol->lcnbmp_lock);

	down_write(&vol->mftbmp_lock);
	ntfs_commit_inode(vol->mftbmp_ino);
	up_write(&vol->mftbmp_lock);

	if (vol->logfile_ino)
		ntfs_commit_inode(vol->logfile_ino);

	if (vol->mftmirr_ino)
		ntfs_commit_inode(vol->mftmirr_ino);
	ntfs_commit_inode(vol->mft_ino);

	if (!(sb->s_flags & MS_RDONLY)) {
		if (!NVolErrors(vol)) {
			if (ntfs_clear_volume_flags(vol, VOLUME_IS_DIRTY))
				ntfs_warning(sb, "Failed to clear dirty bit "
						"in volume information "
						"flags.  Run chkdsk.");
			ntfs_commit_inode(vol->vol_ino);
			ntfs_commit_inode(vol->root_ino);
			if (vol->mftmirr_ino)
				ntfs_commit_inode(vol->mftmirr_ino);
			ntfs_commit_inode(vol->mft_ino);
		} else {
			ntfs_warning(sb, "Volume has errors.  Leaving volume "
					"marked dirty.  Run chkdsk.");
		}
	}
#endif 

	iput(vol->vol_ino);
	vol->vol_ino = NULL;

	
	if (vol->major_ver >= 3) {
#ifdef NTFS_RW
		if (vol->usnjrnl_j_ino) {
			iput(vol->usnjrnl_j_ino);
			vol->usnjrnl_j_ino = NULL;
		}
		if (vol->usnjrnl_max_ino) {
			iput(vol->usnjrnl_max_ino);
			vol->usnjrnl_max_ino = NULL;
		}
		if (vol->usnjrnl_ino) {
			iput(vol->usnjrnl_ino);
			vol->usnjrnl_ino = NULL;
		}
		if (vol->quota_q_ino) {
			iput(vol->quota_q_ino);
			vol->quota_q_ino = NULL;
		}
		if (vol->quota_ino) {
			iput(vol->quota_ino);
			vol->quota_ino = NULL;
		}
#endif 
		if (vol->extend_ino) {
			iput(vol->extend_ino);
			vol->extend_ino = NULL;
		}
		if (vol->secure_ino) {
			iput(vol->secure_ino);
			vol->secure_ino = NULL;
		}
	}

	iput(vol->root_ino);
	vol->root_ino = NULL;

	down_write(&vol->lcnbmp_lock);
	iput(vol->lcnbmp_ino);
	vol->lcnbmp_ino = NULL;
	up_write(&vol->lcnbmp_lock);

	down_write(&vol->mftbmp_lock);
	iput(vol->mftbmp_ino);
	vol->mftbmp_ino = NULL;
	up_write(&vol->mftbmp_lock);

#ifdef NTFS_RW
	if (vol->logfile_ino) {
		iput(vol->logfile_ino);
		vol->logfile_ino = NULL;
	}
	if (vol->mftmirr_ino) {
		
		ntfs_commit_inode(vol->mftmirr_ino);
		ntfs_commit_inode(vol->mft_ino);
		iput(vol->mftmirr_ino);
		vol->mftmirr_ino = NULL;
	}
	/*
	 * We should have no dirty inodes left, due to
	 * mft.c::ntfs_mft_writepage() cleaning all the dirty pages as
	 * the underlying mft records are written out and cleaned.
	 */
	ntfs_commit_inode(vol->mft_ino);
	write_inode_now(vol->mft_ino, 1);
#endif 

	iput(vol->mft_ino);
	vol->mft_ino = NULL;

	
	vol->attrdef_size = 0;
	if (vol->attrdef) {
		ntfs_free(vol->attrdef);
		vol->attrdef = NULL;
	}
	vol->upcase_len = 0;
	mutex_lock(&ntfs_lock);
	if (vol->upcase == default_upcase) {
		ntfs_nr_upcase_users--;
		vol->upcase = NULL;
	}
	if (!ntfs_nr_upcase_users && default_upcase) {
		ntfs_free(default_upcase);
		default_upcase = NULL;
	}
	if (vol->cluster_size <= 4096 && !--ntfs_nr_compression_users)
		free_compression_buffers();
	mutex_unlock(&ntfs_lock);
	if (vol->upcase) {
		ntfs_free(vol->upcase);
		vol->upcase = NULL;
	}

	unload_nls(vol->nls_map);

	sb->s_fs_info = NULL;
	kfree(vol);
}

static s64 get_nr_free_clusters(ntfs_volume *vol)
{
	s64 nr_free = vol->nr_clusters;
	struct address_space *mapping = vol->lcnbmp_ino->i_mapping;
	struct page *page;
	pgoff_t index, max_index;

	ntfs_debug("Entering.");
	
	down_read(&vol->lcnbmp_lock);
	max_index = (((vol->nr_clusters + 7) >> 3) + PAGE_CACHE_SIZE - 1) >>
			PAGE_CACHE_SHIFT;
	
	ntfs_debug("Reading $Bitmap, max_index = 0x%lx, max_size = 0x%lx.",
			max_index, PAGE_CACHE_SIZE / 4);
	for (index = 0; index < max_index; index++) {
		unsigned long *kaddr;

		page = read_mapping_page(mapping, index, NULL);
		
		if (IS_ERR(page)) {
			ntfs_debug("read_mapping_page() error. Skipping "
					"page (index 0x%lx).", index);
			nr_free -= PAGE_CACHE_SIZE * 8;
			continue;
		}
		kaddr = kmap_atomic(page);
		nr_free -= bitmap_weight(kaddr,
					PAGE_CACHE_SIZE * BITS_PER_BYTE);
		kunmap_atomic(kaddr);
		page_cache_release(page);
	}
	ntfs_debug("Finished reading $Bitmap, last index = 0x%lx.", index - 1);
	if (vol->nr_clusters & 63)
		nr_free += 64 - (vol->nr_clusters & 63);
	up_read(&vol->lcnbmp_lock);
	
	if (nr_free < 0)
		nr_free = 0;
	ntfs_debug("Exiting.");
	return nr_free;
}

static unsigned long __get_nr_free_mft_records(ntfs_volume *vol,
		s64 nr_free, const pgoff_t max_index)
{
	struct address_space *mapping = vol->mftbmp_ino->i_mapping;
	struct page *page;
	pgoff_t index;

	ntfs_debug("Entering.");
	
	ntfs_debug("Reading $MFT/$BITMAP, max_index = 0x%lx, max_size = "
			"0x%lx.", max_index, PAGE_CACHE_SIZE / 4);
	for (index = 0; index < max_index; index++) {
		unsigned long *kaddr;

		page = read_mapping_page(mapping, index, NULL);
		
		if (IS_ERR(page)) {
			ntfs_debug("read_mapping_page() error. Skipping "
					"page (index 0x%lx).", index);
			nr_free -= PAGE_CACHE_SIZE * 8;
			continue;
		}
		kaddr = kmap_atomic(page);
		nr_free -= bitmap_weight(kaddr,
					PAGE_CACHE_SIZE * BITS_PER_BYTE);
		kunmap_atomic(kaddr);
		page_cache_release(page);
	}
	ntfs_debug("Finished reading $MFT/$BITMAP, last index = 0x%lx.",
			index - 1);
	
	if (nr_free < 0)
		nr_free = 0;
	ntfs_debug("Exiting.");
	return nr_free;
}

static int ntfs_statfs(struct dentry *dentry, struct kstatfs *sfs)
{
	struct super_block *sb = dentry->d_sb;
	s64 size;
	ntfs_volume *vol = NTFS_SB(sb);
	ntfs_inode *mft_ni = NTFS_I(vol->mft_ino);
	pgoff_t max_index;
	unsigned long flags;

	ntfs_debug("Entering.");
	
	sfs->f_type   = NTFS_SB_MAGIC;
	
	sfs->f_bsize  = PAGE_CACHE_SIZE;
	sfs->f_blocks = vol->nr_clusters << vol->cluster_size_bits >>
				PAGE_CACHE_SHIFT;
	
	size	      = get_nr_free_clusters(vol) << vol->cluster_size_bits >>
				PAGE_CACHE_SHIFT;
	if (size < 0LL)
		size = 0LL;
	
	sfs->f_bavail = sfs->f_bfree = size;
	
	down_read(&vol->mftbmp_lock);
	read_lock_irqsave(&mft_ni->size_lock, flags);
	size = i_size_read(vol->mft_ino) >> vol->mft_record_size_bits;
	max_index = ((((mft_ni->initialized_size >> vol->mft_record_size_bits)
			+ 7) >> 3) + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	read_unlock_irqrestore(&mft_ni->size_lock, flags);
	
	sfs->f_files = size;
	
	sfs->f_ffree = __get_nr_free_mft_records(vol, size, max_index);
	up_read(&vol->mftbmp_lock);
	sfs->f_fsid.val[0] = vol->serial_no & 0xffffffff;
	sfs->f_fsid.val[1] = (vol->serial_no >> 32) & 0xffffffff;
	
	sfs->f_namelen	   = NTFS_MAX_NAME_LEN;
	return 0;
}

#ifdef NTFS_RW
static int ntfs_write_inode(struct inode *vi, struct writeback_control *wbc)
{
	return __ntfs_write_inode(vi, wbc->sync_mode == WB_SYNC_ALL);
}
#endif

static const struct super_operations ntfs_sops = {
	.alloc_inode	= ntfs_alloc_big_inode,	  
	.destroy_inode	= ntfs_destroy_big_inode, 
#ifdef NTFS_RW
	
	
	.write_inode	= ntfs_write_inode,	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
#endif 
	.put_super	= ntfs_put_super,	
	.statfs		= ntfs_statfs,		
	.remount_fs	= ntfs_remount,		
	.evict_inode	= ntfs_evict_big_inode,	
	
	.show_options	= ntfs_show_options,	
};

static int ntfs_fill_super(struct super_block *sb, void *opt, const int silent)
{
	ntfs_volume *vol;
	struct buffer_head *bh;
	struct inode *tmp_ino;
	int blocksize, result;

	lockdep_off();
	ntfs_debug("Entering.");
#ifndef NTFS_RW
	sb->s_flags |= MS_RDONLY;
#endif 
	
	sb->s_fs_info = kmalloc(sizeof(ntfs_volume), GFP_NOFS);
	vol = NTFS_SB(sb);
	if (!vol) {
		if (!silent)
			ntfs_error(sb, "Allocation of NTFS volume structure "
					"failed. Aborting mount...");
		lockdep_on();
		return -ENOMEM;
	}
	
	*vol = (ntfs_volume) {
		.sb = sb,
		.fmask = 0177,
		.dmask = 0077,
	};
	init_rwsem(&vol->mftbmp_lock);
	init_rwsem(&vol->lcnbmp_lock);

	
	NVolSetSparseEnabled(vol);

	
	if (!parse_options(vol, (char*)opt))
		goto err_out_now;

	
	if (bdev_logical_block_size(sb->s_bdev) > PAGE_CACHE_SIZE) {
		if (!silent)
			ntfs_error(sb, "Device has unsupported sector size "
					"(%i).  The maximum supported sector "
					"size on this architecture is %lu "
					"bytes.",
					bdev_logical_block_size(sb->s_bdev),
					PAGE_CACHE_SIZE);
		goto err_out_now;
	}
	blocksize = sb_min_blocksize(sb, NTFS_BLOCK_SIZE);
	if (blocksize < NTFS_BLOCK_SIZE) {
		if (!silent)
			ntfs_error(sb, "Unable to set device block size.");
		goto err_out_now;
	}
	BUG_ON(blocksize != sb->s_blocksize);
	ntfs_debug("Set device block size to %i bytes (block size bits %i).",
			blocksize, sb->s_blocksize_bits);
	
	if (!i_size_read(sb->s_bdev->bd_inode)) {
		if (!silent)
			ntfs_error(sb, "Unable to determine device size.");
		goto err_out_now;
	}
	vol->nr_blocks = i_size_read(sb->s_bdev->bd_inode) >>
			sb->s_blocksize_bits;
	
	if (!(bh = read_ntfs_boot_sector(sb, silent))) {
		if (!silent)
			ntfs_error(sb, "Not an NTFS volume.");
		goto err_out_now;
	}
	result = parse_ntfs_boot_sector(vol, (NTFS_BOOT_SECTOR*)bh->b_data);
	brelse(bh);
	if (!result) {
		if (!silent)
			ntfs_error(sb, "Unsupported NTFS filesystem.");
		goto err_out_now;
	}
	if (vol->sector_size > blocksize) {
		blocksize = sb_set_blocksize(sb, vol->sector_size);
		if (blocksize != vol->sector_size) {
			if (!silent)
				ntfs_error(sb, "Unable to set device block "
						"size to sector size (%i).",
						vol->sector_size);
			goto err_out_now;
		}
		BUG_ON(blocksize != sb->s_blocksize);
		vol->nr_blocks = i_size_read(sb->s_bdev->bd_inode) >>
				sb->s_blocksize_bits;
		ntfs_debug("Changed device block size to %i bytes (block size "
				"bits %i) to match volume sector size.",
				blocksize, sb->s_blocksize_bits);
	}
	
	ntfs_setup_allocators(vol);
	
	sb->s_magic = NTFS_SB_MAGIC;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	
	sb->s_time_gran = 100;
	sb->s_op = &ntfs_sops;
	tmp_ino = new_inode(sb);
	if (!tmp_ino) {
		if (!silent)
			ntfs_error(sb, "Failed to load essential metadata.");
		goto err_out_now;
	}
	tmp_ino->i_ino = FILE_MFT;
	insert_inode_hash(tmp_ino);
	if (ntfs_read_inode_mount(tmp_ino) < 0) {
		if (!silent)
			ntfs_error(sb, "Failed to load essential metadata.");
		goto iput_tmp_ino_err_out_now;
	}
	mutex_lock(&ntfs_lock);
	if (vol->cluster_size <= 4096 && !ntfs_nr_compression_users++) {
		result = allocate_compression_buffers();
		if (result) {
			ntfs_error(NULL, "Failed to allocate buffers "
					"for compression engine.");
			ntfs_nr_compression_users--;
			mutex_unlock(&ntfs_lock);
			goto iput_tmp_ino_err_out_now;
		}
	}
	if (!default_upcase)
		default_upcase = generate_default_upcase();
	ntfs_nr_upcase_users++;
	mutex_unlock(&ntfs_lock);
	if (!load_system_files(vol)) {
		ntfs_error(sb, "Failed to load system files.");
		goto unl_upcase_iput_tmp_ino_err_out_now;
	}

	
	ihold(vol->root_ino);
	if ((sb->s_root = d_make_root(vol->root_ino))) {
		ntfs_debug("Exiting, status successful.");
		
		mutex_lock(&ntfs_lock);
		if (!--ntfs_nr_upcase_users && default_upcase) {
			ntfs_free(default_upcase);
			default_upcase = NULL;
		}
		mutex_unlock(&ntfs_lock);
		sb->s_export_op = &ntfs_export_ops;
		lockdep_on();
		return 0;
	}
	ntfs_error(sb, "Failed to allocate root directory.");
	
	
	
	
	iput(vol->vol_ino);
	vol->vol_ino = NULL;
	
	if (vol->major_ver >= 3) {
#ifdef NTFS_RW
		if (vol->usnjrnl_j_ino) {
			iput(vol->usnjrnl_j_ino);
			vol->usnjrnl_j_ino = NULL;
		}
		if (vol->usnjrnl_max_ino) {
			iput(vol->usnjrnl_max_ino);
			vol->usnjrnl_max_ino = NULL;
		}
		if (vol->usnjrnl_ino) {
			iput(vol->usnjrnl_ino);
			vol->usnjrnl_ino = NULL;
		}
		if (vol->quota_q_ino) {
			iput(vol->quota_q_ino);
			vol->quota_q_ino = NULL;
		}
		if (vol->quota_ino) {
			iput(vol->quota_ino);
			vol->quota_ino = NULL;
		}
#endif 
		if (vol->extend_ino) {
			iput(vol->extend_ino);
			vol->extend_ino = NULL;
		}
		if (vol->secure_ino) {
			iput(vol->secure_ino);
			vol->secure_ino = NULL;
		}
	}
	iput(vol->root_ino);
	vol->root_ino = NULL;
	iput(vol->lcnbmp_ino);
	vol->lcnbmp_ino = NULL;
	iput(vol->mftbmp_ino);
	vol->mftbmp_ino = NULL;
#ifdef NTFS_RW
	if (vol->logfile_ino) {
		iput(vol->logfile_ino);
		vol->logfile_ino = NULL;
	}
	if (vol->mftmirr_ino) {
		iput(vol->mftmirr_ino);
		vol->mftmirr_ino = NULL;
	}
#endif 
	
	vol->attrdef_size = 0;
	if (vol->attrdef) {
		ntfs_free(vol->attrdef);
		vol->attrdef = NULL;
	}
	vol->upcase_len = 0;
	mutex_lock(&ntfs_lock);
	if (vol->upcase == default_upcase) {
		ntfs_nr_upcase_users--;
		vol->upcase = NULL;
	}
	mutex_unlock(&ntfs_lock);
	if (vol->upcase) {
		ntfs_free(vol->upcase);
		vol->upcase = NULL;
	}
	if (vol->nls_map) {
		unload_nls(vol->nls_map);
		vol->nls_map = NULL;
	}
	
unl_upcase_iput_tmp_ino_err_out_now:
	mutex_lock(&ntfs_lock);
	if (!--ntfs_nr_upcase_users && default_upcase) {
		ntfs_free(default_upcase);
		default_upcase = NULL;
	}
	if (vol->cluster_size <= 4096 && !--ntfs_nr_compression_users)
		free_compression_buffers();
	mutex_unlock(&ntfs_lock);
iput_tmp_ino_err_out_now:
	iput(tmp_ino);
	if (vol->mft_ino && vol->mft_ino != tmp_ino)
		iput(vol->mft_ino);
	vol->mft_ino = NULL;
	
err_out_now:
	sb->s_fs_info = NULL;
	kfree(vol);
	ntfs_debug("Failed, returning -EINVAL.");
	lockdep_on();
	return -EINVAL;
}

struct kmem_cache *ntfs_name_cache;

struct kmem_cache *ntfs_inode_cache;
struct kmem_cache *ntfs_big_inode_cache;

static void ntfs_big_inode_init_once(void *foo)
{
	ntfs_inode *ni = (ntfs_inode *)foo;

	inode_init_once(VFS_I(ni));
}

struct kmem_cache *ntfs_attr_ctx_cache;
struct kmem_cache *ntfs_index_ctx_cache;

DEFINE_MUTEX(ntfs_lock);

static struct dentry *ntfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, ntfs_fill_super);
}

static struct file_system_type ntfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "ntfs",
	.mount		= ntfs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static const char ntfs_index_ctx_cache_name[] = "ntfs_index_ctx_cache";
static const char ntfs_attr_ctx_cache_name[] = "ntfs_attr_ctx_cache";
static const char ntfs_name_cache_name[] = "ntfs_name_cache";
static const char ntfs_inode_cache_name[] = "ntfs_inode_cache";
static const char ntfs_big_inode_cache_name[] = "ntfs_big_inode_cache";

static int __init init_ntfs_fs(void)
{
	int err = 0;

	
	printk(KERN_INFO "NTFS driver " NTFS_VERSION " [Flags: R/"
#ifdef NTFS_RW
			"W"
#else
			"O"
#endif
#ifdef DEBUG
			" DEBUG"
#endif
#ifdef MODULE
			" MODULE"
#endif
			"].\n");

	ntfs_debug("Debug messages are enabled.");

	ntfs_index_ctx_cache = kmem_cache_create(ntfs_index_ctx_cache_name,
			sizeof(ntfs_index_context), 0 ,
			SLAB_HWCACHE_ALIGN, NULL );
	if (!ntfs_index_ctx_cache) {
		printk(KERN_CRIT "NTFS: Failed to create %s!\n",
				ntfs_index_ctx_cache_name);
		goto ictx_err_out;
	}
	ntfs_attr_ctx_cache = kmem_cache_create(ntfs_attr_ctx_cache_name,
			sizeof(ntfs_attr_search_ctx), 0 ,
			SLAB_HWCACHE_ALIGN, NULL );
	if (!ntfs_attr_ctx_cache) {
		printk(KERN_CRIT "NTFS: Failed to create %s!\n",
				ntfs_attr_ctx_cache_name);
		goto actx_err_out;
	}

	ntfs_name_cache = kmem_cache_create(ntfs_name_cache_name,
			(NTFS_MAX_NAME_LEN+1) * sizeof(ntfschar), 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!ntfs_name_cache) {
		printk(KERN_CRIT "NTFS: Failed to create %s!\n",
				ntfs_name_cache_name);
		goto name_err_out;
	}

	ntfs_inode_cache = kmem_cache_create(ntfs_inode_cache_name,
			sizeof(ntfs_inode), 0,
			SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD, NULL);
	if (!ntfs_inode_cache) {
		printk(KERN_CRIT "NTFS: Failed to create %s!\n",
				ntfs_inode_cache_name);
		goto inode_err_out;
	}

	ntfs_big_inode_cache = kmem_cache_create(ntfs_big_inode_cache_name,
			sizeof(big_ntfs_inode), 0,
			SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD,
			ntfs_big_inode_init_once);
	if (!ntfs_big_inode_cache) {
		printk(KERN_CRIT "NTFS: Failed to create %s!\n",
				ntfs_big_inode_cache_name);
		goto big_inode_err_out;
	}

	
	err = ntfs_sysctl(1);
	if (err) {
		printk(KERN_CRIT "NTFS: Failed to register NTFS sysctls!\n");
		goto sysctl_err_out;
	}

	err = register_filesystem(&ntfs_fs_type);
	if (!err) {
		ntfs_debug("NTFS driver registered successfully.");
		return 0; 
	}
	printk(KERN_CRIT "NTFS: Failed to register NTFS filesystem driver!\n");

	
	ntfs_sysctl(0);
sysctl_err_out:
	kmem_cache_destroy(ntfs_big_inode_cache);
big_inode_err_out:
	kmem_cache_destroy(ntfs_inode_cache);
inode_err_out:
	kmem_cache_destroy(ntfs_name_cache);
name_err_out:
	kmem_cache_destroy(ntfs_attr_ctx_cache);
actx_err_out:
	kmem_cache_destroy(ntfs_index_ctx_cache);
ictx_err_out:
	if (!err) {
		printk(KERN_CRIT "NTFS: Aborting NTFS filesystem driver "
				"registration...\n");
		err = -ENOMEM;
	}
	return err;
}

static void __exit exit_ntfs_fs(void)
{
	ntfs_debug("Unregistering NTFS driver.");

	unregister_filesystem(&ntfs_fs_type);
	kmem_cache_destroy(ntfs_big_inode_cache);
	kmem_cache_destroy(ntfs_inode_cache);
	kmem_cache_destroy(ntfs_name_cache);
	kmem_cache_destroy(ntfs_attr_ctx_cache);
	kmem_cache_destroy(ntfs_index_ctx_cache);
	
	ntfs_sysctl(0);
}

MODULE_AUTHOR("Anton Altaparmakov <anton@tuxera.com>");
MODULE_DESCRIPTION("NTFS 1.2/3.x driver - Copyright (c) 2001-2011 Anton Altaparmakov and Tuxera Inc.");
MODULE_VERSION(NTFS_VERSION);
MODULE_LICENSE("GPL");
#ifdef DEBUG
module_param(debug_msgs, bint, 0);
MODULE_PARM_DESC(debug_msgs, "Enable debug messages.");
#endif

module_init(init_ntfs_fs)
module_exit(exit_ntfs_fs)
