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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gdkcolor.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

static void     free_colormap            (Colormap          colormap);

static gint     gdk_colormap_match_color (GdkColormap      *cmap,
					  GdkColor         *color,
					  const gchar      *available);
static void     gdk_colormap_add         (GdkColormap      *cmap);
static void     gdk_colormap_remove      (GdkColormap      *cmap);
static guint    gdk_colormap_hash        (Colormap         *cmap);
static gboolean gdk_colormap_equal       (Colormap         *a,
					  Colormap         *b);

static void     gdk_colormap_init        (GdkColormap      *colormap);
static void     gdk_colormap_class_init  (GdkColormapClass *klass);
static void     gdk_colormap_finalize    (GObject          *object);

static gpointer parent_class = NULL;

static GHashTable *colormap_hash = NULL;

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
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_colormap_init (GdkColormap *colormap)
{
  GdkColormapPrivateWin32 *private;

  private = g_new (GdkColormapPrivateWin32, 1);

  colormap->windowing_data = private;
  
  private->hash = NULL;
  private->last_sync_time = 0;
  private->info = NULL;

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

static void
gdk_colormap_finalize (GObject *object)
{
  GdkColormap *colormap = GDK_COLORMAP (object);
  GdkColormapPrivateWin32 *private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  gdk_colormap_remove (colormap);

  free_colormap (private->xcolormap);

  if (private->hash)
    g_hash_table_destroy (private->hash);
  
  g_free (private->info);
  g_free (colormap->colors);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
alloc_color_cells(Colormap      colormap,
		  gboolean      contig,
		  unsigned long plane_masks_return[],
		  unsigned int  nplanes,
		  unsigned long pixels_return[],
		  unsigned int  npixels)
{
  unsigned int i, nfree, iret;

  nfree = 0;
  for (i = 0; i < colormap->size && nfree < npixels; i++)
    if (!colormap->in_use[i])
      nfree++;

  if (colormap->size + npixels - nfree > colormap->sizepalette)
    {
      g_warning ("alloc_color_cells: too large palette: %d",
		 colormap->size + npixels);
      return FALSE;
    }

  iret = 0;
  for (i = 0; i < colormap->size && iret < npixels; i++)
    if (!colormap->in_use[i])
      {
	colormap->in_use[i] = TRUE;
	pixels_return[iret] = i;
	iret++;
      }

  if (nfree < npixels)
    {
      int nmore = npixels - nfree;

      /* I don't understand why, if the code below in #if 0 is
	 enabled, gdkrgb fails miserably. The palette doesn't get
	 realized correctly. There doesn't seem to be any harm done by
	 keeping this code out, either.  */
#ifdef SOME_STRANGE_BUG
      if (!ResizePalette (colormap->palette, colormap->size + nmore))
	{
	  WIN32_GDI_FAILED ("ResizePalette")
	  return FALSE;
	}
      g_print("alloc_color_cells: %#x to %d\n",
	      colormap->palette, colormap->size + nmore);
#endif
      for (i = colormap->size; i < colormap->size + nmore; i++)
	{
	  pixels_return[iret] = i;
	  iret++;
	  colormap->in_use[i] = TRUE;
	}
#ifdef SOME_STRANGE_BUG
      colormap->size += nmore;
#endif
    }
  return TRUE;
}

/* The following functions are from Tk8.0, but heavily modified.
   Here are tk's licensing terms. I hope these terms don't conflict
   with the GNU Lesser General Public License? They shouldn't, as
   they are looser that the GLPL, yes? */

/*
This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., and other parties.  The following
terms apply to all files associated with the software unless explicitly
disclaimed in individual files.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal 
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license.
*/
/*
 *----------------------------------------------------------------------
 *
 * XAllocColor --
 *
 *	Find the closest available color to the specified XColor.
 *
 * Results:
 *	Updates the color argument and returns 1 on success.  Otherwise
 *	returns 0.
 *
 * Side effects:
 *	Allocates a new color in the palette.
 *
 *----------------------------------------------------------------------
 */

static int
alloc_color(Colormap  colormap,
	    XColor   *color,
	    guint    *pixelp)
{
  PALETTEENTRY entry, closeEntry;
  unsigned int i;
    
  entry = *color;
  entry.peFlags = 0;

  if (colormap->rc_palette)
    {
      COLORREF newPixel, closePixel;
      UINT index;

      /*
       * Find the nearest existing palette entry.
       */
	
      newPixel = RGB (entry.peRed, entry.peGreen, entry.peBlue);
      index = GetNearestPaletteIndex (colormap->palette, newPixel);
      GetPaletteEntries (colormap->palette, index, 1, &closeEntry);
      closePixel = RGB (closeEntry.peRed, closeEntry.peGreen,
			closeEntry.peBlue);

      if (newPixel != closePixel)
	{
	  /* Not a perfect match. */
	  if (!colormap->in_use[index])
	    {
	      /* It was a free'd entry anyway, so we can use it, and
		 set it to the correct color. */
	      if (SetPaletteEntries (colormap->palette, index, 1, &entry) == 0)
		WIN32_GDI_FAILED ("SetPaletteEntries");
	    }
	  else
	    {
	      /* The close entry found is in use, so search for a
		 unused slot. */
		 
	      for (i = 0; i < colormap->size; i++)
		if (!colormap->in_use[i])
		  {
		    /* A free slot, use it. */
		    if (SetPaletteEntries (colormap->palette,
					   index, 1, &entry) == 0)
		      WIN32_GDI_FAILED ("SetPaletteEntries");
		    index = i;
		    break;
		  }
	      if (i == colormap->size)
		{
		  /* No free slots found. If the palette isn't maximal
		     yet, grow it. */
		  if (colormap->size == colormap->sizepalette)
		    {
		      /* The palette is maximal, and no free slots available,
			 so use the close entry, then, dammit. */
		      *color = closeEntry;
		    }
		  else
		    {
		      /* There is room to grow the palette. */
		      index = colormap->size;
		      colormap->size++;
		      if (!ResizePalette (colormap->palette, colormap->size))
			WIN32_GDI_FAILED ("ResizePalette");
		      if (SetPaletteEntries (colormap->palette, index, 1, &entry) == 0)
			WIN32_GDI_FAILED ("SetPaletteEntries");
		    }
		}
	    }
	  colormap->stale = TRUE;
	}
      else
	{
	  /* We got a match, so use it. */
	}

      *pixelp = index;
      colormap->in_use[index] = TRUE;
#if 0
      g_print("alloc_color from %#x: index %d for %02x %02x %02x\n",
	      colormap->palette, index,
	      entry.peRed, entry.peGreen, entry.peBlue);
#endif
    }
  else
    {
      /*
       * Determine what color will actually be used on non-colormap systems.
       */
      *pixelp = GetNearestColor (gdk_display_hdc, RGB(entry.peRed, entry.peGreen, entry.peBlue));
      
      color->peRed = GetRValue (*pixelp);
      color->peGreen = GetGValue (*pixelp);
      color->peBlue = GetBValue (*pixelp);
    }
  
  return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeColors --
 *
 *	Deallocate a block of colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes entries for the current palette and compacts the
 *	remaining set.
 *
 *----------------------------------------------------------------------
 */

static void
free_colors (Colormap colormap,
	     gulong  *pixels,
	     gint     npixels,
	     gulong   planes)
{
  gint i;
  PALETTEENTRY entries[256];

  /*
   * We don't have to do anything for non-palette devices.
   */
  
  if (colormap->rc_palette)
    {
      int npal;
      int lowestpixel = 256;
      int highestpixel = -1;

      npal = GetPaletteEntries (colormap->palette, 0, 256, entries);
      for (i = 0; i < npixels; i++)
	{
	  int pixel = pixels[i];

	  if (pixel < lowestpixel)
	    lowestpixel = pixel;
	  if (pixel > highestpixel)
	    highestpixel = pixel;

	  colormap->in_use[pixel] = FALSE;

	  entries[pixel] = entries[0];
	}
#if 0
      if (SetPaletteEntries (colormap->palette, lowestpixel,
			     highestpixel - lowestpixel + 1,
			     entries + lowestpixel) == 0)
	WIN32_GDI_FAILED ("SetPaletteEntries");
#endif
      colormap->stale = TRUE;
#if 0
      g_print("free_colors %#x lowestpixel = %d, highestpixel = %d\n",
	      colormap->palette, lowestpixel, highestpixel);
#endif
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XCreateColormap --
 *
 *	Allocate a new colormap.
 *
 * Results:
 *	Returns a newly allocated colormap.
 *
 * Side effects:
 *	Allocates an empty palette and color list.
 *
 *----------------------------------------------------------------------
 */

static Colormap
create_colormap (HWND     w,
		 Visual  *visual,
		 gboolean alloc)
{
  char logPalBuf[sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY)];
  LOGPALETTE *logPalettePtr;
  Colormap colormap;
  guint i;
  HPALETTE sysPal;
  HDC hdc;

  /* Should the alloc parameter do something? */


  /* Allocate a starting palette with all of the reserved colors. */
  
  logPalettePtr = (LOGPALETTE *) logPalBuf;
  logPalettePtr->palVersion = 0x300;
  sysPal = (HPALETTE) GetStockObject (DEFAULT_PALETTE);
  logPalettePtr->palNumEntries =
    GetPaletteEntries (sysPal, 0, 256, logPalettePtr->palPalEntry);
  
  colormap = (Colormap) g_new (ColormapStruct, 1);
  colormap->size = logPalettePtr->palNumEntries;
  colormap->stale = TRUE;
  colormap->palette = CreatePalette (logPalettePtr);
  hdc = GetDC (NULL);
  colormap->rc_palette = ((GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) != 0);
  if (colormap->rc_palette)
    {
      colormap->sizepalette = GetDeviceCaps (hdc, SIZEPALETTE);
      colormap->in_use = g_new (gboolean, colormap->sizepalette);
      /* Mark static colors in use. */
      for (i = 0; i < logPalettePtr->palNumEntries; i++)
	colormap->in_use[i] = TRUE;
      /* Mark rest not in use */
      for (i = logPalettePtr->palNumEntries; i < colormap->sizepalette; i++)
	colormap->in_use[i] = FALSE;
    }
  ReleaseDC (NULL, hdc);

  return colormap;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeColormap --
 *
 *	Frees the resources associated with the given colormap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the palette associated with the colormap.  Note that
 *	the palette must not be selected into a device context when
 *	this occurs.
 *
 *----------------------------------------------------------------------
 */

static void
free_colormap(Colormap colormap)
 
{
  if (!DeleteObject (colormap->palette))
    {
      g_error ("Unable to free colormap, palette is still selected.");
    }
  g_free (colormap);
}

static Colormap
default_colormap ()
{
  static Colormap colormap;

  if (colormap)
    return colormap;

  colormap = create_colormap ( NULL, NULL, FALSE);
  return colormap;
}

GdkColormap*
gdk_colormap_new (GdkVisual *visual,
		  gboolean   private_cmap)
{
  GdkColormap *colormap;
  GdkColormapPrivateWin32 *private;
  Visual *xvisual;
  int i;

  g_return_val_if_fail (visual != NULL, NULL);

  colormap = g_object_new (gdk_colormap_get_type (), NULL);
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  colormap->visual = visual;

  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  colormap->size = visual->colormap_size;

  switch (visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      private->info = g_new0 (GdkColorInfo, colormap->size);
      colormap->colors = g_new (GdkColor, colormap->size);
      
      private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					(GEqualFunc) gdk_color_equal);
      
      private->private_val = private_cmap;
      private->xcolormap = create_colormap (gdk_root_window, xvisual, private_cmap);

      if (private_cmap)
	{
	  PALETTEENTRY pal[256];
	  guint npal;

	  npal = GetPaletteEntries (private->xcolormap->palette, 0, colormap->size, pal);
	  for (i = 0; i < colormap->size; i++)
	    {
	      colormap->colors[i].pixel = i;
	      if (i >= npal)
		{
		  colormap->colors[i].red =
		    colormap->colors[i].green =
		    colormap->colors[i].blue = 0;
		}
	      else
		{
		  colormap->colors[i].red = (pal[i].peRed * 65535) / 255;
		  colormap->colors[i].green = (pal[i].peGreen * 65525) / 255;
		  colormap->colors[i].blue = (pal[i].peBlue * 65535) / 255;
		}
	    }
	  gdk_colormap_change (colormap, colormap->size);
	}
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      private->private_val = FALSE;
      private->xcolormap = create_colormap (gdk_root_window,
					    xvisual, FALSE);
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      g_assert_not_reached ();
    }

  gdk_colormap_add (colormap);

  return colormap;
}

#define MIN_SYNC_TIME 2

static void
gdk_colormap_sync (GdkColormap *colormap,
		   gboolean     force)
{
  time_t current_time;
  GdkColormapPrivateWin32 *private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  XColor *xpalette;
  gint nlookup;
  gint i;
  
  g_return_if_fail (colormap != NULL);

  current_time = time (NULL);
  if (!force && ((current_time - private->last_sync_time) < MIN_SYNC_TIME))
    return;

  private->last_sync_time = current_time;

  nlookup = 0;
  xpalette = g_new (XColor, colormap->size);
  
  nlookup = GetPaletteEntries (private->xcolormap->palette,
			       0, colormap->size, xpalette);
  
  for (i = 0; i < nlookup; i++)
    {
      colormap->colors[i].pixel = i;
      colormap->colors[i].red = (xpalette[i].peRed * 65535) / 255;
      colormap->colors[i].green = (xpalette[i].peGreen * 65535) / 255;
      colormap->colors[i].blue = (xpalette[i].peBlue * 65535) / 255;
    }

  for (  ; i < colormap->size; i++)
    {
      colormap->colors[i].pixel = i;
      colormap->colors[i].red = 0;
      colormap->colors[i].green = 0;
      colormap->colors[i].blue = 0;
    }

  g_free (xpalette);
}
		   
GdkColormap*
gdk_colormap_get_system (void)
{
  static GdkColormap *colormap = NULL;
  GdkColormapPrivateWin32 *private;

  if (!colormap)
    {
      colormap = g_object_new (gdk_colormap_get_type (), NULL);
      private = GDK_COLORMAP_PRIVATE_DATA (colormap);

      private->xcolormap = default_colormap ();
      colormap->visual = gdk_visual_get_system ();
      private->private_val = FALSE;

      private->hash = NULL;
      private->last_sync_time = 0;
      private->info = NULL;

      colormap->colors = NULL;
      colormap->size = colormap->visual->colormap_size;

      if ((colormap->visual->type == GDK_VISUAL_GRAYSCALE) ||
	  (colormap->visual->type == GDK_VISUAL_PSEUDO_COLOR))
	{
	  private->info = g_new0 (GdkColorInfo, colormap->size);
	  colormap->colors = g_new (GdkColor, colormap->size);
	  
	  private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					    (GEqualFunc) gdk_color_equal);

	  gdk_colormap_sync (colormap, TRUE);
	}
      gdk_colormap_add (colormap);
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  gint bitspixel;
  
  bitspixel = GetDeviceCaps (gdk_display_hdc, BITSPIXEL);

  if (bitspixel == 1)
    return 2;
  else if (bitspixel == 4)
    return 16;
  else if (bitspixel == 8)
    return 256;
  else if (bitspixel == 12)
    return 32;
  else if (bitspixel == 16)
    return 64;
  else /* if (bitspixel >= 24) */
    return 256;
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
  GdkColormapPrivateWin32 *private;
  XColor *palette;
  int i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  palette = g_new (XColor, ncolors);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  switch (colormap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      for (i = 0; i < ncolors; i++)
	{
	  palette[i].peRed = (colormap->colors[i].red >> 8);
	  palette[i].peGreen = (colormap->colors[i].green >> 8);
	  palette[i].peBlue = (colormap->colors[i].blue >> 8);
	  palette[i].peFlags = 0;
	}

      if (SetPaletteEntries (private->xcolormap->palette,
			     0, ncolors, palette) == 0)
	WIN32_GDI_FAILED ("SetPaletteEntries");
      private->xcolormap->stale = TRUE;
      break;

    default:
      break;
    }

  g_free (palette);
}

gboolean
gdk_colors_alloc (GdkColormap   *colormap,
		  gboolean       contiguous,
		  gulong        *planes,
		  gint           nplanes,
		  gulong        *pixels,
		  gint           npixels)
{
  GdkColormapPrivateWin32 *private;
  gint return_val;
  gint i;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), 0);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  return_val = alloc_color_cells (private->xcolormap, contiguous,
				  planes, nplanes, pixels, npixels);

  if (return_val)
    {
      for (i=0; i<npixels; i++)
	{
	  private->info[pixels[i]].ref_count++;
	  private->info[pixels[i]].flags |= GDK_COLOR_WRITEABLE;
	}
    }

  return return_val != 0;
}

/* This is almost identical to gdk_colormap_free_colors.
 * Keep them in sync!
 */
void
gdk_colors_free (GdkColormap *colormap,
		 gulong      *in_pixels,
		 gint         in_npixels,
		 gulong       planes)
{
  GdkColormapPrivateWin32 *private;
  gulong *pixels;
  gint npixels = 0;
  gint i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (in_pixels != NULL);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;
  
  pixels = g_new (gulong, in_npixels);

  for (i=0; i<in_npixels; i++)
    {
      gulong pixel = in_pixels[i];
      
      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      pixels[npixels++] = pixel;
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }

  if (npixels)
    free_colors (private->xcolormap, pixels, npixels, planes);

  g_free (pixels);
}

/* This is almost identical to gdk_colors_free.
 * Keep them in sync!
 */
void
gdk_colormap_free_colors (GdkColormap *colormap,
			  GdkColor    *colors,
			  gint         ncolors)
{
  GdkColormapPrivateWin32 *private;
  gulong *pixels;
  gint npixels = 0;
  gint i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (colors != NULL);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;

  pixels = g_new (gulong, ncolors);

  for (i=0; i<ncolors; i++)
    {
      gulong pixel = colors[i].pixel;
      
      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      pixels[npixels++] = pixel;
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }
  if (npixels)
    free_colors (private->xcolormap, pixels, npixels, 0);
  g_free (pixels);
}

/********************
 * Color allocation *
 ********************/

/* Try to allocate a single color using alloc_color. If it succeeds,
 * cache the result in our colormap, and store in ret.
 */
static gboolean 
gdk_colormap_alloc1 (GdkColormap *colormap,
		     GdkColor    *color,
		     GdkColor    *ret)
{
  GdkColormapPrivateWin32 *private;
  XColor xcolor;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  xcolor.peRed = color->red >> 8;
  xcolor.peGreen = color->green >> 8;
  xcolor.peBlue = color->blue >> 8;

  if (alloc_color (private->xcolormap, &xcolor, &ret->pixel))
    {
      ret->red = (xcolor.peRed * 65535) / 255;
      ret->green = (xcolor.peGreen * 65535) / 255;
      ret->blue = (xcolor.peBlue * 65535) / 255;
      
      if ((guint) ret->pixel < colormap->size)
	{
	  if (private->info[ret->pixel].ref_count) /* got a duplicate */
	    {
	      /* XXX */
	    }
	  else
	    {
	      colormap->colors[ret->pixel] = *color;
	      private->info[ret->pixel].ref_count = 1;

	      g_hash_table_insert (private->hash,
				   &colormap->colors[ret->pixel],
				   &colormap->colors[ret->pixel]);
	    }
	}
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gint
gdk_colormap_alloc_colors_writeable (GdkColormap *colormap,
				     GdkColor    *colors,
				     gint         ncolors,
				     gboolean     writeable,
				     gboolean     best_match,
				     gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  gulong *pixels;
  gboolean status;
  gint i, index;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if (private->private_val)
    {
      index = 0;
      for (i=0; i<ncolors; i++)
	{
	  while ((index < colormap->size) && (private->info[index].ref_count != 0))
	    index++;
	  
	  if (index < colormap->size)
	    {
	      colors[i].pixel = index;
	      success[i] = TRUE;
	      private->info[index].ref_count++;
	      private->info[i].flags |= GDK_COLOR_WRITEABLE;
	    }
	  else
	    break;
	}
      return i;
    }
  else
    {
      pixels = g_new (gulong, ncolors);
      /* Allocation of a writeable color cells */
      
      status =  alloc_color_cells (private->xcolormap, FALSE, NULL,
				   0, pixels, ncolors);
      if (status)
	{
	  for (i=0; i<ncolors; i++)
	    {
	      colors[i].pixel = pixels[i];
	      private->info[pixels[i]].ref_count++;
	      private->info[pixels[i]].flags |= GDK_COLOR_WRITEABLE;
	    }
	}
      
      g_free (pixels);

      return status ? ncolors : 0; 
    }
}

static gint
gdk_colormap_alloc_colors_private (GdkColormap *colormap,
				   GdkColor    *colors,
				   gint         ncolors,
				   gboolean     writeable,
				   gboolean     best_match,
				   gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  gint i, index;
  XColor *store = g_new (XColor, ncolors);
  gint nstore = 0;
  gint nremaining = 0;
  
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  index = -1;

  /* First, store the colors we have room for */

  index = 0;
  for (i=0; i<ncolors; i++)
    {
      if (!success[i])
	{
	  while ((index < colormap->size) && (private->info[index].ref_count != 0))
	    index++;

	  if (index < colormap->size)
	    {
	      store[nstore].peRed = colors[i].red >> 8;
	      store[nstore].peBlue = colors[i].blue >> 8;
	      store[nstore].peGreen = colors[i].green >> 8;
	      nstore++;

	      success[i] = TRUE;

	      colors[i].pixel = index;
	      private->info[index].ref_count++;
	    }
	  else
	    nremaining++;
	}
    }
  
  if (SetPaletteEntries (private->xcolormap->palette,
			 0, nstore, store) == 0)
    WIN32_GDI_FAILED ("SetPaletteEntries");
  private->xcolormap->stale = TRUE;

  g_free (store);

  if (nremaining > 0 && best_match)
    {
      /* Get best matches for remaining colors */

      gchar *available = g_new (gchar, colormap->size);
      for (i = 0; i < colormap->size; i++)
	available[i] = TRUE;

      for (i=0; i<ncolors; i++)
	{
	  if (!success[i])
	    {
	      index = gdk_colormap_match_color (colormap, 
						&colors[i], 
						available);
	      if (index != -1)
		{
		  colors[i] = colormap->colors[index];
		  private->info[index].ref_count++;

		  success[i] = TRUE;
		  nremaining--;
		}
	    }
	}
      g_free (available);
    }

  return (ncolors - nremaining);
}

static gint
gdk_colormap_alloc_colors_shared (GdkColormap *colormap,
				  GdkColor    *colors,
				  gint         ncolors,
				  gboolean     writeable,
				  gboolean     best_match,
				  gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  gint i, index;
  gint nremaining = 0;
  gint nfailed = 0;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  index = -1;

  for (i=0; i<ncolors; i++)
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
      gdk_colormap_sync (colormap, FALSE);
      
      while (nremaining > 0)
	{
	  for (i=0; i<ncolors; i++)
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
      for (i=0; i<ncolors; i++)
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
  GdkColormapPrivateWin32 *private;
  GdkColor *lookup_color;
  gint i;
  gint nremaining = 0;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  /* Check for an exact match among previously allocated colors */

  for (i=0; i<ncolors; i++)
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
    {
      if (private->private_val)
	return gdk_colormap_alloc_colors_private (colormap, colors, ncolors, writeable, best_match, success);
      else
	return gdk_colormap_alloc_colors_shared (colormap, colors, ncolors, writeable, best_match, success);
    }
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
  GdkColormapPrivateWin32 *private;
  GdkVisual *visual;
  gint i;
  gint nremaining = 0;
  XColor xcolor;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (colors != NULL, FALSE);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  for (i=0; i<ncolors; i++)
    {
      success[i] = FALSE;
    }

  switch (colormap->visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_GRAYSCALE:
      if (writeable)
	return gdk_colormap_alloc_colors_writeable (colormap, colors, ncolors,
						    writeable, best_match, success);
      else
	return gdk_colormap_alloc_colors_pseudocolor (colormap, colors, ncolors,
						    writeable, best_match, success);
      break;

    case GDK_VISUAL_TRUE_COLOR:
      visual = colormap->visual;

      for (i=0; i<ncolors; i++)
	{
	  colors[i].pixel = (((colors[i].red >> (16 - visual->red_prec)) << visual->red_shift) +
			     ((colors[i].green >> (16 - visual->green_prec)) << visual->green_shift) +
			     ((colors[i].blue >> (16 - visual->blue_prec)) << visual->blue_shift));
	  success[i] = TRUE;
	}
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
      for (i=0; i<ncolors; i++)
	{
	  xcolor.peRed = colors[i].red >> 8;
	  xcolor.peGreen = colors[i].green >> 8;
	  xcolor.peBlue = colors[i].blue >> 8;
	  if (alloc_color (private->xcolormap, &xcolor, &colors[i].pixel))
	    success[i] = TRUE;
	  else
	    nremaining++;
	}
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      g_assert_not_reached ();
    }
  return nremaining;
}

void
gdk_colormap_query_color (GdkColormap *colormap,
			  gulong       pixel,
			  GdkColor    *result)
{
  GdkVisual *visual;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  
  visual = gdk_colormap_get_visual (colormap);

  switch (visual->type) {
  case GDK_VISUAL_DIRECT_COLOR:
  case GDK_VISUAL_TRUE_COLOR:
    result->red = 65535. * (double)((pixel & visual->red_mask) >> visual->red_shift) / ((1 << visual->red_prec) - 1);
    result->green = 65535. * (double)((pixel & visual->green_mask) >> visual->green_shift) / ((1 << visual->green_prec) - 1);
    result->blue = 65535. * (double)((pixel & visual->blue_mask) >> visual->blue_shift) / ((1 << visual->blue_prec) - 1);
    break;
  case GDK_VISUAL_STATIC_GRAY:
  case GDK_VISUAL_GRAYSCALE:
    result->red = result->green = result->blue = 65535. * (double)pixel/((1<<visual->depth) - 1);
    break;
  case GDK_VISUAL_STATIC_COLOR:
    g_assert_not_reached ();
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

gboolean
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
  GdkColormapPrivateWin32 *private;
  XColor xcolor;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  xcolor.peRed = color->red >> 8;
  xcolor.peGreen = color->green >> 8;
  xcolor.peBlue = color->blue >> 8;

  if (SetPaletteEntries (private->xcolormap->palette,
			 color->pixel, 1, &xcolor) == 0)
    WIN32_GDI_FAILED ("SetPaletteEntries");
  private->xcolormap->stale = TRUE;

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
  GdkColormapPrivateWin32 *private;

  if (!colormap_hash)
    colormap_hash = g_hash_table_new ((GHashFunc) gdk_colormap_hash,
				      (GEqualFunc) gdk_colormap_equal);

  private = GDK_COLORMAP_PRIVATE_DATA (cmap);

  g_hash_table_insert (colormap_hash, &private->xcolormap, cmap);
}

static void
gdk_colormap_remove (GdkColormap *cmap)
{
  GdkColormapPrivateWin32 *private;

  if (!colormap_hash)
    colormap_hash = g_hash_table_new ((GHashFunc) gdk_colormap_hash,
				      (GEqualFunc) gdk_colormap_equal);

  private = GDK_COLORMAP_PRIVATE_DATA (cmap);

  g_hash_table_remove (colormap_hash, &private->xcolormap);
}

static guint
gdk_colormap_hash (Colormap *cmap)
{
  return (guint) *cmap;
}

static gboolean
gdk_colormap_equal (Colormap *a,
		    Colormap *b)
{
  return (*a == *b);
}

#ifdef G_ENABLE_DEBUG

gchar *
gdk_win32_color_to_string (const GdkColor *color)
{
  static char buf[100];

  sprintf (buf, "(%.04x,%.04x,%.04x):%.06x",
	   color->red, color->green, color->blue, color->pixel);

  return buf;
}

#endif
