/*
 * GdkPixbuf library - PCX image loader
 *
 * Copyright (C) 2003 Josh A. Beam
 *
 * Authors: Josh A. Beam <josh@joshbeam.com>
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
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"

#undef PCX_DEBUG

#define PCX_TASK_DONE 0
#define PCX_TASK_LOAD_HEADER 1
#define PCX_TASK_LOAD_DATA 2
#define PCX_TASK_LOAD_PALETTE 3

struct pcx_header {
	guint8 manufacturer;
	guint8 version;
	guint8 encoding;
	guint8 bitsperpixel;
	gint16 xmin;
	gint16 ymin;
	gint16 xmax;
	gint16 ymax;
	guint16 horizdpi;
	guint16 vertdpi;
	guint8 palette[48];
	guint8 reserved;
	guint8 colorplanes;
	guint16 bytesperline;
	guint16 palettetype;
	guint16 hscrsize;
	guint16 vscrsize;
	guint8 filler[54];
};

struct pcx_context {
	GdkPixbuf *pixbuf;
	gint rowstride;

	GdkPixbufModuleSizeFunc size_func;
	GdkPixbufModuleUpdatedFunc updated_func;
	GdkPixbufModulePreparedFunc prepared_func;
	gpointer user_data;

	guchar current_task;

	gboolean header_loaded;
	struct pcx_header *header;
	guint bpp;
	gint width, height;
	guint num_planes;
	guint bytesperline;

	guchar *buf;
	guint buf_size;
	guint buf_pos;
	guchar *data;
	guchar *line;
	guint current_line;
	guchar *p_data;
};

/*
 * set context's image information based on the header
 */
static void
fill_pcx_context(struct pcx_context *context)
{
	struct pcx_header *header = context->header;

	context->bpp = header->bitsperpixel;
	context->width = header->xmax - header->xmin + 1;
	context->height = header->ymax - header->ymin + 1;
	context->num_planes = header->colorplanes;
	context->bytesperline = header->bytesperline;

	if(header->version == 5 && context->bpp == 8 && context->num_planes == 3)
		context->bpp = 24;
}

static void
free_pcx_context(struct pcx_context *context, gboolean unref_pixbuf)
{
	if(context->header)
		g_free(context->header);
	if(context->buf)
		g_free(context->buf);
	if(unref_pixbuf && context->pixbuf)
		g_object_unref(context->pixbuf);
	if(context->line)
		g_free(context->line);
	if(context->p_data)
		g_free(context->p_data);

	g_free(context);
}

/*
 * read each plane of a single scanline. store_planes is
 * the number of planes that can be stored in the planes array.
 * data is the pointer to the block of memory to read
 * from, size is the length of that data, and line_bytes
 * is where the number of bytes read will be stored.
 */
static gboolean
read_scanline_data(guchar *data, guint size, guchar *planes[],
                   guint store_planes, guint num_planes, guint bytesperline,
                   guint *line_bytes)
{
	guint i, j;
	guint p, count;
	guint d = 0;
	guint8 byte;

	for(p = 0; p < num_planes; p++) {
		for(i = 0; i < bytesperline;) { /* i incremented when line byte set */
			if(d >= size)
				return FALSE;
			byte = data[d++];

			if(byte >> 6 == 0x3) {
				count = byte & ~(0x3 << 6);
				if(count == 0)
					return FALSE;
				if(d >= size)
					return FALSE;
				byte = data[d++];
			} else {
				count = 1;
			}

			for(j = 0; j < count; j++) {
				if(p < store_planes)
					planes[p][i++] = byte;
				else
					i++;

				if(i >= bytesperline) {
					p++;
					if(p >= num_planes) {
						*line_bytes = d;
						return TRUE;
					}
					i = 0;
				}
			}
		}
	}

	*line_bytes = d; /* number of bytes read for scanline */
	return TRUE;
}

