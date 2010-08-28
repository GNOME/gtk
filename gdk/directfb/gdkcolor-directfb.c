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
 * Copyright (C) 2002       convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdkcolor.h"
#include "gdkinternals.h"
#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"
#include "gdkalias.h"


typedef struct {
  GdkColorInfo     *info;
  IDirectFBPalette *palette;
} GdkColormapPrivateDirectFB;


static void  gdk_colormap_finalize (GObject *object);

static gint  gdk_colormap_alloc_pseudocolors (GdkColormap *colormap,
                                              GdkColor    *colors,
                                              gint         ncolors,
                                              gboolean     writeable,
                                              gboolean     best_match,
                                              gboolean    *success);
static void  gdk_directfb_allocate_color_key (GdkColormap *colormap);


G_DEFINE_TYPE (GdkColormap, gdk_colormap, G_TYPE_OBJECT)

static void
gdk_colormap_init (GdkColormap *colormap)
{
  colormap->size           = 0;
  colormap->colors         = NULL;
  colormap->windowing_data = NULL;
}

static void
gdk_colormap_class_init (GdkColormapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_colormap_finalize;
}

static void
gdk_colormap_finalize (GObject *object)
{
  GdkColormap                *colormap = GDK_COLORMAP (object);
  GdkColormapPrivateDirectFB *private  = colormap->windowing_data;

  g_free (colormap->colors);

  if (private)
    {
      g_free (private->info);

      if (private->palette)
        private->palette->Release (private->palette);

      g_free (private);
      colormap->windowing_data = NULL;
    }

  G_OBJECT_CLASS (gdk_colormap_parent_class)->finalize (object);
}

GdkColormap*
gdk_colormap_new (GdkVisual *visual,
                  gboolean   private_cmap)
{
  GdkColormap *colormap;
  gint         i;

  g_return_val_if_fail (visual != NULL, NULL);

  colormap = g_object_new (gdk_colormap_get_type (), NULL);
  colormap->visual = visual;
  colormap->size   = visual->colormap_size;

  switch (visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
      {
        IDirectFB                  *dfb = _gdk_display->directfb;
        IDirectFBPalette           *palette;
        GdkColormapPrivateDirectFB *private;
        DFBPaletteDescription       dsc;

        dsc.flags = DPDESC_SIZE;
        dsc.size  = colormap->size;
        if (!dfb->CreatePalette (dfb, &dsc, &palette))
          return NULL;

        colormap->colors = g_new0 (GdkColor, colormap->size);

        private = g_new0 (GdkColormapPrivateDirectFB, 1);
        private->info = g_new0 (GdkColorInfo, colormap->size);

	if (visual == gdk_visual_get_system ())
	  {
            /* save the first (transparent) palette entry */
            private->info[0].ref_count++;
          }

        private->palette = palette;

        colormap->windowing_data = private;

        gdk_directfb_allocate_color_key (colormap);
      }
      break;

    case GDK_VISUAL_STATIC_COLOR:
      colormap->colors = g_new0 (GdkColor, colormap->size);
      for (i = 0; i < colormap->size; i++)
        {
          GdkColor *color = colormap->colors + i;

          color->pixel = i;
          color->red   = (i & 0xE0) <<  8 | (i & 0xE0);
          color->green = (i & 0x1C) << 11 | (i & 0x1C) << 3;
          color->blue  = (i & 0x03) << 14 | (i & 0x03) << 6;
        }
      break;

    default:
      break;
    }

  return colormap;
}

GdkScreen*
gdk_colormap_get_screen (GdkColormap *cmap)
{
  return _gdk_screen;
}

