/* 
 * GdkPixbuf library - TGA image loader
 * Copyright (C) 1999 Nicola Girardi <nikke@swlibero.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/*
 * Some NOTES about the TGA loader (2001/06/07, nikke@swlibero.org)
 *
 * - The module doesn't currently provide support for TGA images where the
 *   order of the pixels isn't left-to-right and top-to-bottom.  I plan to
 *   add support for those files as soon as I get one of them.  I haven't
 *   run into one yet.  (And I don't seem to be able to create it with GIMP.)
 *
 * - The TGAFooter isn't present in all TGA files.  In fact, there's an older
 *   format specification, still in use, which doesn't cover the TGAFooter.
 *   Actually, most TGA files I have are of the older type.  Anyway I put the 
 *   struct declaration here for completeness.
 *
 * - Error handling was designed to be very paranoid.
 */

#include <stdio.h>
#include <string.h>

#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"
#include "gdk-pixbuf-private.h"

#define TGA_INTERLEAVE_MASK     0xc0
#define TGA_INTERLEAVE_NONE     0x00
#define TGA_INTERLEAVE_2WAY     0x40
#define TGA_INTERLEAVE_4WAY     0x80

#define TGA_ORIGIN_MASK         0x30
#define TGA_ORIGIN_LEFT         0x00
#define TGA_ORIGIN_RIGHT        0x10
#define TGA_ORIGIN_LOWER        0x00
#define TGA_ORIGIN_UPPER        0x20

enum {
	TGA_TYPE_NODATA = 0,
	TGA_TYPE_PSEUDOCOLOR = 1,
	TGA_TYPE_TRUECOLOR = 2,
	TGA_TYPE_GRAYSCALE = 3,
	TGA_TYPE_RLE_PSEUDOCOLOR = 9,
	TGA_TYPE_RLE_TRUECOLOR = 10,
	TGA_TYPE_RLE_GRAYSCALE = 11
};

#define LE16(p) ((p)[0] + ((p)[1] << 8))

typedef struct _IOBuffer IOBuffer;

typedef struct _TGAHeader TGAHeader;
typedef struct _TGAFooter TGAFooter;

typedef struct _TGAColormap TGAColormap;
typedef struct _TGAColor TGAColor;

typedef struct _TGAContext TGAContext;

struct _TGAHeader {
	guint8 infolen;
	guint8 has_cmap;
	guint8 type;
	
	guint8 cmap_start[2];
	guint8 cmap_n_colors[2];
	guint8 cmap_bpp;
	
	guint8 x_origin[2];
	guint8 y_origin[2];
	
	guint8 width[2];
	guint8 height[2];
	guint8 bpp;
	
	guint8 flags;
};

struct _TGAFooter {
	guint32 extension_area_offset;
	guint32 developer_directory_offset;

	/* Standard TGA signature, "TRUEVISION-XFILE.\0". */
	union {
		gchar sig_full[18];
		struct {
			gchar sig_chunk[16];
			gchar dot, null;
		} sig_struct;
	} sig;
};

struct _TGAColormap {
	gint size;
	TGAColor *cols;
};

struct _TGAColor {
	guchar r, g, b, a;
};

struct _TGAContext {
	TGAHeader *hdr;
	guint rowstride;
	guint completed_lines;
	gboolean run_length_encoded;

	TGAColormap *cmap;
	guint cmap_size;

	GdkPixbuf *pbuf;
	guint pbuf_bytes;
	guint pbuf_bytes_done;
	guchar *pptr;

	IOBuffer *in;

	gboolean skipped_info;
	gboolean prepared;
	gboolean done;

	ModulePreparedNotifyFunc pfunc;
	ModuleUpdatedNotifyFunc ufunc;
	gpointer udata;
};

struct _IOBuffer {
	guchar *data;
	guint size;
};