static gpointer
gdk_pixbuf__pcx_begin_load(GdkPixbufModuleSizeFunc size_func,
                           GdkPixbufModulePreparedFunc prepared_func,
                           GdkPixbufModuleUpdatedFunc updated_func,
                           gpointer user_data, GError **error)
{
	struct pcx_context *context;

	context = g_new0(struct pcx_context, 1);
	if(!context)
		return NULL;

	context->header = g_try_malloc(sizeof(struct pcx_header));
	if(!context->header) {
		g_free(context);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, _("Couldn't allocate memory for header"));
		return NULL;
	}

	context->size_func = size_func;
	context->updated_func = updated_func;
	context->prepared_func = prepared_func;
	context->user_data = user_data;

	context->current_task = PCX_TASK_LOAD_HEADER;

	context->buf = g_try_malloc(sizeof(guchar) * 512);
	if(!context->buf) {
		g_free(context->header);
		g_free(context);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, _("Couldn't allocate memory for context buffer"));
		return NULL;
	}
	context->buf_size = 512;

	context->header_loaded = FALSE;

	return context;
}

static gboolean
pcx_resize_context_buf(struct pcx_context *context, guint size)
{
	guchar *new_buf;

	new_buf = g_try_realloc(context->buf, size);
	if(!new_buf)
		return FALSE;

	context->buf = new_buf;
	context->buf_size = size;
	return TRUE;
}

/*
 * remove a number of bytes (specified by size) from the
 * beginning of a context's buf
 */
static gboolean
pcx_chop_context_buf(struct pcx_context *context, guint size)
{
	guint i, j;

	if(size > context->buf_pos)
		return FALSE;
	else if(size < 0)
		return FALSE;
	else if(size == 0)
		return TRUE;

	for(i = 0, j = size; j < context->buf_pos; i++, j++)
		context->buf[i] = context->buf[j];

	context->buf_pos -= size;

	return TRUE;
}

static guchar
read_pixel_1(guchar *data, guint offset)
{
	guchar retval;
	guint bit_offset;

	if(!(offset % 8)) {
		offset /= 8;
		retval = data[offset] >> 7;
	} else {
		bit_offset = offset % 8;
		offset -= bit_offset;
		offset /= 8;
		retval = (data[offset] >> (7 - bit_offset)) & 0x1;
	}

	return retval;
}

static guchar
read_pixel_4(guchar *data, guint offset)
{
	guchar retval;

	if(!(offset % 2)) {
		offset /= 2;
		retval = data[offset] >> 4;
	} else {
		offset--;
		offset /= 2;
		retval = data[offset] & 0xf;
	}

	return retval;
}

static gboolean
pcx_increment_load_data_1(struct pcx_context *context)
{
	guint i;
	guchar *planes[4];
	guint line_bytes;
	guint store_planes;

	if(context->num_planes == 4) {
		planes[0] = context->line;
		planes[1] = planes[0] + context->bytesperline;
		planes[2] = planes[1] + context->bytesperline;
		planes[3] = planes[2] + context->bytesperline;
		store_planes = 4;
	} else if(context->num_planes == 3) {
		planes[0] = context->line;
		planes[1] = planes[0] + context->bytesperline;
		planes[2] = planes[1] + context->bytesperline;
		store_planes = 3;
	} else if(context->num_planes == 2) {
		planes[0] = context->line;
		planes[1] = planes[0] + context->bytesperline;
		store_planes = 2;
	} else if(context->num_planes == 1) {
		planes[0] = context->line;
		store_planes = 1;
	} else {
		return FALSE;
	}

	while(read_scanline_data(context->buf, context->buf_pos, planes, store_planes, context->num_planes, context->bytesperline, &line_bytes)) {
		pcx_chop_context_buf(context, line_bytes);

		for(i = 0; i < context->width; i++) {
			guchar p;

			if(context->num_planes == 4) {
				p = read_pixel_1(planes[3], i);
				p <<= 1;
				p |= read_pixel_1(planes[2], i);
				p <<= 1;
				p |= read_pixel_1(planes[1], i);
				p <<= 1;
				p |= read_pixel_1(planes[0], i);
			} else if(context->num_planes == 3) {
				p = read_pixel_1(planes[2], i);
				p <<= 1;
				p |= read_pixel_1(planes[1], i);
				p <<= 1;
				p |= read_pixel_1(planes[0], i);
			} else if(context->num_planes == 2) {
				p = read_pixel_1(planes[1], i);
				p <<= 1;
				p |= read_pixel_1(planes[0], i);
			} else if(context->num_planes == 1) {
				p = read_pixel_1(planes[0], i);
			} else {
				return FALSE;
			}
			p &= 0xf;
			context->data[context->current_line * context->rowstride + i * 3 + 0] = context->header->palette[p * 3 + 0];
			context->data[context->current_line * context->rowstride + i * 3 + 1] = context->header->palette[p * 3 + 1];
			context->data[context->current_line * context->rowstride + i * 3 + 2] = context->header->palette[p * 3 + 2];
		}

		if(context->updated_func)
			context->updated_func(context->pixbuf, 0, context->current_line, context->width, 1, context->user_data);

		context->current_line++;

		if(context->current_line == context->height) {
			context->current_task = PCX_TASK_DONE;
			return TRUE;
		}
	}

	return TRUE;
}

