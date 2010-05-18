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

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkinternals.h"


#include "gdkregion-generic.h"
#include "gdkalias.h"

#include "cairo-directfb.h"


#include <direct/debug.h>
#include <direct/messages.h>

/*
 * There can be multiple domains in one file and one domain (same same) in multiple files.
 */
D_DEBUG_DOMAIN (GDKDFB_Drawable, "GDKDFB/Drawable", "GDK DirectFB Drawable");
D_DEBUG_DOMAIN (GDKDFB_DrawClip, "GDKDFB/DrawClip", "GDK DirectFB Drawable Clip Region");


/* From DirectFB's <gfx/generix/duffs_device.h> */
#define DUFF_1()                                \
  case 1:                                       \
  SET_PIXEL (D[0], S[0]);

#define DUFF_2()                                \
  case 3:                                       \
  SET_PIXEL (D[2], S[2]);                       \
 case 2:                                        \
 SET_PIXEL (D[1], S[1]);                        \
 DUFF_1 ()

#define DUFF_3()                                \
  case 7:                                       \
  SET_PIXEL (D[6], S[6]);                       \
 case 6:                                        \
 SET_PIXEL (D[5], S[5]);                        \
 case 5:                                        \
 SET_PIXEL (D[4], S[4]);                        \
 case 4:                                        \
 SET_PIXEL (D[3], S[3]);                        \
 DUFF_2 ()

#define DUFF_4()                                \
  case 15:                                      \
  SET_PIXEL (D[14], S[14]);                     \
 case 14:                                       \
 SET_PIXEL (D[13], S[13]);                      \
 case 13:                                       \
 SET_PIXEL (D[12], S[12]);                      \
 case 12:                                       \
 SET_PIXEL (D[11], S[11]);                      \
 case 11:                                       \
 SET_PIXEL (D[10], S[10]);                      \
 case 10:                                       \
 SET_PIXEL (D[9], S[9]);                        \
 case 9:                                        \
 SET_PIXEL (D[8], S[8]);                        \
 case 8:                                        \
 SET_PIXEL (D[7], S[7]);                        \
 DUFF_3 ()

#define SET_PIXEL_DUFFS_DEVICE_N(D, S, w, n)            \
  do {                                                  \
    while (w) {                                         \
      register int l = w & ((1 << n) - 1);              \
      switch (l) {                                      \
      default:                                          \
        l = (1 << n);                                   \
        SET_PIXEL (D[(1 << n) - 1], S[(1 << n) - 1]);   \
        DUFF_##n ()                                     \
          }                                             \
      D += l;                                           \
      S += l;                                           \
      w -= l;                                           \
    }                                                   \
  } while(0)


static GdkScreen *gdk_directfb_get_screen (GdkDrawable *drawable);
static void gdk_drawable_impl_directfb_class_init (GdkDrawableImplDirectFBClass *klass);
static void gdk_directfb_draw_lines (GdkDrawable *drawable,
                                     GdkGC       *gc,
                                     GdkPoint    *points,
                                     gint         npoints);

static cairo_surface_t *gdk_directfb_ref_cairo_surface (GdkDrawable *drawable);


static gboolean  accelerated_alpha_blending = FALSE;
static gpointer  parent_class               = NULL;
static const cairo_user_data_key_t gdk_directfb_cairo_key;

static void (*real_draw_pixbuf) (GdkDrawable *drawable,
                                 GdkGC       *gc,
                                 GdkPixbuf   *pixbuf,
                                 gint         src_x,
                                 gint         src_y,
                                 gint         dest_x,
                                 gint         dest_y,
                                 gint         width,
                                 gint         height,
                                 GdkRgbDither dither,
                                 gint         x_dither,
                                 gint         y_dither);


/**********************************************************
 * DirectFB specific implementations of generic functions *
 **********************************************************/