static IOBuffer *io_buffer_new(GError **err)
{
	IOBuffer *buffer;
	buffer = g_try_malloc(sizeof(IOBuffer));
	if (!buffer) {
		g_set_error(err, GDK_PIXBUF_ERROR,
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate memory for IOBuffer struct"));
		return NULL;
	}
	buffer->data = NULL;
	buffer->size = 0;
	return buffer;
}

static IOBuffer *io_buffer_append(IOBuffer *buffer, 
				  const guchar *data, guint len, 
				  GError **err)
{
	if (!buffer)
		return NULL;
	if (!buffer->data) {
		buffer->data = g_try_malloc(len);
		if (!buffer->data) {
			g_set_error(err, GDK_PIXBUF_ERROR,
				    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				    _("Can't allocate memory for IOBuffer data"));
			g_free(buffer);
			return NULL;
		}
		g_memmove(buffer->data, data, len);
		buffer->size = len;
	} else {
		buffer->data = g_try_realloc(buffer->data, buffer->size + len);
		if (!buffer->data) {
			g_set_error(err, GDK_PIXBUF_ERROR,
				    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				    _("Can't realloc IOBuffer data"));
			g_free(buffer);
			return NULL;
		}
		g_memmove(&buffer->data[buffer->size], data, len);
		buffer->size += len;
	}
	return buffer;
}

static IOBuffer *io_buffer_free_segment(IOBuffer *buffer, 
					guint count,
                                        GError **err)
{
	g_return_val_if_fail(buffer != NULL, NULL);
	g_return_val_if_fail(buffer->data != NULL, NULL);
	if (count == buffer->size) {
		g_free(buffer->data);
		buffer->data = NULL;
		buffer->size = 0;
	} else {
		guchar *new_buf;
		guint new_size;

		new_size = buffer->size - count;
		new_buf = g_try_malloc(new_size);
		if (!new_buf) {
			g_set_error(err, GDK_PIXBUF_ERROR,
				    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				    _("Can't allocate temporary IOBuffer data"));
			g_free(buffer->data);
			g_free(buffer);
			return NULL;
		}

		g_memmove(new_buf, &buffer->data[count], new_size);
		g_free(buffer->data);
		buffer->data = new_buf;
		buffer->size = new_size;
	}
	return buffer;
}

static void io_buffer_free(IOBuffer *buffer)
{
	g_return_if_fail(buffer != NULL);
	if (buffer->data)
		g_free(buffer->data);
	g_free(buffer);
}

static void free_buffer(guchar *pixels, gpointer data)
{
	g_free(pixels);
}

static gboolean fread_check(gpointer dest, 
			    size_t size, size_t count, 
			    FILE *f, GError **err)
{
	if (fread(dest, size, count, f) != count) {
		g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
			    _("fread() failed -- premature end-of-file probably encountered"));
		return FALSE;
	}
	return TRUE;
}

static gboolean fseek_check(FILE *f, glong offset, gint whence, GError **err)
{
	if (fseek(f, offset, whence) != 0) {
		g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
			    _("fseek() failed -- premature end-of-file probably encountered"));
		return FALSE;
	}
	return TRUE;
}

static gboolean fill_in_context(TGAContext *ctx, GError **err)
{
	gboolean alpha;

	g_return_val_if_fail(ctx != NULL, FALSE);

	ctx->run_length_encoded =
		((ctx->hdr->type == TGA_TYPE_RLE_PSEUDOCOLOR)
		 || (ctx->hdr->type == TGA_TYPE_RLE_TRUECOLOR)
		 || (ctx->hdr->type == TGA_TYPE_RLE_GRAYSCALE));

	if (ctx->hdr->has_cmap)
		ctx->cmap_size = ((ctx->hdr->cmap_bpp + 7) >> 3) *
			LE16(ctx->hdr->cmap_n_colors);

	alpha = ((ctx->hdr->bpp == 32) ||
		 (ctx->hdr->has_cmap && (ctx->hdr->cmap_bpp == 32)));

	ctx->pbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, alpha, 8,
				   LE16(ctx->hdr->width),
				   LE16(ctx->hdr->height));
	if (!ctx->pbuf) {
		g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate new pixbuf"));
		return FALSE;
	}
	ctx->pbuf_bytes = ctx->pbuf->rowstride * ctx->pbuf->height;
	ctx->pptr = ctx->pbuf->pixels;

	if ((ctx->hdr->type == TGA_TYPE_PSEUDOCOLOR) ||
	    (ctx->hdr->type == TGA_TYPE_GRAYSCALE))
		ctx->rowstride = ctx->pbuf->width;
	else if (ctx->hdr->type == TGA_TYPE_TRUECOLOR)
		ctx->rowstride = ctx->pbuf->rowstride;

	ctx->completed_lines = 0;
	return TRUE;
}

static void parse_data_for_row_pseudocolor(TGAContext *ctx)
{
	guchar *s = ctx->in->data;
	guint upper_bound = ctx->pbuf->width;

	for (; upper_bound; upper_bound--, s++) {
		*ctx->pptr++ = ctx->cmap->cols[*s].r;
		*ctx->pptr++ = ctx->cmap->cols[*s].g;
		*ctx->pptr++ = ctx->cmap->cols[*s].b;
		if (ctx->hdr->cmap_bpp == 32)
			*ctx->pptr++ = ctx->cmap->cols[*s].a;
	}
	ctx->pbuf_bytes_done += ctx->pbuf->n_channels * ctx->pbuf->width;
	if (ctx->pbuf_bytes_done == ctx->pbuf_bytes)
		ctx->done = TRUE;
}

