/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "gdk.h"
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkprivate-fb.h"

static gpointer parent_class = NULL;

#define GDK_IMAGE_PRIVATE_DATA(image) ((GdkImagePrivateFB *) GDK_IMAGE (image)->windowing_data)

void
_gdk_windowing_image_init (void)
{
}

static void
gdk_image_init (GdkImage *image)
{
  image->windowing_data = g_new0 (GdkImagePrivateFB, 1);
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  g_free (image->windowing_data);
  image->windowing_data = NULL;
  
  g_free (image->mem);
  image->mem = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_image_finalize;
}

GType
gdk_image_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkImageClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkImage),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_image_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkImage",
                                            &object_info,
					    0);
    }
  
  return object_type;
}


GdkImage*
_gdk_image_new_for_depth (GdkScreen    *screen,
			  GdkImageType  type,
			  GdkVisual    *visual,
			  gint          width,
			  gint          height,
			  gint          depth)
{
  GdkImage *image;
  GdkImagePrivateFB *private;

  g_return_val_if_fail (!visual || GDK_IS_VISUAL (visual), NULL);
  g_return_val_if_fail (visual || depth != -1, NULL);

  if (visual)
    depth = visual->depth;

  image = g_object_new (gdk_image_get_type (), NULL);
  private = GDK_IMAGE_PRIVATE_DATA (image);

  image->type = 0;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = depth;
  
  image->byte_order = 0;
  image->bits_per_pixel = image->depth;
  image->bpp = image->depth/8;
  image->bpl = (width * image->depth + 7) / 8;
  image->mem = g_malloc (image->bpl * height);

  return image;
}

GdkImage *
gdk_image_new_bitmap(GdkVisual *visual,
		     gpointer data,
		     gint w,
		     gint h)
{
  GdkImage *image;
  GdkImagePrivateFB *private;
  
  image = g_object_new (gdk_image_get_type (), NULL);
  private = GDK_IMAGE_PRIVATE_DATA (image);
  
  image->type = GDK_IMAGE_NORMAL;
  image->visual = visual;
  image->width = w;
  image->height = h;
  image->depth = 1;

  image->byte_order = 1 /* MSBFirst */;
  image->bits_per_pixel = 1;
  image->bpp = 1;
  image->bpl = (w+7)/8;
  image->mem = g_malloc (image->bpl * h / 8);
  memcpy (image->mem, data, image->bpl * h / 8);

  /* This must be freed using free, not g_free, since in the X version
     this is freed by xlib. */
  free (data); 
  
  return image;
}

GdkImage*
_gdk_fb_copy_to_image (GdkDrawable    *drawable,
		       GdkImage       *image,
		       gint            src_x,
		       gint            src_y,
		       gint            dest_x,
		       gint            dest_y,
		       gint            width,
		       gint            height)
{
  GdkImagePrivateFB *private;
  GdkPixmapFBData fbd;
  GdkRegion *region = NULL;
  gboolean handle_cursor = FALSE;

  g_return_val_if_fail (drawable != NULL, NULL);

  if (image == NULL)
    {
      image = g_object_new (gdk_image_get_type (), NULL);

      image->type = GDK_IMAGE_NORMAL;
      image->visual = gdk_drawable_get_visual (drawable);
      image->width = width;
      image->height = height;
      image->bits_per_pixel = GDK_DRAWABLE_FBDATA (drawable)->depth;
      image->depth = image->bits_per_pixel;
      
      if (image->bits_per_pixel <= 8)
	image->bpp = 1;
      else if (image->bits_per_pixel <= 16)
	image->bpp = 2;
      else if (image->bits_per_pixel <= 24)
	image->bpp = 3;
      else
	image->bpp = 4;
      image->byte_order = 1;

      image->bpl = (image->width * image->depth + 7) / 8; /* Packed pixels */
      image->mem = g_malloc (image->bpl * image->height);

      dest_x = 0;
      dest_y = 0;
    }
  
  private = GDK_IMAGE_PRIVATE_DATA (image);
  
  /* Fake its existence as a pixmap */
  memset (&fbd, 0, sizeof(fbd));
  ((GTypeInstance *)&fbd)->g_class = g_type_class_peek (_gdk_pixmap_impl_get_type ());
  fbd.drawable_data.mem = image->mem;
  fbd.drawable_data.rowstride = image->bpl;
  fbd.drawable_data.width = fbd.drawable_data.lim_x = image->width;
  fbd.drawable_data.height = fbd.drawable_data.lim_y = image->height;
  fbd.drawable_data.depth = image->depth;
  fbd.drawable_data.window_type = GDK_DRAWABLE_PIXMAP;

  if (GDK_DRAWABLE_FBDATA (drawable)->mem == GDK_DRAWABLE_IMPL_FBDATA (_gdk_parent_root)->mem)
    {
      region = gdk_fb_clip_region (drawable, _gdk_fb_screen_gc, TRUE, FALSE, FALSE);

      if (gdk_fb_cursor_region_need_hide (region))
	{
	  handle_cursor = TRUE;
	  gdk_fb_cursor_hide ();
	}
    }

  gdk_fb_draw_drawable_2 ((GdkPixmap *)&fbd,
			  _gdk_fb_screen_gc,
			  drawable,
			  src_x, src_y,
			  dest_x, dest_y,
			  width, height,
			  TRUE, TRUE);

  if (region)
    gdk_region_destroy (region);
  
  if (handle_cursor)
    gdk_fb_cursor_unhide ();

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  GdkImagePrivateFB *private;

  g_return_val_if_fail (image != NULL, 0);
  g_return_val_if_fail (x >= 0 && x < image->width, 0);
  g_return_val_if_fail (y >= 0 && y < image->height, 0);

  private = GDK_IMAGE_PRIVATE_DATA (image);

  switch (image->depth)
    {
    case 8:
      return ((guchar *)image->mem)[x + y * image->bpl];
      break;
    case 16:
      return *((guint16 *)&((guchar *)image->mem)[x*2 + y*image->bpl]);
      break;
    case 24:
    case 32:
      {
	guchar *smem = &(((guchar *)image->mem)[x*image->bpp + y*image->bpl]);
	return smem[0]|(smem[1]<<8)|(smem[2]<<16);
      }
      break;
    }

  return 0;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  guchar *ptr = image->mem;

  g_return_if_fail (image != NULL);
  g_return_if_fail (x >= 0 && x < image->width);
  g_return_if_fail (y >= 0 && y < image->height);

  switch (image->depth)
    {
    case 8:
      ptr[x + y * image->bpl] = pixel;
      break;
    case 16:
      {
	guint16 *p16 = (guint16 *)&ptr[x*2 + y*image->bpl];
	*p16 = pixel;
      }
      break;
    case 24:
      {
	guchar *smem = &ptr[x*3 + y*image->bpl];
	smem[0] = (pixel & 0xFF);
	smem[1] = (pixel & 0xFF00) >> 8;
	smem[2] = (pixel & 0xFF0000) >> 16;
      }
      break;
    case 32:
      {
	guint32 *smem = (guint32 *)&ptr[x*4 + y*image->bpl];
	*smem = pixel;
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

void
_gdk_image_exit(void)
{
}

/* copy/pasted from gdkimage-win32.c */
gint
_gdk_windowing_get_bits_for_depth (GdkDisplay *display,
				   gint        depth)
{
  if ((1 == depth) || (8 == depth) || (16 == depth) || 
      (24 == depth) || (32 == depth))
    return depth;
  else if (15 == depth)
    return 16;
  else
    g_assert_not_reached ();

  return 0;	
}

