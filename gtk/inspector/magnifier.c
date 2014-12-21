/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "magnifier.h"

#include "gtkmagnifierprivate.h"

#include "gtklabel.h"


struct _GtkInspectorMagnifierPrivate
{
  GtkWidget *object;
  GtkWidget *magnifier;
  GtkWidget *object_title;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorMagnifier, gtk_inspector_magnifier, GTK_TYPE_BOX)

static void
gtk_inspector_magnifier_init (GtkInspectorMagnifier *sl)
{
  sl->priv = gtk_inspector_magnifier_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

void
gtk_inspector_magnifier_set_object (GtkInspectorMagnifier *sl,
                                    GObject              *object)
{
  const gchar *title;

  sl->priv->object = NULL;

  if (!GTK_IS_WIDGET (object))
    {
      gtk_widget_hide (GTK_WIDGET (sl));
      _gtk_magnifier_set_inspected (GTK_MAGNIFIER (sl->priv->magnifier), NULL);
      return;
    }

  title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
  gtk_label_set_label (GTK_LABEL (sl->priv->object_title), title);

  gtk_widget_show (GTK_WIDGET (sl));

  sl->priv->object = GTK_WIDGET (object);

  _gtk_magnifier_set_inspected (GTK_MAGNIFIER (sl->priv->magnifier), GTK_WIDGET (object));
  _gtk_magnifier_set_coords (GTK_MAGNIFIER (sl->priv->magnifier), 0, 0);
}

static void
gtk_inspector_magnifier_class_init (GtkInspectorMagnifierClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/magnifier.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMagnifier, magnifier);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMagnifier, object_title);
}

// vim: set et sw=2 ts=2:
