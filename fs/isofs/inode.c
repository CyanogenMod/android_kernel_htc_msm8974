
#include <linux/init.h>
#include <linux/module.h>

#include <linux/slab.h>
#include <linux/nls.h>
#include <linux/ctype.h>
#include <linux/statfs.h>
#include <linux/cdrom.h>
#include <linux/parser.h>
#include <linux/mpage.h>

#include "isofs.h"
#include "zisofs.h"

#define BEQUIET

static int isofs_hashi(const struct dentry *parent, const struct inode *inode,
		struct qstr *qstr);
static int isofs_hash(const struct dentry *parent, const struct inode *inode,
		struct qstr *qstr);
static int isofs_dentry_cmpi(const struct dentry *parent,
		const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name);
static int isofs_dentry_cmp(const struct dentry *parent,
		const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name);

#ifdef CONFIG_JOLIET
static int isofs_hashi_ms(const struct dentry *parent, const struct inode *inode,
		struct qstr *qstr);
static int isofs_hash_ms(const struct dentry *parent, const struct inode *inode,
		struct qstr *qstr);
static int isofs_dentry_cmpi_ms(const struct dentry *parent,
		const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name);
static int isofs_dentry_cmp_ms(const struct dentry *parent,
		const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name);
#endif

static void isofs_put_super(struct super_block *sb)
{
	struct isofs_sb_info *sbi = ISOFS_SB(sb);

#ifdef CONFIG_JOLIET
	unload_nls(sbi->s_nls_iocharset);
#endif

	kfree(sbi);
	sb->s_fs_info = NULL;
	return;
}

static int isofs_read_inode(struct inode *);
static int isofs_statfs (struct dentry *, struct kstatfs *);

static struct kmem_cache *isofs_inode_cachep;

static struct inode *isofs_alloc_inode(struct super_block *sb)
{
	struct iso_inode_info *ei;
	ei = kmem_cache_alloc(isofs_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void isofs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(isofs_inode_cachep, ISOFS_I(inode));
}

static void isofs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, isofs_i_callback);
}

static void init_once(void *foo)
{
	struct iso_inode_info *ei = foo;

	inode_init_once(&ei->vfs_inode);
}

