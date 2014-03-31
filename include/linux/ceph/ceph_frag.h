#ifndef FS_CEPH_FRAG_H
#define FS_CEPH_FRAG_H

static inline __u32 ceph_frag_make(__u32 b, __u32 v)
{
	return (b << 24) |
		(v & (0xffffffu << (24-b)) & 0xffffffu);
}
static inline __u32 ceph_frag_bits(__u32 f)
{
	return f >> 24;
}
static inline __u32 ceph_frag_value(__u32 f)
{
	return f & 0xffffffu;
}
static inline __u32 ceph_frag_mask(__u32 f)
{
	return (0xffffffu << (24-ceph_frag_bits(f))) & 0xffffffu;
}
static inline __u32 ceph_frag_mask_shift(__u32 f)
{
	return 24 - ceph_frag_bits(f);
}

static inline int ceph_frag_contains_value(__u32 f, __u32 v)
{
	return (v & ceph_frag_mask(f)) == ceph_frag_value(f);
}
static inline int ceph_frag_contains_frag(__u32 f, __u32 sub)
{
	
	return ceph_frag_bits(sub) >= ceph_frag_bits(f) &&
	       (ceph_frag_value(sub) & ceph_frag_mask(f)) == ceph_frag_value(f);
}

static inline __u32 ceph_frag_parent(__u32 f)
{
	return ceph_frag_make(ceph_frag_bits(f) - 1,
			 ceph_frag_value(f) & (ceph_frag_mask(f) << 1));
}
static inline int ceph_frag_is_left_child(__u32 f)
{
	return ceph_frag_bits(f) > 0 &&
		(ceph_frag_value(f) & (0x1000000 >> ceph_frag_bits(f))) == 0;
}
static inline int ceph_frag_is_right_child(__u32 f)
{
	return ceph_frag_bits(f) > 0 &&
		(ceph_frag_value(f) & (0x1000000 >> ceph_frag_bits(f))) == 1;
}
static inline __u32 ceph_frag_sibling(__u32 f)
{
	return ceph_frag_make(ceph_frag_bits(f),
		      ceph_frag_value(f) ^ (0x1000000 >> ceph_frag_bits(f)));
}
static inline __u32 ceph_frag_left_child(__u32 f)
{
	return ceph_frag_make(ceph_frag_bits(f)+1, ceph_frag_value(f));
}
static inline __u32 ceph_frag_right_child(__u32 f)
{
	return ceph_frag_make(ceph_frag_bits(f)+1,
	      ceph_frag_value(f) | (0x1000000 >> (1+ceph_frag_bits(f))));
}
static inline __u32 ceph_frag_make_child(__u32 f, int by, int i)
{
	int newbits = ceph_frag_bits(f) + by;
	return ceph_frag_make(newbits,
			 ceph_frag_value(f) | (i << (24 - newbits)));
}
static inline int ceph_frag_is_leftmost(__u32 f)
{
	return ceph_frag_value(f) == 0;
}
static inline int ceph_frag_is_rightmost(__u32 f)
{
	return ceph_frag_value(f) == ceph_frag_mask(f);
}
static inline __u32 ceph_frag_next(__u32 f)
{
	return ceph_frag_make(ceph_frag_bits(f),
			 ceph_frag_value(f) + (0x1000000 >> ceph_frag_bits(f)));
}

int ceph_frag_compare(__u32 a, __u32 b);

#endif