static void
gdk_directfb_set_colormap (GdkDrawable *drawable,
                           GdkColormap *colormap)
{
  GdkDrawableImplDirectFB *impl;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p ) <- old %p\n",
              G_STRFUNC, drawable, colormap, impl->colormap);

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
    gdk_directfb_set_colormap (drawable, retval);
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
                          GdkRegion    *ret_clip)
{
  GdkDrawableImplDirectFB *private;
  GdkRectangle             rect;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_DRAWABLE_IMPL_DIRECTFB (drawable));
  g_return_if_fail (ret_clip != NULL);

  D_DEBUG_AT (GDKDFB_DrawClip, "%s( %p, %p, %p )\n",
              G_STRFUNC, drawable, gc, draw_rect);

  private = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!draw_rect)
    {
      rect.x      = 0;
      rect.y      = 0;
      rect.width  = private->width;
      rect.height = private->height;

      draw_rect = &rect;
    }
  D_DEBUG_AT (GDKDFB_DrawClip, "  -> draw rectangle   == %4d,%4d - %4dx%4d =\n",
              draw_rect->x, draw_rect->y, draw_rect->width, draw_rect->height);

  temp_region_init_rectangle (ret_clip, draw_rect);

  if (private->buffered) {
    D_DEBUG_AT (GDKDFB_DrawClip, "  -> buffered region   > %4d,%4d - %4dx%4d <  (%ld boxes)\n",
                GDKDFB_RECTANGLE_VALS_FROM_BOX (&private->paint_region.extents),
                private->paint_region.numRects);

    gdk_region_intersect (ret_clip, &private->paint_region);
  }

  if (gc)
    {
      GdkGCDirectFB *gc_private = GDK_GC_DIRECTFB (gc);
      GdkRegion     *region     = &gc_private->clip_region;

      if (region->numRects)
        {
          D_DEBUG_AT (GDKDFB_DrawClip, "  -> clipping region   > %4d,%4d - %4dx%4d <  (%ld boxes)\n",
                      GDKDFB_RECTANGLE_VALS_FROM_BOX (&region->extents), region->numRects);

          if (gc->clip_x_origin || gc->clip_y_origin)
            {
              gdk_region_offset (ret_clip, -gc->clip_x_origin, -gc->clip_y_origin);
              gdk_region_intersect (ret_clip, region);
              gdk_region_offset (ret_clip, gc->clip_x_origin, gc->clip_y_origin);
            }
          else
            {
              gdk_region_intersect (ret_clip, region);
            }
        }

      if (gc_private->values_mask & GDK_GC_SUBWINDOW &&
          gc_private->values.subwindow_mode == GDK_INCLUDE_INFERIORS)
        return;
    }

  if (private->buffered) {
    D_DEBUG_AT (GDKDFB_DrawClip, "  => returning clip   >> %4d,%4d - %4dx%4d << (%ld boxes)\n",
                GDKDFB_RECTANGLE_VALS_FROM_BOX (&ret_clip->extents), ret_clip->numRects);
    return;
  }

  if (GDK_IS_WINDOW (private->wrapper) &&
      GDK_WINDOW_IS_MAPPED (private->wrapper) &&
      !GDK_WINDOW_OBJECT (private->wrapper)->input_only)
    {
      GList     *cur;
      GdkRegion  temp;

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

          D_DEBUG_AT (GDKDFB_DrawClip, "  -> clipping child    [ %4d,%4d - %4dx%4d ]  (%ld boxes)\n",
                      GDKDFB_RECTANGLE_VALS_FROM_BOX (&temp.extents), temp.numRects);

          gdk_region_subtract (ret_clip, &temp);
        }
    }

  D_DEBUG_AT (GDKDFB_DrawClip, "  => returning clip   >> %4d,%4d - %4dx%4d << (%ld boxes)\n",
              GDKDFB_RECTANGLE_VALS_FROM_BOX (&ret_clip->extents), ret_clip->numRects);
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

static void
gdk_directfb_draw_rectangle (GdkDrawable *drawable,
                             GdkGC       *gc,
                             gint         filled,
                             gint         x,
                             gint         y,
                             gint         width,
                             gint         height)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion                clip;
  GdkGCDirectFB           *gc_private = NULL;
  IDirectFBSurface        *surface    = NULL;
  gint  i;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %s, %4d,%4d - %4dx%4d )\n", G_STRFUNC,
              drawable, gc, filled ? " filled" : "outline", x, y, width, height);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!impl->surface)
    return;

  if (gc)
    gc_private = GDK_GC_DIRECTFB (gc);

  if (gc_private)
    {
      if (gdk_directfb_enable_color_keying &&
	  (gc_private->values.foreground.red   >> 8) == gdk_directfb_bg_color_key.r &&
	  (gc_private->values.foreground.green >> 8) == gdk_directfb_bg_color_key.g &&
	  (gc_private->values.foreground.blue  >> 8) == gdk_directfb_bg_color_key.b)
	{
          if (DFB_PIXELFORMAT_IS_INDEXED (impl->format))
            impl->surface->SetColorIndex (impl->surface, 255);
          else
            impl->surface->SetColor (impl->surface,
                                     gdk_directfb_bg_color.r,
                                     gdk_directfb_bg_color.g,
                                     gdk_directfb_bg_color.b,
                                     gdk_directfb_bg_color.a);
	}
      else
	{
          if (!gdk_directfb_setup_for_drawing (impl, gc_private)) {
            return;
          }
	}
    }
  else
    {
      GdkWindowObject *win = GDK_WINDOW_OBJECT (impl->wrapper);

      if (gdk_directfb_enable_color_keying)
	{
          if (DFB_PIXELFORMAT_IS_INDEXED (impl->format))
            impl->surface->SetColorIndex (impl->surface, 255);
          else
            impl->surface->SetColor (impl->surface,
                                     gdk_directfb_bg_color.r,
                                     gdk_directfb_bg_color.b,
                                     gdk_directfb_bg_color.g,
                                     gdk_directfb_bg_color.a);
	}
      else
	{
          gdk_directfb_set_color (impl, &win->bg_color, 0xFF);
	}
    }

  if (filled)
    {
      GdkRectangle  rect = { x, y, width, height };

      gdk_directfb_clip_region (drawable, gc, &rect, &clip);

      if (gc_private && gc_private->values_mask & GDK_GC_FILL)
        {
          if (gc_private->values.fill == GDK_STIPPLED  &&
              gc_private->values_mask & GDK_GC_STIPPLE &&
              gc_private->values.stipple)
            {
              surface =
                GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (gc_private->values.stipple)->impl)->surface;

              if (surface)
                impl->surface->SetBlittingFlags (impl->surface,
                                                 (DSBLIT_BLEND_ALPHACHANNEL |
                                                  DSBLIT_COLORIZE));
            }
          else if (gc_private->values.fill == GDK_TILED  &&
                   gc_private->values_mask & GDK_GC_TILE &&
                   gc_private->values.tile)
            {
              surface = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (gc_private->values.tile)->impl)->surface;

              impl->surface->SetBlittingFlags (impl->surface, DSBLIT_NOFX);
            }
        }

      if (surface)
        {
          if (gc_private->values_mask & GDK_GC_TS_X_ORIGIN)
            x = gc_private->values.ts_x_origin;
          if (gc_private->values_mask & GDK_GC_TS_Y_ORIGIN)
            y = gc_private->values.ts_y_origin;

          for (i = 0; i < clip.numRects; i++)
            {
              DFBRegion reg = { clip.rects[i].x1, clip.rects[i].y1,
                                clip.rects[i].x2, clip.rects[i].y2 };

              impl->surface->SetClip (impl->surface, &reg);
              impl->surface->TileBlit (impl->surface, surface, NULL, x, y);
            }
        }
      else  /* normal rectangle filling */
        {
          DFBRectangle rects[clip.numRects];

          impl->surface->SetClip (impl->surface, NULL);

          for (i = 0; i < clip.numRects; i++)
            {
              GdkRegionBox *box = &clip.rects[i];

              rects[i].x = box->x1;
              rects[i].y = box->y1;
              rects[i].w = box->x2 - box->x1;
              rects[i].h = box->y2 - box->y1;
            }

          impl->surface->FillRectangles (impl->surface, rects, clip.numRects);
        }

      temp_region_deinit (&clip);
    }
  else
    {

      DFBRegion region = { x, y, x + width, y + height };

      impl->surface->SetClip (impl->surface, &region);

      /*  DirectFB does not draw rectangles the X way. Using DirectFB,
          a filled Rectangle has the same size as a drawn one, while
          X draws the rectangle one pixel taller and wider.  */
      impl->surface->DrawRectangle (impl->surface,
                                    x, y, width, height);
    }
}

static void
gdk_directfb_draw_arc (GdkDrawable *drawable,
                       GdkGC       *gc,
                       gint         filled,
                       gint         x,
                       gint         y,
                       gint         width,
                       gint         height,
                       gint         angle1,
                       gint         angle2)
{
  D_UNIMPLEMENTED ();
}

static void
gdk_directfb_draw_polygon (GdkDrawable *drawable,
                           GdkGC       *gc,
                           gint         filled,
                           GdkPoint    *points,
                           gint         npoints)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %s, %p, %d )\n", G_STRFUNC,
              drawable, gc, filled ? " filled" : "outline", points, npoints);

  if (npoints < 3)
    return;

  if (filled)
    {
      if (npoints == 3 || (npoints == 4 &&
                           points[0].x == points[npoints - 1].x &&
                           points[0].y == points[npoints - 1].y))
        {
          GdkDrawableImplDirectFB *impl;
          GdkRegion                clip;
          gint                     i;

          impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

          if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
            return;

          gdk_directfb_clip_region (drawable, gc, NULL, &clip);

          for (i = 0; i < clip.numRects; i++)
            {
              DFBRegion reg = { clip.rects[i].x1, clip.rects[i].y1,
                                clip.rects[i].x2, clip.rects[i].y2 };

              impl->surface->SetClip (impl->surface, &reg);
              impl->surface->FillTriangle (impl->surface,
                                           points[0].x, points[0].y,
                                           points[1].x, points[1].y,
                                           points[2].x, points[2].y);

            }

          temp_region_deinit (&clip);

          return;
        }
      else
        g_message ("filled polygons with n > 3 are not yet supported, "
                   "drawing outlines");
    }

  if (points[0].x != points[npoints - 1].x ||
      points[0].y != points[npoints - 1].y)
    {
      GdkPoint *tmp_points;

      tmp_points = g_new (GdkPoint, npoints + 1);
      memcpy (tmp_points, points, npoints * sizeof (GdkPoint));
      tmp_points[npoints].x = points[0].x;
      tmp_points[npoints].y = points[0].y;

      gdk_directfb_draw_lines (drawable, gc, tmp_points, npoints + 1);

      g_free (tmp_points);
    }
  else
    {
      gdk_directfb_draw_lines (drawable, gc, points, npoints);
    }
}

static void
gdk_directfb_draw_text (GdkDrawable *drawable,
                        GdkFont     *font,
                        GdkGC       *gc,
                        gint         x,
                        gint         y,
                        const gchar *text,
                        gint         text_length)
{
  D_UNIMPLEMENTED ();
}

static void
gdk_directfb_draw_text_wc (GdkDrawable    *drawable,
                           GdkFont        *font,
                           GdkGC          *gc,
                           gint            x,
                           gint            y,
                           const GdkWChar *text,
                           gint            text_length)
{
  D_UNIMPLEMENTED ();
}

