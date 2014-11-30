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

#include "selector.h"

#include "gtktreeselection.h"
#include "gtktreestore.h"
#include "gtktreeview.h"
#include "gtkwidgetpath.h"
#include "gtklabel.h"


enum
{
  COLUMN_SELECTOR
};

struct _GtkInspectorSelectorPrivate
{
  GtkTreeStore *model;
  GtkTreeView *tree;
  GtkWidget *object_title;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorSelector, gtk_inspector_selector, GTK_TYPE_BOX)

static void
gtk_inspector_selector_init (GtkInspectorSelector *oh)
{
  oh->priv = gtk_inspector_selector_get_instance_private (oh);
  gtk_widget_init_template (GTK_WIDGET (oh));
}

static void
gtk_inspector_selector_class_init (GtkInspectorSelectorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/selector.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSelector, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSelector, tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorSelector, object_title);
}

void
gtk_inspector_selector_set_object (GtkInspectorSelector *oh,
                                   GObject              *object)
{
  GtkTreeIter iter, parent;
  gint i;
  GtkWidget *widget;
  gchar *path, **words;
  const gchar *title;

  gtk_tree_store_clear (oh->priv->model);

  if (!GTK_IS_WIDGET (object))
    {
      gtk_widget_hide (GTK_WIDGET (oh));
      return;
    }

  title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
  gtk_label_set_label (GTK_LABEL (oh->priv->object_title), title);

  widget = GTK_WIDGET (object);

  path = gtk_widget_path_to_string (gtk_widget_get_path (widget));
  words = g_strsplit (path, " ", 0);

  for (i = 0; i < g_strv_length (words); i++)
    {
      gtk_tree_store_append (oh->priv->model, &iter, i ? &parent : NULL);
      gtk_tree_store_set (oh->priv->model, &iter,
                          COLUMN_SELECTOR, words[i],
                          -1);
      parent = iter;
    }

  g_strfreev (words);
  g_free (path);

  gtk_tree_view_expand_all (oh->priv->tree);
  gtk_tree_selection_select_iter (gtk_tree_view_get_selection (oh->priv->tree), &iter);

  gtk_widget_show (GTK_WIDGET (oh));
}

// vim: set et sw=2 ts=2:
