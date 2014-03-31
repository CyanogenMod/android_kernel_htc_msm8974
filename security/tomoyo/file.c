/*
 * security/tomoyo/file.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/slab.h>

static const u8 tomoyo_p2mac[TOMOYO_MAX_PATH_OPERATION] = {
	[TOMOYO_TYPE_EXECUTE]    = TOMOYO_MAC_FILE_EXECUTE,
	[TOMOYO_TYPE_READ]       = TOMOYO_MAC_FILE_OPEN,
	[TOMOYO_TYPE_WRITE]      = TOMOYO_MAC_FILE_OPEN,
	[TOMOYO_TYPE_APPEND]     = TOMOYO_MAC_FILE_OPEN,
	[TOMOYO_TYPE_UNLINK]     = TOMOYO_MAC_FILE_UNLINK,
	[TOMOYO_TYPE_GETATTR]    = TOMOYO_MAC_FILE_GETATTR,
	[TOMOYO_TYPE_RMDIR]      = TOMOYO_MAC_FILE_RMDIR,
	[TOMOYO_TYPE_TRUNCATE]   = TOMOYO_MAC_FILE_TRUNCATE,
	[TOMOYO_TYPE_SYMLINK]    = TOMOYO_MAC_FILE_SYMLINK,
	[TOMOYO_TYPE_CHROOT]     = TOMOYO_MAC_FILE_CHROOT,
	[TOMOYO_TYPE_UMOUNT]     = TOMOYO_MAC_FILE_UMOUNT,
};

const u8 tomoyo_pnnn2mac[TOMOYO_MAX_MKDEV_OPERATION] = {
	[TOMOYO_TYPE_MKBLOCK] = TOMOYO_MAC_FILE_MKBLOCK,
	[TOMOYO_TYPE_MKCHAR]  = TOMOYO_MAC_FILE_MKCHAR,
};

const u8 tomoyo_pp2mac[TOMOYO_MAX_PATH2_OPERATION] = {
	[TOMOYO_TYPE_LINK]       = TOMOYO_MAC_FILE_LINK,
	[TOMOYO_TYPE_RENAME]     = TOMOYO_MAC_FILE_RENAME,
	[TOMOYO_TYPE_PIVOT_ROOT] = TOMOYO_MAC_FILE_PIVOT_ROOT,
};

const u8 tomoyo_pn2mac[TOMOYO_MAX_PATH_NUMBER_OPERATION] = {
	[TOMOYO_TYPE_CREATE] = TOMOYO_MAC_FILE_CREATE,
	[TOMOYO_TYPE_MKDIR]  = TOMOYO_MAC_FILE_MKDIR,
	[TOMOYO_TYPE_MKFIFO] = TOMOYO_MAC_FILE_MKFIFO,
	[TOMOYO_TYPE_MKSOCK] = TOMOYO_MAC_FILE_MKSOCK,
	[TOMOYO_TYPE_IOCTL]  = TOMOYO_MAC_FILE_IOCTL,
	[TOMOYO_TYPE_CHMOD]  = TOMOYO_MAC_FILE_CHMOD,
	[TOMOYO_TYPE_CHOWN]  = TOMOYO_MAC_FILE_CHOWN,
	[TOMOYO_TYPE_CHGRP]  = TOMOYO_MAC_FILE_CHGRP,
};

void tomoyo_put_name_union(struct tomoyo_name_union *ptr)
{
	tomoyo_put_group(ptr->group);
	tomoyo_put_name(ptr->filename);
}

const struct tomoyo_path_info *
tomoyo_compare_name_union(const struct tomoyo_path_info *name,
			  const struct tomoyo_name_union *ptr)
{
	if (ptr->group)
		return tomoyo_path_matches_group(name, ptr->group);
	if (tomoyo_path_matches_pattern(name, ptr->filename))
		return ptr->filename;
	return NULL;
}

void tomoyo_put_number_union(struct tomoyo_number_union *ptr)
{
	tomoyo_put_group(ptr->group);
}

bool tomoyo_compare_number_union(const unsigned long value,
				 const struct tomoyo_number_union *ptr)
{
	if (ptr->group)
		return tomoyo_number_matches_group(value, value, ptr->group);
	return value >= ptr->values[0] && value <= ptr->values[1];
}

static void tomoyo_add_slash(struct tomoyo_path_info *buf)
{
	if (buf->is_dir)
		return;
	strcat((char *) buf->name, "/");
	tomoyo_fill_path_info(buf);
}

static bool tomoyo_get_realpath(struct tomoyo_path_info *buf, struct path *path)
{
	buf->name = tomoyo_realpath_from_path(path);
	if (buf->name) {
		tomoyo_fill_path_info(buf);
		return true;
	}
	return false;
}

static int tomoyo_audit_path_log(struct tomoyo_request_info *r)
{
	return tomoyo_supervisor(r, "file %s %s\n", tomoyo_path_keyword
				 [r->param.path.operation],
				 r->param.path.filename->name);
}

static int tomoyo_audit_path2_log(struct tomoyo_request_info *r)
{
	return tomoyo_supervisor(r, "file %s %s %s\n", tomoyo_mac_keywords
				 [tomoyo_pp2mac[r->param.path2.operation]],
				 r->param.path2.filename1->name,
				 r->param.path2.filename2->name);
}

static int tomoyo_audit_mkdev_log(struct tomoyo_request_info *r)
{
	return tomoyo_supervisor(r, "file %s %s 0%o %u %u\n",
				 tomoyo_mac_keywords
				 [tomoyo_pnnn2mac[r->param.mkdev.operation]],
				 r->param.mkdev.filename->name,
				 r->param.mkdev.mode, r->param.mkdev.major,
				 r->param.mkdev.minor);
}

static int tomoyo_audit_path_number_log(struct tomoyo_request_info *r)
{
	const u8 type = r->param.path_number.operation;
	u8 radix;
	char buffer[64];
	switch (type) {
	case TOMOYO_TYPE_CREATE:
	case TOMOYO_TYPE_MKDIR:
	case TOMOYO_TYPE_MKFIFO:
	case TOMOYO_TYPE_MKSOCK:
	case TOMOYO_TYPE_CHMOD:
		radix = TOMOYO_VALUE_TYPE_OCTAL;
		break;
	case TOMOYO_TYPE_IOCTL:
		radix = TOMOYO_VALUE_TYPE_HEXADECIMAL;
		break;
	default:
		radix = TOMOYO_VALUE_TYPE_DECIMAL;
		break;
	}
	tomoyo_print_ulong(buffer, sizeof(buffer), r->param.path_number.number,
			   radix);
	return tomoyo_supervisor(r, "file %s %s %s\n", tomoyo_mac_keywords
				 [tomoyo_pn2mac[type]],
				 r->param.path_number.filename->name, buffer);
}

static bool tomoyo_check_path_acl(struct tomoyo_request_info *r,
				  const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_path_acl *acl = container_of(ptr, typeof(*acl),
							 head);
	if (acl->perm & (1 << r->param.path.operation)) {
		r->param.path.matched_path =
			tomoyo_compare_name_union(r->param.path.filename,
						  &acl->name);
		return r->param.path.matched_path != NULL;
	}
	return false;
}

static bool tomoyo_check_path_number_acl(struct tomoyo_request_info *r,
					 const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_path_number_acl *acl =
		container_of(ptr, typeof(*acl), head);
	return (acl->perm & (1 << r->param.path_number.operation)) &&
		tomoyo_compare_number_union(r->param.path_number.number,
					    &acl->number) &&
		tomoyo_compare_name_union(r->param.path_number.filename,
					  &acl->name);
}

static bool tomoyo_check_path2_acl(struct tomoyo_request_info *r,
				   const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_path2_acl *acl =
		container_of(ptr, typeof(*acl), head);
	return (acl->perm & (1 << r->param.path2.operation)) &&
		tomoyo_compare_name_union(r->param.path2.filename1, &acl->name1)
		&& tomoyo_compare_name_union(r->param.path2.filename2,
					     &acl->name2);
}

static bool tomoyo_check_mkdev_acl(struct tomoyo_request_info *r,
				   const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_mkdev_acl *acl =
		container_of(ptr, typeof(*acl), head);
	return (acl->perm & (1 << r->param.mkdev.operation)) &&
		tomoyo_compare_number_union(r->param.mkdev.mode,
					    &acl->mode) &&
		tomoyo_compare_number_union(r->param.mkdev.major,
					    &acl->major) &&
		tomoyo_compare_number_union(r->param.mkdev.minor,
					    &acl->minor) &&
		tomoyo_compare_name_union(r->param.mkdev.filename,
					  &acl->name);
}

static bool tomoyo_same_path_acl(const struct tomoyo_acl_info *a,
				 const struct tomoyo_acl_info *b)
{
	const struct tomoyo_path_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_path_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->name, &p2->name);
}

static bool tomoyo_merge_path_acl(struct tomoyo_acl_info *a,
				  struct tomoyo_acl_info *b,
				  const bool is_delete)
{
	u16 * const a_perm = &container_of(a, struct tomoyo_path_acl, head)
		->perm;
	u16 perm = *a_perm;
	const u16 b_perm = container_of(b, struct tomoyo_path_acl, head)->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

static int tomoyo_update_path_acl(const u16 perm,
				  struct tomoyo_acl_param *param)
{
	struct tomoyo_path_acl e = {
		.head.type = TOMOYO_TYPE_PATH_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_path_acl,
					     tomoyo_merge_path_acl);
	tomoyo_put_name_union(&e.name);
	return error;
}

static bool tomoyo_same_mkdev_acl(const struct tomoyo_acl_info *a,
					 const struct tomoyo_acl_info *b)
{
	const struct tomoyo_mkdev_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_mkdev_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->name, &p2->name) &&
		tomoyo_same_number_union(&p1->mode, &p2->mode) &&
		tomoyo_same_number_union(&p1->major, &p2->major) &&
		tomoyo_same_number_union(&p1->minor, &p2->minor);
}

static bool tomoyo_merge_mkdev_acl(struct tomoyo_acl_info *a,
				   struct tomoyo_acl_info *b,
				   const bool is_delete)
{
	u8 *const a_perm = &container_of(a, struct tomoyo_mkdev_acl,
					 head)->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_mkdev_acl, head)
		->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

static int tomoyo_update_mkdev_acl(const u8 perm,
				   struct tomoyo_acl_param *param)
{
	struct tomoyo_mkdev_acl e = {
		.head.type = TOMOYO_TYPE_MKDEV_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name) ||
	    !tomoyo_parse_number_union(param, &e.mode) ||
	    !tomoyo_parse_number_union(param, &e.major) ||
	    !tomoyo_parse_number_union(param, &e.minor))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_mkdev_acl,
					     tomoyo_merge_mkdev_acl);
	tomoyo_put_name_union(&e.name);
	tomoyo_put_number_union(&e.mode);
	tomoyo_put_number_union(&e.major);
	tomoyo_put_number_union(&e.minor);
	return error;
}

static bool tomoyo_same_path2_acl(const struct tomoyo_acl_info *a,
				  const struct tomoyo_acl_info *b)
{
	const struct tomoyo_path2_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_path2_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->name1, &p2->name1) &&
		tomoyo_same_name_union(&p1->name2, &p2->name2);
}

static bool tomoyo_merge_path2_acl(struct tomoyo_acl_info *a,
				   struct tomoyo_acl_info *b,
				   const bool is_delete)
{
	u8 * const a_perm = &container_of(a, struct tomoyo_path2_acl, head)
		->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_path2_acl, head)->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

static int tomoyo_update_path2_acl(const u8 perm,
				   struct tomoyo_acl_param *param)
{
	struct tomoyo_path2_acl e = {
		.head.type = TOMOYO_TYPE_PATH2_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name1) ||
	    !tomoyo_parse_name_union(param, &e.name2))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_path2_acl,
					     tomoyo_merge_path2_acl);
	tomoyo_put_name_union(&e.name1);
	tomoyo_put_name_union(&e.name2);
	return error;
}

static int tomoyo_path_permission(struct tomoyo_request_info *r, u8 operation,
				  const struct tomoyo_path_info *filename)
{
	int error;

	r->type = tomoyo_p2mac[operation];
	r->mode = tomoyo_get_mode(r->domain->ns, r->profile, r->type);
	if (r->mode == TOMOYO_CONFIG_DISABLED)
		return 0;
	r->param_type = TOMOYO_TYPE_PATH_ACL;
	r->param.path.filename = filename;
	r->param.path.operation = operation;
	do {
		tomoyo_check_acl(r, tomoyo_check_path_acl);
		error = tomoyo_audit_path_log(r);
	} while (error == TOMOYO_RETRY_REQUEST);
	return error;
}

int tomoyo_execute_permission(struct tomoyo_request_info *r,
			      const struct tomoyo_path_info *filename)
{
	r->type = TOMOYO_MAC_FILE_EXECUTE;
	r->mode = tomoyo_get_mode(r->domain->ns, r->profile, r->type);
	r->param_type = TOMOYO_TYPE_PATH_ACL;
	r->param.path.filename = filename;
	r->param.path.operation = TOMOYO_TYPE_EXECUTE;
	tomoyo_check_acl(r, tomoyo_check_path_acl);
	r->ee->transition = r->matched_acl && r->matched_acl->cond ?
		r->matched_acl->cond->transit : NULL;
	if (r->mode != TOMOYO_CONFIG_DISABLED)
		return tomoyo_audit_path_log(r);
	return 0;
}

static bool tomoyo_same_path_number_acl(const struct tomoyo_acl_info *a,
					const struct tomoyo_acl_info *b)
{
	const struct tomoyo_path_number_acl *p1 = container_of(a, typeof(*p1),
							       head);
	const struct tomoyo_path_number_acl *p2 = container_of(b, typeof(*p2),
							       head);
	return tomoyo_same_name_union(&p1->name, &p2->name) &&
		tomoyo_same_number_union(&p1->number, &p2->number);
}

static bool tomoyo_merge_path_number_acl(struct tomoyo_acl_info *a,
					 struct tomoyo_acl_info *b,
					 const bool is_delete)
{
	u8 * const a_perm = &container_of(a, struct tomoyo_path_number_acl,
					  head)->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_path_number_acl, head)
		->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

static int tomoyo_update_path_number_acl(const u8 perm,
					 struct tomoyo_acl_param *param)
{
	struct tomoyo_path_number_acl e = {
		.head.type = TOMOYO_TYPE_PATH_NUMBER_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name) ||
	    !tomoyo_parse_number_union(param, &e.number))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_path_number_acl,
					     tomoyo_merge_path_number_acl);
	tomoyo_put_name_union(&e.name);
	tomoyo_put_number_union(&e.number);
	return error;
}

int tomoyo_path_number_perm(const u8 type, struct path *path,
			    unsigned long number)
{
	struct tomoyo_request_info r;
	struct tomoyo_obj_info obj = {
		.path1 = *path,
	};
	int error = -ENOMEM;
	struct tomoyo_path_info buf;
	int idx;

	if (tomoyo_init_request_info(&r, NULL, tomoyo_pn2mac[type])
	    == TOMOYO_CONFIG_DISABLED || !path->dentry)
		return 0;
	idx = tomoyo_read_lock();
	if (!tomoyo_get_realpath(&buf, path))
		goto out;
	r.obj = &obj;
	if (type == TOMOYO_TYPE_MKDIR)
		tomoyo_add_slash(&buf);
	r.param_type = TOMOYO_TYPE_PATH_NUMBER_ACL;
	r.param.path_number.operation = type;
	r.param.path_number.filename = &buf;
	r.param.path_number.number = number;
	do {
		tomoyo_check_acl(&r, tomoyo_check_path_number_acl);
		error = tomoyo_audit_path_number_log(&r);
	} while (error == TOMOYO_RETRY_REQUEST);
	kfree(buf.name);
 out:
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

int tomoyo_check_open_permission(struct tomoyo_domain_info *domain,
				 struct path *path, const int flag)
{
	const u8 acc_mode = ACC_MODE(flag);
	int error = 0;
	struct tomoyo_path_info buf;
	struct tomoyo_request_info r;
	struct tomoyo_obj_info obj = {
		.path1 = *path,
	};
	int idx;

	buf.name = NULL;
	r.mode = TOMOYO_CONFIG_DISABLED;
	idx = tomoyo_read_lock();
	if (acc_mode &&
	    tomoyo_init_request_info(&r, domain, TOMOYO_MAC_FILE_OPEN)
	    != TOMOYO_CONFIG_DISABLED) {
		if (!tomoyo_get_realpath(&buf, path)) {
			error = -ENOMEM;
			goto out;
		}
		r.obj = &obj;
		if (acc_mode & MAY_READ)
			error = tomoyo_path_permission(&r, TOMOYO_TYPE_READ,
						       &buf);
		if (!error && (acc_mode & MAY_WRITE))
			error = tomoyo_path_permission(&r, (flag & O_APPEND) ?
						       TOMOYO_TYPE_APPEND :
						       TOMOYO_TYPE_WRITE,
						       &buf);
	}
 out:
	kfree(buf.name);
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

int tomoyo_path_perm(const u8 operation, struct path *path, const char *target)
{
	struct tomoyo_request_info r;
	struct tomoyo_obj_info obj = {
		.path1 = *path,
	};
	int error;
	struct tomoyo_path_info buf;
	bool is_enforce;
	struct tomoyo_path_info symlink_target;
	int idx;

	if (tomoyo_init_request_info(&r, NULL, tomoyo_p2mac[operation])
	    == TOMOYO_CONFIG_DISABLED)
		return 0;
	is_enforce = (r.mode == TOMOYO_CONFIG_ENFORCING);
	error = -ENOMEM;
	buf.name = NULL;
	idx = tomoyo_read_lock();
	if (!tomoyo_get_realpath(&buf, path))
		goto out;
	r.obj = &obj;
	switch (operation) {
	case TOMOYO_TYPE_RMDIR:
	case TOMOYO_TYPE_CHROOT:
		tomoyo_add_slash(&buf);
		break;
	case TOMOYO_TYPE_SYMLINK:
		symlink_target.name = tomoyo_encode(target);
		if (!symlink_target.name)
			goto out;
		tomoyo_fill_path_info(&symlink_target);
		obj.symlink_target = &symlink_target;
		break;
	}
	error = tomoyo_path_permission(&r, operation, &buf);
	if (operation == TOMOYO_TYPE_SYMLINK)
		kfree(symlink_target.name);
 out:
	kfree(buf.name);
	tomoyo_read_unlock(idx);
	if (!is_enforce)
		error = 0;
	return error;
}

int tomoyo_mkdev_perm(const u8 operation, struct path *path,
		      const unsigned int mode, unsigned int dev)
{
	struct tomoyo_request_info r;
	struct tomoyo_obj_info obj = {
		.path1 = *path,
	};
	int error = -ENOMEM;
	struct tomoyo_path_info buf;
	int idx;

	if (tomoyo_init_request_info(&r, NULL, tomoyo_pnnn2mac[operation])
	    == TOMOYO_CONFIG_DISABLED)
		return 0;
	idx = tomoyo_read_lock();
	error = -ENOMEM;
	if (tomoyo_get_realpath(&buf, path)) {
		r.obj = &obj;
		dev = new_decode_dev(dev);
		r.param_type = TOMOYO_TYPE_MKDEV_ACL;
		r.param.mkdev.filename = &buf;
		r.param.mkdev.operation = operation;
		r.param.mkdev.mode = mode;
		r.param.mkdev.major = MAJOR(dev);
		r.param.mkdev.minor = MINOR(dev);
		tomoyo_check_acl(&r, tomoyo_check_mkdev_acl);
		error = tomoyo_audit_mkdev_log(&r);
		kfree(buf.name);
	}
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

int tomoyo_path2_perm(const u8 operation, struct path *path1,
		      struct path *path2)
{
	int error = -ENOMEM;
	struct tomoyo_path_info buf1;
	struct tomoyo_path_info buf2;
	struct tomoyo_request_info r;
	struct tomoyo_obj_info obj = {
		.path1 = *path1,
		.path2 = *path2,
	};
	int idx;

	if (tomoyo_init_request_info(&r, NULL, tomoyo_pp2mac[operation])
	    == TOMOYO_CONFIG_DISABLED)
		return 0;
	buf1.name = NULL;
	buf2.name = NULL;
	idx = tomoyo_read_lock();
	if (!tomoyo_get_realpath(&buf1, path1) ||
	    !tomoyo_get_realpath(&buf2, path2))
		goto out;
	switch (operation) {
		struct dentry *dentry;
	case TOMOYO_TYPE_RENAME:
	case TOMOYO_TYPE_LINK:
		dentry = path1->dentry;
		if (!dentry->d_inode || !S_ISDIR(dentry->d_inode->i_mode))
			break;
		
	case TOMOYO_TYPE_PIVOT_ROOT:
		tomoyo_add_slash(&buf1);
		tomoyo_add_slash(&buf2);
		break;
	}
	r.obj = &obj;
	r.param_type = TOMOYO_TYPE_PATH2_ACL;
	r.param.path2.operation = operation;
	r.param.path2.filename1 = &buf1;
	r.param.path2.filename2 = &buf2;
	do {
		tomoyo_check_acl(&r, tomoyo_check_path2_acl);
		error = tomoyo_audit_path2_log(&r);
	} while (error == TOMOYO_RETRY_REQUEST);
 out:
	kfree(buf1.name);
	kfree(buf2.name);
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

static bool tomoyo_same_mount_acl(const struct tomoyo_acl_info *a,
				  const struct tomoyo_acl_info *b)
{
	const struct tomoyo_mount_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_mount_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->dev_name, &p2->dev_name) &&
		tomoyo_same_name_union(&p1->dir_name, &p2->dir_name) &&
		tomoyo_same_name_union(&p1->fs_type, &p2->fs_type) &&
		tomoyo_same_number_union(&p1->flags, &p2->flags);
}

static int tomoyo_update_mount_acl(struct tomoyo_acl_param *param)
{
	struct tomoyo_mount_acl e = { .head.type = TOMOYO_TYPE_MOUNT_ACL };
	int error;
	if (!tomoyo_parse_name_union(param, &e.dev_name) ||
	    !tomoyo_parse_name_union(param, &e.dir_name) ||
	    !tomoyo_parse_name_union(param, &e.fs_type) ||
	    !tomoyo_parse_number_union(param, &e.flags))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_mount_acl, NULL);
	tomoyo_put_name_union(&e.dev_name);
	tomoyo_put_name_union(&e.dir_name);
	tomoyo_put_name_union(&e.fs_type);
	tomoyo_put_number_union(&e.flags);
	return error;
}

int tomoyo_write_file(struct tomoyo_acl_param *param)
{
	u16 perm = 0;
	u8 type;
	const char *operation = tomoyo_read_token(param);
	for (type = 0; type < TOMOYO_MAX_PATH_OPERATION; type++)
		if (tomoyo_permstr(operation, tomoyo_path_keyword[type]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_path_acl(perm, param);
	for (type = 0; type < TOMOYO_MAX_PATH2_OPERATION; type++)
		if (tomoyo_permstr(operation,
				   tomoyo_mac_keywords[tomoyo_pp2mac[type]]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_path2_acl(perm, param);
	for (type = 0; type < TOMOYO_MAX_PATH_NUMBER_OPERATION; type++)
		if (tomoyo_permstr(operation,
				   tomoyo_mac_keywords[tomoyo_pn2mac[type]]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_path_number_acl(perm, param);
	for (type = 0; type < TOMOYO_MAX_MKDEV_OPERATION; type++)
		if (tomoyo_permstr(operation,
				   tomoyo_mac_keywords[tomoyo_pnnn2mac[type]]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_mkdev_acl(perm, param);
	if (tomoyo_permstr(operation,
			   tomoyo_mac_keywords[TOMOYO_MAC_FILE_MOUNT]))
		return tomoyo_update_mount_acl(param);
	return -EINVAL;
}
