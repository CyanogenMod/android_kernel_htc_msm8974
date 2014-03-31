/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#ifndef __JFFS2_NODELIST_H__
#define __JFFS2_NODELIST_H__

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/jffs2.h>
#include "jffs2_fs_sb.h"
#include "jffs2_fs_i.h"
#include "xattr.h"
#include "acl.h"
#include "summary.h"

#ifdef __ECOS
#include "os-ecos.h"
#else
#include "os-linux.h"
#endif

#define JFFS2_NATIVE_ENDIAN


#if defined(JFFS2_NATIVE_ENDIAN)
#define cpu_to_je16(x) ((jint16_t){x})
#define cpu_to_je32(x) ((jint32_t){x})
#define cpu_to_jemode(x) ((jmode_t){os_to_jffs2_mode(x)})

#define constant_cpu_to_je16(x) ((jint16_t){x})
#define constant_cpu_to_je32(x) ((jint32_t){x})

#define je16_to_cpu(x) ((x).v16)
#define je32_to_cpu(x) ((x).v32)
#define jemode_to_cpu(x) (jffs2_to_os_mode((x).m))
#elif defined(JFFS2_BIG_ENDIAN)
#define cpu_to_je16(x) ((jint16_t){cpu_to_be16(x)})
#define cpu_to_je32(x) ((jint32_t){cpu_to_be32(x)})
#define cpu_to_jemode(x) ((jmode_t){cpu_to_be32(os_to_jffs2_mode(x))})

#define constant_cpu_to_je16(x) ((jint16_t){__constant_cpu_to_be16(x)})
#define constant_cpu_to_je32(x) ((jint32_t){__constant_cpu_to_be32(x)})

#define je16_to_cpu(x) (be16_to_cpu(x.v16))
#define je32_to_cpu(x) (be32_to_cpu(x.v32))
#define jemode_to_cpu(x) (be32_to_cpu(jffs2_to_os_mode((x).m)))
#elif defined(JFFS2_LITTLE_ENDIAN)
#define cpu_to_je16(x) ((jint16_t){cpu_to_le16(x)})
#define cpu_to_je32(x) ((jint32_t){cpu_to_le32(x)})
#define cpu_to_jemode(x) ((jmode_t){cpu_to_le32(os_to_jffs2_mode(x))})

#define constant_cpu_to_je16(x) ((jint16_t){__constant_cpu_to_le16(x)})
#define constant_cpu_to_je32(x) ((jint32_t){__constant_cpu_to_le32(x)})

#define je16_to_cpu(x) (le16_to_cpu(x.v16))
#define je32_to_cpu(x) (le32_to_cpu(x.v32))
#define jemode_to_cpu(x) (le32_to_cpu(jffs2_to_os_mode((x).m)))
#else
#error wibble
#endif

#define JFFS2_MIN_NODE_HEADER sizeof(struct jffs2_raw_dirent)

struct jffs2_raw_node_ref
{
	struct jffs2_raw_node_ref *next_in_ino; 
	uint32_t flash_offset;
#undef TEST_TOTLEN
#ifdef TEST_TOTLEN
	uint32_t __totlen; 
#endif
};

#define REF_LINK_NODE ((int32_t)-1)
#define REF_EMPTY_NODE ((int32_t)-2)

#define REFS_PER_BLOCK ((255/sizeof(struct jffs2_raw_node_ref))-1)

static inline struct jffs2_raw_node_ref *ref_next(struct jffs2_raw_node_ref *ref)
{
	ref++;

	
	if (ref->flash_offset == REF_LINK_NODE) {
		ref = ref->next_in_ino;
		if (!ref)
			return ref;
	}

	
	if (ref->flash_offset == REF_EMPTY_NODE)
		return NULL;

	return ref;
}

static inline struct jffs2_inode_cache *jffs2_raw_ref_to_ic(struct jffs2_raw_node_ref *raw)
{
	while(raw->next_in_ino)
		raw = raw->next_in_ino;

	return ((struct jffs2_inode_cache *)raw);
}

#define REF_UNCHECKED	0	
#define REF_OBSOLETE	1	
#define REF_PRISTINE	2	
#define REF_NORMAL	3	
#define ref_flags(ref)		((ref)->flash_offset & 3)
#define ref_offset(ref)		((ref)->flash_offset & ~3)
#define ref_obsolete(ref)	(((ref)->flash_offset & 3) == REF_OBSOLETE)
#define mark_ref_normal(ref)    do { (ref)->flash_offset = ref_offset(ref) | REF_NORMAL; } while(0)

