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
#include <assert.h>

#include <string.h>

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "../../gdk-pixbuf/gdk-pixbuf-private.h"

#include "gdkinternals.h"

#include "cairo-directfb.h"


#include <direct/debug.h>
#include <direct/messages.h>

/*
 * There can be multiple domains in one file and one domain (same same) in multiple files.
 */
D_DEBUG_DOMAIN( GDKDFB_Drawable, "GDKDFB/Drawable", "GDK DirectFB Drawable" );
D_DEBUG_DOMAIN( GDKDFB_DrawClip, "GDKDFB/DrawClip", "GDK DirectFB Drawable Clip Region" );


/* From DirectFB's <gfx/generix/duffs_device.h> */
#define DUFF_1() \
               case 1:\
                    SET_PIXEL( D[0], S[0] );

#define DUFF_2() \
               case 3:\
                    SET_PIXEL( D[2], S[2] );\
               case 2:\
                    SET_PIXEL( D[1], S[1] );\
               DUFF_1()

#define DUFF_3() \
               case 7:\
                    SET_PIXEL( D[6], S[6] );\
               case 6:\
                    SET_PIXEL( D[5], S[5] );\
               case 5:\
                    SET_PIXEL( D[4], S[4] );\
               case 4:\
                    SET_PIXEL( D[3], S[3] );\
               DUFF_2()

#define DUFF_4() \
               case 15:\
                    SET_PIXEL( D[14], S[14] );\
               case 14:\
                    SET_PIXEL( D[13], S[13] );\
               case 13:\
                    SET_PIXEL( D[12], S[12] );\
               case 12:\
                    SET_PIXEL( D[11], S[11] );\
               case 11:\
                    SET_PIXEL( D[10], S[10] );\
               case 10:\
                    SET_PIXEL( D[9], S[9] );\
               case 9:\
                    SET_PIXEL( D[8], S[8] );\
               case 8:\
                    SET_PIXEL( D[7], S[7] );\
               DUFF_3()

#define SET_PIXEL_DUFFS_DEVICE_N( D, S, w, n ) \
do {\
     while (w) {\
          register int l = w & ((1 << n) - 1);\
          switch (l) {\
               default:\
                    l = (1 << n);\
                    SET_PIXEL( D[(1 << n)-1], S[(1 << n)-1] );\
               DUFF_##n()\
          }\
          D += l;\
          S += l;\
          w -= l;\
     }\
} while(0)


static GdkScreen * gdk_directfb_get_screen (GdkDrawable    *drawable);
static void gdk_drawable_impl_directfb_class_init (GdkDrawableImplDirectFBClass *klass);

static cairo_surface_t *gdk_directfb_ref_cairo_surface (GdkDrawable *drawable);


static gboolean  accelerated_alpha_blending = FALSE;
static gpointer  parent_class               = NULL;
static const cairo_user_data_key_t gdk_directfb_cairo_key;


/**********************************************************
 * DirectFB specific implementations of generic functions *
 **********************************************************/


static void
gdk_directfb_set_colormap (GdkDrawable *drawable,
                           GdkColormap *colormap)
{
  GdkDrawableImplDirectFB *impl;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  D_DEBUG_AT( GDKDFB_Drawable, "%s( %p, %p ) <- old %p\n", G_STRFUNC, drawable, colormap, impl->colormap );

  if (impl->colormap == colormap)
    return;

  if (impl->colormap)
    g_object_unref (impl->colormap);

  impl->colormap = colormap;

  if (colormap)
    g_object_ref (colormap);
}

static GdkColormap*
gdk_directfb_get_colormap (GdkDrawable *drawable)
{
  GdkColormap *retval;

  retval = GDK_DRAWABLE_IMPL_DIRECTFB (drawable)->colormap;

  if (!retval) {
    retval = gdk_colormap_get_system ();
	gdk_directfb_set_colormap(drawable,retval);
  }

  return retval;
}

static gint
gdk_directfb_get_depth (GdkDrawable *drawable)
{
  GdkDrawableImplDirectFB *impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  return DFB_BITS_PER_PIXEL (impl->format);
}

static void
gdk_directfb_get_size (GdkDrawable *drawable,
                       gint        *width,
                       gint        *height)
{
  GdkDrawableImplDirectFB *impl;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (width)
    *width = impl->width;

  if (height)
    *height = impl->height;
}