static void
gdk_directfb_draw_drawable (GdkDrawable *drawable,
                            GdkGC       *gc,
                            GdkDrawable *src,
                            gint         xsrc,
                            gint         ysrc,
                            gint         xdest,
                            gint         ydest,
                            gint         width,
                            gint         height,
                            GdkDrawable *original_src)
{
  GdkDrawableImplDirectFB *impl;
  GdkDrawableImplDirectFB *src_impl;
  GdkRegion                clip;
  GdkRectangle             dest_rect = { xdest,
                                         ydest,
                                         xdest + width ,
                                         ydest + height};

  DFBRectangle rect = { xsrc, ysrc, width, height };
  gint i;

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %p, %4d,%4d -> %4d,%4d - %dx%d )\n", G_STRFUNC,
              drawable, gc, src, xsrc, ysrc, xdest, ydest, width, height);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!impl->surface)
    return;

  if (GDK_IS_PIXMAP (src))
    src_impl = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (src)->impl);
  else if (GDK_IS_WINDOW (src))
    src_impl = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (src)->impl);
  else if (GDK_IS_DRAWABLE_IMPL_DIRECTFB (src))
    src_impl = GDK_DRAWABLE_IMPL_DIRECTFB (src);
  else
    return;

  gdk_directfb_clip_region (drawable, gc, &dest_rect, &clip);

  impl->surface->SetBlittingFlags (impl->surface, DSBLIT_NOFX);

  for (i = 0; i < clip.numRects; i++)
    {
      DFBRegion reg = { clip.rects[i].x1, clip.rects[i].y1,
                        clip.rects[i].x2, clip.rects[i].y2 };

      impl->surface->SetClip (impl->surface, &reg);
      impl->surface->Blit (impl->surface,
                           src_impl->surface,
                           &rect,
                           xdest, ydest);
    }

  temp_region_deinit (&clip);
}

static void
gdk_directfb_draw_points (GdkDrawable *drawable,
                          GdkGC       *gc,
                          GdkPoint    *points,
                          gint         npoints)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion                clip;

  DFBRegion region = { points->x, points->y, points->x, points->y };

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %p, %d )\n",
              G_STRFUNC, drawable, gc, points, npoints);

  if (npoints < 1)
    return;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
    return;

  gdk_directfb_clip_region (drawable, gc, NULL, &clip);

  while (npoints > 0)
    {
      if (gdk_region_point_in (&clip, points->x, points->y))
        {
          impl->surface->FillRectangle (impl->surface,
                                        points->x, points->y, 1, 1);

          if (points->x < region.x1)
            region.x1 = points->x;
          if (points->x > region.x2)
            region.x2 = points->x;

          if (points->y < region.y1)
            region.y1 = points->y;
          if (points->y > region.y2)
            region.y2 = points->y;
        }

      npoints--;
      points++;
    }

  temp_region_deinit (&clip);
}

static void
gdk_directfb_draw_segments (GdkDrawable *drawable,
                            GdkGC       *gc,
                            GdkSegment  *segs,
                            gint         nsegs)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion                clip;
  gint                     i;

  //  DFBRegion region = { segs->x1, segs->y1, segs->x2, segs->y2 };

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %p, %d )\n",
              G_STRFUNC, drawable, gc, segs, nsegs);

  if (nsegs < 1)
    return;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
    return;

  gdk_directfb_clip_region (drawable, gc, NULL, &clip);

  for (i = 0; i < clip.numRects; i++)
    {
      DFBRegion reg = { clip.rects[i].x1, clip.rects[i].y1,
                        clip.rects[i].x2, clip.rects[i].y2 };

      impl->surface->SetClip (impl->surface, &reg);

      impl->surface->DrawLines (impl->surface, (DFBRegion *)segs, nsegs);
    }

  temp_region_deinit (&clip);

  /* everything below can be omitted if the drawing is buffered */
  /*  if (impl->buffered)
      return;

      if (region.x1 > region.x2)
      {
      region.x1 = segs->x2;
      region.x2 = segs->x1;
      }
      if (region.y1 > region.y2)
      {
      region.y1 = segs->y2;
      region.y2 = segs->y1;
      }

      while (nsegs > 1)
      {
      nsegs--;
      segs++;

      if (segs->x1 < region.x1)
      region.x1 = segs->x1;
      if (segs->x2 < region.x1)
      region.x1 = segs->x2;

      if (segs->y1 < region.y1)
      region.y1 = segs->y1;
      if (segs->y2 < region.y1)
      region.y1 = segs->y2;

      if (segs->x1 > region.x2)
      region.x2 = segs->x1;
      if (segs->x2 > region.x2)
      region.x2 = segs->x2;

      if (segs->y1 > region.y2)
      region.y2 = segs->y1;
      if (segs->y2 > region.y2)
      region.y2 = segs->y2;
      }*/
}

