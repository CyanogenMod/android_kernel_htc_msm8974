
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/nls.h>
#include <linux/errno.h>

static struct nls_table *p_nls;

static int uni2char(const wchar_t uni,
		    unsigned char *out, int boundlen)
{
	if (boundlen <= 0)
		return -ENAMETOOLONG;

	if ((uni & 0xffaf) == 0x040e || (uni & 0xffce) == 0x254c) {
		
		if (uni == 0x040e)
			out[0] = 0xbe;
		else if (uni == 0x045e)
			out[0] = 0xae;
		else if (uni == 0x255d || uni == 0x256c)
			return 0;
		else
			return p_nls->uni2char(uni, out, boundlen);
		return 1;
	}
	else
		
		return p_nls->uni2char(uni, out, boundlen);
}

static int char2uni(const unsigned char *rawstring, int boundlen,
		    wchar_t *uni)
{
	int n;

	if ((*rawstring & 0xef) != 0xae) {
		
		*uni = (*rawstring & 0x10) ? 0x040e : 0x045e;
		return 1;
	}

	n = p_nls->char2uni(rawstring, boundlen, uni);
	return n;
}

static struct nls_table table = {
	.charset	= "koi8-ru",
	.uni2char	= uni2char,
	.char2uni	= char2uni,
	.owner		= THIS_MODULE,
};

static int __init init_nls_koi8_ru(void)
{
	p_nls = load_nls("koi8-u");

	if (p_nls) {
		table.charset2upper = p_nls->charset2upper;
		table.charset2lower = p_nls->charset2lower;
		return register_nls(&table);
	}

	return -EINVAL;
}

static void __exit exit_nls_koi8_ru(void)
{
	unregister_nls(&table);
	unload_nls(p_nls);
}

module_init(init_nls_koi8_ru)
module_exit(exit_nls_koi8_ru)

MODULE_LICENSE("Dual BSD/GPL");
