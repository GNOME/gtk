/* GTK+ Pixbuf Engine
 * Copyright (C) 1998-2000 Red Hat, Inc.
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
 * Written by Owen Taylor <otaylor@redhat.com>, based on code by
 * Carsten Haitzler <raster@rasterman.com>
 */

#include "pixmap_theme.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

GCache *pixbuf_cache = NULL;

static void
pixbuf_render (GdkPixbuf    *src,
	       GdkWindow    *window,
	       GdkBitmap    *mask,
	       GdkRectangle *clip_rect,
	       gint          src_x,
	       gint          src_y,
	       gint          src_width,
	       gint          src_height,
	       gint          dest_x,
	       gint          dest_y,
	       gint          dest_width,
	       gint          dest_height)
{
  GdkPixbuf *tmp_pixbuf;
  GdkRectangle rect;
  int x_offset, y_offset;

  if (dest_width <= 0 || dest_height <= 0)
    return;

  rect.x = dest_x;
  rect.y = dest_y;
  rect.width = dest_width;
  rect.height = dest_height;

  /* FIXME: we need the full mask, not a partial mask; the following is, however,
   *  horribly expensive
   */
  if (!mask && clip_rect && !gdk_rectangle_intersect (clip_rect, &rect, &rect))
    return;
  
  if (dest_width != src->art_pixbuf->width ||
      dest_height != src->art_pixbuf->height)
    {
      ArtPixBuf *partial_src_art;
      GdkPixbuf *partial_src_gdk;

      if (src->art_pixbuf->n_channels == 3)
	{
	  partial_src_art = 
	    art_pixbuf_new_const_rgb (src->art_pixbuf->pixels + src_y * src->art_pixbuf->rowstride + src_x * src->art_pixbuf->n_channels,
				      src_width, 
				      src_height, 
				      src->art_pixbuf->rowstride);
	}
      else
	{
	  partial_src_art = 
	    art_pixbuf_new_const_rgba (src->art_pixbuf->pixels + src_y * src->art_pixbuf->rowstride + src_x * src->art_pixbuf->n_channels,
				       src_width, 
				       src_height, 
				       src->art_pixbuf->rowstride);
	}

      partial_src_gdk = gdk_pixbuf_new_from_art_pixbuf (partial_src_art);
      tmp_pixbuf = gdk_pixbuf_new (ART_PIX_RGB, src->art_pixbuf->has_alpha, 8, rect.width, rect.height);

      gdk_pixbuf_scale (partial_src_gdk, tmp_pixbuf, 0, 0, rect.width, rect.height,
			dest_x - rect.x, dest_y - rect.y, 
			(double)dest_width / src_width, (double)dest_height / src_height,
			ART_FILTER_BILINEAR);

      gdk_pixbuf_unref (partial_src_gdk);
      
      x_offset = 0;
      y_offset = 0;
    }
  else
    {
      tmp_pixbuf = src;
      gdk_pixbuf_ref (tmp_pixbuf);

      x_offset = src_x + rect.x - dest_x;
      y_offset = src_y + rect.y - dest_y;
    }

  if (mask)
    {
      GdkGC *tmp_gc;

      gdk_pixbuf_render_threshold_alpha (tmp_pixbuf, mask,
					 x_offset, y_offset,
					 rect.x, rect.y,
					 rect.width, rect.height,
					 128);

      tmp_gc = gdk_gc_new (window);
      gdk_pixbuf_render_to_drawable (tmp_pixbuf, window, tmp_gc, 
				     x_offset, y_offset,
				     rect.x, rect.y,
				     rect.width, rect.height,
				     GDK_RGB_DITHER_NORMAL,
				     0, 0);
      gdk_gc_unref (tmp_gc);
    }
  else
    gdk_pixbuf_render_to_drawable_alpha (tmp_pixbuf, window,
					 x_offset, y_offset,
					 rect.x, rect.y,
					 rect.width, rect.height,
					 GDK_PIXBUF_ALPHA_BILEVEL, 128,
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
  gdk_pixbuf_unref (tmp_pixbuf);
}

ThemePixbuf *
theme_pixbuf_new (void)
{
  ThemePixbuf *result = g_new (ThemePixbuf, 1);
  result->filename = NULL;
  result->pixbuf = NULL;

  result->stretch = TRUE;
  result->border_left = 0;
  result->border_right = 0;
  result->border_bottom = 0;
  result->border_top = 0;

  return result;
}

void
theme_pixbuf_destroy (ThemePixbuf *theme_pb)
{
  if (theme_pb->pixbuf)
    g_cache_remove (pixbuf_cache, theme_pb->pixbuf);
}

void         
theme_pixbuf_set_filename (ThemePixbuf *theme_pb,
			   const char  *filename)
{
  if (theme_pb->pixbuf)
    {
      g_cache_remove (pixbuf_cache, theme_pb->pixbuf);
      theme_pb->pixbuf = NULL;
    }

  if (theme_pb->filename)
    g_free (theme_pb->filename);

  theme_pb->filename = g_strdup (filename);
}

void
theme_pixbuf_set_border (ThemePixbuf *theme_pb,
			 gint         left,
			 gint         right,
			 gint         top,
			 gint         bottom)
{
  theme_pb->border_left = left;
  theme_pb->border_right = right;
  theme_pb->border_top = top;
  theme_pb->border_bottom = bottom;
}

void
theme_pixbuf_set_stretch (ThemePixbuf *theme_pb,
			  gboolean     stretch)
{
  theme_pb->stretch = stretch;
}

GdkPixbuf *
pixbuf_cache_value_new (gchar *filename)
{
  GdkPixbuf *result = gdk_pixbuf_new_from_file (filename);
  if (!result)
    g_warning("Pixbuf theme: Cannot load pixmap file %s\n", filename);

  return result;
}

GdkPixbuf *
theme_pixbuf_get_pixbuf (ThemePixbuf *theme_pb)
{
  if (!theme_pb->pixbuf)
    {
      if (!pixbuf_cache)
	pixbuf_cache = g_cache_new ((GCacheNewFunc)pixbuf_cache_value_new,
				    (GCacheDestroyFunc)gdk_pixbuf_unref,
				    (GCacheDupFunc)g_strdup,
				    (GCacheDestroyFunc)g_free,
				    g_str_hash, g_direct_hash, g_str_equal);
      
      theme_pb->pixbuf = g_cache_insert (pixbuf_cache, theme_pb->filename);
    }
  
  return theme_pb->pixbuf;
}

void
theme_pixbuf_render (ThemePixbuf  *theme_pb,
		     GdkWindow    *window,
		     GdkBitmap    *mask,
		     GdkRectangle *clip_rect,
		     guint         component_mask,
		     gboolean      center,
		     gint          x,
		     gint          y,
		     gint          width,
		     gint          height)
{
  GdkPixbuf *pixbuf = theme_pixbuf_get_pixbuf (theme_pb);
  gint src_x[4], src_y[4], dest_x[4], dest_y[4];

  if (!pixbuf)
    return;

  if (theme_pb->stretch)
    {
      src_x[0] = 0;
      src_x[1] = theme_pb->border_left;
      src_x[2] = pixbuf->art_pixbuf->width - theme_pb->border_right;
      src_x[3] = pixbuf->art_pixbuf->width;
      
      src_y[0] = 0;
      src_y[1] = theme_pb->border_top;
      src_y[2] = pixbuf->art_pixbuf->height - theme_pb->border_bottom;
      src_y[3] = pixbuf->art_pixbuf->height;
      
      dest_x[0] = x;
      dest_x[1] = x + theme_pb->border_left;
      dest_x[2] = x + width - theme_pb->border_right;
      dest_x[3] = x + width;

      dest_y[0] = y;
      dest_y[1] = y + theme_pb->border_top;
      dest_y[2] = y + height - theme_pb->border_bottom;
      dest_y[3] = y + height;

      if (component_mask & COMPONENT_ALL)
	component_mask = (COMPONENT_ALL - 1) & ~component_mask;

#define RENDER_COMPONENT(X1,X2,Y1,Y2)					\
        pixbuf_render (pixbuf, window, mask, clip_rect,			\
	 	       src_x[X1], src_y[Y1],				\
		       src_x[X2] - src_x[X1], src_y[Y2] - src_y[Y1],	\
		       dest_x[X1], dest_y[Y1],				\
		       dest_x[X2] - dest_x[X1], dest_y[Y2] - dest_y[Y1]);
      
      if (component_mask & COMPONENT_NORTH_WEST)
	RENDER_COMPONENT (0, 1, 0, 1);

      if (component_mask & COMPONENT_NORTH)
	RENDER_COMPONENT (1, 2, 0, 1);

      if (component_mask & COMPONENT_NORTH_EAST)
	RENDER_COMPONENT (2, 3, 0, 1);

      if (component_mask & COMPONENT_WEST)
	RENDER_COMPONENT (0, 1, 1, 2);

      if (component_mask & COMPONENT_CENTER)
	RENDER_COMPONENT (1, 2, 1, 2);

      if (component_mask & COMPONENT_EAST)
	RENDER_COMPONENT (2, 3, 1, 2);

      if (component_mask & COMPONENT_SOUTH_WEST)
	RENDER_COMPONENT (0, 1, 2, 3);

      if (component_mask & COMPONENT_SOUTH)
	RENDER_COMPONENT (1, 2, 2, 3);

      if (component_mask & COMPONENT_SOUTH_EAST)
	RENDER_COMPONENT (2, 3, 2, 3);
    }
  else
    {
      if (center)
	{
	  x += (width - pixbuf->art_pixbuf->width) / 2;
	  y += (height - pixbuf->art_pixbuf->height) / 2;
	  
	  pixbuf_render (pixbuf, window, NULL, clip_rect,
			 0, 0,
			 pixbuf->art_pixbuf->width, pixbuf->art_pixbuf->height,
			 x, y,
			 pixbuf->art_pixbuf->width, pixbuf->art_pixbuf->height);
	}
      else
	{
	  GdkPixmap *tmp_pixmap;
	  GdkGC *tmp_gc;
	  GdkGCValues gc_values;

	  tmp_pixmap = gdk_pixmap_new (window,
				       pixbuf->art_pixbuf->width,
				       pixbuf->art_pixbuf->height,
				       -1);
	  tmp_gc = gdk_gc_new (tmp_pixmap);
	  gdk_pixbuf_render_to_drawable (pixbuf, tmp_pixmap, tmp_gc,
					 0, 0, 
					 0, 0,
					 pixbuf->art_pixbuf->width, pixbuf->art_pixbuf->height,
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
	  gdk_gc_unref (tmp_gc);

	  gc_values.fill = GDK_TILED;
	  gc_values.tile = tmp_pixmap;
	  tmp_gc = gdk_gc_new_with_values (window,
					   &gc_values, GDK_GC_FILL | GDK_GC_TILE);
	  if (clip_rect)
	    gdk_draw_rectangle (window, tmp_gc, TRUE,
				clip_rect->x, clip_rect->y, clip_rect->width, clip_rect->height);
	  else
	    gdk_draw_rectangle (window, tmp_gc, TRUE, x, y, width, height);
	  
	  gdk_gc_unref (tmp_gc);
	  gdk_pixmap_unref (tmp_pixmap);
	}
    }
}