static GdkVisual*
gdk_directfb_get_visual (GdkDrawable *drawable)
{
  return gdk_visual_get_system ();
}

/* Calculates the real clipping region for a drawable, taking into account
 * other windows and the gc clip region.
 */
void
gdk_directfb_clip_region (GdkDrawable  *drawable,
                          GdkGC        *gc,
                          GdkRectangle *draw_rect,
                          cairo_region_t    *ret_clip)
{
  GdkDrawableImplDirectFB *private;
  GdkRectangle             rect;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_DRAWABLE_IMPL_DIRECTFB (drawable));
  g_return_if_fail (ret_clip != NULL);

  D_DEBUG_AT( GDKDFB_DrawClip, "%s( %p, %p, %p )\n", G_STRFUNC, drawable, gc, draw_rect );

  private = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!draw_rect)
    {
      rect.x      = 0;
      rect.y      = 0;
      rect.width  = private->width;
      rect.height = private->height;

      draw_rect = &rect;
    }
  D_DEBUG_AT( GDKDFB_DrawClip, "  -> draw rectangle   == %4d,%4d - %4dx%4d =\n",
              draw_rect->x, draw_rect->y, draw_rect->width, draw_rect->height );

  temp_region_init_rectangle( ret_clip, draw_rect );

  if (private->buffered) {
       D_DEBUG_AT( GDKDFB_DrawClip, "  -> buffered region   > %4d,%4d - %4dx%4d <  (%ld boxes)\n",
                   GDKDFB_RECTANGLE_VALS_FROM_BOX( &private->paint_region.extents ),
                   private->paint_region.numRects );

    cairo_region_intersect (ret_clip, &private->paint_region);
  }

  if (gc)
    {
      GdkGCDirectFB *gc_private = GDK_GC_DIRECTFB (gc);
      cairo_region_t     *region     = &gc_private->clip_region;

      if (region->numRects)
        {
          D_DEBUG_AT( GDKDFB_DrawClip, "  -> clipping region   > %4d,%4d - %4dx%4d <  (%ld boxes)\n",
                      GDKDFB_RECTANGLE_VALS_FROM_BOX( &region->extents ), region->numRects );

          if (gc->clip_x_origin || gc->clip_y_origin)
            {
              cairo_region_translate (ret_clip, -gc->clip_x_origin, -gc->clip_y_origin);
              cairo_region_intersect (ret_clip, region);
              cairo_region_translate (ret_clip, gc->clip_x_origin, gc->clip_y_origin);
            }
          else
            {
              cairo_region_intersect (ret_clip, region);
            }
        }

      if (gc_private->values_mask & GDK_GC_SUBWINDOW &&
          gc_private->values.subwindow_mode == GDK_INCLUDE_INFERIORS)
        return;
    }

  if (private->buffered) {
       D_DEBUG_AT( GDKDFB_DrawClip, "  => returning clip   >> %4d,%4d - %4dx%4d << (%ld boxes)\n",
                   GDKDFB_RECTANGLE_VALS_FROM_BOX( &ret_clip->extents ), ret_clip->numRects );
    return;
  }

  if (GDK_IS_WINDOW (private->wrapper) &&
      GDK_WINDOW_IS_MAPPED (private->wrapper) &&
      !GDK_WINDOW_OBJECT (private->wrapper)->input_only)
    {
      GList     *cur;
      cairo_region_t  temp;

      temp.numRects = 1;
      temp.rects = &temp.extents;
      temp.size = 1;

      for (cur = GDK_WINDOW_OBJECT (private->wrapper)->children;
           cur;
           cur = cur->next)
        {
          GdkWindowObject         *cur_private;
          GdkDrawableImplDirectFB *cur_impl;

          cur_private = GDK_WINDOW_OBJECT (cur->data);

          if (!GDK_WINDOW_IS_MAPPED (cur_private) || cur_private->input_only)
            continue;

          cur_impl = GDK_DRAWABLE_IMPL_DIRECTFB (cur_private->impl);

          temp.extents.x1 = cur_private->x;
          temp.extents.y1 = cur_private->y;
          temp.extents.x2 = cur_private->x + cur_impl->width;
          temp.extents.y2 = cur_private->y + cur_impl->height;

          D_DEBUG_AT( GDKDFB_DrawClip, "  -> clipping child    [ %4d,%4d - %4dx%4d ]  (%ld boxes)\n",
                      GDKDFB_RECTANGLE_VALS_FROM_BOX( &temp.extents ), temp.numRects );

          cairo_region_subtract (ret_clip, &temp);
        }
    }

  D_DEBUG_AT( GDKDFB_DrawClip, "  => returning clip   >> %4d,%4d - %4dx%4d << (%ld boxes)\n",
              GDKDFB_RECTANGLE_VALS_FROM_BOX( &ret_clip->extents ), ret_clip->numRects );
}

