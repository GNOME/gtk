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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by Owen Taylor <otaylor@redhat.com>, based on code by
 * Carsten Haitzler <raster@rasterman.com>
 */

#include <string.h>

#include "pixbuf.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

static GHashTable *pixbuf_cache = NULL;

static GdkPixbuf *
bilinear_gradient (GdkPixbuf    *src,
		   gint          src_x,
		   gint          src_y,
		   gint          width,
		   gint          height)
{
  guint n_channels = gdk_pixbuf_get_n_channels (src);
  guint src_rowstride = gdk_pixbuf_get_rowstride (src);
  guchar *src_pixels = gdk_pixbuf_get_pixels (src);
  guchar *p1, *p2, *p3, *p4;
  guint dest_rowstride;
  guchar *dest_pixels;
  GdkPixbuf *result;
  int i, j, k;

  if (src_x == 0 || src_y == 0)
    {
      g_warning ("invalid source position for bilinear gradient");
      return NULL;
    }

  p1 = src_pixels + (src_y - 1) * src_rowstride + (src_x - 1) * n_channels;
  p2 = p1 + n_channels;
  p3 = src_pixels + src_y * src_rowstride + (src_x - 1) * n_channels;
  p4 = p3 + n_channels;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
			   width, height);

  if (result == NULL)
    {
      g_warning ("failed to create a %dx%d pixbuf", width, height);
      return NULL;
    }

  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    {
      guchar *p = dest_pixels + dest_rowstride *i;
      guint v[4];
      gint dv[4];

      for (k = 0; k < n_channels; k++)
	{
	  guint start = ((height - i) * p1[k] + (1 + i) * p3[k]) / (height + 1);
	  guint end = ((height -  i) * p2[k] + (1 + i) * p4[k]) / (height + 1);

	  dv[k] = (((gint)end - (gint)start) << 16) / (width + 1);
	  v[k] = (start << 16) + dv[k] + 0x8000;
	}

      for (j = width; j; j--)
	{
	  for (k = 0; k < n_channels; k++)
	    {
	      *(p++) = v[k] >> 16;
	      v[k] += dv[k];
	    }
	}
    }

  return result;
}

static GdkPixbuf *
horizontal_gradient (GdkPixbuf    *src,
		     gint          src_x,
		     gint          src_y,
		     gint          width,
		     gint          height)
{
  guint n_channels = gdk_pixbuf_get_n_channels (src);
  guint src_rowstride = gdk_pixbuf_get_rowstride (src);
  guchar *src_pixels = gdk_pixbuf_get_pixels (src);
  guint dest_rowstride;
  guchar *dest_pixels;
  GdkPixbuf *result;
  int i, j, k;

  if (src_x == 0)
    {
      g_warning ("invalid source position for horizontal gradient");
      return NULL;
    }

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
			   width, height);

  if (result == NULL)
    {
      g_warning ("failed to create a %dx%d pixbuf", width, height);
      return NULL;
    }

  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    {
      guchar *p = dest_pixels + dest_rowstride *i;
      guchar *p1 = src_pixels + (src_y + i) * src_rowstride + (src_x - 1) * n_channels;
      guchar *p2 = p1 + n_channels;

      guint v[4];
      gint dv[4];

      for (k = 0; k < n_channels; k++)
	{
	  dv[k] = (((gint)p2[k] - (gint)p1[k]) << 16) / (width + 1);
	  v[k] = (p1[k] << 16) + dv[k] + 0x8000;
	}
      
      for (j = width; j; j--)
	{
	  for (k = 0; k < n_channels; k++)
	    {
	      *(p++) = v[k] >> 16;
	      v[k] += dv[k];
	    }
	}
    }

  return result;
}

static GdkPixbuf *
vertical_gradient (GdkPixbuf    *src,
		   gint          src_x,
		   gint          src_y,
		   gint          width,
		   gint          height)
{
  guint n_channels = gdk_pixbuf_get_n_channels (src);
  guint src_rowstride = gdk_pixbuf_get_rowstride (src);
  guchar *src_pixels = gdk_pixbuf_get_pixels (src);
  guchar *top_pixels, *bottom_pixels;
  guint dest_rowstride;
  guchar *dest_pixels;
  GdkPixbuf *result;
  int i, j;

  if (src_y == 0)
    {
      g_warning ("invalid source position for vertical gradient");
      return NULL;
    }

  top_pixels = src_pixels + (src_y - 1) * src_rowstride + (src_x) * n_channels;
  bottom_pixels = top_pixels + src_rowstride;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
			   width, height);

  if (result == NULL)
    {
      g_warning ("failed to create a %dx%d pixbuf", width, height);
      return NULL;
    }

  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    {
      guchar *p = dest_pixels + dest_rowstride *i;
      guchar *p1 = top_pixels;
      guchar *p2 = bottom_pixels;

      for (j = width * n_channels; j; j--)
	*(p++) = ((height - i) * *(p1++) + (1 + i) * *(p2++)) / (height + 1);
    }

  return result;
}

static GdkPixbuf *
replicate_single (GdkPixbuf    *src,
		  gint          src_x,
		  gint          src_y,
		  gint          width,
		  gint          height)
{
  guint n_channels = gdk_pixbuf_get_n_channels (src);
  guchar *pixels = (gdk_pixbuf_get_pixels (src) +
		    src_y * gdk_pixbuf_get_rowstride (src) +
		    src_x * n_channels);
  guchar r = *(pixels++);
  guchar g = *(pixels++);
  guchar b = *(pixels++);
  guint dest_rowstride;
  guchar *dest_pixels;
  guchar a = 0;
  GdkPixbuf *result;
  int i, j;

  if (n_channels == 4)
    a = *(pixels++);

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
			   width, height);

  if (result == NULL)
    {
      g_warning ("failed to create a %dx%d pixbuf", width, height);
      return NULL;
    }

  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);
  
  for (i = 0; i < height; i++)
    {
      guchar *p = dest_pixels + dest_rowstride *i;

      for (j = 0; j < width; j++)
	{
	  *(p++) = r;
	  *(p++) = g;
	  *(p++) = b;

	  if (n_channels == 4)
	    *(p++) = a;
	}
    }

  return result;
}

static GdkPixbuf *
replicate_rows (GdkPixbuf    *src,
		gint          src_x,
		gint          src_y,
		gint          width,
		gint          height)
{
  guint n_channels = gdk_pixbuf_get_n_channels (src);
  guint src_rowstride = gdk_pixbuf_get_rowstride (src);
  guchar *pixels = (gdk_pixbuf_get_pixels (src) + src_y * src_rowstride + src_x * n_channels);
  guchar *dest_pixels;
  GdkPixbuf *result;
  guint dest_rowstride;
  int i;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
			   width, height);

  if (result == NULL)
    {
      g_warning ("failed to create a %dx%d pixbuf", width, height);
      return NULL;
    }

  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    memcpy (dest_pixels + dest_rowstride * i, pixels, n_channels * width);

  return result;
}

static GdkPixbuf *
replicate_cols (GdkPixbuf    *src,
		gint          src_x,
		gint          src_y,
		gint          width,
		gint          height)
{
  guint n_channels = gdk_pixbuf_get_n_channels (src);
  guint src_rowstride = gdk_pixbuf_get_rowstride (src);
  guchar *pixels = (gdk_pixbuf_get_pixels (src) + src_y * src_rowstride + src_x * n_channels);
  guchar *dest_pixels;
  GdkPixbuf *result;
  guint dest_rowstride;
  int i, j;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
			   width, height);

  if (result == NULL)
    {
      g_warning ("failed to create a %dx%d pixbuf", width, height);
      return NULL;
    }

  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    {
      guchar *p = dest_pixels + dest_rowstride * i;
      guchar *q = pixels + src_rowstride * i;

      guchar r = *(q++);
      guchar g = *(q++);
      guchar b = *(q++);
      guchar a = 0;
      
      if (n_channels == 4)
	a = *(q++);

      for (j = 0; j < width; j++)
	{
	  *(p++) = r;
	  *(p++) = g;
	  *(p++) = b;

	  if (n_channels == 4)
	    *(p++) = a;
	}
    }

  return result;
}

/* Scale the rectangle (src_x, src_y, src_width, src_height)
 * onto the rectangle (dest_x, dest_y, dest_width, dest_height)
 * of the destination, clip by clip_rect and render
 */
