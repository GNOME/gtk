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
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

static void     gdk_visual_add            (GdkVisual *visual);
static void     gdk_visual_decompose_mask (gulong     mask,
					   gint      *shift,
					   gint      *prec);
static guint    gdk_visual_hash           (Visual    *key);
static gboolean gdk_visual_equal          (Visual    *a,
					   Visual    *b);


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
_gdk_visual_init (GdkScreen *screen)
{
  GdkScreenImplX11 *screen_impl;

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
  GdkVisualPrivate **visuals;
  int nxvisuals;
  int nvisuals;
  int i, j;
  
  g_return_if_fail (GDK_IS_SCREEN (screen));
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  visual_template.screen = screen_impl->screen_num;
  visual_list = XGetVisualInfo (screen_impl->xdisplay, VisualScreenMask, &visual_template, &nxvisuals);
  
  visuals = g_new (GdkVisualPrivate *, nxvisuals);
  for (i = 0; i < nxvisuals; i++)
    visuals[i] = g_object_new (GDK_TYPE_VISUAL, NULL);

  default_xvisual = DefaultVisual (screen_impl->xdisplay, screen_impl->screen_num);

  nvisuals = 0;
  for (i = 0; i < nxvisuals; i++)
    {
      visuals[nvisuals]->visual.screen = screen;
      if (visual_list[i].depth >= 1) {
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
	    (ImageByteOrder(screen_impl->xdisplay) == LSBFirst) ?
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
	screen_impl->system_visual = visuals[i];
	break;
      }

#ifdef G_ENABLE_DEBUG 
  if (gdk_debug_flags & GDK_DEBUG_MISC)
    for (i = 0; i < nvisuals; i++)
      g_message ("visual: %s: %d",
		 visual_names[visuals[i]->visual.type],
		 visuals[i]->visual.depth);
#endif /* G_ENABLE_DEBUG */

  screen_impl->navailable_depths = 0;
  for (i = 0; i < npossible_depths; i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.depth == possible_depths[i])
	    {
	      screen_impl->available_depths[screen_impl->navailable_depths++] = visuals[j]->visual.depth;
	      break;
	    }
	}
    }

  if (screen_impl->navailable_depths == 0)
    g_error ("unable to find a usable depth");

  screen_impl->navailable_types = 0;
  for (i = 0; i < npossible_types; i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.type == possible_types[i])
	    {
	      screen_impl->available_types[screen_impl->navailable_types++] = visuals[j]->visual.type;
	      break;
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    gdk_visual_add ((GdkVisual*) visuals[i]);

  if (screen_impl->navailable_types == 0)
    g_error ("unable to find a usable visual type");

  screen_impl->visuals = visuals;
  screen_impl->nvisuals = nvisuals;
  screen_impl->visual_initialised = TRUE;
}

gint
gdk_visual_get_best_depth_for_screen (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_IMPL_X11 (screen)->available_depths[0];
}
#ifndef GDK_MULTIHEAD_SAFE
gint
gdk_visual_get_best_depth (void)
{
  GDK_NOTE (MULTIHEAD,
	    g_message("Use gdk_visual_get_best_depth_for_screen instead\n"));
  return gdk_visual_get_best_depth_for_screen (gdk_get_default_screen ());
}
#endif

GdkVisualType
gdk_visual_get_best_type_for_screen (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_IMPL_X11 (screen)->available_types[0];
}

#ifndef GDK_MULTIHEAD_SAFE
GdkVisualType
gdk_visual_get_best_type (void)
{
  GDK_NOTE (MULTIHEAD,
	    g_message("Use gdk_visual_get_best_type_for_screen instead\n"));
  return gdk_visual_get_best_type_for_screen (gdk_get_default_screen ());
}
#endif

GdkVisual *
gdk_visual_get_system_for_screen (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return ((GdkVisual *) GDK_SCREEN_IMPL_X11 (screen)->system_visual);
}

#ifndef GDK_MULTIHEAD_SAFE
GdkVisual*
gdk_visual_get_system (void)
{
  GDK_NOTE (MULTIHEAD,
	    g_message("Use gdk_visual_get_system_for_screen instead\n"));
  return gdk_visual_get_system_for_screen (gdk_get_default_screen ());
}
#endif

GdkVisual *
gdk_visual_get_best_for_screen (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return ((GdkVisual *)GDK_SCREEN_IMPL_X11 (screen)->visuals[0]);
}

GdkVisual*
gdk_visual_get_best (void)
{
  return gdk_visual_get_best_for_screen (gdk_get_default_screen ());
}

GdkVisual *
gdk_visual_get_best_with_depth_for_screen (GdkScreen *screen,
					   gint       depth)
{
  GdkVisual *return_val;
  GdkScreenImplX11 *screen_impl;
  int i;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);
  
  return_val = NULL;
  for (i = 0; i < screen_impl->nvisuals; i++)
    if (depth == screen_impl->visuals[i]->visual.depth)
      {
	return_val = (GdkVisual *) & (screen_impl->visuals[i]);
	break;
      }

  return return_val;
}

GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  return gdk_visual_get_best_with_depth_for_screen (gdk_get_default_screen (), depth);
}

