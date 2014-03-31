#include "util.h"
#include "string.h"

#define K 1024LL
s64 perf_atoll(const char *str)
{
	unsigned int i;
	s64 length = -1, unit = 1;

	if (!isdigit(str[0]))
		goto out_err;

	for (i = 1; i < strlen(str); i++) {
		switch (str[i]) {
		case 'B':
		case 'b':
			break;
		case 'K':
			if (str[i + 1] != 'B')
				goto out_err;
			else
				goto kilo;
		case 'k':
			if (str[i + 1] != 'b')
				goto out_err;
kilo:
			unit = K;
			break;
		case 'M':
			if (str[i + 1] != 'B')
				goto out_err;
			else
				goto mega;
		case 'm':
			if (str[i + 1] != 'b')
				goto out_err;
mega:
			unit = K * K;
			break;
		case 'G':
			if (str[i + 1] != 'B')
				goto out_err;
			else
				goto giga;
		case 'g':
			if (str[i + 1] != 'b')
				goto out_err;
giga:
			unit = K * K * K;
			break;
		case 'T':
			if (str[i + 1] != 'B')
				goto out_err;
			else
				goto tera;
		case 't':
			if (str[i + 1] != 'b')
				goto out_err;
tera:
			unit = K * K * K * K;
			break;
		case '\0':	
			unit = 1;
			break;
		default:
			if (!isdigit(str[i]))
				goto out_err;
			break;
		}
	}

	length = atoll(str) * unit;
	goto out;

out_err:
	length = -1;
out:
	return length;
}

static const char *skip_sep(const char *cp)
{
	while (*cp && isspace(*cp))
		cp++;

	return cp;
}

static const char *skip_arg(const char *cp)
{
	while (*cp && !isspace(*cp))
		cp++;

	return cp;
}

static int count_argc(const char *str)
{
	int count = 0;

	while (*str) {
		str = skip_sep(str);
		if (*str) {
			count++;
			str = skip_arg(str);
		}
	}

	return count;
}

void argv_free(char **argv)
{
	char **p;
	for (p = argv; *p; p++)
		free(*p);

	free(argv);
}

char **argv_split(const char *str, int *argcp)
{
	int argc = count_argc(str);
	char **argv = zalloc(sizeof(*argv) * (argc+1));
	char **argvp;

	if (argv == NULL)
		goto out;

	if (argcp)
		*argcp = argc;

	argvp = argv;

	while (*str) {
		str = skip_sep(str);

		if (*str) {
			const char *p = str;
			char *t;

			str = skip_arg(str);

			t = strndup(p, str-p);
			if (t == NULL)
				goto fail;
			*argvp++ = t;
		}
	}
	*argvp = NULL;

out:
	return argv;

fail:
	argv_free(argv);
	return NULL;
}

static bool __match_charclass(const char *pat, char c, const char **npat)
{
	bool complement = false, ret = true;

	if (*pat == '!') {
		complement = true;
		pat++;
	}
	if (*pat++ == c)	
		goto end;

	while (*pat && *pat != ']') {	
		if (*pat == '-' && *(pat + 1) != ']') {	
			if (*(pat - 1) <= c && c <= *(pat + 1))
				goto end;
			if (*(pat - 1) > *(pat + 1))
				goto error;
			pat += 2;
		} else if (*pat++ == c)
			goto end;
	}
	if (!*pat)
		goto error;
	ret = false;

end:
	while (*pat && *pat != ']')	
		pat++;
	if (!*pat)
		goto error;
	*npat = pat + 1;
	return complement ? !ret : ret;

error:
	return false;
}

static bool __match_glob(const char *str, const char *pat, bool ignore_space)
{
	while (*str && *pat && *pat != '*') {
		if (ignore_space) {
			
			if (isspace(*str)) {
				str++;
				continue;
			}
			if (isspace(*pat)) {
				pat++;
				continue;
			}
		}
		if (*pat == '?') {	
			str++;
			pat++;
			continue;
		} else if (*pat == '[')	
			if (__match_charclass(pat + 1, *str, &pat)) {
				str++;
				continue;
			} else
				return false;
		else if (*pat == '\\') 
			pat++;
		if (*str++ != *pat++)
			return false;
	}
	
	if (*pat == '*') {
		while (*pat == '*')
			pat++;
		if (!*pat)	
			return true;
		while (*str)
			if (__match_glob(str++, pat, ignore_space))
				return true;
	}
	return !*str && !*pat;
}

bool strglobmatch(const char *str, const char *pat)
{
	return __match_glob(str, pat, false);
}

bool strlazymatch(const char *str, const char *pat)
{
	return __match_glob(str, pat, true);
}

int strtailcmp(const char *s1, const char *s2)
{
	int i1 = strlen(s1);
	int i2 = strlen(s2);
	while (--i1 >= 0 && --i2 >= 0) {
		if (s1[i1] != s2[i2])
			return s1[i1] - s2[i2];
	}
	return 0;
}