static void
pixbuf_render (GdkPixbuf    *src,
	       guint         hints,
               cairo_t      *cr,
	       gint          src_x,
	       gint          src_y,
	       gint          src_width,
	       gint          src_height,
	       gint          dest_x,
	       gint          dest_y,
	       gint          dest_width,
	       gint          dest_height)
{
  GdkPixbuf *tmp_pixbuf = NULL;
  GdkRectangle rect;
  int x_offset, y_offset;
  gboolean has_alpha = gdk_pixbuf_get_has_alpha (src);
  gint src_rowstride = gdk_pixbuf_get_rowstride (src);
  gint src_n_channels = gdk_pixbuf_get_n_channels (src);

  if (dest_width <= 0 || dest_height <= 0)
    return;

  rect.x = dest_x;
  rect.y = dest_y;
  rect.width = dest_width;
  rect.height = dest_height;

  if (hints & THEME_MISSING)
    return;

  if (dest_width == src_width && dest_height == src_height)
    {
      tmp_pixbuf = g_object_ref (src);

      x_offset = src_x + rect.x - dest_x;
      y_offset = src_y + rect.y - dest_y;
    }
  else if (src_width == 0 && src_height == 0)
    {
      tmp_pixbuf = bilinear_gradient (src, src_x, src_y, dest_width, dest_height);      
      
      x_offset = rect.x - dest_x;
      y_offset = rect.y - dest_y;
    }
  else if (src_width == 0 && dest_height == src_height)
    {
      tmp_pixbuf = horizontal_gradient (src, src_x, src_y, dest_width, dest_height);      
      
      x_offset = rect.x - dest_x;
      y_offset = rect.y - dest_y;
    }
  else if (src_height == 0 && dest_width == src_width)
    {
      tmp_pixbuf = vertical_gradient (src, src_x, src_y, dest_width, dest_height);
      
      x_offset = rect.x - dest_x;
      y_offset = rect.y - dest_y;
    }
  else if ((hints & THEME_CONSTANT_COLS) && (hints & THEME_CONSTANT_ROWS))
    {
      tmp_pixbuf = replicate_single (src, src_x, src_y, dest_width, dest_height);

      x_offset = rect.x - dest_x;
      y_offset = rect.y - dest_y;
    }
  else if (dest_width == src_width && (hints & THEME_CONSTANT_COLS))
    {
      tmp_pixbuf = replicate_rows (src, src_x, src_y, dest_width, dest_height);

      x_offset = rect.x - dest_x;
      y_offset = rect.y - dest_y;
    }
  else if (dest_height == src_height && (hints & THEME_CONSTANT_ROWS))
    {
      tmp_pixbuf = replicate_cols (src, src_x, src_y, dest_width, dest_height);

      x_offset = rect.x - dest_x;
      y_offset = rect.y - dest_y;
    }
  else if (src_width > 0 && src_height > 0)
    {
      double x_scale = (double)dest_width / src_width;
      double y_scale = (double)dest_height / src_height;
      guchar *pixels;
      GdkPixbuf *partial_src;
      
      pixels = (gdk_pixbuf_get_pixels (src)
		+ src_y * src_rowstride
		+ src_x * src_n_channels);

      partial_src = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB,
					      has_alpha,
					      8, src_width, src_height,
					      src_rowstride,
					      NULL, NULL);
						  
      tmp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				   has_alpha, 8,
				   rect.width, rect.height);

      gdk_pixbuf_scale (partial_src, tmp_pixbuf,
			0, 0, rect.width, rect.height,
			dest_x - rect.x, dest_y - rect.y, 
			x_scale, y_scale,
			GDK_INTERP_BILINEAR);

      g_object_unref (partial_src);

      x_offset = 0;
      y_offset = 0;
    }

  if (tmp_pixbuf)
    {
      gdk_cairo_set_source_pixbuf (cr, 
                                   tmp_pixbuf,
                                   -x_offset + rect.x, 
                                   -y_offset + rect.y);
      gdk_cairo_rectangle (cr, &rect);
      cairo_fill (cr);

      g_object_unref (tmp_pixbuf);
    }
}

ThemePixbuf *
theme_pixbuf_new (void)
{
  ThemePixbuf *result = g_new0 (ThemePixbuf, 1);
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
  theme_pixbuf_set_filename (theme_pb, NULL);
  g_free (theme_pb);
}

void         
theme_pixbuf_set_filename (ThemePixbuf *theme_pb,
			   const char  *filename)
{
  if (theme_pb->pixbuf)
    {
      g_object_unref (theme_pb->pixbuf);
      theme_pb->pixbuf = NULL;
    }

  g_free (theme_pb->filename);

  if (filename)
    theme_pb->filename = g_strdup (filename);
  else
    theme_pb->filename = NULL;
}

