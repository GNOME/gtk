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
#include <time.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#include "gdkcolor.h"
#include "gdkprivate-fb.h"

#define GDK_COLORMAP_PRIVATE_DATA(cmap) ((GdkColormapPrivateFB *) GDK_COLORMAP (cmap)->windowing_data)

static gint  gdk_colormap_match_color (GdkColormap *cmap,
				       GdkColor    *color,
				       const gchar *available);
static void  gdk_fb_color_round_to_hw (GdkColor *color);

static gpointer parent_class;

static void
gdk_colormap_finalize (GObject *object)
{
  GdkColormap *colormap = GDK_COLORMAP (object);
  GdkColormapPrivateFB *private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if (private->hash)
    g_hash_table_destroy (private->hash);
  
  g_free (private->info);
  g_free (colormap->colors);
  g_free (private);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_colormap_init (GdkColormap *colormap)
{
  GdkColormapPrivateFB *private;

  private = g_new (GdkColormapPrivateFB, 1);

  colormap->windowing_data = private;
  
  colormap->size = 0;
  colormap->colors = NULL;
}

static void
gdk_colormap_class_init (GdkColormapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_colormap_finalize;
}

GType
gdk_colormap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkColormapClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_colormap_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkColormap),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_colormap_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkColormap",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkColormap *
gdk_colormap_new (GdkVisual *visual,
		  gint       private_cmap)
{
  GdkColormap *colormap;
  GdkColormap *system;
  GdkColormapPrivateFB *private;
  GdkFBDisplay *fbd;
  int i;

  g_return_val_if_fail (visual != NULL, NULL);

  colormap = g_object_new (gdk_colormap_get_type (), NULL);
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  colormap->visual = visual;
  fbd = gdk_display;

  private->hash = NULL;
  
  colormap->size = visual->colormap_size;
  colormap->colors = NULL;

  switch (visual->type)
    {
    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      private->info = g_new0 (GdkColorInfo, colormap->size);
      colormap->colors = g_new (GdkColor, colormap->size);
      
      private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					(GEqualFunc) gdk_color_equal);

      system = gdk_colormap_get_system ();
      memcpy (colormap->colors, system->colors, colormap->size * sizeof (GdkColor));
      
      if (private_cmap)
	{
	  guint16 red[256], green[256], blue[256];
	  struct fb_cmap fbc = {0, 256};

	  fbc.red = red;
	  fbc.green = green;
	  fbc.blue = blue;

	  if (ioctl (fbd->fb_fd, FBIOGETCMAP, &fbc))
	    g_error("ioctl(FBIOGETCMAP) failed");

	  for (i = 0; i < colormap->size; i++)
	    {
	      colormap->colors[i].pixel = i;
	      colormap->colors[i].red = red[i];
	      colormap->colors[i].green = green[i];
	      colormap->colors[i].blue = blue[i];
	    }

	  gdk_colormap_change (colormap, colormap->size);
	}
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      g_warning ("gdk_colormap_new () on a direct color visual not implemented");
#if 0
      colormap->colors = g_new (GdkColor, colormap->size);

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
#endif
      break;

    default:
      g_assert_not_reached ();

    case GDK_VISUAL_TRUE_COLOR:
      break;
    }

  return colormap;
}

