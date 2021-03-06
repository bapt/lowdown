/*
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
 * Copyright (c) 2016--2017 Kristaps Dzonsons <kristaps@bsd.l>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lowdown.h"
#include "extern.h"

void *
xmalloc(size_t siz)
{
	void	*p;

	if (siz == 0)
		errx(EXIT_FAILURE, "xmalloc: zero size");
	if ((p = malloc(siz)) == NULL)
		err(EXIT_FAILURE, NULL);

	return (p);
}

void *
xcalloc(size_t no, size_t siz)
{
	void	*p;

	if (siz == 0 || no == 0)
		errx(EXIT_FAILURE, "xcalloc: zero size");
	if ((p = calloc(no, siz)) == NULL)
		err(EXIT_FAILURE, NULL);

	return (p);
}

void *
xrealloc(void *p, size_t sz)
{

	if ((p = realloc(p, sz)) == NULL)
		err(EXIT_FAILURE, NULL);

	return (p);
}

void *
xreallocarray(void *p, size_t nm, size_t sz)
{

#if defined(__OpenBSD__) || defined(__FreeBSD__)
	if ((p = reallocarray(p, nm, sz)) == NULL)
		err(EXIT_FAILURE, NULL);
#else
	if ((p = realloc(p, nm * sz)) == NULL)
		err(EXIT_FAILURE, NULL);
#endif

	return (p);
}

char *
xstrndup(const char *p, size_t sz)
{
	char	*pp;

	if ((pp = strndup(p, sz)) == NULL)
		err(EXIT_FAILURE, NULL);

	return(pp);
}
