/* GdkPixbuf library - GIF image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Jonathan Blandford <jrb@redhat.com>
 *          Adapted from the gimp gif filter written by Adam Moss <adam@gimp.org>
 *          Gimp work based on earlier work.
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
#define MAX_LZW_BITS     12

#define INTERLACE          0x40
#define LOCALCOLORMAP      0x80
#define BitSet(byte, bit)  (((byte) & (bit)) == (bit))
#define LM_to_uint(a,b)         (((b)<<8)|(a))

typedef unsigned char CMap[3][MAXCOLORMAPSIZE];

/* Possible states we can be in. */
enum {
	GIF_START,
	GIF_GET_COLORMAP,
	GIF_GET_NEXT_STEP,
	GIF_GET_FRAME_INFO,
	GIF_GET_EXTENTION,
	GIF_GET_COLORMAP2,
	GIF_PREPARE_LZW,
	GIF_LZW_FILL_BUFFER,
	GIF_LZW_CLEAR_CODE,
	GIF_GET_LZW,
	GIF_DONE,
};


typedef struct _Gif89 Gif89;
struct _Gif89
{
	int transparent;
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

	/* colormap context */
	gint colormap_index;
	gint colormap_flag;

	/* extension context */
	guchar extension_label;
	guchar extension_flag;

	/* get block context */
	guchar block_count;
	guchar block_buf[280];
	gint block_ptr;

	int old_state; /* used by lzw_fill buffer */
	/* get_code context */
	int code_curbit;
	int code_lastbit;
	int code_done;
	int code_last_byte;
	int lzw_code_pending;

	/* lzw context */
	gint lzw_fresh;
	gint lzw_code_size;
	guchar lzw_set_code_size;
	gint lzw_max_code;
	gint lzw_max_code_size;
	gint lzw_firstcode;
	gint lzw_oldcode;
	gint lzw_clear_code;
	gint lzw_end_code;
	gint *lzw_sp;

	gint lzw_table[2][(1 << MAX_LZW_BITS)];
	gint lzw_stack[(1 << (MAX_LZW_BITS)) * 2 + 1];

	/* painting context */
	gint draw_xpos;
	gint draw_ypos;
	gint draw_pass;
};

static int GetDataBlock (GifContext *, unsigned char *);

#ifdef IO_GIFDEBUG
static int count = 0;
#endif

/* Returns TRUE if Read is OK,
 * FALSE if more memory is needed. */
static int
ReadOK (GifContext *context, guchar *buffer, size_t len)
{
	gint retval;
#ifdef IO_GIFDEBUG
	gint i;
#endif
	if (context->file) {
#ifdef IO_GIFDEBUG
		count += len;
		g_print ("Fsize :%d\tcount :%d\t", len, count);
#endif
		retval = (fread(buffer, len, 1, context->file) != 0);
#ifdef IO_GIFDEBUG
		if (len < 100) {
			for (i = 0; i < len; i++)
				g_print ("%d ", buffer[i]);
		}
		g_print ("\n");
#endif
		return retval;
	} else {
#ifdef IO_GIFDEBUG
//		g_print ("\tlooking for %d bytes.  size == %d, ptr == %d\n", len, context->size, context->ptr);
#endif
		if ((context->size - context->ptr) >= len) {
#ifdef IO_GIFDEBUG
			count += len;
#endif
			memcpy (buffer, context->buf + context->ptr, len);
			context->ptr += len;
			context->amount_needed = 0;
#ifdef IO_GIFDEBUG
			g_print ("Psize :%d\tcount :%d\t", len, count);
			if (len < 100) {
				for (i = 0; i < len; i++)
					g_print ("%d ", buffer[i]);
			}
			g_print ("\n");
#endif
			return TRUE;
		}
		context->amount_needed = len - (context->size - context->ptr);
	}
	return 0;
}