static void
gdk_directfb_draw_lines (GdkDrawable *drawable,
                         GdkGC       *gc,
                         GdkPoint    *points,
                         gint         npoints)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion                clip;
  gint                     i;

  DFBRegion lines[npoints > 1 ? npoints - 1 : 1];

  DFBRegion region = { points->x, points->y, points->x, points->y };

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %p, %d )\n", G_STRFUNC,
              drawable, gc, points, npoints);

  if (npoints < 2)
    return;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
    return;

  /* create an array of DFBRegions so we can use DrawLines */

  lines[0].x1 = points->x;
  lines[0].y1 = points->y;

  for (i = 0; i < npoints - 2; i++)
    {
      points++;

      lines[i].x2 = lines[i+1].x1 = points->x;
      lines[i].y2 = lines[i+1].y1 = points->y;

      if (points->x < region.x1)
        region.x1 = points->x;

      if (points->y < region.y1)
        region.y1 = points->y;

      if (points->x > region.x2)
        region.x2 = points->x;

      if (points->y > region.y2)
        region.y2 = points->y;
    }

  points++;
  lines[i].x2 = points->x;
  lines[i].y2 = points->y;

  gdk_directfb_clip_region (drawable, gc, NULL, &clip);

  for (i = 0; i < clip.numRects; i++)
    {
      DFBRegion reg = { clip.rects[i].x1, clip.rects[i].y1,
                        clip.rects[i].x2, clip.rects[i].y2 };

      impl->surface->SetClip (impl->surface, &reg);
      impl->surface->DrawLines (impl->surface, lines, npoints - 1);
    }

  temp_region_deinit (&clip);
}

static void
gdk_directfb_draw_image (GdkDrawable *drawable,
                         GdkGC       *gc,
                         GdkImage    *image,
                         gint         xsrc,
                         gint         ysrc,
                         gint         xdest,
                         gint         ydest,
                         gint         width,
                         gint         height)
{
  GdkDrawableImplDirectFB *impl;
  GdkImageDirectFB        *image_private;
  GdkRegion                clip;
  GdkRectangle             dest_rect = { xdest, ydest, width, height };

  gint pitch = 0;
  gint i;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (image != NULL);

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %p, %4d,%4d -> %4d,%4d - %dx%d )\n",
              G_STRFUNC,
              drawable, gc, image, xsrc, ysrc, xdest, ydest, width, height);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
  image_private = image->windowing_data;

  if (!impl->surface)
    return;

  gdk_directfb_clip_region (drawable, gc, &dest_rect, &clip);

  if (!gdk_region_empty (&clip))
    {
      DFBRectangle  src_rect = { xsrc, ysrc, width, height };

      image_private->surface->Unlock (image_private->surface);

      impl->surface->SetBlittingFlags (impl->surface, DSBLIT_NOFX);

      for (i = 0; i < clip.numRects; i++)
        {
          DFBRegion reg = { clip.rects[i].x1, clip.rects[i].y1,
                            clip.rects[i].x2, clip.rects[i].y2 };

          impl->surface->SetClip (impl->surface, &reg);
          impl->surface->Blit (impl->surface,
                               image_private->surface, &src_rect,
                               xdest, ydest);
        }

      image_private->surface->Lock (image_private->surface, DSLF_WRITE,
                                    &image->mem, &pitch);
      image->bpl = pitch;
    }

  temp_region_deinit (&clip);
}

static void
composite (guchar *src_buf,
           gint    src_rowstride,
           guchar *dest_buf,
           gint    dest_rowstride,
           gint    width,
           gint    height)
{
  guchar *src = src_buf;
  guchar *dest = dest_buf;

  while (height--)
    {
      gint twidth = width;
      guchar *p = src;
      guchar *q = dest;

      while (twidth--)
        {
          guchar a = p[3];
          guint t;

          t = a * p[0] + (255 - a) * q[0] + 0x80;
          q[0] = (t + (t >> 8)) >> 8;
          t = a * p[1] + (255 - a) * q[1] + 0x80;
          q[1] = (t + (t >> 8)) >> 8;
          t = a * p[2] + (255 - a) * q[2] + 0x80;
          q[2] = (t + (t >> 8)) >> 8;

          p += 4;
          q += 3;
        }

      src += src_rowstride;
      dest += dest_rowstride;
    }
}

static void
composite_0888 (guchar      *src_buf,
                gint         src_rowstride,
                guchar      *dest_buf,
                gint         dest_rowstride,
                GdkByteOrder dest_byte_order,
                gint         width,
                gint         height)
{
  guchar *src = src_buf;
  guchar *dest = dest_buf;

  while (height--)
    {
      gint twidth = width;
      guchar *p = src;
      guchar *q = dest;

      if (dest_byte_order == GDK_LSB_FIRST)
        {
          while (twidth--)
            {
              guint t;

              t = p[3] * p[2] + (255 - p[3]) * q[0] + 0x80;
              q[0] = (t + (t >> 8)) >> 8;
              t = p[3] * p[1] + (255 - p[3]) * q[1] + 0x80;
              q[1] = (t + (t >> 8)) >> 8;
              t = p[3] * p[0] + (255 - p[3]) * q[2] + 0x80;
              q[2] = (t + (t >> 8)) >> 8;
              p += 4;
              q += 4;
            }
        }
      else
        {
          while (twidth--)
            {
              guint t;

              t = p[3] * p[0] + (255 - p[3]) * q[1] + 0x80;
              q[1] = (t + (t >> 8)) >> 8;
              t = p[3] * p[1] + (255 - p[3]) * q[2] + 0x80;
              q[2] = (t + (t >> 8)) >> 8;
              t = p[3] * p[2] + (255 - p[3]) * q[3] + 0x80;
              q[3] = (t + (t >> 8)) >> 8;
              p += 4;
              q += 4;
            }
        }

      src += src_rowstride;
      dest += dest_rowstride;
    }
}

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D, S, w)         \
  SET_PIXEL_DUFFS_DEVICE_N (D, S, w, 2)

