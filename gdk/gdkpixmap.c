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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
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

typedef struct
{
  guint ncolors;
  GdkColormap *colormap;
  gulong pixels[1];
} _GdkPixmapInfo;

GdkPixmap*
gdk_pixmap_new (GdkWindow *window,
		gint       width,
		gint       height,
		gint       depth)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;

  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return NULL;

  if (depth == -1)
    depth = gdk_window_get_visual (window)->depth;

  private = g_new0 (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;

  private->xdisplay = window_private->xdisplay;
  private->window_type = GDK_WINDOW_PIXMAP;
  private->xwindow = XCreatePixmap (private->xdisplay, window_private->xwindow,
				    width, height, depth);
  private->colormap = NULL;
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
gdk_bitmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return NULL;

  private = g_new0 (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;

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
					    (char *)data, width, height);

  gdk_xid_table_insert (&private->xwindow, pixmap);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height,
			     gint         depth,
			     GdkColor    *fg,
			     GdkColor    *bg)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return NULL;

  if (depth == -1)
    depth = gdk_window_get_visual (window)->depth;

  private = g_new0 (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;

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
						  (char *)data, width, height,
						  fg->pixel, bg->pixel, depth);

  gdk_xid_table_insert (&private->xwindow, pixmap);

  return pixmap;
}

static gint
gdk_pixmap_seek_string (FILE  *infile,
                        const gchar *str,
                        gint   skip_comments)
{
  char instr[1024];

  while (1)
    {
      if (fscanf (infile, "%1023s", instr) != 1)
	return FALSE;
	  
      if (skip_comments == TRUE && strcmp (instr, "/*") == 0)
        {
	  do
	    {
	      if (fscanf (infile, "%1023s", instr) != 1)
		return FALSE;
	    }
	  while (strcmp (instr, "*/") != 0);
        }
      else if (strcmp (instr, str) == 0)
        return TRUE;
    }
}

static gint
gdk_pixmap_seek_char (FILE  *infile,
                      gchar  c)
{
  gint b, oldb;

  while ((b = getc(infile)) != EOF)
    {
      if (c != b && b == '/')
	{
	  b = getc (infile);
	  if (b == EOF)
	    return FALSE;
	  else if (b == '*')	/* we have a comment */
 	    {
	      b = -1;
	      do
 		{
 		  oldb = b;
		  b = getc (infile);
 		  if (b == EOF)
 		    return FALSE;
 		}
 	      while (!(oldb == '*' && b == '/'));
 	    }
        }
      else if (c == b)
 	return TRUE;
    }
  return FALSE;
}

static gint
gdk_pixmap_read_string (FILE  *infile,
                        gchar **buffer,
			guint *buffer_size)
{
  gint c;
  guint cnt = 0, bufsiz, ret = FALSE;
  gchar *buf;

  buf = *buffer;
  bufsiz = *buffer_size;
  if (buf == NULL)
    {
      bufsiz = 10 * sizeof (gchar);
      buf = g_new(gchar, bufsiz);
    }

  do
    c = getc (infile);
  while (c != EOF && c != '"');

  if (c != '"')
    goto out;

  while ((c = getc(infile)) != EOF)
    {
      if (cnt == bufsiz)
	{
	  guint new_size = bufsiz * 2;
	  if (new_size > bufsiz)
	    bufsiz = new_size;
	  else
	    goto out;
	  
 	  buf = (gchar *) g_realloc (buf, bufsiz);
	  buf[bufsiz-1] = '\0';
	}

      if (c != '"')
        buf[cnt++] = c;
      else
        {
          buf[cnt] = 0;
	  ret = TRUE;
	  break;
        }
    }

 out:
  buf[bufsiz-1] = '\0';		/* ensure null termination for errors */
  *buffer = buf;
  *buffer_size = bufsiz;
  return ret;
}

static gchar*
gdk_pixmap_skip_whitespaces (gchar *buffer)
{
  gint32 index = 0;

  while (buffer[index] != 0 && (buffer[index] == 0x20 || buffer[index] == 0x09))
    index++;

  return &buffer[index];
}

static gchar*
gdk_pixmap_skip_string (gchar *buffer)
{
  gint32 index = 0;

  while (buffer[index] != 0 && buffer[index] != 0x20 && buffer[index] != 0x09)
    index++;

  return &buffer[index];
}

