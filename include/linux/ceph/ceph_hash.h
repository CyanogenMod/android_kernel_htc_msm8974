#ifndef FS_CEPH_HASH_H
#define FS_CEPH_HASH_H

#define CEPH_STR_HASH_LINUX      0x1  
#define CEPH_STR_HASH_RJENKINS   0x2  

extern unsigned ceph_str_hash_linux(const char *s, unsigned len);
extern unsigned ceph_str_hash_rjenkins(const char *s, unsigned len);

extern unsigned ceph_str_hash(int type, const char *s, unsigned len);
extern const char *ceph_str_hash_name(int type);

#endif
