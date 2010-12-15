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

#include "config.h"

#include "gdkvisualprivate.h"

#include "gdkx.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _GdkVisualX11 GdkVisualX11;
typedef struct _GdkVisualClass GdkVisualX11Class;

#define GDK_TYPE_VISUAL_X11 (gdk_visual_x11_get_type ())
#define GDK_VISUAL_X11(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_VISUAL_X11, GdkVisualX11))

struct _GdkVisualX11
{
  GdkVisual visual;

  Visual *xvisual;
  Colormap colormap;
};

static void     gdk_visual_add            (GdkVisual *visual);
static void     gdk_visual_decompose_mask (gulong     mask,
					   gint      *shift,
					   gint      *prec);
static guint    gdk_visual_hash           (Visual    *key);
static gboolean gdk_visual_equal          (Visual    *a,
					   Visual    *b);


#ifdef G_ENABLE_DEBUG

static const gchar *const visual_names[] =
{
  "static gray",
  "grayscale",
  "static color",
  "pseudo color",
  "true color",
  "direct color",
};

#endif /* G_ENABLE_DEBUG */

G_DEFINE_TYPE (GdkVisualX11, gdk_visual_x11, GDK_TYPE_VISUAL)

static void
gdk_visual_x11_init (GdkVisualX11 *visual_x11)
{
  visual_x11->colormap = None;
}

static void
gdk_visual_x11_finalize (GObject *object)
{
  GdkVisual *visual = (GdkVisual *)object;
  GdkVisualX11 *visual_x11 = (GdkVisualX11 *)object;

  if (visual_x11->colormap != None)
    XFreeColormap (GDK_SCREEN_XDISPLAY (visual->screen), visual_x11->colormap);

  G_OBJECT_CLASS (gdk_visual_x11_parent_class)->finalize (object);
}

static void
gdk_visual_x11_class_init (GdkVisualX11Class *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_visual_x11_finalize;
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

  GdkScreenX11 *screen_x11;
  XVisualInfo *visual_list;
  XVisualInfo visual_template;
  GdkVisual *temp_visual;
  Visual *default_xvisual;
  GdkVisual **visuals;
  int nxvisuals;
  int nvisuals;
  int i, j;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  screen_x11 = GDK_SCREEN_X11 (screen);

  nxvisuals = 0;
  visual_template.screen = screen_x11->screen_num;
  visual_list = XGetVisualInfo (screen_x11->xdisplay, VisualScreenMask, &visual_template, &nxvisuals);

  visuals = g_new (GdkVisual *, nxvisuals);
  for (i = 0; i < nxvisuals; i++)
    visuals[i] = g_object_new (GDK_TYPE_VISUAL_X11, NULL);

  default_xvisual = DefaultVisual (screen_x11->xdisplay, screen_x11->screen_num);

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
	    (ImageByteOrder(screen_x11->xdisplay) == LSBFirst) ?
	    GDK_LSB_FIRST : GDK_MSB_FIRST;
	  visuals[nvisuals]->red_mask = visual_list[i].red_mask;
	  visuals[nvisuals]->green_mask = visual_list[i].green_mask;
	  visuals[nvisuals]->blue_mask = visual_list[i].blue_mask;
	  visuals[nvisuals]->colormap_size = visual_list[i].colormap_size;
	  visuals[nvisuals]->bits_per_rgb = visual_list[i].bits_per_rgb;
	  GDK_VISUAL_X11 (visuals[nvisuals])->xvisual = visual_list[i].visual;

	  if ((visuals[nvisuals]->type == GDK_VISUAL_TRUE_COLOR) ||
	      (visuals[nvisuals]->type == GDK_VISUAL_DIRECT_COLOR))
	    {
	      gdk_visual_decompose_mask (visuals[nvisuals]->red_mask,
					 &visuals[nvisuals]->red_shift,
					 &visuals[nvisuals]->red_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals]->green_mask,
					 &visuals[nvisuals]->green_shift,
					 &visuals[nvisuals]->green_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals]->blue_mask,
					 &visuals[nvisuals]->blue_shift,
					 &visuals[nvisuals]->blue_prec);
	    }
	  else
	    {
	      visuals[nvisuals]->red_mask = 0;
	      visuals[nvisuals]->red_shift = 0;
	      visuals[nvisuals]->red_prec = 0;

	      visuals[nvisuals]->green_mask = 0;
	      visuals[nvisuals]->green_shift = 0;
	      visuals[nvisuals]->green_prec = 0;

	      visuals[nvisuals]->blue_mask = 0;
	      visuals[nvisuals]->blue_shift = 0;
	      visuals[nvisuals]->blue_prec = 0;
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
      if (default_xvisual->visualid == GDK_VISUAL_X11 (visuals[i])->xvisual->visualid)
         {
           screen_x11->system_visual = visuals[i];
           GDK_VISUAL_X11 (visuals[i])->colormap =
               DefaultColormap (screen_x11->xdisplay, screen_x11->screen_num);
         }

      /* For now, we only support 8888 ARGB for the "rgba visual".
       * Additional formats (like ABGR) could be added later if they
       * turn up.
       */
      if (visuals[i]->depth == 32 &&
	  (visuals[i]->red_mask   == 0xff0000 &&
	   visuals[i]->green_mask == 0x00ff00 &&
	   visuals[i]->blue_mask  == 0x0000ff))
	{
	  screen_x11->rgba_visual = visuals[i];
        }
    }

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_MISC)
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

  screen_x11->navailable_depths = 0;
  for (i = 0; i < G_N_ELEMENTS (possible_depths); i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->depth == possible_depths[i])
	    {
	      screen_x11->available_depths[screen_x11->navailable_depths++] = visuals[j]->depth;
	      break;
	    }
	}
    }

  if (screen_x11->navailable_depths == 0)
    g_error ("unable to find a usable depth");

  screen_x11->navailable_types = 0;
  for (i = 0; i < G_N_ELEMENTS (possible_types); i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->type == possible_types[i])
	    {
	      screen_x11->available_types[screen_x11->navailable_types++] = visuals[j]->type;
	      break;
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    gdk_visual_add (visuals[i]);

  if (screen_x11->navailable_types == 0)
    g_error ("unable to find a usable visual type");

  screen_x11->visuals = visuals;
  screen_x11->nvisuals = nvisuals;
}

