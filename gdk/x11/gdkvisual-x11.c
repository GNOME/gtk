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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkvisualprivate.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct _GdkX11Visual
{
  GdkVisual visual;

  Visual *xvisual;
  Colormap colormap;
};

struct _GdkX11VisualClass
{
  GdkVisualClass visual_class;
};

static void     gdk_visual_add            (GdkVisual *visual);
static guint    gdk_visual_hash           (Visual    *key);
static gboolean gdk_visual_equal          (Visual    *a,
					   Visual    *b);


G_DEFINE_TYPE (GdkX11Visual, gdk_x11_visual, GDK_TYPE_VISUAL)

static void
gdk_x11_visual_init (GdkX11Visual *x11_visual)
{
  x11_visual->colormap = None;
}

static void
gdk_x11_visual_finalize (GObject *object)
{
  GdkVisual *visual = (GdkVisual *)object;
  GdkX11Visual *x11_visual = (GdkX11Visual *)object;

  if (x11_visual->colormap != None)
    XFreeColormap (GDK_SCREEN_XDISPLAY (visual->screen), x11_visual->colormap);

  G_OBJECT_CLASS (gdk_x11_visual_parent_class)->finalize (object);
}

static void
gdk_x11_visual_dispose (GObject *object)
{
  GdkVisual *visual = (GdkVisual *)object;
  GdkX11Visual *x11_visual = (GdkX11Visual *)object;

  if (x11_visual->colormap != None)
    {
      XFreeColormap (GDK_SCREEN_XDISPLAY (visual->screen), x11_visual->colormap);
      x11_visual->colormap = None;
    }

  G_OBJECT_CLASS (gdk_x11_visual_parent_class)->dispose (object);
}

static void
gdk_x11_visual_class_init (GdkX11VisualClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_x11_visual_finalize;
  object_class->dispose = gdk_x11_visual_dispose;
}

