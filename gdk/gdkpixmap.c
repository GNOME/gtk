/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "../config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Needed for SEEK_END in SunOS */
#include <unistd.h>
#include <X11/Xlib.h>

#include "gdk.h"
#include "gdkprivate.h"

typedef struct
{
  gchar *color_string;
  GdkColor color;
  gint transparent;
} _GdkPixmapColor;

GdkPixmap*
gdk_pixmap_new (GdkWindow *window,
		gint       width,
		gint       height,
		gint       depth)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  if (depth == -1)
    gdk_window_get_geometry (window, NULL, NULL, NULL, NULL, &depth);

  private = g_new (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;

  window_private = (GdkWindowPrivate*) window;

  private->xdisplay = window_private->xdisplay;
  private->window_type = GDK_WINDOW_PIXMAP;
  private->xwindow = XCreatePixmap (private->xdisplay, window_private->xwindow,
				    width, height, depth);
  private->parent = NULL;
  private->x = 0;
  private->y = 0;
  private->width = width;
  private->height = height;
  private->resize_count = 0;
  private->ref_count = 1;
  private->destroyed = 0;

  gdk_xid_table_insert (&private->xwindow, pixmap);

  return pixmap;
}

GdkPixmap *
gdk_bitmap_create_from_data (GdkWindow *window,
			     gchar     *data,
			     gint       width,
			     gint       height)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (data != NULL, NULL);

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  private = g_new (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;

  window_private = (GdkWindowPrivate*) window;

  private->parent = NULL;
  private->xdisplay = window_private->xdisplay;
  private->window_type = GDK_WINDOW_PIXMAP;
  private->x = 0;
  private->y = 0;
  private->width = width;
  private->height = height;
  private->resize_count = 0;
  private->ref_count = 1;
  private->destroyed = FALSE;

  private->xwindow = XCreateBitmapFromData (private->xdisplay,
					    window_private->xwindow,
					    data, width, height);

  gdk_xid_table_insert (&private->xwindow, pixmap);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkWindow *window,
			     gchar     *data,
			     gint       width,
			     gint       height,
			     gint       depth,
			     GdkColor  *fg,
			     GdkColor  *bg)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  if (depth == -1)
    gdk_window_get_geometry (window, NULL, NULL, NULL, NULL, &depth);

  private = g_new (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;

  window_private = (GdkWindowPrivate*) window;

  private->parent = NULL;
  private->xdisplay = window_private->xdisplay;
  private->window_type = GDK_WINDOW_PIXMAP;
  private->x = 0;
  private->y = 0;
  private->width = width;
  private->height = height;
  private->resize_count = 0;
  private->ref_count = 1;
  private->destroyed = FALSE;

  private->xwindow = XCreatePixmapFromBitmapData (private->xdisplay,
						  window_private->xwindow,
						  data, width, height,
						  fg->pixel, bg->pixel, depth);

  gdk_xid_table_insert (&private->xwindow, pixmap);

  return pixmap;
}

gint
gdk_pixmap_seek_string (FILE  *infile,
                        const gchar *str,
                        gint   skip_comments)
{
  char instr[1024];

  while (!feof (infile))
    {
      fscanf (infile, "%s", instr);
      if (skip_comments == TRUE && strcmp (instr, "/*") == 0)
        {
          fscanf (infile, "%s", instr);
          while (!feof (infile) && strcmp (instr, "*/") != 0)
            fscanf (infile, "%s", instr);
          fscanf(infile, "%s", instr);
        }
      if (strcmp (instr, str)==0)
        return TRUE;
    }

  return FALSE;
}

gint
gdk_pixmap_seek_char (FILE  *infile,
                      gchar  c)
{
  gchar b, oldb;

  while (!feof (infile))
    {
      fscanf(infile, "%c", &b);
      if (c != b && b == '/')
        {
          fscanf (infile, "%c", &b);
          if (b == '*')
            {
              oldb = b;
              while (!feof (infile) && !(oldb == '*' && b == '/'))
                {
                  oldb = b;
                  fscanf (infile, "%c", &b);
                }
              fscanf (infile, "%c", &b);
            }
        }
      if (c == b)
        return TRUE;
    }

  return FALSE;
}