GdkColormap*
gdk_screen_get_system_colormap (GdkScreen *screen)
{
  static GdkColormap *colormap = NULL;

  if (!colormap)
    {
      GdkVisual *visual = gdk_visual_get_system ();

      /* special case PSEUDO_COLOR to use the system palette */
      if (visual->type == GDK_VISUAL_PSEUDO_COLOR)
        {
          GdkColormapPrivateDirectFB *private;
          IDirectFBSurface           *surface;

          colormap = g_object_new (gdk_colormap_get_type (), NULL);

          colormap->visual = visual;
          colormap->size   = visual->colormap_size;
          colormap->colors = g_new0 (GdkColor, colormap->size);

          private = g_new0 (GdkColormapPrivateDirectFB, 1);
          private->info = g_new0 (GdkColorInfo, colormap->size);

          surface = GDK_WINDOW_IMPL_DIRECTFB (
                        GDK_WINDOW_OBJECT (_gdk_parent_root)->impl)->drawable.surface;
          surface->GetPalette (surface, &private->palette);

          colormap->windowing_data = private;

          /* save the first (transparent) palette entry */
          private->info[0].ref_count++;

          gdk_directfb_allocate_color_key (colormap);
        }
      else
        {
          colormap = gdk_colormap_new (visual, FALSE);
        }
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  GdkVisual *visual;

  visual = gdk_visual_get_system ();

  return visual->colormap_size;
}

void
gdk_colormap_change (GdkColormap *colormap,
                     gint         ncolors)
{
  g_message ("gdk_colormap_change() is deprecated and unimplemented");
}

gboolean
gdk_colors_alloc (GdkColormap   *colormap,
                  gboolean       contiguous,
                  gulong        *planes,
                  gint           nplanes,
                  gulong        *pixels,
                  gint           npixels)
{
  /* g_message ("gdk_colors_alloc() is deprecated and unimplemented"); */

  return TRUE;  /* return TRUE here to make GdkRGB happy */
}

void
gdk_colors_free (GdkColormap *colormap,
                 gulong      *in_pixels,
                 gint         in_npixels,
                 gulong       planes)
{
  /* g_message ("gdk_colors_free() is deprecated and unimplemented"); */
}

void
gdk_colormap_free_colors (GdkColormap    *colormap,
                          const GdkColor *colors,
                          gint            ncolors)
{
  GdkColormapPrivateDirectFB *private;
  gint                        i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (colors != NULL);

  private = colormap->windowing_data;
  if (!private)
    return;

  for (i = 0; i < ncolors; i++)
    {
      gint  index = colors[i].pixel;

      if (index < 0 || index >= colormap->size)
        continue;

      if (private->info[index].ref_count)
        private->info[index].ref_count--;
    }
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
                           GdkColor    *colors,
                           gint         ncolors,
                           gboolean     writeable,
                           gboolean     best_match,
                           gboolean    *success)
{
  GdkVisual *visual;
  gint       i;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), 0);
  g_return_val_if_fail (colors != NULL, 0);
  g_return_val_if_fail (success != NULL, 0);

  switch (colormap->visual->type)
    {
    case GDK_VISUAL_TRUE_COLOR:
      visual = colormap->visual;

      for (i = 0; i < ncolors; i++)
        {
          colors[i].pixel =
            (((colors[i].red
               >> (16 - visual->red_prec))   << visual->red_shift)   +
             ((colors[i].green
               >> (16 - visual->green_prec)) << visual->green_shift) +
             ((colors[i].blue
               >> (16 - visual->blue_prec))  << visual->blue_shift));

          success[i] = TRUE;
        }
      break;

    case GDK_VISUAL_PSEUDO_COLOR:
      return gdk_colormap_alloc_pseudocolors (colormap,
                                              colors, ncolors,
                                              writeable, best_match,
                                              success);
      break;

    case GDK_VISUAL_STATIC_COLOR:
      for (i = 0; i < ncolors; i++)
        {
          colors[i].pixel = (((colors[i].red   & 0xE000) >> 8)  |
                             ((colors[i].green & 0xE000) >> 11) |
                             ((colors[i].blue  & 0xC000) >> 14));
          success[i] = TRUE;
        }
      break;

    default:
      for (i = 0; i < ncolors; i++)
        success[i] = FALSE;
      break;
    }

  return 0;
}

gboolean
gdk_color_change (GdkColormap *colormap,
                  GdkColor    *color)
{
  GdkColormapPrivateDirectFB *private;
  IDirectFBPalette           *palette;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  private = colormap->windowing_data;
  if (!private)
    return FALSE;

  palette = private->palette;
  if (!palette)
    return FALSE;

  if (color->pixel < 0 || color->pixel >= colormap->size)
    return FALSE;

  if (private->info[color->pixel].flags & GDK_COLOR_WRITEABLE)
    {
      DFBColor  entry = { 0xFF,
                          color->red   >> 8,
                          color->green >> 8,
                          color->blue  >> 8 };

      if (palette->SetEntries (palette, &entry, 1, color->pixel) != DFB_OK)
        return FALSE;

      colormap->colors[color->pixel] = *color;
      return TRUE;
    }

  return FALSE;
}

void
gdk_colormap_query_color (GdkColormap *colormap,
                          gulong       pixel,
                          GdkColor    *result)
{
  GdkVisual *visual;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  visual = gdk_colormap_get_visual (colormap);

  switch (visual->type)
    {
    case GDK_VISUAL_TRUE_COLOR:
      result->red = 65535. *
        (gdouble)((pixel & visual->red_mask) >> visual->red_shift) /
        ((1 << visual->red_prec) - 1);

      result->green = 65535. *
        (gdouble)((pixel & visual->green_mask) >> visual->green_shift) /
        ((1 << visual->green_prec) - 1);

      result->blue = 65535. *
        (gdouble)((pixel & visual->blue_mask) >> visual->blue_shift) /
        ((1 << visual->blue_prec) - 1);
      break;

    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_PSEUDO_COLOR:
      if (pixel >= 0 && pixel < colormap->size)
        {
          result->red   = colormap->colors[pixel].red;
          result->green = colormap->colors[pixel].green;
          result->blue  = colormap->colors[pixel].blue;
        }
      else
        g_warning ("gdk_colormap_query_color: pixel outside colormap");
      break;

    case GDK_VISUAL_DIRECT_COLOR:
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_STATIC_GRAY:
      /* unsupported */
      g_assert_not_reached ();
      break;
    }
}