void
_gdk_x11_screen_init_visuals (GdkScreen *screen)
{
  static const gint possible_depths[8] = { 32, 30, 24, 16, 15, 8, 4, 1 };
  static const GdkVisualType possible_types[6] =
    {
      GDK_VISUAL_DIRECT_COLOR,
      GDK_VISUAL_TRUE_COLOR,
      GDK_VISUAL_PSEUDO_COLOR,
      GDK_VISUAL_STATIC_COLOR,
      GDK_VISUAL_GRAYSCALE,
      GDK_VISUAL_STATIC_GRAY
    };

  GdkX11Screen *x11_screen;
  XVisualInfo *visual_list;
  XVisualInfo visual_template;
  GdkVisual *temp_visual;
  Visual *default_xvisual;
  GdkVisual **visuals;
  int nxvisuals;
  int nvisuals;
  int i, j;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  x11_screen = GDK_X11_SCREEN (screen);

  nxvisuals = 0;
  visual_template.screen = x11_screen->screen_num;
  visual_list = XGetVisualInfo (x11_screen->xdisplay, VisualScreenMask, &visual_template, &nxvisuals);

  visuals = g_new (GdkVisual *, nxvisuals);
  for (i = 0; i < nxvisuals; i++)
    visuals[i] = g_object_new (GDK_TYPE_X11_VISUAL, NULL);

  default_xvisual = DefaultVisual (x11_screen->xdisplay, x11_screen->screen_num);

  nvisuals = 0;
  for (i = 0; i < nxvisuals; i++)
    {
      visuals[nvisuals]->screen = screen;

      if (visual_list[i].depth >= 1)
	{
#ifdef __cplusplus
	  switch (visual_list[i].c_class)
#else /* __cplusplus */
	  switch (visual_list[i].class)
#endif /* __cplusplus */
	    {
	    case StaticGray:
	      visuals[nvisuals]->type = GDK_VISUAL_STATIC_GRAY;
	      break;
	    case GrayScale:
	      visuals[nvisuals]->type = GDK_VISUAL_GRAYSCALE;
	      break;
	    case StaticColor:
	      visuals[nvisuals]->type = GDK_VISUAL_STATIC_COLOR;
	      break;
	    case PseudoColor:
	      visuals[nvisuals]->type = GDK_VISUAL_PSEUDO_COLOR;
	      break;
	    case TrueColor:
	      visuals[nvisuals]->type = GDK_VISUAL_TRUE_COLOR;
	      break;
	    case DirectColor:
	      visuals[nvisuals]->type = GDK_VISUAL_DIRECT_COLOR;
	      break;
	    }

	  visuals[nvisuals]->depth = visual_list[i].depth;
	  visuals[nvisuals]->byte_order =
	    (ImageByteOrder(x11_screen->xdisplay) == LSBFirst) ?
	    GDK_LSB_FIRST : GDK_MSB_FIRST;
	  visuals[nvisuals]->red_mask = visual_list[i].red_mask;
	  visuals[nvisuals]->green_mask = visual_list[i].green_mask;
	  visuals[nvisuals]->blue_mask = visual_list[i].blue_mask;
	  visuals[nvisuals]->colormap_size = visual_list[i].colormap_size;
	  visuals[nvisuals]->bits_per_rgb = visual_list[i].bits_per_rgb;
	  GDK_X11_VISUAL (visuals[nvisuals])->xvisual = visual_list[i].visual;

	  if ((visuals[nvisuals]->type != GDK_VISUAL_TRUE_COLOR) &&
	      (visuals[nvisuals]->type != GDK_VISUAL_DIRECT_COLOR))
	    {
	      visuals[nvisuals]->red_mask = 0;
	      visuals[nvisuals]->green_mask = 0;
	      visuals[nvisuals]->blue_mask = 0;
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
	  if (visuals[j]->depth >= visuals[i]->depth)
	    {
	      if ((visuals[j]->depth == 8) && (visuals[i]->depth == 8))
		{
		  if (visuals[j]->type == GDK_VISUAL_PSEUDO_COLOR)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		  else if ((visuals[i]->type != GDK_VISUAL_PSEUDO_COLOR) &&
			   visuals[j]->type > visuals[i]->type)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		}
	      else if ((visuals[j]->depth > visuals[i]->depth) ||
		       ((visuals[j]->depth == visuals[i]->depth) &&
			(visuals[j]->type > visuals[i]->type)))
		{
		  temp_visual = visuals[j];
		  visuals[j] = visuals[i];
		  visuals[i] = temp_visual;
		}
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    {
      if (default_xvisual->visualid == GDK_X11_VISUAL (visuals[i])->xvisual->visualid)
         {
           x11_screen->system_visual = visuals[i];
           GDK_X11_VISUAL (visuals[i])->colormap =
               DefaultColormap (x11_screen->xdisplay, x11_screen->screen_num);
         }

      /* For now, we only support 8888 ARGB for the "rgba visual".
       * Additional formats (like ABGR) could be added later if they
       * turn up.
       */
      if (x11_screen->rgba_visual == NULL &&
          visuals[i]->depth == 32 &&
	  (visuals[i]->red_mask   == 0xff0000 &&
	   visuals[i]->green_mask == 0x00ff00 &&
	   visuals[i]->blue_mask  == 0x0000ff))
	{
	  x11_screen->rgba_visual = visuals[i];
        }
    }

#ifdef G_ENABLE_DEBUG
  if (GDK_DEBUG_CHECK (MISC))
    {
      static const gchar *const visual_names[] =
      {
        "static gray",
        "grayscale",
        "static color",
        "pseudo color",
        "true color",
        "direct color",
      };

      for (i = 0; i < nvisuals; i++)
        g_message ("visual: %s: %d", visual_names[visuals[i]->type], visuals[i]->depth);
    }
#endif /* G_ENABLE_DEBUG */

  x11_screen->navailable_depths = 0;
  for (i = 0; i < G_N_ELEMENTS (possible_depths); i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->depth == possible_depths[i])
	    {
	      x11_screen->available_depths[x11_screen->navailable_depths++] = visuals[j]->depth;
	      break;
	    }
	}
    }

  if (x11_screen->navailable_depths == 0)
    g_error ("unable to find a usable depth");

  x11_screen->navailable_types = 0;
  for (i = 0; i < G_N_ELEMENTS (possible_types); i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->type == possible_types[i])
	    {
	      x11_screen->available_types[x11_screen->navailable_types++] = visuals[j]->type;
	      break;
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    gdk_visual_add (visuals[i]);

  if (x11_screen->navailable_types == 0)
    g_error ("unable to find a usable visual type");

  x11_screen->visuals = visuals;
  x11_screen->nvisuals = nvisuals;

  /* If GL is available we want to pick better default/rgba visuals,
     as we care about glx details such as alpha/depth/stencil depth,
     stereo and double buffering */
  _gdk_x11_screen_update_visuals_for_gl (screen);
}

gint
_gdk_x11_screen_visual_get_best_depth (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->available_depths[0];
}

GdkVisualType
_gdk_x11_screen_visual_get_best_type (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->available_types[0];
}

GdkVisual *
_gdk_x11_screen_get_system_visual (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return ((GdkVisual *) GDK_X11_SCREEN (screen)->system_visual);
}

GdkVisual*
_gdk_x11_screen_visual_get_best (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  return x11_screen->visuals[0];
}

GdkVisual*
_gdk_x11_screen_visual_get_best_with_depth (GdkScreen *screen,
                                            gint       depth)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GdkVisual *return_val;
  int i;
  
  return_val = NULL;
  for (i = 0; i < x11_screen->nvisuals; i++)
    if (depth == x11_screen->visuals[i]->depth)
      {
	return_val = x11_screen->visuals[i];
	break;
      }

  return return_val;
}

GdkVisual*
_gdk_x11_screen_visual_get_best_with_type (GdkScreen     *screen,
                                           GdkVisualType  visual_type)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < x11_screen->nvisuals; i++)
    if (visual_type == x11_screen->visuals[i]->type)
      {
	return_val = x11_screen->visuals[i];
	break;
      }

  return return_val;
}

