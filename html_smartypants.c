/*	$Id$ */
/*
 * Copyright (c) 2008, Natacha Porté
 * Copyright (c) 2011, Vicent Martí
 * Copyright (c) 2014, Xavier Mendez, Devin Torres and the Hoedown authors
 * Copyright (c) 2016, Kristaps Dzonsons
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
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lowdown.h"
#include "extern.h"

struct smartypants_data {
	int in_squote;
	int in_dquote;
};

static size_t smartypants_cb__ltag(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__dquote(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__amp(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__period(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__number(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__dash(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__parens(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__squote(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__backtick(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__escape(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);

static size_t (*smartypants_cb_ptrs[])
	(hbuf *, struct smartypants_data *, uint8_t, const uint8_t *, size_t) =
{
	NULL,					/* 0 */
	smartypants_cb__dash,	/* 1 */
	smartypants_cb__parens,	/* 2 */
	smartypants_cb__squote, /* 3 */
	smartypants_cb__dquote, /* 4 */
	smartypants_cb__amp,	/* 5 */
	smartypants_cb__period,	/* 6 */
	smartypants_cb__number,	/* 7 */
	smartypants_cb__ltag,	/* 8 */
	smartypants_cb__backtick, /* 9 */
	smartypants_cb__escape, /* 10 */
};

static const uint8_t smartypants_cb_chars[UINT8_MAX+1] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 4, 0, 0, 0, 5, 3, 2, 0, 0, 0, 0, 1, 6, 0,
	0, 7, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0,
	9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static int
word_boundary(uint8_t c)
{
	return c == 0 || isspace(c) || ispunct(c);
}

/*
	If 'text' begins with any kind of single quote (e.g. "'" or "&apos;" etc.),
	returns the length of the sequence of characters that makes up the single-
	quote.  Otherwise, returns zero.
*/
static size_t
squote_len(const uint8_t *text, size_t size)
{
	static const char* single_quote_list[] = { "'", "&#39;", "&#x27;", "&apos;", NULL };
	const char** p;

	for (p = single_quote_list; *p; ++p) {
		size_t len = strlen(*p);
		if (size >= len && memcmp(text, *p, len) == 0) {
			return len;
		}
	}

	return 0;
}

/* Converts " or ' at very beginning or end of a word to left or right quote */
static int
smartypants_quotes(hbuf *ob, uint8_t previous_char, uint8_t next_char, uint8_t quote, int *is_open)
{

	if (*is_open && !word_boundary(next_char))
		return 0;

	if (!(*is_open) && !word_boundary(previous_char))
		return 0;

	assert('d' == quote || 's' == quote);

	if ('d' == quote)
		hbuf_puts(ob, *is_open ? "&#8221;" : "&#8220;");
	else
		hbuf_puts(ob, *is_open ? "&#8217;" : "&#8216;");

	*is_open = !(*is_open);
	return 1;
}

/*
	Converts ' to left or right single quote; but the initial ' might be in
	different forms, e.g. &apos; or &#39; or &#x27;.
	'squote_text' points to the original single quote, and 'squote_size' is its length.
	'text' points at the last character of the single-quote, e.g. ' or ;
*/
static size_t
smartypants_squote(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size,
				   const uint8_t *squote_text, size_t squote_size)
{
	if (size >= 2) {
		uint8_t t1 = tolower(text[1]);
		size_t next_squote_len = squote_len(text+1, size-1);

		/* convert '' to &ldquo; or &rdquo; */
		if (next_squote_len > 0) {
			uint8_t next_char = (size > 1+next_squote_len) ? text[1+next_squote_len] : 0;
			if (smartypants_quotes(ob, previous_char, next_char, 'd', &smrt->in_dquote))
				return next_squote_len;
		}

		/* Tom's, isn't, I'm, I'd */
		if ((t1 == 's' || t1 == 't' || t1 == 'm' || t1 == 'd') &&
			(size == 3 || word_boundary(text[2]))) {
			HBUF_PUTSL(ob, "&#8217;");
			return 0;
		}

		/* you're, you'll, you've */
		if (size >= 3) {
			uint8_t t2 = tolower(text[2]);

			if (((t1 == 'r' && t2 == 'e') ||
				(t1 == 'l' && t2 == 'l') ||
				(t1 == 'v' && t2 == 'e')) &&
				(size == 4 || word_boundary(text[3]))) {
				HBUF_PUTSL(ob, "&#8217;");
				return 0;
			}
		}
	}

	if (smartypants_quotes(ob, previous_char, size > 0 ? text[1] : 0, 's', &smrt->in_squote))
		return 0;

	hbuf_put(ob, squote_text, squote_size);
	return 0;
}

/* Converts ' to left or right single quote. */
static size_t
smartypants_cb__squote(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	return smartypants_squote(ob, smrt, previous_char, text, size, text, 1);
}

/* Converts (c), (r), (tm) */
static size_t
smartypants_cb__parens(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (size >= 3) {
		uint8_t t1 = tolower(text[1]);
		uint8_t t2 = tolower(text[2]);

		if (t1 == 'c' && t2 == ')') {
			HBUF_PUTSL(ob, "&#169;");
			return 2;
		}

		if (t1 == 'r' && t2 == ')') {
			HBUF_PUTSL(ob, "&#174;");
			return 2;
		}

		if (size >= 4 && t1 == 't' && t2 == 'm' && text[3] == ')') {
			HBUF_PUTSL(ob, "&#8482;");
			return 3;
		}
	}

	hbuf_putc(ob, text[0]);
	return 0;
}

/* Converts "--" to em-dash, etc. */
static size_t
smartypants_cb__dash(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (size >= 3 && text[1] == '-' && text[2] == '-') {
		HBUF_PUTSL(ob, "&#8212;");
		return 2;
	}

	if (size >= 2 && text[1] == '-') {
		HBUF_PUTSL(ob, "&#8211;");
		return 1;
	}

	hbuf_putc(ob, text[0]);
	return 0;
}

/* Converts &quot; etc. */
static size_t
smartypants_cb__amp(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	size_t len;
	if (size >= 6 && memcmp(text, "&quot;", 6) == 0) {
		if (smartypants_quotes(ob, previous_char, size >= 7 ? text[6] : 0, 'd', &smrt->in_dquote))
			return 5;
	}

	len = squote_len(text, size);
	if (len > 0) {
		return (len-1) + smartypants_squote(ob, smrt, previous_char, text+(len-1), size-(len-1), text, len);
	}

	if (size >= 4 && memcmp(text, "&#0;", 4) == 0)
		return 3;

	hbuf_putc(ob, '&');
	return 0;
}

/* Converts "..." to ellipsis */
static size_t
smartypants_cb__period(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (size >= 3 && text[1] == '.' && text[2] == '.') {
		HBUF_PUTSL(ob, "&#8230;");
		return 2;
	}

	if (size >= 5 && text[1] == ' ' && text[2] == '.' && text[3] == ' ' && text[4] == '.') {
		HBUF_PUTSL(ob, "&#8230;");
		return 4;
	}

	hbuf_putc(ob, text[0]);
	return 0;
}

/* Converts `` to opening double quote */
static size_t
smartypants_cb__backtick(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (size >= 2 && text[1] == '`') {
		if (smartypants_quotes(ob, previous_char, size >= 3 ? text[2] : 0, 'd', &smrt->in_dquote))
			return 1;
	}

	hbuf_putc(ob, text[0]);
	return 0;
}

/* Converts 1/2, 1/4, 3/4 */
static size_t
smartypants_cb__number(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (word_boundary(previous_char) && size >= 3) {
		if (text[0] == '1' && text[1] == '/' && text[2] == '2') {
			if (size == 3 || word_boundary(text[3])) {
				HBUF_PUTSL(ob, "&#189;");
				return 2;
			}
		}

		if (text[0] == '1' && text[1] == '/' && text[2] == '4') {
			if (size == 3 || word_boundary(text[3]) ||
				(size >= 5 && tolower(text[3]) == 't' && tolower(text[4]) == 'h')) {
				HBUF_PUTSL(ob, "&#188;");
				return 2;
			}
		}

		if (text[0] == '3' && text[1] == '/' && text[2] == '4') {
			if (size == 3 || word_boundary(text[3]) ||
				(size >= 6 && tolower(text[3]) == 't' && tolower(text[4]) == 'h' && tolower(text[5]) == 's')) {
				HBUF_PUTSL(ob, "&#190;");
				return 2;
			}
		}
	}

	hbuf_putc(ob, text[0]);
	return 0;
}

/* Converts " to left or right double quote */
static size_t
smartypants_cb__dquote(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (!smartypants_quotes(ob, previous_char, size > 0 ? text[1] : 0, 'd', &smrt->in_dquote))
		HBUF_PUTSL(ob, "&#34;");

	return 0;
}

static size_t
smartypants_cb__ltag(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	static const char *skip_tags[] = {
	  "pre", "code", "var", "samp", "kbd", "math", "script", "style"
	};
	static const size_t skip_tags_count = 8;

	size_t tag, i = 0;

	/* This is a comment. Copy everything verbatim until --> or EOF is seen. */
	if (i + 4 < size && memcmp(text + i, "<!--", 4) == 0) {
		i += 4;
		while (i + 3 < size && memcmp(text + i, "-->",  3) != 0)
			i++;
		i += 3;
		hbuf_put(ob, text, i + 1);
		return i;
	}

	while (i < size && text[i] != '>')
		i++;

	for (tag = 0; tag < skip_tags_count; ++tag) {
		if (hhtml_get_tag(text, size, skip_tags[tag]) == HOEDOWN_HTML_TAG_OPEN)
			break;
	}

	if (tag < skip_tags_count) {
		for (;;) {
			while (i < size && text[i] != '<')
				i++;

			if (i == size)
				break;

			if (hhtml_get_tag(text + i, size - i, skip_tags[tag]) == HOEDOWN_HTML_TAG_CLOSE)
				break;

			i++;
		}

		while (i < size && text[i] != '>')
			i++;
	}

	hbuf_put(ob, text, i + 1);
	return i;
}

static size_t
smartypants_cb__escape(hbuf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size)
{
	if (size < 2)
		return 0;

	switch (text[1]) {
	case '\\':
	case '"':
	case '\'':
	case '.':
	case '-':
	case '`':
		hbuf_putc(ob, text[1]);
		return 1;

	default:
		hbuf_putc(ob, '\\');
		return 0;
	}
}

#if 0
static struct {
    uint8_t c0;
    const uint8_t *pattern;
    const uint8_t *entity;
    int skip;
} smartypants_subs[] = {
    { '\'', "'s>",      "&rsquo;",  0 },
    { '\'', "'t>",      "&rsquo;",  0 },
    { '\'', "'re>",     "&rsquo;",  0 },
    { '\'', "'ll>",     "&rsquo;",  0 },
    { '\'', "'ve>",     "&rsquo;",  0 },
    { '\'', "'m>",      "&rsquo;",  0 },
    { '\'', "'d>",      "&rsquo;",  0 },
    { '-',  "--",       "&mdash;",  1 },
    { '-',  "<->",      "&ndash;",  0 },
    { '.',  "...",      "&hellip;", 2 },
    { '.',  ". . .",    "&hellip;", 4 },
    { '(',  "(c)",      "&copy;",   2 },
    { '(',  "(r)",      "&reg;",    2 },
    { '(',  "(tm)",     "&trade;",  3 },
    { '3',  "<3/4>",    "&frac34;", 2 },
    { '3',  "<3/4ths>", "&frac34;", 2 },
    { '1',  "<1/2>",    "&frac12;", 2 },
    { '1',  "<1/4>",    "&frac14;", 2 },
    { '1',  "<1/4th>",  "&frac14;", 2 },
    { '&',  "&#0;",      0,       3 },
};
#endif

/* process an HTML snippet using SmartyPants for smart punctuation */
void
hsmrt_html(hbuf *ob, const uint8_t *text, size_t size)
{
	size_t i;
	struct smartypants_data smrt = {0, 0};

	if (!text)
		return;

	hbuf_grow(ob, size);

	for (i = 0; i < size; ++i) {
		size_t org;
		uint8_t action = 0;

		org = i;
		while (i < size && (action = smartypants_cb_chars[text[i]]) == 0)
			i++;

		if (i > org)
			hbuf_put(ob, text + org, i - org);

		if (i < size) {
			i += smartypants_cb_ptrs[(int)action]
				(ob, &smrt, i ? text[i - 1] : 0, text + i, size - i);
		}
	}
}