static void swap_channels(TGAContext *ctx)
{
	register guchar swap;
	register guint count;
	for (count = ctx->pbuf->width; count; count--) {
		swap = ctx->pptr[0];
		ctx->pptr[0] = ctx->pptr[2];
		ctx->pptr[2] = swap;
		ctx->pptr += ctx->pbuf->n_channels;
	}
}

static void parse_data_for_row_truecolor(TGAContext *ctx)
{
	g_memmove(ctx->pptr, ctx->in->data, ctx->pbuf->rowstride);
	swap_channels(ctx);
	ctx->pbuf_bytes_done += ctx->pbuf->rowstride;
	if (ctx->pbuf_bytes_done == ctx->pbuf_bytes)
		ctx->done = TRUE;
}

static void parse_data_for_row_grayscale(TGAContext *ctx)
{
	guchar *s = ctx->in->data;
	guint upper_bound = ctx->pbuf->width;

	for (; upper_bound; upper_bound--) {
		ctx->pptr[0] = ctx->pptr[1] = ctx->pptr[2] = *s++;
		ctx->pptr += 3;
	}
	ctx->pbuf_bytes_done = ctx->pbuf->width * 3;
	if (ctx->pbuf_bytes_done == ctx->pbuf_bytes)
		ctx->done = TRUE;
}

static gboolean parse_data_for_row(TGAContext *ctx, GError **err)
{
	if (ctx->hdr->type == TGA_TYPE_PSEUDOCOLOR)
		parse_data_for_row_pseudocolor(ctx);
	else if (ctx->hdr->type == TGA_TYPE_TRUECOLOR)
		parse_data_for_row_truecolor(ctx);
	else if (ctx->hdr->type == TGA_TYPE_GRAYSCALE)
		parse_data_for_row_grayscale(ctx);
	ctx->in = io_buffer_free_segment(ctx->in, ctx->rowstride, err);
	if (!ctx->in)
		return FALSE;
	(*ctx->ufunc) (ctx->pbuf, 0,
		       (ctx->pbuf_bytes_done / ctx->pbuf->rowstride) - 1,
		       ctx->pbuf->width, 1, ctx->udata);
	return TRUE;
}

static void write_rle_data(TGAContext *ctx, TGAColor *color, guint *rle_count)
{
	ctx->pbuf_bytes_done += ctx->pbuf->n_channels * (*rle_count);
	for (; *rle_count; (*rle_count)--) {
		g_memmove(ctx->pptr, (guchar *) color, ctx->pbuf->n_channels);
		ctx->pptr += ctx->pbuf->n_channels;
	}
}

static guint parse_rle_data_pseudocolor(TGAContext *ctx)
{
	guint rle_num, raw_num;
	guchar *s, tag;
	guint n;

	g_return_val_if_fail(ctx->in->size > 0, 0);
	s = ctx->in->data;

	for (n = 0; n < ctx->in->size; ) {
		tag = *s;
		s++, n++;
		if (tag & 0x80) {
			if (n == ctx->in->size) {
				return --n;
			} else {
				rle_num = (tag & 0x7f) + 1;
				write_rle_data(ctx, &ctx->cmap->cols[*s], &rle_num);
				s++, n++;
			}
		} else {
			raw_num = tag + 1;
			if (n + raw_num >= ctx->in->size) {
				return --n;
			} else {
				for (; raw_num; raw_num--) {
					*ctx->pptr++ =
						ctx->cmap->cols[*s].r;
					*ctx->pptr++ =
						ctx->cmap->cols[*s].g;
					*ctx->pptr++ =
						ctx->cmap->cols[*s].b;
					if (ctx->pbuf->n_channels == 4)
						*ctx->pptr++ = ctx->cmap->cols[*s].a;
					s++, n++;
					ctx->pbuf_bytes_done += ctx->pbuf->n_channels;
				}
			}
		}
	}
	if (ctx->pbuf_bytes_done == ctx->pbuf_bytes)
		ctx->done = TRUE;
	return n;
}

static void swap_channels_rle(TGAContext *ctx, guint count)
{
	register guchar swap;
	for (; count; count--) {
		swap = ctx->pptr[0];
		ctx->pptr[0] = ctx->pptr[2];
		ctx->pptr[2] = swap;
		ctx->pptr += ctx->pbuf->n_channels;
	}
}

