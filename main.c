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
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dict.h"
#include "database.h"
#include "index.h"

#define MAX_RESULTS	1000

static __dead void
usage(void)
{
	fputs("usage: dict -D dictionary [-Vdem] word [...]\n", stderr);
	exit(1);
}

static void
match(struct dc_index_list *l)
{
	struct dc_index_entry *e;
	const char *prev_match;
	int prev_len = 0;

	SLIST_FOREACH(e, l, entries) {
		if (e->match == NULL)
			break;

		if (prev_len > 0 && prev_len == e->match_len
		    && strncmp(prev_match, e->match, prev_len) == 0) {
			e->match = NULL;
			continue;
		}
		prev_len = e->match_len;
		prev_match = e->match;

		printf("- %.*s\n", e->match_len, e->match);

		e->match = NULL;
	}
}

static void
define(struct dc_database *db, struct dc_index_list *l)
{
	char buf[LOOKUP_MAX];
	struct dc_index_entry *e;
	int r;

	SLIST_FOREACH(e, l, entries) {
		if (e->match == NULL)
			break;
		if (e->def_len > LOOKUP_MAX)
			errx(1, "definition is too large.");
		if ((r = database_lookup(e, db, buf)) == -1) {
			errx(1, "dictionary lookup failed for: %.*s\n",
			    e->match_len, e->match);
		} else {
			printf("- %.*s", r, buf);
		}

		e->match = NULL;
	}
}

int
main(int argc, char *argv[])
{
	struct dc_database db;
	struct dc_index_list list;
	struct dc_index_entry *res;
	char *db_path = NULL, *idx_path = NULL;
	char *lookup;
	int ch, i;
	int Vflag = 0, dflag = 0, eflag = 0, mflag = 0;

	while ((ch = getopt(argc, argv, "D:Vdem")) != -1) {
		switch (ch) {
		case 'D':
			asprintf(&db_path, "/usr/local/freedict/%s/%s.dict.dz",
			    optarg, optarg);
			asprintf(&idx_path, "/usr/local/freedict/%s/%s.index",
			    optarg, optarg);
			break;
		case 'V':
			Vflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0 || db_path == NULL || idx_path == NULL)
		usage();

	if (!dflag)
		mflag = 1;

	if (unveil("/usr/local/freedict", "r") == -1)
		err(1, "unveil");
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	SLIST_INIT(&list);
	if ((res = calloc(MAX_RESULTS, sizeof(struct dc_index_entry))) == NULL)
		err(1, "calloc");
	for (i = 0; i < MAX_RESULTS; i++)
		SLIST_INSERT_HEAD(&list, &res[i], entries);

	if (database_open(db_path, &db) == -1)
		errx(1, "cannot open dictionary '%s'", db_path);

	if (index_open(idx_path, &db.index) == -1)
		err(1, "cannot open index '%s'", idx_path);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (!Vflag && index_validate(&db.index, db.size) == -1)
		errx(1, "index '%s' failed validation", idx_path);

	for (i = 0; i < argc; i++) {
		if ((lookup = strdup(argv[i])) == NULL)
			err(1, "strdup");
		for (ch = 0; lookup[ch] != '\0'; ch++)
			lookup[ch] = tolower(lookup[ch]);

		if (eflag) {
			index_exact_find(lookup, &db.index, &list);
		} else {
			index_prefix_find(lookup, &db.index, &list);
		}

		free(lookup);

		if (mflag)
			match(&list);
		if (dflag)
			define(&db, &list);
	}

	return EXIT_SUCCESS;
}