gint
_gdk_screen_x11_visual_get_best_depth (GdkScreen *screen)
{
  return GDK_SCREEN_X11 (screen)->available_depths[0];
}

GdkVisualType
_gdk_screen_x11_visual_get_best_type (GdkScreen *screen)
{
  return GDK_SCREEN_X11 (screen)->available_types[0];
}

GdkVisual *
_gdk_screen_x11_get_system_visual (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return ((GdkVisual *) GDK_SCREEN_X11 (screen)->system_visual);
}

GdkVisual*
_gdk_screen_x11_visual_get_best (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return screen_x11->visuals[0];
}

GdkVisual*
_gdk_screen_x11_visual_get_best_with_depth (GdkScreen *screen,
                                            gint       depth)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  GdkVisual *return_val;
  int i;
  
  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if (depth == screen_x11->visuals[i]->depth)
      {
	return_val = screen_x11->visuals[i];
	break;
      }

  return return_val;
}

GdkVisual*
_gdk_screen_x11_visual_get_best_with_type (GdkScreen     *screen,
                                           GdkVisualType  visual_type)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if (visual_type == screen_x11->visuals[i]->type)
      {
	return_val = screen_x11->visuals[i];
	break;
      }

  return return_val;
}

GdkVisual*
_gdk_screen_x11_visual_get_best_with_both (GdkScreen     *screen,
                                           gint           depth,
                                           GdkVisualType  visual_type)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if ((depth == screen_x11->visuals[i]->depth) &&
	(visual_type == screen_x11->visuals[i]->type))
      {
	return_val = screen_x11->visuals[i];
	break;
      }

  return return_val;
}

void
_gdk_screen_x11_query_depths  (GdkScreen  *screen,
                               gint      **depths,
                               gint       *count)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  *count = screen_x11->navailable_depths;
  *depths = screen_x11->available_depths;
}

void
_gdk_screen_x11_query_visual_types (GdkScreen      *screen,
                                    GdkVisualType **visual_types,
                                    gint           *count)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  *count = screen_x11->navailable_types;
  *visual_types = screen_x11->available_types;
}

GList *
_gdk_screen_x11_list_visuals (GdkScreen *screen)
{
  GList *list;
  GdkScreenX11 *screen_x11;
  guint i;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_x11 = GDK_SCREEN_X11 (screen);

  list = NULL;

  for (i = 0; i < screen_x11->nvisuals; ++i)
    list = g_list_append (list, screen_x11->visuals[i]);

  return list;
}

/**
 * gdk_x11_screen_lookup_visual:
 * @screen: a #GdkScreen.
 * @xvisualid: an X Visual ID.
 *
 * Looks up the #GdkVisual for a particular screen and X Visual ID.
 *
 * Returns: the #GdkVisual (owned by the screen object), or %NULL
 *   if the visual ID wasn't found.
 *
 * Since: 2.2
 */
GdkVisual *
gdk_x11_screen_lookup_visual (GdkScreen *screen,
                              VisualID   xvisualid)
{
  int i;
  GdkScreenX11 *screen_x11;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_x11 = GDK_SCREEN_X11 (screen);

  for (i = 0; i < screen_x11->nvisuals; i++)
    if (xvisualid == GDK_VISUAL_X11 (screen_x11->visuals[i])->xvisual->visualid)
      return screen_x11->visuals[i];

  return NULL;
}

static void
gdk_visual_add (GdkVisual *visual)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (visual->screen);

  if (!screen_x11->visual_hash)
    screen_x11->visual_hash = g_hash_table_new ((GHashFunc) gdk_visual_hash,
                                                (GEqualFunc) gdk_visual_equal);

  g_hash_table_insert (screen_x11->visual_hash, GDK_VISUAL_X11 (visual)->xvisual, visual);
}

static void
gdk_visual_decompose_mask (gulong  mask,
                           gint   *shift,
                           gint   *prec)
{
  *shift = 0;
  *prec = 0;

  if (mask == 0)
    {
      g_warning ("Mask is 0 in visual. Server bug ?");
      return;
    }

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
  GdkVisualX11 *visual_x11;

  g_return_val_if_fail (GDK_IS_VISUAL (visual), None);

  visual_x11 = GDK_VISUAL_X11 (visual);

  if (visual_x11->colormap == None)
    {
      visual_x11->colormap = XCreateColormap (GDK_SCREEN_XDISPLAY (visual->screen),
                                              GDK_SCREEN_XROOTWIN (visual->screen),
                                              visual_x11->xvisual,
                                              AllocNone);
    }

  return visual_x11->colormap;
}

/**
 * gdk_x11_visual_get_xvisual:
 * @visual: a #GdkVisual.
 *
 * Returns the X visual belonging to a #GdkVisual.
 *
 * Return value: an Xlib <type>Visual*</type>.
 **/
Visual *
gdk_x11_visual_get_xvisual (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return GDK_VISUAL_X11 (visual)->xvisual;
}
