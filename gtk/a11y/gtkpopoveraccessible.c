/* GTK - The GIMP Toolkit
 * Copyright © 2014 Red Hat Inc.
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtkpopoveraccessible.h"

typedef struct _GtkPopoverAccessiblePrivate GtkPopoverAccessiblePrivate;

struct _GtkPopoverAccessiblePrivate
{
  GtkWidget *widget;
};

G_DEFINE_TYPE_WITH_CODE (GtkPopoverAccessible, gtk_popover_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkPopoverAccessible))

static void
popover_update_relative_to (AtkObject  *obj,
                            GtkPopover *popover)
{
  GtkPopoverAccessiblePrivate *priv;
  AtkObject *widget_accessible;
  GtkWidget *widget;

  priv = gtk_popover_accessible_get_instance_private (GTK_POPOVER_ACCESSIBLE (obj));
  widget = gtk_popover_get_relative_to (popover);

  if (priv->widget == widget)
    return;

  if (priv->widget)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->widget),
                                    (gpointer*) &priv->widget);
      widget_accessible = gtk_widget_get_accessible (priv->widget);
      atk_object_remove_relationship (obj,
                                      ATK_RELATION_POPUP_FOR,
                                      widget_accessible);
    }

  priv->widget = widget;

  if (widget)
    {
      AtkObject *parent;

      parent = gtk_widget_get_accessible (widget);

      if (parent)
        atk_object_set_parent (obj, parent);

      g_object_add_weak_pointer (G_OBJECT (priv->widget),
                                 (gpointer*) &priv->widget);
      widget_accessible = gtk_widget_get_accessible (widget);
      atk_object_add_relationship (obj,
                                   ATK_RELATION_POPUP_FOR,
                                   widget_accessible);
    }
}

static void
popover_update_modality (AtkObject  *object,
                         GtkPopover *popover)
{
  atk_object_notify_state_change (object, ATK_STATE_MODAL,
                                  gtk_popover_get_modal (popover));
}

static void
popover_notify_cb (GtkPopover *popover,
                   GParamSpec *pspec,
                   AtkObject  *object)
{
  AtkObject *popover_accessible;

  popover_accessible = gtk_widget_get_accessible (GTK_WIDGET (popover));

  if (strcmp (g_param_spec_get_name (pspec), "relative-to") == 0)
    popover_update_relative_to (popover_accessible, popover);
  else if (strcmp (g_param_spec_get_name (pspec), "modal") == 0)
    popover_update_modality (popover_accessible, popover);
}

static void
gtk_popover_accessible_initialize (AtkObject *obj,
                                   gpointer   data)
{
  GtkPopover *popover = GTK_POPOVER (data);

  ATK_OBJECT_CLASS (gtk_popover_accessible_parent_class)->initialize (obj, data);

  g_signal_connect (popover, "notify",
                    G_CALLBACK (popover_notify_cb), obj);
  popover_update_relative_to (obj, popover);
  popover_update_modality (obj, popover);

  obj->role = ATK_ROLE_PANEL;
}

static AtkStateSet *
gtk_popover_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_popover_accessible_parent_class)->ref_state_set (obj);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (gtk_popover_get_modal (GTK_POPOVER (widget)))
    atk_state_set_add_state (state_set, ATK_STATE_MODAL);

  return state_set;
}

static void
gtk_popover_accessible_finalize (GObject *object)
{
  GtkPopoverAccessiblePrivate *priv;

  priv = gtk_popover_accessible_get_instance_private (GTK_POPOVER_ACCESSIBLE (object));

  if (priv->widget)
    g_object_remove_weak_pointer (G_OBJECT (priv->widget),
                                  (gpointer*) &priv->widget);
  G_OBJECT_CLASS (gtk_popover_accessible_parent_class)->finalize (object);
}

static void
gtk_popover_accessible_class_init (GtkPopoverAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_popover_accessible_finalize;
  class->initialize = gtk_popover_accessible_initialize;
  class->ref_state_set = gtk_popover_accessible_ref_state_set;
}

static void
gtk_popover_accessible_init (GtkPopoverAccessible *popover)
{
}