IDirectFBPalette *
gdk_directfb_colormap_get_palette (GdkColormap *colormap)
{
  GdkColormapPrivateDirectFB *private;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), NULL);

  private = colormap->windowing_data;

  if (private && private->palette)
    return private->palette;
  else
    return NULL;
}

static gint
gdk_colormap_alloc_pseudocolors (GdkColormap *colormap,
                                 GdkColor    *colors,
                                 gint         ncolors,
                                 gboolean     writeable,
                                 gboolean     best_match,
                                 gboolean    *success)
{
  GdkColormapPrivateDirectFB *private = colormap->windowing_data;
  IDirectFBPalette           *palette;
  gint                        i, j;
  gint                        remaining = ncolors;

  palette = private->palette;

  for (i = 0; i < ncolors; i++)
    {
      guint     index;
      DFBColor  lookup = { 0xFF,
                           colors[i].red   >> 8,
                           colors[i].green >> 8,
                           colors[i].blue  >> 8 };

      success[i] = FALSE;

      if (writeable)
        {
          /* look for an empty slot and allocate a new color */
          for (j = 0; j < colormap->size; j++)
            if (private->info[j].ref_count == 0)
              {
                index = j;

                palette->SetEntries (palette, &lookup, 1, index);

                private->info[index].flags = GDK_COLOR_WRITEABLE;

                colors[i].pixel = index;
                colormap->colors[index] = colors[i];

                goto allocated;
              }
        }
      else
        {
          palette->FindBestMatch (palette,
                                  lookup.r, lookup.g, lookup.b, lookup.a,
                                  &index);

          if (index < 0 || index > colormap->size)
            continue;

          /* check if we have an exact (non-writeable) match */
          if (private->info[index].ref_count &&
              !(private->info[index].flags & GDK_COLOR_WRITEABLE))
            {
              DFBColor  entry;

              palette->GetEntries (palette, &entry, 1, index);

              if (entry.a == 0xFF &&
                  entry.r == lookup.r && entry.g == lookup.g && entry.b == lookup.b)
                {
                  colors[i].pixel = index;

                  goto allocated;
                }
            }

          /* look for an empty slot and allocate a new color */
          for (j = 0; j < colormap->size; j++)
            if (private->info[j].ref_count == 0)
              {
                index = j;

                palette->SetEntries (palette, &lookup, 1, index);
    		private->info[index].flags = 0;

                colors[i].pixel = index;
                colormap->colors[index] = colors[i];

                goto allocated;
              }

          /* if that failed, use the best match */
          if (best_match &&
              !(private->info[index].flags & GDK_COLOR_WRITEABLE))
            {
#if 0
               g_print ("best match for (%d %d %d)  ",
                       colormap->colors[index].red,
                       colormap->colors[index].green,
                       colormap->colors[index].blue);
#endif

              colors[i].pixel = index;

              goto allocated;
            }
        }

      /* if we got here, all attempts failed */
      continue;

    allocated:
      private->info[index].ref_count++;

#if 0
      g_print ("cmap %p: allocated (%d %d %d) %d [%d]\n", colormap,
                colors[i].red, colors[i].green, colors[i].blue, colors[i].pixel,
	        private->info[index].ref_count);
#endif

      success[i] = TRUE;
      remaining--;
    }

  return remaining;
}

/* dirty hack for color_keying */
static void
gdk_directfb_allocate_color_key (GdkColormap *colormap)
{
  GdkColormapPrivateDirectFB *private = colormap->windowing_data;
  IDirectFBPalette           *palette = private->palette;

  if (!gdk_directfb_enable_color_keying)
    return;

  palette->SetEntries (palette, &gdk_directfb_bg_color, 1, 255);

  colormap->colors[255].pixel = 255;
  colormap->colors[255].red   = ((gdk_directfb_bg_color_key.r << 8)
                                 | gdk_directfb_bg_color_key.r);
  colormap->colors[255].green = ((gdk_directfb_bg_color_key.g << 8)
                                 | gdk_directfb_bg_color_key.g);
  colormap->colors[255].blue  = ((gdk_directfb_bg_color_key.b << 8)
                                 | gdk_directfb_bg_color_key.b);

  private->info[255].ref_count++;
}
#define __GDK_COLOR_X11_C__
#include "gdkaliasdef.c"
