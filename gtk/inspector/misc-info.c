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

#include "misc-info.h"
#include "window.h"
#include "object-tree.h"

#include "gtktypebuiltins.h"
#include "gtktreeview.h"
#include "gtkbuildable.h"
#include "gtklabel.h"
#include "gtkframe.h"

struct _GtkInspectorMiscInfoPrivate {
  GtkInspectorObjectTree *object_tree;

  GObject *object;

  GtkWidget *state_row;
  GtkWidget *state;
  GtkWidget *buildable_id_row;
  GtkWidget *buildable_id;
  GtkWidget *default_widget_row;
  GtkWidget *default_widget;
  GtkWidget *default_widget_button;
  GtkWidget *focus_widget_row;
  GtkWidget *focus_widget;
  GtkWidget *focus_widget_button;
  GtkWidget *allocated_size_row;
  GtkWidget *allocated_size;
};

enum
{
  PROP_0,
  PROP_OBJECT_TREE
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorMiscInfo, gtk_inspector_misc_info, GTK_TYPE_SCROLLED_WINDOW)

static gchar *
format_state_flags (GtkStateFlags state)
{
  GFlagsClass *fclass;
  GString *str;
  gint i;

  str = g_string_new ("");

  if (state)
    {
      fclass = g_type_class_ref (GTK_TYPE_STATE_FLAGS);
      for (i = 0; i < fclass->n_values; i++)
        {
          if (state & fclass->values[i].value)
            {
              if (str->len)
                g_string_append (str, " | ");
              g_string_append (str, fclass->values[i].value_nick);
            }
        } 
      g_type_class_unref (fclass);
    }
  else
    g_string_append (str, "normal");

  return g_string_free (str, FALSE);
}

static void
state_flags_changed (GtkWidget *w, GtkStateFlags old_flags, GtkInspectorMiscInfo *sl)
{
  gchar *s;

  s = format_state_flags (gtk_widget_get_state_flags (w));
  gtk_label_set_label (GTK_LABEL (sl->priv->state), s);
  g_free (s);
}

static void
allocation_changed (GtkWidget *w, GdkRectangle *allocation, GtkInspectorMiscInfo *sl)
{
  gchar *size_label = g_strdup_printf ("%d × %d",
                                       gtk_widget_get_allocated_width (w),
                                       gtk_widget_get_allocated_height (w));

  gtk_label_set_label (GTK_LABEL (sl->priv->allocated_size), size_label);
  g_free (size_label);
}

static void
disconnect_each_other (gpointer  still_alive,
                       GObject  *for_science)
{
  if (GTK_INSPECTOR_IS_MISC_INFO (still_alive))
    {
      GtkInspectorMiscInfo *self = GTK_INSPECTOR_MISC_INFO (still_alive);
      self->priv->object = NULL;
    }

  g_signal_handlers_disconnect_matched (still_alive, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, for_science);
  g_object_weak_unref (still_alive, disconnect_each_other, for_science);
}

static void
show_object (GtkInspectorMiscInfo *sl,
             GObject              *object,
             const gchar          *name,
             const gchar          *tab)
{
  GtkTreeIter iter;

  g_object_set_data (G_OBJECT (sl->priv->object_tree), "next-tab", (gpointer)tab);
  if (gtk_inspector_object_tree_find_object (sl->priv->object_tree, object, &iter))
    {
      gtk_inspector_object_tree_select_object (sl->priv->object_tree, object);
    }
  else if (GTK_IS_WIDGET (object) &&
           gtk_inspector_object_tree_find_object (sl->priv->object_tree, G_OBJECT (gtk_widget_get_parent (GTK_WIDGET (object))), &iter))

    {
      gtk_inspector_object_tree_append_object (sl->priv->object_tree, object, &iter, name);
      gtk_inspector_object_tree_select_object (sl->priv->object_tree, object);
    }
  else
    {
      g_warning ("GtkInspector: couldn't find the object in the tree");
    }
}

static void
update_default_widget (GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  widget = gtk_window_get_default_widget (GTK_WINDOW (sl->priv->object));
  if (widget)
    {
      gchar *tmp;
      tmp = g_strdup_printf ("%p", widget);
      gtk_label_set_label (GTK_LABEL (sl->priv->default_widget), tmp);
      g_free (tmp);
      gtk_widget_set_sensitive (sl->priv->default_widget_button, TRUE);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (sl->priv->default_widget), "NULL");   
      gtk_widget_set_sensitive (sl->priv->default_widget_button, FALSE);
    }
}