GdkColormap*
gdk_screen_get_system_colormap (GdkScreen *screen)
{
  static GdkColormap *colormap = NULL;

  if (!colormap)
    {
      GdkColormapPrivateFB *private;
      GdkVisual *visual = gdk_visual_get_system ();
      int i, r, g, b;
      
      colormap = g_object_new (gdk_colormap_get_type (), NULL);
      private = GDK_COLORMAP_PRIVATE_DATA (colormap);

      colormap->visual = visual;
      private->hash = NULL;
      
      colormap->size = visual->colormap_size;
      colormap->colors = NULL;
      
      switch (visual->type)
	{
	case GDK_VISUAL_STATIC_GRAY:
	case GDK_VISUAL_STATIC_COLOR:
	case GDK_VISUAL_GRAYSCALE:
	case GDK_VISUAL_PSEUDO_COLOR:
	  private->info = g_new0 (GdkColorInfo, colormap->size);
	  colormap->colors = g_new (GdkColor, colormap->size);
	  
	  private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					    (GEqualFunc) gdk_color_equal);
	  switch(visual->type)
	    {
	    case GDK_VISUAL_GRAYSCALE:
	      for(i = 0; i < 256; i++) {
		colormap->colors[i].red = 
		  colormap->colors[i].green =
		  colormap->colors[i].blue = i << 8;
		gdk_fb_color_round_to_hw (&colormap->colors[i]);
	      }
	      i--;
	      colormap->colors[i].red = 
		colormap->colors[i].green =
		colormap->colors[i].blue = 65535; /* Make it a true white */
	      gdk_fb_color_round_to_hw (&colormap->colors[i]);
	      break;
	    case GDK_VISUAL_PSEUDO_COLOR:
	      /* Color cube stolen from gdkrgb upon advice from Owen */
	      for(i = r = 0; r < 6; r++)
		for(g = 0; g < 6; g++)
		  for(b = 0; b < 6; b++)
		    {
		      colormap->colors[i].red = r * 65535 / 5;
		      colormap->colors[i].green = g * 65535 / 5;
		      colormap->colors[i].blue = b * 65535 / 5;
		      gdk_fb_color_round_to_hw (&colormap->colors[i]);
		      i++;
		    }
	      g_assert (i == 216);
	      /* Fill in remaining space with grays */
	      for(i = 216; i < 256; i++)
		{
		  colormap->colors[i].red = 
		    colormap->colors[i].green =
		    colormap->colors[i].blue = (i - 216) * 40;
		  gdk_fb_color_round_to_hw (&colormap->colors[i]);
		}
	      /* Real white */
	      colormap->colors[255].red = 
		  colormap->colors[255].green =
		  colormap->colors[255].blue = 65535;
	      gdk_fb_color_round_to_hw (&colormap->colors[255]);

	      break;
	    default:
	      break;
	    }
	  break;
	case GDK_VISUAL_DIRECT_COLOR:
	  g_warning ("gdk_colormap_get_system() on a direct color visual is not implemented");
	  break;
	default:
	  g_assert_not_reached ();
	case GDK_VISUAL_TRUE_COLOR:
	  break;
	}
      
      /* Lock all colors for the system colormap
       * on pseudocolor visuals. The AA text rendering
       * takes to many colors otherwise.
       */
      if ((visual->type == GDK_VISUAL_GRAYSCALE) ||
	  (visual->type == GDK_VISUAL_PSEUDO_COLOR))
	{
	  for(i = 0; i < 256; i++)
	    {
	      colormap->colors[i].pixel = i;
	      private->info[i].ref_count = 1;
	      g_hash_table_insert (private->hash,
				   &colormap->colors[i],
				   &colormap->colors[i]);
	    }
	}
      gdk_colormap_change (colormap, colormap->size);
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  return 1 << (gdk_display->modeinfo.bits_per_pixel);
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
  guint16 red[256], green[256], blue[256];
  struct fb_cmap fbc = {0,256};
  GdkColormapPrivateFB *private;
  int i;

  g_return_if_fail (colormap != NULL);

  fbc.red = red;
  fbc.green = green;
  fbc.blue = blue;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  switch (colormap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
      for(i = 0; i < ncolors; i++)
	{
	  red[i] = green[i] = blue[i] =
	    (colormap->colors[i].red +
	     colormap->colors[i].green +
	     colormap->colors[i].blue)/3;
	}
      ioctl (gdk_display->fb_fd, FBIOPUTCMAP, &fbc);
      break;

    case GDK_VISUAL_PSEUDO_COLOR:
      for (i = 0; i < ncolors; i++)
	{
	  red[i] = colormap->colors[i].red;
	  green[i] = colormap->colors[i].green;
	  blue[i] = colormap->colors[i].blue;
	}
      ioctl (gdk_display->fb_fd, FBIOPUTCMAP, &fbc);
      break;

    default:
      break;
    }
}

void
gdk_colormap_free_colors (GdkColormap *colormap,
			  GdkColor    *colors,
			  gint         ncolors)
{
  GdkColormapPrivateFB *private;
  gint i;

  g_return_if_fail (colormap != NULL);
  g_return_if_fail (colors != NULL);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;

  for (i = 0; i < ncolors; i++)
    {
      gulong pixel = colors[i].pixel;
      
      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }
}

/********************
 * Color allocation *
 ********************/

static void
gdk_fb_color_round_to_hw (GdkColor *color)
{
  guint rmask, gmask, bmask, len;

  len = gdk_display->modeinfo.red.length;
  rmask = ((1 << len) - 1) << (16-len);
  len = gdk_display->modeinfo.green.length;
  gmask = ((1 << len) - 1) << (16-len);
  len = gdk_display->modeinfo.blue.length;
  bmask = ((1 << len) - 1) << (16-len);
  
  color->red &=rmask;
  color->green &=gmask;
  color->blue &=bmask;
}

/* Try to allocate a single color using XAllocColor. If it succeeds,
 * cache the result in our colormap, and store in ret.
 */
static gboolean 
gdk_colormap_alloc1 (GdkColormap *colormap,
		     GdkColor    *color,
		     GdkColor    *ret)
{
  GdkColormapPrivateFB *private;
  int i;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if (colormap->visual->type != GDK_VISUAL_GRAYSCALE
      && colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR)
    return FALSE;

  *ret = *color;
  
  gdk_fb_color_round_to_hw (ret);

  for (i = 0; i<colormap->size; i++)
    {
      if (!(private->info[i].flags & GDK_COLOR_WRITEABLE) &&
	  (ret->red == colormap->colors[i].red) &&
	  (ret->green == colormap->colors[i].green) &&
	  (ret->blue == colormap->colors[i].blue))
	{
	  ret->pixel = i;
	  colormap->colors[i].pixel = i;
	  if (private->info[i].ref_count == 0)
	    g_hash_table_insert (private->hash,
				 &colormap->colors[ret->pixel],
				 &colormap->colors[ret->pixel]);
	  private->info[i].ref_count++;
	  return TRUE;
	}
    }

  for (i = 0; i<colormap->size; i++)
    {
      if (private->info[i].ref_count==0)
	{
	  guint16 red = color->red, green = color->green, blue = color->blue;
	  struct fb_cmap fbc;
	  
	  fbc.len = 1;
	  fbc.start = i;
	  fbc.red = &red;
	  fbc.green = &green;
	  fbc.blue = &blue;

	  ioctl (gdk_display->fb_fd, FBIOPUTCMAP, &fbc);

	  ret->pixel = i;
	  colormap->colors[ret->pixel] = *ret;
	  private->info[ret->pixel].ref_count = 1;
	  g_hash_table_insert (private->hash,
			       &colormap->colors[ret->pixel],
			       &colormap->colors[ret->pixel]);
	  return TRUE;
	}
    }
      
  return FALSE;
}

static gint
gdk_colormap_alloc_colors_shared (GdkColormap *colormap,
				  GdkColor    *colors,
				  gint         ncolors,
				  gboolean     writeable,
				  gboolean     best_match,
				  gboolean    *success)
{
  GdkColormapPrivateFB *private;
  gint i, index;
  gint nremaining = 0;
  gint nfailed = 0;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  index = -1;

  for (i = 0; i < ncolors; i++)
    {
      if (!success[i])
	{
	  if (gdk_colormap_alloc1 (colormap, &colors[i], &colors[i]))
	    success[i] = TRUE;
	  else
	    nremaining++;
	}
    }


  if (nremaining > 0 && best_match)
    {
      gchar *available = g_new (gchar, colormap->size);

      for (i = 0; i < colormap->size; i++)
	available[i] = ((private->info[i].ref_count == 0) ||
			!(private->info[i].flags & GDK_COLOR_WRITEABLE));
      
      while (nremaining > 0)
	{
	  for (i = 0; i < ncolors; i++)
	    {
	      if (!success[i])
		{
		  index = gdk_colormap_match_color (colormap, &colors[i], available);
		  if (index != -1)
		    {
		      if (private->info[index].ref_count)
			{
			  private->info[index].ref_count++;
			  colors[i] = colormap->colors[index];
			  success[i] = TRUE;
			  nremaining--;
			}
		      else
			{
			  if (gdk_colormap_alloc1 (colormap, 
						   &colormap->colors[index],
						   &colors[i]))
			    {
			      success[i] = TRUE;
			      nremaining--;
			      break;
			    }
			  else
			    {
			      available[index] = FALSE;
			    }
			}
		    }
		  else
		    {
		      nfailed++;
		      nremaining--;
		      success[i] = 2; /* flag as permanent failure */
		    }
		}
	    }
	}
      g_free (available);
    }

  /* Change back the values we flagged as permanent failures */
  if (nfailed > 0)
    {
      for (i = 0; i < ncolors; i++)
	if (success[i] == 2)
	  success[i] = FALSE;
      nremaining = nfailed;
    }
  
  return (ncolors - nremaining);
}