#define dirent_node_state(rd)	( (je32_to_cpu((rd)->ino)?REF_PRISTINE:REF_NORMAL) )


struct jffs2_inode_cache {

	struct jffs2_full_dirent *scan_dents; 
	struct jffs2_raw_node_ref *nodes;
	uint8_t class;	

	

	uint8_t flags;
	uint16_t state;
	uint32_t ino;
	struct jffs2_inode_cache *next;
#ifdef CONFIG_JFFS2_FS_XATTR
	struct jffs2_xattr_ref *xref;
#endif
	uint32_t pino_nlink;	
};

#define INO_STATE_UNCHECKED	0	
#define INO_STATE_CHECKING	1	
#define INO_STATE_PRESENT	2	
#define INO_STATE_CHECKEDABSENT	3	
#define INO_STATE_GC		4	
#define INO_STATE_READING	5	
#define INO_STATE_CLEARING	6	

#define INO_FLAGS_XATTR_CHECKED	0x01	

#define RAWNODE_CLASS_INODE_CACHE	0
#define RAWNODE_CLASS_XATTR_DATUM	1
#define RAWNODE_CLASS_XATTR_REF		2

#define INOCACHE_HASHSIZE_MIN 128
#define INOCACHE_HASHSIZE_MAX 1024

#define write_ofs(c) ((c)->nextblock->offset + (c)->sector_size - (c)->nextblock->free_size)


struct jffs2_full_dnode
{
	struct jffs2_raw_node_ref *raw;
	uint32_t ofs; 
	uint32_t size;
	uint32_t frags; 
};

struct jffs2_tmp_dnode_info
{
	struct rb_node rb;
	struct jffs2_full_dnode *fn;
	uint32_t version;
	uint32_t data_crc;
	uint32_t partial_crc;
	uint16_t csize;
	uint16_t overlapped;
};

struct jffs2_readinode_info
{
	struct rb_root tn_root;
	struct jffs2_tmp_dnode_info *mdata_tn;
	uint32_t highest_version;
	uint32_t latest_mctime;
	uint32_t mctime_ver;
	struct jffs2_full_dirent *fds;
	struct jffs2_raw_node_ref *latest_ref;
};

struct jffs2_full_dirent
{
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dirent *next;
	uint32_t version;
	uint32_t ino; 
	unsigned int nhash;
	unsigned char type;
	unsigned char name[0];
};

struct jffs2_node_frag
{
	struct rb_node rb;
	struct jffs2_full_dnode *node; 
	uint32_t size;
	uint32_t ofs; 
};

struct jffs2_eraseblock
{
	struct list_head list;
	int bad_count;
	uint32_t offset;		

	uint32_t unchecked_size;
	uint32_t used_size;
	uint32_t dirty_size;
	uint32_t wasted_size;
	uint32_t free_size;	
	uint32_t allocated_refs;
	struct jffs2_raw_node_ref *first_node;
	struct jffs2_raw_node_ref *last_node;

	struct jffs2_raw_node_ref *gc_node;	
};

static inline int jffs2_blocks_use_vmalloc(struct jffs2_sb_info *c)
{
	return ((c->flash_size / c->sector_size) * sizeof (struct jffs2_eraseblock)) > (128 * 1024);
}

#define ref_totlen(a, b, c) __jffs2_ref_totlen((a), (b), (c))

#define ALLOC_NORMAL	0	
#define ALLOC_DELETION	1	
#define ALLOC_GC	2	
#define ALLOC_NORETRY	3	

#define VERYDIRTY(c, size) ((size) >= ((c)->sector_size / 2))

#define ISDIRTY(size) ((size) >  sizeof (struct jffs2_raw_inode) + JFFS2_MIN_DATA_LEN)

#define PAD(x) (((x)+3)&~3)

static inline int jffs2_encode_dev(union jffs2_device_node *jdev, dev_t rdev)
{
	if (old_valid_dev(rdev)) {
		jdev->old_id = cpu_to_je16(old_encode_dev(rdev));
		return sizeof(jdev->old_id);
	} else {
		jdev->new_id = cpu_to_je32(new_encode_dev(rdev));
		return sizeof(jdev->new_id);
	}
}

static inline struct jffs2_node_frag *frag_first(struct rb_root *root)
{
	struct rb_node *node = rb_first(root);

	if (!node)
		return NULL;

	return rb_entry(node, struct jffs2_node_frag, rb);
}