/* Drawing
 */

static inline void
gdk_directfb_set_color (GdkDrawableImplDirectFB *impl,
                        GdkColor                *color,
                        guchar                   alpha)
{
  if (DFB_PIXELFORMAT_IS_INDEXED (impl->format))
    {
      impl->surface->SetColorIndex (impl->surface, color->pixel);
    }
  else
    {
      impl->surface->SetColor (impl->surface,
                               color->red   >> 8,
                               color->green >> 8,
                               color->blue  >> 8,
                               alpha);
    }
}

static gboolean
gdk_directfb_setup_for_drawing (GdkDrawableImplDirectFB *impl,
                                GdkGCDirectFB           *gc_private)
{
  DFBSurfaceDrawingFlags flags = DSDRAW_NOFX;
  GdkColor               color = { 0, 0, 0, 0 };
  guchar                 alpha = 0xFF;

  if (!impl->surface)
    return FALSE;

  if (gc_private && gc_private->values_mask & GDK_GC_FOREGROUND)
    color = gc_private->values.foreground;

  if (gc_private && gc_private->values_mask & GDK_GC_FUNCTION)
    {
      switch (gc_private->values.function)
        {
        case GDK_COPY:
          flags = DSDRAW_NOFX;
          break;

        case GDK_INVERT:
          color.red = color.green = color.blue = 0xFFFF;
          alpha = 0x0;
          flags = DSDRAW_XOR;
          break;

        case GDK_XOR:
          alpha = 0x0;
          flags = DSDRAW_XOR;
          break;

        case GDK_CLEAR:
          color.red = color.green = color.blue = 0x0;
          flags = DSDRAW_NOFX;
          break;

        case GDK_NOOP:
          return FALSE;

        case GDK_SET:
          color.red = color.green = color.blue = 0xFFFF;
          flags = DSDRAW_NOFX;
          break;

        default:
          g_message ("unsupported GC function %d",
                     gc_private->values.function);
          flags = DSDRAW_NOFX;
          break;
        }
    }

  gdk_directfb_set_color (impl, &color, alpha);

  impl->surface->SetDrawingFlags (impl->surface, flags);

  return TRUE;
}

static inline void
convert_rgba_pixbuf_to_image (guint32 *src,
                              guint    src_pitch,
                              guint32 *dest,
                              guint    dest_pitch,
                              guint    width,
                              guint    height)
{
  guint i;

  while (height--)
    {
      for (i = 0; i < width; i++)
        {
          guint32 pixel = GUINT32_FROM_BE (src[i]);
          dest[i] = (pixel >> 8) | (pixel << 24);
        }

      src  += src_pitch;
      dest += dest_pitch;
    }
}

static inline void
convert_rgb_pixbuf_to_image (guchar  *src,
                             guint    src_pitch,
                             guint32 *dest,
                             guint    dest_pitch,
                             guint    width,
                             guint    height)
{
  guint   i;
  guchar *s;

  while (height--)
    {
      s = src;

      for (i = 0; i < width; i++, s += 3)
        dest[i] = 0xFF000000 | (s[0] << 16) | (s[1] << 8) | s[2];

      src  += src_pitch;
      dest += dest_pitch;
    }
}

/*
 * Object stuff
 */
static inline const char *
drawable_impl_type_name( GObject *object )
{
     if (GDK_IS_PIXMAP (object))
          return "PIXMAP";

     if (GDK_IS_WINDOW (object))
          return "WINDOW";

     if (GDK_IS_DRAWABLE_IMPL_DIRECTFB (object))
          return "DRAWABLE";

     return "unknown";
}