static guint parse_rle_data_truecolor(TGAContext *ctx)
{
	TGAColor col;
	guint rle_num, raw_num;
	guchar *s, tag;
	guint n = 0;

	g_return_val_if_fail(ctx->in->size > 0, 0);
	s = ctx->in->data;

	for (n = 0; n < ctx->in->size; ) {
		tag = *s;
		s++, n++;
		if (tag & 0x80) {
			if (n + ctx->pbuf->n_channels >= ctx->in->size) {
				return --n;
			} else {
				rle_num = (tag & 0x7f) + 1;
				col.b = *s++;
				col.g = *s++;
				col.r = *s++;
				if (ctx->hdr->bpp == 32)
					col.a = *s++;
				write_rle_data(ctx, &col, &rle_num);
				n += ctx->pbuf->n_channels;
			}
		} else {
			raw_num = tag + 1;
			if (n + (raw_num * ctx->pbuf->n_channels) >= ctx->in->size) {
				return --n;
			} else {
				g_memmove(ctx->pptr, s, raw_num * ctx->pbuf->n_channels);
				swap_channels_rle(ctx, raw_num);
				s += raw_num * ctx->pbuf->n_channels;
				n += raw_num * ctx->pbuf->n_channels;
				ctx->pbuf_bytes_done += raw_num * ctx->pbuf->n_channels;
			}
		}
	}
	if (ctx->pbuf_bytes_done == ctx->pbuf_bytes)
		ctx->done = TRUE;
	return n;
}

static guint parse_rle_data_grayscale(TGAContext *ctx)
{
	TGAColor tone;
	guint rle_num, raw_num;
	guchar *s, tag;
	guint n;

	g_return_val_if_fail(ctx->in->size > 0, 0);
	s = ctx->in->data;

	for (n = 0; n < ctx->in->size; ) {
		tag = *s;
		s++, n++;
		if (tag & 0x80) {
			if (n == ctx->in->size) {
				return --n;
			} else {
				rle_num = (tag & 0x7f) + 1;
				tone.r = tone.g = tone.b = *s;
				s++, n++;
				write_rle_data(ctx, &tone, &rle_num);
			}
		} else {
			raw_num = tag + 1;
			if (n + raw_num >= ctx->in->size) {
				return --n;
			} else {
				for (; raw_num; raw_num--) {
					ctx->pptr[0] = ctx->pptr[1] = ctx->pptr[2] = *s;
					s++, n++;
					ctx->pptr += 3;
					ctx->pbuf_bytes_done += 3;
				}
			}
		}
	}
	if (ctx->pbuf_bytes_done == ctx->pbuf_bytes)
		ctx->done = TRUE;
	return n;
}

static gboolean parse_rle_data(TGAContext *ctx, GError **err)
{
	guint count = 0;
	guint pbuf_count = 0;
	if (ctx->hdr->type == TGA_TYPE_RLE_PSEUDOCOLOR) {
		count = parse_rle_data_pseudocolor(ctx);
		pbuf_count = count *ctx->pbuf->n_channels;
	} else if (ctx->hdr->type == TGA_TYPE_RLE_TRUECOLOR) {
		count = parse_rle_data_truecolor(ctx);
		pbuf_count = count;
	} else if (ctx->hdr->type == TGA_TYPE_RLE_GRAYSCALE) {
		count = parse_rle_data_grayscale(ctx);
		pbuf_count = count * 3;
	}
	ctx->in = io_buffer_free_segment(ctx->in, count, err);
	if (!ctx->in)
		return FALSE;
	(*ctx->ufunc) (ctx->pbuf, 0, ctx->pbuf_bytes_done / ctx->pbuf->rowstride,
		       ctx->pbuf->width, pbuf_count / ctx->pbuf->rowstride,
		       ctx->udata);
	return TRUE;
}

static gboolean try_colormap(TGAContext *ctx, GError **err)
{
	static guchar *p;
	static guint n;

	g_return_val_if_fail(ctx != NULL, FALSE);
	g_return_val_if_fail(ctx->cmap_size > 0, TRUE);

	ctx->cmap = g_try_malloc(sizeof(TGAColormap));
	if (!ctx->cmap) {
		g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate colormap structure"));
		return FALSE;
	}
	ctx->cmap->size = LE16(ctx->hdr->cmap_n_colors);
	ctx->cmap->cols = g_try_malloc(sizeof(TGAColor) * ctx->cmap->size);
	if (!ctx->cmap->cols) {
		g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate colormap entries"));
		g_free(ctx->cmap);
		return FALSE;
	}

	p = ctx->in->data;
	for (n = 0; n < ctx->cmap->size; n++) {
		if ((ctx->hdr->cmap_bpp == 15) || (ctx->hdr->cmap_bpp == 16)) {
			guint16 col = p[0] + (p[1] << 8);
			ctx->cmap->cols[n].b = (col >> 7) & 0xf8;
			ctx->cmap->cols[n].g = (col >> 2) & 0xf8;
			ctx->cmap->cols[n].r = col << 3;
			p += 2;
		}
		else if ((ctx->hdr->cmap_bpp == 24) || (ctx->hdr->cmap_bpp == 32)) {
			ctx->cmap->cols[n].b = *p++;
			ctx->cmap->cols[n].g = *p++;
			ctx->cmap->cols[n].r = *p++;
			if (ctx->hdr->cmap_bpp == 32)
				ctx->cmap->cols[n].a = *p++;
		} else {
			g_set_error(err, GDK_PIXBUF_ERROR, 
				    GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
				    _("Unexpected bitdepth for colormap entries"));
			g_free(ctx->cmap->cols);
			g_free(ctx->cmap);
			return FALSE;
		}
	}
	ctx->in = io_buffer_free_segment(ctx->in, ctx->cmap_size, err);
	if (!ctx->in)
		return FALSE;
	return TRUE;
}