GdkVisual*
_gdk_x11_screen_visual_get_best_with_both (GdkScreen     *screen,
                                           gint           depth,
                                           GdkVisualType  visual_type)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < x11_screen->nvisuals; i++)
    if ((depth == x11_screen->visuals[i]->depth) &&
	(visual_type == x11_screen->visuals[i]->type))
      {
	return_val = x11_screen->visuals[i];
	break;
      }

  return return_val;
}

void
_gdk_x11_screen_query_depths  (GdkScreen  *screen,
                               gint      **depths,
                               gint       *count)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  *count = x11_screen->navailable_depths;
  *depths = x11_screen->available_depths;
}

void
_gdk_x11_screen_query_visual_types (GdkScreen      *screen,
                                    GdkVisualType **visual_types,
                                    gint           *count)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  *count = x11_screen->navailable_types;
  *visual_types = x11_screen->available_types;
}

GList *
_gdk_x11_screen_list_visuals (GdkScreen *screen)
{
  GList *list;
  GdkX11Screen *x11_screen;
  guint i;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  x11_screen = GDK_X11_SCREEN (screen);

  list = NULL;

  for (i = 0; i < x11_screen->nvisuals; ++i)
    list = g_list_append (list, x11_screen->visuals[i]);

  return list;
}

/**
 * gdk_x11_screen_lookup_visual:
 * @screen: (type GdkX11Screen): a #GdkScreen.
 * @xvisualid: an X Visual ID.
 *
 * Looks up the #GdkVisual for a particular screen and X Visual ID.
 *
 * Returns: (transfer none) (type GdkX11Visual): the #GdkVisual (owned by the screen
 *   object), or %NULL if the visual ID wasn’t found.
 *
 * Since: 2.2
 */
GdkVisual *
gdk_x11_screen_lookup_visual (GdkScreen *screen,
                              VisualID   xvisualid)
{
  int i;
  GdkX11Screen *x11_screen;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  x11_screen = GDK_X11_SCREEN (screen);

  for (i = 0; i < x11_screen->nvisuals; i++)
    if (xvisualid == GDK_X11_VISUAL (x11_screen->visuals[i])->xvisual->visualid)
      return x11_screen->visuals[i];

  return NULL;
}

static void
gdk_visual_add (GdkVisual *visual)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (visual->screen);

  if (!x11_screen->visual_hash)
    x11_screen->visual_hash = g_hash_table_new ((GHashFunc) gdk_visual_hash,
                                                (GEqualFunc) gdk_visual_equal);

  g_hash_table_insert (x11_screen->visual_hash, GDK_X11_VISUAL (visual)->xvisual, visual);
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

/**
 * _gdk_visual_get_x11_colormap:
 * @visual: the visual to get the colormap from
 *
 * Gets the colormap to use
 *
 * Returns: the X Colormap to use for new windows using @visual
 **/
Colormap
_gdk_visual_get_x11_colormap (GdkVisual *visual)
{
  GdkX11Visual *x11_visual;

  g_return_val_if_fail (GDK_IS_VISUAL (visual), None);

  x11_visual = GDK_X11_VISUAL (visual);

  if (x11_visual->colormap == None)
    {
      x11_visual->colormap = XCreateColormap (GDK_SCREEN_XDISPLAY (visual->screen),
                                              GDK_SCREEN_XROOTWIN (visual->screen),
                                              x11_visual->xvisual,
                                              AllocNone);
    }

  return x11_visual->colormap;
}

/**
 * gdk_x11_visual_get_xvisual:
 * @visual: (type GdkX11Visual): a #GdkVisual.
 *
 * Returns the X visual belonging to a #GdkVisual.
 *
 * Returns: (transfer none): an Xlib Visual*.
 **/
Visual *
gdk_x11_visual_get_xvisual (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return GDK_X11_VISUAL (visual)->xvisual;
}
