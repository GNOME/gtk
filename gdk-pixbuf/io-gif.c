/* GdkPixbuf library - GIF image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Jonathan Blandford <jrb@redhat.com>
 *          Adapted from the gimp gif filter written by Adam Moss <adam@gimp.org>
 *          Gimp work based on earlier work by ......
 *          Permission to relicense under the LGPL obtained.
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
#include <glib.h>
#include <string.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



#define MAXCOLORMAPSIZE  256

#define CM_RED           0
#define CM_GREEN         1
#define CM_BLUE          2

#define MAX_LWZ_BITS     12

#define INTERLACE          0x40
#define LOCALCOLORMAP      0x80
#define BitSet(byte, bit)  (((byte) & (bit)) == (bit))
#define LM_to_uint(a,b)         (((b)<<8)|(a))

#define GRAYSCALE        1
#define COLOR            2

typedef unsigned char CMap[3][MAXCOLORMAPSIZE];

/* Possible states we can be in. */
enum {
	GIF_START = 1,
	GIF_GET_COLORMAP = 2,
	GIF_GET_NEXT_STEP = 3,
	GIF_GET_EXTENTION = 4,
	GIF_GET_COLORMAP2 = 5,
	GIF_PREPARE_LZW = 6,
	GIF_GET_LZW = 7,
	GIF_DONE = 8
};


typedef struct _Gif89 Gif89;
struct _Gif89
{
	int transparent;
	int delay_time;
	int input_flag;
	int disposal;
};

typedef struct _GifContext GifContext;
struct _GifContext
{
	int state; /* really only relevant for progressive loading */
	unsigned int width;
	unsigned int height;
	CMap color_map;
	unsigned int bit_pixel;
	unsigned int color_resolution;
	unsigned int background;
	unsigned int aspect_ratio;
	int gray_scale;
	GdkPixbuf *pixbuf;
	Gif89 gif89;

	/* stuff per frame.  As we only support the first one, not so
	 * relevant.  But still needed */
	int frame_len;
	int frame_height;
	int frame_interlace;

	/* Static read only */
	FILE *file;

	/* progressive read, only. */
	ModulePreparedNotifyFunc func;
	gpointer user_data;
	guchar *buf;
	guint ptr;
	guint size;
	guint amount_needed;

	/* our colormap context */
	gint colormap_index;
	gint colormap_flag;

	/* our extension context */
	guchar extension_label;
	guchar extension_flag;

	/* get block context */
	guchar block_count;
	guchar block_buf[256];
	gint block_ptr;

	/* our lwz context */
	gint lwz_fresh;
	gint lwz_code_size;
	guchar lwz_set_code_size;
	gint lwz_max_code;
	gint lwz_max_code_size;
	gint lwz_firstcode;
	gint lwz_oldcode;
	gint lwz_clear_code;
	gint lwz_end_code;
	gint lwz_table[2][(1 << MAX_LWZ_BITS)];
	gint lwz_stack[(1 << (MAX_LWZ_BITS)) * 2];
	gint *lwz_sp;
};

static int GetDataBlock (GifContext *, unsigned char *);
static int GetCode (GifContext *, int, int);
static int LWZReadByte (GifContext *);

static int
ReadOK (GifContext *context, guchar *buffer, size_t len)
{
	static int count = 0;
	count += len;
	g_print ("size :%d\tcount :%d\n", len, count);
	if (context->file)
		return fread(buffer, len, 1, context->file) != 0;
	else {
		if ((context->size - context->ptr) < len) {
			memcpy (buffer, context->buf + context->ptr, len);
			context->ptr += len;
			context->amount_needed = 0;
			return 0;
		}
		context->amount_needed = len - (context->size - context->ptr);
	}
	return 1;
}


/*
 * Return vals.
 *
 * for the gif_get* functions,
 *-1 -> more bytes needed.
 * 0 -> success
 * 1 -> failure; abort the load
 */



/* Changes the stage to be GIF_GET_COLORMAP */
static void
gif_set_get_colormap (GifContext *context)
{
	context->colormap_flag = TRUE;
	context->colormap_index = 0;
	context->state = GIF_GET_COLORMAP;
}

static void
gif_set_get_colormap2 (GifContext *context)
{
	context->colormap_flag = TRUE;
	context->colormap_index = 0;
	context->state = GIF_GET_COLORMAP2;
}

static gint
gif_get_colormap (GifContext *context)
{
	unsigned char rgb[3];

	while (context->colormap_index < context->bit_pixel) {
		if (!ReadOK (context, rgb, sizeof (rgb))) {
			/*g_message (_("GIF: bad colormap\n"));*/
			return -1;
		}

		context->color_map[CM_RED][context->colormap_index] = rgb[0];
		context->color_map[CM_GREEN][context->colormap_index] = rgb[1];
		context->color_map[CM_BLUE][context->colormap_index] = rgb[2];

		context->colormap_flag &= (rgb[0] == rgb[1] && rgb[1] == rgb[2]);
		context->colormap_index ++;
	}

	context->gray_scale = (context->colormap_flag) ? GRAYSCALE : COLOR;

	return 0;
}

/*
 * in order for this function to work, we need to perform some black magic.
 * We want to return -1 to let the calling function know, as before, that it needs
 * more bytes.  If we return 0, we were able to successfully read all block->count bytes.
 * Problem is, we don't want to reread block_count every time, so we check to see if
 * context->block_count is 0 before we read in the function.
 *
 * As a result, context->block_count MUST be 0 the first time the get_data_block is called
 * within a context.
 */

static int
get_data_block (GifContext *context,
		unsigned char *buf,
		gint *empty_block)
{

	if (context->block_count == 0) {
		if (!ReadOK (context, &context->block_count, 1)) {
			return -1;
		}
	}

	if (context->block_count == 0)
		if (empty_block)
			*empty_block = TRUE;

	return 0;

	if (!ReadOK (context, buf, context->block_count)) {
		return -1;
	}

	return 0;
}

static void
gif_set_get_extension (GifContext *context)
{
	g_print ("getting the extension woooooo\n");
	context->state = GIF_GET_EXTENTION;
	context->extension_flag = TRUE;
	context->extension_label = '\000';
	context->block_count = 0;
	context->block_ptr = 0;
}

static int
gif_get_extension (GifContext *context)
{
	gint retval;
	gint empty_block = FALSE;

	g_print ("in gif_get_extension\n");
	if (context->extension_flag) {
		if (!context->extension_label) {
			if (!ReadOK (context, &context->extension_label , 1)) {
				return -1;
			}
		}

		switch (context->extension_label) {
		case 0xf9:			/* Graphic Control Extension */
			retval = get_data_block (context, (unsigned char *) context->block_buf, NULL);
			if (retval != 0)
				return retval;
			context->gif89.disposal = (context->block_buf[0] >> 2) & 0x7;
			context->gif89.input_flag = (context->block_buf[0] >> 1) & 0x1;
			context->gif89.delay_time = LM_to_uint (context->block_buf[1], context->block_buf[2]);
			if (context->pixbuf == NULL) {
				/* I only want to set the transparency if I haven't
				 * created the pixbuf yet. */
				if ((context->block_buf[0] & 0x1) != 0)
					context->gif89.transparent = context->block_buf[3];
				else
					context->gif89.transparent = -1;
			}
			
			/* Now we've successfully loaded this one, we continue on our way */
			context->extension_flag = FALSE;
		default:
			/* Unhandled extension */
			break;
		}
	}
	/* read all blocks, until I get an empty block */
	/* not sure why, but it makes it work.  -jrb */
	do {
		retval = get_data_block (context, (unsigned char *) context->block_buf, &empty_block);
		if (retval != 0)
			return retval;
	} while (!empty_block);

	return 0;
}

static int ZeroDataBlock = FALSE;

static int
GetDataBlock (GifContext *context,
	      unsigned char *buf)
{
//	unsigned char count;

	if (!ReadOK (context, &context->block_count, 1)) {
		/*g_message (_("GIF: error in getting DataBlock size\n"));*/
		return -1;
	}

	ZeroDataBlock = context->block_count == 0;

	if ((context->block_count != 0) && (!ReadOK (context, buf, context->block_count))) {
		/*g_message (_("GIF: error in reading DataBlock\n"));*/
		return -1;
	}

	return context->block_count;
}


