/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdlib.h>

#include "gdkvisual.h"
#include "gdkscreen.h" /* gdk_screen_get_default() */
#include "gdkprivate-win32.h"
#include "gdkvisualprivate.h"

static void  gdk_visual_decompose_mask (gulong     mask,
					gint      *shift,
					gint      *prec);

static GdkVisual *system_visual = NULL;
static GdkVisual *rgba_visual = NULL;

static gint available_depths[1];

static GdkVisualType available_types[1];

static void
_gdk_visual_init_internal (GdkScreen *screen, gboolean is_rgba)
{
  GdkVisual *visual;
  struct
  {
    BITMAPINFOHEADER bi;
    union
    {
      RGBQUAD colors[256];
      DWORD fields[256];
    } u;
  } bmi;
  HBITMAP hbm;

  const gint rastercaps = GetDeviceCaps (_gdk_display_hdc, RASTERCAPS);
  const int numcolors = GetDeviceCaps (_gdk_display_hdc, NUMCOLORS);
  gint bitspixel = GetDeviceCaps (_gdk_display_hdc, BITSPIXEL);
  gint map_entries = 0;

  visual = g_object_new (GDK_TYPE_VISUAL, NULL);
  visual->screen = screen;

  if (rastercaps & RC_PALETTE)
    {
      const int sizepalette = GetDeviceCaps (_gdk_display_hdc, SIZEPALETTE);
      gchar *max_colors = getenv ("GDK_WIN32_MAX_COLORS");
      visual->type = GDK_VISUAL_PSEUDO_COLOR;

      g_assert (sizepalette == 256);

      if (max_colors != NULL)
	_gdk_max_colors = atoi (max_colors);
      
      map_entries = _gdk_max_colors;

      if (map_entries >= 16 && map_entries < sizepalette)
	{
	  if (map_entries < 32)
	    {
	      map_entries = 16;
	      visual->type = GDK_VISUAL_STATIC_COLOR;
	      bitspixel = 4;
	    }
	  else if (map_entries < 64)
	    {
	      map_entries = 32;
	      bitspixel = 5;
	    }
	  else if (map_entries < 128)
	    {
	      map_entries = 64;
	      bitspixel = 6;
	    }
	  else if (map_entries < 256)
	    {
	      map_entries = 128;
	      bitspixel = 7;
	    }
	  else
	    g_assert_not_reached ();
	}
      else
	map_entries = sizepalette;
    }
  else if (bitspixel == 1 && numcolors == 16)
    {
      bitspixel = 4;
      visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 16;
    }
  else if (bitspixel == 1)
    {
      visual->type = GDK_VISUAL_STATIC_GRAY;
      map_entries = 2;
    }
  else if (bitspixel == 4)
    {
      visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 16;
    }
  else if (bitspixel == 8)
    {
      visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 256;
    }
  else if (bitspixel == 16)
    {
      visual->type = GDK_VISUAL_TRUE_COLOR;
#if 1
      /* This code by Mike Enright,
       * see http://www.users.cts.com/sd/m/menright/display.html
       */
      memset (&bmi, 0, sizeof (bmi));
      bmi.bi.biSize = sizeof (bmi.bi);

      hbm = CreateCompatibleBitmap (_gdk_display_hdc, 1, 1);
      GetDIBits (_gdk_display_hdc, hbm, 0, 1, NULL,
		 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
      GetDIBits (_gdk_display_hdc, hbm, 0, 1, NULL,
		 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
      DeleteObject (hbm);

      if (bmi.bi.biCompression != BI_BITFIELDS)
	{
	  /* Either BI_RGB or BI_RLE_something
	   * .... or perhaps (!!) something else.
	   * Theoretically biCompression might be
	   * mmioFourCC('c','v','i','d') but I doubt it.
	   */
	  if (bmi.bi.biCompression == BI_RGB)
	    {
	      /* It's 555 */
	      bitspixel = 15;
	      visual->red_mask   = 0x00007C00;
	      visual->green_mask = 0x000003E0;
	      visual->blue_mask  = 0x0000001F;
	    }
	  else
	    {
	      g_assert_not_reached ();
	    }
	}
      else
	{
	  DWORD allmasks =
	    bmi.u.fields[0] | bmi.u.fields[1] | bmi.u.fields[2];
	  int k = 0;
	  while (allmasks)
	    {
	      if (allmasks&1)
		k++;
	      allmasks/=2;
	    }
	  bitspixel = k;
	  visual->red_mask = bmi.u.fields[0];
	  visual->green_mask = bmi.u.fields[1];
	  visual->blue_mask  = bmi.u.fields[2];
	}
#else
      /* Old, incorrect (but still working) code. */
#if 0
      visual->red_mask   = 0x0000F800;
      visual->green_mask = 0x000007E0;
      visual->blue_mask  = 0x0000001F;
#else
      visual->red_mask   = 0x00007C00;
      visual->green_mask = 0x000003E0;
      visual->blue_mask  = 0x0000001F;
#endif
#endif
    }
  else if (bitspixel == 24 || bitspixel == 32)
    {
      if (!is_rgba)
        bitspixel = 24;
      visual->type = GDK_VISUAL_TRUE_COLOR;
      visual->red_mask   = 0x00FF0000;
      visual->green_mask = 0x0000FF00;
      visual->blue_mask  = 0x000000FF;
    }
  else
    g_error ("_gdk_visual_init: unsupported BITSPIXEL: %d\n", bitspixel);

  visual->depth = bitspixel;
  visual->byte_order = GDK_LSB_FIRST;
  visual->bits_per_rgb = 42; /* Not used? */

  if ((visual->type == GDK_VISUAL_TRUE_COLOR) ||
      (visual->type == GDK_VISUAL_DIRECT_COLOR))
    {
      gdk_visual_decompose_mask (visual->red_mask,
				 &visual->red_shift,
				 &visual->red_prec);

      gdk_visual_decompose_mask (visual->green_mask,
				 &visual->green_shift,
				 &visual->green_prec);

      gdk_visual_decompose_mask (visual->blue_mask,
				 &visual->blue_shift,
				 &visual->blue_prec);
      map_entries = 1 << (MAX (visual->red_prec,
			       MAX (visual->green_prec,
				    visual->blue_prec)));
    }
  else
    {
      visual->red_mask = 0;
      visual->red_shift = 0;
      visual->red_prec = 0;

      visual->green_mask = 0;
      visual->green_shift = 0;
      visual->green_prec = 0;

      visual->blue_mask = 0;
      visual->blue_shift = 0;
      visual->blue_prec = 0;
    }
  visual->colormap_size = map_entries;

  available_depths[0] = visual->depth;
  available_types[0] = visual->type;

  if (is_rgba)
    rgba_visual = visual;
  else
    system_visual = visual;
}

void
_gdk_visual_init (GdkScreen *screen)
{
  _gdk_visual_init_internal (screen, FALSE);
  _gdk_visual_init_internal (screen, TRUE);
}

gint
_gdk_win32_screen_visual_get_best_depth (GdkScreen *screen)
{
  return available_depths[0];
}

GdkVisualType
_gdk_win32_screen_visual_get_best_type (GdkScreen *screen)
{
  return available_types[0];
}

GdkVisual*
_gdk_win32_screen_get_system_visual (GdkScreen *screen)
{
  return system_visual;
}

GdkVisual *
_gdk_win32_screen_get_rgba_visual (GdkScreen *screen)
{
  return rgba_visual;
}

GdkVisual*
_gdk_win32_screen_visual_get_best (GdkScreen *screen)
{
  return ((GdkVisual*) rgba_visual);
}

GdkVisual*
_gdk_win32_screen_visual_get_best_with_depth (GdkScreen *screen, gint depth)
{
  if (depth == rgba_visual->depth)
    return (GdkVisual*) rgba_visual;
  else if (depth == system_visual->depth)
    return (GdkVisual*) system_visual;
  else
    return NULL;
}

GdkVisual*
_gdk_win32_screen_visual_get_best_with_type (GdkScreen *screen, GdkVisualType visual_type)
{
  if (visual_type == rgba_visual->type)
    return rgba_visual;
  else if (visual_type == system_visual->type)
    return system_visual;
  else
    return NULL;
}

GdkVisual*
_gdk_win32_screen_visual_get_best_with_both (GdkScreen    *screen,
					     gint          depth,
					     GdkVisualType visual_type)
{
  if ((depth == rgba_visual->depth) && (visual_type == rgba_visual->type))
    return rgba_visual;
  else if ((depth == system_visual->depth) && (visual_type == system_visual->type))
    return system_visual;
  else
    return NULL;
}

void
_gdk_win32_screen_query_depths  (GdkScreen *screen,
				 gint **depths,
				 gint  *count)
{
  *count = 1;
  *depths = available_depths;
}

void
_gdk_win32_screen_query_visual_types (GdkScreen      *screen,
				      GdkVisualType **visual_types,
				      gint           *count)
{
  *count = 1;
  *visual_types = available_types;
}

GList*
_gdk_win32_screen_list_visuals (GdkScreen *screen)
{
  GList *result = NULL;

  result = g_list_append (result, (gpointer) rgba_visual);
  result = g_list_append (result, (gpointer) system_visual);

  return result;
}

static void
gdk_visual_decompose_mask (gulong  mask,
			   gint   *shift,
			   gint   *prec)
{
  *shift = 0;
  *prec = 0;

  while (!(mask & 0x1))
    {
      (*shift)++;
      mask >>= 1;
    }

  while (mask & 0x1)
    {
      (*prec)++;
      mask >>= 1;
    }
}
