/* GIMP Drawing Kit
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

#include "config.h"

#include "gdkx.h"

#include <cairo-xlib.h>

#include <stdlib.h>
#include <string.h>		/* for memcpy() */

#if defined (HAVE_IPC_H) && defined (HAVE_SHM_H) && defined (HAVE_XSHM_H)
#define USE_SHM
#endif

#ifdef USE_SHM
#include <X11/extensions/XShm.h>
#endif /* USE_SHM */

#include "gdkprivate-x11.h"
#include "gdkdrawable-x11.h"
#include "gdkpixmap-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"


static cairo_surface_t *gdk_x11_ref_cairo_surface (GdkDrawable *drawable);
static cairo_surface_t *gdk_x11_create_cairo_surface (GdkDrawable *drawable,
                                                      int          width,
                                                      int          height);
     
static void gdk_x11_set_colormap   (GdkDrawable    *drawable,
                                    GdkColormap    *colormap);

static GdkColormap* gdk_x11_get_colormap   (GdkDrawable    *drawable);
static gint         gdk_x11_get_depth      (GdkDrawable    *drawable);
static GdkScreen *  gdk_x11_get_screen	   (GdkDrawable    *drawable);
static GdkVisual*   gdk_x11_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_x11_finalize   (GObject *object);

static const cairo_user_data_key_t gdk_x11_cairo_key;

G_DEFINE_TYPE (GdkDrawableImplX11, _gdk_drawable_impl_x11, GDK_TYPE_DRAWABLE)

static void
_gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = gdk_drawable_impl_x11_finalize;
  
  drawable_class->ref_cairo_surface = gdk_x11_ref_cairo_surface;
  drawable_class->create_cairo_surface = gdk_x11_create_cairo_surface;

  drawable_class->set_colormap = gdk_x11_set_colormap;
  drawable_class->get_colormap = gdk_x11_get_colormap;

  drawable_class->get_depth = gdk_x11_get_depth;
  drawable_class->get_screen = gdk_x11_get_screen;
  drawable_class->get_visual = gdk_x11_get_visual;
}

static void
_gdk_drawable_impl_x11_init (GdkDrawableImplX11 *impl)
{
}

static void
gdk_drawable_impl_x11_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (_gdk_drawable_impl_x11_parent_class)->finalize (object);
}

/**
 * _gdk_x11_drawable_finish:
 * @drawable: a #GdkDrawableImplX11.
 * 
 * Performs necessary cleanup prior to freeing a pixmap or
 * destroying a window.
 **/
void
_gdk_x11_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				   NULL, NULL);
    }
}

/**
 * _gdk_x11_drawable_update_size:
 * @drawable: a #GdkDrawableImplX11.
 * 
 * Updates the state of the drawable (in particular the drawable's
 * cairo surface) when its size has changed.
 **/
void
_gdk_x11_drawable_update_size (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (impl->cairo_surface)
    {
      int width, height;
      
      gdk_drawable_get_size (drawable, &width, &height);
      cairo_xlib_surface_set_size (impl->cairo_surface, width, height);
    }
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_x11_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  return impl->colormap;
}

static void
gdk_x11_set_colormap (GdkDrawable *drawable,
                      GdkColormap *colormap)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

static gint
gdk_x11_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

static GdkDrawable *
get_impl_drawable (GdkDrawable *drawable)
{
  if (GDK_IS_WINDOW (drawable))
    return ((GdkWindowObject *)drawable)->impl;
  else if (GDK_IS_PIXMAP (drawable))
    return ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return NULL;
    }
}

static GdkScreen*
gdk_x11_get_screen (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  else
    return GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen;
}

