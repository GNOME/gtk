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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <gdk/gdk.h>
#include "gdkx.h"

static void  gdk_visual_add            (GdkVisual *visual);
static void  gdk_visual_decompose_mask (gulong     mask,
					gint      *shift,
					gint      *prec);
static guint gdk_visual_hash           (Visual    *key);
static gint  gdk_visual_compare        (Visual    *a,
					Visual    *b);


static GdkVisualPrivate *system_visual;
static GdkVisualPrivate *visuals;
static gint nvisuals;

static gint available_depths[7];
static gint navailable_depths;

static GdkVisualType available_types[6];
static gint navailable_types;

#ifdef G_ENABLE_DEBUG

static const gchar* visual_names[] =
{
  "static gray",
  "grayscale",
  "static color",
  "pseudo color",
  "true color",
  "direct color",
};

#endif /* G_ENABLE_DEBUG */

static GHashTable *visual_hash = NULL;

void
gdk_visual_init (void)
{
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

  static const gint possible_depths[7] = { 32, 24, 16, 15, 8, 4, 1 };
  static const GdkVisualType possible_types[6] =
    {
      GDK_VISUAL_DIRECT_COLOR,
      GDK_VISUAL_TRUE_COLOR,
      GDK_VISUAL_PSEUDO_COLOR,
      GDK_VISUAL_STATIC_COLOR,
      GDK_VISUAL_GRAYSCALE,
      GDK_VISUAL_STATIC_GRAY
    };

  static const gint npossible_depths = sizeof(possible_depths)/sizeof(gint);
  static const gint npossible_types = sizeof(possible_types)/sizeof(GdkVisualType);

  int rastercaps, numcolors, sizepalette, colorres, bitspixel;
  Visual *default_xvisual;
  GdkVisualPrivate temp_visual;
  int nxvisuals;
  int i, j;

  nxvisuals = 1;
  visuals = g_new (GdkVisualPrivate, nxvisuals);

  nvisuals = 0;
  for (i = 0; i < nxvisuals; i++)
    {
      if (1)
	{
	  bitspixel = GetDeviceCaps (gdk_DC, BITSPIXEL);
	  rastercaps = GetDeviceCaps (gdk_DC, RASTERCAPS);
	  default_xvisual = g_new (Visual, 1);
	  visuals[nvisuals].xvisual = default_xvisual;
	  visuals[nvisuals].xvisual->visualid = nvisuals;
	  visuals[nvisuals].xvisual->bitspixel = bitspixel;

	  if (rastercaps & RC_PALETTE)
	    {
	      visuals[nvisuals].visual.type = GDK_VISUAL_PSEUDO_COLOR;
	      numcolors = GetDeviceCaps (gdk_DC, NUMCOLORS);
	      sizepalette = GetDeviceCaps (gdk_DC, SIZEPALETTE);
	      colorres = GetDeviceCaps (gdk_DC, COLORRES);
	      visuals[nvisuals].xvisual->map_entries = sizepalette;
	    }
	  else if (bitspixel == 1)
	    {
	      visuals[nvisuals].visual.type = GDK_VISUAL_STATIC_GRAY;
	      visuals[nvisuals].xvisual->map_entries = 2;
	    }
	  else if (bitspixel == 4)
	    {
	      visuals[nvisuals].visual.type = GDK_VISUAL_STATIC_COLOR;
	      visuals[nvisuals].xvisual->map_entries = 16;
	    }
	  else if (bitspixel == 8)
	    {
	      visuals[nvisuals].visual.type = GDK_VISUAL_STATIC_COLOR;
	      visuals[nvisuals].xvisual->map_entries = 256;
	    }
	  else if (bitspixel == 16)
	    {
	      visuals[nvisuals].visual.type = GDK_VISUAL_TRUE_COLOR;
#if 1
	      /* This code by Mike Enright,
	       * see http://www.users.cts.com/sd/m/menright/display.html
	       */
	      memset (&bmi, 0, sizeof (bmi));
	      bmi.bi.biSize = sizeof (bmi.bi);
  
	      hbm = CreateCompatibleBitmap (gdk_DC, 1, 1);
	      GetDIBits (gdk_DC, hbm, 0, 1, NULL,
			 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
	      GetDIBits (gdk_DC, hbm, 0, 1, NULL,
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
		      visuals[nvisuals].visual.red_mask   = 0x00007C00;
		      visuals[nvisuals].visual.green_mask = 0x000003E0;
		      visuals[nvisuals].visual.blue_mask  = 0x0000001F;
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
		  visuals[nvisuals].visual.red_mask = bmi.u.fields[0];
		  visuals[nvisuals].visual.green_mask = bmi.u.fields[1];
		  visuals[nvisuals].visual.blue_mask  = bmi.u.fields[2];
		}
#else
	      /* Old, incorrect (but still working) code. */
#if 0
	      visuals[nvisuals].visual.red_mask   = 0x0000F800;
	      visuals[nvisuals].visual.green_mask = 0x000007E0;
	      visuals[nvisuals].visual.blue_mask  = 0x0000001F;
#else
	      visuals[nvisuals].visual.red_mask   = 0x00007C00;
	      visuals[nvisuals].visual.green_mask = 0x000003E0;
	      visuals[nvisuals].visual.blue_mask  = 0x0000001F;
#endif
#endif
	    }
	  else if (bitspixel == 24 || bitspixel == 32)
	    {
	      visuals[nvisuals].visual.type = GDK_VISUAL_TRUE_COLOR;
	      visuals[nvisuals].visual.red_mask   = 0x00FF0000;
	      visuals[nvisuals].visual.green_mask = 0x0000FF00;
	      visuals[nvisuals].visual.blue_mask  = 0x000000FF;
	    }
	  else
	    g_error ("gdk_visual_init: unsupported BITSPIXEL: %d\n", bitspixel);

	  visuals[nvisuals].visual.depth = bitspixel;
	  visuals[nvisuals].visual.byte_order = GDK_LSB_FIRST;
	  visuals[nvisuals].visual.bits_per_rgb = 42; /* Not used? */

	  if ((visuals[nvisuals].visual.type == GDK_VISUAL_TRUE_COLOR) ||
	      (visuals[nvisuals].visual.type == GDK_VISUAL_DIRECT_COLOR))
	    {
	      gdk_visual_decompose_mask (visuals[nvisuals].visual.red_mask,
					 &visuals[nvisuals].visual.red_shift,
					 &visuals[nvisuals].visual.red_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals].visual.green_mask,
					 &visuals[nvisuals].visual.green_shift,
					 &visuals[nvisuals].visual.green_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals].visual.blue_mask,
					 &visuals[nvisuals].visual.blue_shift,
					 &visuals[nvisuals].visual.blue_prec);
	      visuals[nvisuals].xvisual->map_entries =
		1 << (MAX (visuals[nvisuals].visual.red_prec,
			   MAX (visuals[nvisuals].visual.green_prec,
				visuals[nvisuals].visual.blue_prec)));
	    }
	  else
	    {
	      visuals[nvisuals].visual.red_mask = 0;
	      visuals[nvisuals].visual.red_shift = 0;
	      visuals[nvisuals].visual.red_prec = 0;

	      visuals[nvisuals].visual.green_mask = 0;
	      visuals[nvisuals].visual.green_shift = 0;
	      visuals[nvisuals].visual.green_prec = 0;

	      visuals[nvisuals].visual.blue_mask = 0;
	      visuals[nvisuals].visual.blue_shift = 0;
	      visuals[nvisuals].visual.blue_prec = 0;
	    }
	  visuals[nvisuals].visual.colormap_size = visuals[nvisuals].xvisual->map_entries;

	  nvisuals += 1;
	}
    }

  for (i = 0; i < nvisuals; i++)
    {
      for (j = i+1; j < nvisuals; j++)
	{
	  if (visuals[j].visual.depth >= visuals[i].visual.depth)
	    {
	      if ((visuals[j].visual.depth == 8) && (visuals[i].visual.depth == 8))
		{
		  if (visuals[j].visual.type == GDK_VISUAL_PSEUDO_COLOR)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		  else if ((visuals[i].visual.type != GDK_VISUAL_PSEUDO_COLOR) &&
			   visuals[j].visual.type > visuals[i].visual.type)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		}
	      else if ((visuals[j].visual.depth > visuals[i].visual.depth) ||
		       ((visuals[j].visual.depth == visuals[i].visual.depth) &&
			(visuals[j].visual.type > visuals[i].visual.type)))
		{
		  temp_visual = visuals[j];
		  visuals[j] = visuals[i];
		  visuals[i] = temp_visual;
		}
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    if (default_xvisual->visualid == visuals[i].xvisual->visualid)
      {
	system_visual = &visuals[i];
	break;
      }

  navailable_depths = 0;
  for (i = 0; i < npossible_depths; i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j].visual.depth == possible_depths[i])
	    {
	      available_depths[navailable_depths++] = visuals[j].visual.depth;
	      break;
	    }
	}
    }

  if (navailable_depths == 0)
    g_error ("unable to find a usable depth");

  navailable_types = 0;
  for (i = 0; i < npossible_types; i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j].visual.type == possible_types[i])
	    {
	      available_types[navailable_types++] = visuals[j].visual.type;
	      break;
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    gdk_visual_add ((GdkVisual*) &visuals[i]);

  if (npossible_types == 0)
    g_error ("unable to find a usable visual type");
}

