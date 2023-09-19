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

#include <sys/mman.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>

#include "dictd.h"
#include "database.h"
#include "index.h"

int
main(int argc, char *argv[])
{
	struct dc_database mydb;
	struct dc_index_list list;
	struct dc_index myidx;
	struct dc_index_entry myr[100], *my;
	char ans[256] = {0};
	int i, r;

	if (argc != 2)
		return 1;

	SLIST_INIT(&list);
	memset(myr, 0, sizeof(struct dc_index_entry) * 100);
	for (i = 0; i < 100; i++)
		SLIST_INSERT_HEAD(&list, &myr[i], entries);

	r = database_open("/home/mbuhl/Downloads/eng-deu/eng-deu.dict.dz", &mydb);
	if (r != 0)
		return 2;
	r = index_open("/home/mbuhl/Downloads/eng-deu/eng-deu.index", &myidx);
	if (r != 0)
		return 3;

	r = index_prefix_find(argv[1], &myidx, &list);
	if (r < 0)
		return 4;

	SLIST_FOREACH(my, &list, entries) {
		if (my->match == NULL)
			break;
		strncpy(ans, my->match, my->match_len);
		ans[my->match_len] = '\0';
		database_lookup(my, &mydb, ans);
	}

}