static int
GetCode (GifContext *context,
	 int   code_size,
	 int   flag)
{
	static unsigned char buf[280];
	static int curbit, lastbit, done, last_byte;
	int i, j, ret;
	unsigned char count;

	if (flag) {
		curbit = 0;
		lastbit = 0;
		done = FALSE;
		return 0;
	}

	if ((curbit + code_size) >= lastbit){
		if (done) {
			if (curbit >= lastbit) {
				/*g_message (_("GIF: ran off the end of by bits\n"));*/
				return -1;
			}
			return -1;
		}
		buf[0] = buf[last_byte - 2];
		buf[1] = buf[last_byte - 1];

		if ((count = GetDataBlock (context, &buf[2])) == 0)
			done = TRUE;

		last_byte = 2 + count;
		curbit = (curbit - lastbit) + 16;
		lastbit = (2 + count) * 8;
	}

	ret = 0;
	for (i = curbit, j = 0; j < code_size; ++i, ++j)
		ret |= ((buf[i / 8] & (1 << (i % 8))) != 0) << j;

	curbit += code_size;

	return ret;
}

static int
LWZReadByte (GifContext *context)
{
	int code, incode;
	register int i;

	if (context->lwz_fresh) {
		context->lwz_fresh = FALSE;
		do {
			context->lwz_firstcode = context->lwz_oldcode = GetCode (context, context->lwz_code_size, FALSE);
		} while (context->lwz_firstcode == context->lwz_clear_code);
		return context->lwz_firstcode;
	}

	if (context->lwz_sp > context->lwz_stack)
		return *--(context->lwz_sp);

	while ((code = GetCode (context, context->lwz_code_size, FALSE)) >= 0) {
		if (code == context->lwz_clear_code) {
			for (i = 0; i < context->lwz_clear_code; ++i) {
				context->lwz_table[0][i] = 0;
				context->lwz_table[1][i] = i;
			}
			for (; i < (1 << MAX_LWZ_BITS); ++i)
				context->lwz_table[0][i] = context->lwz_table[1][i] = 0;
			context->lwz_code_size = context->lwz_set_code_size + 1;
			context->lwz_max_code_size = 2 * context->lwz_clear_code;
			context->lwz_max_code = context->lwz_clear_code + 2;
			context->lwz_sp = context->lwz_stack;
			context->lwz_firstcode = context->lwz_oldcode =
				GetCode (context, context->lwz_code_size, FALSE);
			return context->lwz_firstcode;
		} else if (code == context->lwz_end_code) {
			int count;
			unsigned char buf[260];

			if (ZeroDataBlock)
				return -2;

			while ((count = GetDataBlock (context, buf)) > 0)
				;

			if (count != 0)
				/*g_print (_("GIF: missing EOD in data stream (common occurence)"));*/
			return -2;
		}

		incode = code;

		if (code >= context->lwz_max_code) {
			*(context->lwz_sp)++ = context->lwz_firstcode;
			code = context->lwz_oldcode;
		}

		while (code >= context->lwz_clear_code) {
			*(context->lwz_sp)++ = context->lwz_table[1][code];
			if (code == context->lwz_table[0][code]) {
				/*g_message (_("GIF: circular table entry BIG ERROR\n"));*/
				/*gimp_quit ();*/
				return -1;
			}
			code = context->lwz_table[0][code];
		}

		*(context->lwz_sp)++ = context->lwz_firstcode = context->lwz_table[1][code];

		if ((code = context->lwz_max_code) < (1 << MAX_LWZ_BITS)) {
			context->lwz_table[0][code] = context->lwz_oldcode;
			context->lwz_table[1][code] = context->lwz_firstcode;
			++context->lwz_max_code;
			if ((context->lwz_max_code >= context->lwz_max_code_size) &&
			    (context->lwz_max_code_size < (1 << MAX_LWZ_BITS))) {
				context->lwz_max_code_size *= 2;
				++context->lwz_code_size;
			}
		}

		context->lwz_oldcode = incode;

		if (context->lwz_sp > context->lwz_stack)
			return *--(context->lwz_sp);
	}
	return code;
}