static gboolean try_preload(TGAContext *ctx, GError **err)
{
	if (!ctx->hdr) {
		if (ctx->in->size >= sizeof(TGAHeader)) {
			ctx->hdr = g_try_malloc(sizeof(TGAHeader));
			if (!ctx->hdr) {
				g_set_error(err, GDK_PIXBUF_ERROR,
					    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
					    _("Can't allocate TGA header memory"));
				return FALSE;
			}
			g_memmove(ctx->hdr, ctx->in->data, sizeof(TGAHeader));
			ctx->in = io_buffer_free_segment(ctx->in, sizeof(TGAHeader), err);
			if (!ctx->in)
				return FALSE;
			if (!fill_in_context(ctx, err))
				return FALSE;
			if (ctx->hdr->infolen > 255) {
				g_set_error(err, GDK_PIXBUF_ERROR,
					    GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
					    _("TGA image comment length is too long"));
				return FALSE;
			}
			if ((ctx->hdr->flags & TGA_INTERLEAVE_MASK) != 
			    TGA_INTERLEAVE_NONE ||
			    ctx->hdr->flags & TGA_ORIGIN_RIGHT || 
			    ctx->hdr->flags & TGA_ORIGIN_LOWER) {
				g_set_error(err, GDK_PIXBUF_ERROR, 
					    GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
					    _("TGA image type not supported"));
				return FALSE;
			}
		} else {
			return TRUE;
		}
	}
	if (!ctx->skipped_info) {
		if (ctx->in->size >= ctx->hdr->infolen) {
			ctx->in = io_buffer_free_segment(ctx->in, ctx->hdr->infolen, err);
			if (!ctx->in)
				return FALSE;
			ctx->skipped_info = TRUE;
		} else {
			return TRUE;
		}
	}
	if (ctx->hdr->has_cmap && !ctx->cmap) {
		if (ctx->in->size >= ctx->cmap_size) {
			if (!try_colormap(ctx, err))
				return FALSE;
		} else {
			return TRUE;
		}
	}
	if (!ctx->prepared) {
		(*ctx->pfunc) (ctx->pbuf, NULL, ctx->udata);
		ctx->prepared = TRUE;
	}
	/* We shouldn't get here anyway. */
	return TRUE;
}

static gpointer gdk_pixbuf__tga_begin_load(ModulePreparedNotifyFunc f1,
					   ModuleUpdatedNotifyFunc f2,
					   gpointer udata, GError **err)
{
	TGAContext *ctx;

	ctx = g_try_malloc(sizeof(TGAContext));
	if (!ctx) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate memory for TGA context struct"));
		return NULL;
	}

	ctx->hdr = NULL;
	ctx->rowstride = 0;
	ctx->run_length_encoded = FALSE;

	ctx->cmap = NULL;
	ctx->cmap_size = 0;

	ctx->pbuf = NULL;
	ctx->pbuf_bytes = 0;
	ctx->pbuf_bytes_done = 0;
	ctx->pptr = NULL;

	ctx->in = io_buffer_new(err);
	if (!ctx->in) {
		g_free(ctx);
		return NULL;
	}

	ctx->skipped_info = FALSE;
	ctx->prepared = FALSE;
	ctx->done = FALSE;

	ctx->pfunc = f1;
	ctx->ufunc = f2;
	ctx->udata = udata;

	return ctx;
}

static gboolean gdk_pixbuf__tga_load_increment(gpointer data,
					       const guchar *buffer,
					       guint size,
					       GError **err)
{
	TGAContext *ctx = (TGAContext*) data;
	g_return_val_if_fail(ctx != NULL, FALSE);

	if (ctx->done)
		return TRUE;

	g_return_val_if_fail(buffer != NULL, TRUE);
	ctx->in = io_buffer_append(ctx->in, buffer, size, err);
	if (!ctx->in)
		return FALSE;
	if (!ctx->prepared) {
		if (!try_preload(ctx, err))
			return FALSE;
		if (!ctx->prepared)
			return TRUE;
		if (ctx->in->size == 0)
			return TRUE;
	}

	if (ctx->run_length_encoded) {
		if (!parse_rle_data(ctx, err))
			return FALSE;
	} else {
		while (ctx->in->size >= ctx->rowstride) {
			if (ctx->completed_lines >= ctx->pbuf->height) {
				g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
					    _("Excess data in file"));
				return FALSE;
			}
			if (!parse_data_for_row(ctx, err))
				return FALSE;
			ctx->completed_lines++;
		}
	}

	return TRUE;
}

