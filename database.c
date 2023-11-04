/*	$OpenBSD: gzopen.c,v 1.35 2022/06/18 03:23:19 gkoehler Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
 * Copyright (c) 2023 Moritz Buhl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/* this is partially derived from the zlib's gzio.c file, so the notice: */
/*
  zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.0.4, Jul 24th, 1996.

  Copyright (C) 1995-1996 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  gzip@prep.ai.mit.edu    madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "dict.h"
#include "database.h"

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

typedef
struct gz_stream {
	int		 z_eof;		/* set if end of input file */
	z_stream	 z_stream;	/* libz stream */
	u_char		*z_buf;		/* i/o buffer */
	size_t		 z_buflen;
	u_int32_t	 z_hlen;	/* length of the gz header */
	u_int16_t	 ra_clen;
	u_int16_t	 ra_ccount;
	u_int16_t	*ra_chunks;
	u_int64_t	*ra_offset;
	u_int8_t	*o_buf;		/* to keep a single ra_clen buffer */
} gz_stream;

static const u_char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

static u_int16_t get_int16(gz_stream *);
static int get_header(gz_stream *);
static int get_byte(gz_stream *);
static gz_stream *gz_ropen(int);
static int gz_read(void *, size_t, char *, size_t);
static int gz_close(void *);

int
database_open(int fd, struct dc_database *db)
{
	gz_stream *s;
	if ((s = gz_ropen(fd)) == NULL)
		return -1;

	db->data = s;
	db->size = s->ra_clen * s->ra_ccount;

	return 0;
}

int
database_lookup(struct dc_index_entry *req, struct dc_database *db, char *out)
{
	if (gz_read(db->data, req->def_off, out, req->def_len) == -1)
		return -1;

	return req->def_len;
}

static gz_stream *
gz_ropen(int fd)
{
	struct stat sb;
	gz_stream *s;

	if ((s = calloc(1, sizeof(gz_stream))) == NULL)
		return NULL;

	if (inflateInit2(&(s->z_stream), -MAX_WBITS) != Z_OK)
		goto fail;

	if (fstat(fd, &sb) == -1)
		goto fail;
	s->z_buflen = sb.st_size;

	s->z_buf = mmap(NULL, s->z_buflen, PROT_READ, MAP_PRIVATE, fd, 0);
	if (s->z_buf == MAP_FAILED)
		goto fail;

	s->z_stream.avail_in = s->z_buflen;
	s->z_stream.next_in = s->z_buf;

	/* read the .gz header */
	if (get_header(s) != 0 || (s->o_buf = malloc(65535)) == NULL) {
		gz_close(s);
		return NULL;
	}

	return s;

 fail:
	free(s);
	return NULL;
}

static int
get_byte(gz_stream *s)
{
	if (s->z_stream.avail_in == 0) {
		s->z_eof = 1;
		return EOF;
	}
	s->z_stream.avail_in--;
	return *s->z_stream.next_in++;
}

static u_int16_t
get_int16(gz_stream *s)
{
	u_int16_t x;

	x = ((u_int16_t)(get_byte(s) & 0xff));
	x |= ((u_int16_t)(get_byte(s) & 0xff)) << 8;
	return x;
}

static int
get_header_extra_RA(gz_stream *s, int slen)
{
	u_int64_t offset = 0;
	int ver, clen, ccount, chunk, i;

	if (slen < 6)
		return -1;

	ver = get_int16(s);
	clen = get_int16(s);
	ccount = get_int16(s);
	slen -= 6;

	if (ver != 1 || clen < 0 || ccount < 0 || 2 * ccount != slen)
		return -1;

	s->ra_clen = clen;
	s->ra_ccount = ccount;
	if ((s->ra_chunks = malloc(sizeof(u_int16_t) * ccount)) == NULL)
		return -1;
	if ((s->ra_offset = malloc(sizeof(u_int64_t) * ccount)) == NULL)
		return -1;

	for (i = 0; i < ccount; i++) {
		chunk = get_int16(s);
		if (chunk < 0)
			return -1;
		s->ra_chunks[i] = chunk;
		s->ra_offset[i] = offset;
		offset += chunk;
	}

	return 0;
}