static void
show_default_widget (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  update_default_widget (sl);
  widget = gtk_window_get_default_widget (GTK_WINDOW (sl->priv->object));
  if (widget)
    show_object (sl, G_OBJECT (widget), NULL, "properties"); 
}

static void
update_focus_widget (GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  widget = gtk_window_get_focus (GTK_WINDOW (sl->priv->object));
  if (widget)
    {
      gchar *tmp;
      tmp = g_strdup_printf ("%p", widget);
      gtk_label_set_label (GTK_LABEL (sl->priv->focus_widget), tmp);
      g_free (tmp);
      gtk_widget_set_sensitive (sl->priv->focus_widget_button, TRUE);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (sl->priv->focus_widget), "NULL");   
      gtk_widget_set_sensitive (sl->priv->focus_widget_button, FALSE);
    }
}

static void
set_focus_cb (GtkWindow *window, GtkWidget *focus, GtkInspectorMiscInfo *sl)
{
  update_focus_widget (sl);
}

static void
show_focus_widget (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  widget = gtk_window_get_focus (GTK_WINDOW (sl->priv->object));
  if (widget)
    show_object (sl, G_OBJECT (widget), NULL, "properties"); 
}

void
gtk_inspector_misc_info_set_object (GtkInspectorMiscInfo *sl,
                                    GObject              *object)
{
  if (sl->priv->object)
    {
      g_signal_handlers_disconnect_by_func (sl->priv->object, state_flags_changed, sl);
      g_signal_handlers_disconnect_by_func (sl->priv->object, set_focus_cb, sl);
      g_signal_handlers_disconnect_by_func (sl->priv->object, allocation_changed, sl);
      disconnect_each_other (sl->priv->object, G_OBJECT (sl));
      disconnect_each_other (sl, sl->priv->object);
      sl->priv->object = NULL;
    }

  if (!GTK_IS_WIDGET (object) && !GTK_IS_BUILDABLE (object))
    {
      gtk_widget_hide (GTK_WIDGET (sl));
      return;
    }

  gtk_widget_show (GTK_WIDGET (sl));

  sl->priv->object = object;
  g_object_weak_ref (G_OBJECT (sl), disconnect_each_other, object);
  g_object_weak_ref (object, disconnect_each_other, sl);

  if (GTK_IS_WIDGET (object))
    {
      gtk_widget_show (sl->priv->state_row);
      g_signal_connect_object (object, "state-flags-changed", G_CALLBACK (state_flags_changed), sl, 0);
      state_flags_changed (GTK_WIDGET (sl->priv->object), 0, sl);

      allocation_changed (GTK_WIDGET (sl->priv->object), NULL, sl);
      gtk_widget_show (sl->priv->allocated_size_row);
      g_signal_connect_object (object, "size-allocate", G_CALLBACK (allocation_changed), sl, 0);
    }
  else
    {
      gtk_widget_hide (sl->priv->state_row);
      gtk_widget_hide (sl->priv->allocated_size_row);
    }

  if (GTK_IS_BUILDABLE (object))
    {
      gtk_label_set_text (GTK_LABEL (sl->priv->buildable_id),
                          gtk_buildable_get_name (GTK_BUILDABLE (object)));
      gtk_widget_show (sl->priv->buildable_id_row);
    }
  else
    {
      gtk_widget_hide (sl->priv->buildable_id_row);
    }

  if (GTK_IS_WINDOW (object))
    {
      gtk_widget_show (sl->priv->default_widget_row);
      gtk_widget_show (sl->priv->focus_widget_row);

      update_default_widget (sl);
      update_focus_widget (sl);

      g_signal_connect_object (object, "set-focus", G_CALLBACK (set_focus_cb), sl, G_CONNECT_AFTER);
    }
  else
    {
      gtk_widget_hide (sl->priv->default_widget_row);
      gtk_widget_hide (sl->priv->focus_widget_row);
    }
}

static void
gtk_inspector_misc_info_init (GtkInspectorMiscInfo *sl)
{
  sl->priv = gtk_inspector_misc_info_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        g_value_take_object (value, sl->priv->object_tree);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        sl->priv->object_tree = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_inspector_misc_info_class_init (GtkInspectorMiscInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Object Tree", "Obect tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/misc-info.ui");
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, state_row);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, state);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, buildable_id_row);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, buildable_id);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, default_widget_row);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, default_widget);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, default_widget_button);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, focus_widget_row);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, focus_widget);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, focus_widget_button);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, allocated_size_row);
   gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, allocated_size);

  gtk_widget_class_bind_template_callback (widget_class, show_default_widget);
  gtk_widget_class_bind_template_callback (widget_class, show_focus_widget);
}

// vim: set et sw=2 ts=2:
