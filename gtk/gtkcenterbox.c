/*
 * Copyright (c) 2017 Timm Bäder <mail@baedert.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Timm Bäder <mail@baedert.org>
 */

/**
 * SECTION:gtkcenterbox
 * @Short_description: A centering container
 * @Title: GtkCenterBox
 * @See_also: #GtkBox
 *
 * The GtkCenterBox widget arranges three children in a horizontal
 * or vertical arrangement, keeping the middle child centered as well
 * as possible.
 *
 * To add children to GtkCenterBox, use gtk_center_box_set_start_widget(),
 * gtk_center_box_set_center_widget() and gtk_center_box_set_end_widget().
 *
 * The sizing and positioning of children can be influenced with the
 * align and expand properties of the children.
 *
 * # GtkCenterBox as GtkBuildable
 *
 * The GtkCenterBox implementation of the #GtkBuildable interface supports
 * placing children in the 3 positions by specifying “start”, “center” or
 * “end” as the “type” attribute of a <child> element.
 *
 * # CSS nodes
 *
 * GtkCenterBox uses a single CSS node with the name “box”,
 *
 * The first child of the #GtkCenterBox will be allocated depending on the
 * text direction, i.e. in left-to-right layouts it will be allocated on the
 * left and in right-to-left layouts on the right.
 *
 * In vertical orientation, the nodes of the children are arranged from top to
 * bottom.
 */

#include "config.h"
#include "gtkcenterbox.h"
#include "gtkcenterlayout.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkbuildable.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkCenterBox
{
  GtkWidget parent_instance;

  GtkWidget *start_widget;
  GtkWidget *center_widget;
  GtkWidget *end_widget;
};

struct _GtkCenterBoxClass
{
  GtkWidgetClass parent_class;
};


enum {
  PROP_0,
  PROP_BASELINE_POSITION,
  PROP_ORIENTATION
};

static GtkBuildableIface *parent_buildable_iface;

static void gtk_center_box_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkCenterBox, gtk_center_box, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_center_box_buildable_init))

static void
gtk_center_box_buildable_add_child (GtkBuildable  *buildable,
                                    GtkBuilder    *builder,
                                    GObject       *child,
                                    const gchar   *type)
{
  if (g_strcmp0 (type, "start") == 0)
    gtk_center_box_set_start_widget (GTK_CENTER_BOX (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "center") == 0)
    gtk_center_box_set_center_widget (GTK_CENTER_BOX (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "end") == 0)
    gtk_center_box_set_end_widget (GTK_CENTER_BOX (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_center_box_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_center_box_buildable_add_child;
}

static void
gtk_center_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkCenterBox *self = GTK_CENTER_BOX (object);
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (object));

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      gtk_center_box_set_baseline_position (self, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        GtkOrientation current = gtk_center_layout_get_orientation (GTK_CENTER_LAYOUT (layout));
        if (current != orientation)
          {
            gtk_center_layout_set_orientation (GTK_CENTER_LAYOUT (layout), orientation);
            _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify (object, "orientation");
          }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_center_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (object));

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, gtk_center_layout_get_baseline_position (GTK_CENTER_LAYOUT (layout)));
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, gtk_center_layout_get_orientation (GTK_CENTER_LAYOUT (layout)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_center_box_dispose (GObject *object)
{
  GtkCenterBox *self = GTK_CENTER_BOX (object);

  g_clear_pointer (&self->start_widget, gtk_widget_unparent);
  g_clear_pointer (&self->center_widget, gtk_widget_unparent);
  g_clear_pointer (&self->end_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_center_box_parent_class)->dispose (object);
}

static void
gtk_center_box_class_init (GtkCenterBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gtk_center_box_set_property;
  object_class->get_property = gtk_center_box_get_property;
  object_class->dispose = gtk_center_box_dispose;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  g_object_class_install_property (object_class, PROP_BASELINE_POSITION,
          g_param_spec_enum ("baseline-position",
                             P_("Baseline position"),
                             P_("The position of the baseline aligned widgets if extra space is available"),
                             GTK_TYPE_BASELINE_POSITION,
                             GTK_BASELINE_POSITION_CENTER,
                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILLER);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CENTER_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("box"));
}

static void
gtk_center_box_init (GtkCenterBox *self)
{
  self->start_widget = NULL;
  self->center_widget = NULL;
  self->end_widget = NULL;
}

/**
 * gtk_center_box_new:
 *
 * Creates a new #GtkCenterBox.
 *
 * Returns: the new #GtkCenterBox.
 */
GtkWidget *
gtk_center_box_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_CENTER_BOX, NULL));
}

/**
 * gtk_center_box_set_start_widget:
 * @self: a #GtkCenterBox
 * @child: (nullable): the new start widget, or %NULL
 *
 * Sets the start widget. To remove the existing start widget, pass %NULL.
 */
void
gtk_center_box_set_start_widget (GtkCenterBox *self,
                                 GtkWidget    *child)
{
  GtkLayoutManager *layout_manager;

  if (self->start_widget)
    gtk_widget_unparent (self->start_widget);

  self->start_widget = child;
  if (child)
    gtk_widget_insert_after (child, GTK_WIDGET (self), NULL);

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  gtk_center_layout_set_start_widget (GTK_CENTER_LAYOUT (layout_manager), child);
}

/**
 * gtk_center_box_set_center_widget:
 * @self: a #GtkCenterBox
 * @child: (nullable): the new center widget, or %NULL
 *
 * Sets the center widget. To remove the existing center widget, pas %NULL.
 */
void
gtk_center_box_set_center_widget (GtkCenterBox *self,
                                  GtkWidget    *child)
{
  GtkLayoutManager *layout_manager;

  if (self->center_widget)
    gtk_widget_unparent (self->center_widget);

  self->center_widget = child;
  if (child)
    gtk_widget_insert_after (child, GTK_WIDGET (self), self->start_widget);

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  gtk_center_layout_set_center_widget (GTK_CENTER_LAYOUT (layout_manager), child);
}

/**
 * gtk_center_box_set_end_widget:
 * @self: a #GtkCenterBox
 * @child: (nullable): the new end widget, or %NULL
 *
 * Sets the end widget. To remove the existing end widget, pass %NULL.
 */
void
gtk_center_box_set_end_widget (GtkCenterBox *self,
                               GtkWidget    *child)
{
  GtkLayoutManager *layout_manager;

  if (self->end_widget)
    gtk_widget_unparent (self->end_widget);

  self->end_widget = child;
  if (child)
    gtk_widget_insert_before (child, GTK_WIDGET (self), NULL);

  layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  gtk_center_layout_set_end_widget (GTK_CENTER_LAYOUT (layout_manager), child);
}

/**
 * gtk_center_box_get_start_widget:
 * @self: a #GtkCenterBox
 *
 * Gets the start widget, or %NULL if there is none.
 *
 * Returns: (transfer none) (nullable): the start widget.
 */
GtkWidget *
gtk_center_box_get_start_widget (GtkCenterBox *self)
{
  return self->start_widget;
}

/**
 * gtk_center_box_get_center_widget:
 * @self: a #GtkCenterBox
 *
 * Gets the center widget, or %NULL if there is none.
 *
 * Returns: (transfer none) (nullable): the center widget.
 */
GtkWidget *
gtk_center_box_get_center_widget (GtkCenterBox *self)
{
  return self->center_widget;
}

/**
 * gtk_center_box_get_end_widget:
 * @self: a #GtkCenterBox
 *
 * Gets the end widget, or %NULL if there is none.
 *
 * Returns: (transfer none) (nullable): the end widget.
 */
GtkWidget *
gtk_center_box_get_end_widget (GtkCenterBox *self)
{
  return self->end_widget;
}

/**
 * gtk_center_box_set_baseline_position:
 * @self: a #GtkCenterBox
 * @position: a #GtkBaselinePosition
 *
 * Sets the baseline position of a center box.
 *
 * This affects only horizontal boxes with at least one baseline
 * aligned child. If there is more vertical space available than
 * requested, and the baseline is not allocated by the parent then
 * @position is used to allocate the baseline wrt. the extra space
 * available.
 */
void
gtk_center_box_set_baseline_position (GtkCenterBox        *self,
                                      GtkBaselinePosition  position)
{
  GtkBaselinePosition current_position;
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_CENTER_BOX (self));

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  current_position = gtk_center_layout_get_baseline_position (GTK_CENTER_LAYOUT (layout));
  if (current_position != position)
    {
      gtk_center_layout_set_baseline_position (GTK_CENTER_LAYOUT (layout), position);
      g_object_notify (G_OBJECT (self), "baseline-position");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_center_box_get_baseline_position:
 * @self: a #GtkCenterBox
 *
 * Gets the value set by gtk_center_box_set_baseline_position().
 *
 * Returns: the baseline position
 */
GtkBaselinePosition
gtk_center_box_get_baseline_position (GtkCenterBox *self)
{
  GtkLayoutManager *layout;

  g_return_val_if_fail (GTK_IS_CENTER_BOX (self), GTK_BASELINE_POSITION_CENTER);

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));

  return gtk_center_layout_get_baseline_position (GTK_CENTER_LAYOUT (layout));
}

