/* gdkdrawable-quartz.c
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"
#include <sys/time.h>
#include <cairo-quartz.h>
#include "gdkprivate-quartz.h"

static gpointer parent_class;

static cairo_user_data_key_t gdk_quartz_cairo_key;

typedef struct {
  GdkDrawable  *drawable;
  CGContextRef  cg_context;
} GdkQuartzCairoSurfaceData;

gboolean
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
				       int              width,
				       int              height)
{
  /* This is not supported with quartz surfaces. */
  return FALSE;
}

static void
gdk_quartz_cairo_surface_destroy (void *data)
{
  GdkQuartzCairoSurfaceData *surface_data = data;
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (surface_data->drawable);

  impl->cairo_surface = NULL;

  gdk_quartz_drawable_release_context (surface_data->drawable,
                                       surface_data->cg_context);

  g_free (surface_data);
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
				     int          width,
				     int          height)
{
  CGContextRef cg_context;
  GdkQuartzCairoSurfaceData *surface_data;
  cairo_surface_t *surface;

  cg_context = gdk_quartz_drawable_get_context (drawable, TRUE);

  if (!cg_context)
    return NULL;

  surface_data = g_new (GdkQuartzCairoSurfaceData, 1);
  surface_data->drawable = drawable;
  surface_data->cg_context = cg_context;

  surface = cairo_quartz_surface_create_for_cg_context (cg_context,
                                                        width, height);

  cairo_surface_set_user_data (surface, &gdk_quartz_cairo_key,
                               surface_data,
                               gdk_quartz_cairo_surface_destroy);

  return surface;
}

static cairo_surface_t *
gdk_quartz_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      int width, height;

      gdk_drawable_get_size (drawable, &width, &height);
      impl->cairo_surface = _gdk_windowing_create_cairo_surface (drawable,
                                                                 width, height);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static void
gdk_quartz_set_colormap (GdkDrawable *drawable,
			 GdkColormap *colormap)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

static GdkColormap*
gdk_quartz_get_colormap (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_IMPL_QUARTZ (drawable)->colormap;
}

static GdkScreen*
gdk_quartz_get_screen (GdkDrawable *drawable)
{
  return _gdk_screen;
}

static GdkVisual*
gdk_quartz_get_visual (GdkDrawable *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
}

static int
gdk_quartz_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
}

static void
gdk_drawable_impl_quartz_finalize (GObject *object)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (object);

  if (impl->colormap)
    g_object_unref (impl->colormap);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_drawable_impl_quartz_class_init (GdkDrawableImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_quartz_finalize;

  drawable_class->ref_cairo_surface = gdk_quartz_ref_cairo_surface;

  drawable_class->set_colormap = gdk_quartz_set_colormap;
  drawable_class->get_colormap = gdk_quartz_get_colormap;

  drawable_class->get_depth = gdk_quartz_get_depth;
  drawable_class->get_screen = gdk_quartz_get_screen;
  drawable_class->get_visual = gdk_quartz_get_visual;
}

GType
gdk_drawable_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkDrawableImplQuartzClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_quartz_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableImplQuartz),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplQuartz",
                                            &object_info, 0);
    }
  
  return object_type;
}

CGContextRef
gdk_quartz_drawable_get_context (GdkDrawable *drawable,
				 gboolean     antialias)
{
  if (!GDK_DRAWABLE_IMPL_QUARTZ_GET_CLASS (drawable)->get_context)
    {
      g_warning ("%s doesn't implement GdkDrawableImplQuartzClass::get_context()",
                 G_OBJECT_TYPE_NAME (drawable));
      return NULL;
    }

  return GDK_DRAWABLE_IMPL_QUARTZ_GET_CLASS (drawable)->get_context (drawable, antialias);
}

/* Help preventing "beam sync penalty" where CG makes all graphics code
 * block until the next vsync if we try to flush (including call display on
 * a view) too often. We do this by limiting the manual flushing done
 * outside of expose calls to less than some frequency when measured over
 * the last 4 flushes. This is a bit arbitray, but seems to make it possible
 * for some quick manual flushes (such as gtkruler or gimp's marching ants)
 * without hitting the max flush frequency.
 *
 * If drawable NULL, no flushing is done, only registering that a flush was
 * done externally.
 */
void
_gdk_quartz_drawable_flush (GdkDrawable *drawable)
{
  static struct timeval prev_tv;
  static gint intervals[4];
  static gint index;
  struct timeval tv;
  gint ms;

  gettimeofday (&tv, NULL);
  ms = (tv.tv_sec - prev_tv.tv_sec) * 1000 + (tv.tv_usec - prev_tv.tv_usec) / 1000;
  intervals[index++ % 4] = ms;

  if (drawable)
    {
      ms = intervals[0] + intervals[1] + intervals[2] + intervals[3];

      /* ~25Hz on average. */
      if (ms > 4*40)
        {
          if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
            {
              GdkWindowImplQuartz *window_impl = GDK_WINDOW_IMPL_QUARTZ (drawable);

              [window_impl->toplevel flushWindow];
            }

          prev_tv = tv;
        }
    }
  else
    prev_tv = tv;
}

void
gdk_quartz_drawable_release_context (GdkDrawable  *drawable, 
				     CGContextRef  cg_context)
{
  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
    {
      GdkWindowImplQuartz *window_impl = GDK_WINDOW_IMPL_QUARTZ (drawable);

      CGContextRestoreGState (cg_context);
      CGContextSetAllowsAntialiasing (cg_context, TRUE);

      /* See comment in gdk_quartz_drawable_get_context(). */
      if (window_impl->in_paint_rect_count == 0)
        {
          _gdk_quartz_drawable_flush (drawable);
          [window_impl->view unlockFocus];
        }
    }
  else if (GDK_IS_PIXMAP_IMPL_QUARTZ (drawable))
    CGContextRelease (cg_context);
}

void
_gdk_quartz_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_quartz_cairo_key,
				   NULL, NULL);
      impl->cairo_surface = NULL;
    }
}  