/*
 * Return vals.
 * Unless otherwise specified, these are the return vals for most functions:
 *
 *  0 -> success
 * -1 -> more bytes needed.
 * -2 -> failure; abort the load
 * -3 -> control needs to be passed back to the main loop
 *        \_ (most of the time returning 0 will get this, but not always)
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

		context->color_map[0][context->colormap_index] = rgb[0];
		context->color_map[1][context->colormap_index] = rgb[1];
		context->color_map[2][context->colormap_index] = rgb[2];

		context->colormap_flag &= (rgb[0] == rgb[1] && rgb[1] == rgb[2]);
		context->colormap_index ++;
	}

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
 * within a context, and cannot be 0 the second time it's called.
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
		if (empty_block) {
			*empty_block = TRUE;
			return 0;
		}

	if (!ReadOK (context, buf, context->block_count)) {
		return -1;
	}

	return 0;
}

static void
gif_set_get_extension (GifContext *context)
{
	context->state = GIF_GET_EXTENTION;
	context->extension_flag = TRUE;
	context->extension_label = 0;
	context->block_count = 0;
	context->block_ptr = 0;
}

static int
gif_get_extension (GifContext *context)
{
	gint retval;
	gint empty_block = FALSE;

	if (context->extension_flag) {
		if (context->extension_label == 0) {
			/* I guess bad things can happen if we have an extension of 0 )-: */
			/* I should look into this sometime */
			if (!ReadOK (context, & context->extension_label , 1)) {
				return -1;
			}
		}

		switch (context->extension_label) {
		case 0xf9:			/* Graphic Control Extension */
			retval = get_data_block (context, (unsigned char *) context->block_buf, NULL);
			if (retval != 0)
				return retval;
			if (context->pixbuf == NULL) {
				/* I only want to set the transparency if I haven't
				 * created the pixbuf yet. */
				if ((context->block_buf[0] & 0x1) != 0) {
					context->gif89.transparent = context->block_buf[3];
				} else {
					context->gif89.transparent = FALSE;
				}
			}

			/* Now we've successfully loaded this one, we continue on our way */
			context->block_count = 0;
			context->extension_flag = FALSE;
		default:
			/* Unhandled extension */
			break;
		}
	}
	/* read all blocks, until I get an empty block, in case there was an extension I didn't know about. */
	do {
		retval = get_data_block (context, (unsigned char *) context->block_buf, &empty_block);
		if (retval != 0)
			return retval;
		context->block_count = 0;
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


static void
gif_set_lzw_fill_buffer (GifContext *context)
{
	context->block_count = 0;
	context->old_state = context->state;
	context->state = GIF_LZW_FILL_BUFFER;
}

static int
gif_lzw_fill_buffer (GifContext *context)
{
	gint retval;

	if (context->code_done) {
		if (context->code_curbit >= context->code_lastbit) {
				g_message ("GIF: ran off the end of by bits\n");
			return -2;
		}
		g_message ("trying to read more data after we've done stuff\n");
		return -2;
	}

	context->block_buf[0] = context->block_buf[context->code_last_byte - 2];
	context->block_buf[1] = context->block_buf[context->code_last_byte - 1];

	retval = get_data_block (context, &context->block_buf[2], NULL);

	if (retval == -1)
		return -1;

	if (context->block_count == 0)
		context->code_done = TRUE;

	context->code_last_byte = 2 + context->block_count;
	context->code_curbit = (context->code_curbit - context->code_lastbit) + 16;
	context->code_lastbit = (2 + context->block_count) * 8;

	context->state = context->old_state;
	return 0;
}

static int
get_code (GifContext *context,
	  int   code_size)
{
	int i, j, ret;

	if ((context->code_curbit + code_size) >= context->code_lastbit){
		gif_set_lzw_fill_buffer (context);
		return -3;
	}

	ret = 0;
	for (i = context->code_curbit, j = 0; j < code_size; ++i, ++j)
		ret |= ((context->block_buf[i / 8] & (1 << (i % 8))) != 0) << j;

	context->code_curbit += code_size;

	return ret;
}


static void
set_gif_lzw_clear_code (GifContext *context)
{
	context->state = GIF_LZW_CLEAR_CODE;
	context->lzw_code_pending = -1;
}

static int
gif_lzw_clear_code (GifContext *context)
{
	gint code;

	code = get_code (context, context->lzw_code_size);
	if (code == -3)
		return -0;

	context->lzw_firstcode = context->lzw_oldcode = code;
	context->lzw_code_pending = code;
	context->state = GIF_GET_LZW;
	return 0;
}

static int
lzw_read_byte (GifContext *context)
{
	int code, incode;
	gint retval;
	gint my_retval;
	register int i;

	if (context->lzw_code_pending != -1) {
		retval = context->lzw_code_pending;
		context->lzw_code_pending = -1;
		return retval;
	}

	if (context->lzw_fresh) {
		context->lzw_fresh = FALSE;
		do {
			retval = get_code (context, context->lzw_code_size);
			if (retval < 0) {
				return retval;
			}

			context->lzw_firstcode = context->lzw_oldcode = retval;
		} while (context->lzw_firstcode == context->lzw_clear_code);
		return context->lzw_firstcode;
	}

	if (context->lzw_sp > context->lzw_stack) {
		my_retval = *--(context->lzw_sp);
		return my_retval;
	}

	while ((code = get_code (context, context->lzw_code_size)) >= 0) {
		if (code == context->lzw_clear_code) {
			for (i = 0; i < context->lzw_clear_code; ++i) {
				context->lzw_table[0][i] = 0;
				context->lzw_table[1][i] = i;
			}
			for (; i < (1 << MAX_LZW_BITS); ++i)
				context->lzw_table[0][i] = context->lzw_table[1][i] = 0;
			context->lzw_code_size = context->lzw_set_code_size + 1;
			context->lzw_max_code_size = 2 * context->lzw_clear_code;
			context->lzw_max_code = context->lzw_clear_code + 2;
			context->lzw_sp = context->lzw_stack;

			set_gif_lzw_clear_code (context);
			return -3;
		} else if (code == context->lzw_end_code) {
			int count;
			unsigned char buf[260];

			/*g_error (" DID WE EVER EVER GET HERE\n");*/
			g_error ("Unhandled Case.  If you have an image that causes this, let me <jrb@redhat.com> know.\n");

			if (ZeroDataBlock) {
				return -2;
			}

			while ((count = GetDataBlock (context, buf)) > 0)
				;

			if (count != 0) {
				/*g_print (_("GIF: missing EOD in data stream (common occurence)"));*/
				return -2;
			}
		}

		incode = code;

		if (code >= context->lzw_max_code) {
			*(context->lzw_sp)++ = context->lzw_firstcode;
			code = context->lzw_oldcode;
		}

		while (code >= context->lzw_clear_code) {
			*(context->lzw_sp)++ = context->lzw_table[1][code];

			if (code == context->lzw_table[0][code]) {
				/*g_message (_("GIF: circular table entry BIG ERROR\n"));*/
				/*gimp_quit ();*/
				return -2;
			}
			code = context->lzw_table[0][code];
		}

		*(context->lzw_sp)++ = context->lzw_firstcode = context->lzw_table[1][code];

		if ((code = context->lzw_max_code) < (1 << MAX_LZW_BITS)) {
			context->lzw_table[0][code] = context->lzw_oldcode;
			context->lzw_table[1][code] = context->lzw_firstcode;
			++context->lzw_max_code;
			if ((context->lzw_max_code >= context->lzw_max_code_size) &&
			    (context->lzw_max_code_size < (1 << MAX_LZW_BITS))) {
				context->lzw_max_code_size *= 2;
				++context->lzw_code_size;
			}
		}

		context->lzw_oldcode = incode;

		if (context->lzw_sp > context->lzw_stack) {
			my_retval = *--(context->lzw_sp);
			return my_retval;
		}
	}
	return code;
}

static void
gif_set_get_lzw (GifContext *context)
{
	context->state = GIF_GET_LZW;
	context->draw_xpos = 0;
	context->draw_ypos = 0;
	context->draw_pass = 0;
}

static void
gif_fill_in_pixels (GifContext *context, guchar *dest, gint offset, guchar v)
{
	guchar *pixel = NULL;

	if (context->gif89.transparent) {
		pixel = dest + (context->draw_ypos + offset) * gdk_pixbuf_get_rowstride (context->pixbuf) + context->draw_xpos * 4;
		*pixel = context->color_map [0][(guchar) v];
		*(pixel+1) = context->color_map [1][(guchar) v];
		*(pixel+2) = context->color_map [2][(guchar) v];
		*(pixel+3) = (guchar) ((v == context->gif89.transparent) ? 0 : 65535);
	} else {
		pixel = dest + (context->draw_ypos + offset) * gdk_pixbuf_get_rowstride (context->pixbuf) + context->draw_xpos * 3;
		*pixel = context->color_map [0][(guchar) v];
		*(pixel+1) = context->color_map [1][(guchar) v];
		*(pixel+2) = context->color_map [2][(guchar) v];
	}
}


/* only called if progressive and interlaced */
static void
gif_fill_in_lines (GifContext *context, guchar *dest, guchar v)
{
	switch (context->draw_pass) {
	case 0:
		if (context->draw_ypos > 4) {
			gif_fill_in_pixels (context, dest, -4, v);
			gif_fill_in_pixels (context, dest, -3, v);
		}
		if (context->draw_ypos < (context->frame_height - 4)) {
			gif_fill_in_pixels (context, dest, 3, v);
			gif_fill_in_pixels (context, dest, 4, v);
		}
	case 1:
		if (context->draw_ypos > 2)
			gif_fill_in_pixels (context, dest, -2, v);
		if (context->draw_ypos < (context->frame_height - 2))
			gif_fill_in_pixels (context, dest, 2, v);
	case 2:
		if (context->draw_ypos > 1)
			gif_fill_in_pixels (context, dest, -1, v);
		if (context->draw_ypos < (context->frame_height - 1))
			gif_fill_in_pixels (context, dest, 1, v);
	case 3:
	default:
		break;
	}
}

static int
gif_get_lzw (GifContext *context)
{
	guchar *dest, *temp;
	gint v;


	if (context->pixbuf == NULL) {
		context->pixbuf = gdk_pixbuf_new (ART_PIX_RGB,
						  context->gif89.transparent,
						  8,
						  context->width,
						  context->height);

		if (context->func)
			(* context->func) (context->pixbuf, context->user_data);
	}

	dest = gdk_pixbuf_get_pixels (context->pixbuf);
	while (TRUE) {
		v = lzw_read_byte (context);
		if (v < 0) {
			return v;
		}


		if (context->gif89.transparent) {
			temp = dest + context->draw_ypos * gdk_pixbuf_get_rowstride (context->pixbuf) + context->draw_xpos * 4;
			*temp = context->color_map [0][(guchar) v];
			*(temp+1) = context->color_map [1][(guchar) v];
			*(temp+2) = context->color_map [2][(guchar) v];
			*(temp+3) = (guchar) ((v == context->gif89.transparent) ? 0 : -1);
		} else {
			temp = dest + context->draw_ypos * gdk_pixbuf_get_rowstride (context->pixbuf) + context->draw_xpos * 3;
			*temp = context->color_map [0][(guchar) v];
			*(temp+1) = context->color_map [1][(guchar) v];
			*(temp+2) = context->color_map [2][(guchar) v];
		}

		if (context->func && context->frame_interlace)
			gif_fill_in_lines (context, dest, v);

		context->draw_xpos++;
		if (context->draw_xpos == context->frame_len) {
			context->draw_xpos = 0;
			if (context->frame_interlace) {
				switch (context->draw_pass) {
				case 0:
				case 1:
					context->draw_ypos += 8;
					break;
				case 2:
					context->draw_ypos += 4;
					break;
				case 3:
					context->draw_ypos += 2;
					break;
				}

				if (context->draw_ypos >= context->frame_height) {
					context->draw_pass++;
					switch (context->draw_pass) {
					case 1:
						context->draw_ypos = 4;
						break;
					case 2:
						context->draw_ypos = 2;
						break;
					case 3:
						context->draw_ypos = 1;
						break;
					default:
						goto done;
					}
				}
			} else {
				context->draw_ypos++;
			}
		}
		if (context->draw_ypos >= context->frame_height)
			break;
	}
 done:
	/* we got enough data. there may be more (ie, newer layers) but we can quit now */
	context->state = GIF_DONE;
	return 0;
}

static void
gif_set_prepare_lzw (GifContext *context)
{
	context->state = GIF_PREPARE_LZW;
	context->lzw_code_pending = -1;
}
static int
gif_prepare_lzw (GifContext *context)
{
	gint i;

	if (!ReadOK (context, &(context->lzw_set_code_size), 1)) {
		/*g_message (_("GIF: EOF / read error on image data\n"));*/
		return -1;
	}

	context->lzw_code_size = context->lzw_set_code_size + 1;
	context->lzw_clear_code = 1 << context->lzw_set_code_size;
	context->lzw_end_code = context->lzw_clear_code + 1;
	context->lzw_max_code_size = 2 * context->lzw_clear_code;
	context->lzw_max_code = context->lzw_clear_code + 2;
	context->lzw_fresh = TRUE;
	context->code_curbit = 0;
	context->code_lastbit = 0;
	context->code_last_byte = 0;
	context->code_done = FALSE;

	for (i = 0; i < context->lzw_clear_code; ++i) {
		context->lzw_table[0][i] = 0;
		context->lzw_table[1][i] = i;
	}
	for (; i < (1 << MAX_LZW_BITS); ++i)
		context->lzw_table[0][i] = context->lzw_table[1][0] = 0;

	context->lzw_sp = context->lzw_stack;
	gif_set_get_lzw (context);

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

static void
gif_set_get_frame_info (GifContext *context)
{
	context->state = GIF_GET_FRAME_INFO;
}

static gint
gif_get_frame_info (GifContext *context)
{
	unsigned char buf[9];
	if (!ReadOK (context, buf, 9)) {
		return -1;
	}
	/* Okay, we got all the info we need.  Lets record it */
	context->frame_len = LM_to_uint (buf[4], buf[5]);
	context->frame_height = LM_to_uint (buf[6], buf[7]);
	if (context->frame_height > context->height) {
		context->state = GIF_DONE;
		return -2;
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
	gif_set_prepare_lzw (context);
	return 0;

}

static gint
gif_get_next_step (GifContext *context)
{
	unsigned char c;
	while (TRUE) {
		if (!ReadOK (context, &c, 1)) {
			return -1;
		}
		if (c == ';') {
			/* GIF terminator */
			/* hmm.  Not 100% sure what to do about this.  Should
			 * i try to return a blank image instead? */
			context->state = GIF_DONE;
			return -2;
		}

		if (c == '!') {
			/* Check the extention */
			gif_set_get_extension (context);
			return 0;
		}

		/* look for frame */
		if (c != ',') {
			/* Not a valid start character */
			continue;
		}
		/* load the frame */
		gif_set_get_frame_info (context);
		return 0;
	}
}


static gint
gif_main_loop (GifContext *context)
{
	gint retval = 0;

	do {
		switch (context->state) {
		case GIF_START:
			retval = gif_init (context);
			break;

		case GIF_GET_COLORMAP:
			retval = gif_get_colormap (context);
			if (retval == 0)
				context->state = GIF_GET_NEXT_STEP;
			break;

		case GIF_GET_NEXT_STEP:
			retval = gif_get_next_step (context);
			break;

		case GIF_GET_FRAME_INFO:
			retval = gif_get_frame_info (context);
			break;

		case GIF_GET_EXTENTION:
			retval = gif_get_extension (context);
			if (retval == 0)
				context->state = GIF_GET_NEXT_STEP;
			break;

		case GIF_GET_COLORMAP2:
			retval = gif_get_colormap (context);
			if (retval == 0)
				gif_set_prepare_lzw (context);
			break;

		case GIF_PREPARE_LZW:
			retval = gif_prepare_lzw (context);
			break;

		case GIF_LZW_FILL_BUFFER:
			retval = gif_lzw_fill_buffer (context);
			break;

		case GIF_LZW_CLEAR_CODE:
			retval = gif_lzw_clear_code (context);
			break;

		case GIF_GET_LZW:
			retval = gif_get_lzw (context);
			break;

		case GIF_DONE:
		default:
			retval = 0;
			goto done;
		};
	} while ((retval == 0) || (retval == -3));
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
	context->file = file;
	context->pixbuf = NULL;
	context->state = GIF_START;
	context->func = NULL;

	gif_main_loop (context);

	return context->pixbuf;
}

gpointer
image_begin_load (ModulePreparedNotifyFunc func, gpointer user_data)
{
	GifContext *context;

#ifdef IO_GIFDEBUG
	count = 0;
#endif
	context = g_new (GifContext, 1);
	context->func = func;
	context->user_data = user_data;
	context->file = NULL;
	context->pixbuf = NULL;
	context->state = GIF_START;
	context->buf = NULL;
	context->amount_needed = 0;
	return (gpointer) context;
}

void
image_stop_load (gpointer data)
{
	GifContext *context = (GifContext *) data;

	if (context->pixbuf)
		gdk_pixbuf_unref (context->pixbuf);
//	g_free (context->buf);
	g_free (context);
}

gboolean
image_load_increment (gpointer data, guchar *buf, guint size)
{
	gint retval;
	GifContext *context = (GifContext *) data;

	if (context->amount_needed == 0) {
		/* we aren't looking for some bytes. */
		/* we can use buf now, but we don't want to keep it around at all.
		 * it will be gone by the end of the call. */
		context->buf = buf;
		context->ptr = 0;
		context->size = size;
	} else {
		/* we need some bytes */
		if (size < context->amount_needed) {
			context->amount_needed -= size;
			/* copy it over and return */
			memcpy (context->buf + context->size, buf, size);
			context->size += size;
			return TRUE;
		} else if (size == context->amount_needed) {
			memcpy (context->buf + context->size, buf, size);
			context->size += size;
		} else {
			context->buf = g_realloc (context->buf, context->size + size);
			memcpy (context->buf + context->size, buf, size);
			context->size += size;
		}
	}

	retval = gif_main_loop (context);

	if (retval == -2)
		return FALSE;
	if (retval == -1) {
		/* we didn't have enough memory */
		/* prepare for the next image_load_increment */
		if (context->buf == buf) {
			g_assert (context->size == size);
			context->buf = (guchar *)g_new (guchar, context->amount_needed + (context->size - context->ptr));
			memcpy (context->buf, buf + context->ptr, context->size - context->ptr);
		} else {
			/* copy the left overs to the begining of the buffer */
			/* and realloc the memory */
			memmove (context->buf, context->buf + context->ptr, context->size - context->ptr);
			context->buf = g_realloc (context->buf, context->amount_needed + (context->size - context->ptr));
		}
		context->size = context->size - context->ptr;
		context->ptr = 0;
	} else {
		/* we are prolly all done */
		if (context->buf == buf)
			context->buf = NULL;
	}
	return TRUE;
}