static void
gif_get_lzw (GifContext *context)
{
	guchar *dest, *temp;
	gint xpos = 0, ypos = 0, pass = 0;
	gint v;

	context->pixbuf = gdk_pixbuf_new (ART_PIX_RGB,
					  context->gif89.transparent,
					  8,
					  context->width,
					  context->height);

	dest = gdk_pixbuf_get_pixels (context->pixbuf);
	while ((v = LWZReadByte ( context)) >= 0) {
		if (context->gif89.transparent) {
			temp = dest + ypos * gdk_pixbuf_get_rowstride (context->pixbuf) + xpos * 4;
			*temp = context->color_map [0][(guchar) v];
			*(temp+1) = context->color_map [1][(guchar) v];
			*(temp+2) = context->color_map [2][(guchar) v];
			*(temp+3) = (guchar) ((v == context->gif89.transparent) ? 0 : 65535);
		} else {
			temp = dest + ypos * gdk_pixbuf_get_rowstride (context->pixbuf) + xpos * 3;
			*temp = context->color_map [0][(guchar) v];
			*(temp+1) = context->color_map [1][(guchar) v];
			*(temp+2) = context->color_map [2][(guchar) v];
		}

		xpos++;
		if (xpos == context->frame_len) {
			xpos = 0;
			if (context->frame_interlace) {
				switch (pass) {
				case 0:
				case 1:
					ypos += 8;
					break;
				case 2:
					ypos += 4;
					break;
				case 3:
					ypos += 2;
					break;
				}

				if (ypos >= context->frame_height) {
					pass++;
					switch (pass) {
					case 1:
						ypos = 4;
						break;
					case 2:
						ypos = 2;
						break;
					case 3:
						ypos = 1;
						break;
					default:
						goto done;
					}
				}
			} else {
				ypos++;
			}
		}
		if (ypos >= context->frame_height)
			break;
	}

 done:
	/* we got enough data. there may be more (ie, newer layers) but we can quit now */
	context->state = GIF_DONE;
}

static int
gif_prepare_lzw (GifContext *context)
{
	gint i;

	if (!ReadOK (context, &(context->lwz_set_code_size), 1)) {
		/*g_message (_("GIF: EOF / read error on image data\n"));*/
		return -1;
	}

	context->lwz_code_size = context->lwz_set_code_size + 1;
	context->lwz_clear_code = 1 << context->lwz_set_code_size;
	context->lwz_end_code = context->lwz_clear_code + 1;
	context->lwz_max_code_size = 2 * context->lwz_clear_code;
	context->lwz_max_code = context->lwz_clear_code + 2;

	GetCode (context, 0, TRUE);

	context->lwz_fresh = TRUE;

	for (i = 0; i < context->lwz_clear_code; ++i) {
		context->lwz_table[0][i] = 0;
		context->lwz_table[1][i] = i;
	}
	for (; i < (1 << MAX_LWZ_BITS); ++i)
		context->lwz_table[0][i] = context->lwz_table[1][0] = 0;

	context->lwz_sp = context->lwz_stack;
	return 0;
}

/* needs 13 bytes to proceed. */
static gint
gif_init (GifContext *context)
{
	unsigned char buf[16];
	char version[4];

	if (!ReadOK (context, buf, 6)) {
		/* Unable to read magic number */
		return -1;
	}

	if (strncmp ((char *) buf, "GIF", 3) != 0) {
		/* Not a GIF file */
		return -1;
	}

	strncpy (version, (char *) buf + 3, 3);
	version[3] = '\0';

	if ((strcmp (version, "87a") != 0) && (strcmp (version, "89a") != 0)) {
		/* bad version number, not '87a' or '89a' */
		return -1;
	}

	/* read the screen descriptor */
	if (!ReadOK (context, buf, 7)) {
		/* Failed to read screen descriptor */
		return -1;
	}

	context->width = LM_to_uint (buf[0], buf[1]);
	context->height = LM_to_uint (buf[2], buf[3]);
	context->bit_pixel = 2 << (buf[4] & 0x07);
	context->color_resolution = (((buf[4] & 0x70) >> 3) + 1);
	context->background = buf[5];
	context->aspect_ratio = buf[6];
	context->pixbuf = NULL;

	if (BitSet (buf[4], LOCALCOLORMAP)) {
		gif_set_get_colormap (context);
	} else {
		context->state = GIF_GET_NEXT_STEP;
	}
	return 0;
}