static guint
compute_hint (GdkPixbuf *pixbuf,
	      gint       x0,
	      gint       x1,
	      gint       y0,
	      gint       y1)
{
  int i, j;
  int hints = THEME_CONSTANT_ROWS | THEME_CONSTANT_COLS | THEME_MISSING;
  int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  
  guchar *data = gdk_pixbuf_get_pixels (pixbuf);
  int rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  if (x0 == x1 || y0 == y1)
    return 0;

  for (i = y0; i < y1; i++)
    {
      guchar *p = data + i * rowstride + x0 * n_channels;
      guchar r = p[0];
      guchar g = p[1];
      guchar b = p[2];
      guchar a = 0;
      
      if (n_channels == 4)
	a = p[3];

      for (j = x0; j < x1 ; j++)
	{
	  if (n_channels != 4 || p[3] != 0)
	    {
	      hints &= ~THEME_MISSING;
	      if (!(hints & THEME_CONSTANT_ROWS))
		goto cols;
	    }
	  
	  if (r != *(p++) ||
	      g != *(p++) ||
	      b != *(p++) ||
	      (n_channels != 4 && a != *(p++)))
	    {
	      hints &= ~THEME_CONSTANT_ROWS;
	      if (!(hints & THEME_MISSING))
		goto cols;
	    }
	}
    }

 cols:
  for (i = y0 + 1; i < y1; i++)
    {
      guchar *base = data + y0 * rowstride + x0 * n_channels;
      guchar *p = data + i * rowstride + x0 * n_channels;

      if (memcmp (p, base, n_channels * (x1 - x0)) != 0)
	{
	  hints &= ~THEME_CONSTANT_COLS;
	  return hints;
	}
    }

  return hints;
}

static void
theme_pixbuf_compute_hints (ThemePixbuf *theme_pb)
{
  int i, j;
  gint width = gdk_pixbuf_get_width (theme_pb->pixbuf);
  gint height = gdk_pixbuf_get_height (theme_pb->pixbuf);

  if (theme_pb->border_left + theme_pb->border_right > width ||
      theme_pb->border_top + theme_pb->border_bottom > height)
    {
      g_warning ("Invalid borders specified for theme pixmap:\n"
		 "        %s,\n"
		 "borders don't fit within the image", theme_pb->filename);
      if (theme_pb->border_left + theme_pb->border_right > width)
	{
	  theme_pb->border_left = width / 2;
	  theme_pb->border_right = (width + 1) / 2;
	}
      if (theme_pb->border_bottom + theme_pb->border_top > height)
	{
	  theme_pb->border_top = height / 2;
	  theme_pb->border_bottom = (height + 1) / 2;
	}
    }
  
  for (i = 0; i < 3; i++)
    {
      gint y0, y1;

      switch (i)
	{
	case 0:
	  y0 = 0;
	  y1 = theme_pb->border_top;
	  break;
	case 1:
	  y0 = theme_pb->border_top;
	  y1 = height - theme_pb->border_bottom;
	  break;
	default:
	  y0 = height - theme_pb->border_bottom;
	  y1 = height;
	  break;
	}
      
      for (j = 0; j < 3; j++)
	{
	  gint x0, x1;
	  
	  switch (j)
	    {
	    case 0:
	      x0 = 0;
	      x1 = theme_pb->border_left;
	      break;
	    case 1:
	      x0 = theme_pb->border_left;
	      x1 = width - theme_pb->border_right;
	      break;
	    default:
	      x0 = width - theme_pb->border_right;
	      x1 = width;
	      break;
	    }

	  theme_pb->hints[i][j] = compute_hint (theme_pb->pixbuf, x0, x1, y0, y1);
	}
    }
  
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

  if (theme_pb->pixbuf)
    theme_pixbuf_compute_hints (theme_pb);
}

void
theme_pixbuf_set_stretch (ThemePixbuf *theme_pb,
			  gboolean     stretch)
{
  theme_pb->stretch = stretch;

  if (theme_pb->pixbuf)
    theme_pixbuf_compute_hints (theme_pb);
}

void
theme_pixbuf_uncache (gpointer  data,
                      GObject  *where_the_object_was)
{
  g_hash_table_remove (pixbuf_cache, data);
}

