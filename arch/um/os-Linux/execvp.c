/* Copyright (C) 2006 by Paolo Giarrusso - modified from glibc' execvp.c.
   Original copyright notice follows:

   Copyright (C) 1991,92,1995-99,2002,2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */
#include <unistd.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifndef TEST
#include "um_malloc.h"
#else
#include <stdio.h>
#define um_kmalloc malloc
#endif
#include "os.h"

int execvp_noalloc(char *buf, const char *file, char *const argv[])
{
	if (*file == '\0') {
		return -ENOENT;
	}

	if (strchr (file, '/') != NULL) {
		
		execv(file, argv);
	} else {
		int got_eacces;
		size_t len, pathlen;
		char *name, *p;
		char *path = getenv("PATH");
		if (path == NULL)
			path = ":/bin:/usr/bin";

		len = strlen(file) + 1;
		pathlen = strlen(path);
		
		name = memcpy(buf + pathlen + 1, file, len);
		
		*--name = '/';

		got_eacces = 0;
		p = path;
		do {
			char *startp;

			path = p;
			
			
			p = strchr(path, ':');
			if (!p)
				p = strchr(path, '\0');

			if (p == path)
				startp = name + 1;
			else
				startp = memcpy(name - (p - path), path, p - path);

			
			execv(startp, argv);


			switch (errno) {
				case EACCES:
					got_eacces = 1;
				case ENOENT:
				case ESTALE:
				case ENOTDIR:
				case ENODEV:
				case ETIMEDOUT:
				case ENOEXEC:
					break;

				default:
					return -errno;
			}
		} while (*p++ != '\0');

		
		if (got_eacces)
			return -EACCES;
	}

	
	return -errno;
}
#ifdef TEST
int main(int argc, char**argv)
{
	char buf[PATH_MAX];
	int ret;
	argc--;
	if (!argc) {
		fprintf(stderr, "Not enough arguments\n");
		return 1;
	}
	argv++;
	if (ret = execvp_noalloc(buf, argv[0], argv)) {
		errno = -ret;
		perror("execvp_noalloc");
	}
	return 0;
}
#endif
