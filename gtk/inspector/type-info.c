/*
 * Copyright © 2019 Zander Brown
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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "type-info.h"

#include "gtkpopover.h"

struct _GtkInspectorTypePopover
{
  GtkPopover parent_instance;
};

typedef struct _GtkInspectorTypePopoverPrivate GtkInspectorTypePopoverPrivate;

struct _GtkInspectorTypePopoverPrivate
{
  GType type;

  GtkWidget *parents;
  GtkWidget *interfaces;
};

enum
{
  PROP_0,
  PROP_TYPE
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorTypePopover, gtk_inspector_type_popover,
                            GTK_TYPE_POPOVER)

static void
add_row (GtkContainer *box,
         const gchar  *name)
{
  GtkWidget *row;
  GtkWidget *label;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "selectable", FALSE,
                      "activatable", FALSE,
                      "can-focus", FALSE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "margin", 6,
                        "label", name,
                        "selectable", TRUE,
                        "xalign", 0.0,
                        NULL);

  gtk_container_add (GTK_CONTAINER (row), label);
  gtk_container_add (box, row);
}

static void
remove_row (GtkWidget *item,
            gpointer   data)
{
  if (GTK_IS_LIST_BOX_ROW (item))
    gtk_container_remove (GTK_CONTAINER (data), item);
}

void
gtk_inspector_type_popover_set_gtype (GtkInspectorTypePopover *self,
                                      GType                    gtype)
{
  GtkInspectorTypePopoverPrivate *priv;
  GHashTable *implements;
  GHashTableIter iter;
  const gchar *name;
  GType *interfaces;
  GType tmp;
  gint i;

  g_return_if_fail (GTK_IS_INSPECTOR_TYPE_POPOVER (self));

  priv = gtk_inspector_type_popover_get_instance_private (self);

  if (priv->type == gtype)
    return;

  priv->type = gtype;

  gtk_container_foreach (GTK_CONTAINER (priv->parents),
                         remove_row, priv->parents);
  gtk_container_foreach (GTK_CONTAINER (priv->interfaces),
                         remove_row, priv->interfaces);

  implements = g_hash_table_new (g_str_hash, g_str_equal);

  tmp = gtype;

  do
    {
      add_row (GTK_CONTAINER (priv->parents), g_type_name (tmp));

      interfaces = g_type_interfaces (tmp, NULL);

      for (i = 0; interfaces[i]; i++)
        g_hash_table_add (implements, (gchar *) g_type_name (interfaces[i]));

      g_free (interfaces);
    }
  while ((tmp = g_type_parent (tmp)));

  g_hash_table_iter_init (&iter, implements);

  while (g_hash_table_iter_next (&iter, (gpointer *) &name, NULL))
    add_row (GTK_CONTAINER (priv->interfaces), name);

  g_hash_table_unref (implements);
}

static void
gtk_inspector_type_popover_init (GtkInspectorTypePopover *self)
{
  GtkInspectorTypePopoverPrivate *priv;
  GtkWidget *label;

  gtk_widget_init_template (GTK_WIDGET (self));

  priv = gtk_inspector_type_popover_get_instance_private (self);

  priv->type = G_TYPE_NONE;

  label = g_object_new (GTK_TYPE_LABEL,
                        "margin", 12,
                        "label", "None",
                        NULL);

  gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->parents), label);

  label = g_object_new (GTK_TYPE_LABEL,
                        "margin", 12,
                        "label", "None",
                        NULL);

  gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->interfaces), label);
}

static void
gtk_inspector_type_popover_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkInspectorTypePopover *self = GTK_INSPECTOR_TYPE_POPOVER (object);
  GtkInspectorTypePopoverPrivate *priv = gtk_inspector_type_popover_get_instance_private (self);

  switch (param_id)
    {
      case PROP_TYPE:
        g_value_set_gtype (value, priv->type);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_inspector_type_popover_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkInspectorTypePopover *self = GTK_INSPECTOR_TYPE_POPOVER (object);

  switch (param_id)
    {
      case PROP_TYPE:
        gtk_inspector_type_popover_set_gtype (self, g_value_get_gtype (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_inspector_type_popover_class_init (GtkInspectorTypePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gtk_inspector_type_popover_get_property;
  object_class->set_property = gtk_inspector_type_popover_set_property;

  g_object_class_install_property (object_class, PROP_TYPE,
      g_param_spec_gtype ("type", "Type", "Type",
                          G_TYPE_NONE, G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/type-info.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorTypePopover, parents);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorTypePopover, interfaces);
}