static gboolean gdk_pixbuf__tga_stop_load(gpointer data, GError **err)
{
	TGAContext *ctx = (TGAContext *) data;
	g_return_val_if_fail(ctx != NULL, FALSE);

	g_free(ctx->hdr);
	if (ctx->cmap)
		g_free(ctx->cmap);
	g_object_unref(ctx->pbuf);
	if (ctx->in->size)
		ctx->in = io_buffer_free_segment(ctx->in, ctx->in->size, err);
	if (!ctx->in) {
		g_free(ctx);
		return FALSE;
	}
	io_buffer_free(ctx->in);
	g_free(ctx);
	return TRUE;
}

static TGAHeader *get_header_from_file(FILE *f, GError **err)
{
	TGAHeader *hdr;

	if (!fseek_check(f, 0, SEEK_SET, err))
		return NULL;
	if (!(hdr = g_try_malloc(sizeof(TGAHeader)))) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate memory for TGA header"));
		return NULL;
	}
	if (!fread_check(hdr, sizeof(TGAHeader), 1, f, err)) {
		g_free(hdr);
		return NULL;
	}
	if (hdr->infolen > 255) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			    _("Too big value in the infolen field of TGA header."));
		g_free(hdr);
		return NULL;
	}

	return hdr;
}

static TGAColormap *get_colormap_from_file(FILE *f, 
					   TGAHeader *hdr,
					   GError **err)
{
	TGAColormap *cmap;
	guchar *pal_buf, *p;
	guint n, pal_size;
  
	if (!fseek_check(f, sizeof(TGAHeader) + hdr->infolen, SEEK_SET, err))
		return NULL;

	pal_size = LE16(hdr->cmap_n_colors) * ((hdr->cmap_bpp + 7) >> 3);
	pal_buf = g_try_malloc(pal_size);
	if (!pal_buf) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate memory for TGA cmap temporary buffer"));
		return NULL;
	}
	if (!fread_check(pal_buf, pal_size, 1, f, err)) {
		g_free(pal_buf);
		return NULL;
	}
	p = pal_buf;

	if (!(cmap = g_try_malloc(sizeof(TGAColormap)))) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate memory for TGA colormap struct"));
		g_free(pal_buf);
		return NULL;
	}
	cmap->size = LE16(hdr->cmap_n_colors);
	cmap->cols = g_try_malloc(sizeof(TGAColor) * cmap->size);
	if (!cmap->cols) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate memory for TGA colormap entries"));
		g_free(pal_buf);
		g_free(cmap);
		return NULL;
	}

	if (hdr->cmap_bpp != 15 && hdr->cmap_bpp != 16 &&
	    hdr->cmap_bpp != 24 && hdr->cmap_bpp != 32) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
			    _("Unexpected bitdepth for TGA colormap"));
		g_free(pal_buf);
		g_free(cmap->cols);
		g_free(cmap);
		return NULL;
	}

	for (n = 0; n < cmap->size; n++) {
		if ((hdr->cmap_bpp == 15) || (hdr->cmap_bpp == 16)) {
			guint16 col = p[0] + (p[1] << 8);
			p += 2;
			cmap->cols[n].b = (col >> 7) & 0xf8;
			cmap->cols[n].g = (col >> 2) & 0xf8;
			cmap->cols[n].r = col << 3;
		} else if ((hdr->cmap_bpp == 24) || (hdr->cmap_bpp == 32)) {
			cmap->cols[n].b = *p++;
			cmap->cols[n].g = *p++;
			cmap->cols[n].r = *p++;
			if (hdr->cmap_bpp == 32)
				cmap->cols[n].a = *p++;
		}
	}

	g_free(pal_buf);
	return cmap;
}

static GdkPixbuf *get_image_pseudocolor(FILE *f, TGAHeader *hdr,
					TGAColormap *cmap, gboolean rle,
					GError **err)
{
	GdkPixbuf *pbuf;
	guchar *p, color, tag;
	glong n, image_offset;
	guint count;

	image_offset = sizeof(TGAHeader) + hdr->infolen;
	if (!hdr->has_cmap) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			    _("Pseudo-Color image without colormap"));
		return NULL;
	} else {
		image_offset += cmap->size * ((hdr->cmap_bpp + 7) >> 3);
	}
	if (!fseek_check(f, image_offset, SEEK_SET, err)) {
		g_set_error(err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
			    _("Can't seek to image offset -- end-of-file probably encountered"));
		return NULL;
	}

	pbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, (hdr->cmap_bpp == 32), 8,
			      LE16(hdr->width), LE16(hdr->height));
	if (!pbuf) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate pixbuf"));
		return NULL;
	}
	pbuf->destroy_fn = free_buffer;
	pbuf->destroy_fn_data = NULL;
	p = pbuf->pixels;

	if (rle) {
		n = count = 0;
		for (; n < pbuf->width * pbuf->height;) {
			if (!fread_check(&tag, 1, 1, f, err)) {
				g_object_unref(pbuf);
				return NULL;
			}
			if (tag & 0x80) {
				count = (tag & 0x7f) + 1;
				n += count;
				if (!fread_check(&color, 1, 1, f, err)) {
					g_object_unref(pbuf);
					return NULL;
				}
				for (; count; count--) {
					*p++ = cmap->cols[color].r;
					*p++ = cmap->cols[color].g;
					*p++ = cmap->cols[color].b;
					if (hdr->cmap_bpp == 32)
						*p++ = cmap->cols[color].a;
				}
			} else {
				count = tag + 1;
				n += count;
				for (; count; count--) {
					if (!fread_check(&color, 1, 1, f, err)) {
						g_object_unref(pbuf);
						return NULL;
					}
					*p++ = cmap->cols[color].r;
					*p++ = cmap->cols[color].g;
					*p++ = cmap->cols[color].b;
					if (hdr->cmap_bpp == 32)
						*p++ = cmap->cols[color].a;
				}
			}
		}
	} else {
		for (n = 0; n < pbuf->width * pbuf->height; n++) {
			if (!fread_check(&color, 1, 1, f, err)) {
				g_object_unref(pbuf);
				return NULL;
			}
			*p++ = cmap->cols[color].r;
			*p++ = cmap->cols[color].g;
			*p++ = cmap->cols[color].b;
			if (hdr->cmap_bpp == 32)
				*p++ = cmap->cols[color].a;
		}
	}

	return pbuf;
}

static void swap_channels_pixbuf(GdkPixbuf *pbuf)
{
	guchar *p, swap;
	glong n;

	p = pbuf->pixels;
	for (n = 0; n < pbuf->width * pbuf->height; n++) {
		swap = p[0];
		p[0] = p[2];
		p[2] = swap;
		p += pbuf->n_channels;
	}
}

static GdkPixbuf *get_image_truecolor(FILE *f, TGAHeader *hdr,
				      gboolean rle, GError **err)
{
	GdkPixbuf *pbuf;
	guchar *p, tag;
	glong n, image_offset;
	guint32 pixel;
	guint count;

	image_offset = sizeof(TGAHeader) + hdr->infolen;
	/* A truecolor image shouldn't actually have a colormap. */
	if (hdr->has_cmap)
		image_offset += LE16(hdr->cmap_n_colors) * ((hdr->cmap_bpp + 7) >> 3);
	if (!fseek_check(f, image_offset, SEEK_SET, err))
		return NULL;

	pbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, (hdr->bpp == 32), 8,
			      LE16(hdr->width), LE16(hdr->height));
	if (!pbuf) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate pixbuf"));
		return NULL;
	}
	pbuf->destroy_fn = free_buffer;
	pbuf->destroy_fn_data = NULL;
	p = pbuf->pixels;

	if (rle) {
		n = count = 0;
		for (; n < pbuf->width * pbuf->height;) {
			if (!fread_check(&tag, 1, 1, f, err)) {
				g_object_unref(pbuf);
				return NULL;
			}
			if (tag & 0x80) {
				count = (tag & 0x7f) + 1;
				n += count;
				if (!fread_check(&pixel, pbuf->n_channels, 1, f, err)) {
					g_object_unref(pbuf);
					return NULL;
				}
				for (; count; count--) {
					g_memmove(p, &pixel, pbuf->n_channels);
					p += pbuf->n_channels;
				}
			} else {
				count = tag + 1;
				n += count;
				if (!fread_check(p, pbuf->n_channels * count, 1, f, err)) {
					g_object_unref(pbuf);
					return NULL;
				}
				p += pbuf->n_channels * count;
			}
		}
	} else {
		if (!fread_check(p, pbuf->rowstride * pbuf->height, 1, f, err)) {
			g_object_unref(pbuf);
			return NULL;
		}
	}

	swap_channels_pixbuf(pbuf);
	return pbuf;
}

