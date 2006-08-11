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

#include <config.h>
#include "gdk.h"
#include <assert.h>

#include <string.h>

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"


#include "gdkregion-generic.h"
#include "gdkalias.h"

#include "cairo-directfb.h"

#define WARN_UNIMPLEMENTED(func)\
{\
  static gboolean first_call = TRUE;\
  if (first_call)\
    {\
                        g_message ("unimplemented " func);\
      first_call = FALSE;\
    }\
}

static GdkScreen * gdk_directfb_get_screen (GdkDrawable    *drawable);
static void gdk_drawable_impl_directfb_class_init (GdkDrawableImplDirectFBClass *klass);
static void gdk_directfb_draw_lines (GdkDrawable *drawable,
                                     GdkGC       *gc,
                                     GdkPoint    *points,
                                     gint         npoints);

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
static GdkRegion *
gdk_directfb_clip_region (GdkDrawable  *drawable,
                          GdkGC        *gc,
                          GdkRectangle *draw_rect)
{
  GdkDrawableImplDirectFB *private;
  GdkRegion               *clip_region;
  GdkRegion               *tmpreg;
  GdkRectangle             rect;

  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_DIRECTFB (drawable), NULL);

  private = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!draw_rect)
    {
      rect.x      = 0;
      rect.y      = 0;
      rect.width  = private->width;
      rect.height = private->height;

      draw_rect = &rect;
    }

  clip_region = gdk_region_rectangle (draw_rect);

  if (private->buffered && private->paint_region)
    gdk_region_intersect (clip_region, private->paint_region);

  if (gc)
    {
      GdkGCDirectFB *gc_private = GDK_GC_DIRECTFB (gc);
      GdkRegion     *region     = gc_private->clip_region;

      if (region)
        {
          if (gc->clip_x_origin || gc->clip_y_origin)
            {
              tmpreg = gdk_region_copy (region);

              gdk_region_offset (tmpreg, gc->clip_x_origin, gc->clip_y_origin);
              gdk_region_intersect (clip_region, tmpreg);
              gdk_region_destroy (tmpreg);
            }
          else
            {
              gdk_region_intersect (clip_region, region);
            }
        }

      if (gc_private->values_mask & GDK_GC_SUBWINDOW &&
          gc_private->values.subwindow_mode == GDK_INCLUDE_INFERIORS)
        return clip_region;
    }

  if (GDK_IS_WINDOW (private->wrapper) &&
      GDK_WINDOW_IS_MAPPED (private->wrapper) &&
      !GDK_WINDOW_OBJECT (private->wrapper)->input_only)
    {
      GList *cur;

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

          rect.x      = cur_private->x;
          rect.y      = cur_private->y;
          rect.width  = cur_impl->width;
          rect.height = cur_impl->height;

          tmpreg = gdk_region_rectangle (&rect);
          gdk_region_subtract (clip_region, tmpreg);
          gdk_region_destroy (tmpreg);
        }
    }

  return clip_region;
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

void
_gdk_directfb_draw_rectangle (GdkDrawable *drawable,
                              GdkGC       *gc,
                              gint         filled,
                              gint         x,
                              gint         y,
                              gint         width,
                              gint         height)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion               *clip;
  GdkGCDirectFB           *gc_private = NULL;
  IDirectFBSurface        *surface    = NULL;
  gint  i;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

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
          if (!gdk_directfb_setup_for_drawing (impl, gc_private)){
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

      clip = gdk_directfb_clip_region (drawable, gc, &rect);

      if (gc_private && gc_private->values_mask & GDK_GC_FILL)
        {
          if (gc_private->values.fill == GDK_STIPPLED  &&
              gc_private->values_mask & GDK_GC_STIPPLE &&
              gc_private->values.stipple)
            {
              surface = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_PIXMAP_OBJECT (gc_private->values.stipple)->impl)->surface;

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
            }
        }

      if (surface)
        {
          if (gc_private->values_mask & GDK_GC_TS_X_ORIGIN)
            x = gc_private->values.ts_x_origin;
          if (gc_private->values_mask & GDK_GC_TS_Y_ORIGIN)
            y = gc_private->values.ts_y_origin;

          for (i = 0; i < clip->numRects; i++)
            {
              DFBRegion reg = { clip->rects[i].x1,     clip->rects[i].y1,
                                clip->rects[i].x2, clip->rects[i].y2 };

              impl->surface->SetClip (impl->surface, &reg);
              impl->surface->TileBlit (impl->surface, surface, NULL, x, y);
            }

          impl->surface->SetBlittingFlags (impl->surface, DSBLIT_NOFX);
          impl->surface->SetClip (impl->surface, NULL);
        }
      else  /* normal rectangle filling */
        {
          for (i = 0; i < clip->numRects; i++)
            {
              DFBRegion *region = (DFBRegion *) &clip->rects[i];

              impl->surface->FillRectangle (impl->surface,
                                            region->x1,
                                            region->y1,
                                            region->x2 - region->x1,
                                            region->y2 - region->y1);
            }
        }

      gdk_region_destroy (clip);
    }
  else
    {

      DFBRegion region = { x, y, x + width, y + height };
      impl->surface->SetClip (impl->surface, &region);

      /*  DirectFB does not draw rectangles the X way. Using DirectFB,
          a filled Rectangle has the same size as a drawn one, while
          X draws the rectangle one pixel taller and wider.  */
      impl->surface->DrawRectangle (impl->surface,
                                    x, y, width , height);
      impl->surface->SetClip (impl->surface, NULL);
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
  WARN_UNIMPLEMENTED (G_GNUC_FUNCTION);
}

static void
gdk_directfb_draw_polygon (GdkDrawable *drawable,
                           GdkGC       *gc,
                           gint         filled,
                           GdkPoint    *points,
                           gint         npoints)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));

  if (npoints < 3)
    return;

  if (filled)
    {
                if (npoints == 3 ||
                                (npoints == 4 && 
                                 points[0].x == points[npoints-1].x &&
                                 points[0].y == points[npoints-1].y))
          {
            GdkDrawableImplDirectFB *impl;
            GdkRegion               *clip;
            gint                     i;

            impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

            if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
              return;

            clip = gdk_directfb_clip_region (drawable, gc, NULL);

            for (i = 0; i < clip->numRects; i++)
              {
                                DFBRegion reg = { clip->rects[i].x1,     clip->rects[i].y1, 
                    clip->rects[i].x2 , clip->rects[i].y2  };

                impl->surface->SetClip (impl->surface, &reg);
                impl->surface->FillTriangle (impl->surface,
                                             points[0].x, points[0].y,
                                             points[1].x, points[1].y,
                                             points[2].x, points[2].y);

              }
            impl->surface->SetClip (impl->surface, NULL);
            gdk_region_destroy (clip);

            return;
          }
                else
                        g_message ("filled polygons with n > 3 are not yet supported, "
                     "drawing outlines");
    }

  if (points[0].x != points[npoints-1].x ||
      points[0].y != points[npoints-1].y)
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
  WARN_UNIMPLEMENTED (G_GNUC_FUNCTION);
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
  WARN_UNIMPLEMENTED (G_GNUC_FUNCTION);
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
                            gint         height)
{
  GdkDrawableImplDirectFB *impl;
  GdkDrawableImplDirectFB *src_impl;
  GdkRegion               *clip;
  GdkRectangle             dest_rect = { xdest,
                                         ydest,
                xdest + width ,
                ydest + height};

  DFBRectangle rect = { xsrc, ysrc, width, height };
  gint i;

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

  clip = gdk_directfb_clip_region (drawable, gc, &dest_rect);

  for (i = 0; i < clip->numRects; i++)
    {
      DFBRegion reg = { clip->rects[i].x1,     clip->rects[i].y1,
                        clip->rects[i].x2 , clip->rects[i].y2 };

      impl->surface->SetClip (impl->surface, &reg);
      impl->surface->Blit (impl->surface, src_impl->surface, &rect,
                           xdest, ydest);
    }
  impl->surface->SetClip (impl->surface, NULL);
  gdk_region_destroy (clip);
}

static void
gdk_directfb_draw_points (GdkDrawable *drawable,
                          GdkGC       *gc,
                          GdkPoint    *points,
                          gint         npoints)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion               *clip;

  DFBRegion region = { points->x, points->y, points->x, points->y };

  if (npoints < 1)
    return;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
    return;

  clip = gdk_directfb_clip_region (drawable, gc, NULL);

  while (npoints > 0)
    {
      if (gdk_region_point_in (clip, points->x, points->y))
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

  gdk_region_destroy (clip);
}

static void
gdk_directfb_draw_segments (GdkDrawable *drawable,
                            GdkGC       *gc,
                            GdkSegment  *segs,
                            gint         nsegs)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion               *clip;
  gint                     i;

  DFBRegion region = { segs->x1, segs->y1, segs->x2, segs->y2 };

  if (nsegs < 1)
    return;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

  if (!gdk_directfb_setup_for_drawing (impl, GDK_GC_DIRECTFB (gc)))
    return;

  clip = gdk_directfb_clip_region (drawable, gc, NULL);

  for (i = 0; i < clip->numRects; i++)
    {
      DFBRegion reg = { clip->rects[i].x1,   clip->rects[i].y1,
                        clip->rects[i].x2, clip->rects[i].y2 };

      impl->surface->SetClip (impl->surface, &reg);

      impl->surface->DrawLines (impl->surface, (DFBRegion *)segs, nsegs);
    }

  impl->surface->SetClip (impl->surface, NULL);

  gdk_region_destroy (clip);

  /* everything below can be omitted if the drawing is buffered */
  if (impl->buffered)
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
    }
}

