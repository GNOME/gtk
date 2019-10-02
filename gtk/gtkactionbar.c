/*
 * Copyright (c) 2013 - 2014 Red Hat, Inc.
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
 */

#include "config.h"

#include "gtkactionbar.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkbuildable.h"
#include "gtktypebuiltins.h"
#include "gtkbox.h"
#include "gtkrevealer.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkprivate.h"
#include "gtkcenterbox.h"

#include <string.h>

/**
 * SECTION:gtkactionbar
 * @Short_description: A full width bar for presenting contextual actions
 * @Title: GtkActionBar
 * @See_also: #GtkBox
 *
 * GtkActionBar is designed to present contextual actions. It is
 * expected to be displayed below the content and expand horizontally
 * to fill the area.
 *
 * It allows placing children at the start or the end. In addition, it
 * contains an internal centered box which is centered with respect to
 * the full width of the box, even if the children at either side take
 * up different amounts of space.
 *
 * # CSS nodes
 *
 * GtkActionBar has a single CSS node with name actionbar.
 */

typedef struct _GtkActionBarPrivate       GtkActionBarPrivate;
typedef struct _GtkActionBarClass         GtkActionBarClass;

struct _GtkActionBar
{
  GtkContainer container;
};

struct _GtkActionBarClass
{
  GtkContainerClass parent_class;
};

struct _GtkActionBarPrivate
{
  GtkWidget *center_box;
  GtkWidget *start_box;
  GtkWidget *end_box;
  GtkWidget *revealer;
};

enum {
  PROP_0,
  PROP_REVEALED,
  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { NULL, };

static void gtk_action_bar_buildable_interface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkActionBar, gtk_action_bar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkActionBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_action_bar_buildable_interface_init))

static void
gtk_action_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (container));

  /* Default for pack-type is start */
  gtk_container_add (GTK_CONTAINER (priv->start_box), child);
}

static void
gtk_action_bar_remove (GtkContainer *container,
                       GtkWidget    *child)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (container));

  if (gtk_widget_get_parent (child) == priv->start_box)
    gtk_container_remove (GTK_CONTAINER (priv->start_box), child);
  else if (gtk_widget_get_parent (child) == priv->end_box)
    gtk_container_remove (GTK_CONTAINER (priv->end_box), child);
  else if (child == gtk_center_box_get_center_widget (GTK_CENTER_BOX (priv->center_box)))
    gtk_center_box_set_center_widget (GTK_CENTER_BOX (priv->center_box), NULL);
  else
    g_warning ("Can't remove non-child %s %p from GtkActionBar %p",
               G_OBJECT_TYPE_NAME (child), child, container);
}

static void
gtk_action_bar_forall (GtkContainer *container,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (container));

  if (priv->start_box != NULL)
    gtk_container_forall (GTK_CONTAINER (priv->start_box), callback, callback_data);

  if (gtk_center_box_get_center_widget (GTK_CENTER_BOX (priv->center_box)) != NULL)
    (*callback) (gtk_center_box_get_center_widget (GTK_CENTER_BOX (priv->center_box)), callback_data);

  if (priv->end_box != NULL)
    gtk_container_forall (GTK_CONTAINER (priv->end_box), callback, callback_data);
}

static void
gtk_action_bar_finalize (GObject *object)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (object));

  gtk_widget_unparent (priv->revealer);
  G_OBJECT_CLASS (gtk_action_bar_parent_class)->finalize (object);
}

static GType
gtk_action_bar_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_action_bar_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (widget));

  gtk_widget_size_allocate (priv->revealer,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            },
                            baseline);
}

static void
gtk_action_bar_measure_ (GtkWidget *widget,
                        GtkOrientation orientation,
                        int        for_size,
                        int       *minimum,
                        int       *natural,
                        int       *minimum_baseline,
                        int       *natural_baseline)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (GTK_ACTION_BAR (widget));

  gtk_widget_measure (priv->revealer,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_action_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkActionBar *action_bar = GTK_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_REVEALED:
      gtk_action_bar_set_revealed (action_bar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkActionBar *action_bar = GTK_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_REVEALED:
      g_value_set_boolean (value, gtk_action_bar_get_revealed (action_bar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_bar_dispose (GObject *object)
{
  GtkActionBar *self = GTK_ACTION_BAR (object);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (self);

  gtk_center_box_set_start_widget (GTK_CENTER_BOX (priv->center_box), NULL);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (priv->center_box), NULL);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (priv->center_box), NULL);

  priv->start_box = NULL;
  priv->end_box = NULL;

  G_OBJECT_CLASS (gtk_action_bar_parent_class)->dispose (object);
}

static void
gtk_action_bar_class_init (GtkActionBarClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  container_class = GTK_CONTAINER_CLASS (klass);

  object_class->set_property = gtk_action_bar_set_property;
  object_class->get_property = gtk_action_bar_get_property;
  object_class->dispose = gtk_action_bar_dispose;
  object_class->finalize = gtk_action_bar_finalize;

  widget_class->size_allocate = gtk_action_bar_size_allocate;
  widget_class->measure = gtk_action_bar_measure_;

  container_class->add = gtk_action_bar_add;
  container_class->remove = gtk_action_bar_remove;
  container_class->forall = gtk_action_bar_forall;
  container_class->child_type = gtk_action_bar_child_type;

  props[PROP_REVEALED] =
    g_param_spec_boolean ("revealed",
                          P_("Reveal"),
                          P_("Controls whether the action bar shows its contents or not"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_PANEL);
  gtk_widget_class_set_css_name (widget_class, I_("actionbar"));
}

static void
gtk_action_bar_init (GtkActionBar *action_bar)
{
  GtkWidget *widget = GTK_WIDGET (action_bar);
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  priv->revealer = gtk_revealer_new ();
  gtk_widget_set_parent (priv->revealer, widget);

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);
  gtk_revealer_set_transition_type (GTK_REVEALER (priv->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);

  priv->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  priv->center_box = gtk_center_box_new ();
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (priv->center_box), priv->start_box);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (priv->center_box), priv->end_box);

  gtk_container_add (GTK_CONTAINER (priv->revealer), priv->center_box);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_action_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  GtkActionBar *action_bar = GTK_ACTION_BAR (buildable);

  if (g_strcmp0 (type, "center") == 0)
    gtk_action_bar_set_center_widget (action_bar, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "start") == 0)
    gtk_action_bar_pack_start (action_bar, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "end") == 0)
    gtk_action_bar_pack_end (action_bar, GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_action_bar_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_action_bar_buildable_add_child;
}

/**
 * gtk_action_bar_pack_start:
 * @action_bar: A #GtkActionBar
 * @child: the #GtkWidget to be added to @action_bar
 *
 * Adds @child to @action_bar, packed with reference to the
 * start of the @action_bar.
 */
void
gtk_action_bar_pack_start (GtkActionBar *action_bar,
                           GtkWidget    *child)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  gtk_container_add (GTK_CONTAINER (priv->start_box), child);
}

/**
 * gtk_action_bar_pack_end:
 * @action_bar: A #GtkActionBar
 * @child: the #GtkWidget to be added to @action_bar
 *
 * Adds @child to @action_bar, packed with reference to the
 * end of the @action_bar.
 */
void
gtk_action_bar_pack_end (GtkActionBar *action_bar,
                         GtkWidget    *child)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  gtk_box_insert_child_after (GTK_BOX (priv->end_box), child, NULL);
}

/**
 * gtk_action_bar_set_center_widget:
 * @action_bar: a #GtkActionBar
 * @center_widget: (allow-none): a widget to use for the center
 *
 * Sets the center widget for the #GtkActionBar.
 */
void
gtk_action_bar_set_center_widget (GtkActionBar *action_bar,
                                  GtkWidget    *center_widget)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  gtk_center_box_set_center_widget (GTK_CENTER_BOX (priv->center_box), center_widget);
}

/**
 * gtk_action_bar_get_center_widget:
 * @action_bar: a #GtkActionBar
 *
 * Retrieves the center bar widget of the bar.
 *
 * Returns: (transfer none) (nullable): the center #GtkWidget or %NULL.
 */
GtkWidget *
gtk_action_bar_get_center_widget (GtkActionBar *action_bar)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  g_return_val_if_fail (GTK_IS_ACTION_BAR (action_bar), NULL);

  return gtk_center_box_get_center_widget (GTK_CENTER_BOX (priv->center_box));
}

/**
 * gtk_action_bar_new:
 *
 * Creates a new #GtkActionBar widget.
 *
 * Returns: a new #GtkActionBar
 */
GtkWidget *
gtk_action_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_ACTION_BAR, NULL));
}

/**
 * gtk_action_bar_set_revealed:
 * @action_bar: a #GtkActionBar
 * @revealed: The new value of the property
 *
 * Sets the #GtkActionBar:revealed property to @revealed. Changing this will
 * make @action_bar reveal (%TRUE) or conceal (%FALSE) itself via a sliding
 * transition.
 *
 * Note: this does not show or hide @action_bar in the #GtkWidget:visible sense,
 * so revealing has no effect if #GtkWidget:visible is %FALSE.
 */
void
gtk_action_bar_set_revealed (GtkActionBar *action_bar,
                             gboolean      revealed)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  g_return_if_fail (GTK_IS_ACTION_BAR (action_bar));

  revealed = !!revealed;
  if (revealed != gtk_revealer_get_reveal_child (GTK_REVEALER (priv->revealer)))
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), revealed);
      g_object_notify_by_pspec (G_OBJECT (action_bar), props[PROP_REVEALED]);
    }
}

/**
 * gtk_action_bar_get_revealed:
 * @action_bar: a #GtkActionBar
 *
 * Gets the value of the #GtkActionBar:revealed property.
 *
 * Returns: the current value of the #GtkActionBar:revealed property.
 */
gboolean
gtk_action_bar_get_revealed (GtkActionBar *action_bar)
{
  GtkActionBarPrivate *priv = gtk_action_bar_get_instance_private (action_bar);

  g_return_val_if_fail (GTK_IS_ACTION_BAR (action_bar), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (priv->revealer));
}