static inline struct jffs2_node_frag *frag_last(struct rb_root *root)
{
	struct rb_node *node = rb_last(root);

	if (!node)
		return NULL;

	return rb_entry(node, struct jffs2_node_frag, rb);
}

#define frag_next(frag) rb_entry(rb_next(&(frag)->rb), struct jffs2_node_frag, rb)
#define frag_prev(frag) rb_entry(rb_prev(&(frag)->rb), struct jffs2_node_frag, rb)
#define frag_parent(frag) rb_entry(rb_parent(&(frag)->rb), struct jffs2_node_frag, rb)
#define frag_left(frag) rb_entry((frag)->rb.rb_left, struct jffs2_node_frag, rb)
#define frag_right(frag) rb_entry((frag)->rb.rb_right, struct jffs2_node_frag, rb)
#define frag_erase(frag, list) rb_erase(&frag->rb, list);

#define tn_next(tn) rb_entry(rb_next(&(tn)->rb), struct jffs2_tmp_dnode_info, rb)
#define tn_prev(tn) rb_entry(rb_prev(&(tn)->rb), struct jffs2_tmp_dnode_info, rb)
#define tn_parent(tn) rb_entry(rb_parent(&(tn)->rb), struct jffs2_tmp_dnode_info, rb)
#define tn_left(tn) rb_entry((tn)->rb.rb_left, struct jffs2_tmp_dnode_info, rb)
#define tn_right(tn) rb_entry((tn)->rb.rb_right, struct jffs2_tmp_dnode_info, rb)
#define tn_erase(tn, list) rb_erase(&tn->rb, list);
#define tn_last(list) rb_entry(rb_last(list), struct jffs2_tmp_dnode_info, rb)
#define tn_first(list) rb_entry(rb_first(list), struct jffs2_tmp_dnode_info, rb)

void jffs2_add_fd_to_list(struct jffs2_sb_info *c, struct jffs2_full_dirent *new, struct jffs2_full_dirent **list);
void jffs2_set_inocache_state(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic, int state);
struct jffs2_inode_cache *jffs2_get_ino_cache(struct jffs2_sb_info *c, uint32_t ino);
void jffs2_add_ino_cache (struct jffs2_sb_info *c, struct jffs2_inode_cache *new);
void jffs2_del_ino_cache(struct jffs2_sb_info *c, struct jffs2_inode_cache *old);
void jffs2_free_ino_caches(struct jffs2_sb_info *c);
void jffs2_free_raw_node_refs(struct jffs2_sb_info *c);
struct jffs2_node_frag *jffs2_lookup_node_frag(struct rb_root *fragtree, uint32_t offset);
void jffs2_kill_fragtree(struct rb_root *root, struct jffs2_sb_info *c_delete);
int jffs2_add_full_dnode_to_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, struct jffs2_full_dnode *fn);
uint32_t jffs2_truncate_fragtree (struct jffs2_sb_info *c, struct rb_root *list, uint32_t size);
struct jffs2_raw_node_ref *jffs2_link_node_ref(struct jffs2_sb_info *c,
					       struct jffs2_eraseblock *jeb,
					       uint32_t ofs, uint32_t len,
					       struct jffs2_inode_cache *ic);
extern uint32_t __jffs2_ref_totlen(struct jffs2_sb_info *c,
				   struct jffs2_eraseblock *jeb,
				   struct jffs2_raw_node_ref *ref);

int jffs2_thread_should_wake(struct jffs2_sb_info *c);
int jffs2_reserve_space(struct jffs2_sb_info *c, uint32_t minsize,
			uint32_t *len, int prio, uint32_t sumsize);
int jffs2_reserve_space_gc(struct jffs2_sb_info *c, uint32_t minsize,
			uint32_t *len, uint32_t sumsize);
struct jffs2_raw_node_ref *jffs2_add_physical_node_ref(struct jffs2_sb_info *c, 
						       uint32_t ofs, uint32_t len,
						       struct jffs2_inode_cache *ic);
void jffs2_complete_reservation(struct jffs2_sb_info *c);
void jffs2_mark_node_obsolete(struct jffs2_sb_info *c, struct jffs2_raw_node_ref *raw);

int jffs2_do_new_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, uint32_t mode, struct jffs2_raw_inode *ri);

struct jffs2_full_dnode *jffs2_write_dnode(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
					   struct jffs2_raw_inode *ri, const unsigned char *data,
					   uint32_t datalen, int alloc_mode);
