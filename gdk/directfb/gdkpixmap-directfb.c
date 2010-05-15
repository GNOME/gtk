/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"

#include <stdlib.h>
#include <string.h>

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"

#include "gdkpixmap.h"
#include "gdkalias.h"


static void gdk_pixmap_impl_directfb_init       (GdkPixmapImplDirectFB      *pixmap);
static void gdk_pixmap_impl_directfb_class_init (GdkPixmapImplDirectFBClass *klass);
static void gdk_pixmap_impl_directfb_finalize   (GObject                    *object);


static gpointer parent_class = NULL;

G_DEFINE_TYPE (GdkPixmapImplDirectFB,
               gdk_pixmap_impl_directfb,
               GDK_TYPE_DRAWABLE_IMPL_DIRECTFB);

GType
_gdk_pixmap_impl_get_type (void)
{
  return gdk_pixmap_impl_directfb_get_type ();
}

static void
gdk_pixmap_impl_directfb_init (GdkPixmapImplDirectFB *impl)
{
  GdkDrawableImplDirectFB *draw_impl = GDK_DRAWABLE_IMPL_DIRECTFB (impl);
  draw_impl->width  = 1;
  draw_impl->height = 1;
}

static void
gdk_pixmap_impl_directfb_class_init (GdkPixmapImplDirectFBClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_directfb_finalize;
}

static void
gdk_pixmap_impl_directfb_finalize (GObject *object)
{
  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

GdkPixmap *
_gdk_pixmap_new (GdkDrawable *drawable,
                 gint       width,
                 gint       height,
                 gint       depth)
{
  DFBSurfacePixelFormat    format;
  IDirectFBSurface        *surface;
  GdkPixmap               *pixmap;
  GdkDrawableImplDirectFB *draw_impl;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (drawable != NULL || depth != -1, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  if (!drawable)
    drawable = _gdk_parent_root;

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  GDK_NOTE (MISC, g_print ("gdk_pixmap_new: %dx%dx%d\n",
                           width, height, depth));

  if (depth == -1)
    depth = gdk_drawable_get_depth (GDK_DRAWABLE (drawable));

  switch (depth)
    {
    case  1:
      format = DSPF_A8;
      break;
    case  8:
      format = DSPF_LUT8;
      break;
    case 15:
      format = DSPF_ARGB1555;
      break;
    case 16:
      format = DSPF_RGB16;
      break;
    case 24:
      format = DSPF_RGB24;
      break;
    case 32:
      format = DSPF_RGB32;
      break;
    default:
      g_message ("unimplemented %s for depth %d", G_STRFUNC, depth);
      return NULL;
    }

  if (!(surface =
	gdk_display_dfb_create_surface (_gdk_display, format, width, height))) {
    g_assert (surface != NULL);
    return NULL;
  }

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->surface = surface;
  surface->Clear (surface, 0x0, 0x0, 0x0, 0x0);
  surface->GetSize (surface, &draw_impl->width, &draw_impl->height);
  surface->GetPixelFormat (surface, &draw_impl->format);

  draw_impl->abs_x = draw_impl->abs_y = 0;

  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  return pixmap;
}

GdkPixmap *
_gdk_bitmap_create_from_data (GdkDrawable *drawable,
                              const gchar *data,
                              gint         width,
                              gint         height)
{
  GdkPixmap *pixmap;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  GDK_NOTE (MISC, g_print ("gdk_bitmap_create_from_data: %dx%d\n",
                           width, height));

  pixmap = gdk_pixmap_new (drawable, width, height, 1);

#define GET_PIXEL(data,pixel)                                           \
  ((data[(pixel / 8)] & (0x1 << ((pixel) % 8))) >> ((pixel) % 8))

  if (pixmap)
    {
      guchar *dst;
      gint    pitch;

      IDirectFBSurface *surface;

      surface = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (pixmap)->impl)->surface;

      if (surface->Lock (surface, DSLF_WRITE, (void**)(&dst), &pitch) == DFB_OK)
        {
          gint i, j;

          for (i = 0; i < height; i++)
            {
	      for (j = 0; j < width; j++)
		{
		  dst[j] = GET_PIXEL (data, j) * 255;
		}

              data += (width + 7) / 8;
	      dst += pitch;
            }

          surface->Unlock (surface);
        }
    }

#undef GET_PIXEL

  return pixmap;
}

GdkPixmap *
_gdk_pixmap_create_from_data (GdkDrawable    *drawable,
                              const gchar    *data,
                              gint            width,
                              gint            height,
                              gint            depth,
                              const GdkColor *fg,
                              const GdkColor *bg)
{
  GdkPixmap *pixmap;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (drawable != NULL || depth > 0, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  GDK_NOTE (MISC, g_print ("gdk_pixmap_create_from_data: %dx%dx%d\n",
                           width, height, depth));

  pixmap = gdk_pixmap_new (drawable, width, height, depth);

  if (pixmap)
    {
      IDirectFBSurface *surface;
      gchar            *dst;
      gint              pitch;
      gint              src_pitch;

      depth = gdk_drawable_get_depth (pixmap);
      src_pitch = width * ((depth + 7) / 8);

      surface = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (pixmap)->impl)->surface;

      if (surface->Lock (surface,
                         DSLF_WRITE, (void**)(&dst), &pitch) == DFB_OK)
        {
          gint i;

          for (i = 0; i < height; i++)
            {
              memcpy (dst, data, src_pitch);
              dst += pitch;
              data += src_pitch;
            }

          surface->Unlock (surface);
        }
    }

  return pixmap;
}

GdkPixmap *
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
  g_warning (" gdk_pixmap_foreign_new unsuporrted \n");
  return NULL;
}

GdkPixmap *
gdk_pixmap_foreign_new_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  return gdk_pixmap_foreign_new (anid);
}

GdkPixmap *
gdk_pixmap_foreign_new_for_screen (GdkScreen       *screen,
                                   GdkNativeWindow  anid,
                                   gint             width,
                                   gint             height,
                                   gint             depth)
{
  /*Use the root drawable for now since only one screen */
  return gdk_pixmap_new (NULL, width, height, depth);
}


GdkPixmap *
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  g_warning (" gdk_pixmap_lookup unsuporrted \n");
  return NULL;
}

GdkPixmap *
gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  return gdk_pixmap_lookup (anid);
}

#define __GDK_PIXMAP_X11_C__
#include "gdkaliasdef.c"