static int init_inodecache(void)
{
	isofs_inode_cachep = kmem_cache_create("isofs_inode_cache",
					sizeof(struct iso_inode_info),
					0, (SLAB_RECLAIM_ACCOUNT|
					SLAB_MEM_SPREAD),
					init_once);
	if (isofs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	kmem_cache_destroy(isofs_inode_cachep);
}

static int isofs_remount(struct super_block *sb, int *flags, char *data)
{
	
	*flags |= MS_RDONLY;
	return 0;
}

static const struct super_operations isofs_sops = {
	.alloc_inode	= isofs_alloc_inode,
	.destroy_inode	= isofs_destroy_inode,
	.put_super	= isofs_put_super,
	.statfs		= isofs_statfs,
	.remount_fs	= isofs_remount,
	.show_options	= generic_show_options,
};


static const struct dentry_operations isofs_dentry_ops[] = {
	{
		.d_hash		= isofs_hash,
		.d_compare	= isofs_dentry_cmp,
	},
	{
		.d_hash		= isofs_hashi,
		.d_compare	= isofs_dentry_cmpi,
	},
#ifdef CONFIG_JOLIET
	{
		.d_hash		= isofs_hash_ms,
		.d_compare	= isofs_dentry_cmp_ms,
	},
	{
		.d_hash		= isofs_hashi_ms,
		.d_compare	= isofs_dentry_cmpi_ms,
	},
#endif
};

struct iso9660_options{
	unsigned int rock:1;
	unsigned int joliet:1;
	unsigned int cruft:1;
	unsigned int hide:1;
	unsigned int showassoc:1;
	unsigned int nocompress:1;
	unsigned int overriderockperm:1;
	unsigned int uid_set:1;
	unsigned int gid_set:1;
	unsigned int utf8:1;
	unsigned char map;
	unsigned char check;
	unsigned int blocksize;
	umode_t fmode;
	umode_t dmode;
	gid_t gid;
	uid_t uid;
	char *iocharset;
	
	s32 session;
	s32 sbsector;
};

static int
isofs_hash_common(const struct dentry *dentry, struct qstr *qstr, int ms)
{
	const char *name;
	int len;

	len = qstr->len;
	name = qstr->name;
	if (ms) {
		while (len && name[len-1] == '.')
			len--;
	}

	qstr->hash = full_name_hash(name, len);

	return 0;
}

static int
isofs_hashi_common(const struct dentry *dentry, struct qstr *qstr, int ms)
{
	const char *name;
	int len;
	char c;
	unsigned long hash;

	len = qstr->len;
	name = qstr->name;
	if (ms) {
		while (len && name[len-1] == '.')
			len--;
	}

	hash = init_name_hash();
	while (len--) {
		c = tolower(*name++);
		hash = partial_name_hash(c, hash);
	}
	qstr->hash = end_name_hash(hash);

	return 0;
}

static int isofs_dentry_cmp_common(
		unsigned int len, const char *str,
		const struct qstr *name, int ms, int ci)
{
	int alen, blen;

	
	alen = name->len;
	blen = len;
	if (ms) {
		while (alen && name->name[alen-1] == '.')
			alen--;
		while (blen && str[blen-1] == '.')
			blen--;
	}
	if (alen == blen) {
		if (ci) {
			if (strnicmp(name->name, str, alen) == 0)
				return 0;
		} else {
			if (strncmp(name->name, str, alen) == 0)
				return 0;
		}
	}
	return 1;
}

static int
isofs_hash(const struct dentry *dentry, const struct inode *inode,
		struct qstr *qstr)
{
	return isofs_hash_common(dentry, qstr, 0);
}

static int
isofs_hashi(const struct dentry *dentry, const struct inode *inode,
		struct qstr *qstr)
{
	return isofs_hashi_common(dentry, qstr, 0);
}

static int
isofs_dentry_cmp(const struct dentry *parent, const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	return isofs_dentry_cmp_common(len, str, name, 0, 0);
}

static int
isofs_dentry_cmpi(const struct dentry *parent, const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	return isofs_dentry_cmp_common(len, str, name, 0, 1);
}

#ifdef CONFIG_JOLIET
static int
isofs_hash_ms(const struct dentry *dentry, const struct inode *inode,
		struct qstr *qstr)
{
	return isofs_hash_common(dentry, qstr, 1);
}

static int
isofs_hashi_ms(const struct dentry *dentry, const struct inode *inode,
		struct qstr *qstr)
{
	return isofs_hashi_common(dentry, qstr, 1);
}

static int
isofs_dentry_cmp_ms(const struct dentry *parent, const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	return isofs_dentry_cmp_common(len, str, name, 1, 0);
}

static int
isofs_dentry_cmpi_ms(const struct dentry *parent, const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	return isofs_dentry_cmp_common(len, str, name, 1, 1);
}
#endif

enum {
	Opt_block, Opt_check_r, Opt_check_s, Opt_cruft, Opt_gid, Opt_ignore,
	Opt_iocharset, Opt_map_a, Opt_map_n, Opt_map_o, Opt_mode, Opt_nojoliet,
	Opt_norock, Opt_sb, Opt_session, Opt_uid, Opt_unhide, Opt_utf8, Opt_err,
	Opt_nocompress, Opt_hide, Opt_showassoc, Opt_dmode, Opt_overriderockperm,
};

static const match_table_t tokens = {
	{Opt_norock, "norock"},
	{Opt_nojoliet, "nojoliet"},
	{Opt_unhide, "unhide"},
	{Opt_hide, "hide"},
	{Opt_showassoc, "showassoc"},
	{Opt_cruft, "cruft"},
	{Opt_utf8, "utf8"},
	{Opt_iocharset, "iocharset=%s"},
	{Opt_map_a, "map=acorn"},
	{Opt_map_a, "map=a"},
	{Opt_map_n, "map=normal"},
	{Opt_map_n, "map=n"},
	{Opt_map_o, "map=off"},
	{Opt_map_o, "map=o"},
	{Opt_session, "session=%u"},
	{Opt_sb, "sbsector=%u"},
	{Opt_check_r, "check=relaxed"},
	{Opt_check_r, "check=r"},
	{Opt_check_s, "check=strict"},
	{Opt_check_s, "check=s"},
	{Opt_uid, "uid=%u"},
	{Opt_gid, "gid=%u"},
	{Opt_mode, "mode=%u"},
	{Opt_dmode, "dmode=%u"},
	{Opt_overriderockperm, "overriderockperm"},
	{Opt_block, "block=%u"},
	{Opt_ignore, "conv=binary"},
	{Opt_ignore, "conv=b"},
	{Opt_ignore, "conv=text"},
	{Opt_ignore, "conv=t"},
	{Opt_ignore, "conv=mtext"},
	{Opt_ignore, "conv=m"},
	{Opt_ignore, "conv=auto"},
	{Opt_ignore, "conv=a"},
	{Opt_nocompress, "nocompress"},
	{Opt_err, NULL}
};

static int parse_options(char *options, struct iso9660_options *popt)
{
	char *p;
	int option;

	popt->map = 'n';
	popt->rock = 1;
	popt->joliet = 1;
	popt->cruft = 0;
	popt->hide = 0;
	popt->showassoc = 0;
	popt->check = 'u';		
	popt->nocompress = 0;
	popt->blocksize = 1024;
	popt->fmode = popt->dmode = ISOFS_INVALID_MODE;
	popt->uid_set = 0;
	popt->gid_set = 0;
	popt->gid = 0;
	popt->uid = 0;
	popt->iocharset = NULL;
	popt->utf8 = 0;
	popt->overriderockperm = 0;
	popt->session=-1;
	popt->sbsector=-1;
	if (!options)
		return 1;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		substring_t args[MAX_OPT_ARGS];
		unsigned n;

		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_norock:
			popt->rock = 0;
			break;
		case Opt_nojoliet:
			popt->joliet = 0;
			break;
		case Opt_hide:
			popt->hide = 1;
			break;
		case Opt_unhide:
		case Opt_showassoc:
			popt->showassoc = 1;
			break;
		case Opt_cruft:
			popt->cruft = 1;
			break;
		case Opt_utf8:
			popt->utf8 = 1;
			break;
#ifdef CONFIG_JOLIET
		case Opt_iocharset:
			popt->iocharset = match_strdup(&args[0]);
			break;
#endif
		case Opt_map_a:
			popt->map = 'a';
			break;
		case Opt_map_o:
			popt->map = 'o';
			break;
		case Opt_map_n:
			popt->map = 'n';
			break;
		case Opt_session:
			if (match_int(&args[0], &option))
				return 0;
			n = option;
			if (n > 99)
				return 0;
			popt->session = n + 1;
			break;
		case Opt_sb:
			if (match_int(&args[0], &option))
				return 0;
			popt->sbsector = option;
			break;
		case Opt_check_r:
			popt->check = 'r';
			break;
		case Opt_check_s:
			popt->check = 's';
			break;
		case Opt_ignore:
			break;
		case Opt_uid:
			if (match_int(&args[0], &option))
				return 0;
			popt->uid = option;
			popt->uid_set = 1;
			break;
		case Opt_gid:
			if (match_int(&args[0], &option))
				return 0;
			popt->gid = option;
			popt->gid_set = 1;
			break;
		case Opt_mode:
			if (match_int(&args[0], &option))
				return 0;
			popt->fmode = option;
			break;
		case Opt_dmode:
			if (match_int(&args[0], &option))
				return 0;
			popt->dmode = option;
			break;
		case Opt_overriderockperm:
			popt->overriderockperm = 1;
			break;
		case Opt_block:
			if (match_int(&args[0], &option))
				return 0;
			n = option;
			if (n != 512 && n != 1024 && n != 2048)
				return 0;
			popt->blocksize = n;
			break;
		case Opt_nocompress:
			popt->nocompress = 1;
			break;
		default:
			return 0;
		}
	}
	return 1;
}

/*
 * look if the driver can tell the multi session redirection value
 *
 * don't change this if you don't know what you do, please!
 * Multisession is legal only with XA disks.
 * A non-XA disk with more than one volume descriptor may do it right, but
 * usually is written in a nowhere standardized "multi-partition" manner.
 * Multisession uses absolute addressing (solely the first frame of the whole
 * track is #0), multi-partition uses relative addressing (each first frame of
 * each track is #0), and a track is not a session.
 *
 * A broken CDwriter software or drive firmware does not set new standards,
 * at least not if conflicting with the existing ones.
 *
 * emoenke@gwdg.de
 */
#define WE_OBEY_THE_WRITTEN_STANDARDS 1

static unsigned int isofs_get_last_session(struct super_block *sb, s32 session)
{
	struct cdrom_multisession ms_info;
	unsigned int vol_desc_start;
	struct block_device *bdev = sb->s_bdev;
	int i;

	vol_desc_start=0;
	ms_info.addr_format=CDROM_LBA;
	if(session >= 0 && session <= 99) {
		struct cdrom_tocentry Te;
		Te.cdte_track=session;
		Te.cdte_format=CDROM_LBA;
		i = ioctl_by_bdev(bdev, CDROMREADTOCENTRY, (unsigned long) &Te);
		if (!i) {
			printk(KERN_DEBUG "ISOFS: Session %d start %d type %d\n",
				session, Te.cdte_addr.lba,
				Te.cdte_ctrl&CDROM_DATA_TRACK);
			if ((Te.cdte_ctrl&CDROM_DATA_TRACK) == 4)
				return Te.cdte_addr.lba;
		}

		printk(KERN_ERR "ISOFS: Invalid session number or type of track\n");
	}
	i = ioctl_by_bdev(bdev, CDROMMULTISESSION, (unsigned long) &ms_info);
	if (session > 0)
		printk(KERN_ERR "ISOFS: Invalid session number\n");
#if 0
	printk(KERN_DEBUG "isofs.inode: CDROMMULTISESSION: rc=%d\n",i);
	if (i==0) {
		printk(KERN_DEBUG "isofs.inode: XA disk: %s\n",ms_info.xa_flag?"yes":"no");
		printk(KERN_DEBUG "isofs.inode: vol_desc_start = %d\n", ms_info.addr.lba);
	}
#endif
	if (i==0)
#if WE_OBEY_THE_WRITTEN_STANDARDS
		if (ms_info.xa_flag) 
#endif
			vol_desc_start=ms_info.addr.lba;
	return vol_desc_start;
}

static bool rootdir_empty(struct super_block *sb, unsigned long block)
{
	int offset = 0, files = 0, de_len;
	struct iso_directory_record *de;
	struct buffer_head *bh;

	bh = sb_bread(sb, block);
	if (!bh)
		return true;
	while (files < 3) {
		de = (struct iso_directory_record *) (bh->b_data + offset);
		de_len = *(unsigned char *) de;
		if (de_len == 0)
			break;
		files++;
		offset += de_len;
	}
	brelse(bh);
	return files < 3;
}

static int isofs_fill_super(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh = NULL, *pri_bh = NULL;
	struct hs_primary_descriptor *h_pri = NULL;
	struct iso_primary_descriptor *pri = NULL;
	struct iso_supplementary_descriptor *sec = NULL;
	struct iso_directory_record *rootp;
	struct inode *inode;
	struct iso9660_options opt;
	struct isofs_sb_info *sbi;
	unsigned long first_data_zone;
	int joliet_level = 0;
	int iso_blknum, block;
	int orig_zonesize;
	int table, error = -EINVAL;
	unsigned int vol_desc_start;

	save_mount_options(s, data);

	sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	s->s_fs_info = sbi;

	if (!parse_options((char *)data, &opt))
		goto out_freesbi;

	opt.blocksize = sb_min_blocksize(s, opt.blocksize);

	sbi->s_high_sierra = 0; 

	vol_desc_start = (opt.sbsector != -1) ?
		opt.sbsector : isofs_get_last_session(s,opt.session);

	for (iso_blknum = vol_desc_start+16;
		iso_blknum < vol_desc_start+100; iso_blknum++) {
		struct hs_volume_descriptor *hdp;
		struct iso_volume_descriptor  *vdp;

		block = iso_blknum << (ISOFS_BLOCK_BITS - s->s_blocksize_bits);
		if (!(bh = sb_bread(s, block)))
			goto out_no_read;

		vdp = (struct iso_volume_descriptor *)bh->b_data;
		hdp = (struct hs_volume_descriptor *)bh->b_data;

		if (strncmp (vdp->id, ISO_STANDARD_ID, sizeof vdp->id) == 0) {
			if (isonum_711(vdp->type) == ISO_VD_END)
				break;
			if (isonum_711(vdp->type) == ISO_VD_PRIMARY) {
				if (pri == NULL) {
					pri = (struct iso_primary_descriptor *)vdp;
					
					pri_bh = bh;
					bh = NULL;
				}
			}
#ifdef CONFIG_JOLIET
			else if (isonum_711(vdp->type) == ISO_VD_SUPPLEMENTARY) {
				sec = (struct iso_supplementary_descriptor *)vdp;
				if (sec->escape[0] == 0x25 && sec->escape[1] == 0x2f) {
					if (opt.joliet) {
						if (sec->escape[2] == 0x40)
							joliet_level = 1;
						else if (sec->escape[2] == 0x43)
							joliet_level = 2;
						else if (sec->escape[2] == 0x45)
							joliet_level = 3;

						printk(KERN_DEBUG "ISO 9660 Extensions: "
							"Microsoft Joliet Level %d\n",
							joliet_level);
					}
					goto root_found;
				} else {
				
				sec = NULL;
				}
			}
#endif
		} else {
			if (strncmp (hdp->id, HS_STANDARD_ID, sizeof hdp->id) == 0) {
				if (isonum_711(hdp->type) != ISO_VD_PRIMARY)
					goto out_freebh;

				sbi->s_high_sierra = 1;
				opt.rock = 0;
				h_pri = (struct hs_primary_descriptor *)vdp;
				goto root_found;
			}
		}

		

		brelse(bh);
		bh = NULL;
	}
	if (!pri)
		goto out_unknown_format;
	brelse(bh);
	bh = pri_bh;
	pri_bh = NULL;

root_found:

	if (joliet_level && (pri == NULL || !opt.rock)) {
		pri = (struct iso_primary_descriptor *) sec;
	}

	if(sbi->s_high_sierra){
		rootp = (struct iso_directory_record *) h_pri->root_directory_record;
		sbi->s_nzones = isonum_733(h_pri->volume_space_size);
		sbi->s_log_zone_size = isonum_723(h_pri->logical_block_size);
		sbi->s_max_size = isonum_733(h_pri->volume_space_size);
	} else {
		if (!pri)
			goto out_freebh;
		rootp = (struct iso_directory_record *) pri->root_directory_record;
		sbi->s_nzones = isonum_733(pri->volume_space_size);
		sbi->s_log_zone_size = isonum_723(pri->logical_block_size);
		sbi->s_max_size = isonum_733(pri->volume_space_size);
	}

	sbi->s_ninodes = 0; 

	orig_zonesize = sbi->s_log_zone_size;
	if (orig_zonesize < opt.blocksize)
		goto out_bad_size;

	
	switch (sbi->s_log_zone_size) {
	case  512: sbi->s_log_zone_size =  9; break;
	case 1024: sbi->s_log_zone_size = 10; break;
	case 2048: sbi->s_log_zone_size = 11; break;

	default:
		goto out_bad_zone_size;
	}

	s->s_magic = ISOFS_SUPER_MAGIC;

	s->s_maxbytes = 0x80000000000LL;


	s->s_flags |= MS_RDONLY ;


	first_data_zone = isonum_733(rootp->extent) +
			  isonum_711(rootp->ext_attr_length);
	sbi->s_firstdatazone = first_data_zone;
#ifndef BEQUIET
	printk(KERN_DEBUG "ISOFS: Max size:%ld   Log zone size:%ld\n",
		sbi->s_max_size, 1UL << sbi->s_log_zone_size);
	printk(KERN_DEBUG "ISOFS: First datazone:%ld\n", sbi->s_firstdatazone);
	if(sbi->s_high_sierra)
		printk(KERN_DEBUG "ISOFS: Disc in High Sierra format.\n");
#endif

	if (joliet_level) {
		pri = (struct iso_primary_descriptor *) sec;
		rootp = (struct iso_directory_record *)
			pri->root_directory_record;
		first_data_zone = isonum_733(rootp->extent) +
				isonum_711(rootp->ext_attr_length);
	}

	brelse(pri_bh);
	brelse(bh);

	sb_set_blocksize(s, orig_zonesize);

	sbi->s_nls_iocharset = NULL;

#ifdef CONFIG_JOLIET
	if (joliet_level && opt.utf8 == 0) {
		char *p = opt.iocharset ? opt.iocharset : CONFIG_NLS_DEFAULT;
		sbi->s_nls_iocharset = load_nls(p);
		if (! sbi->s_nls_iocharset) {
			
			if (opt.iocharset)
				goto out_freesbi;
			sbi->s_nls_iocharset = load_nls_default();
		}
	}
#endif
	s->s_op = &isofs_sops;
	s->s_export_op = &isofs_export_ops;
	sbi->s_mapping = opt.map;
	sbi->s_rock = (opt.rock ? 2 : 0);
	sbi->s_rock_offset = -1; 
	sbi->s_cruft = opt.cruft;
	sbi->s_hide = opt.hide;
	sbi->s_showassoc = opt.showassoc;
	sbi->s_uid = opt.uid;
	sbi->s_gid = opt.gid;
	sbi->s_uid_set = opt.uid_set;
	sbi->s_gid_set = opt.gid_set;
	sbi->s_utf8 = opt.utf8;
	sbi->s_nocompress = opt.nocompress;
	sbi->s_overriderockperm = opt.overriderockperm;
	if (opt.fmode != ISOFS_INVALID_MODE)
		sbi->s_fmode = opt.fmode & 0777;
	else
		sbi->s_fmode = ISOFS_INVALID_MODE;
	if (opt.dmode != ISOFS_INVALID_MODE)
		sbi->s_dmode = opt.dmode & 0777;
	else
		sbi->s_dmode = ISOFS_INVALID_MODE;

	inode = isofs_iget(s, sbi->s_firstdatazone, 0);
	if (IS_ERR(inode))
		goto out_no_root;

	if (sbi->s_rock == 1 && joliet_level &&
				rootdir_empty(s, sbi->s_firstdatazone)) {
		printk(KERN_NOTICE
			"ISOFS: primary root directory is empty. "
			"Disabling Rock Ridge and switching to Joliet.");
		sbi->s_rock = 0;
	}

	if (sbi->s_rock == 1) {
		joliet_level = 0;
	} else if (joliet_level) {
		sbi->s_rock = 0;
		if (sbi->s_firstdatazone != first_data_zone) {
			sbi->s_firstdatazone = first_data_zone;
			printk(KERN_DEBUG
				"ISOFS: changing to secondary root\n");
			iput(inode);
			inode = isofs_iget(s, sbi->s_firstdatazone, 0);
			if (IS_ERR(inode))
				goto out_no_root;
		}
	}

	if (opt.check == 'u') {
		
		if (joliet_level)
			opt.check = 'r';
		else
			opt.check = 's';
	}
	sbi->s_joliet_level = joliet_level;

	
	if (!S_ISDIR(inode->i_mode)) {
		printk(KERN_WARNING
			"isofs_fill_super: root inode is not a directory. "
			"Corrupted media?\n");
		goto out_iput;
	}

	table = 0;
	if (joliet_level)
		table += 2;
	if (opt.check == 'r')
		table++;

	s->s_d_op = &isofs_dentry_ops[table];

	
	s->s_root = d_make_root(inode);
	if (!(s->s_root)) {
		error = -ENOMEM;
		goto out_no_inode;
	}

	kfree(opt.iocharset);

	return 0;

out_iput:
	iput(inode);
	goto out_no_inode;
out_no_root:
	error = PTR_ERR(inode);
	if (error != -ENOMEM)
		printk(KERN_WARNING "%s: get root inode failed\n", __func__);
out_no_inode:
#ifdef CONFIG_JOLIET
	unload_nls(sbi->s_nls_iocharset);
#endif
	goto out_freesbi;
out_no_read:
	printk(KERN_WARNING "%s: bread failed, dev=%s, iso_blknum=%d, block=%d\n",
		__func__, s->s_id, iso_blknum, block);
	goto out_freebh;
out_bad_zone_size:
	printk(KERN_WARNING "ISOFS: Bad logical zone size %ld\n",
		sbi->s_log_zone_size);
	goto out_freebh;
out_bad_size:
	printk(KERN_WARNING "ISOFS: Logical zone size(%d) < hardware blocksize(%u)\n",
		orig_zonesize, opt.blocksize);
	goto out_freebh;
out_unknown_format:
	if (!silent)
		printk(KERN_WARNING "ISOFS: Unable to identify CD-ROM format.\n");

out_freebh:
	brelse(bh);
	brelse(pri_bh);
out_freesbi:
	kfree(opt.iocharset);
	kfree(sbi);
	s->s_fs_info = NULL;
	return error;
}

static int isofs_statfs (struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);

	buf->f_type = ISOFS_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = (ISOFS_SB(sb)->s_nzones
		<< (ISOFS_SB(sb)->s_log_zone_size - sb->s_blocksize_bits));
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = ISOFS_SB(sb)->s_ninodes;
	buf->f_ffree = 0;
	buf->f_fsid.val[0] = (u32)id;
	buf->f_fsid.val[1] = (u32)(id >> 32);
	buf->f_namelen = NAME_MAX;
	return 0;
}

int isofs_get_blocks(struct inode *inode, sector_t iblock,
		     struct buffer_head **bh, unsigned long nblocks)
{
	unsigned long b_off = iblock;
	unsigned offset, sect_size;
	unsigned int firstext;
	unsigned long nextblk, nextoff;
	int section, rv, error;
	struct iso_inode_info *ei = ISOFS_I(inode);

	error = -EIO;
	rv = 0;
	if (iblock != b_off) {
		printk(KERN_DEBUG "%s: block number too large\n", __func__);
		goto abort;
	}


	offset = 0;
	firstext = ei->i_first_extent;
	sect_size = ei->i_section_size >> ISOFS_BUFFER_BITS(inode);
	nextblk = ei->i_next_section_block;
	nextoff = ei->i_next_section_offset;
	section = 0;

	while (nblocks) {
		if (b_off > ((inode->i_size + PAGE_CACHE_SIZE - 1) >> ISOFS_BUFFER_BITS(inode))) {
			printk(KERN_DEBUG "%s: block >= EOF (%lu, %llu)\n",
				__func__, b_off,
				(unsigned long long)inode->i_size);
			goto abort;
		}

		while (nextblk && (b_off >= (offset + sect_size))) {
			struct inode *ninode;

			offset += sect_size;
			ninode = isofs_iget(inode->i_sb, nextblk, nextoff);
			if (IS_ERR(ninode)) {
				error = PTR_ERR(ninode);
				goto abort;
			}
			firstext  = ISOFS_I(ninode)->i_first_extent;
			sect_size = ISOFS_I(ninode)->i_section_size >> ISOFS_BUFFER_BITS(ninode);
			nextblk   = ISOFS_I(ninode)->i_next_section_block;
			nextoff   = ISOFS_I(ninode)->i_next_section_offset;
			iput(ninode);

			if (++section > 100) {
				printk(KERN_DEBUG "%s: More than 100 file sections ?!?"
					" aborting...\n", __func__);
				printk(KERN_DEBUG "%s: block=%lu firstext=%u sect_size=%u "
					"nextblk=%lu nextoff=%lu\n", __func__,
					b_off, firstext, (unsigned) sect_size,
					nextblk, nextoff);
				goto abort;
			}
		}

		if (*bh) {
			map_bh(*bh, inode->i_sb, firstext + b_off - offset);
		} else {
			*bh = sb_getblk(inode->i_sb, firstext+b_off-offset);
			if (!*bh)
				goto abort;
		}
		bh++;	
		b_off++;	
		nblocks--;
		rv++;
	}

	error = 0;
abort:
	return rv != 0 ? rv : error;
}

static int isofs_get_block(struct inode *inode, sector_t iblock,
		    struct buffer_head *bh_result, int create)
{
	int ret;

	if (create) {
		printk(KERN_DEBUG "%s: Kernel tries to allocate a block\n", __func__);
		return -EROFS;
	}

	ret = isofs_get_blocks(inode, iblock, &bh_result, 1);
	return ret < 0 ? ret : 0;
}

static int isofs_bmap(struct inode *inode, sector_t block)
{
	struct buffer_head dummy;
	int error;

	dummy.b_state = 0;
	dummy.b_blocknr = -1000;
	error = isofs_get_block(inode, block, &dummy, 0);
	if (!error)
		return dummy.b_blocknr;
	return 0;
}

struct buffer_head *isofs_bread(struct inode *inode, sector_t block)
{
	sector_t blknr = isofs_bmap(inode, block);
	if (!blknr)
		return NULL;
	return sb_bread(inode->i_sb, blknr);
}

static int isofs_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, isofs_get_block);
}

static int isofs_readpages(struct file *file, struct address_space *mapping,
			struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, isofs_get_block);
}

static sector_t _isofs_bmap(struct address_space *mapping, sector_t block)
{
	return generic_block_bmap(mapping,block,isofs_get_block);
}

static const struct address_space_operations isofs_aops = {
	.readpage = isofs_readpage,
	.readpages = isofs_readpages,
	.bmap = _isofs_bmap
};

static int isofs_read_level3_size(struct inode *inode)
{
	unsigned long bufsize = ISOFS_BUFFER_SIZE(inode);
	int high_sierra = ISOFS_SB(inode->i_sb)->s_high_sierra;
	struct buffer_head *bh = NULL;
	unsigned long block, offset, block_saved, offset_saved;
	int i = 0;
	int more_entries = 0;
	struct iso_directory_record *tmpde = NULL;
	struct iso_inode_info *ei = ISOFS_I(inode);

	inode->i_size = 0;

	ei->i_next_section_block = 0;
	ei->i_next_section_offset = 0;

	block = ei->i_iget5_block;
	offset = ei->i_iget5_offset;

	do {
		struct iso_directory_record *de;
		unsigned int de_len;

		if (!bh) {
			bh = sb_bread(inode->i_sb, block);
			if (!bh)
				goto out_noread;
		}
		de = (struct iso_directory_record *) (bh->b_data + offset);
		de_len = *(unsigned char *) de;

		if (de_len == 0) {
			brelse(bh);
			bh = NULL;
			++block;
			offset = 0;
			continue;
		}

		block_saved = block;
		offset_saved = offset;
		offset += de_len;

		
		if (offset >= bufsize) {
			int slop = bufsize - offset + de_len;
			if (!tmpde) {
				tmpde = kmalloc(256, GFP_KERNEL);
				if (!tmpde)
					goto out_nomem;
			}
			memcpy(tmpde, de, slop);
			offset &= bufsize - 1;
			block++;
			brelse(bh);
			bh = NULL;
			if (offset) {
				bh = sb_bread(inode->i_sb, block);
				if (!bh)
					goto out_noread;
				memcpy((void *)tmpde+slop, bh->b_data, offset);
			}
			de = tmpde;
		}

		inode->i_size += isonum_733(de->size);
		if (i == 1) {
			ei->i_next_section_block = block_saved;
			ei->i_next_section_offset = offset_saved;
		}

		more_entries = de->flags[-high_sierra] & 0x80;

		i++;
		if (i > 100)
			goto out_toomany;
	} while (more_entries);
out:
	kfree(tmpde);
	if (bh)
		brelse(bh);
	return 0;

out_nomem:
	if (bh)
		brelse(bh);
	return -ENOMEM;

out_noread:
	printk(KERN_INFO "ISOFS: unable to read i-node block %lu\n", block);
	kfree(tmpde);
	return -EIO;

out_toomany:
	printk(KERN_INFO "%s: More than 100 file sections ?!?, aborting...\n"
		"isofs_read_level3_size: inode=%lu\n",
		__func__, inode->i_ino);
	goto out;
}

static int isofs_read_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct isofs_sb_info *sbi = ISOFS_SB(sb);
	unsigned long bufsize = ISOFS_BUFFER_SIZE(inode);
	unsigned long block;
	int high_sierra = sbi->s_high_sierra;
	struct buffer_head *bh = NULL;
	struct iso_directory_record *de;
	struct iso_directory_record *tmpde = NULL;
	unsigned int de_len;
	unsigned long offset;
	struct iso_inode_info *ei = ISOFS_I(inode);
	int ret = -EIO;

	block = ei->i_iget5_block;
	bh = sb_bread(inode->i_sb, block);
	if (!bh)
		goto out_badread;

	offset = ei->i_iget5_offset;

	de = (struct iso_directory_record *) (bh->b_data + offset);
	de_len = *(unsigned char *) de;

	if (offset + de_len > bufsize) {
		int frag1 = bufsize - offset;

		tmpde = kmalloc(de_len, GFP_KERNEL);
		if (tmpde == NULL) {
			printk(KERN_INFO "%s: out of memory\n", __func__);
			ret = -ENOMEM;
			goto fail;
		}
		memcpy(tmpde, bh->b_data + offset, frag1);
		brelse(bh);
		bh = sb_bread(inode->i_sb, ++block);
		if (!bh)
			goto out_badread;
		memcpy((char *)tmpde+frag1, bh->b_data, de_len - frag1);
		de = tmpde;
	}

	inode->i_ino = isofs_get_ino(ei->i_iget5_block,
					ei->i_iget5_offset,
					ISOFS_BUFFER_BITS(inode));

	
	ei->i_file_format = isofs_file_normal;

	if (de->flags[-high_sierra] & 2) {
		if (sbi->s_dmode != ISOFS_INVALID_MODE)
			inode->i_mode = S_IFDIR | sbi->s_dmode;
		else
			inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
		set_nlink(inode, 1);	
	} else {
		if (sbi->s_fmode != ISOFS_INVALID_MODE) {
			inode->i_mode = S_IFREG | sbi->s_fmode;
		} else {
			inode->i_mode = S_IFREG | S_IRUGO | S_IXUGO;
		}
		set_nlink(inode, 1);
	}
	inode->i_uid = sbi->s_uid;
	inode->i_gid = sbi->s_gid;
	inode->i_blocks = 0;

	ei->i_format_parm[0] = 0;
	ei->i_format_parm[1] = 0;
	ei->i_format_parm[2] = 0;

	ei->i_section_size = isonum_733(de->size);
	if (de->flags[-high_sierra] & 0x80) {
		ret = isofs_read_level3_size(inode);
		if (ret < 0)
			goto fail;
		ret = -EIO;
	} else {
		ei->i_next_section_block = 0;
		ei->i_next_section_offset = 0;
		inode->i_size = isonum_733(de->size);
	}


	if (sbi->s_cruft)
		inode->i_size &= 0x00ffffff;

	if (de->interleave[0]) {
		printk(KERN_DEBUG "ISOFS: Interleaved files not (yet) supported.\n");
		inode->i_size = 0;
	}

	if (de->file_unit_size[0] != 0) {
		printk(KERN_DEBUG "ISOFS: File unit size != 0 for ISO file (%ld).\n",
			inode->i_ino);
	}

#ifdef DEBUG
	if((de->flags[-high_sierra] & ~2)!= 0){
		printk(KERN_DEBUG "ISOFS: Unusual flag settings for ISO file "
				"(%ld %x).\n",
			inode->i_ino, de->flags[-high_sierra]);
	}
#endif

	inode->i_mtime.tv_sec =
	inode->i_atime.tv_sec =
	inode->i_ctime.tv_sec = iso_date(de->date, high_sierra);
	inode->i_mtime.tv_nsec =
	inode->i_atime.tv_nsec =
	inode->i_ctime.tv_nsec = 0;

	ei->i_first_extent = (isonum_733(de->extent) +
			isonum_711(de->ext_attr_length));

	
	inode->i_blocks = (inode->i_size + 511) >> 9;


	if (!high_sierra) {
		parse_rock_ridge_inode(de, inode);
		
		if (sbi->s_uid_set)
			inode->i_uid = sbi->s_uid;
		if (sbi->s_gid_set)
			inode->i_gid = sbi->s_gid;
	}
	
	if (S_ISDIR(inode->i_mode) && sbi->s_overriderockperm &&
	    sbi->s_dmode != ISOFS_INVALID_MODE)
		inode->i_mode = S_IFDIR | sbi->s_dmode;
	if (S_ISREG(inode->i_mode) && sbi->s_overriderockperm &&
	    sbi->s_fmode != ISOFS_INVALID_MODE)
		inode->i_mode = S_IFREG | sbi->s_fmode;

	
	if (S_ISREG(inode->i_mode)) {
		inode->i_fop = &generic_ro_fops;
		switch (ei->i_file_format) {
#ifdef CONFIG_ZISOFS
		case isofs_file_compressed:
			inode->i_data.a_ops = &zisofs_aops;
			break;
#endif
		default:
			inode->i_data.a_ops = &isofs_aops;
			break;
		}
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &isofs_dir_inode_operations;
		inode->i_fop = &isofs_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &page_symlink_inode_operations;
		inode->i_data.a_ops = &isofs_symlink_aops;
	} else
		
		init_special_inode(inode, inode->i_mode, inode->i_rdev);

	ret = 0;
out:
	kfree(tmpde);
	if (bh)
		brelse(bh);
	return ret;

out_badread:
	printk(KERN_WARNING "ISOFS: unable to read i-node block\n");
fail:
	goto out;
}

struct isofs_iget5_callback_data {
	unsigned long block;
	unsigned long offset;
};

static int isofs_iget5_test(struct inode *ino, void *data)
{
	struct iso_inode_info *i = ISOFS_I(ino);
	struct isofs_iget5_callback_data *d =
		(struct isofs_iget5_callback_data*)data;
	return (i->i_iget5_block == d->block)
		&& (i->i_iget5_offset == d->offset);
}

static int isofs_iget5_set(struct inode *ino, void *data)
{
	struct iso_inode_info *i = ISOFS_I(ino);
	struct isofs_iget5_callback_data *d =
		(struct isofs_iget5_callback_data*)data;
	i->i_iget5_block = d->block;
	i->i_iget5_offset = d->offset;
	return 0;
}

struct inode *isofs_iget(struct super_block *sb,
			 unsigned long block,
			 unsigned long offset)
{
	unsigned long hashval;
	struct inode *inode;
	struct isofs_iget5_callback_data data;
	long ret;

	if (offset >= 1ul << sb->s_blocksize_bits)
		return ERR_PTR(-EINVAL);

	data.block = block;
	data.offset = offset;

	hashval = (block << sb->s_blocksize_bits) | offset;

	inode = iget5_locked(sb, hashval, &isofs_iget5_test,
				&isofs_iget5_set, &data);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		ret = isofs_read_inode(inode);
		if (ret < 0) {
			iget_failed(inode);
			inode = ERR_PTR(ret);
		} else {
			unlock_new_inode(inode);
		}
	}

	return inode;
}

static struct dentry *isofs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, isofs_fill_super);
}

static struct file_system_type iso9660_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "iso9660",
	.mount		= isofs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_iso9660_fs(void)
{
	int err = init_inodecache();
	if (err)
		goto out;
#ifdef CONFIG_ZISOFS
	err = zisofs_init();
	if (err)
		goto out1;
#endif
	err = register_filesystem(&iso9660_fs_type);
	if (err)
		goto out2;
	return 0;
out2:
#ifdef CONFIG_ZISOFS
	zisofs_cleanup();
out1:
#endif
	destroy_inodecache();
out:
	return err;
}

static void __exit exit_iso9660_fs(void)
{
        unregister_filesystem(&iso9660_fs_type);
#ifdef CONFIG_ZISOFS
	zisofs_cleanup();
#endif
	destroy_inodecache();
}

module_init(init_iso9660_fs)
module_exit(exit_iso9660_fs)
MODULE_LICENSE("GPL");
MODULE_ALIAS("iso9660");
