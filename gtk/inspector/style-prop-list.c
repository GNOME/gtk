/*
 * Copyright (c) 2014 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "style-prop-list.h"

#include "gtkcssproviderprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsssectionprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkliststore.h"
#include "gtksettings.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtkstack.h"
#include "gtksearchentry.h"
#include "gtklabel.h"

enum
{
  COLUMN_NAME,
  COLUMN_VALUE,
  COLUMN_LOCATION
};

struct _GtkInspectorStylePropListPrivate
{
  GtkListStore *model;
  GtkWidget *widget;
  GtkWidget *tree;
  GtkWidget *search_entry;
  GtkWidget *search_stack;
  GtkWidget *object_title;
  GHashTable *prop_iters;
  GtkTreeViewColumn *name_column;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorStylePropList, gtk_inspector_style_prop_list, GTK_TYPE_BOX)

static void
search_close_clicked (GtkWidget                 *button,
                      GtkInspectorStylePropList *pl)
{
  gtk_entry_set_text (GTK_ENTRY (pl->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "title");
}

static gboolean
key_press_event (GtkWidget                 *window,
                 GdkEvent                  *event,
                 GtkInspectorStylePropList *pl)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (pl)))
    return GDK_EVENT_PROPAGATE;

  if (gtk_search_entry_handle_event (GTK_SEARCH_ENTRY (pl->priv->search_entry), event))
    {
      gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "search");
      return GDK_EVENT_STOP;
    }
  return GDK_EVENT_PROPAGATE;
}

static void
hierarchy_changed (GtkWidget *widget,
                   GtkWidget *previous_toplevel)
{
  if (previous_toplevel)
    g_signal_handlers_disconnect_by_func (previous_toplevel, key_press_event, widget);
  g_signal_connect (gtk_widget_get_toplevel (widget), "key-press-event",
                    G_CALLBACK (key_press_event), widget);
}

static void
gtk_inspector_style_prop_list_init (GtkInspectorStylePropList *pl)
{
  gint i;

  pl->priv = gtk_inspector_style_prop_list_get_instance_private (pl);
  gtk_widget_init_template (GTK_WIDGET (pl));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pl->priv->model),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (pl->priv->tree),
                                  GTK_ENTRY (pl->priv->search_entry));

  pl->priv->prop_iters = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                (GDestroyNotify) gtk_tree_iter_free);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      GtkTreeIter iter;
      const gchar *name;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      gtk_list_store_append (pl->priv->model, &iter);
      gtk_list_store_set (pl->priv->model, &iter, COLUMN_NAME, name, -1);
      g_hash_table_insert (pl->priv->prop_iters, (gpointer)name, gtk_tree_iter_copy (&iter));
    }
}

static void
disconnect_each_other (gpointer  still_alive,
                       GObject  *for_science)
{
  if (GTK_INSPECTOR_IS_STYLE_PROP_LIST (still_alive))
    {
      GtkInspectorStylePropList *self = GTK_INSPECTOR_STYLE_PROP_LIST (still_alive);
      self->priv->widget = NULL;
    }

  g_signal_handlers_disconnect_matched (still_alive, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, for_science);
  g_object_weak_unref (still_alive, disconnect_each_other, for_science);
}

static void
finalize (GObject *object)
{
  GtkInspectorStylePropList *pl = GTK_INSPECTOR_STYLE_PROP_LIST (object);

  g_hash_table_unref (pl->priv->prop_iters);

  G_OBJECT_CLASS (gtk_inspector_style_prop_list_parent_class)->finalize (object);
}

static void
ensure_css_sections (void)
{
  GtkSettings *settings;
  gchar *theme_name;

  gtk_css_provider_set_keep_css_sections ();

  settings = gtk_settings_get_default ();
  g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
  g_object_set (settings, "gtk-theme-name", theme_name, NULL);
  g_free (theme_name);
}

static void
gtk_inspector_style_prop_list_class_init (GtkInspectorStylePropListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  ensure_css_sections ();

  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/style-prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, search_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, object_title);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, name_column);
  gtk_widget_class_bind_template_callback (widget_class, search_close_clicked);
  gtk_widget_class_bind_template_callback (widget_class, hierarchy_changed);
}

static void
populate (GtkInspectorStylePropList *self)
{
  GtkInspectorStylePropListPrivate *priv = self->priv;
  GtkStyleContext *context;
  GtkCssStyle *style;
  gint i;

  context = gtk_widget_get_style_context (priv->widget);
  style = gtk_style_context_lookup_style (context);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      const gchar *name;
      GtkTreeIter *iter;
      GtkCssSection *section;
      gchar *location;
      gchar *value;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      iter = (GtkTreeIter *)g_hash_table_lookup (priv->prop_iters, name);

      value = _gtk_css_value_to_string (gtk_css_style_get_value (style, i));

      section = gtk_css_style_get_section (style, i);
      if (section)
        location = _gtk_css_section_to_string (section);
      else
        location = NULL;

      gtk_list_store_set (priv->model,
                          iter,
                          COLUMN_VALUE, value,
                          COLUMN_LOCATION, location,
                          -1);

      g_free (location);
      g_free (value);
    }
}

static void
widget_style_updated (GtkWidget                 *widget,
                      GtkInspectorStylePropList *self)
{
  populate (self);
}

static void
widget_state_flags_changed (GtkWidget                 *widget,
                            GtkStateFlags              flags,
                            GtkInspectorStylePropList *self)
{
  populate (self);
}

void
gtk_inspector_style_prop_list_set_object (GtkInspectorStylePropList *self,
                                          GObject                   *object)
{
  const gchar *title;

  if (self->priv->widget == (GtkWidget *)object)
    {
      gtk_widget_hide (GTK_WIDGET (self));
      return;
    }

 if (self->priv->widget)
    {
      disconnect_each_other (self->priv->widget, G_OBJECT (self));
      disconnect_each_other (self, G_OBJECT (self->priv->widget));
      self->priv->widget = NULL;
    }

  if (!GTK_IS_WIDGET (object))
    {
      gtk_widget_hide (GTK_WIDGET (self));
      return;
    }

  title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
  gtk_label_set_label (GTK_LABEL (self->priv->object_title), title);

  gtk_entry_set_text (GTK_ENTRY (self->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (self->priv->search_stack), "title");

  self->priv->widget = (GtkWidget *)object;
  g_object_weak_ref (G_OBJECT (self), disconnect_each_other, object);
  g_object_weak_ref (G_OBJECT (object), disconnect_each_other, self);

  populate (self);
  gtk_widget_show (GTK_WIDGET (self));

  g_signal_connect (object, "style-updated",
                    G_CALLBACK (widget_style_updated), self);
  g_signal_connect (object, "state-flags-changed",
                    G_CALLBACK (widget_state_flags_changed), self);
}

// vim: set et sw=2 ts=2:
