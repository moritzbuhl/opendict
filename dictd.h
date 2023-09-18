/*
 * Copyright (c) 2023 Moritz Buhl <mbuhl@openbsd.org>
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

#define MAXIMUM(a,b)	(((a)>(b))?(a):(b))
#define MINIMUM(a,b)	(((a)<(b))?(a):(b))

SLIST_HEAD(dc_index_list, dc_index_entry);
struct dc_index_entry {
	const char 			*match; // XXX: do we really need this?
	size_t				 match_len; // XXX: same
	size_t				 def_off;
	size_t				 def_len;
	SLIST_ENTRY(dc_index_entry)	 entries;
};

struct dc_index {
	const char 	*data;
	off_t		 size;
};

struct dc_database {
	char				*name;
	struct dc_index			*index;
	void				*data;
	TAILQ_ENTRY(dc_database)	 entry;
};