static GdkVisual*
gdk_x11_get_visual (GdkDrawable    *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

/**
 * gdk_x11_drawable_get_xdisplay:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the display of a #GdkDrawable.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_drawable_get_xdisplay (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (drawable)->screen);
  else
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen);
}

/**
 * gdk_x11_drawable_get_xid:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the X resource (window or pixmap) belonging to a #GdkDrawable.
 * 
 * Return value: the ID of @drawable's X resource.
 **/
XID
gdk_x11_drawable_get_xid (GdkDrawable *drawable)
{
  GdkDrawable *impl;
  
  if (GDK_IS_WINDOW (drawable))
    {
      GdkWindow *window = (GdkWindow *)drawable;
      
      /* Try to ensure the window has a native window */
      if (!_gdk_window_has_impl (window))
	{
	  gdk_window_ensure_native (window);

	  /* We sync here to ensure the window is created in the Xserver when
	   * this function returns. This is required because the returned XID
	   * for this window must be valid immediately, even with another
	   * connection to the Xserver */
	  gdk_display_sync (gdk_drawable_get_display (window));
	}
      
      if (!GDK_WINDOW_IS_X11 (window))
        {
          g_warning (G_STRLOC " drawable is not a native X11 window");
          return None;
        }
      
      impl = ((GdkWindowObject *)drawable)->impl;
    }
  else if (GDK_IS_PIXMAP (drawable))
    impl = ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return None;
    }

  return ((GdkDrawableImplX11 *)impl)->xid;
}

GdkDrawable *
gdk_x11_window_get_drawable_impl (GdkWindow *window)
{
  return ((GdkWindowObject *)window)->impl;
}
GdkDrawable *
gdk_x11_pixmap_get_drawable_impl (GdkPixmap *pixmap)
{
  return ((GdkPixmapObject *)pixmap)->impl;
}

#if 0
static void
list_formats (XRenderPictFormat *pf)
{
  gint i;
  
  for (i=0 ;; i++)
    {
      XRenderPictFormat *pf = XRenderFindFormat (impl->xdisplay, 0, NULL, i);
      if (pf)
	{
	  g_print ("%2d R-%#06x/%#06x G-%#06x/%#06x B-%#06x/%#06x A-%#06x/%#06x\n",
		   pf->depth,
		   pf->direct.red,
		   pf->direct.redMask,
		   pf->direct.green,
		   pf->direct.greenMask,
		   pf->direct.blue,
		   pf->direct.blueMask,
		   pf->direct.alpha,
		   pf->direct.alphaMask);
	}
      else
	break;
    }
}
#endif  

