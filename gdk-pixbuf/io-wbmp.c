/* GdkPixbuf library - Wireless Bitmap image loader
 *
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * Authors: Elliot Lee <sopwith@redhat.com
 *
 * Based on io-bmp.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*

Known bugs:
        * Since this is based off the libgd implementation, no extended headers implemented (not required for a WAP client)
*/

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"



/* Progressive loading */

struct wbmp_progressive_state {
  ModulePreparedNotifyFunc prepared_func;
  ModuleUpdatedNotifyFunc updated_func;
  gpointer user_data;

  gboolean need_type : 1;
  gboolean need_header : 1;
  gboolean need_width : 1;
  gboolean need_height : 1;
  gboolean needmore : 1;
  gboolean call_progressive_updates : 1;

  guchar last_buf[16]; /* Just needs to be big enough to hold the largest datum requestable via 'getin' */
  guint last_len;

  int type;
  int width, height, curx, cury;

  GdkPixbuf *pixbuf;	/* Our "target" */
};

gpointer
gdk_pixbuf__wbmp_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				  ModuleUpdatedNotifyFunc updated_func,
				  ModuleFrameDoneNotifyFunc frame_done_func,
				  ModuleAnimationDoneNotifyFunc
				  anim_done_func, gpointer user_data);

void gdk_pixbuf__wbmp_image_stop_load(gpointer data);
gboolean gdk_pixbuf__wbmp_image_load_increment(gpointer data, guchar * buf,
					       guint size);


/* Shared library entry point --> This should be removed when
   generic_image_load enters gdk-pixbuf-io. */
GdkPixbuf *gdk_pixbuf__wbmp_image_load(FILE * f)
{
	size_t length;
	char membuf[4096];
	struct wbmp_progressive_state *State;

	GdkPixbuf *pb;

	State = gdk_pixbuf__wbmp_image_begin_load(NULL, NULL, NULL, NULL, NULL);

	while (feof(f) == 0) {
		length = fread(membuf, 1, 4096, f);
		if (length > 0)
		  gdk_pixbuf__wbmp_image_load_increment(State,
							membuf,
							length);

	}
	if (State->pixbuf != NULL)
		gdk_pixbuf_ref(State->pixbuf);

	pb = State->pixbuf;

	gdk_pixbuf__wbmp_image_stop_load(State);
	return pb;
}

/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

gpointer
gdk_pixbuf__wbmp_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
				 ModuleFrameDoneNotifyFunc frame_done_func,
				 ModuleAnimationDoneNotifyFunc
				 anim_done_func, gpointer user_data)
{
	struct wbmp_progressive_state *context;

	context = g_new0(struct wbmp_progressive_state, 1);
	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;

	context->needmore = context->need_type = context->need_header = context->need_width = context->need_height = TRUE;
	context->call_progressive_updates = TRUE;
	context->pixbuf = NULL;

	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
void gdk_pixbuf__wbmp_image_stop_load(gpointer data)
{
	struct wbmp_progressive_state *context =
	    (struct wbmp_progressive_state *) data;

	g_return_if_fail(context != NULL);
	if (context->pixbuf)
	  gdk_pixbuf_unref(context->pixbuf);

	g_free(context);
}

static gboolean
getin(struct wbmp_progressive_state *context, guchar **buf, guint *buf_size, guchar *ptr, int datum_size)
{
  int last_num, buf_num;

  if((context->last_len + *buf_size) < datum_size)
    return FALSE;

  /* We know we can pull it out of there */
  last_num = MIN(datum_size, context->last_len);
  buf_num = MIN(datum_size-last_num, *buf_size);
  memcpy(ptr, context->last_buf, last_num);
  memcpy(ptr+last_num, *buf, buf_num);
	 
  context->last_len -= last_num;
  if(context->last_len)
    memmove(context->last_buf, context->last_buf+last_num, context->last_len);
  *buf_size -= buf_num;
  *buf += buf_num;

  return TRUE;
}

static gboolean
save_rest(struct wbmp_progressive_state *context, const guchar *buf, guint buf_size)
{
  if(buf_size > (sizeof(context->last_buf) - context->last_len))
    return FALSE;

  memcpy(context->last_buf+context->last_len, buf, buf_size);
  context->last_len += buf_size;

  return TRUE;
}

static gboolean
get_mbi(struct wbmp_progressive_state *context, guchar **buf, guint *buf_size, int *val)
{
  guchar intbuf[16];
  int i, n;
  gboolean rv;

  *val = 0;
  n = i = 0;
  do {
    rv = getin(context, buf, buf_size, intbuf+n, 1);
    if(!rv)
      goto out;
    *val <<= 7;
    *val |= intbuf[n] & 0x7F;
    n++;
  } while(n < sizeof(intbuf) && (intbuf[n-1] & 0x80));

 out:
  if(!rv || (intbuf[n-1] & 0x80))
    {
      rv = save_rest(context, intbuf, n);

      if(!rv)
	g_error("Couldn't save_rest of intbuf");
      return FALSE;
    }

  return TRUE;
}

/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
gboolean gdk_pixbuf__wbmp_image_load_increment(gpointer data, guchar * buf,
					      guint size)
{
	struct wbmp_progressive_state *context =
	    (struct wbmp_progressive_state *) data;
	gboolean bv;

	do
	  {
	    if(context->need_type)
	      {
		guchar val;

		bv = getin(context, &buf, &size, &val, 1);
		if(bv)
		  {
		    context->type = val;
		    context->need_type = FALSE;
		  }
	      }
	    else if(context->need_header)
	      {
		guchar val;

		bv = getin(context, &buf, &size, &val, 1);
		if(bv)
		  {
		    /* We skip over the extended header - val is unused */
		    if(!(val & 0x80))
		      context->need_header = FALSE;
		  }
	      }
	    else if(context->need_width)
	      {
		bv = get_mbi(context, &buf, &size, &context->width);
		if(bv)
		  context->need_width = FALSE;
	      }
	    else if(context->need_height)
	      {
		bv = get_mbi(context, &buf, &size, &context->height);
		if(bv)
		  {
		    context->need_height = FALSE;
		    context->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, context->width, context->height);
		    g_assert(context->pixbuf);

		    if(context->prepared_func)
		      context->prepared_func(context->pixbuf, context->user_data);
		  }
	      }
	    else if(context->needmore)
	      {
		int first_row;
		first_row = context->cury;
		for( ; context->cury < context->height; context->cury++, context->curx = 0)
		  {
		    for( ; context->curx < context->width; context->curx += 8)
		      {
			guchar byte;
			guchar *ptr;
			int xoff;
			bv = getin(context, &buf, &size, &byte, 1);
			if(!bv)
			  goto out;

			ptr = context->pixbuf->pixels + context->pixbuf->rowstride * context->cury + context->curx * 3;
			for(xoff = 7; xoff >= 0; xoff--, ptr += 3)
			  {
			    guchar pixval;

			    if(byte & (1<<xoff))
			      pixval = 0xFF;
			    else
			      pixval = 0x0;

			    ptr[0] = ptr[1] = ptr[2] = pixval;
			  }
		      }
		  }
		context->needmore = FALSE;

	      out:
		context->updated_func(context->pixbuf, 0, first_row, context->width, context->cury - first_row + 1,
				      context->user_data);
	      }
	    else
	      bv = FALSE; /* Nothing left to do, stop feeding me data! */

	  } while(bv);

	if(size)
	  return save_rest(context, buf, size);
	else
	  return context->needmore;
}
