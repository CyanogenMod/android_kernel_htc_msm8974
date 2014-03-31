/*
 * security/tomoyo/util.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include <linux/slab.h>
#include "common.h"

DEFINE_MUTEX(tomoyo_policy_lock);

bool tomoyo_policy_loaded;

const u8 tomoyo_index2category[TOMOYO_MAX_MAC_INDEX] = {
	
	[TOMOYO_MAC_FILE_EXECUTE]    = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_OPEN]       = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_CREATE]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_UNLINK]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_GETATTR]    = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_MKDIR]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_RMDIR]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_MKFIFO]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_MKSOCK]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_TRUNCATE]   = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_SYMLINK]    = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_MKBLOCK]    = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_MKCHAR]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_LINK]       = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_RENAME]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_CHMOD]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_CHOWN]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_CHGRP]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_IOCTL]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_CHROOT]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_MOUNT]      = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_UMOUNT]     = TOMOYO_MAC_CATEGORY_FILE,
	[TOMOYO_MAC_FILE_PIVOT_ROOT] = TOMOYO_MAC_CATEGORY_FILE,
	
	[TOMOYO_MAC_NETWORK_INET_STREAM_BIND]       =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_INET_STREAM_LISTEN]     =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_INET_STREAM_CONNECT]    =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_INET_DGRAM_BIND]        =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_INET_DGRAM_SEND]        =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_INET_RAW_BIND]          =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_INET_RAW_SEND]          =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_STREAM_BIND]       =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_STREAM_LISTEN]     =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_STREAM_CONNECT]    =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_DGRAM_BIND]        =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_DGRAM_SEND]        =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_BIND]    =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_LISTEN]  =
	TOMOYO_MAC_CATEGORY_NETWORK,
	[TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_CONNECT] =
	TOMOYO_MAC_CATEGORY_NETWORK,
	
	[TOMOYO_MAC_ENVIRON]         = TOMOYO_MAC_CATEGORY_MISC,
};

void tomoyo_convert_time(time_t time, struct tomoyo_time *stamp)
{
	static const u16 tomoyo_eom[2][12] = {
		{ 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
		{ 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};
	u16 y;
	u8 m;
	bool r;
	stamp->sec = time % 60;
	time /= 60;
	stamp->min = time % 60;
	time /= 60;
	stamp->hour = time % 24;
	time /= 24;
	for (y = 1970; ; y++) {
		const unsigned short days = (y & 3) ? 365 : 366;
		if (time < days)
			break;
		time -= days;
	}
	r = (y & 3) == 0;
	for (m = 0; m < 11 && time >= tomoyo_eom[r][m]; m++)
		;
	if (m)
		time -= tomoyo_eom[r][m - 1];
	stamp->year = y;
	stamp->month = ++m;
	stamp->day = ++time;
}

bool tomoyo_permstr(const char *string, const char *keyword)
{
	const char *cp = strstr(string, keyword);
	if (cp)
		return cp == string || *(cp - 1) == '/';
	return false;
}

char *tomoyo_read_token(struct tomoyo_acl_param *param)
{
	char *pos = param->data;
	char *del = strchr(pos, ' ');
	if (del)
		*del++ = '\0';
	else
		del = pos + strlen(pos);
	param->data = del;
	return pos;
}

const struct tomoyo_path_info *tomoyo_get_domainname
(struct tomoyo_acl_param *param)
{
	char *start = param->data;
	char *pos = start;
	while (*pos) {
		if (*pos++ != ' ' || *pos++ == '/')
			continue;
		pos -= 2;
		*pos++ = '\0';
		break;
	}
	param->data = pos;
	if (tomoyo_correct_domain(start))
		return tomoyo_get_name(start);
	return NULL;
}

u8 tomoyo_parse_ulong(unsigned long *result, char **str)
{
	const char *cp = *str;
	char *ep;
	int base = 10;
	if (*cp == '0') {
		char c = *(cp + 1);
		if (c == 'x' || c == 'X') {
			base = 16;
			cp += 2;
		} else if (c >= '0' && c <= '7') {
			base = 8;
			cp++;
		}
	}
	*result = simple_strtoul(cp, &ep, base);
	if (cp == ep)
		return TOMOYO_VALUE_TYPE_INVALID;
	*str = ep;
	switch (base) {
	case 16:
		return TOMOYO_VALUE_TYPE_HEXADECIMAL;
	case 8:
		return TOMOYO_VALUE_TYPE_OCTAL;
	default:
		return TOMOYO_VALUE_TYPE_DECIMAL;
	}
}

void tomoyo_print_ulong(char *buffer, const int buffer_len,
			const unsigned long value, const u8 type)
{
	if (type == TOMOYO_VALUE_TYPE_DECIMAL)
		snprintf(buffer, buffer_len, "%lu", value);
	else if (type == TOMOYO_VALUE_TYPE_OCTAL)
		snprintf(buffer, buffer_len, "0%lo", value);
	else if (type == TOMOYO_VALUE_TYPE_HEXADECIMAL)
		snprintf(buffer, buffer_len, "0x%lX", value);
	else
		snprintf(buffer, buffer_len, "type(%u)", type);
}

bool tomoyo_parse_name_union(struct tomoyo_acl_param *param,
			     struct tomoyo_name_union *ptr)
{
	char *filename;
	if (param->data[0] == '@') {
		param->data++;
		ptr->group = tomoyo_get_group(param, TOMOYO_PATH_GROUP);
		return ptr->group != NULL;
	}
	filename = tomoyo_read_token(param);
	if (!tomoyo_correct_word(filename))
		return false;
	ptr->filename = tomoyo_get_name(filename);
	return ptr->filename != NULL;
}

bool tomoyo_parse_number_union(struct tomoyo_acl_param *param,
			       struct tomoyo_number_union *ptr)
{
	char *data;
	u8 type;
	unsigned long v;
	memset(ptr, 0, sizeof(*ptr));
	if (param->data[0] == '@') {
		param->data++;
		ptr->group = tomoyo_get_group(param, TOMOYO_NUMBER_GROUP);
		return ptr->group != NULL;
	}
	data = tomoyo_read_token(param);
	type = tomoyo_parse_ulong(&v, &data);
	if (type == TOMOYO_VALUE_TYPE_INVALID)
		return false;
	ptr->values[0] = v;
	ptr->value_type[0] = type;
	if (!*data) {
		ptr->values[1] = v;
		ptr->value_type[1] = type;
		return true;
	}
	if (*data++ != '-')
		return false;
	type = tomoyo_parse_ulong(&v, &data);
	if (type == TOMOYO_VALUE_TYPE_INVALID || *data || ptr->values[0] > v)
		return false;
	ptr->values[1] = v;
	ptr->value_type[1] = type;
	return true;
}

static inline bool tomoyo_byte_range(const char *str)
{
	return *str >= '0' && *str++ <= '3' &&
		*str >= '0' && *str++ <= '7' &&
		*str >= '0' && *str <= '7';
}

static inline bool tomoyo_alphabet_char(const char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline u8 tomoyo_make_byte(const u8 c1, const u8 c2, const u8 c3)
{
	return ((c1 - '0') << 6) + ((c2 - '0') << 3) + (c3 - '0');
}

static inline bool tomoyo_valid(const unsigned char c)
{
	return c > ' ' && c < 127;
}

static inline bool tomoyo_invalid(const unsigned char c)
{
	return c && (c <= ' ' || c >= 127);
}

bool tomoyo_str_starts(char **src, const char *find)
{
	const int len = strlen(find);
	char *tmp = *src;

	if (strncmp(tmp, find, len))
		return false;
	tmp += len;
	*src = tmp;
	return true;
}

void tomoyo_normalize_line(unsigned char *buffer)
{
	unsigned char *sp = buffer;
	unsigned char *dp = buffer;
	bool first = true;

	while (tomoyo_invalid(*sp))
		sp++;
	while (*sp) {
		if (!first)
			*dp++ = ' ';
		first = false;
		while (tomoyo_valid(*sp))
			*dp++ = *sp++;
		while (tomoyo_invalid(*sp))
			sp++;
	}
	*dp = '\0';
}

static bool tomoyo_correct_word2(const char *string, size_t len)
{
	const char *const start = string;
	bool in_repetition = false;
	unsigned char c;
	unsigned char d;
	unsigned char e;
	if (!len)
		goto out;
	while (len--) {
		c = *string++;
		if (c == '\\') {
			if (!len--)
				goto out;
			c = *string++;
			switch (c) {
			case '\\':  
				continue;
			case '$':   
			case '+':   
			case '?':   
			case '*':   
			case '@':   
			case 'x':   
			case 'X':   
			case 'a':   
			case 'A':   
			case '-':   
				continue;
			case '{':   
				if (string - 3 < start || *(string - 3) != '/')
					break;
				in_repetition = true;
				continue;
			case '}':   
				if (*string != '/')
					break;
				if (!in_repetition)
					break;
				in_repetition = false;
				continue;
			case '0':   
			case '1':
			case '2':
			case '3':
				if (!len-- || !len--)
					break;
				d = *string++;
				e = *string++;
				if (d < '0' || d > '7' || e < '0' || e > '7')
					break;
				c = tomoyo_make_byte(c, d, e);
				if (c <= ' ' || c >= 127)
					continue;
			}
			goto out;
		} else if (in_repetition && c == '/') {
			goto out;
		} else if (c <= ' ' || c >= 127) {
			goto out;
		}
	}
	if (in_repetition)
		goto out;
	return true;
 out:
	return false;
}

bool tomoyo_correct_word(const char *string)
{
	return tomoyo_correct_word2(string, strlen(string));
}

bool tomoyo_correct_path(const char *filename)
{
	return *filename == '/' && tomoyo_correct_word(filename);
}

bool tomoyo_correct_domain(const unsigned char *domainname)
{
	if (!domainname || !tomoyo_domain_def(domainname))
		return false;
	domainname = strchr(domainname, ' ');
	if (!domainname++)
		return true;
	while (1) {
		const unsigned char *cp = strchr(domainname, ' ');
		if (!cp)
			break;
		if (*domainname != '/' ||
		    !tomoyo_correct_word2(domainname, cp - domainname))
			return false;
		domainname = cp + 1;
	}
	return tomoyo_correct_path(domainname);
}

bool tomoyo_domain_def(const unsigned char *buffer)
{
	const unsigned char *cp;
	int len;
	if (*buffer != '<')
		return false;
	cp = strchr(buffer, ' ');
	if (!cp)
		len = strlen(buffer);
	else
		len = cp - buffer;
	if (buffer[len - 1] != '>' ||
	    !tomoyo_correct_word2(buffer + 1, len - 2))
		return false;
	return true;
}

struct tomoyo_domain_info *tomoyo_find_domain(const char *domainname)
{
	struct tomoyo_domain_info *domain;
	struct tomoyo_path_info name;

	name.name = domainname;
	tomoyo_fill_path_info(&name);
	list_for_each_entry_rcu(domain, &tomoyo_domain_list, list) {
		if (!domain->is_deleted &&
		    !tomoyo_pathcmp(&name, domain->domainname))
			return domain;
	}
	return NULL;
}

static int tomoyo_const_part_length(const char *filename)
{
	char c;
	int len = 0;

	if (!filename)
		return 0;
	while ((c = *filename++) != '\0') {
		if (c != '\\') {
			len++;
			continue;
		}
		c = *filename++;
		switch (c) {
		case '\\':  
			len += 2;
			continue;
		case '0':   
		case '1':
		case '2':
		case '3':
			c = *filename++;
			if (c < '0' || c > '7')
				break;
			c = *filename++;
			if (c < '0' || c > '7')
				break;
			len += 4;
			continue;
		}
		break;
	}
	return len;
}

void tomoyo_fill_path_info(struct tomoyo_path_info *ptr)
{
	const char *name = ptr->name;
	const int len = strlen(name);

	ptr->const_len = tomoyo_const_part_length(name);
	ptr->is_dir = len && (name[len - 1] == '/');
	ptr->is_patterned = (ptr->const_len < len);
	ptr->hash = full_name_hash(name, len);
}

static bool tomoyo_file_matches_pattern2(const char *filename,
					 const char *filename_end,
					 const char *pattern,
					 const char *pattern_end)
{
	while (filename < filename_end && pattern < pattern_end) {
		char c;
		if (*pattern != '\\') {
			if (*filename++ != *pattern++)
				return false;
			continue;
		}
		c = *filename;
		pattern++;
		switch (*pattern) {
			int i;
			int j;
		case '?':
			if (c == '/') {
				return false;
			} else if (c == '\\') {
				if (filename[1] == '\\')
					filename++;
				else if (tomoyo_byte_range(filename + 1))
					filename += 3;
				else
					return false;
			}
			break;
		case '\\':
			if (c != '\\')
				return false;
			if (*++filename != '\\')
				return false;
			break;
		case '+':
			if (!isdigit(c))
				return false;
			break;
		case 'x':
			if (!isxdigit(c))
				return false;
			break;
		case 'a':
			if (!tomoyo_alphabet_char(c))
				return false;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
			if (c == '\\' && tomoyo_byte_range(filename + 1)
			    && strncmp(filename + 1, pattern, 3) == 0) {
				filename += 3;
				pattern += 2;
				break;
			}
			return false; 
		case '*':
		case '@':
			for (i = 0; i <= filename_end - filename; i++) {
				if (tomoyo_file_matches_pattern2(
						    filename + i, filename_end,
						    pattern + 1, pattern_end))
					return true;
				c = filename[i];
				if (c == '.' && *pattern == '@')
					break;
				if (c != '\\')
					continue;
				if (filename[i + 1] == '\\')
					i++;
				else if (tomoyo_byte_range(filename + i + 1))
					i += 3;
				else
					break; 
			}
			return false; 
		default:
			j = 0;
			c = *pattern;
			if (c == '$') {
				while (isdigit(filename[j]))
					j++;
			} else if (c == 'X') {
				while (isxdigit(filename[j]))
					j++;
			} else if (c == 'A') {
				while (tomoyo_alphabet_char(filename[j]))
					j++;
			}
			for (i = 1; i <= j; i++) {
				if (tomoyo_file_matches_pattern2(
						    filename + i, filename_end,
						    pattern + 1, pattern_end))
					return true;
			}
			return false; 
		}
		filename++;
		pattern++;
	}
	while (*pattern == '\\' &&
	       (*(pattern + 1) == '*' || *(pattern + 1) == '@'))
		pattern += 2;
	return filename == filename_end && pattern == pattern_end;
}

static bool tomoyo_file_matches_pattern(const char *filename,
					const char *filename_end,
					const char *pattern,
					const char *pattern_end)
{
	const char *pattern_start = pattern;
	bool first = true;
	bool result;

	while (pattern < pattern_end - 1) {
		
		if (*pattern++ != '\\' || *pattern++ != '-')
			continue;
		result = tomoyo_file_matches_pattern2(filename,
						      filename_end,
						      pattern_start,
						      pattern - 2);
		if (first)
			result = !result;
		if (result)
			return false;
		first = false;
		pattern_start = pattern;
	}
	result = tomoyo_file_matches_pattern2(filename, filename_end,
					      pattern_start, pattern_end);
	return first ? result : !result;
}

static bool tomoyo_path_matches_pattern2(const char *f, const char *p)
{
	const char *f_delimiter;
	const char *p_delimiter;

	while (*f && *p) {
		f_delimiter = strchr(f, '/');
		if (!f_delimiter)
			f_delimiter = f + strlen(f);
		p_delimiter = strchr(p, '/');
		if (!p_delimiter)
			p_delimiter = p + strlen(p);
		if (*p == '\\' && *(p + 1) == '{')
			goto recursive;
		if (!tomoyo_file_matches_pattern(f, f_delimiter, p,
						 p_delimiter))
			return false;
		f = f_delimiter;
		if (*f)
			f++;
		p = p_delimiter;
		if (*p)
			p++;
	}
	
	while (*p == '\\' &&
	       (*(p + 1) == '*' || *(p + 1) == '@'))
		p += 2;
	return !*f && !*p;
 recursive:
	if (*(p - 1) != '/' || p_delimiter <= p + 3 || *p_delimiter != '/' ||
	    *(p_delimiter - 1) != '}' || *(p_delimiter - 2) != '\\')
		return false; 
	do {
		
		if (!tomoyo_file_matches_pattern(f, f_delimiter, p + 2,
						 p_delimiter - 2))
			break;
		
		f = f_delimiter;
		if (!*f)
			break;
		f++;
		
		if (tomoyo_path_matches_pattern2(f, p_delimiter + 1))
			return true;
		f_delimiter = strchr(f, '/');
	} while (f_delimiter);
	return false; 
}

bool tomoyo_path_matches_pattern(const struct tomoyo_path_info *filename,
				 const struct tomoyo_path_info *pattern)
{
	const char *f = filename->name;
	const char *p = pattern->name;
	const int len = pattern->const_len;

	
	if (!pattern->is_patterned)
		return !tomoyo_pathcmp(filename, pattern);
	
	if (filename->is_dir != pattern->is_dir)
		return false;
	
	if (strncmp(f, p, len))
		return false;
	f += len;
	p += len;
	return tomoyo_path_matches_pattern2(f, p);
}

const char *tomoyo_get_exe(void)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	const char *cp = NULL;

	if (!mm)
		return NULL;
	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) && vma->vm_file) {
			cp = tomoyo_realpath_from_path(&vma->vm_file->f_path);
			break;
		}
	}
	up_read(&mm->mmap_sem);
	return cp;
}

int tomoyo_get_mode(const struct tomoyo_policy_namespace *ns, const u8 profile,
		    const u8 index)
{
	u8 mode;
	struct tomoyo_profile *p;

	if (!tomoyo_policy_loaded)
		return TOMOYO_CONFIG_DISABLED;
	p = tomoyo_profile(ns, profile);
	mode = p->config[index];
	if (mode == TOMOYO_CONFIG_USE_DEFAULT)
		mode = p->config[tomoyo_index2category[index]
				 + TOMOYO_MAX_MAC_INDEX];
	if (mode == TOMOYO_CONFIG_USE_DEFAULT)
		mode = p->default_config;
	return mode & 3;
}

int tomoyo_init_request_info(struct tomoyo_request_info *r,
			     struct tomoyo_domain_info *domain, const u8 index)
{
	u8 profile;
	memset(r, 0, sizeof(*r));
	if (!domain)
		domain = tomoyo_domain();
	r->domain = domain;
	profile = domain->profile;
	r->profile = profile;
	r->type = index;
	r->mode = tomoyo_get_mode(domain->ns, profile, index);
	return r->mode;
}

bool tomoyo_domain_quota_is_ok(struct tomoyo_request_info *r)
{
	unsigned int count = 0;
	struct tomoyo_domain_info *domain = r->domain;
	struct tomoyo_acl_info *ptr;

	if (r->mode != TOMOYO_CONFIG_LEARNING)
		return false;
	if (!domain)
		return true;
	list_for_each_entry_rcu(ptr, &domain->acl_info_list, list) {
		u16 perm;
		u8 i;
		if (ptr->is_deleted)
			continue;
		switch (ptr->type) {
		case TOMOYO_TYPE_PATH_ACL:
			perm = container_of(ptr, struct tomoyo_path_acl, head)
				->perm;
			break;
		case TOMOYO_TYPE_PATH2_ACL:
			perm = container_of(ptr, struct tomoyo_path2_acl, head)
				->perm;
			break;
		case TOMOYO_TYPE_PATH_NUMBER_ACL:
			perm = container_of(ptr, struct tomoyo_path_number_acl,
					    head)->perm;
			break;
		case TOMOYO_TYPE_MKDEV_ACL:
			perm = container_of(ptr, struct tomoyo_mkdev_acl,
					    head)->perm;
			break;
		case TOMOYO_TYPE_INET_ACL:
			perm = container_of(ptr, struct tomoyo_inet_acl,
					    head)->perm;
			break;
		case TOMOYO_TYPE_UNIX_ACL:
			perm = container_of(ptr, struct tomoyo_unix_acl,
					    head)->perm;
			break;
		case TOMOYO_TYPE_MANUAL_TASK_ACL:
			perm = 0;
			break;
		default:
			perm = 1;
		}
		for (i = 0; i < 16; i++)
			if (perm & (1 << i))
				count++;
	}
	if (count < tomoyo_profile(domain->ns, domain->profile)->
	    pref[TOMOYO_PREF_MAX_LEARNING_ENTRY])
		return true;
	if (!domain->flags[TOMOYO_DIF_QUOTA_WARNED]) {
		domain->flags[TOMOYO_DIF_QUOTA_WARNED] = true;
		
		tomoyo_write_log(r, "%s", tomoyo_dif[TOMOYO_DIF_QUOTA_WARNED]);
		printk(KERN_WARNING "WARNING: "
		       "Domain '%s' has too many ACLs to hold. "
		       "Stopped learning mode.\n", domain->domainname->name);
	}
	return false;
}