/* From DirectFB's gfx/generic/generic.c" */
#define SET_PIXEL(D, S)                                                 \
        switch (S >> 26) {                                              \
  case 0:                                                               \
    break;                                                              \
  case 0x3f:                                                            \
    D = ((S <<  8) & 0xF800) |                                          \
      ((S >>  5) & 0x07E0) |                                            \
      ((S >> 19) & 0x001F);                                             \
    break;                                                              \
  default:                                                              \
    D = (((( (((S<<8) & 0xf800) | ((S>>19) & 0x001f))                   \
             - (D & 0xf81f)) * ((S>>26)+1) + ((D & 0xf81f)<<6)) & 0x003e07c0) \
         +                                                              \
         ((( ((S>>5) & 0x07e0)                                          \
             - (D & 0x07e0)) * ((S>>26)+1) + ((D & 0x07e0)<<6)) & 0x0001f800)) >> 6; \
  }

static void
composite_565 (guchar      *src_buf,
               gint         src_rowstride,
               guchar      *dest_buf,
               gint         dest_rowstride,
               GdkByteOrder dest_byte_order,
               gint         width,
               gint         height)
{
  while (height--) {
    int  w = width;
    u16 *D = (u16*) dest_buf;
    u32 *S = (u32*) src_buf;
#if 1
    if ((unsigned long)D & 2) {
      SET_PIXEL (D[0], S[0]);
      w--;
      D++;
      S++;
    }

    int  i;
    int  w2  = w / 2;
    u32 *D32 = (u32*) D;

    for (i=0; i<w2; i++) {
      register u32 S0 = S[(i << 1) + 0];
      register u32 S1 = S[(i << 1) + 1];

      if ((S0 >> 26) == 0x3f && (S1 >> 26) == 0x3f) {
        D32[i] = ((S0 <<  8) & 0x0000F800) |
          ((S0 >>  5) & 0x000007E0) |
          ((S0 >> 19) & 0x0000001F) |
          ((S1 << 24) & 0xF8000000) |
          ((S1 << 11) & 0x07E00000) |
          ((S1 >>  3) & 0x001F0000);
      }
      else {
        SET_PIXEL (D[(i << 1) + 0], S0);
        SET_PIXEL (D[(i << 1) + 1], S1);
      }
    }

    if (w & 1)
      SET_PIXEL (D[w - 1], S[w - 1]);
#else
    SET_PIXEL_DUFFS_DEVICE (D, S, w);
#endif

    dest_buf += dest_rowstride;
    src_buf += src_rowstride;
  }
}

#undef SET_PIXEL
#undef SET_PIXEL_DUFFS_DEVICE

static void
gdk_directfb_draw_pixbuf (GdkDrawable  *drawable,
                          GdkGC        *gc,
                          GdkPixbuf    *pixbuf,
                          gint          src_x,
                          gint          src_y,
                          gint          dest_x,
                          gint          dest_y,
                          gint          width,
                          gint          height,
                          GdkRgbDither  dither,
                          gint          x_dither,
                          gint          y_dither)
{
  GdkPixbuf *composited = NULL;
  guchar *pb_pixels = NULL;
  gint pb_n_channels, pb_bits_per_sample, pb_rowstride;
  gint pb_width, pb_height;
#if 0
  GdkRegion *clip;
  GdkRegion *drect;
  GdkRectangle tmp_rect;
#endif
  GdkDrawableImplDirectFB *impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  
  pb_n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  pb_bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  
  g_return_if_fail (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
  g_return_if_fail (pb_n_channels == 3 || pb_n_channels == 4);
  g_return_if_fail (pb_bits_per_sample == 8);
  
  g_return_if_fail (drawable != NULL);

  pb_width = gdk_pixbuf_get_width (pixbuf);
  pb_height = gdk_pixbuf_get_height (pixbuf);
  pb_pixels = gdk_pixbuf_get_pixels (pixbuf);
  pb_rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  if (width == -1) 
    width = pb_width;
  if (height == -1)
    height = pb_height;
  
  g_return_if_fail (width >= 0 && height >= 0);
  g_return_if_fail (src_x >= 0 && src_x + width <= pb_width);
  g_return_if_fail (src_y >= 0 && src_y + height <= pb_height);

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p, %p, %p, %4d,%4d -> %4d,%4d - %dx%d )\n",
              G_STRFUNC,
              drawable, gc, pixbuf, src_x, src_y, dest_x, dest_y, width, height);

  /* Clip to the drawable; this is required for get_from_drawable() so
   * can't be done implicitly
   */

  if (dest_x < 0)
    {
      src_x -= dest_x;
      width += dest_x;
      dest_x = 0;
    }

  if (dest_y < 0)
    {
      src_y -= dest_y;
      height += dest_y;
      dest_y = 0;
    }

  if ((dest_x + width) > impl->width)
    width = impl->width - dest_x;

  if ((dest_y + height) > impl->height)
    height = impl->height - dest_y;

  if (width <= 0 || height <= 0)
    return;

