/*
 * Copyright (c) 2026 Florian Leander Singer <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Florian Leander Singer <sp1rit@disroot.org>
 */
/*	$OpenBSD: diffreg.c,v 1.95 2021/10/24 21:24:16 deraadt Exp $	*/
/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)diffreg.c   8.1 (Berkeley) 6/6/93
 */

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <glib/gstdio.h>

#include "testsuite/diff/diff.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))
#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

/*
 * diff - compare two files.
 */

/*
 *	Uses an algorithm due to Harold Stone, which finds
 *	a pair of longest identical subsequences in the two
 *	files.
 *
 *	The major goal is to generate the match vector J.
 *	J[i] is the index of the line in file1 corresponding
 *	to line i file0. J[i] = 0 if there is no
 *	such line in file1.
 *
 *	Lines are hashed so as to work in core. All potential
 *	matches are located by sorting the lines of each file
 *	on the hash (called ``value''). In particular, this
 *	collects the equivalence classes in file1 together.
 *	Subroutine equiv replaces the value of each line in
 *	file0 by the index of the first element of its
 *	matching equivalence in (the reordered) file1.
 *	To save space equiv squeezes file1 into a single
 *	array member in which the equivalence classes
 *	are simply concatenated, except that their first
 *	members are flagged by changing sign.
 *
 *	Next the indices that point into member are unsorted into
 *	array class according to the original order of file0.
 *
 *	The cleverness lies in routine stone. This marches
 *	through the lines of file0, developing a vector klist
 *	of "k-candidates". At step i a k-candidate is a matched
 *	pair of lines x,y (x in file0 y in file1) such that
 *	there is a common subsequence of length k
 *	between the first i lines of file0 and the first y
 *	lines of file1, but there is no such subsequence for
 *	any smaller y. x is the earliest possible mate to y
 *	that occurs in such a subsequence.
 *
 *	Whenever any of the members of the equivalence class of
 *	lines in file1 matable to a line in file0 has serial number
 *	less than the y of some k-candidate, that k-candidate
 *	with the smallest such y is replaced. The new
 *	k-candidate is chained (via pred) to the current
 *	k-1 candidate so that the actual subsequence can
 *	be recovered. When a member has serial number greater
 *	that the y of all k-candidates, the klist is extended.
 *	At the end, the longest subsequence is pulled out
 *	and placed in the array J by unravel
 *
 *	With J in hand, the matches there recorded are
 *	check'ed against reality to assure that no spurious
 *	matches have crept in due to hashing. If they have,
 *	they are broken, and "jackpot" is recorded--a harmless
 *	matter except that a true match for a spuriously
 *	mated line may now be unnecessarily reported as a change.
 *
 *	Much of the complexity of the program comes simply
 *	from trying to minimize core utilization and
 *	maximize the range of doable problems by dynamically
 *	allocating what is needed and reusing what is not.
 *	The core requirements for problems larger than somewhat
 *	are (in words) 2*length(file0) + length(file1) +
 *	3*(number of k-candidates installed),  typically about
 *	6n words for files of length n.
 */

struct cand {
	int	x;
	int	y;
	int	pred;
};

struct line {
	int	serial;
	int	value;
};

/*
 * The following struct is used to record change information when
 * doing a "context" or "unified" diff.  (see routine "change" to
 * understand the highly mnemonic field names)
 */
struct context_vec {
	int	a;		/* start line in old file */
	int	b;		/* end line in old file */
	int	c;		/* start line in new file */
	int	d;		/* end line in new file */
};

#define FUNCTION_CONTEXT_SIZE	55


#define diff_context 3
#define Tflag FALSE

typedef struct
{
  const char *filename;
  GOutputStream *out;

  struct line *file[2];

  int  *J;			/* will be overlaid on class */
  int  *class;		/* will be overlaid on file[0] */
  int  *klist;		/* will be overlaid on file[0] after class */
  int  *member;		/* will be overlaid on file[1] */
  int   clen;
  int   len[2];
  int   pref, suff;	/* length of prefix and suffix */
  int   slen[2];
  int   anychange;
  long *ixnew;		/* will be overlaid on file[1] */
  long *ixold;		/* will be overlaid on klist */
  struct cand *clist;	/* merely a free storage pot for candidates */
  int   clistlen;		/* the length of clist */
  struct line *sfile[2];	/* shortened by pruning common prefix/suffix */
  struct context_vec *context_vec_start;
  struct context_vec *context_vec_end;
  struct context_vec *context_vec_ptr;

  char lastbuf[FUNCTION_CONTEXT_SIZE];
  int lastline;
  int lastmatchline;
} DiffOperation;

