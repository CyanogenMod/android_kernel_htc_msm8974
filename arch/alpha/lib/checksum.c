 
#include <linux/module.h>
#include <linux/string.h>

#include <asm/byteorder.h>

static inline unsigned short from64to16(unsigned long x)
{

	union {
		unsigned long	ul;
		unsigned int	ui[2];
		unsigned short	us[4];
	} in_v, tmp_v, out_v;

	in_v.ul = x;
	tmp_v.ul = (unsigned long) in_v.ui[0] + (unsigned long) in_v.ui[1];

	out_v.ul = (unsigned long) tmp_v.us[0] + (unsigned long) tmp_v.us[1]
			+ (unsigned long) tmp_v.us[2];

	
	return out_v.us[0] + out_v.us[1];
}

__sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
				   unsigned short len,
				   unsigned short proto,
				   __wsum sum)
{
	return (__force __sum16)~from64to16(
		(__force u64)saddr + (__force u64)daddr +
		(__force u64)sum + ((len + proto) << 8));
}

__wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
				   unsigned short len,
				   unsigned short proto,
				   __wsum sum)
{
	unsigned long result;

	result = (__force u64)saddr + (__force u64)daddr +
		 (__force u64)sum + ((len + proto) << 8);

	
	result = (result & 0xffffffff) + (result >> 32);
	
	result = (result & 0xffffffff) + (result >> 32);
	return (__force __wsum)result;
}
EXPORT_SYMBOL(csum_tcpudp_nofold);

static inline unsigned long do_csum(const unsigned char * buff, int len)
{
	int odd, count;
	unsigned long result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
		result = *buff << 8;
		len--;
		buff++;
	}
	count = len >> 1;		
	if (count) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			count--;
			len -= 2;
			buff += 2;
		}
		count >>= 1;		
		if (count) {
			if (4 & (unsigned long) buff) {
				result += *(unsigned int *) buff;
				count--;
				len -= 4;
				buff += 4;
			}
			count >>= 1;	
			if (count) {
				unsigned long carry = 0;
				do {
					unsigned long w = *(unsigned long *) buff;
					count--;
					buff += 8;
					result += carry;
					result += w;
					carry = (w > result);
				} while (count);
				result += carry;
				result = (result & 0xffffffff) + (result >> 32);
			}
			if (len & 4) {
				result += *(unsigned int *) buff;
				buff += 4;
			}
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
		result += *buff;
	result = from64to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

__sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	return (__force __sum16)~do_csum(iph,ihl*4);
}

__wsum csum_partial(const void *buff, int len, __wsum sum)
{
	unsigned long result = do_csum(buff, len);

	
	result += (__force u32)sum;
	
	result = (result & 0xffffffff) + (result >> 32);
	return (__force __wsum)result;
}

EXPORT_SYMBOL(csum_partial);

__sum16 ip_compute_csum(const void *buff, int len)
{
	return (__force __sum16)~from64to16(do_csum(buff,len));
}