void
_gdk_x11_convert_to_format (guchar           *src_buf,
                            gint              src_rowstride,
                            guchar           *dest_buf,
                            gint              dest_rowstride,
                            GdkX11FormatType  dest_format,
                            GdkByteOrder      dest_byteorder,
                            gint              width,
                            gint              height)
{
  gint i;

  for (i=0; i < height; i++)
    {
      switch (dest_format)
	{
	case GDK_X11_FORMAT_EXACT_MASK:
	  {
	    memcpy (dest_buf + i * dest_rowstride,
		    src_buf + i * src_rowstride,
		    width * 4);
	    break;
	  }
	case GDK_X11_FORMAT_ARGB_MASK:
	  {
	    guchar *row = src_buf + i * src_rowstride;
	    if (((gsize)row & 3) != 0)
	      {
		guchar *p = row;
		guint32 *q = (guint32 *)(dest_buf + i * dest_rowstride);
		guchar *end = p + 4 * width;

		while (p < end)
		  {
		    *q = (p[3] << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
		    p += 4;
		    q++;
		  }
	      }
	    else
	      {
		guint32 *p = (guint32 *)row;
		guint32 *q = (guint32 *)(dest_buf + i * dest_rowstride);
		guint32 *end = p + width;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN	    
		if (dest_byteorder == GDK_LSB_FIRST)
		  {
		    /* ABGR => ARGB */
		
		    while (p < end)
		      {
			*q = ( (*p & 0xff00ff00) |
			       ((*p & 0x000000ff) << 16) |
			       ((*p & 0x00ff0000) >> 16));
			q++;
			p++;
		      }
		  }
		else
		  {
		    /* ABGR => BGRA */
		
		    while (p < end)
		      {
			*q = (((*p & 0xff000000) >> 24) |
			      ((*p & 0x00ffffff) << 8));
			q++;
			p++;
		      }
		  }
#else /* G_BYTE_ORDER == G_BIG_ENDIAN */
		if (dest_byteorder == GDK_LSB_FIRST)
		  {
		    /* RGBA => BGRA */
		
		    while (p < end)
		      {
			*q = ( (*p & 0x00ff00ff) |
			       ((*p & 0x0000ff00) << 16) |
			       ((*p & 0xff000000) >> 16));
			q++;
			p++;
		      }
		  }
		else
		  {
		    /* RGBA => ARGB */
		
		    while (p < end)
		      {
			*q = (((*p & 0xffffff00) >> 8) |
			      ((*p & 0x000000ff) << 24));
			q++;
			p++;
		      }
		  }
#endif /* G_BYTE_ORDER*/	    
	      }
	    break;
	  }
	case GDK_X11_FORMAT_ARGB:
	  {
	    guchar *p = (src_buf + i * src_rowstride);
	    guchar *q = (dest_buf + i * dest_rowstride);
	    guchar *end = p + 4 * width;
	    guint t1,t2,t3;
	    
#define MULT(d,c,a,t) G_STMT_START { t = c * a; d = ((t >> 8) + t) >> 8; } G_STMT_END
	    
	    if (dest_byteorder == GDK_LSB_FIRST)
	      {
		while (p < end)
		  {
		    MULT(q[0], p[2], p[3], t1);
		    MULT(q[1], p[1], p[3], t2);
		    MULT(q[2], p[0], p[3], t3);
		    q[3] = p[3];
		    p += 4;
		    q += 4;
		  }
	      }
	    else
	      {
		while (p < end)
		  {
		    q[0] = p[3];
		    MULT(q[1], p[0], p[3], t1);
		    MULT(q[2], p[1], p[3], t2);
		    MULT(q[3], p[2], p[3], t3);
		    p += 4;
		    q += 4;
		  }
	      }
#undef MULT
	    break;
	  }
	case GDK_X11_FORMAT_NONE:
	  g_assert_not_reached ();
	  break;
	}
    }
}

static void
gdk_x11_cairo_surface_destroy (void *data)
{
  GdkDrawableImplX11 *impl = data;

  impl->cairo_surface = NULL;
}

gboolean
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
				       int width,
				       int height)
{
  cairo_xlib_surface_set_size (surface, width, height);
  return TRUE;
}

static cairo_surface_t *
gdk_x11_create_cairo_surface (GdkDrawable *drawable,
			      int width,
			      int height)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  GdkVisual *visual;
    
  visual = gdk_drawable_get_visual (drawable);
  if (visual) 
    return cairo_xlib_surface_create (GDK_SCREEN_XDISPLAY (impl->screen),
				      impl->xid,
				      GDK_VISUAL_XVISUAL (visual),
				      width, height);
  else if (gdk_drawable_get_depth (drawable) == 1)
    return cairo_xlib_surface_create_for_bitmap (GDK_SCREEN_XDISPLAY (impl->screen),
						    impl->xid,
						    GDK_SCREEN_XSCREEN (impl->screen),
						    width, height);
  else
    {
      g_warning ("Using Cairo rendering requires the drawable argument to\n"
		 "have a specified colormap. All windows have a colormap,\n"
		 "however, pixmaps only have colormap by default if they\n"
		 "were created with a non-NULL window argument. Otherwise\n"
		 "a colormap must be set on them with gdk_drawable_set_colormap");
      return NULL;
    }
  
}

static cairo_surface_t *
gdk_x11_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (GDK_IS_WINDOW_IMPL_X11 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      int width, height;
  
      gdk_drawable_get_size (impl->wrapper, &width, &height);

      impl->cairo_surface = gdk_x11_create_cairo_surface (drawable, width, height);
      
      if (impl->cairo_surface)
	cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				     drawable, gdk_x11_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}
