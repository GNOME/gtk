/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkwidgetactorprivate.h"

#include "gtkwidgetprivate.h"

#include <math.h>

struct _GtkWidgetActorPrivate {
  gpointer dummy;
};

G_DEFINE_TYPE (GtkWidgetActor, _gtk_widget_actor, GTK_TYPE_CSS_BOX)

static void
gtk_widget_actor_real_queue_redraw (GtkActor                *actor,
                                    const cairo_rectangle_t *box)
{
  GtkWidget *widget;
  int x, y, w, h;

  if (_gtk_actor_get_parent (actor))
    {
      GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->queue_redraw (actor, box);
      return;
    }

  widget = _gtk_actor_get_widget (actor);
  x = floor (box->x);
  y = floor (box->y);
  w = ceil (box->x + box->width) - x;
  h = ceil (box->y + box->height) - y;

  if (!gtk_widget_get_has_window (widget))
    {
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      
      x += allocation.x;
      y += allocation.y;
    }

  gtk_widget_queue_draw_area (widget, x, y, w, h);
}

static void
gtk_widget_actor_real_style_updated (GtkCssActor      *actor,
                                     const GtkBitmask *changed)
{
  _gtk_widget_emit_style_updated (_gtk_actor_get_widget (GTK_ACTOR (actor)));

  GTK_CSS_ACTOR_CLASS (_gtk_widget_actor_parent_class)->style_updated (actor, changed);
}

static void
_gtk_widget_actor_class_init (GtkWidgetActorClass *klass)
{
  GtkActorClass *actor_class = GTK_ACTOR_CLASS (klass);
  GtkCssActorClass *css_actor_class = GTK_CSS_ACTOR_CLASS (klass);
#if 0
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  actor_class->parent_set = gtk_widget_actor_real_parent_set;
  actor_class->show = gtk_css_box_real_show;
  actor_class->hide = gtk_css_box_real_hide;
  actor_class->map = gtk_css_box_real_map;
  actor_class->unmap = gtk_css_box_real_unmap;
  actor_class->draw = gtk_css_box_real_draw;
  actor_class->parent_set = gtk_css_box_real_parent_set;
  actor_class->get_preferred_size = gtk_css_box_real_get_preferred_size;
  actor_class->allocate = gtk_css_box_real_allocate;
#endif
  actor_class->queue_redraw = gtk_widget_actor_real_queue_redraw;

  css_actor_class->style_updated = gtk_widget_actor_real_style_updated;

  g_type_class_add_private (klass, sizeof (GtkWidgetActorPrivate));
}

static void
_gtk_widget_actor_init (GtkWidgetActor *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GTK_TYPE_WIDGET_ACTOR,
                                            GtkWidgetActorPrivate);
}

void
_gtk_widget_actor_show (GtkActor *actor)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->show (actor);
}

void
_gtk_widget_actor_hide (GtkActor *actor)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->hide (actor);
}

void
_gtk_widget_actor_map (GtkActor *actor)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->map (actor);
}

void
_gtk_widget_actor_unmap (GtkActor *actor)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->unmap (actor);
}

void
_gtk_widget_actor_realize (GtkActor *actor)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->realize (actor);
}

void
_gtk_widget_actor_unrealize (GtkActor *actor)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->unrealize (actor);
}

GtkSizeRequestMode
_gtk_widget_actor_get_request_mode (GtkActor *actor)
{
  g_return_val_if_fail (GTK_IS_WIDGET_ACTOR (actor), GTK_SIZE_REQUEST_CONSTANT_SIZE);

  return GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->get_request_mode (actor);
}

void
_gtk_widget_actor_get_preferred_size (GtkActor       *actor,
                                      GtkOrientation  orientation,
                                      gfloat          for_size,
                                      gfloat         *min_size_p,
                                      gfloat         *natural_size_p)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->get_preferred_size (actor,
                                                                        orientation,
                                                                        for_size,
                                                                        min_size_p,
                                                                        natural_size_p);
}

void
_gtk_widget_actor_allocate (GtkActor *actor,
                            double    x,
                            double    y,
                            double    width,
                            double    height)
{
  cairo_matrix_t position;

  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));

  cairo_matrix_init_translate (&position, x, y);
  GTK_ACTOR_CLASS (_gtk_widget_actor_parent_class)->allocate (actor,
                                                              &position,
                                                              width, height);
}

void
_gtk_widget_actor_screen_changed (GtkActor  *actor,
                                  GdkScreen *new_screen,
                                  GdkScreen *old_screen)
{
  g_return_if_fail (GTK_IS_WIDGET_ACTOR (actor));
  g_return_if_fail (new_screen == NULL || GDK_IS_SCREEN (new_screen));
  g_return_if_fail (old_screen == NULL || GDK_IS_SCREEN (old_screen));

  if (new_screen == NULL)
    new_screen = gdk_screen_get_default ();
  if (old_screen == NULL)
    old_screen = gdk_screen_get_default ();

  if (new_screen == old_screen)
    return;

  GTK_ACTOR_GET_CLASS (actor)->screen_changed (actor, new_screen, old_screen);

  for (actor = _gtk_actor_get_first_child (actor);
       actor != NULL;
       actor = _gtk_actor_get_next_sibling (actor))
    {
      if (GTK_IS_WIDGET_ACTOR (actor))
        continue;

      _gtk_widget_actor_screen_changed (actor, new_screen, old_screen);
    }
}
