/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtkwidgetpaintableprivate.h"

#include "gtkintl.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkwidgetpaintable
 * @Short_description: Drawing a widget elsewhere
 * @Title: GtkWidgetPaintable
 * @see_also: #GtkWidget, #GdkPaintable
 *
 * #GtkWidgetPaintable is an implementation of the #GdkPaintable interface 
 * that allows displaying the contents of a #GtkWidget.
 *
 * #GtkWidgetPaintable will also take care of the widget not being in a
 * state where it can be drawn (like when it isn't shown) and just draw
 * nothing or where it does not have a size (like when it is hidden) and
 * report no size in that case.
 *
 * Of course, #GtkWidgetPaintable allows you to monitor widgets for size
 * changes by emitting the GdkPaintable::invalidate-size signal whenever
 * the size of the widget changes as well as for visual changes by
 * emitting the GdkPaintable::invalidate-contents signal whenever the
 * widget changes.
 *
 * You can of course use a #GtkWidgetPaintable everywhere a
 * #GdkPaintable is allowed, including using it on a #GtkImage (or one
 * of its parents) that it was set on itself via
 * gtk_image_set_from_paintable(). The paintable will take care of recursion
 * when this happens.  
 * If you do this however, make sure to set the GtkImage:can-shrink property
 * to %TRUE or you might end up with an infinitely growing image.
 */
struct _GtkWidgetPaintable
{
  GObject parent_instance;

  GtkWidget *widget;
  guint loop_tracker;

  guint size_invalid : 1;
  guint contents_invalid : 1;
};

struct _GtkWidgetPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_WIDGET,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_widget_paintable_paintable_snapshot (GdkPaintable *paintable,
                                         GdkSnapshot  *snapshot,
                                         double        width,
                                         double        height)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);
  graphene_matrix_t transform;

  self->contents_invalid = FALSE;

  if (self->widget == NULL ||
      !_gtk_widget_is_drawable (self->widget) ||
      _gtk_widget_get_alloc_needed (self->widget))
    return;

  if (self->loop_tracker >= 5)
    return;
  self->loop_tracker++;

  /* need to clip because widgets may draw out of bounds */
  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(0, 0, width, height),
                          "WidgetPaintableClip<%g,%g>",
                          width, height);
  graphene_matrix_init_scale (&transform,
                              width / gtk_widget_get_allocated_width (self->widget),
                              height / gtk_widget_get_allocated_height (self->widget),
                              1.0);
  gtk_snapshot_push_transform (snapshot,
                               &transform,
                               "WidgetPaintableScale<%g,%g>",
                               width / gtk_widget_get_allocated_width (self->widget),
                               height / gtk_widget_get_allocated_height (self->widget));

  gtk_widget_snapshot (self->widget, snapshot);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);

  self->loop_tracker--;
}

static GdkPaintable *
gtk_widget_paintable_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);
  GtkSnapshot *snapshot;
  int width, height;

  self->contents_invalid = FALSE;
  self->size_invalid = FALSE;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_width (paintable);
  if (width == 0 || height == 0)
    return gdk_paintable_new_empty (width, height);

  snapshot = gtk_snapshot_new (FALSE, "WidgetPaintableCurrentImage");
  gdk_paintable_snapshot (GDK_PAINTABLE (self), 
                          snapshot,
                          width, height);
  return gtk_snapshot_free_to_paintable (snapshot, &(graphene_size_t) { width, height });
}

static int
gtk_widget_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);

  self->size_invalid = FALSE;

  if (self->widget == NULL)
    return 0;

  return gtk_widget_get_allocated_width (self->widget);
}

static int
gtk_widget_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);

  self->size_invalid = FALSE;

  if (self->widget == NULL)
    return 0;

  return gtk_widget_get_allocated_height (self->widget);
}

static void
gtk_widget_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_widget_paintable_paintable_snapshot;
  iface->get_current_image = gtk_widget_paintable_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_widget_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_widget_paintable_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (GtkWidgetPaintable, gtk_widget_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_widget_paintable_paintable_init))

static void
gtk_widget_paintable_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      gtk_widget_paintable_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_paintable_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, self->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_paintable_dispose (GObject *object)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  gtk_widget_paintable_set_widget (self, NULL);

  G_OBJECT_CLASS (gtk_widget_paintable_parent_class)->dispose (object);
}

static void
gtk_widget_paintable_class_init (GtkWidgetPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_widget_paintable_get_property;
  gobject_class->set_property = gtk_widget_paintable_set_property;
  gobject_class->dispose = gtk_widget_paintable_dispose;

  /**
   * GtkWidgetPaintable:widget
   *
   * The observed widget or %NULL if none.
   */
  properties[PROP_WIDGET] =
    g_param_spec_object ("widget",
                         P_("Widget"),
                         P_("Observed widget"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_widget_paintable_init (GtkWidgetPaintable *self)
{
}

/**
 * gtk_widget_paintable_new:
 * @widget: (allow-none) (transfer none): a #GtkWidget or %NULL
 *
 * Creates a new widget paintable observing the given widget.
 *
 * Returns: (transfer full) (type GtkWidgetPaintable): a new #GtkWidgetPaintable
 **/
GdkPaintable *
gtk_widget_paintable_new (GtkWidget *widget)
{
  g_return_val_if_fail (widget == NULL || GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_WIDGET_PAINTABLE,
                       "widget", widget,
                       NULL);
}

/**
 * gtk_widget_paintable_get_widget:
 * @self: a #GtkWidgetPaintable
 *
 * Returns the widget that is observed or %NULL
 * if none.
 *
 * Returns: (transfer none) (nullable): the observed widget.
 **/
GtkWidget *
gtk_widget_paintable_get_widget (GtkWidgetPaintable *self)
{
  g_return_val_if_fail (GTK_IS_WIDGET_PAINTABLE (self), NULL);

  return self->widget;
}

/**
 * gtk_widget_paintable_set_widget:
 * @self: a #GtkWidgetPaintable
 * @widget: (allow-none): the widget to observe or %NULL
 *
 * Sets the widget that should be observed.
 **/
void
gtk_widget_paintable_set_widget (GtkWidgetPaintable *self,
                                 GtkWidget          *widget)
{

  g_return_if_fail (GTK_IS_WIDGET_PAINTABLE (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (self->widget == widget)
    return;

  if (self->widget)
    {
      self->widget->priv->paintables = g_slist_remove (self->widget->priv->paintables,
                                                       self);
    }

  /* We do not ref the widget to not cause ref cycles when a widget
   * is told to observe itself or one of its parent.
   */
  self->widget = widget;

  if (widget)
    {
      widget->priv->paintables = g_slist_prepend (widget->priv->paintables,
                                                  self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDGET]);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

void
gtk_widget_paintable_invalidate_size (GtkWidgetPaintable *self)
{
  if (self->size_invalid)
    return;

  self->size_invalid = TRUE;
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
}

void
gtk_widget_paintable_invalidate_contents (GtkWidgetPaintable *self)
{
  if (self->contents_invalid)
    return;

  self->contents_invalid = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}