static gboolean
pcx_increment_load_data_2(struct pcx_context *context)
{
	guint i;
	guchar *planes[1];
	guint line_bytes;
	guint shift, h;

	planes[0] = context->line;

	while(read_scanline_data(context->buf, context->buf_pos, planes, 1, context->num_planes, context->bytesperline, &line_bytes)) {
		pcx_chop_context_buf(context, line_bytes);

		for(i = 0; i < context->width; i++) {
			shift = 6 - 2 * (i % 4);
			h = (planes[0][i / 4] >> shift) & 0x3;
			context->data[context->current_line * context->rowstride + i * 3 + 0] = context->header->palette[h * 3 + 0];
			context->data[context->current_line * context->rowstride + i * 3 + 1] = context->header->palette[h * 3 + 1];
			context->data[context->current_line * context->rowstride + i * 3 + 2] = context->header->palette[h * 3 + 2];
		}

		if(context->updated_func)
			context->updated_func(context->pixbuf, 0, context->current_line, context->width, 1, context->user_data);

		context->current_line++;

		if(context->current_line == context->height) {
			context->current_task = PCX_TASK_DONE;
			return TRUE;
		}
	}

	return TRUE;
}

static gboolean
pcx_increment_load_data_4(struct pcx_context *context)
{
	guint i;
	guchar *planes[1];
	guint line_bytes;

	planes[0] = context->line;

	while(read_scanline_data(context->buf, context->buf_pos, planes, 1, context->num_planes, context->bytesperline, &line_bytes)) {
		pcx_chop_context_buf(context, line_bytes);

		for(i = 0; i < context->width; i++) {
			guchar p;

			p = read_pixel_4(planes[0], i) & 0xf;
			context->data[context->current_line * context->rowstride + i * 3 + 0] = context->header->palette[p * 3 + 0];
			context->data[context->current_line * context->rowstride + i * 3 + 1] = context->header->palette[p * 3 + 1];
			context->data[context->current_line * context->rowstride + i * 3 + 2] = context->header->palette[p * 3 + 2];
		}

		if(context->updated_func)
			context->updated_func(context->pixbuf, 0, context->current_line, context->width, 1, context->user_data);

		context->current_line++;

		if(context->current_line == context->height) {
			context->current_task = PCX_TASK_DONE;
			return TRUE;
		}
	}

	return TRUE;
}

/*
 * for loading 8-bit pcx images, we keep a buffer containing
 * each pixel's palette number; once we've loaded each scanline,
 * we wait for loading to stop and call pcx_load_palette_8,
 * which finds the palette at the end of the pcx data and sets the
 * RGB data.
 */
static gboolean
pcx_increment_load_data_8(struct pcx_context *context)
{
	guint i;
	guchar *planes[1];
	guint line_bytes;

	planes[0] = context->line;

	while(read_scanline_data(context->buf, context->buf_pos, planes, 1, context->num_planes, context->bytesperline, &line_bytes)) {
		pcx_chop_context_buf(context, line_bytes);

		for(i = 0; i < context->width; i++)
			context->p_data[context->current_line * context->width + i + 0] = planes[0][i];

		context->current_line++;

		if(context->current_line == context->height) {
			context->current_task = PCX_TASK_LOAD_PALETTE;
			return TRUE;
		}
	}

	return TRUE;
}

/*
 * read the palette and set the RGB data
 */
static gboolean
pcx_load_palette_8(struct pcx_context *context)
{
	guint i, j;

	if(context->current_line < context->height)
		return FALSE;

	if(context->buf_pos >= 769) {
		guchar *palette = context->buf + (context->buf_pos - 769);

		if(palette[0] == 12) {
			palette++;
			for(i = 0; i < context->height; i++) {
				for(j = 0; j < context->width; j++) {
					context->data[i * context->rowstride + j * 3 + 0] = palette[(context->p_data[i * context->width + j]) * 3 + 0];
					context->data[i * context->rowstride + j * 3 + 1] = palette[(context->p_data[i * context->width + j]) * 3 + 1];
					context->data[i * context->rowstride + j * 3 + 2] = palette[(context->p_data[i * context->width + j]) * 3 + 2];
				}

				if(context->updated_func)
					context->updated_func(context->pixbuf, 0, i, context->width, 1, context->user_data);
			}

#ifdef PCX_DEBUG
			g_print("read palette\n");
#endif

			context->current_task = PCX_TASK_DONE;
			return TRUE;
		} else {
#ifdef PCX_DEBUG
			g_print("this ain't a palette\n");
#endif
			return FALSE;
		}
	}

	return FALSE;
}

/*
 * in 24-bit images, each scanline has three color planes
 * for red, green, and blue, respectively.
 */
static gboolean
pcx_increment_load_data_24(struct pcx_context *context)
{
	guint i;
	guchar *planes[3];
	guint line_bytes;

	planes[0] = context->line;
	planes[1] = planes[0] + context->bytesperline;
	planes[2] = planes[1] + context->bytesperline;

	while(read_scanline_data(context->buf, context->buf_pos, planes, 3, context->num_planes, context->bytesperline, &line_bytes)) {
		pcx_chop_context_buf(context, line_bytes);

		for(i = 0; i < context->width; i++) {
			context->data[context->current_line * context->rowstride + i * 3 + 0] = planes[0][i];
			context->data[context->current_line * context->rowstride + i * 3 + 1] = planes[1][i];
			context->data[context->current_line * context->rowstride + i * 3 + 2] = planes[2][i];
		}

		if(context->updated_func)
			context->updated_func(context->pixbuf, 0, context->current_line, context->width, 1, context->user_data);

		context->current_line++;

		if(context->current_line == context->height) {
			context->current_task = PCX_TASK_DONE;
			return TRUE;
		}
	}

	return TRUE;
}

static gboolean
gdk_pixbuf__pcx_load_increment(gpointer data, const guchar *buf, guint size,
                               GError **error)
{
	struct pcx_context *context = (struct pcx_context *)data;
	struct pcx_header *header;
	guint i;
	gboolean retval = TRUE;

	/* if context's buf isn't large enough to hold its current data plus the passed buf, increase its size */
	if(context->buf_pos + size > context->buf_size) {
		if(!pcx_resize_context_buf(context, sizeof(guchar) * (context->buf_pos + size))) {
			g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, _("Couldn't allocate memory for context buffer"));
			return FALSE;
		}
	}

	for(i = 0; i < size; i++)
		context->buf[context->buf_pos++] = buf[i];

	if(context->current_task == PCX_TASK_LOAD_HEADER) {
		if(!context->header_loaded && context->buf_pos > sizeof(struct pcx_header)) { /* set header */
			gint width, height;

			memcpy(context->header, context->buf, sizeof(struct pcx_header));
			pcx_chop_context_buf(context, sizeof(struct pcx_header));
			header = context->header;

			/* convert the multi-byte header variables that will be used */
			header->xmin = GINT16_FROM_LE(header->xmin);
			header->ymin = GINT16_FROM_LE(header->ymin);
			header->xmax = GINT16_FROM_LE(header->xmax);
			header->ymax = GINT16_FROM_LE(header->ymax);
			header->bytesperline = GUINT16_FROM_LE(header->bytesperline);

#ifdef PCX_DEBUG
			g_print ("Manufacturer %d\n"
			         "Version %d\n"
			         "Encoding %d\n"
				 "Bits/Pixel %d\n"
				 "Planes %d\n"
				 "Palette %d\n", 
				 header->manufacturer, header->version, 
				 header->encoding, header->bitsperpixel,
				 header->colorplanes, header->palettetype);
#endif

			context->header_loaded = TRUE;
			fill_pcx_context(context);

			width = context->width;
			height = context->height;
			if(width <= 0 || height <= 0) {
				g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE, _("Image has invalid width and/or height"));
				return FALSE;
			}
			if (context->size_func)
			  {
			    (*context->size_func) (&width, &height, context->user_data);
			    if (width == 0 || height == 0)
			      return TRUE;
			  }

			switch(context->bpp) {
				default:
					g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNKNOWN_TYPE, _("Image has unsupported bpp"));
					return FALSE;
					break;
				case 1:
					if(context->num_planes < 1 || context->num_planes > 4) {
						g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNKNOWN_TYPE, _("Image has unsupported number of %d-bit planes"), 1);
						return FALSE;
					}
					break;
				case 2:
				case 4:
				case 8:
					if(context->num_planes != 1) {
						g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNKNOWN_TYPE, _("Image has unsupported number of %d-bit planes"), context->bpp);
						return FALSE;
					}
					break;
				case 24:
					break; /* context's bpp is set to 24 if there are three 8-bit planes */
			}

#ifdef PCX_DEBUG
			g_print("io-pcx: header loaded\n");
			g_print("bpp: %u\n", context->bpp);
			g_print("dimensions: %ux%u\n", context->width, context->height);
#endif

			context->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, context->width, context->height);
			if(!context->pixbuf) {
				g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, _("Couldn't create new pixbuf"));
				return FALSE;
			}
			context->data = gdk_pixbuf_get_pixels(context->pixbuf);
			context->rowstride = gdk_pixbuf_get_rowstride(context->pixbuf);

			context->line = g_try_malloc(sizeof(guchar) * context->bytesperline * context->num_planes);
			if(!context->line) {
				g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, _("Couldn't allocate memory for line data"));
				return FALSE;
			}

			if(context->bpp == 8) {
				context->p_data = g_try_malloc(sizeof(guchar) * context->width * context->height);
				if(!context->p_data) {
					g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, _("Couldn't allocate memory for paletted data"));
					return FALSE;
				}
			}

			if(context->prepared_func)
			        context->prepared_func(context->pixbuf, NULL, context->user_data);

			context->current_task = PCX_TASK_LOAD_DATA;
		}

		retval = TRUE;
	}

	if(context->current_task == PCX_TASK_LOAD_DATA) {
		switch(context->bpp) {
			default:
				g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNKNOWN_TYPE, _("Image has unsupported bpp"));
				retval = FALSE;
				break;
			case 1:
				retval = pcx_increment_load_data_1(context);
				break;
			case 2:
				retval = pcx_increment_load_data_2(context);
				break;
			case 4:
				retval = pcx_increment_load_data_4(context);
				break;
			case 8:
				retval = pcx_increment_load_data_8(context);
				break;
			case 24:
				retval = pcx_increment_load_data_24(context);
				break;
		}
	}

	return retval;
}

static gboolean
gdk_pixbuf__pcx_stop_load(gpointer data, GError **error)
{
	struct pcx_context *context = (struct pcx_context *)data;

	if(context->current_line != context->height) {
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, _("Didn't get all lines of PCX image"));
		free_pcx_context(context, FALSE);
		return FALSE;
	}

	if(context->current_task == PCX_TASK_LOAD_PALETTE) {
		if(!pcx_load_palette_8(context)) {
			g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, _("No palette found at end of PCX data"));
			free_pcx_context(context, FALSE);
			return FALSE;
		}
	}

	free_pcx_context(context, FALSE);

	return TRUE;
}

void
MODULE_ENTRY (pcx, fill_vtable) (GdkPixbufModule *module)
{
	module->begin_load = gdk_pixbuf__pcx_begin_load;
	module->stop_load = gdk_pixbuf__pcx_stop_load;
	module->load_increment = gdk_pixbuf__pcx_load_increment;
}

void
MODULE_ENTRY (pcx, fill_info) (GdkPixbufFormat *info)
{
	static GdkPixbufModulePattern signature[] = {
		{ "\x0a \x01", NULL, 100 },
		{ "\x0a\x02\x01", NULL, 100 },
		{ "\x0a\x03\x01", NULL, 100 },
		{ "\x0a\x04\x01", NULL, 100 },
		{ "\x0a\x05\x01", NULL, 100 },
		{ NULL, NULL, 0 }
	};
	static gchar *mime_types[] = {
		"image/x-pcx",
		NULL,
	};
	static gchar *extensions[] = {
		"pcx",
		NULL,
	};

	info->name = "pcx";
	info->signature = signature;
	info->description = N_("The PCX image format");
	info->mime_types = mime_types;
	info->extensions = extensions;
	info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = "LGPL";
}
