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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "gdkvisual.h"
#include "gdkprivate-x11.h"
#include "gdkinternals.h"

struct _GdkVisualClass
{
  GObjectClass parent_class;
};

static void     gdk_visual_add            (GdkVisual *visual);
static void     gdk_visual_decompose_mask (gulong     mask,
					   gint      *shift,
					   gint      *prec);
static guint    gdk_visual_hash           (Visual    *key);
static gboolean gdk_visual_equal          (Visual    *a,
					   Visual    *b);


static GdkVisualPrivate *system_visual;
static GdkVisualPrivate **visuals;
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

static void
gdk_visual_finalize (GObject *object)
{
  g_error ("A GdkVisual object was finalized. This should not happen");
}

static void
gdk_visual_class_init (GObjectClass *class)
{
  class->finalize = gdk_visual_finalize;
}


GType
gdk_visual_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkVisualClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_visual_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkVisualPrivate),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkVisual",
                                            &object_info, 0);
    }
  
  return object_type;
}


void
_gdk_visual_init (void)
{
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

  XVisualInfo *visual_list;
  XVisualInfo visual_template;
  GdkVisualPrivate *temp_visual;
  Visual *default_xvisual;
  int nxvisuals;
  int i, j;

  visual_template.screen = _gdk_screen;
  visual_list = XGetVisualInfo (gdk_display, VisualScreenMask, &visual_template, &nxvisuals);
  
  visuals = g_new (GdkVisualPrivate *, nxvisuals);
  for (i = 0; i < nxvisuals; i++)
    visuals[i] = g_object_new (GDK_TYPE_VISUAL, NULL);

  default_xvisual = DefaultVisual (gdk_display, _gdk_screen);

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
	      visuals[nvisuals]->visual.type = GDK_VISUAL_STATIC_GRAY;
	      break;
	    case GrayScale:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_GRAYSCALE;
	      break;
	    case StaticColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_STATIC_COLOR;
	      break;
	    case PseudoColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_PSEUDO_COLOR;
	      break;
	    case TrueColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_TRUE_COLOR;
	      break;
	    case DirectColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_DIRECT_COLOR;
	      break;
	    }

	  visuals[nvisuals]->visual.depth = visual_list[i].depth;
	  visuals[nvisuals]->visual.byte_order =
	    (ImageByteOrder(gdk_display) == LSBFirst) ?
	    GDK_LSB_FIRST : GDK_MSB_FIRST;
	  visuals[nvisuals]->visual.red_mask = visual_list[i].red_mask;
	  visuals[nvisuals]->visual.green_mask = visual_list[i].green_mask;
	  visuals[nvisuals]->visual.blue_mask = visual_list[i].blue_mask;
	  visuals[nvisuals]->visual.colormap_size = visual_list[i].colormap_size;
	  visuals[nvisuals]->visual.bits_per_rgb = visual_list[i].bits_per_rgb;
	  visuals[nvisuals]->xvisual = visual_list[i].visual;

	  if ((visuals[nvisuals]->visual.type == GDK_VISUAL_TRUE_COLOR) ||
	      (visuals[nvisuals]->visual.type == GDK_VISUAL_DIRECT_COLOR))
	    {
	      gdk_visual_decompose_mask (visuals[nvisuals]->visual.red_mask,
					 &visuals[nvisuals]->visual.red_shift,
					 &visuals[nvisuals]->visual.red_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals]->visual.green_mask,
					 &visuals[nvisuals]->visual.green_shift,
					 &visuals[nvisuals]->visual.green_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals]->visual.blue_mask,
					 &visuals[nvisuals]->visual.blue_shift,
					 &visuals[nvisuals]->visual.blue_prec);
	    }
	  else
	    {
	      visuals[nvisuals]->visual.red_mask = 0;
	      visuals[nvisuals]->visual.red_shift = 0;
	      visuals[nvisuals]->visual.red_prec = 0;

	      visuals[nvisuals]->visual.green_mask = 0;
	      visuals[nvisuals]->visual.green_shift = 0;
	      visuals[nvisuals]->visual.green_prec = 0;

	      visuals[nvisuals]->visual.blue_mask = 0;
	      visuals[nvisuals]->visual.blue_shift = 0;
	      visuals[nvisuals]->visual.blue_prec = 0;
	    }

	  nvisuals += 1;
	}
    }

  if (visual_list)
    XFree (visual_list);

  for (i = 0; i < nvisuals; i++)
    {
      for (j = i+1; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.depth >= visuals[i]->visual.depth)
	    {
	      if ((visuals[j]->visual.depth == 8) && (visuals[i]->visual.depth == 8))
		{
		  if (visuals[j]->visual.type == GDK_VISUAL_PSEUDO_COLOR)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		  else if ((visuals[i]->visual.type != GDK_VISUAL_PSEUDO_COLOR) &&
			   visuals[j]->visual.type > visuals[i]->visual.type)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		}
	      else if ((visuals[j]->visual.depth > visuals[i]->visual.depth) ||
		       ((visuals[j]->visual.depth == visuals[i]->visual.depth) &&
			(visuals[j]->visual.type > visuals[i]->visual.type)))
		{
		  temp_visual = visuals[j];
		  visuals[j] = visuals[i];
		  visuals[i] = temp_visual;
		}
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    if (default_xvisual->visualid == visuals[i]->xvisual->visualid)
      {
	system_visual = visuals[i];
	break;
      }

#ifdef G_ENABLE_DEBUG 
  if (_gdk_debug_flags & GDK_DEBUG_MISC)
    for (i = 0; i < nvisuals; i++)
      g_message ("visual: %s: %d",
		 visual_names[visuals[i]->visual.type],
		 visuals[i]->visual.depth);
#endif /* G_ENABLE_DEBUG */

  navailable_depths = 0;
  for (i = 0; i < npossible_depths; i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.depth == possible_depths[i])
	    {
	      available_depths[navailable_depths++] = visuals[j]->visual.depth;
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
	  if (visuals[j]->visual.type == possible_types[i])
	    {
	      available_types[navailable_types++] = visuals[j]->visual.type;
	      break;
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    gdk_visual_add ((GdkVisual*) visuals[i]);

  if (npossible_types == 0)
    g_error ("unable to find a usable visual type");
}

/**
 * gdk_visual_get_best_depth:
 * 
 * Get the best available depth for the default GDK display.  "Best"
 * means "largest," i.e. 32 preferred over 24 preferred over 8 bits
 * per pixel.
 * 
 * Return value: best available depth
 **/
gint
gdk_visual_get_best_depth (void)
{
  return available_depths[0];
}

/**
 * gdk_visual_get_best_type:
 * 
 * Return the best available visual type (the one with the most
 * colors) for the default GDK display.
 * 
 * Return value: best visual type
 **/
GdkVisualType
gdk_visual_get_best_type (void)
{
  return available_types[0];
}

/**
 * gdk_visual_get_system:
 * 
 * Get the default or system visual for the default GDK display.
 * This is the visual for the root window of the display.
 * The return value should not be freed.
 * 
 * Return value: system visual
 **/
GdkVisual*
gdk_visual_get_system (void)
{
  return ((GdkVisual*) system_visual);
}

/**
 * gdk_visual_get_best:
 *
 * Get the visual with the most available colors for the default
 * GDK display. The return value should not be freed.
 * 
 * Return value: best visual
 **/
GdkVisual*
gdk_visual_get_best (void)
{
  return ((GdkVisual*) visuals[0]);
}

/**
 * gdk_visual_get_best_with_depth:
 * @depth: a bit depth
 * 
 * Get the best visual with depth @depth for the default GDK display.
 * Color visuals and visuals with mutable colormaps are preferred
 * over grayscale or fixed-colormap visuals. The return value should not
 * be freed. %NULL may be returned if no visual supports @depth.
 * 
 * Return value: best visual for the given depth
 **/
GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < nvisuals; i++)
    if (depth == visuals[i]->visual.depth)
      {
	return_val = (GdkVisual*) visuals[i];
	break;
      }

  return return_val;
}

/**
 * gdk_visual_get_best_with_type:
 * @visual_type: a visual type
 *
 * Get the best visual of the given @visual_type for the default GDK display.
 * Visuals with higher color depths are considered better. The return value
 * should not be freed. %NULL may be returned if no visual has type
 * @visual_type.
 * 
 * Return value: best visual of the given type
 **/
GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < nvisuals; i++)
    if (visual_type == visuals[i]->visual.type)
      {
	return_val = (GdkVisual*) visuals[i];
	break;
      }

  return return_val;
}

/**
 * gdk_visual_get_best_with_both:
 * @depth: a bit depth
 * @visual_type: a visual type
 *
 * Combines gdk_visual_get_best_with_depth() and gdk_visual_get_best_with_type().
 * 
 * Return value: best visual with both @depth and @visual_type, or %NULL if none
 **/
GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < nvisuals; i++)
    if ((depth == visuals[i]->visual.depth) &&
	(visual_type == visuals[i]->visual.type))
      {
	return_val = (GdkVisual*) visuals[i];
	break;
      }

  return return_val;
}

/**
 * gdk_query_depths:
 * @depths: return location for available depths 
 * @count: return location for number of available depths
 *
 * This function returns the available bit depths for the default
 * display. It's equivalent to listing the visuals
 * (gdk_list_visuals()) and then looking at the depth field in each
 * visual, removing duplicates.
 * 
 * The array returned by this function should not be freed.
 * 
 **/
void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  *count = navailable_depths;
  *depths = available_depths;
}

/**
 * gdk_query_visual_types:
 * @visual_types: return location for the available visual types
 * @count: return location for the number of available visual types
 *
 * This function returns the available visual types for the default
 * display. It's equivalent to listing the visuals
 * (gdk_list_visuals()) and then looking at the type field in each
 * visual, removing duplicates.
 * 
 * The array returned by this function should not be freed.
 * 
 **/
void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  *count = navailable_types;
  *visual_types = available_types;
}

/**
 * gdk_list_visuals:
 * 
 * Lists the available visuals for the default display.
 * A visual describes a hardware image data format.
 * For example, a visual might support 24-bit color, or 8-bit color,
 * and might expect pixels to be in a certain format.
 *
 * Call g_list_free() on the return value when you're finished with it.
 * 
 * Return value: a list of visuals; the list must be freed, but not its contents
 **/
GList*
gdk_list_visuals (void)
{
  GList *list;
  guint i;

  list = NULL;
  for (i = 0; i < nvisuals; ++i)
    list = g_list_append (list, (gpointer) visuals[i]);

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
    if (xvisualid == visuals[i]->xvisual->visualid)
      return (GdkVisual*) visuals[i];

  return NULL;
}


static void
gdk_visual_add (GdkVisual *visual)
{
  GdkVisualPrivate *private;

  if (!visual_hash)
    visual_hash = g_hash_table_new ((GHashFunc) gdk_visual_hash,
				    (GEqualFunc) gdk_visual_equal);

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

static gboolean
gdk_visual_equal (Visual *a,
		  Visual *b)
{
  return (a->visualid == b->visualid);
}

Visual *
gdk_x11_visual_get_xvisual (GdkVisual *visual)
{
  g_return_val_if_fail (visual != NULL, NULL);

  return  ((GdkVisualPrivate*) visual)->xvisual;
}