/* Xlib crashed ince at a color name lengths around 125 */
#define MAX_COLOR_LEN 120

static gchar*
gdk_pixmap_extract_color (gchar *buffer)
{
  gint counter, numnames;
  gchar *ptr = NULL, ch, temp[128];
  gchar color[MAX_COLOR_LEN], *retcol;
  gint space;

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

  ptr = gdk_pixmap_skip_whitespaces (ptr);

  if (ptr[0] == 0)
    return NULL;
  else if (ptr[0] == '#')
    {
      counter = 1;
      while (ptr[counter] != 0 && 
             ((ptr[counter] >= '0' && ptr[counter] <= '9') ||
              (ptr[counter] >= 'a' && ptr[counter] <= 'f') ||
              (ptr[counter] >= 'A' && ptr[counter] <= 'F')))
        counter++;

      retcol = g_new (gchar, counter+1);
      strncpy (retcol, ptr, counter);

      retcol[counter] = 0;
      
      return retcol;
    }

  color[0] = 0;
  numnames = 0;

  space = MAX_COLOR_LEN - 1;
  while (space > 0)
    {
      sscanf (ptr, "%127s", temp);

      if (((gint)ptr[0] == 0) ||
	  (strcmp ("s", temp) == 0) || (strcmp ("m", temp) == 0) ||
          (strcmp ("g", temp) == 0) || (strcmp ("g4", temp) == 0))
	{
	  break;
	}
      else
        {
          if (numnames > 0)
	    {
	      space -= 1;
	      strcat (color, " ");
	    }
	  strncat (color, temp, space);
	  space -= MIN (space, strlen (temp));
          ptr = gdk_pixmap_skip_string (ptr);
          ptr = gdk_pixmap_skip_whitespaces (ptr);
          numnames++;
        }
    }

  retcol = g_strdup (color);
  return retcol;
}


enum buffer_op
{
  op_header,
  op_cmap,
  op_body
};
  

static void 
gdk_xpm_destroy_notify (gpointer data)
{
  _GdkPixmapInfo *info = (_GdkPixmapInfo *)data;
  GdkColor color;
  int i;

  for (i=0; i<info->ncolors; i++)
    {
      color.pixel = info->pixels[i];
      gdk_colormap_free_colors (info->colormap, &color, 1);
    }

  gdk_colormap_unref (info->colormap);
  g_free (info);
}
  