static void	 diff_output(DiffOperation *, const char *, ...) G_GNUC_PRINTF (2, 3);
static void	 output(DiffOperation *, GInputStream *, GInputStream *, int);
static void	 check(DiffOperation *, GInputStream *, GInputStream *, int);
static void	 uni_range(DiffOperation *, int, int);
static void	 dump_unified_vec(DiffOperation *op, GInputStream *, GInputStream *, int);
static void      stream_rewind (GInputStream *);
static void	 prepare(DiffOperation *, int, GInputStream *, int);
static void	 prune(DiffOperation *);
static void	 equiv(struct line *, int, struct line *, int, int *);
static void	 unravel(DiffOperation *, int);
static void	 unsort(struct line *, int, int *);
static void	 change(DiffOperation *op, GInputStream *, GInputStream *, int, int, int, int, int *);
static void	 sort(struct line *, int);
static void	 print_header(DiffOperation *op);
static int	 fetch(DiffOperation *op, long *, int, int, GInputStream *, int, int, int);
static int	 newcand(DiffOperation *, int, int, int);
static int	 search(DiffOperation *, int *, int, int);
static int	 skipline(GInputStream *);
static int	 isqrt(int);
static int	 stone(DiffOperation *, int *, int, int *, int *, int);
static int	 readhash(GInputStream *, int);
static int	 files_differ(GInputStream *, GInputStream *);
static char	*match_function(DiffOperation *op, const long *, int, GInputStream *);

static GInputStream *
make_seekable_stream (GInputStream *input)
{
  GOutputStream *buffer;
  GBytes *bytes;
  GInputStream *ret;

  if (G_IS_SEEKABLE (input) && g_seekable_can_seek (G_SEEKABLE (input)))
    return g_object_ref (input);

  buffer = g_memory_output_stream_new_resizable ();
  if (g_output_stream_splice (buffer, input, G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, NULL, NULL) < 0)
    g_error ("Failed to copy non-seekable stream");
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (buffer));
  ret = g_memory_input_stream_new_from_bytes (bytes);
  g_object_unref (buffer);
  g_bytes_unref (bytes);
  return ret;
}

int
diffreg(const char *filename, GInputStream *file1, GInputStream *file2, GOutputStream *out, int flags)
{
	DiffOperation op = { 0 };
	int i, rval;
	GInputStream *f1, *f2;

	rval = D_SAME;

	op.filename = filename;
	op.out = out;

	op.anychange = 0;
	op.lastline = 0;
	op.lastmatchline = 0;

	op.context_vec_start = NULL;
	op.context_vec_end = NULL;
	op.context_vec_ptr = op.context_vec_start - 1;

	f1 = make_seekable_stream (file1);
	f2 = make_seekable_stream (file2);

	switch (files_differ(f1, f2)) {
	case 0:
		goto closem;
	case 1:
		break;
	default:
		g_error ("Failed comparing files");
	}

	prepare(&op, 0, f1, flags);
	prepare(&op, 1, f2, flags);

	prune(&op);
	sort(op.sfile[0], op.slen[0]);
	sort(op.sfile[1], op.slen[1]);

	op.member = (int *)op.file[1];
	equiv(op.sfile[0], op.slen[0], op.sfile[1], op.slen[1], op.member);
	op.member = g_realloc_n(op.member, op.slen[1] + 2, sizeof(*op.member));

	op.class = (int *)op.file[0];
	unsort(op.sfile[0], op.slen[0], op.class);
	op.class = g_realloc_n(op.class, op.slen[0] + 2, sizeof(*op.class));

	op.klist = g_malloc0_n(op.slen[0] + 2, sizeof(*op.klist));
	op.clen = 0;
	op.clistlen = 100;
	op.clist = g_malloc0_n(op.clistlen, sizeof(*op.clist));
	i = stone(&op, op.class, op.slen[0], op.member, op.klist, flags);
	g_free(op.member);
	g_free(op.class);

	op.J = g_realloc_n(op.J, op.len[0] + 2, sizeof(*op.J));
	unravel(&op, op.klist[i]);
	g_free(op.clist);
	g_free(op.klist);

	op.ixold = g_realloc_n(op.ixold, op.len[0] + 2, sizeof(*op.ixold));
	op.ixnew = g_realloc_n(op.ixnew, op.len[1] + 2, sizeof(*op.ixnew));
	check(&op, f1, f2, flags);
	output(&op, f1, f2, flags);
closem:
	if (op.anychange && rval == D_SAME)
		rval = D_DIFFER;

	g_object_unref (f1);
	g_object_unref (f2);

	return (rval);
}