struct jffs2_full_dirent *jffs2_write_dirent(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
					     struct jffs2_raw_dirent *rd, const unsigned char *name,
					     uint32_t namelen, int alloc_mode);
int jffs2_write_inode_range(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
			    struct jffs2_raw_inode *ri, unsigned char *buf,
			    uint32_t offset, uint32_t writelen, uint32_t *retlen);
int jffs2_do_create(struct jffs2_sb_info *c, struct jffs2_inode_info *dir_f, struct jffs2_inode_info *f,
		    struct jffs2_raw_inode *ri, const struct qstr *qstr);
int jffs2_do_unlink(struct jffs2_sb_info *c, struct jffs2_inode_info *dir_f, const char *name,
		    int namelen, struct jffs2_inode_info *dead_f, uint32_t time);
int jffs2_do_link(struct jffs2_sb_info *c, struct jffs2_inode_info *dir_f, uint32_t ino,
		   uint8_t type, const char *name, int namelen, uint32_t time);


int jffs2_do_read_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
			uint32_t ino, struct jffs2_raw_inode *latest_node);
int jffs2_do_crccheck_inode(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic);
void jffs2_do_clear_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f);

int jffs2_create_slab_caches(void);
void jffs2_destroy_slab_caches(void);

struct jffs2_full_dirent *jffs2_alloc_full_dirent(int namesize);
void jffs2_free_full_dirent(struct jffs2_full_dirent *);
struct jffs2_full_dnode *jffs2_alloc_full_dnode(void);
void jffs2_free_full_dnode(struct jffs2_full_dnode *);
struct jffs2_raw_dirent *jffs2_alloc_raw_dirent(void);
void jffs2_free_raw_dirent(struct jffs2_raw_dirent *);
struct jffs2_raw_inode *jffs2_alloc_raw_inode(void);
void jffs2_free_raw_inode(struct jffs2_raw_inode *);
struct jffs2_tmp_dnode_info *jffs2_alloc_tmp_dnode_info(void);
void jffs2_free_tmp_dnode_info(struct jffs2_tmp_dnode_info *);
int jffs2_prealloc_raw_node_refs(struct jffs2_sb_info *c,
				 struct jffs2_eraseblock *jeb, int nr);
void jffs2_free_refblock(struct jffs2_raw_node_ref *);
struct jffs2_node_frag *jffs2_alloc_node_frag(void);
void jffs2_free_node_frag(struct jffs2_node_frag *);
struct jffs2_inode_cache *jffs2_alloc_inode_cache(void);
void jffs2_free_inode_cache(struct jffs2_inode_cache *);
#ifdef CONFIG_JFFS2_FS_XATTR
struct jffs2_xattr_datum *jffs2_alloc_xattr_datum(void);
void jffs2_free_xattr_datum(struct jffs2_xattr_datum *);
struct jffs2_xattr_ref *jffs2_alloc_xattr_ref(void);
void jffs2_free_xattr_ref(struct jffs2_xattr_ref *);
#endif

int jffs2_garbage_collect_pass(struct jffs2_sb_info *c);

int jffs2_read_dnode(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
		     struct jffs2_full_dnode *fd, unsigned char *buf,
		     int ofs, int len);
int jffs2_read_inode_range(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
			   unsigned char *buf, uint32_t offset, uint32_t len);
char *jffs2_getlink(struct jffs2_sb_info *c, struct jffs2_inode_info *f);

int jffs2_scan_medium(struct jffs2_sb_info *c);
void jffs2_rotate_lists(struct jffs2_sb_info *c);
struct jffs2_inode_cache *jffs2_scan_make_ino_cache(struct jffs2_sb_info *c, uint32_t ino);
int jffs2_scan_classify_jeb(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
int jffs2_scan_dirty_space(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, uint32_t size);

int jffs2_do_mount_fs(struct jffs2_sb_info *c);

int jffs2_erase_pending_blocks(struct jffs2_sb_info *c, int count);
void jffs2_free_jeb_node_refs(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);

#ifdef CONFIG_JFFS2_FS_WRITEBUFFER
int jffs2_flush_wbuf_gc(struct jffs2_sb_info *c, uint32_t ino);
int jffs2_flush_wbuf_pad(struct jffs2_sb_info *c);
int jffs2_check_nand_cleanmarker(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
int jffs2_write_nand_cleanmarker(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
#endif

#include "debug.h"

#endif 
