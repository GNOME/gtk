/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2026  Benjamin Otte
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
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkpopout.h"

#include "gtkrootprivate.h"

/**
 * GtkPopout:
 *
 * Allows the child widget to pop out of the widget tree and appear in front of
 * all other widgets.
 *
 * Since: 4.26
 */

struct _GtkPopout
{
  GtkWidget parent_instance;

  GtkWidget *child;

  gboolean popped_out;
};

struct _GtkPopoutClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_POPPED_OUT,
  LAST_PROP,
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GtkPopout, gtk_popout, GTK_TYPE_WIDGET)

static void
gtk_popout_init (GtkPopout *self)
{
}

static void
gtk_popout_dispose (GObject *object)
{
  GtkPopout *self = GTK_POPOUT (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popout_parent_class)->dispose (object);
}

static void
gtk_popout_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkPopout *self = GTK_POPOUT (object);

  switch (property_id)
    {
    case PROP_CHILD:
      gtk_popout_set_child (self, g_value_get_object (value));
      break;

    case PROP_POPPED_OUT:
      gtk_popout_set_popped_out (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_popout_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtkPopout *self = GTK_POPOUT (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, gtk_popout_get_child (self));
      break;

    case PROP_POPPED_OUT:
      g_value_set_boolean (value, gtk_popout_get_popped_out (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_popout_unpop (GtkPopout *self)
{
  g_assert (self->popped_out);

  self->popped_out = FALSE; 
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

static void
gtk_popout_root (GtkWidget *widget)
{
  GtkPopout *self = GTK_POPOUT (widget);

  GTK_WIDGET_CLASS (gtk_popout_parent_class)->root (widget);

  if (self->popped_out)
    {
      if (!gtk_root_set_popout (gtk_widget_get_root (widget), self))
        gtk_popout_unpop (self);
    }
}

static void
gtk_popout_unroot (GtkWidget *widget)
{
  GtkPopout *self = GTK_POPOUT (widget);

  if (self->popped_out)
    gtk_root_set_popout (gtk_widget_get_root (widget), NULL);

  GTK_WIDGET_CLASS (gtk_popout_parent_class)->unroot (widget);
}

static GtkSizeRequestMode
gtk_popout_get_request_mode (GtkWidget *widget)
{
  GtkPopout *self = GTK_POPOUT (widget);

  if (self->child)
    return gtk_widget_get_request_mode (self->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_popout_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GtkPopout *self = GTK_POPOUT (widget);

  if (self->child)
    gtk_widget_measure (self->child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_popout_size_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  GtkPopout *self = GTK_POPOUT (widget);
  GskTransform *transform;

  if (self->child == NULL)
    return;

  if (self->popped_out)
    {
      GtkWidget *root = GTK_WIDGET (gtk_widget_get_root (widget));
      graphene_matrix_t matrix;

      if (gtk_widget_compute_transform (root, widget, &matrix))
        {
          transform = gsk_transform_matrix (NULL, &matrix);
        }
      else
        {
          g_warning ("FIXME: invalid transform, what now?");
          transform = NULL;
        }
      gtk_widget_allocate (self->child,
                           gtk_widget_get_width (root),
                           gtk_widget_get_height (root),
                           baseline,
                           transform);
    }
  else
    {
      gtk_widget_size_allocate (self->child,
                                &(GtkAllocation) { 0, 0, width, height },
                                baseline);
    }
}

static void
gtk_popout_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  GtkPopout *self = GTK_POPOUT (widget);

  if (self->child && !self->popped_out)
    gtk_widget_snapshot_child (widget, self->child, snapshot);
}

static void
gtk_popout_class_init (GtkPopoutClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_popout_dispose;
  object_class->set_property = gtk_popout_set_property;
  object_class->get_property = gtk_popout_get_property;

  widget_class->root = gtk_popout_root;
  widget_class->unroot = gtk_popout_unroot;
  widget_class->get_request_mode = gtk_popout_get_request_mode;
  widget_class->measure = gtk_popout_measure;
  widget_class->size_allocate = gtk_popout_size_allocate;
  widget_class->snapshot = gtk_popout_snapshot;

  /**
   * GtkPopout:child:
   *
   * The child widget.
   *
   * Since: 4.26
   */
  properties[PROP_CHILD] = g_param_spec_object ("child", NULL, NULL,
                                                GTK_TYPE_WIDGET,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPopout:popped-out:
   *
   * Whether the child is currently popped out
   *
   * Since: 4.26
   */
  properties[PROP_POPPED_OUT] = g_param_spec_boolean ("popped-out", NULL, NULL,
                                                      FALSE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "popout");
}

/**
 * gtk_popout_new:
 * @child: (nullable): the child widget
 *
 * Creates a new GtkPopout widget.
 *
 * Returns: the new widget
 *
 * Since: 4.26
 */
GtkWidget *
gtk_popout_new (GtkWidget *child)
{
  return g_object_new (GTK_TYPE_POPOUT,
                       "child", child,
                       NULL);
}

/**
 * gtk_popout_set_child:
 * @self: a `GtkPopout`
 * @child: (nullable): the child widget
 *
 * Sets the child of @self.
 *
 * Since: 4.26
 */
void
gtk_popout_set_child (GtkPopout *self,
                      GtkWidget *child)
{
  g_return_if_fail (GTK_IS_POPOUT (self));
  g_return_if_fail (child == NULL || self->child == child || (GTK_IS_WIDGET (child) &&gtk_widget_get_parent (child) == NULL));

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_popout_get_child:
 * @self: a `GtkPopout`
 *
 * Gets the child of @self.
 *
 * Returns: (nullable) (transfer none): the child widget
 *
 * Since: 4.26
 */
GtkWidget *
gtk_popout_get_child (GtkPopout *self)
{
  g_return_val_if_fail (GTK_IS_POPOUT (self), NULL);

  return self->child;
}

/**
 * gtk_popout_set_popped_out:
 * @self: a `GtkPopout`
 * @popped_out: whether to pop out the child
 *
 * Sets whether this GtkPopout widget will pop out its child.
 *
 * Since: 4.26
 */
void
gtk_popout_set_popped_out (GtkPopout *self,
                           gboolean   popped_out)
{
  g_return_if_fail (GTK_IS_POPOUT (self));

  if (self->popped_out == popped_out)
    return;

  if (popped_out)
    {
      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

      if (root != NULL && !gtk_root_set_popout (root, self))
        return;
    }

  self->popped_out = popped_out;

  if (self->child)
    gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPPED_OUT]);
}

/**
 * gtk_popout_get_popped_out:
 * @self: a `GtkPopout`
 *
 * Returns whether the child is currently popped out.
 *
 * See [method@Gtk.Popout.set_popped_out].
 *
 * Returns: `TRUE` if the child is popped out
 *
 * Since: 4.26
 */
gboolean
gtk_popout_get_popped_out (GtkPopout *self)
{
  g_return_val_if_fail (GTK_IS_POPOUT (self), FALSE);

  return self->popped_out;
}