GdkVisual *
gdk_visual_get_best_with_type_for_screen (GdkScreen     *screen,
					  GdkVisualType  visual_type)
{
  GdkVisual *return_val;
  GdkScreenImplX11 *screen_impl; 
  int i;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return_val = NULL;
  for (i = 0; i < screen_impl->nvisuals; i++)
    if (visual_type == screen_impl->visuals[i]->visual.type)
      {
	return_val = (GdkVisual *) screen_impl->visuals[i];
	break;
      }

  return return_val;
}

#ifndef GDK_MULTIHEAD_SAFE
GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  GDK_NOTE (MULTIHEAD,
	    g_message("Use gdk_visual_get_best_with_type_for_screen instead\n"));	
  return gdk_visual_get_best_with_type_for_screen (gdk_get_default_screen (), visual_type);
}
#endif

GdkVisual *
gdk_visual_get_best_with_both_for_screen (GdkScreen    *screen,
					  gint          depth,
					  GdkVisualType visual_type)
{
  GdkVisual *return_val;
  GdkScreenImplX11 *screen_impl; 
  int i;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return_val = NULL;
  for (i = 0; i < screen_impl->nvisuals; i++)
    if ((depth == screen_impl->visuals[i]->visual.depth) &&
	(visual_type == screen_impl->visuals[i]->visual.type))
      {
	return_val = (GdkVisual *) screen_impl->visuals[i];
	break;
      }

  return return_val;
}

#ifndef GDK_MULTIHEAD_SAFE
GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  GDK_NOTE (MULTIHEAD,
	    g_message("Use gdk_visual_get_best_with_both_for_screen instead\n"));	
  return gdk_visual_get_best_with_both_for_screen (gdk_get_default_screen (),
						   depth,
						   visual_type);
}
#endif

void
gdk_query_depths_for_screen (GdkScreen  *screen,
			     gint      **depths,
			     gint       *count)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  *count = GDK_SCREEN_IMPL_X11 (screen)->navailable_depths;
  *depths = GDK_SCREEN_IMPL_X11 (screen)->available_depths;
}

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  GDK_NOTE (MULTIHEAD,
	   g_message("Use gdk_query_depths_for_screen instead\n"));
  gdk_query_depths_for_screen (gdk_get_default_screen (), depths, count);
}
#endif

void
gdk_query_visual_types_for_screen (GdkScreen      *screen,
				   GdkVisualType **visual_types,
				   gint           *count)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  *count = GDK_SCREEN_IMPL_X11 (screen)->navailable_types;
  *visual_types = GDK_SCREEN_IMPL_X11 (screen)->available_types;
}

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  GDK_NOTE (MULTIHEAD,
	    g_message("Use gdk_query_visual_types_for_screen instead\n"));
  gdk_query_visual_types_for_screen (gdk_get_default_screen (), visual_types, count);
}
#endif

GList *
gdk_list_visuals_for_screen (GdkScreen *screen)
{
  GList *list;
  GdkScreenImplX11 *screen_impl;
  guint i;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);
  list = NULL;

  for (i = 0; i < screen_impl->nvisuals; ++i)
    list = g_list_append (list, screen_impl->visuals[i]);

  return list;
}

GList*
gdk_list_visuals (void)
{
  return gdk_list_visuals_for_screen (gdk_get_default_screen ());
}

GdkVisual*
gdk_visual_lookup_for_screen (GdkScreen *screen,
			      Visual    *xvisual)
{
  GdkVisual *visual;
  GdkScreenImplX11 *screen_impl;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);  
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  if (!screen_impl->visual_hash)
    return NULL;

  visual = g_hash_table_lookup (screen_impl->visual_hash, xvisual);
  return visual;
}
#ifndef GDK_MULTIHEAD_SAFE
GdkVisual*
gdk_visual_lookup (Visual *xvisual)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_visual_lookup_for_screen instead\n"));
  return gdk_visual_lookup_for_screen (gdk_get_default_screen (), xvisual);
}
#endif

GdkVisual *
gdkx_visual_get_for_screen (GdkScreen *screen,
			    VisualID   xvisualid)
{
  int i;
  GdkScreenImplX11 *screen_impl;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  for (i = 0; i < screen_impl->nvisuals; i++)
    if (xvisualid == screen_impl->visuals[i]->xvisual->visualid)
      return (GdkVisual *)  screen_impl->visuals[i];

  return NULL;
}

#ifndef GDK_MULTIHEAD_SAFE
GdkVisual*
gdkx_visual_get (VisualID xvisualid)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdkx_visual_get_for_screen instead\n"));
  return gdkx_visual_get_for_screen (gdk_get_default_screen (), xvisualid);
}
#endif

static void
gdk_visual_add (GdkVisual *visual)
{
  GdkVisualPrivate *private;
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (visual->screen);
  
  if (!screen_impl->visual_hash)
    screen_impl->visual_hash = g_hash_table_new ((GHashFunc) gdk_visual_hash,
					      (GEqualFunc) gdk_visual_equal);

  private = (GdkVisualPrivate *) visual;
  g_hash_table_insert (screen_impl->visual_hash, private->xvisual, visual);
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