static void
gdk_directfb_draw_lines (GdkDrawable *drawable,
                         GdkGC       *gc,
                         GdkPoint    *points,
                         gint         npoints)
{
  GdkDrawableImplDirectFB *impl;
  GdkRegion               *clip;
  gint                     i;

  DFBRegion lines[npoints > 1 ? npoints - 1 : 1];

  DFBRegion region = { points->x, points->y, points->x, points->y };

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

  clip = gdk_directfb_clip_region (drawable, gc, NULL);

  for (i = 0; i < clip->numRects; i++)
    {
      DFBRegion reg = { clip->rects[i].x1,   clip->rects[i].y1,
                        clip->rects[i].x2, clip->rects[i].y2 };

      impl->surface->SetClip (impl->surface, &reg);
      impl->surface->DrawLines (impl->surface, lines, npoints - 1);
    }

  impl->surface->SetClip (impl->surface, NULL);

  gdk_region_destroy (clip);
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
  GdkRegion               *clip;
  GdkRectangle             dest_rect = { xdest, ydest, width, height };

  gint pitch = 0;
  gint i;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (image != NULL);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
  image_private = image->windowing_data;

  if (!impl->surface)
    return;

  clip = gdk_directfb_clip_region (drawable, gc, &dest_rect);

  if (!gdk_region_empty (clip))
    {
      DFBRectangle  src_rect = { xsrc, ysrc, width, height };

      image_private->surface->Unlock (image_private->surface);

      for (i = 0; i < clip->numRects; i++)
        {
          DFBRegion reg = { clip->rects[i].x1,     clip->rects[i].y1,
                            clip->rects[i].x2 , clip->rects[i].y2  };

          impl->surface->SetClip (impl->surface, &reg);
          impl->surface->Blit (impl->surface,
                               image_private->surface, &src_rect,
                               xdest, ydest);
        }
      impl->surface->SetClip (impl->surface, NULL);

      image_private->surface->Lock (image_private->surface, DSLF_WRITE,
                                    &image->mem, &pitch);
      image->bpl = pitch;
    }

  gdk_region_destroy (clip);
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

static void
gdk_drawable_impl_directfb_finalize (GObject *object)
{
  GdkDrawableImplDirectFB *impl;
  impl = GDK_DRAWABLE_IMPL_DIRECTFB (object);

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
  drawable_class->draw_rectangle = _gdk_directfb_draw_rectangle;
  drawable_class->draw_arc       = gdk_directfb_draw_arc;
  drawable_class->draw_polygon   = gdk_directfb_draw_polygon;
  drawable_class->draw_text      = gdk_directfb_draw_text;
  drawable_class->draw_text_wc   = gdk_directfb_draw_text_wc;
  drawable_class->draw_drawable  = gdk_directfb_draw_drawable;
  drawable_class->draw_points    = gdk_directfb_draw_points;
  drawable_class->draw_segments  = gdk_directfb_draw_segments;
  drawable_class->draw_lines     = gdk_directfb_draw_lines;
#if 0
  drawable_class->draw_glyphs    = NULL;
  drawable_class->draw_glyphs_transformed    = NULL;
#endif
  drawable_class->draw_image     = gdk_directfb_draw_image;

  drawable_class->ref_cairo_surface = gdk_directfb_ref_cairo_surface;
  drawable_class->set_colormap   = gdk_directfb_set_colormap;
  drawable_class->get_colormap   = gdk_directfb_get_colormap;

  drawable_class->get_depth      = gdk_directfb_get_depth;
  drawable_class->get_visual     = gdk_directfb_get_visual;

  drawable_class->get_size       = gdk_directfb_get_size;

  drawable_class->_copy_to_image = _gdk_directfb_copy_to_image;
        drawable_class->get_screen = gdk_directfb_get_screen;

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
      static const GTypeInfo object_info =
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


static cairo_surface_t *
gdk_directfb_ref_cairo_surface (GdkDrawable *drawable)
{
    g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
    g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_DIRECTFB (drawable), NULL);

    GdkDrawableImplDirectFB *impl = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
    IDirectFB *dfb = GDK_DISPLAY_DFB(gdk_drawable_get_display(drawable))->directfb;
    if (!impl->cairo_surface) {
//      IDirectFBSurface *surface;
     // if (impl->surface->GetSubSurface (impl->surface, NULL, &surface) == DFB_OK) {
        //impl->cairo_surface = cairo_directfb_surface_create (dfb, surface);
        g_assert( impl->surface != NULL);
        impl->cairo_surface = cairo_directfb_surface_create (dfb,impl->surface);
        g_assert( impl->cairo_surface != NULL);
        cairo_surface_set_user_data (impl->cairo_surface, 
                                     &gdk_directfb_cairo_key, drawable, 
                                     gdk_directfb_cairo_surface_destroy);
       // surface->Release (surface);
      //}
    } else {
        cairo_surface_reference (impl->cairo_surface);
    }
  g_assert( impl->cairo_surface != NULL);
  return impl->cairo_surface;
}

#define __GDK_DRAWABLE_X11_C__
#include "gdkaliasdef.c"
