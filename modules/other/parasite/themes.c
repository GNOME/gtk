/*
 * Copyright (c) 2013 Intel Corporation
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

#include "themes.h"

struct _ParasiteThemesPrivate
{
  GtkWidget *dark_switch;
  GtkWidget *theme_combo;
  GtkWidget *icon_combo;
};

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteThemes, parasite_themes, GTK_TYPE_LIST_BOX)

static void
init_dark (ParasiteThemes *pt)
{
  g_object_bind_property (pt->priv->dark_switch, "active",
                          gtk_settings_get_default (), "gtk-application-prefer-dark-theme",
                          G_BINDING_BIDIRECTIONAL);
}

static void
fill_gtk (const gchar *path,
          GHashTable  *t)
{
  const gchar *dir_entry;
  GDir *dir = g_dir_open (path, 0, NULL);

  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      gchar *filename = g_build_filename (path, dir_entry, "gtk-3.0", "gtk.css", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }
}

static void
init_theme (ParasiteThemes *pt)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *current_theme, *path;
  gint i, pos;
  GSettings *settings;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_hash_table_add (t, g_strdup ("Raleigh"));

  fill_gtk (GTK_DATADIR "/themes", t);
  path = g_build_filename (g_get_user_data_dir (), "themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  settings = g_settings_new ("org.gnome.desktop.interface");
  current_theme = g_settings_get_string (settings, "gtk-theme");
  g_object_unref (settings);

  g_hash_table_iter_init (&iter, t);
  pos = i = 0;
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    {
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (pt->priv->theme_combo), theme);
      if (g_strcmp0 (theme, current_theme) == 0)
        pos = i;
      i++;
    }
  g_hash_table_destroy (t);

  gtk_combo_box_set_active (GTK_COMBO_BOX (pt->priv->theme_combo), pos);
}

static void
fill_icons (const gchar *path,
            GHashTable  *t)
{
  const gchar *dir_entry;
  GDir *dir = g_dir_open (path, 0, NULL);

  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      gchar *filename = g_build_filename (path, dir_entry, "index.theme", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
          g_strcmp0 (dir_entry, "hicolor") != 0 &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }
}

static void
init_icons (ParasiteThemes *pt)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *current_theme, *path;
  gint i, pos;
  GSettings *settings;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  fill_icons (GTK_DATADIR "/icons", t);
  path = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  fill_icons (path, t);
  g_free (path);

  settings = g_settings_new ("org.gnome.desktop.interface");
  current_theme = g_settings_get_string (settings, "icon-theme");
  g_object_unref (settings);

  g_hash_table_iter_init (&iter, t);
  pos = i = 0;
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    {
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (pt->priv->icon_combo), theme);
      if (g_strcmp0 (theme, current_theme) == 0)
        pos = i;
      i++;
    }
  g_hash_table_destroy (t);

  gtk_combo_box_set_active (GTK_COMBO_BOX (pt->priv->icon_combo), pos);
}

static void
theme_changed (GtkComboBox    *c,
               ParasiteThemes *pt)
{
  gchar *theme = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (c));
  g_object_set (gtk_settings_get_default (), "gtk-theme-name", theme, NULL);
  g_free (theme);
}

static void
icons_changed (GtkComboBox    *c,
               ParasiteThemes *pt)
{
  gchar *theme = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (c));
  g_object_set (gtk_settings_get_default (), "gtk-icon-theme-name", theme, NULL);
  g_free (theme);
}

static void
parasite_themes_init (ParasiteThemes *pt)
{
  pt->priv = parasite_themes_get_instance_private (pt);
  gtk_widget_init_template (GTK_WIDGET (pt));

  init_dark (pt);
  init_theme (pt);
  init_icons (pt);
}

static void
parasite_themes_class_init (ParasiteThemesClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/parasite/themes.ui");
  gtk_widget_class_bind_template_child_private (widget_class, ParasiteThemes, dark_switch);
  gtk_widget_class_bind_template_child_private (widget_class, ParasiteThemes, theme_combo);
  gtk_widget_class_bind_template_child_private (widget_class, ParasiteThemes, icon_combo);

  gtk_widget_class_bind_template_callback (widget_class, theme_changed);
  gtk_widget_class_bind_template_callback (widget_class, icons_changed);
}

GtkWidget *
parasite_themes_new (void)
{
  return GTK_WIDGET (g_object_new (PARASITE_TYPE_THEMES, NULL));
}

// vim: set et sw=2 ts=2:
