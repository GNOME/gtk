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
#include "gtkliststore.h"
#include "gtksettings.h"

enum
{
  COLUMN_NAME,
  COLUMN_VALUE,
  COLUMN_LOCATION,
  COLUMN_URI,
  COLUMN_LINE
};

struct _GtkInspectorStylePropListPrivate
{
  GHashTable *css_files;
  GtkListStore *model;
  GtkWidget *widget;
  GHashTable *prop_iters;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorStylePropList, gtk_inspector_style_prop_list, GTK_TYPE_BOX)

static void
gtk_inspector_style_prop_list_init (GtkInspectorStylePropList *pl)
{
  gint i;

  pl->priv = gtk_inspector_style_prop_list_get_instance_private (pl);
  gtk_widget_init_template (GTK_WIDGET (pl));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pl->priv->model),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);

  pl->priv->css_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                           g_object_unref, (GDestroyNotify) g_strfreev);

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

  g_hash_table_unref (pl->priv->css_files);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/style-prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStylePropList, model);
}

static gchar *
strip_property (const gchar *property)
{
  gchar **split;
  gchar *value;

  split = g_strsplit_set (property, ":;", 3);
  if (!split[0] || !split[1])
    value = g_strdup ("");
  else
    value = g_strdup (split[1]);

  g_strfreev (split);

  return value;
}

static gchar *
get_css_content (GtkInspectorStylePropList *self,
                 GFile                     *file,
                 guint                      start_line,
                 guint                      end_line)
{
  GtkInspectorStylePropListPrivate *priv = self->priv;
  guint i;
  guint contents_lines;
  gchar *value, *property;
  gchar **contents;

  contents = g_hash_table_lookup (priv->css_files, file);
  if (!contents)
    {
      gchar *tmp;

      if (g_file_load_contents (file, NULL, &tmp, NULL, NULL, NULL))
        {
          contents = g_strsplit_set (tmp, "\n\r", -1);
          g_free (tmp);
        }
      else
        {
          contents =  g_strsplit ("", "", -1);
        }

      g_object_ref (file);
      g_hash_table_insert (priv->css_files, file, contents);
    }

  contents_lines = g_strv_length (contents);
  property = g_strdup ("");
  for (i = start_line; (i < end_line + 1) && (i < contents_lines); ++i)
    {
      gchar *s1, *s2;

      s1 = g_strdup (contents[i]);
      s1 = g_strstrip (s1);
      s2 = g_strconcat (property, s1, NULL);
      g_free (property);
      g_free (s1);
      property = s2;
    }

  value = strip_property (property);
  g_free (property);

  return value;
}

static void
populate (GtkInspectorStylePropList *self)
{
  GtkInspectorStylePropListPrivate *priv = self->priv;
  GtkStyleContext *context;
  gint i;

  context = gtk_widget_get_style_context (priv->widget);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      const gchar *name;
      GtkTreeIter *iter;
      GtkCssSection *section;
      gchar *location;
      gchar *value;
      gchar *uri;
      guint start_line, end_line;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      iter = (GtkTreeIter *)g_hash_table_lookup (priv->prop_iters, name);

      section = gtk_style_context_get_section (context, name);
      if (section)
        {
          GFileInfo *info;
          GFile *file;
          const gchar *path;

          start_line = gtk_css_section_get_start_line (section);
          end_line = gtk_css_section_get_end_line (section);

          file = gtk_css_section_get_file (section);
          if (file)
            {
              info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

              if (info)
                path = g_file_info_get_display_name (info);
              else
                path = "<broken file>";

              uri = g_file_get_uri (file);
              value = get_css_content (self, file, start_line, end_line);
            }
          else
            {
              info = NULL;
              path = "<data>";
              uri = NULL;
              value = NULL;
            }

          if (end_line != start_line)
            location = g_strdup_printf ("%s:%u-%u", path, start_line + 1, end_line + 1);
          else
            location = g_strdup_printf ("%s:%u", path, start_line + 1);
          if (info)
            g_object_unref (info);
        }
      else
        {
          location = NULL;
          value = NULL;
          uri = NULL;
          start_line = -1;
        }

      gtk_list_store_set (priv->model,
                          iter,
                          COLUMN_VALUE, value,
                          COLUMN_LOCATION, location,
                          COLUMN_URI, uri,
                          COLUMN_LINE, start_line + 1,
                          -1);

      g_free (location);
      g_free (value);
      g_free (uri);
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
