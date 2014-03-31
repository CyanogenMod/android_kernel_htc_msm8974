
#include <linux/xattr.h>

#define EXT3_XATTR_MAGIC		0xEA020000

#define EXT3_XATTR_REFCOUNT_MAX		1024

#define EXT3_XATTR_INDEX_USER			1
#define EXT3_XATTR_INDEX_POSIX_ACL_ACCESS	2
#define EXT3_XATTR_INDEX_POSIX_ACL_DEFAULT	3
#define EXT3_XATTR_INDEX_TRUSTED		4
#define	EXT3_XATTR_INDEX_LUSTRE			5
#define EXT3_XATTR_INDEX_SECURITY	        6

struct ext3_xattr_header {
	__le32	h_magic;	
	__le32	h_refcount;	
	__le32	h_blocks;	
	__le32	h_hash;		
	__u32	h_reserved[4];	
};

struct ext3_xattr_ibody_header {
	__le32	h_magic;	
};

struct ext3_xattr_entry {
	__u8	e_name_len;	
	__u8	e_name_index;	
	__le16	e_value_offs;	
	__le32	e_value_block;	
	__le32	e_value_size;	
	__le32	e_hash;		
	char	e_name[0];	
};

#define EXT3_XATTR_PAD_BITS		2
#define EXT3_XATTR_PAD		(1<<EXT3_XATTR_PAD_BITS)
#define EXT3_XATTR_ROUND		(EXT3_XATTR_PAD-1)
#define EXT3_XATTR_LEN(name_len) \
	(((name_len) + EXT3_XATTR_ROUND + \
	sizeof(struct ext3_xattr_entry)) & ~EXT3_XATTR_ROUND)
#define EXT3_XATTR_NEXT(entry) \
	( (struct ext3_xattr_entry *)( \
	  (char *)(entry) + EXT3_XATTR_LEN((entry)->e_name_len)) )
#define EXT3_XATTR_SIZE(size) \
	(((size) + EXT3_XATTR_ROUND) & ~EXT3_XATTR_ROUND)

# ifdef CONFIG_EXT3_FS_XATTR

extern const struct xattr_handler ext3_xattr_user_handler;
extern const struct xattr_handler ext3_xattr_trusted_handler;
extern const struct xattr_handler ext3_xattr_acl_access_handler;
extern const struct xattr_handler ext3_xattr_acl_default_handler;
extern const struct xattr_handler ext3_xattr_security_handler;

extern ssize_t ext3_listxattr(struct dentry *, char *, size_t);

extern int ext3_xattr_get(struct inode *, int, const char *, void *, size_t);
extern int ext3_xattr_set(struct inode *, int, const char *, const void *, size_t, int);
extern int ext3_xattr_set_handle(handle_t *, struct inode *, int, const char *, const void *, size_t, int);

extern void ext3_xattr_delete_inode(handle_t *, struct inode *);
extern void ext3_xattr_put_super(struct super_block *);

extern int init_ext3_xattr(void);
extern void exit_ext3_xattr(void);

extern const struct xattr_handler *ext3_xattr_handlers[];

# else  

static inline int
ext3_xattr_get(struct inode *inode, int name_index, const char *name,
	       void *buffer, size_t size, int flags)
{
	return -EOPNOTSUPP;
}

static inline int
ext3_xattr_set(struct inode *inode, int name_index, const char *name,
	       const void *value, size_t size, int flags)
{
	return -EOPNOTSUPP;
}

static inline int
ext3_xattr_set_handle(handle_t *handle, struct inode *inode, int name_index,
	       const char *name, const void *value, size_t size, int flags)
{
	return -EOPNOTSUPP;
}

static inline void
ext3_xattr_delete_inode(handle_t *handle, struct inode *inode)
{
}

static inline void
ext3_xattr_put_super(struct super_block *sb)
{
}

static inline int
init_ext3_xattr(void)
{
	return 0;
}

static inline void
exit_ext3_xattr(void)
{
}

#define ext3_xattr_handlers	NULL

# endif  

#ifdef CONFIG_EXT3_FS_SECURITY
extern int ext3_init_security(handle_t *handle, struct inode *inode,
			      struct inode *dir, const struct qstr *qstr);
#else
static inline int ext3_init_security(handle_t *handle, struct inode *inode,
				     struct inode *dir, const struct qstr *qstr)
{
	return 0;
}
#endif