static GdkPixmap *
_gdk_pixmap_create_from_xpm (GdkWindow  *window,
			     GdkColormap *colormap,
			     GdkBitmap **mask,
			     GdkColor   *transparent_color,
			     gchar *   (*get_buf) (enum buffer_op op,
						   gpointer       handle),
			     gpointer    handle)
{
  GdkPixmap *pixmap = NULL;
  GdkImage *image = NULL;
  GdkVisual *visual;
  GdkGC *gc = NULL;
  GdkColor tmp_color;
  gint width, height, num_cols, cpp, n, ns, cnt, xcnt, ycnt, wbytes;
  gchar *buffer, pixel_str[32];
  gchar *name_buf;
  _GdkPixmapColor *color = NULL, *fallbackcolor = NULL;
  _GdkPixmapColor *colors = NULL;
  gulong index;
  GHashTable *color_hash = NULL;
  _GdkPixmapInfo *color_info = NULL;
  
  if ((window == NULL) && (colormap == NULL))
    g_warning ("Creating pixmap from xpm with NULL window and colormap");
  
  if (window == NULL)
    window = (GdkWindow *)&gdk_root_parent;
  
  if (colormap == NULL)
    {
      colormap = gdk_window_get_colormap (window);
      visual = gdk_window_get_visual (window);
    }
  else
    visual = ((GdkColormapPrivate *)colormap)->visual;
  
  buffer = (*get_buf) (op_header, handle);
  if (buffer == NULL)
    return NULL;
  
  sscanf (buffer,"%d %d %d %d", &width, &height, &num_cols, &cpp);
  if (cpp >= 32)
    {
      g_warning ("Pixmap has more than 31 characters per color\n");
      return NULL;
    }
  
  color_hash = g_hash_table_new (g_str_hash, g_str_equal);
  
  if (transparent_color == NULL)
    {
      gdk_color_white (colormap, &tmp_color);
      transparent_color = &tmp_color;
    }

  /* For pseudo-color and grayscale visuals, we have to remember
   * the colors we allocated, so we can free them later.
   */
  if ((visual->type == GDK_VISUAL_PSEUDO_COLOR) ||
      (visual->type == GDK_VISUAL_GRAYSCALE))
    {
      color_info = g_malloc (sizeof (_GdkPixmapInfo) + 
			     sizeof(gulong) * (num_cols - 1));
      color_info->ncolors = num_cols;
      color_info->colormap = colormap;
      gdk_colormap_ref (colormap);
    }

  name_buf = g_new (gchar, num_cols * (cpp+1));
  colors = g_new (_GdkPixmapColor, num_cols);

  for (cnt = 0; cnt < num_cols; cnt++)
    {
      gchar *color_name;
      
      buffer = (*get_buf) (op_cmap, handle);
      if (buffer == NULL)
	goto error;
      
      color = &colors[cnt];
      color->color_string = &name_buf [cnt * (cpp + 1)];
      strncpy (color->color_string, buffer, cpp);
      color->color_string[cpp] = 0;
      buffer += strlen (color->color_string);
      color->transparent = FALSE;
      
      color_name = gdk_pixmap_extract_color (buffer);
      
      if (color_name == NULL || g_strcasecmp (color_name, "None") == 0 ||
	  gdk_color_parse (color_name, &color->color) == FALSE)
	{
	  color->color = *transparent_color;
	  color->transparent = TRUE;
	}
      
      g_free (color_name);
      
      /* FIXME: The remaining slowness appears to happen in this
         function. */
      gdk_color_alloc (colormap, &color->color);

      if (color_info)
	color_info->pixels[cnt] = color->color.pixel;
      
      g_hash_table_insert (color_hash, color->color_string, color);
      if (cnt == 0)
	fallbackcolor = color;
    }
  
  index = 0;
  image = gdk_image_new (GDK_IMAGE_FASTEST, visual, width, height);
  
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
  
  wbytes = width * cpp;
  for (ycnt = 0; ycnt < height; ycnt++)
    {
      buffer = (*get_buf) (op_body, handle);
      
      /* FIXME: this slows things down a little - it could be
       * integrated into the strncpy below, perhaps. OTOH, strlen
       * is fast.
       */
      if ((buffer == NULL) || strlen (buffer) < wbytes)
	continue;
      
      for (n = 0, cnt = 0, xcnt = 0; n < wbytes; n += cpp, xcnt++)
	{
	  strncpy (pixel_str, &buffer[n], cpp);
	  pixel_str[cpp] = 0;
	  ns = 0;
	  
	  color = g_hash_table_lookup (color_hash, pixel_str);
	  
	  if (!color) /* screwed up XPM file */
	    color = fallbackcolor;
	  
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
  
 error:
  
  if (mask)
    gdk_gc_destroy (gc);
  
  if (image != NULL)
    {
      pixmap = gdk_pixmap_new (window, width, height, visual->depth);

      if (color_info)
	gdk_drawable_set_data (pixmap, "gdk-xpm", color_info, 
			       gdk_xpm_destroy_notify);
      
      gc = gdk_gc_new (pixmap);
      gdk_gc_set_foreground (gc, transparent_color);
      gdk_draw_image (pixmap, gc, image, 0, 0, 0, 0, image->width, image->height);
      gdk_gc_destroy (gc);
      gdk_image_destroy (image);
    }
  else if (color_info)
    gdk_xpm_destroy_notify (color_info);
  
  if (color_hash != NULL)
    g_hash_table_destroy (color_hash);

  if (colors != NULL)
    g_free (colors);

  if (name_buf != NULL)
    g_free (name_buf);

  return pixmap;
}


struct file_handle
{
  FILE *infile;
  gchar *buffer;
  guint buffer_size;
};


static gchar *
file_buffer (enum buffer_op op, gpointer handle)
{
  struct file_handle *h = handle;

  switch (op)
    {
    case op_header:
      if (gdk_pixmap_seek_string (h->infile, "XPM", FALSE) != TRUE)
	break;

      if (gdk_pixmap_seek_char (h->infile,'{') != TRUE)
	break;
      /* Fall through to the next gdk_pixmap_seek_char. */

    case op_cmap:
      gdk_pixmap_seek_char (h->infile, '"');
      fseek (h->infile, -1, SEEK_CUR);
      /* Fall through to the gdk_pixmap_read_string. */

    case op_body:
      gdk_pixmap_read_string (h->infile, &h->buffer, &h->buffer_size);
      return h->buffer;
    }
  return 0;
}


GdkPixmap*
gdk_pixmap_colormap_create_from_xpm (GdkWindow   *window,
				     GdkColormap *colormap,
				     GdkBitmap  **mask,
				     GdkColor    *transparent_color,
				     const gchar *filename)
{
  struct file_handle h;
  GdkPixmap *pixmap = NULL;

  memset (&h, 0, sizeof (h));
  h.infile = fopen (filename, "rb");
  if (h.infile != NULL)
    {
      pixmap = _gdk_pixmap_create_from_xpm (window, colormap, mask,
					    transparent_color,
					    file_buffer, &h);
      fclose (h.infile);
      g_free (h.buffer);
    }

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_xpm (GdkWindow  *window,
			    GdkBitmap **mask,
			    GdkColor   *transparent_color,
			    const gchar *filename)
{
  return gdk_pixmap_colormap_create_from_xpm (window, NULL, mask,
				       transparent_color, filename);
}


struct mem_handle
{
  gchar **data;
  int offset;
};


static gchar *
mem_buffer (enum buffer_op op, gpointer handle)
{
  struct mem_handle *h = handle;
  switch (op)
    {
    case op_header:
    case op_cmap:
    case op_body:
      if (h->data[h->offset])
	return h->data[h->offset ++];
    }
  return 0;
}


GdkPixmap*
gdk_pixmap_colormap_create_from_xpm_d (GdkWindow  *window,
				       GdkColormap *colormap,
				       GdkBitmap **mask,
				       GdkColor   *transparent_color,
				       gchar     **data)
{
  struct mem_handle h;
  GdkPixmap *pixmap = NULL;

  memset (&h, 0, sizeof (h));
  h.data = data;
  pixmap = _gdk_pixmap_create_from_xpm (window, colormap, mask,
					transparent_color,
					mem_buffer, &h);
  return pixmap;
}


GdkPixmap*
gdk_pixmap_create_from_xpm_d (GdkWindow  *window,
			      GdkBitmap **mask,
			      GdkColor   *transparent_color,
			      gchar     **data)
{
  return gdk_pixmap_colormap_create_from_xpm_d (window, NULL, mask,
						transparent_color, data);
}

GdkPixmap*
gdk_pixmap_foreign_new (guint32 anid)
{
  GdkPixmap *pixmap;
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *private;
  Pixmap xpixmap;
  Window root_return;
  unsigned int x_ret, y_ret, w_ret, h_ret, bw_ret, depth_ret;

  /* check to make sure we were passed something at
     least a little sane */
  g_return_val_if_fail((anid != 0), NULL);
  
  /* set the pixmap to the passed in value */
  xpixmap = anid;
  /* get the root window */
  window_private = &gdk_root_parent;

  /* get information about the Pixmap to fill in the structure for
     the gdk window */
  if (!XGetGeometry(window_private->xdisplay, xpixmap, &root_return,
		    &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
      return NULL;
      
  /* allocate a new gdk pixmap */
  private = g_new(GdkWindowPrivate, 1);
  pixmap = (GdkPixmap *)private;

  private->xdisplay = window_private->xdisplay;
  private->window_type = GDK_WINDOW_PIXMAP;
  private->xwindow = xpixmap;
  private->colormap = NULL;
  private->parent = NULL;
  private->x = 0;
  private->y = 0;
  private->width = w_ret;
  private->height = h_ret;
  private->resize_count = 0;
  private->ref_count = 1;
  private->destroyed = 0;
  
  gdk_xid_table_insert(&private->xwindow, pixmap);

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
  g_return_if_fail (pixmap != NULL);
  g_return_if_fail (private->ref_count > 0);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    {
      XFreePixmap (private->xdisplay, private->xwindow);
      gdk_xid_table_remove (private->xwindow);
      g_dataset_destroy (private);
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