static int
get_header_extra(gz_stream *s, int elen)
{
	int slen;
	int SI1, SI2;

	while (elen > 4) {
		SI1 = get_byte(s);
		SI2 = get_byte(s);
		slen = get_int16(s);
		s->z_hlen += 4;
		elen -= 4;

		if (slen > elen)
			return -1;

		if (SI1 == 'R' && SI2 == 'A') {
			if (get_header_extra_RA(s, slen) != 0)
				return -1;
		} else {
			while (slen-- != 0)
				if (get_byte(s) == EOF)
					return -1;
		}

		s->z_hlen += slen;
		elen -= slen;
	}

	if (elen != 0)
		return -1;
	return 0;
}

static int
get_header(gz_stream *s)
{
	int method; /* method byte */
	int flags;  /* flags byte */
	uInt len;
	int c;

	/* Check the gzip magic header */
	for (len = 0; len < 2; len++) {
		c = get_byte(s);
		if (c != gz_magic[len]) {
			errno = EFTYPE;
			return -1;
		}
	}

	method = get_byte(s);
	flags = get_byte(s);
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		errno = EFTYPE;
		return -1;
	}

	/* Stash timestamp (mtime) */
	(void)get_int16(s);
	(void)get_int16(s);

	/* Discard xflags and OS code */
	(void)get_int16(s);

	s->z_hlen += 10; /* magic, method, flags, time, xflags, OS code */
	if ((flags & EXTRA_FIELD) != 0) {
		len = get_int16(s);
		s->z_hlen += 2;
		if (get_header_extra(s, len) != 0)
			return -1;
	}

	if ((flags & ORIG_NAME) != 0) { /* read/save the original file name */
		while ((c = get_byte(s)) != EOF) {
			s->z_hlen++;
			if (c == '\0')
				break;
		}
	}

	if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while ((c = get_byte(s)) != EOF) {
			s->z_hlen++;
			if (c == '\0')
				break;
		}
	}

	if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		(void)get_int16(s);
		s->z_hlen += 2;
	}

	if (s->z_eof) {
		errno = EFTYPE;
		return -1;
	}

	return 0;
}

static int
gz_read(void *cookie, size_t off, char *out, size_t len)
{
	gz_stream *s = (gz_stream *)cookie;
	size_t chunk, z_off, cpylen;
	int error = Z_OK;

	chunk = off / s->ra_clen;
	off = off % s->ra_clen;

 again:
	if (chunk >= s->ra_ccount)
		return -1;
	z_off = s->z_hlen + s->ra_offset[chunk];
	if (s->z_buflen < z_off + s->ra_chunks[chunk])
		return -1;

	s->z_stream.next_in = s->z_buf + z_off;
	s->z_stream.avail_in = s->ra_chunks[chunk];
	s->z_stream.next_out = s->o_buf;
	s->z_stream.avail_out = s->ra_clen;

	while (error == Z_OK && !s->z_eof && s->z_stream.avail_out != 0) {
		if (s->z_stream.avail_in == 0)
			break;

		error = inflate(&(s->z_stream), Z_PARTIAL_FLUSH);

		if (error == Z_DATA_ERROR) {
			errno = EINVAL;
			return -1;
		} else if (error == Z_BUF_ERROR) {
			errno = EIO;
			return -1;
		} else if (error == Z_STREAM_END) {
			s->z_eof = 1;
			inflateReset(&(s->z_stream)); // XXX
			break;
		}
	}

	cpylen = MIN(len, s->ra_clen - off);
	memcpy(out, s->o_buf + off, cpylen);
	len -= cpylen;
	out += cpylen;
	chunk++;
	off = 0;
	if (len > 0)
		goto again;

	return 0;
}

static int
gz_close(void *cookie)
{
	gz_stream *s = (gz_stream *)cookie;
	int err = 0;

	if (s == NULL)
		return -1;

	if (!err && s->z_stream.state != NULL) {
		err = inflateEnd(&s->z_stream);
	}

	if (!err)
		err = munmap(s->z_buf, s->z_buflen);
	else
		(void)munmap(s->z_buf, s->z_buflen);

	free(s->ra_chunks);
	free(s->ra_offset);
	free(s->o_buf);
	free(s);

	return err;
}