#if 0
  /* Clip to the clip region; this avoids getting more
   * image data from the server than we need to.
   */

  tmp_rect.x = dest_x;
  tmp_rect.y = dest_y;
  tmp_rect.width = width;
  tmp_rect.height = height;

  drect = gdk_region_rectangle (&tmp_rect);
  clip = gdk_drawable_get_clip_region (drawable);

  gdk_region_intersect (drect, clip);

  gdk_region_get_clipbox (drect, &tmp_rect);

  gdk_region_destroy (drect);
  gdk_region_destroy (clip);

  if (tmp_rect.width == 0 ||
      tmp_rect.height == 0)
    return;
#endif

  if (gdk_pixbuf_get_has_alpha (pixbuf) && impl->format == DSPF_RGB16) {
    void *data;
    int   pitch;

    if (impl->surface->Lock (impl->surface, DSLF_READ | DSLF_WRITE, &data, &pitch) == DFB_OK) {
      composite_565 (pb_pixels + src_y * pb_rowstride + src_x * 4,
                     pb_rowstride,
                     data + dest_y * pitch + dest_x * 2,
                     pitch,
#if G_BYTE_ORDER == G_BIG_ENDIAN
                     GDK_MSB_FIRST,
#else
                     GDK_LSB_FIRST,
#endif
                     width, height);

      impl->surface->Unlock (impl->surface);

      return;
    }
  }

  /* Actually draw */
  if (!gc)
    gc = _gdk_drawable_get_scratch_gc (drawable, FALSE);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    {
      GdkVisual *visual = gdk_drawable_get_visual (drawable);
      void (*composite_func) (guchar       *src_buf,
                              gint          src_rowstride,
                              guchar       *dest_buf,
                              gint          dest_rowstride,
                              GdkByteOrder  dest_byte_order,
                              gint          width,
                              gint          height) = NULL;

      /* First we see if we have a visual-specific composition function that can composite
       * the pixbuf data directly onto the image
       */
      if (visual)
        {
          gint bits_per_pixel = _gdk_windowing_get_bits_for_depth (gdk_drawable_get_display (drawable),
                                                                   visual->depth);

          if (visual->byte_order == (G_BYTE_ORDER == G_BIG_ENDIAN ? GDK_MSB_FIRST : GDK_LSB_FIRST) &&
              visual->depth == 16 &&
              visual->red_mask   == 0xf800 &&
              visual->green_mask == 0x07e0 &&
              visual->blue_mask  == 0x001f)
            composite_func = composite_565;
          else if (visual->depth == 24 && bits_per_pixel == 32 &&
                   visual->red_mask   == 0xff0000 &&
                   visual->green_mask == 0x00ff00 &&
                   visual->blue_mask  == 0x0000ff)
            composite_func = composite_0888;
        }

      /* We can't use our composite func if we are required to dither
       */
      if (composite_func && !(dither == GDK_RGB_DITHER_MAX && visual->depth != 24))
        {
#if 0
          gint x0, y0;
          for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
            {
              gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
              for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
                {
                  gint xs0, ys0;

                  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);

                  GdkImage *image = _gdk_image_get_scratch (gdk_drawable_get_screen (drawable),
                                                            width1, height1,
                                                            gdk_drawable_get_depth (drawable), &xs0, &ys0);

                  gdk_drawable_copy_to_image (drawable, image,
                                              dest_x + x0, dest_y + y0,
                                              xs0, ys0,
                                              width1, height1);
                  (*composite_func) (pb_pixels + (src_y + y0) * pb_rowstride + (src_x + x0) * 4,
                                     pb_rowstride,
                                     (guchar*)image->mem + ys0 * image->bpl + xs0 * image->bpp,
                                     image->bpl,
                                     visual->byte_order,
                                     width1, height1);
                  gdk_draw_image (drawable, gc, image,
                                  xs0, ys0,
                                  dest_x + x0, dest_y + y0,
                                  width1, height1);
                }
            }
#else
          void *data;
          int   pitch;

          if (impl->surface->Lock (impl->surface,
                                   DSLF_READ | DSLF_WRITE,
                                   &data, &pitch) == DFB_OK) {
            (*composite_func) (pb_pixels + src_y * pb_rowstride + src_x * 4,
                               pb_rowstride,
                               data + dest_y * pitch + DFB_BYTES_PER_LINE (impl->format, dest_x),
                               pitch,
                               visual->byte_order,
                               width, height);

            impl->surface->Unlock (impl->surface);
          }
#endif
          goto out;
        }
      else
        {
          /* No special composition func, convert dest to 24 bit RGB data, composite against
           * that, and convert back.
           */
          composited = gdk_pixbuf_get_from_drawable (NULL,
                                                     drawable,
                                                     NULL,
                                                     dest_x, dest_y,
                                                     0, 0,
                                                     width, height);

          if (composited)
            composite (pb_pixels + src_y * pb_rowstride + src_x * 4,
                       pb_rowstride,
                       gdk_pixbuf_get_pixels (composited),
                       gdk_pixbuf_get_rowstride (composited),
                       width, height);
        }
    }

  if (composited)
    {
      src_x = 0;
      src_y = 0;
      pixbuf = composited;
      pb_pixels = gdk_pixbuf_get_pixels (pixbuf);
      pb_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    }
  
  if (pb_n_channels == 4)
    {
      guchar *buf = pb_pixels + src_y * pb_rowstride + src_x * 4;

      gdk_draw_rgb_32_image_dithalign (drawable, gc,
                                       dest_x, dest_y,
                                       width, height,
                                       dither,
                                       buf, pb_rowstride,
                                       x_dither, y_dither);
    }
  else                                /* n_channels == 3 */
    {
      guchar *buf = pb_pixels + src_y * pb_rowstride + src_x * 3;

      gdk_draw_rgb_image_dithalign (drawable, gc,
                                    dest_x, dest_y,
                                    width, height,
                                    dither,
                                    buf, pb_rowstride,
                                    x_dither, y_dither);
    }

 out:
  if (composited)
    g_object_unref (composited);
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
drawable_impl_type_name (GObject *object)
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

  D_DEBUG_AT (GDKDFB_Drawable, "%s( %p ) <- %dx%d (%s at %4d,%4d)\n",
              G_STRFUNC,
              object, impl->width, impl->height,
              drawable_impl_type_name (object),
              impl->abs_x, impl->abs_y);

  gdk_directfb_set_colormap (GDK_DRAWABLE (object), NULL);
  if (impl->cairo_surface) {
    cairo_surface_finish (impl->cairo_surface);
  }
  if (impl->surface)
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

  drawable_class->create_gc              = _gdk_directfb_gc_new;
  drawable_class->draw_rectangle         = gdk_directfb_draw_rectangle;
  drawable_class->draw_arc               = gdk_directfb_draw_arc;
  drawable_class->draw_polygon           = gdk_directfb_draw_polygon;
  drawable_class->draw_text              = gdk_directfb_draw_text;
  drawable_class->draw_text_wc           = gdk_directfb_draw_text_wc;
  drawable_class->draw_drawable_with_src = gdk_directfb_draw_drawable;
  drawable_class->draw_points            = gdk_directfb_draw_points;
  drawable_class->draw_segments          = gdk_directfb_draw_segments;
  drawable_class->draw_lines             = gdk_directfb_draw_lines;