static void
gdk_drawable_impl_directfb_finalize (GObject *object)
{
  GdkDrawableImplDirectFB *impl;
  impl = GDK_DRAWABLE_IMPL_DIRECTFB (object);

  D_DEBUG_AT( GDKDFB_Drawable, "%s( %p ) <- %dx%d (%s at %4d,%4d)\n", G_STRFUNC,
              object, impl->width, impl->height,
              drawable_impl_type_name( object ),
              impl->abs_x, impl->abs_y );

  gdk_directfb_set_colormap (GDK_DRAWABLE (object), NULL);
  if( impl->cairo_surface ) {
	cairo_surface_finish(impl->cairo_surface);
  }
  if( impl->surface )
  	impl->surface->Release (impl->surface);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_drawable_impl_directfb_class_init (GdkDrawableImplDirectFBClass *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass     *object_class   = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_directfb_finalize;

  drawable_class->create_gc      = _gdk_directfb_gc_new;

  drawable_class->ref_cairo_surface = gdk_directfb_ref_cairo_surface;
  drawable_class->set_colormap   = gdk_directfb_set_colormap;
  drawable_class->get_colormap   = gdk_directfb_get_colormap;

  drawable_class->get_depth      = gdk_directfb_get_depth;
  drawable_class->get_visual     = gdk_directfb_get_visual;

  drawable_class->get_size       = gdk_directfb_get_size;

        drawable_class->get_screen = gdk_directfb_get_screen;


  real_draw_pixbuf = drawable_class->draw_pixbuf;
  drawable_class->draw_pixbuf = gdk_directfb_draw_pixbuf;

  /* check for hardware-accelerated alpha-blending */
  {
    DFBGraphicsDeviceDescription desc;
                _gdk_display->directfb->GetDeviceDescription ( _gdk_display->directfb, &desc);

    accelerated_alpha_blending =
      ((desc.acceleration_mask & DFXL_BLIT) &&
       (desc.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL));
  }
}

GType
gdk_drawable_impl_directfb_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
        {
          sizeof (GdkDrawableImplDirectFBClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) gdk_drawable_impl_directfb_class_init,
          NULL,           /* class_finalize */
          NULL,           /* class_data */
          sizeof (GdkDrawableImplDirectFB),
          0,              /* n_preallocs */
          (GInstanceInitFunc) NULL,
        };

      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplDirectFB",
                                            &object_info, 0);
    }

  return object_type;
}

static GdkScreen * gdk_directfb_get_screen (GdkDrawable    *drawable){
        return gdk_screen_get_default();
}

static void
gdk_directfb_cairo_surface_destroy (void *data)
{
  GdkDrawableImplDirectFB *impl = data;
  impl->cairo_surface = NULL;
}

void
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
                                       int width,
                                       int height)
{
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
                                     int width,
                                     int height)
{
  GdkDrawableImplDirectFB *impl;
  IDirectFB *dfb;
  cairo_surface_t *ret;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
  dfb = GDK_DISPLAY_DFB (gdk_drawable_get_display (drawable))->directfb;

  ret = cairo_directfb_surface_create (dfb, impl->surface);
  cairo_surface_set_user_data (ret,
                               &gdk_directfb_cairo_key, drawable,
                               gdk_directfb_cairo_surface_destroy);

  return ret;
}

static cairo_surface_t *
gdk_directfb_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplDirectFB *impl;
  IDirectFB               *dfb;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_DIRECTFB (drawable), NULL);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
  dfb = GDK_DISPLAY_DFB(gdk_drawable_get_display(drawable))->directfb;
  
  if (!impl->cairo_surface) {
    IDirectFBSurface *surface;
    g_assert (impl->surface != NULL);
#if defined(CAIRO_VERSION) && CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1,5,5)
    impl->surface->GetSubSurface (impl->surface, NULL, &surface);
#else
    surface = impl->surface;
#endif
    if (surface) {
      impl->cairo_surface = cairo_directfb_surface_create (dfb, surface);
      if (impl->cairo_surface) {
        cairo_surface_set_user_data (impl->cairo_surface, 
                                     &gdk_directfb_cairo_key, drawable, 
                                     gdk_directfb_cairo_surface_destroy);
      }
#if defined(CAIRO_VERSION) && CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1,5,5)
      surface->Release (surface);
#endif
    }
  } else {
    cairo_surface_reference (impl->cairo_surface);
  }
  
  g_assert (impl->cairo_surface != NULL);
  return impl->cairo_surface;
}
