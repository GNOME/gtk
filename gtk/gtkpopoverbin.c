/* gtkpopoverbin.c: A single-child container with a popover
 *
 * SPDX-FileCopyrightText: 2025  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkpopoverbin.h"

#include "gtkbinlayout.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

/**
 * GtkPopoverBin:
 *
 * A single child container with a popover.
 *
 * You should use `GtkPopoverBin` whenever you need to present a [class@Gtk.Popover]
 * to the user.
 */

struct _GtkPopoverBin
{
  GtkWidget parent_instance;

  GtkWidget *child;
  GtkWidget *popover;

  GMenuModel *menu_model;
};

enum
{
  PROP_CHILD = 1,
  PROP_POPOVER,
  PROP_MENU_MODEL,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

static GtkBuildableIface *parent_buildable_iface;

static void gtk_popover_bin_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkPopoverBin, gtk_popover_bin, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                      gtk_popover_bin_buildable_iface_init))

static void
gtk_popover_bin_buildable_add_child (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const char   *type)
{
  if (GTK_IS_WIDGET (child))
    {
      if (GTK_IS_POPOVER (child))
        {
          gtk_buildable_child_deprecation_warning (buildable, builder, NULL, "popover");
          gtk_popover_bin_set_popover (GTK_POPOVER_BIN (buildable), GTK_WIDGET (child));
        }
      else
        {
          gtk_buildable_child_deprecation_warning (buildable, builder, NULL, "child");
          gtk_popover_bin_set_child (GTK_POPOVER_BIN (buildable), GTK_WIDGET (child));
        }
    }
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
}

static void
gtk_popover_bin_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_popover_bin_buildable_add_child;
}

static void
on_popover_destroy (GtkPopoverBin *self)
{
  gtk_popover_bin_set_popover (self, NULL);
}

static void
gtk_popover_bin_dispose (GObject *gobject)
{
  GtkPopoverBin *self = GTK_POPOVER_BIN (gobject);

  if (self->popover != NULL)
    g_signal_handlers_disconnect_by_func (self->popover, on_popover_destroy, self);

  g_clear_pointer (&self->popover, gtk_widget_unparent);
  g_clear_pointer (&self->child, gtk_widget_unparent);

  g_clear_object (&self->menu_model);

  G_OBJECT_CLASS (gtk_popover_bin_parent_class)->dispose (gobject);
}

static void
gtk_popover_bin_set_property (GObject *gobject,
                              unsigned int prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  GtkPopoverBin *self = GTK_POPOVER_BIN (gobject);

  switch (prop_id)
    {
      case PROP_MENU_MODEL:
        gtk_popover_bin_set_menu_model (self, g_value_get_object (value));
        break;

      case PROP_POPOVER:
        gtk_popover_bin_set_popover (self, g_value_get_object (value));
        break;

      case PROP_CHILD:
        gtk_popover_bin_set_child (self, g_value_get_object (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_popover_bin_get_property (GObject *gobject,
                              unsigned int prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  GtkPopoverBin *self = GTK_POPOVER_BIN (gobject);

  switch (prop_id)
    {
      case PROP_MENU_MODEL:
        g_value_set_object (value, self->menu_model);
        break;

      case PROP_POPOVER:
        g_value_set_object (value, self->popover);
        break;

      case PROP_CHILD:
        g_value_set_object (value, self->child);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_popover_bin_compute_expand (GtkWidget *widget,
                                gboolean *hexpand_p,
                                gboolean *vexpand_p)
{
  GtkPopoverBin *self = GTK_POPOVER_BIN (widget);
  gboolean hexpand = FALSE, vexpand = FALSE;

  if (self->child != NULL)
    {
      hexpand = gtk_widget_compute_expand (self->child, GTK_ORIENTATION_HORIZONTAL);
      vexpand = gtk_widget_compute_expand (self->child, GTK_ORIENTATION_VERTICAL);
    }

  if (hexpand_p != NULL)
    *hexpand_p = hexpand;
  if (vexpand_p != NULL)
    *vexpand_p = vexpand;
}

static void
gtk_popover_bin_class_init (GtkPopoverBinClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = gtk_popover_bin_set_property;
  gobject_class->get_property = gtk_popover_bin_get_property;
  gobject_class->dispose = gtk_popover_bin_dispose;

  widget_class->focus = gtk_widget_focus_child;
  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->compute_expand = gtk_popover_bin_compute_expand;

  /**
   * GtkPopoverBin:menu-model:
   *
   * The `GMenuModel` from which the popup will be created.
   *
   * See [method@Gtk.PopoverBin.set_menu_model] for the interaction
   * with the [property@Gtk.PopoverBin:popover] property.
   *
   * Since: 4.22
   */
  obj_props[PROP_MENU_MODEL] =
      g_param_spec_object ("menu-model", NULL, NULL,
                           G_TYPE_MENU_MODEL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPopoverBin:popover:
   *
   * The `GtkPopover` that will be popped up when calling
   * [method@Gtk.PopoverBin.popup].
   *
   * Since: 4.22
   */
  obj_props[PROP_POPOVER] =
      g_param_spec_object ("popover", NULL, NULL,
                           GTK_TYPE_POPOVER,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPopoverBin:child:
   *
   * The child widget of the popover bin.
   *
   * Since: 4.22
   */
  obj_props[PROP_CHILD] =
      g_param_spec_object ("child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_popover_bin_init (GtkPopoverBin *self)
{
}

/**
 * gtk_popover_bin_new:
 *
 * Creates a new popover bin widget.
 *
 * Returns: (transfer floating): the newly created popover bin
 *
 * Since: 4.22
 */
GtkWidget *
gtk_popover_bin_new (void)
{
  return g_object_new (GTK_TYPE_POPOVER_BIN, NULL);
}

/**
 * gtk_popover_bin_set_child:
 * @self: a popover bin
 * @child: (nullable): the child of the popover bin
 *
 * Sets the child of the popover bin.
 *
 * Since: 4.22
 */
void
gtk_popover_bin_set_child (GtkPopoverBin *self,
                           GtkWidget     *child)
{
  g_return_if_fail (GTK_IS_POPOVER_BIN (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child != NULL)
    {
      gtk_widget_set_parent (child, GTK_WIDGET (self));
      self->child = child;
    }

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_CHILD]);
}

/**
 * gtk_popover_bin_get_child:
 * @self: a popover bin
 *
 * Retrieves the child widget of the popover bin.
 *
 * Returns: (transfer none) (nullable): the child widget
 *
 * Since: 4.22
 */
GtkWidget *
gtk_popover_bin_get_child (GtkPopoverBin *self)
{
  g_return_val_if_fail (GTK_IS_POPOVER_BIN (self), NULL);

  return self->child;
}

/**
 * gtk_popover_bin_set_menu_model:
 * @self: a popover bin
 * @model: (nullable): a menu model
 *
 * Sets the menu model used to create the popover that will be
 * presented when calling [method@Gtk.PopoverBin.popup].
 *
 * If @model is `NULL`, the popover will be unset.
 *
 * A [class@Gtk.Popover] will be created from the menu model with
 * [ctor@Gtk.PopoverMenu.new_from_model]. Actions will be connected
 * as documented for this function.
 *
 * If [property@Gtk.PopoverBin:popover] is already set, it will be
 * dissociated from the popover bin, and the property is set to `NULL`.
 *
 * See: [method@Gtk.PopoverBin.set_popover]
 *
 * Since: 4.22
 */
void
gtk_popover_bin_set_menu_model (GtkPopoverBin *self,
                                GMenuModel    *model)
{
  g_return_if_fail (GTK_IS_POPOVER_BIN (self));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  if (self->menu_model == model)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (model != NULL)
    g_object_ref (model);

  if (model != NULL)
    {
      GtkWidget *popover;

      popover = gtk_popover_menu_new_from_model (model);
      gtk_popover_bin_set_popover (self, popover);
    }
  else
    {
      gtk_popover_bin_set_popover (self, NULL);
    }

  self->menu_model = model;
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_MENU_MODEL]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_popover_bin_get_menu_model:
 * @self: a popover bin
 *
 * Retrieves the menu model set using [method@Gtk.PopoverBin.set_menu_model].
 *
 * Returns: (transfer none) (nullable): the menu model for the popover
 *
 * Since: 4.22
 */
GMenuModel *
gtk_popover_bin_get_menu_model (GtkPopoverBin *self)
{
  g_return_val_if_fail (GTK_IS_POPOVER_BIN (self), NULL);

  return self->menu_model;
}

/**
 * gtk_popover_bin_set_popover:
 * @self: a popover bin
 * @popover: (nullable) (type Gtk.Popover): a `GtkPopover`
 *
 * Sets the `GtkPopover` that will be presented when calling
 * [method@Gtk.PopoverBin.popup].
 *
 * If @popover is `NULL`, the popover will be unset.
 *
 * If [property@Gtk.PopoverBin:menu-model] is set before calling
 * this function, then the menu model property will be unset.
 *
 * See: [method@Gtk.PopoverBin.set_menu_model]
 *
 * Since: 4.22
 */
void
gtk_popover_bin_set_popover (GtkPopoverBin *self,
                             GtkWidget     *popover)
{
  g_return_if_fail (GTK_IS_POPOVER_BIN (self));
  g_return_if_fail (popover == NULL || GTK_IS_POPOVER (popover));

  g_object_freeze_notify (G_OBJECT (self));

  g_clear_object (&self->menu_model);

  if (self->popover != NULL)
    {
      gtk_widget_set_visible (self->popover, FALSE);

      g_signal_handlers_disconnect_by_func (self->popover,
                                            on_popover_destroy,
                                            self);

      gtk_widget_unparent (self->popover);
    }

  self->popover = popover;

  if (popover != NULL)
    {
      gtk_widget_set_parent (self->popover, GTK_WIDGET (self));
      g_signal_connect_swapped (self->popover, "destroy",
                                G_CALLBACK (on_popover_destroy),
                                self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_POPOVER]);
  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_MENU_MODEL]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_popover_bin_get_popover:
 * @self: a popover bin
 *
 * Retrieves the `GtkPopover` set using [method@Gtk.PopoverBin.set_popover].
 *
 * Returns: (transfer none) (nullable) (type Gtk.Popover): a popover widget
 *
 * Since: 4.22
 */
GtkWidget *
gtk_popover_bin_get_popover (GtkPopoverBin *self)
{
  g_return_val_if_fail (GTK_IS_POPOVER_BIN (self), NULL);

  return self->popover;
}

/**
 * gtk_popover_bin_popup:
 * @self: a popover bin
 *
 * Presents the popover to the user.
 *
 * Use [method@Gtk.PopoverBin.set_popover] or
 * [method@Gtk.PopoverBin.set_menu_model] to define the popover.
 *
 * See: [method@Gtk.PopoverBin.popdown]
 *
 * Since: 4.22
 */
void
gtk_popover_bin_popup (GtkPopoverBin *self)
{
  g_return_if_fail (GTK_IS_POPOVER_BIN (self));

  if (self->popover != NULL)
    gtk_popover_popup (GTK_POPOVER (self->popover));
}

/**
 * gtk_popover_bin_popdown:
 * @self: a popover bin
 *
 * Hides the popover from the user.
 *
 * See: [method@Gtk.PopoverBin.popup]
 *
 * Since: 4.22
 */
void
gtk_popover_bin_popdown (GtkPopoverBin *self)
{
  g_return_if_fail (GTK_IS_POPOVER_BIN (self));

  if (self->popover != NULL)
    gtk_popover_popdown (GTK_POPOVER (self->popover));
}