#if 0
  drawable_class->draw_glyphs             = NULL;
  drawable_class->draw_glyphs_transformed = NULL;
#endif
  drawable_class->draw_image     = gdk_directfb_draw_image;

  drawable_class->ref_cairo_surface = gdk_directfb_ref_cairo_surface;
  drawable_class->set_colormap      = gdk_directfb_set_colormap;
  drawable_class->get_colormap      = gdk_directfb_get_colormap;

  drawable_class->get_depth  = gdk_directfb_get_depth;
  drawable_class->get_visual = gdk_directfb_get_visual;

  drawable_class->get_size = gdk_directfb_get_size;

  drawable_class->_copy_to_image = _gdk_directfb_copy_to_image;
  drawable_class->get_screen     = gdk_directfb_get_screen;


  real_draw_pixbuf            = drawable_class->draw_pixbuf;
  drawable_class->draw_pixbuf = gdk_directfb_draw_pixbuf;

  /* check for hardware-accelerated alpha-blending */
  {
    DFBGraphicsDeviceDescription desc;
    _gdk_display->directfb->GetDeviceDescription (_gdk_display->directfb, &desc);

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

static GdkScreen *
gdk_directfb_get_screen (GdkDrawable *drawable)
{
  return gdk_screen_get_default ();
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
  dfb = GDK_DISPLAY_DFB (gdk_drawable_get_display (drawable))->directfb;

  if (!impl->cairo_surface) {
    IDirectFBSurface *surface;
    g_assert (impl->surface != NULL);
    impl->surface->GetSubSurface (impl->surface, NULL, &surface);
    if (surface) {
      impl->cairo_surface = cairo_directfb_surface_create (dfb, surface);
      if (impl->cairo_surface) {
        cairo_surface_set_user_data (impl->cairo_surface,
                                     &gdk_directfb_cairo_key, drawable,
                                     gdk_directfb_cairo_surface_destroy);
      }
      surface->Release (surface);
    }
  } else {
    cairo_surface_reference (impl->cairo_surface);
  }

  g_assert (impl->cairo_surface != NULL);
  return impl->cairo_surface;
}

#define __GDK_DRAWABLE_X11_C__
#include "gdkaliasdef.c"