static GdkPixbuf *get_image_grayscale(FILE *f, TGAHeader *hdr,
				      gboolean rle, GError **err)
{
	GdkPixbuf *pbuf;
	glong n, image_offset;
	guchar *p, color, tag;
	guint count;

	image_offset = sizeof(TGAHeader) + hdr->infolen;
	/* A grayscale image shouldn't actually have a colormap. */
	if (hdr->has_cmap)
		image_offset += LE16(hdr->cmap_n_colors) * ((hdr->cmap_bpp + 7) >> 3);
	if (!fseek_check(f, image_offset, SEEK_SET, err))
		return NULL;

	pbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
			      LE16(hdr->width), LE16(hdr->height));
	if (!pbuf) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			    _("Can't allocate pixbuf"));
		return NULL;
	}
	pbuf->destroy_fn = free_buffer;
	pbuf->destroy_fn_data = NULL;
	p = pbuf->pixels;

	if (rle) {
		n = count = 0;
		for (; n < pbuf->width * pbuf->height;) {
			if (!fread_check(&tag, 1, 1, f, err)) {
				g_object_unref(pbuf);
				return NULL;
			}
			if (tag & 0x80) {
				count = (tag & 0x7f) + 1;
				n += count;
				if (!fread_check(&color, 1, 1, f, err)) {
					g_object_unref(pbuf);
					return NULL;
				}
				for (; count; count--) {
					p[0] = p[1] = p[2] = color;
					p += 3;
				}
			} else {
				count = tag + 1;
				n += count;
				for (; count; count--) {
					if (!fread_check(&color, 1, 1, f, err)) {
						g_object_unref(pbuf);
						return NULL;
					}
					p[0] = p[1] = p[2] = color;
					p += 3;
				}
			}
		}
	} else {
		for (n = 0; n < pbuf->width * pbuf->height; n++) {
			if (!fread_check(&color, 1, 1, f, err)) {
				g_object_unref(pbuf);
				return NULL;
			}
			p[0] = p[1] = p[2] = color;
			p += 3;
		}
	}

	return pbuf;
}

static GdkPixbuf *gdk_pixbuf__tga_load(FILE *f, GError **err)
{
	TGAHeader *hdr;
	TGAColormap *cmap;
	GdkPixbuf *pbuf;

	cmap = NULL;
	hdr = get_header_from_file(f, err);
	if (!hdr)
		return NULL;
	if ((hdr->flags & TGA_INTERLEAVE_MASK) != TGA_INTERLEAVE_NONE ||
	    hdr->flags & TGA_ORIGIN_RIGHT || hdr->flags & TGA_ORIGIN_LOWER) {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
			    _("Unsupported TGA image type"));
		g_free(hdr);
		return NULL;
	}
  
	if (hdr->has_cmap && ((hdr->type == TGA_TYPE_PSEUDOCOLOR) || 
			      (hdr->type == TGA_TYPE_RLE_PSEUDOCOLOR))) {
		cmap = get_colormap_from_file(f, hdr, err);
		if (!cmap) {
			g_free(hdr);
			return NULL;
		}
	}

	if (hdr->type == TGA_TYPE_PSEUDOCOLOR)
		pbuf = get_image_pseudocolor(f, hdr, cmap, FALSE, err);
	else if (hdr->type == TGA_TYPE_RLE_PSEUDOCOLOR)
		pbuf = get_image_pseudocolor(f, hdr, cmap, TRUE, err);
	else if (hdr->type == TGA_TYPE_TRUECOLOR)
		pbuf = get_image_truecolor(f, hdr, FALSE, err);
	else if (hdr->type == TGA_TYPE_RLE_TRUECOLOR)
		pbuf = get_image_truecolor(f, hdr, TRUE, err);
	else if (hdr->type == TGA_TYPE_GRAYSCALE)
		pbuf = get_image_grayscale(f, hdr, FALSE, err);
	else if (hdr->type == TGA_TYPE_RLE_GRAYSCALE)
		pbuf = get_image_grayscale(f, hdr, TRUE, err);
	else {
		g_set_error(err, GDK_PIXBUF_ERROR, 
			    GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
			    _("Unsupported TGA image type"));
		pbuf = NULL;
	}
  
	if (cmap) {
		g_free(cmap->cols);
		g_free(cmap);
	}
	g_free(hdr);
  
	return pbuf;
}

void
gdk_pixbuf__tga_fill_vtable (GdkPixbufModule *module)
{
	module->load = gdk_pixbuf__tga_load;
	module->begin_load = gdk_pixbuf__tga_begin_load;
	module->stop_load = gdk_pixbuf__tga_stop_load;
	module->load_increment = gdk_pixbuf__tga_load_increment;
}