/*
 * Check to see if the given files differ.
 * Returns 0 if they are the same, 1 if different, and -1 on error.
 * XXX - could use code from cmp(1) [faster]
 */
static int
files_differ(GInputStream *f1, GInputStream *f2)
{
	char buf1[BUFSIZ], buf2[BUFSIZ];
	gssize i, j;

	for (;;) {
		i = g_input_stream_read(f1, buf1, sizeof(buf1), NULL, NULL);
		j = g_input_stream_read(f2, buf2, sizeof(buf2), NULL, NULL);
		if (i == -1 || j == -1)
			return (-1);
		if (i != j)
			return (1);
		if (i == 0)
			return (0);
		if (memcmp(buf1, buf2, i) != 0)
			return (1);
	}
}

static void
stream_rewind (GInputStream *fd)
{
  if (!g_seekable_seek (G_SEEKABLE (fd), 0, G_SEEK_SET, NULL, NULL))
    g_error ("Failed to rewind stream");
}

static void
prepare(DiffOperation *op, int i, GInputStream *fd, int flags)
{
	struct line *p;
	int j, h;
	size_t filesize,sz;

	if (!g_seekable_seek (G_SEEKABLE (fd), 0, G_SEEK_END, NULL, NULL))
		g_error ("Failed to rewind stream");
	filesize = g_seekable_tell (G_SEEKABLE (fd));
	stream_rewind (fd);

	sz = (filesize <= SIZE_MAX ? filesize : SIZE_MAX) / 25;
	if (sz < 100)
		sz = 100;

	p = g_malloc0_n(sz + 3, sizeof(*p));
	for (j = 0; (h = readhash(fd, flags));) {
		if (j == sz) {
			sz = sz * 3 / 2;
			p = g_realloc_n(p, sz + 3, sizeof(*p));
		}
		p[++j].value = h;
	}
	op->len[i] = j;
	op->file[i] = p;
}

static void
prune(DiffOperation *op)
{
	int i, j;

	for (op->pref = 0; op->pref < op->len[0] && op->pref < op->len[1] &&
	    op->file[0][op->pref + 1].value == op->file[1][op->pref + 1].value;
	    op->pref++)
		;
	for (op->suff = 0; op->suff < op->len[0] - op->pref && op->suff < op->len[1] - op->pref &&
	    op->file[0][op->len[0] - op->suff].value == op->file[1][op->len[1] - op->suff].value;
	    op->suff++)
		;
	for (j = 0; j < 2; j++) {
		op->sfile[j] = op->file[j] + op->pref;
		op->slen[j] = op->len[j] - op->pref - op->suff;
		for (i = 0; i <= op->slen[j]; i++)
			op->sfile[j][i].serial = i;
	}
}

