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
#include <X11/Xlib.h>
#include "gdk.h"
#include "gdkprivate.h"


static gint  gdk_colormap_match_color (GdkColormap *cmap,
				       GdkColor    *color,
				       const gchar *available);
static void  gdk_colormap_add         (GdkColormap *cmap);
static void  gdk_colormap_remove      (GdkColormap *cmap);
static guint gdk_colormap_hash        (Colormap    *cmap);
static gint  gdk_colormap_cmp         (Colormap    *a,
				       Colormap    *b);
static void gdk_colormap_real_destroy (GdkColormap *colormap);

static GHashTable *colormap_hash = NULL;


GdkColormap*
gdk_colormap_new (GdkVisual *visual,
		  gint       private_cmap)
{
  GdkColormap *colormap;
  GdkColormapPrivate *private;
  Visual *xvisual;
  int size;
  int i;

  g_return_val_if_fail (visual != NULL, NULL);

  private = g_new (GdkColormapPrivate, 1);
  colormap = (GdkColormap*) private;

  private->xdisplay = gdk_display;
  private->visual = visual;
  private->next_color = 0;
  private->ref_count = 1;
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  colormap->size = visual->colormap_size;
  colormap->colors = g_new (GdkColor, colormap->size);

  switch (visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      private->private_val = private_cmap;
      private->xcolormap = XCreateColormap (private->xdisplay, gdk_root_window,
					    xvisual, (private_cmap) ? (AllocAll) : (AllocNone));

      if (private_cmap)
	{
	  XColor *default_colors;

	  default_colors = g_new (XColor, colormap->size);

	  for (i = 0; i < colormap->size; i++)
	    default_colors[i].pixel = i;

	  XQueryColors (private->xdisplay,
			DefaultColormap (private->xdisplay, gdk_screen),
			default_colors, colormap->size);

	  for (i = 0; i < colormap->size; i++)
	    {
	      colormap->colors[i].pixel = default_colors[i].pixel;
	      colormap->colors[i].red = default_colors[i].red;
	      colormap->colors[i].green = default_colors[i].green;
	      colormap->colors[i].blue = default_colors[i].blue;
	    }

	  gdk_colormap_change (colormap, colormap->size);
	  
	  g_free (default_colors);
	}
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      private->private_val = TRUE;
      private->xcolormap = XCreateColormap (private->xdisplay, gdk_root_window,
					    xvisual, AllocAll);

      size = 1 << visual->red_prec;
      for (i = 0; i < size; i++)
	colormap->colors[i].red = i * 65535 / (size - 1);

      size = 1 << visual->green_prec;
      for (i = 0; i < size; i++)
	colormap->colors[i].green = i * 65535 / (size - 1);

      size = 1 << visual->blue_prec;
      for (i = 0; i < size; i++)
	colormap->colors[i].blue = i * 65535 / (size - 1);

      gdk_colormap_change (colormap, colormap->size);
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      private->private_val = FALSE;
      private->xcolormap = XCreateColormap (private->xdisplay, gdk_root_window,
					    xvisual, AllocNone);
      break;
    }

  gdk_colormap_add (colormap);

  return colormap;
}

static void
gdk_colormap_real_destroy (GdkColormap *colormap)
{
  GdkColormapPrivate *private = (GdkColormapPrivate*) colormap;

  g_return_if_fail (colormap != NULL);

  if (private->ref_count > 0)
    return;

  gdk_colormap_remove (colormap);
  XFreeColormap (private->xdisplay, private->xcolormap);
  g_free (colormap->colors);
  g_free (colormap);
}

GdkColormap*
gdk_colormap_ref (GdkColormap *cmap)
{
  GdkColormapPrivate *private = (GdkColormapPrivate *)cmap;
  g_return_val_if_fail (cmap != NULL, NULL);

  private->ref_count += 1;
  return cmap;
}

void
gdk_colormap_unref (GdkColormap *cmap)
{
  GdkColormapPrivate *private = (GdkColormapPrivate *)cmap;
  g_return_if_fail (cmap != NULL);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    gdk_colormap_real_destroy (cmap);
}