GdkVisual*
gdk_visual_ref (GdkVisual *visual)
{
  return visual;
}

void
gdk_visual_unref (GdkVisual *visual)
{
  return;
}

gint
gdk_visual_get_best_depth (void)
{
  return available_depths[0];
}

GdkVisualType
gdk_visual_get_best_type (void)
{
  return available_types[0];
}

GdkVisual*
gdk_visual_get_system (void)
{
  return ((GdkVisual*) system_visual);
}

GdkVisual*
gdk_visual_get_best (void)
{
  return ((GdkVisual*) &(visuals[0]));
}

GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < nvisuals; i++)
    if (depth == visuals[i].visual.depth)
      {
	return_val = (GdkVisual*) &(visuals[i]);
	break;
      }

  return return_val;
}

GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < nvisuals; i++)
    if (visual_type == visuals[i].visual.type)
      {
	return_val = (GdkVisual*) &(visuals[i]);
	break;
      }

  return return_val;
}

GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < nvisuals; i++)
    if ((depth == visuals[i].visual.depth) &&
	(visual_type == visuals[i].visual.type))
      {
	return_val = (GdkVisual*) &(visuals[i]);
	break;
      }

  return return_val;
}

void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  *count = navailable_depths;
  *depths = available_depths;
}

void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  *count = navailable_types;
  *visual_types = available_types;
}

GList*
gdk_list_visuals (void)
{
  GList *list;
  guint i;

  list = NULL;
  for (i = 0; i < nvisuals; ++i)
    list = g_list_append (list, (gpointer) &visuals[i]);

  return list;
}

GdkVisual*
gdk_visual_lookup (Visual *xvisual)
{
  GdkVisual *visual;

  if (!visual_hash)
    return NULL;

  visual = g_hash_table_lookup (visual_hash, xvisual);
  return visual;
}

GdkVisual*
gdkx_visual_get (VisualID xvisualid)
{
  int i;

  for (i = 0; i < nvisuals; i++)
    if (xvisualid == visuals[i].xvisual->visualid)
      return (GdkVisual*) &visuals[i];

  return NULL;
}

static void
gdk_visual_add (GdkVisual *visual)
{
  GdkVisualPrivate *private;

  if (!visual_hash)
    visual_hash = g_hash_table_new ((GHashFunc) gdk_visual_hash,
				    (GCompareFunc) gdk_visual_compare);

  private = (GdkVisualPrivate*) visual;

  g_hash_table_insert (visual_hash, private->xvisual, visual);
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

/* This hash stuff is pretty useless on Windows, as there is only
   one visual... */

static guint
gdk_visual_hash (Visual *key)
{
  return key->visualid;
}

static gint
gdk_visual_compare (Visual *a,
		    Visual *b)
{
  return (a->visualid == b->visualid);
}