static void
equiv(struct line *a, int n, struct line *b, int m, int *c)
{
	int i, j;

	i = j = 1;
	while (i <= n && j <= m) {
		if (a[i].value < b[j].value)
			a[i++].value = 0;
		else if (a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while (i <= n)
		a[i++].value = 0;
	b[m + 1].value = 0;
	j = 0;
	while (++j <= m) {
		c[j] = -b[j].serial;
		while (b[j + 1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}

/* Code taken from ping.c */
static int
isqrt(int n)
{
	int y, x = 1;

	if (n == 0)
		return (0);

	do { /* newton was a stinker */
		y = x;
		x = n / x;
		x += y;
		x /= 2;
	} while ((x - y) > 1 || (x - y) < -1);

	return (x);
}

static int
stone(DiffOperation *op, int *a, int n, int *b, int *c, int flags)
{
	int i, k, y, j, l;
	int oldc, tc, oldl, sq;
	unsigned int numtries, bound;

	if (flags & D_MINIMAL)
		bound = UINT_MAX;
	else {
		sq = isqrt(n);
		bound = MAXIMUM(256, sq);
	}

	k = 0;
	c[0] = newcand(op, 0, 0, 0);
	for (i = 1; i <= n; i++) {
		j = a[i];
		if (j == 0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		numtries = 0;
		do {
			if (y <= op->clist[oldc].y)
				continue;
			l = search(op, c, k, y);
			if (l != oldl + 1)
				oldc = c[l - 1];
			if (l <= k) {
				if (op->clist[c[l]].y <= y)
					continue;
				tc = c[l];
				c[l] = newcand(op, i, y, oldc);
				oldc = tc;
				oldl = l;
				numtries++;
			} else {
				c[l] = newcand(op, i, y, oldc);
				k++;
				break;
			}
		} while ((y = b[++j]) > 0 && numtries < bound);
	}
	return (k);
}

static int
newcand(DiffOperation *op, int x, int y, int pred)
{
	struct cand *q;

	if (op->clen == op->clistlen) {
		op->clistlen = op->clistlen * 11 / 10;
		op->clist = g_realloc_n(op->clist, op->clistlen, sizeof(*op->clist));
	}
	q = op->clist + op->clen;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return (op->clen++);
}

static int
search(DiffOperation *op, int *c, int k, int y)
{
	int i, j, l, t;

	if (op->clist[c[k]].y < y)	/* quick look for typical case */
		return (k + 1);
	i = 0;
	j = k + 1;
	for (;;) {
		l = (i + j) / 2;
		if (l <= i)
			break;
		t = op->clist[c[l]].y;
		if (t > y)
			j = l;
		else if (t < y)
			i = l;
		else
			return (l);
	}
	return (l + 1);
}

static void
unravel(DiffOperation *op, int p)
{
	struct cand *q;
	int i;

	for (i = 0; i <= op->len[0]; i++)
		op->J[i] = i <= op->pref ? i :
		    i > op->len[0] - op->suff ? i + op->len[1] - op->len[0] : 0;
	for (q = op->clist + p; q->y != 0; q = op->clist + q->pred)
		op->J[q->x + op->pref] = q->y + op->pref;
}

static int
stream_getc (GInputStream *fd)
{
  gssize len;
  guchar byte;
  len = g_input_stream_read (fd, &byte, 1, NULL, NULL);
  if (len != 1)
    return EOF;
  return byte;
}

/*
 * Check does double duty:
 *  1.	ferret out any fortuitous correspondences due
 *	to confounding by hashing (which result in "jackpot")
 *  2.  collect random access indexes to the two files
 */
static void
check(DiffOperation *op, GInputStream *f1, GInputStream *f2, int flags)
{
	int i, j, jackpot, c, d;
	long ctold, ctnew;

	stream_rewind (f1);
	stream_rewind (f2);
	j = 1;
	op->ixold[0] = op->ixnew[0] = 0;
	jackpot = 0;
	ctold = ctnew = 0;
	for (i = 1; i <= op->len[0]; i++) {
		if (op->J[i] == 0) {
			op->ixold[i] = ctold += skipline(f1);
			continue;
		}
		while (j < op->J[i]) {
			op->ixnew[j] = ctnew += skipline(f2);
			j++;
		}
		if (flags & (D_FOLDBLANKS|D_IGNOREBLANKS)) {
			for (;;) {
				c = stream_getc(f1);
				d = stream_getc(f2);
				/*
				 * GNU diff ignores a missing newline
				 * in one file for -b or -w.
				 */
				if (c == EOF && d == '\n') {
					ctnew++;
					break;
				} else if (c == '\n' && d == EOF) {
					ctold++;
					break;
				}

				ctold++;
				ctnew++;
				if ((flags & D_FOLDBLANKS) && isspace(c) &&
				    isspace(d)) {
					do {
						if (c == '\n')
							break;
						ctold++;
					} while (isspace(c = stream_getc(f1)));
					do {
						if (d == '\n')
							break;
						ctnew++;
					} while (isspace(d = stream_getc(f2)));
				} else if ((flags & D_IGNOREBLANKS)) {
					while (isspace(c) && c != '\n') {
						c = stream_getc(f1);
						ctold++;
					}
					while (isspace(d) && d != '\n') {
						d = stream_getc(f2);
						ctnew++;
					}
				}
				if (c != d) {
					jackpot++;
					op->J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		} else {
			for (;;) {
				ctold++;
				ctnew++;
				if ((c = stream_getc(f1)) != (d = stream_getc(f2))) {
					/* jackpot++; */
					op->J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		}
		op->ixold[i] = ctold;
		op->ixnew[j] = ctnew;
		j++;
	}
	for (; j <= op->len[1]; j++)
		op->ixnew[j] = ctnew += skipline(f2);

	(void)jackpot;
	/*
	 * if (jackpot)
	 *	fprintf(stderr, "jackpot\n");
	 */
}

/* shellsort CACM #201 */
static void
sort(struct line *a, int n)
{
	struct line *ai, *aim, w;
	int j, m = 0, k;

	if (n == 0)
		return;
	for (j = 1; j <= n; j *= 2)
		m = 2 * j - 1;
	for (m /= 2; m != 0; m /= 2) {
		k = n - m;
		for (j = 1; j <= k; j++) {
			for (ai = &a[j]; ai > a; ai -= m) {
				aim = &ai[m];
				if (aim < ai)
					break;	/* wraparound */
				if (aim->value > ai[0].value ||
				    (aim->value == ai[0].value &&
					aim->serial > ai[0].serial))
					break;
				w.value = ai[0].value;
				ai[0].value = aim->value;
				aim->value = w.value;
				w.serial = ai[0].serial;
				ai[0].serial = aim->serial;
				aim->serial = w.serial;
			}
		}
	}
}

static void
unsort(struct line *f, int l, int *b)
{
	int *a, i;

	a = g_malloc0_n(l + 1, sizeof(*a));
	for (i = 1; i <= l; i++)
		a[f[i].serial] = f[i].value;
	for (i = 1; i <= l; i++)
		b[i] = a[i];
	g_free(a);
}

static int
skipline(GInputStream *f)
{
	int i, c;

	for (i = 1; (c = stream_getc(f)) != '\n' && c != EOF; i++)
		continue;
	return (i);
}

static void
output(DiffOperation *op, GInputStream *f1, GInputStream *f2, int flags)
{
	int m, i0, i1, j0, j1;

	stream_rewind (f1);
	stream_rewind (f2);
	m = op->len[0];
	op->J[0] = 0;
	op->J[m + 1] = op->len[1] + 1;

	for (i0 = 1; i0 <= m; i0 = i1 + 1) {
		while (i0 <= m && op->J[i0] == op->J[i0 - 1] + 1)
			i0++;
		j0 = op->J[i0 - 1] + 1;
		i1 = i0 - 1;
		while (i1 < m && op->J[i1 + 1] == 0)
			i1++;
		j1 = op->J[i1 + 1] - 1;
		op->J[i1] = j1;
		change(op, f1, f2, i0, i1, j0, j1, &flags);
	}

	if (m == 0)
		change(op, f1, f2, 1, 0, 1, op->len[1], &flags);

	if (op->anychange != 0)
		dump_unified_vec(op, f1, f2, flags);
}

static void
diff_output (DiffOperation *op, const char *format, ...)
{
  va_list args;
  gboolean ret;

  va_start (args, format);
  ret = g_output_stream_vprintf (op->out, NULL, NULL, NULL, format, args);
  va_end (args);

  if (!ret)
    g_error ("Failed to write to output stream");
}

static void
uni_range(DiffOperation *op, int a, int b)
{
	if (a < b)
		diff_output(op, "%d,%d", a, b - a + 1);
	else if (a == b)
		diff_output(op, "%d", b);
	else
		diff_output(op, "%d,0", b);
}

/*
 * Indicate that there is a difference between lines a and b of the from file
 * to get to lines c to d of the to file.  If a is greater then b then there
 * are no lines in the from file involved and this means that there were
 * lines appended (beginning at b).  If c is greater than d then there are
 * lines missing from the to file.
 */
static void
change(DiffOperation *op, GInputStream *f1, GInputStream *f2, int a, int b, int c, int d,
    int *pflags)
{
	static size_t max_context = 64;

	if (a > b && c > d)
		return;

	/*
	 * Allocate change records as needed.
	 */
	if (op->context_vec_ptr == op->context_vec_end - 1) {
		ptrdiff_t offset = op->context_vec_ptr - op->context_vec_start;
		max_context <<= 1;
		op->context_vec_start = g_realloc_n(op->context_vec_start,
		    max_context, sizeof(*op->context_vec_start));
		op->context_vec_end = op->context_vec_start + max_context;
		op->context_vec_ptr = op->context_vec_start + offset;
	}
	if (op->anychange == 0) {
		/*
		 * Print the context/unidiff header first time through.
		 */
		print_header(op);
		op->anychange = 1;
	} else if (a > op->context_vec_ptr->b + (2 * diff_context) + 1 &&
	    c > op->context_vec_ptr->d + (2 * diff_context) + 1) {
		/*
		 * If this change is more than 'diff_context' lines from the
		 * previous change, dump the record and reset it.
		 */
		dump_unified_vec(op, f1, f2, *pflags);
	}
	op->context_vec_ptr++;
	op->context_vec_ptr->a = a;
	op->context_vec_ptr->b = b;
	op->context_vec_ptr->c = c;
	op->context_vec_ptr->d = d;
}

static int
fetch(DiffOperation *op, long *f, int a, int b, GInputStream *lb, int ch, int oldfile, int flags)
{
	int i, j, c, col, nc;

	if (a > b)
		return (0);
	for (i = a; i <= b; i++) {
		if (!g_seekable_seek (G_SEEKABLE (lb), f[i - 1], G_SEEK_SET, NULL, NULL))
			g_error ("Failed to seek stream to position %ld", f[i - 1]);
		nc = f[i] - f[i - 1];
		if (ch != '\0') {
			diff_output(op, "%c", ch);
			if (Tflag)
				diff_output(op, "\t");
		}
		col = 0;
		for (j = 0; j < nc; j++) {
			if ((c = stream_getc(lb)) == EOF) {
				diff_output(op, "\n\\ No newline at end of file\n");
				return (0);
			}
			if (c == '\t' && (flags & D_EXPANDTABS)) {
				do {
					diff_output(op, " ");
				} while (++col & 7);
			} else {
				diff_output(op, "%c", c);
				col++;
			}
		}
	}
	return (0);
}

/*
 * Hash function taken from Robert Sedgewick, Algorithms in C, 3d ed., p 578.
 */
static int
readhash(GInputStream *f, int flags)
{
	int i, t, space;
	int sum;

	sum = 1;
	space = 0;
	if ((flags & (D_FOLDBLANKS|D_IGNOREBLANKS)) == 0) {
		for (i = 0; (t = stream_getc(f)) != '\n'; i++) {
			if (t == EOF) {
				if (i == 0)
					return (0);
				break;
			}
			sum = sum * 127 + t;
		}
	} else {
		for (i = 0;;) {
			switch (t = stream_getc(f)) {
			case '\t':
			case '\r':
			case '\v':
			case '\f':
			case ' ':
				space++;
				continue;
			default:
				if (space && (flags & D_IGNOREBLANKS) == 0) {
					i++;
					space = 0;
				}
				sum = sum * 127 + t;
				i++;
				continue;
			case EOF:
				if (i == 0)
					return (0);
				/* FALLTHROUGH */
#if defined(__GNUC__) || defined(__clang__)
				__attribute__((fallthrough));
#elif defined(_MSC_VER)
				__fallthrough;
#endif
			case '\n':
				break;
			}
			break;
		}
	}
	/*
	 * There is a remote possibility that we end up with a zero sum.
	 * Zero is used as an EOF marker, so return 1 instead.
	 */
	return (sum == 0 ? 1 : sum);
}

#define begins_with(s, pre) (strncmp(s, pre, sizeof(pre)-1) == 0)

static char *
match_function(DiffOperation *op, const long *f, int pos, GInputStream *fp)
{
	char buf[FUNCTION_CONTEXT_SIZE];
	size_t nc;
	int last = op->lastline;
	const char *state = NULL;

	op->lastline = pos;
	while (pos > last) {
		if (!g_seekable_seek(G_SEEKABLE (fp), f[pos - 1], G_SEEK_SET, NULL, NULL))
			g_error ("Failed to seek stream to position %ld", f[pos - 1]);
		nc = f[pos] - f[pos - 1];
		if (nc >= sizeof(buf))
			nc = sizeof(buf) - 1;
		nc = g_input_stream_read(fp, buf, nc, NULL, NULL);
		if (nc > 0) {
			buf[nc] = '\0';
			buf[strcspn(buf, "\n")] = '\0';
			if (isalpha(buf[0]) || buf[0] == '_' || buf[0] == '$') {
				if (begins_with(buf, "private:")) {
					if (!state)
						state = " (private)";
				} else if (begins_with(buf, "protected:")) {
					if (!state)
						state = " (protected)";
				} else if (begins_with(buf, "public:")) {
					if (!state)
						state = " (public)";
				} else {
					g_strlcpy(op->lastbuf, buf, sizeof op->lastbuf);
					if (state)
						g_strlcat(op->lastbuf, state,
						    sizeof op->lastbuf);
					op->lastmatchline = pos;
					return op->lastbuf;
				}
			}
		}
		pos--;
	}
	return op->lastmatchline > 0 ? op->lastbuf : NULL;
}


/* dump accumulated "unified" diff changes */
static void
dump_unified_vec(DiffOperation *op, GInputStream *f1, GInputStream *f2, int flags)
{
	struct context_vec *cvp = op->context_vec_start;
	int lowa, upb, lowc, upd;
	int a, b, c, d;
	char ch, *f;

	if (op->context_vec_start > op->context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = MAXIMUM(1, cvp->a - diff_context);
	upb = MINIMUM(op->len[0], op->context_vec_ptr->b + diff_context);
	lowc = MAXIMUM(1, cvp->c - diff_context);
	upd = MINIMUM(op->len[1], op->context_vec_ptr->d + diff_context);

	diff_output(op, "@@ -");
	uni_range(op, lowa, upb);
	diff_output(op, " +");
	uni_range(op, lowc, upd);
	diff_output(op, " @@");
	if ((flags & D_PROTOTYPE)) {
		f = match_function(op, op->ixold, lowa-1, f1);
		if (f != NULL)
			diff_output(op, " %s", f);
	}
	diff_output(op, "\n");

	/*
	 * Output changes in "unified" diff format--the old and new lines
	 * are printed together.
	 */
	for (; cvp <= op->context_vec_ptr; cvp++) {
		a = cvp->a;
		b = cvp->b;
		c = cvp->c;
		d = cvp->d;

		/*
		 * c: both new and old changes
		 * d: only changes in the old file
		 * a: only changes in the new file
		 */
		if (a <= b && c <= d)
			ch = 'c';
		else
			ch = (a <= b) ? 'd' : 'a';

		switch (ch) {
		case 'c':
			fetch(op, op->ixold, lowa, a - 1, f1, ' ', 0, flags);
			fetch(op, op->ixold, a, b, f1, '-', 0, flags);
			fetch(op, op->ixnew, c, d, f2, '+', 0, flags);
			break;
		case 'd':
			fetch(op, op->ixold, lowa, a - 1, f1, ' ', 0, flags);
			fetch(op, op->ixold, a, b, f1, '-', 0, flags);
			break;
		case 'a':
			fetch(op, op->ixnew, lowc, c - 1, f2, ' ', 0, flags);
			fetch(op, op->ixnew, c, d, f2, '+', 0, flags);
			break;
		default:
			g_assert_not_reached ();
		}
		lowa = b + 1;
		lowc = d + 1;
	}
	fetch(op, op->ixnew, d + 1, upd, f2, ' ', 0, flags);

	op->context_vec_ptr = op->context_vec_start - 1;
}

static void
print_header(DiffOperation *op)
{
	diff_output(op, "%s a/%s\n", "---", op->filename);
	diff_output(op, "%s b/%s\n", "+++", op->filename);
}