gint
gdk_pixmap_read_string (FILE  *infile,
                        gchar **buffer,
			guint *buffer_size)
{
  gchar c;
  guint cnt = 0;

  if ((*buffer) == NULL)
    {
      (*buffer_size) = 10 * sizeof (gchar);
      (*buffer) = (gchar *) malloc (*buffer_size);
    }

  do
    fscanf (infile, "%c", &c);
  while (!feof (infile) && c != '"');

  if (c != '"')
    return FALSE;

  while (!feof (infile))
    {
      fscanf (infile, "%c", &c);

      if (cnt == (*buffer_size))
	{
	  (*buffer_size) *= 2;
	  (*buffer) = (gchar *) realloc ((*buffer), *buffer_size);	  
	}

      if (c != '"')
        (*buffer)[cnt++] = c;
      else
        {
          (*buffer)[cnt++] = 0;
          return TRUE;
        }
    }

  return FALSE;
}

gchar*
gdk_pixmap_skip_whitespaces (gchar *buffer)
{
  gint32 index = 0;

  while (buffer[index] != 0 && (buffer[index] == 0x20 || buffer[index] == 0x09))
    index++;

  return &buffer[index];
}

gchar*
gdk_pixmap_skip_string (gchar *buffer)
{
  gint32 index = 0;

  while (buffer[index] != 0 && buffer[index] != 0x20 && buffer[index] != 0x09)
    index++;

  return &buffer[index];
}

gchar*
gdk_pixmap_extract_color (gchar *buffer)
{
  gint counter, finished = FALSE, numnames;
  gchar *ptr = NULL, ch, temp[128];
  gchar color[128], *retcol;

  counter = 0;
  while (ptr == NULL)
    {
      if (buffer[counter] == 'c')
        {
          ch = buffer[counter + 1];
          if (ch == 0x20 || ch == 0x09)
            ptr = &buffer[counter + 1];
        }
      else if (buffer[counter] == 0)
        return NULL;

      counter++;
    }

  if (ptr == NULL)
    return NULL;

  ptr = gdk_pixmap_skip_whitespaces (ptr);

  if (ptr[0] == 0)
    return NULL;
  else if (ptr[0] == '#')
    {
      retcol = g_new(gchar, strlen (ptr) + 1);
      strcpy (retcol, ptr);
      return retcol;
    }

  color[0] = 0;
  numnames = 0;

  while (finished == FALSE)
    {
      sscanf (ptr, "%s", temp);

      if ((gint)ptr[0] == 0 || strcmp ("s", temp) == 0 || strcmp ("m", temp) == 0 ||
          strcmp ("g", temp) == 0 || strcmp ("g4", temp) == 0)
       finished = TRUE;
      else
        {
          if (numnames > 0)
            strcat (color, " ");
          strcat (color, temp);
          ptr = gdk_pixmap_skip_string (ptr);
          ptr = gdk_pixmap_skip_whitespaces (ptr);
          numnames++;
        }
    }

  retcol = g_new(gchar, strlen (color) + 1);
  strcpy (retcol, color);
  return retcol;
}


GdkPixmap*
gdk_pixmap_create_from_xpm (GdkWindow  *window,
			    GdkBitmap **mask,
			    GdkColor   *transparent_color,
			    const gchar *filename)
{
  FILE *infile = NULL;
  GdkPixmap *pixmap = NULL;
  GdkImage *image = NULL;
  GdkColormap *colormap;
  GdkVisual *visual;
  GdkGC *gc;
  GdkColor tmp_color;
  gint width, height, num_cols, cpp, cnt, n, ns, xcnt, ycnt;
  gchar *buffer = NULL, *color_name = NULL, pixel_str[32];
  guint buffer_size = 0;
  _GdkPixmapColor *colors = NULL, *color = NULL;
  gulong index;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  infile = fopen (filename, "rb");
  if (infile != NULL)
    {
      if (gdk_pixmap_seek_string (infile, "XPM", FALSE) == TRUE)
        {
          if (gdk_pixmap_seek_char (infile,'{') == TRUE)
            {
              gdk_pixmap_seek_char (infile, '"');
              fseek (infile, -1, SEEK_CUR);
              gdk_pixmap_read_string (infile, &buffer, &buffer_size);

              sscanf (buffer,"%d %d %d %d", &width, &height, &num_cols, &cpp);

              colors = g_new(_GdkPixmapColor, num_cols);

              colormap = gdk_window_get_colormap (window);
              visual = gdk_window_get_visual (window);

	      if (transparent_color == NULL) 
		{
		  gdk_color_white (colormap, &tmp_color);
		  transparent_color = &tmp_color;
		}

              for (cnt = 0; cnt < num_cols; cnt++)
                {
                  gdk_pixmap_seek_char (infile, '"');
                  fseek (infile, -1, SEEK_CUR);
                  gdk_pixmap_read_string (infile, &buffer, &buffer_size);

                  colors[cnt].color_string = g_new(gchar, cpp + 1);
                  for (n = 0; n < cpp; n++)
                    colors[cnt].color_string[n] = buffer[n];
                  colors[cnt].color_string[n] = 0;
		  colors[cnt].transparent = FALSE;

                  if (color_name != NULL)
                    g_free (color_name);

                  color_name = gdk_pixmap_extract_color (&buffer[cpp]);

                  if (color_name != NULL)
                    {
                      if (gdk_color_parse (color_name, &colors[cnt].color) == FALSE)
			{
			  colors[cnt].color = *transparent_color;
			  colors[cnt].transparent = TRUE;
			}
                    }
                  else
		    {
		      colors[cnt].color = *transparent_color;
		      colors[cnt].transparent = TRUE;
		    }

                  gdk_color_alloc (colormap, &colors[cnt].color);
                }

              index = 0;
              image = gdk_image_new (GDK_IMAGE_FASTEST, visual, width, height);

	      gc = NULL;
	      if (mask)
		{
		  /* The pixmap mask is just a bits pattern.
		   * Color 0 is used for background and 1 for foreground.
		   * We don't care about the colormap, we just need 0 and 1.
		   */
		  GdkColor mask_pattern;
		  
		  *mask = gdk_pixmap_new (window, width, height, 1);
		  gc = gdk_gc_new (*mask);
		  
		  mask_pattern.pixel = 0;
		  gdk_gc_set_foreground (gc, &mask_pattern);
		  gdk_draw_rectangle (*mask, gc, TRUE, 0, 0, -1, -1);
		  
		  mask_pattern.pixel = 1;
		  gdk_gc_set_foreground (gc, &mask_pattern);
		}

              for (ycnt = 0; ycnt < height; ycnt++)
                {
                  gdk_pixmap_read_string (infile, &buffer, &buffer_size);

                  for (n = 0, cnt = 0, xcnt = 0; n < (width * cpp); n += cpp, xcnt++)
                    {
                      strncpy (pixel_str, &buffer[n], cpp);
                      pixel_str[cpp] = 0;
                      color = NULL;
                      ns = 0;

                      while (color == NULL)
                        {
                          if (strcmp (pixel_str, colors[ns].color_string) == 0)
                            color = &colors[ns];
                          else
                            ns++;
                        }

                      gdk_image_put_pixel (image, xcnt, ycnt, color->color.pixel);

		      if (mask && color->transparent)
			{
			  if (cnt < xcnt)
			    gdk_draw_line (*mask, gc, cnt, ycnt, xcnt - 1, ycnt);
			  cnt = xcnt + 1;
			}
                    }

		  if (mask && (cnt < xcnt))
		    gdk_draw_line (*mask, gc, cnt, ycnt, xcnt - 1, ycnt);
                }

	      if (mask)
		gdk_gc_destroy (gc);

              pixmap = gdk_pixmap_new (window, width, height, visual->depth);

              gc = gdk_gc_new (pixmap);
              gdk_gc_set_foreground (gc, transparent_color);
              gdk_draw_image (pixmap, gc, image, 0, 0, 0, 0, image->width, image->height);
              gdk_gc_destroy (gc);
              gdk_image_destroy (image);
            }
        }

      fclose (infile);
      free (buffer);

      if (colors != NULL)
        {
          for (cnt = 0; cnt < num_cols; cnt++)
            g_free (colors[cnt].color_string);
          g_free (colors);
        }
    }

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_xpm_d (GdkWindow  *window,
			      GdkBitmap **mask,
			      GdkColor   *transparent_color,
			      gchar     **data)
{
  GdkPixmap *pixmap = NULL;
  GdkImage *image = NULL;
  GdkColormap *colormap;
  GdkVisual *visual;
  GdkGC *gc;
  GdkColor tmp_color;
  gint width, height, num_cols, cpp, cnt, n, ns, xcnt, ycnt, i;
  gchar *buffer, *color_name = NULL, pixel_str[32];
  _GdkPixmapColor *colors = NULL, *color = NULL;
  gulong index;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  i = 0;
  buffer = data[i++];
  sscanf (buffer,"%d %d %d %d", &width, &height, &num_cols, &cpp);

  colors = g_new(_GdkPixmapColor, num_cols);

  colormap = gdk_window_get_colormap (window);
  visual = gdk_window_get_visual (window);

  if (transparent_color == NULL) 
    {
      gdk_color_white (colormap, &tmp_color);
      transparent_color = &tmp_color;
    }

  for (cnt = 0; cnt < num_cols; cnt++)
    {
      buffer = data[i++];

      colors[cnt].color_string = g_new(gchar, cpp + 1);
      for (n = 0; n < cpp; n++)
	colors[cnt].color_string[n] = buffer[n];
      colors[cnt].color_string[n] = 0;
      colors[cnt].transparent = FALSE;

      if (color_name != NULL)
	g_free (color_name);

      color_name = gdk_pixmap_extract_color (&buffer[cpp]);

      if (color_name != NULL)
	{
	  if (gdk_color_parse (color_name, &colors[cnt].color) == FALSE)
	    {
	      colors[cnt].color = *transparent_color;
	      colors[cnt].transparent = TRUE;
	    }
	}
      else
	{
	  colors[cnt].color = *transparent_color;
	  colors[cnt].transparent = TRUE;
	}

      gdk_color_alloc (colormap, &colors[cnt].color);
    }

  index = 0;
  image = gdk_image_new (GDK_IMAGE_FASTEST, visual, width, height);

  gc = NULL;
  if (mask)
    {
      /* The pixmap mask is just a bits pattern.
       * Color 0 is used for background and 1 for foreground.
       * We don't care about the colormap, we just need 0 and 1.
       */
      GdkColor mask_pattern;

      *mask = gdk_pixmap_new (window, width, height, 1);
      gc = gdk_gc_new (*mask);

      mask_pattern.pixel = 0;
      gdk_gc_set_foreground (gc, &mask_pattern);
      gdk_draw_rectangle (*mask, gc, TRUE, 0, 0, -1, -1);

      mask_pattern.pixel = 1;
      gdk_gc_set_foreground (gc, &mask_pattern);
    }

  for (ycnt = 0; ycnt < height; ycnt++)
    {
      buffer = data[i++];

      for (n = 0, cnt = 0, xcnt = 0; n < (width * cpp); n += cpp, xcnt++)
	{
	  strncpy (pixel_str, &buffer[n], cpp);
	  pixel_str[cpp] = 0;
	  color = NULL;
	  ns = 0;

	  while (color == NULL)
	    {
	      if (strcmp (pixel_str, colors[ns].color_string) == 0)
		color = &colors[ns];
	      else
		ns++;
	    }

	  gdk_image_put_pixel (image, xcnt, ycnt, color->color.pixel);

	  if (mask && color->transparent)
	    {
	      if (cnt < xcnt)
		gdk_draw_line (*mask, gc, cnt, ycnt, xcnt - 1, ycnt);
	      cnt = xcnt + 1;
	    }
	}

      if (mask && (cnt < xcnt))
	gdk_draw_line (*mask, gc, cnt, ycnt, xcnt - 1, ycnt);
    }

  if (mask)
    gdk_gc_destroy (gc);

  pixmap = gdk_pixmap_new (window, width, height, visual->depth);

  gc = gdk_gc_new (pixmap);
  gdk_gc_set_foreground (gc, transparent_color);
  gdk_draw_image (pixmap, gc, image, 0, 0, 0, 0, image->width, image->height);
  gdk_gc_destroy (gc);
  gdk_image_destroy (image);

  if (colors != NULL)
    {
      for (cnt = 0; cnt < num_cols; cnt++)
	g_free (colors[cnt].color_string);
      g_free (colors);
    }

  return pixmap;
}

GdkPixmap*
gdk_pixmap_ref (GdkPixmap *pixmap)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)pixmap;
  g_return_val_if_fail (pixmap != NULL, NULL);

  private->ref_count += 1;
  return pixmap;
}

void
gdk_pixmap_unref (GdkPixmap *pixmap)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)pixmap;
  g_return_if_fail(pixmap != NULL);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    {
      XFreePixmap (private->xdisplay, private->xwindow);
      gdk_xid_table_remove (private->xwindow);
      g_free (private);
    }
}

GdkBitmap *
gdk_bitmap_ref (GdkBitmap *bitmap)
{
  return (GdkBitmap *)gdk_pixmap_ref ((GdkPixmap *)bitmap);
}

void
gdk_bitmap_unref (GdkBitmap *bitmap)
{
  gdk_pixmap_unref ((GdkPixmap *)bitmap);
}