static gint
gdk_colormap_alloc_colors_pseudocolor (GdkColormap *colormap,
				       GdkColor    *colors,
				       gint         ncolors,
				       gboolean     writeable,
				       gboolean     best_match,
				       gboolean    *success)
{
  GdkColormapPrivateFB *private;
  GdkColor *lookup_color;
  gint i;
  gint nremaining = 0;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  /* Check for an exact match among previously allocated colors */

  for (i = 0; i < ncolors; i++)
    {
      if (!success[i])
	{
	  lookup_color = g_hash_table_lookup (private->hash, &colors[i]);
	  if (lookup_color)
	    {
	      private->info[lookup_color->pixel].ref_count++;
	      colors[i].pixel = lookup_color->pixel;
	      success[i] = TRUE;
	    }
	  else
	    nremaining++;
	}
    }

  /* If that failed, we try to allocate a new color, or approxmiate
   * with what we can get if best_match is TRUE.
   */
  if (nremaining > 0)
    return gdk_colormap_alloc_colors_shared (colormap, colors, ncolors, writeable, best_match, success);
  else
    return 0;
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
			   GdkColor    *colors,
			   gint         ncolors,
			   gboolean     writeable,
			   gboolean     best_match,
			   gboolean    *success)
{
  GdkColormapPrivateFB *private;
  GdkVisual *visual;
  gint i;
  gint nremaining = 0;

  g_return_val_if_fail (colormap != NULL, FALSE);
  g_return_val_if_fail (colors != NULL, FALSE);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  for (i = 0; i < ncolors; i++)
    success[i] = FALSE;

  visual = colormap->visual;
  switch (visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
      return gdk_colormap_alloc_colors_pseudocolor (colormap, colors, ncolors,
						    writeable, best_match, success);
      break;

    case GDK_VISUAL_DIRECT_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      for (i = 0; i < ncolors; i++)
	{
	  colors[i].pixel = (((colors[i].red >> (16 - visual->red_prec)) << visual->red_shift) +
			     ((colors[i].green >> (16 - visual->green_prec)) << visual->green_shift) +
			     ((colors[i].blue >> (16 - visual->blue_prec)) << visual->blue_shift));
	  success[i] = TRUE;
	}
      break;
    }
  return nremaining;
}

gboolean
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
  GdkColormapPrivateFB *private;
  struct fb_cmap fbc = {0, 1};

  g_return_val_if_fail (colormap != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  switch(colormap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
      color->red = color->green = color->blue = (color->red + color->green + color->blue)/3;

      /* Fall through */
    case GDK_VISUAL_PSEUDO_COLOR:
      colormap->colors[color->pixel] = *color;
      
      fbc.start = color->pixel;
      fbc.red = &color->red;
      fbc.green = &color->green;
      fbc.blue = &color->blue;
      ioctl (gdk_display->fb_fd, FBIOPUTCMAP, &fbc);
      break;

    default:
      break;
    }

  return TRUE;
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

gboolean
gdk_colors_alloc (GdkColormap	*colormap,
		  gboolean 	 contiguous,
		  gulong	*planes,
		  gint		 nplanes,
		  gulong	*pixels,
		  gint		 npixels)
{
  GdkColormapPrivateFB *private;
  gint found, i, col;

  g_return_val_if_fail (colormap != NULL, FALSE);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  
  if (nplanes > 0)
    return FALSE;

  found = 0;
  for (i = 1; i < colormap->size; i++)
    {
      if (private->info[i].ref_count == 0)
	{
	  found++;
	  if (found >= npixels)
	    break;
	}
    }

  if (found < npixels)
    return FALSE;

  col = 0;
  for (i = 1; i < colormap->size; i++)
    {
      if (private->info[i].ref_count == 0)
	{
	  pixels[col++] = i;
	  private->info[i].ref_count++;
	  private->info[i].flags |= GDK_COLOR_WRITEABLE;
	  if (col == npixels)
	    return TRUE;
	}
    }

  g_assert_not_reached ();
  return FALSE;
}

void
gdk_colors_free	(GdkColormap	*colormap,
		 gulong		*pixels,
		 gint		 npixels,
		 gulong	 	 planes)
{
  GdkColormapPrivateFB *private;
  gint i, pixel;

  g_return_if_fail (colormap != NULL);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;
  
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  
  for (i = 0; i < npixels; i++)
    {
      pixel = pixels[i];
      
      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }
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
    case GDK_VISUAL_DIRECT_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      result->red = 65535. * (gdouble) ((pixel & visual->red_mask) >> visual->red_shift) / ((1 << visual->red_prec) - 1);
      result->green = 65535. * (gdouble) ((pixel & visual->green_mask) >> visual->green_shift) / ((1 << visual->green_prec) - 1);
      result->blue = 65535. * (gdouble) ((pixel & visual->blue_mask) >> visual->blue_shift) / ((1 << visual->blue_prec) - 1);
      break;
    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_GRAYSCALE:
      result->red = result->green = result->blue = 65535. * (double)pixel/((1<<visual->depth) - 1);
      break;
    case GDK_VISUAL_PSEUDO_COLOR:
      result->red = colormap->colors[pixel].red;
      result->green = colormap->colors[pixel].green;
      result->blue = colormap->colors[pixel].blue;
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

GdkScreen*
gdk_colormap_get_screen (GdkColormap *cmap)
{
  g_return_val_if_fail (cmap != NULL, NULL);

  return gdk_screen_get_default ();
}
