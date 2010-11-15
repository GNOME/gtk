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

#include "gdkvisual.h"

#include "gdkprivate-broadway.h"
#include "gdkscreen-broadway.h"
#include "gdkinternals.h"

struct _GdkVisualPrivate
{
  GdkScreen *screen;
};

struct _GdkVisualClass
{
  GObjectClass parent_class;
};

static void     gdk_visual_decompose_mask (gulong     mask,
					   gint      *shift,
					   gint      *prec);


G_DEFINE_TYPE (GdkVisual, gdk_visual, G_TYPE_OBJECT)

static void
gdk_visual_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_visual_parent_class)->finalize (object);
}

static void
gdk_visual_class_init (GdkVisualClass *visual_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (visual_class);

  g_type_class_add_private (object_class, sizeof (GdkVisualPrivate));

  object_class->finalize = gdk_visual_finalize;
}

static void
gdk_visual_init (GdkVisual *visual)
{
  visual->priv = G_TYPE_INSTANCE_GET_PRIVATE (visual,
                                              GDK_TYPE_VISUAL,
                                              GdkVisualPrivate);

}

void
_gdk_visual_init (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;
  GdkVisual **visuals;
  int nvisuals;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  screen_x11 = GDK_SCREEN_X11 (screen);

  nvisuals = 1;
  visuals = g_new (GdkVisual *, nvisuals);

  visuals[0] = g_object_new (GDK_TYPE_VISUAL, NULL);
  visuals[0]->priv->screen = screen;
  visuals[0]->type = GDK_VISUAL_TRUE_COLOR;
  visuals[0]->depth = 32;
  visuals[0]->byte_order = (G_BYTE_ORDER == G_LITTLE_ENDIAN) ? GDK_LSB_FIRST : GDK_MSB_FIRST;
  visuals[0]->red_mask = 0xff0000;
  visuals[0]->green_mask = 0xff00;
  visuals[0]->blue_mask = 0xff;
  visuals[0]->colormap_size = 256;
  visuals[0]->bits_per_rgb = 8;
  gdk_visual_decompose_mask (visuals[0]->red_mask,
			     &visuals[0]->red_shift,
			     &visuals[0]->red_prec);
  gdk_visual_decompose_mask (visuals[0]->green_mask,
			     &visuals[0]->green_shift,
			     &visuals[0]->green_prec);
  gdk_visual_decompose_mask (visuals[0]->blue_mask,
			     &visuals[0]->blue_shift,
			     &visuals[0]->blue_prec);

  visuals[1] = g_object_new (GDK_TYPE_VISUAL, NULL);
  visuals[1]->priv->screen = screen;
  visuals[1]->type = GDK_VISUAL_TRUE_COLOR;
  visuals[1]->depth = 24;
  visuals[1]->byte_order = (G_BYTE_ORDER == G_LITTLE_ENDIAN) ? GDK_LSB_FIRST : GDK_MSB_FIRST;
  visuals[1]->red_mask = 0xff0000;
  visuals[1]->green_mask = 0xff00;
  visuals[1]->blue_mask = 0xff;
  visuals[1]->colormap_size = 256;
  visuals[1]->bits_per_rgb = 8;
  gdk_visual_decompose_mask (visuals[1]->red_mask,
			     &visuals[1]->red_shift,
			     &visuals[1]->red_prec);
  gdk_visual_decompose_mask (visuals[1]->green_mask,
			     &visuals[1]->green_shift,
			     &visuals[1]->green_prec);
  gdk_visual_decompose_mask (visuals[1]->blue_mask,
			     &visuals[1]->blue_shift,
			     &visuals[1]->blue_prec);

  screen_x11->system_visual = visuals[1];
  screen_x11->rgba_visual = visuals[0];

  screen_x11->navailable_depths = 2;
  screen_x11->available_depths[0] = 32;
  screen_x11->available_depths[1] = 24;

  screen_x11->navailable_types = 1;
  screen_x11->available_types[0] = GDK_VISUAL_TRUE_COLOR;

  screen_x11->visuals = visuals;
}

gint
gdk_visual_get_best_depth (void)
{
  GdkScreen *screen = gdk_screen_get_default();

  return GDK_SCREEN_X11 (screen)->available_depths[0];
}

GdkVisualType
gdk_visual_get_best_type (void)
{
  GdkScreen *screen = gdk_screen_get_default();

  return GDK_SCREEN_X11 (screen)->available_types[0];
}

GdkVisual *
gdk_screen_get_system_visual (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return ((GdkVisual *) GDK_SCREEN_X11 (screen)->system_visual);
}

GdkVisual*
gdk_visual_get_best (void)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default());

  return (GdkVisual *)screen_x11->visuals[0];
}

GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if (depth == screen_x11->visuals[i]->depth)
      {
	return_val = (GdkVisual *) screen_x11->visuals[i];
	break;
      }

  return return_val;
}

GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if (visual_type == screen_x11->visuals[i]->type)
      {
	return_val = (GdkVisual *) screen_x11->visuals[i];
	break;
      }

  return return_val;
}

GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if ((depth == screen_x11->visuals[i]->depth) &&
	(visual_type == screen_x11->visuals[i]->type))
      {
	return_val = (GdkVisual *) screen_x11->visuals[i];
	break;
      }

  return return_val;
}

void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());

  *count = screen_x11->navailable_depths;
  *depths = screen_x11->available_depths;
}

void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());

  *count = screen_x11->navailable_types;
  *visual_types = screen_x11->available_types;
}

GList *
gdk_screen_list_visuals (GdkScreen *screen)
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

GdkScreen *
gdk_visual_get_screen (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return visual->priv->screen;
}