GdkColormap*
gdk_colormap_get_system (void)
{
  static GdkColormap *colormap = NULL;
  GdkColormapPrivate *private;
  XColor *xpalette;
  gint i;

  if (!colormap)
    {
      private = g_new (GdkColormapPrivate, 1);
      colormap = (GdkColormap*) private;

      private->xdisplay = gdk_display;
      private->xcolormap = DefaultColormap (gdk_display, gdk_screen);
      private->visual = gdk_visual_get_system ();
      private->private_val = FALSE;
      private->next_color = 0;
      private->ref_count = 1;

      colormap->size = private->visual->colormap_size;
      colormap->colors = g_new (GdkColor, colormap->size);

      if ((private->visual->type == GDK_VISUAL_GRAYSCALE) ||
	  (private->visual->type == GDK_VISUAL_PSEUDO_COLOR))
	{
	  xpalette = g_new (XColor, colormap->size);
	  
	  for (i = 0; i < colormap->size; i++)
	    {
	      xpalette[i].pixel = i;
	      xpalette[i].red = 0;
	      xpalette[i].green = 0;
	      xpalette[i].blue = 0;
	    }
	  
	  XQueryColors (gdk_display, private->xcolormap, xpalette, 
			colormap->size);
	  
	  for (i = 0; i < colormap->size; i++)
	    {
	      colormap->colors[i].pixel = xpalette[i].pixel;
	      colormap->colors[i].red = xpalette[i].red;
	      colormap->colors[i].green = xpalette[i].green;
	      colormap->colors[i].blue = xpalette[i].blue;
	    }

	  g_free (xpalette);
	}

      gdk_colormap_add (colormap);
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  return DisplayCells (gdk_display, gdk_screen);
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
  GdkColormapPrivate *private;
  GdkVisual *visual;
  XColor *palette;
  gint shift;
  int max_colors;
  int size;
  int i;

  g_return_if_fail (colormap != NULL);

  palette = g_new (XColor, ncolors);

  private = (GdkColormapPrivate*) colormap;
  switch (private->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      for (i = 0; i < ncolors; i++)
	{
	  palette[i].pixel = colormap->colors[i].pixel;
	  palette[i].red = colormap->colors[i].red;
	  palette[i].green = colormap->colors[i].green;
	  palette[i].blue = colormap->colors[i].blue;
	  palette[i].flags = DoRed | DoGreen | DoBlue;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, ncolors);
      private->next_color = MAX (private->next_color, ncolors);
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      visual = private->visual;

      shift = visual->red_shift;
      max_colors = 1 << visual->red_prec;
      size = (ncolors < max_colors) ? (ncolors) : (max_colors);

      for (i = 0; i < size; i++)
	{
	  palette[i].pixel = i << shift;
	  palette[i].red = colormap->colors[i].red;
	  palette[i].flags = DoRed;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, size);

      shift = visual->green_shift;
      max_colors = 1 << visual->green_prec;
      size = (ncolors < max_colors) ? (ncolors) : (max_colors);

      for (i = 0; i < size; i++)
	{
	  palette[i].pixel = i << shift;
	  palette[i].green = colormap->colors[i].green;
	  palette[i].flags = DoGreen;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, size);

      shift = visual->blue_shift;
      max_colors = 1 << visual->blue_prec;
      size = (ncolors < max_colors) ? (ncolors) : (max_colors);

      for (i = 0; i < size; i++)
	{
	  palette[i].pixel = i << shift;
	  palette[i].blue = colormap->colors[i].blue;
	  palette[i].flags = DoBlue;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, size);
      break;

    default:
      break;
    }

  g_free (palette);
}

void
gdk_colors_store (GdkColormap   *colormap,
		  GdkColor      *colors,
		  gint           ncolors)
{
  gint i;

  for (i = 0; i < ncolors; i++)
    {
      colormap->colors[i].pixel = colors[i].pixel;
      colormap->colors[i].red = colors[i].red;
      colormap->colors[i].green = colors[i].green;
      colormap->colors[i].blue = colors[i].blue;
    }

  gdk_colormap_change (colormap, ncolors);
}

gint
gdk_colors_alloc (GdkColormap   *colormap,
		  gint           contiguous,
		  gulong        *planes,
		  gint           nplanes,
		  gulong        *pixels,
		  gint           npixels)
{
  GdkColormapPrivate *private;
  gint return_val;

  g_return_val_if_fail (colormap != NULL, 0);

  private = (GdkColormapPrivate*) colormap;

  return_val = XAllocColorCells (private->xdisplay, private->xcolormap,
				 contiguous, planes, nplanes, pixels, npixels);

  return return_val;
}

void
gdk_colors_free (GdkColormap *colormap,
		 gulong      *pixels,
		 gint         npixels,
		 gulong       planes)
{
  GdkColormapPrivate *private;

  g_return_if_fail (colormap != NULL);

  private = (GdkColormapPrivate*) colormap;

  XFreeColors (private->xdisplay, private->xcolormap,
	       pixels, npixels, planes);
}

gint
gdk_color_white (GdkColormap *colormap,
		 GdkColor    *color)
{
  gint return_val;

  g_return_val_if_fail (colormap != NULL, FALSE);

  if (color)
    {
      color->pixel = WhitePixel (gdk_display, gdk_screen);
      color->red = 65535;
      color->green = 65535;
      color->blue = 65535;

      return_val = gdk_color_alloc (colormap, color);
    }
  else
    return_val = FALSE;

  return return_val;
}

gint
gdk_color_black (GdkColormap *colormap,
		 GdkColor    *color)
{
  gint return_val;

  g_return_val_if_fail (colormap != NULL, FALSE);

  if (color)
    {
      color->pixel = BlackPixel (gdk_display, gdk_screen);
      color->red = 0;
      color->green = 0;
      color->blue = 0;

      return_val = gdk_color_alloc (colormap, color);
    }
  else
    return_val = FALSE;

  return return_val;
}

gint
gdk_color_parse (const gchar *spec,
		 GdkColor *color)
{
  Colormap xcolormap;
  XColor xcolor;
  gint return_val;

  g_return_val_if_fail (spec != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  xcolormap = DefaultColormap (gdk_display, gdk_screen);

  if (XParseColor (gdk_display, xcolormap, spec, &xcolor))
    {
      return_val = TRUE;
      color->red = xcolor.red;
      color->green = xcolor.green;
      color->blue = xcolor.blue;
    }
  else
    return_val = FALSE;

  return return_val;
}

gint
gdk_color_alloc (GdkColormap *colormap,
		 GdkColor    *color)
{
  GdkColormapPrivate *private;
  GdkVisual *visual;
  XColor xcolor;
  gchar *available = NULL;
  gint return_val;
  gint i, index;

  g_return_val_if_fail (colormap != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  xcolor.red = color->red;
  xcolor.green = color->green;
  xcolor.blue = color->blue;
  xcolor.pixel = color->pixel;
  xcolor.flags = DoRed | DoGreen | DoBlue;

  return_val = FALSE;
  private = (GdkColormapPrivate*) colormap;

  switch (private->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      if (private->private_val)
	{
	  if (private->next_color >= colormap->size)
	    {
	      available = g_new (gchar, colormap->size);
	      for (i = 0; i < colormap->size; i++)
		available[i] = TRUE;

	      index = gdk_colormap_match_color (colormap, color, available);
	      if (index != -1)
		{
		  available[index] = FALSE;
		  *color = colormap->colors[index];
		  return_val = TRUE;
		}
	      else
		{
		  return_val = FALSE;
		}
	    }
	  else
	    {
	      xcolor.pixel = colormap->size - 1 -private->next_color;
	      color->pixel = xcolor.pixel;
	      private->next_color += 1;

	      XStoreColor (private->xdisplay, private->xcolormap, &xcolor);
	      return_val = TRUE;
	    }
	}
      else
	{
	  while (1)
	    {
	      if (XAllocColor (private->xdisplay, private->xcolormap, &xcolor))
		{
		  color->pixel = xcolor.pixel;
		  color->red = xcolor.red;
		  color->green = xcolor.green;
		  color->blue = xcolor.blue;

		  if (color->pixel < colormap->size)
		    colormap->colors[color->pixel] = *color;

		  return_val = TRUE;
		  break;
		}
	      else
		{
		  if (available == NULL)
		    {
		      available = g_new (gchar, colormap->size);
		      for (i = 0; i < colormap->size; i++)
			available[i] = TRUE;
		    }

		  index = gdk_colormap_match_color (colormap, color, available);
		  if (index != -1)
		    {
		      available[index] = FALSE;
		      xcolor.red = colormap->colors[index].red;
		      xcolor.green = colormap->colors[index].green;
		      xcolor.blue = colormap->colors[index].blue;
		    }
		  else
		    {
		      return_val = FALSE;
		      break;
		    }
		}
	    }
	}
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      visual = private->visual;
      xcolor.pixel = (((xcolor.red >> (16 - visual->red_prec)) << visual->red_shift) +
		      ((xcolor.green >> (16 - visual->green_prec)) << visual->green_shift) +
		      ((xcolor.blue >> (16 - visual->blue_prec)) << visual->blue_shift));
      color->pixel = xcolor.pixel;
      return_val = TRUE;
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      if (XAllocColor (private->xdisplay, private->xcolormap, &xcolor))
	{
	  color->pixel = xcolor.pixel;
	  return_val = TRUE;
	}
      else
	return_val = FALSE;
      break;
    }

  if (available)
    g_free (available);
  
  return return_val;
}

gint
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
  GdkColormapPrivate *private;
  XColor xcolor;

  g_return_val_if_fail (colormap != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  xcolor.pixel = color->pixel;
  xcolor.red = color->red;
  xcolor.green = color->green;
  xcolor.blue = color->blue;
  xcolor.flags = DoRed | DoGreen | DoBlue;

  private = (GdkColormapPrivate*) colormap;
  XStoreColor (private->xdisplay, private->xcolormap, &xcolor);

  return TRUE;
}

gint
gdk_color_equal (GdkColor *colora,
		 GdkColor *colorb)
{
  g_return_val_if_fail (colora != NULL, FALSE);
  g_return_val_if_fail (colorb != NULL, FALSE);

  return ((colora->red == colorb->red) &&
	  (colora->green == colorb->green) &&
	  (colora->blue == colorb->blue));
}

GdkColormap*
gdkx_colormap_get (Colormap xcolormap)
{
  GdkColormap *colormap;
  GdkColormapPrivate *private;

  colormap = gdk_colormap_lookup (xcolormap);
  if (colormap)
    return colormap;

  if (xcolormap == DefaultColormap (gdk_display, gdk_screen))
    return gdk_colormap_get_system ();

  private = g_new (GdkColormapPrivate, 1);
  colormap = (GdkColormap*) private;

  private->xdisplay = gdk_display;
  private->xcolormap = xcolormap;
  private->visual = NULL;
  private->private_val = TRUE;
  private->next_color = 0;

  /* To do the following safely, we would have to have some way of finding
   * out what the size or visual of the given colormap is. It seems
   * X doesn't allow this
   */

#if 0
  for (i = 0; i < 256; i++)
    {
      xpalette[i].pixel = i;
      xpalette[i].red = 0;
      xpalette[i].green = 0;
      xpalette[i].blue = 0;
    }

  XQueryColors (gdk_display, private->xcolormap, xpalette, 256);

  for (i = 0; i < 256; i++)
    {
      colormap->colors[i].pixel = xpalette[i].pixel;
      colormap->colors[i].red = xpalette[i].red;
      colormap->colors[i].green = xpalette[i].green;
      colormap->colors[i].blue = xpalette[i].blue;
    }
#endif

  colormap->colors = NULL;
  colormap->size = 0;

  gdk_colormap_add (colormap);

  return colormap;
}


static gint
gdk_colormap_match_color (GdkColormap *cmap,
			  GdkColor    *color,
			  const gchar *available)
{
  GdkColor *colors;
  guint sum, max;
  gint rdiff, gdiff, bdiff;
  gint i, index;

  g_return_val_if_fail (cmap != NULL, 0);
  g_return_val_if_fail (color != NULL, 0);

  colors = cmap->colors;
  max = 3 * (65536);
  index = -1;

  for (i = 0; i < cmap->size; i++)
    {
      if ((!available) || (available && available[i]))
	{
	  rdiff = (color->red - colors[i].red);
	  gdiff = (color->green - colors[i].green);
	  bdiff = (color->blue - colors[i].blue);

	  sum = ABS (rdiff) + ABS (gdiff) + ABS (bdiff);

	  if (sum < max)
	    {
	      index = i;
	      max = sum;
	    }
	}
    }

  return index;
}


GdkColormap*
gdk_colormap_lookup (Colormap xcolormap)
{
  GdkColormap *cmap;

  if (!colormap_hash)
    return NULL;

  cmap = g_hash_table_lookup (colormap_hash, &xcolormap);
  return cmap;
}

static void
gdk_colormap_add (GdkColormap *cmap)
{
  GdkColormapPrivate *private;

  if (!colormap_hash)
    colormap_hash = g_hash_table_new ((GHashFunc) gdk_colormap_hash,
				      (GCompareFunc) gdk_colormap_cmp);

  private = (GdkColormapPrivate*) cmap;

  g_hash_table_insert (colormap_hash, &private->xcolormap, cmap);
}

static void
gdk_colormap_remove (GdkColormap *cmap)
{
  GdkColormapPrivate *private;

  if (!colormap_hash)
    colormap_hash = g_hash_table_new ((GHashFunc) gdk_colormap_hash,
				      (GCompareFunc) gdk_colormap_cmp);

  private = (GdkColormapPrivate*) cmap;

  g_hash_table_remove (colormap_hash, &private->xcolormap);
}

static guint
gdk_colormap_hash (Colormap *cmap)
{
  return *cmap;
}

static gint
gdk_colormap_cmp (Colormap *a,
		  Colormap *b)
{
  return (*a == *b);
}