static gint
gif_get_next_step (GifContext *context)
{
	unsigned char buf[16];
	unsigned char c;
	for (;;) {
		if (!ReadOK (context, &c, 1)) {
			return -1;
		}
		g_print ("c== %c\n", c);
		if (c == ';') {
			/* GIF terminator */
			/* hmm.  Not 100% sure what to do about this.  Should
			 * i try to return a blank image instead? */
			context->state = GIF_DONE;
			return 1;
		}

		if (c == '!') {
			/* Check the extention */
			gif_set_get_extension (context);
			continue;
		}

		/* look for frame */
		if (c != ',') {
			/* Not a valid start character */
			continue;
		}

		if (!ReadOK (context, buf, 9)) {
			return -1;
		}

		/* Okay, we got all the info we need.  Lets record it */
		context->frame_len = LM_to_uint (buf[4], buf[5]);
		context->frame_height = LM_to_uint (buf[6], buf[7]);
		if (context->frame_height > context->height) {
			context->state = GIF_DONE;
			return 1;
		}

		context->frame_interlace = BitSet (buf[8], INTERLACE);
		if (BitSet (buf[8], LOCALCOLORMAP)) {
			/* Does this frame have it's own colormap. */
			/* really only relevant when looking at the first frame
			 * of an animated gif. */
			/* if it does, we need to re-read in the colormap,
			 * the gray_scale, and the bit_pixel */
			context->bit_pixel = 1 << ((buf[8] & 0x07) + 1);
			gif_set_get_colormap2 (context);
			return 0;
		}
		context->state = GIF_PREPARE_LZW;
		return 0;
	}
	g_assert_not_reached ();
	return 1;
}


static gint
gif_main_loop (GifContext *context)
{
	gint retval = 0;

	do {
		switch (context->state) {
		case GIF_START:
			g_print ("in GIF_START\n");
			retval = gif_init (context);
			break;

		case GIF_GET_COLORMAP:
			g_print ("in GIF_GET_COLORMAP\n");
			retval = gif_get_colormap (context);
			if (retval == 0)
				context->state = GIF_GET_NEXT_STEP;
			break;

		case GIF_GET_NEXT_STEP:
			g_print ("in GIF_GET_NEXT_STEP\n");
			retval = gif_get_next_step (context);
			break;

		case GIF_GET_EXTENTION:
			g_print ("in GIF_GET_EXTENTION\n");
			retval = gif_get_extension (context);
			if (retval == 0)
				context->state = GIF_GET_NEXT_STEP;
			break;

		case GIF_GET_COLORMAP2:
			g_print ("in GIF_GET_COLORMAP2\n");
			retval = gif_get_colormap (context);
			if (retval == 0)
				context->state = GIF_PREPARE_LZW;
			break;

		case GIF_PREPARE_LZW:
			g_print ("in GIF_PREPARE_LZW\n");
			retval = gif_prepare_lzw (context);
			if (retval == 0)
				context->state = GIF_GET_LZW;
			break;

		case GIF_GET_LZW:
			g_print ("in GIF_GET_LZW\n");
			gif_get_lzw (context);
			retval = 0;
			break;

		case GIF_DONE:
		default:
			g_print ("in GIF_DONE\n");
			retval = 0;
			goto done;
		};
	} while (retval == 0);
 done:
	return retval;
}

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *file)
{
	GifContext *context;

	g_return_val_if_fail (file != NULL, NULL);

	context = g_new (GifContext, 1);
	context->lwz_fresh = FALSE;
	context->file = file;
	context->pixbuf = NULL;
	context->state = GIF_START;

	gif_main_loop (context);

	return context->pixbuf;
}

gpointer
image_begin_load (ModulePreparedNotifyFunc func, gpointer user_data)
{
	GifContext *context;

	context = g_new (GifContext, 1);
	context->func = func;
	context->user_data = user_data;
	context->lwz_fresh = FALSE;
	context->file = NULL;
	context->pixbuf = NULL;
	context->state = GIF_START;

	return (gpointer) context;
}

void
image_stop_load (gpointer data)
{
	GifContext *context = (GifContext *) data;

	if (context->pixbuf)
		gdk_pixbuf_unref (context->pixbuf);
	g_free (context);
}

gboolean
image_load_increment (gpointer data, guchar *buf, guint size)
{
	gint retval;
	GifContext *context = (GifContext *) data;

	return FALSE;
	if (context->amount_needed == 0) {
		/* we aren't looking for some bytes. */
		context->buf = buf;
		context->ptr = 0;
		context->size = size;
	} 
	retval = gif_main_loop (context);
	if (retval == 1)
		return FALSE;
	if (retval == -1) {
	}
	return TRUE;
}
