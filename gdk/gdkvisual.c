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
#include <X11/Xutil.h>
#include "gdk.h"
#include "gdkprivate.h"


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

static gint available_depths[4];
static gint navailable_depths;

static GdkVisualType available_types[6];
static gint navailable_types;

#ifdef G_ENABLE_DEBUG

static gchar* visual_names[] =
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
  static gint possible_depths[6] = { 32, 24, 16, 15, 8, 1 };
  static GdkVisualType possible_types[6] =
    {
      GDK_VISUAL_DIRECT_COLOR,
      GDK_VISUAL_TRUE_COLOR,
      GDK_VISUAL_PSEUDO_COLOR,
      GDK_VISUAL_STATIC_COLOR,
      GDK_VISUAL_GRAYSCALE,
      GDK_VISUAL_STATIC_GRAY
    };

  static gint npossible_depths = 6;
  static gint npossible_types = 6;

  XVisualInfo *visual_list;
  XVisualInfo visual_template;
  GdkVisualPrivate temp_visual;
  Visual *default_xvisual;
  int nxvisuals;
  int i, j;

  visual_template.screen = gdk_screen;
  visual_list = XGetVisualInfo (gdk_display, VisualScreenMask, &visual_template, &nxvisuals);
  visuals = g_new (GdkVisualPrivate, nxvisuals);

  default_xvisual = DefaultVisual (gdk_display, gdk_screen);

  nvisuals = 0;
  for (i = 0; i < nxvisuals; i++)
    {
      if (visual_list[i].depth >= 1)
	{
#ifdef __cplusplus
	  switch (visual_list[i].c_class)
#else /* __cplusplus */
	  switch (visual_list[i].class)
#endif /* __cplusplus */
	    {
	    case StaticGray:
	      visuals[nvisuals].visual.type = GDK_VISUAL_STATIC_GRAY;
	      break;
	    case GrayScale:
	      visuals[nvisuals].visual.type = GDK_VISUAL_GRAYSCALE;
	      break;
	    case StaticColor:
	      visuals[nvisuals].visual.type = GDK_VISUAL_STATIC_COLOR;
	      break;
	    case PseudoColor:
	      visuals[nvisuals].visual.type = GDK_VISUAL_PSEUDO_COLOR;
	      break;
	    case TrueColor:
	      visuals[nvisuals].visual.type = GDK_VISUAL_TRUE_COLOR;
	      break;
	    case DirectColor:
	      visuals[nvisuals].visual.type = GDK_VISUAL_DIRECT_COLOR;
	      break;
	    }

	  visuals[nvisuals].visual.depth = visual_list[i].depth;
	  visuals[nvisuals].visual.byte_order =
	    (ImageByteOrder(gdk_display) == LSBFirst) ?
	    GDK_LSB_FIRST : GDK_MSB_FIRST;
	  visuals[nvisuals].visual.red_mask = visual_list[i].red_mask;
	  visuals[nvisuals].visual.green_mask = visual_list[i].green_mask;
	  visuals[nvisuals].visual.blue_mask = visual_list[i].blue_mask;
	  visuals[nvisuals].visual.colormap_size = visual_list[i].colormap_size;
	  visuals[nvisuals].visual.bits_per_rgb = visual_list[i].bits_per_rgb;
	  visuals[nvisuals].xvisual = visual_list[i].visual;

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

	  nvisuals += 1;
	}
    }

  XFree (visual_list);

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

#ifdef G_ENABLE_DEBUG 
  if (gdk_debug_flags & GDK_DEBUG_MISC)
    for (i = 0; i < nvisuals; i++)
      g_print ("Gdk: visual: %s: %d\n",
	       visual_names[visuals[i].visual.type],
	       visuals[i].visual.depth);
#endif /* G_ENABLE_DEBUG */

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