GdkPixbuf *
theme_pixbuf_get_pixbuf (ThemePixbuf *theme_pb)
{
  if (!theme_pb->pixbuf)
    {
      gpointer pixbuf;

      if (!pixbuf_cache)
        /* Hash table does not hold its own reference to the GdkPixbuf */
        pixbuf_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, NULL);

      /* Do an extended lookup because we store NULL in the hash table
       * (below) to indicate that we failed to load the given filename.
       */
      if (!g_hash_table_lookup_extended (pixbuf_cache, theme_pb->filename,
                                         NULL, &pixbuf))
        /* Not in the cache.  Add it and take the first ref. */
        {
          gchar *key = g_strdup (theme_pb->filename);
          GError *error = NULL;

          pixbuf = gdk_pixbuf_new_from_file (key, &error);

          if (pixbuf != NULL)
            {
              /* Drop the pixbuf from the cache when we lose the last ref. */
              g_object_weak_ref (G_OBJECT (pixbuf), theme_pixbuf_uncache, key);
            }
          else
            {
              /* Never drop a negative from the cache. */
              g_warning ("Pixbuf theme: Cannot load pixmap file %s: %s\n",
                         theme_pb->filename, error->message);
              g_error_free (error);
            }

          /* Always insert, even if we failed to create the pixbuf. */
          g_hash_table_insert (pixbuf_cache, key, pixbuf);
          theme_pb->pixbuf = pixbuf;
        }

      else
        /* In the cache.  Take an additional ref. */
        theme_pb->pixbuf = g_object_ref (pixbuf);

      if (theme_pb->stretch)
	theme_pixbuf_compute_hints (theme_pb);
    }
  
  return theme_pb->pixbuf;
}

void
theme_pixbuf_render (ThemePixbuf  *theme_pb,
                     cairo_t      *cr,
		     guint         component_mask,
		     gboolean      center,
		     gint          x,
		     gint          y,
		     gint          width,
		     gint          height)
{
  GdkPixbuf *pixbuf = theme_pixbuf_get_pixbuf (theme_pb);
  gint src_x[4], src_y[4], dest_x[4], dest_y[4];
  gint pixbuf_width = gdk_pixbuf_get_width (pixbuf);
  gint pixbuf_height = gdk_pixbuf_get_height (pixbuf);

  if (!pixbuf)
    return;

  if (theme_pb->stretch)
    {
      if (component_mask & COMPONENT_ALL)
	component_mask = (COMPONENT_ALL - 1) & ~component_mask;

      src_x[0] = 0;
      src_x[1] = theme_pb->border_left;
      src_x[2] = pixbuf_width - theme_pb->border_right;
      src_x[3] = pixbuf_width;
      
      src_y[0] = 0;
      src_y[1] = theme_pb->border_top;
      src_y[2] = pixbuf_height - theme_pb->border_bottom;
      src_y[3] = pixbuf_height;
      
      dest_x[0] = x;
      dest_x[1] = x + theme_pb->border_left;
      dest_x[2] = x + width - theme_pb->border_right;
      dest_x[3] = x + width;

      if (dest_x[1] > dest_x[2])
	{
	  component_mask &= ~(COMPONENT_NORTH | COMPONENT_SOUTH | COMPONENT_CENTER);
	  dest_x[1] = dest_x[2] = (dest_x[1] + dest_x[2]) / 2;
	}

      dest_y[0] = y;
      dest_y[1] = y + theme_pb->border_top;
      dest_y[2] = y + height - theme_pb->border_bottom;
      dest_y[3] = y + height;

      if (dest_y[1] > dest_y[2])
	{
	  component_mask &= ~(COMPONENT_EAST | COMPONENT_WEST | COMPONENT_CENTER);
	  dest_y[1] = dest_y[2] = (dest_y[1] + dest_y[2]) / 2;
	}



#define RENDER_COMPONENT(X1,X2,Y1,Y2)					   \
        pixbuf_render (pixbuf, theme_pb->hints[Y1][X1], cr,                \
	 	       src_x[X1], src_y[Y1],				   \
		       src_x[X2] - src_x[X1], src_y[Y2] - src_y[Y1],	   \
		       dest_x[X1], dest_y[Y1],				   \
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
	  x += (width - pixbuf_width) / 2;
	  y += (height - pixbuf_height) / 2;
	  
	  pixbuf_render (pixbuf, 0, cr,
			 0, 0,
			 pixbuf_width, pixbuf_height,
			 x, y,
			 pixbuf_width, pixbuf_height);
	}
      else
	{
          gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
          cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);

	  cairo_rectangle (cr, x, y, width, height);
	  
          cairo_fill (cr);
	}
    }
}
