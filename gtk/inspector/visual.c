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

#include "visual.h"

#include "fpsoverlay.h"
#include "updatesoverlay.h"
#include "layoutoverlay.h"
#include "window.h"

#include "gtkadjustment.h"
#include "gtkbox.h"
#include "gtkcomboboxtext.h"
#include "gtkcssproviderprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkswitch.h"
#include "gtkscale.h"
#include "gtkwindow.h"
#include "gtklistbox.h"

#include "fallback-c89.c"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#include "gdk/gdk-private.h"

#define EPSILON               1e-10

struct _GtkInspectorVisualPrivate
{
  GtkWidget *visual_box;
  GtkWidget *theme_combo;
  GtkWidget *dark_switch;
  GtkWidget *icon_combo;
  GtkWidget *cursor_combo;
  GtkWidget *cursor_size_spin;
  GtkWidget *direction_combo;
  GtkWidget *font_button;
  GtkWidget *hidpi_spin;
  GtkWidget *animation_switch;
  GtkWidget *font_scale_entry;
  GtkAdjustment *font_scale_adjustment;
  GtkAdjustment *scale_adjustment;
  GtkAdjustment *slowdown_adjustment;
  GtkWidget *slowdown_entry;
  GtkAdjustment *cursor_size_adjustment;

  GtkWidget *debug_box;
  GtkWidget *fps_switch;
  GtkWidget *updates_switch;
  GtkWidget *baselines_switch;
  GtkWidget *layout_switch;
  GtkWidget *resize_switch;

  GtkWidget *misc_box;
  GtkWidget *touchscreen_switch;
  GtkWidget *software_gl_switch;

  GtkAdjustment *focus_adjustment;

  GtkInspectorOverlay *fps_overlay;
  GtkInspectorOverlay *updates_overlay;
  GtkInspectorOverlay *layout_overlay;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorVisual, gtk_inspector_visual, GTK_TYPE_SCROLLED_WINDOW)

static void
fix_direction_recurse (GtkWidget *widget,
                       gpointer   data)
{
  GtkTextDirection dir = GPOINTER_TO_INT (data);
  GtkWidget *child;

  g_object_ref (widget);

  gtk_widget_set_direction (widget, dir);
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
     {
        fix_direction_recurse (child, data);
     }

  g_object_unref (widget);
}

static GtkTextDirection initial_direction;

static void
fix_direction (GtkWidget *iw)
{
  fix_direction_recurse (iw, GINT_TO_POINTER (initial_direction));
}

static void
direction_changed (GtkComboBox *combo)
{
  GtkWidget *iw;
  const gchar *direction;

  iw = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (combo)));
  fix_direction (iw);

  direction = gtk_combo_box_get_active_id (combo);
  if (g_strcmp0 (direction, "ltr") == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
  else
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
}

static void
init_direction (GtkInspectorVisual *vis)
{
  const gchar *direction;

  initial_direction = gtk_widget_get_default_direction ();
  if (initial_direction == GTK_TEXT_DIR_LTR)
    direction = "ltr";
  else
    direction = "rtl";
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (vis->priv->direction_combo), direction);
}

static void
redraw_everything (void)
{
  GList *toplevels;
  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) gtk_widget_queue_draw, NULL);
  g_list_free (toplevels);
}

static double
get_font_scale (GtkInspectorVisual *vis)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      int dpi_int;

      g_object_get (gtk_settings_get_default (),
                    "gtk-xft-dpi", &dpi_int,
                    NULL);

      return dpi_int / (96.0 * 1024.0);
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      int dpi_int;

      g_object_get (gtk_settings_get_default (),
                    "gtk-xft-dpi", &dpi_int,
                    NULL);

      return dpi_int / (96.0 * 1024.0);
    }
#endif

  return 1.0;
}

static void
update_font_scale (GtkInspectorVisual *vis,
                   gdouble             factor,
                   gboolean            update_adjustment,
                   gboolean            update_entry)
{
  g_object_set (gtk_settings_get_default (),
                "gtk-xft-dpi", (gint)(factor * 96 * 1024),
                NULL);

  if (update_adjustment)
    gtk_adjustment_set_value (vis->priv->font_scale_adjustment, factor);

  if (update_entry)
    {
      gchar *str = g_strdup_printf ("%0.2f", factor);

      gtk_editable_set_text (GTK_EDITABLE (vis->priv->font_scale_entry), str);
      g_free (str);
    }
}

static void
font_scale_adjustment_changed (GtkAdjustment      *adjustment,
                               GtkInspectorVisual *vis)
{
  gdouble factor;

  factor = gtk_adjustment_get_value (adjustment);
  update_font_scale (vis, factor, FALSE, TRUE);
}

static void
font_scale_entry_activated (GtkEntry           *entry,
                            GtkInspectorVisual *vis)
{
  gdouble factor;
  gchar *err = NULL;

  factor = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    update_font_scale (vis, factor, TRUE, FALSE);
}

static void
fps_activate (GtkSwitch          *sw,
              GParamSpec         *pspec,
              GtkInspectorVisual *vis)
{
  GtkInspectorVisualPrivate *priv = vis->priv;
  GtkInspectorWindow *iw;
  gboolean fps;

  fps = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (fps)
    {
      if (priv->fps_overlay == NULL)
        {
          priv->fps_overlay = gtk_fps_overlay_new ();
          gtk_inspector_window_add_overlay (iw, priv->fps_overlay);
          g_object_unref (priv->fps_overlay);
        }
    }
  else
    {
      if (priv->fps_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, priv->fps_overlay);
          priv->fps_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
updates_activate (GtkSwitch          *sw,
                  GParamSpec         *pspec,
                  GtkInspectorVisual *vis)
{
  GtkInspectorVisualPrivate *priv = vis->priv;
  GtkInspectorWindow *iw;
  gboolean updates;

  updates = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (updates)
    {
      if (priv->updates_overlay == NULL)
        {
          priv->updates_overlay = gtk_updates_overlay_new ();
          gtk_inspector_window_add_overlay (iw, priv->updates_overlay);
          g_object_unref (priv->updates_overlay);
        }
    }
  else
    {
      if (priv->updates_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, priv->updates_overlay);
          priv->updates_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
baselines_activate (GtkSwitch *sw)
{
  guint flags;

  flags = gtk_get_debug_flags ();

  if (gtk_switch_get_active (sw))
    flags |= GTK_DEBUG_BASELINES;
  else
    flags &= ~GTK_DEBUG_BASELINES;

  gtk_set_debug_flags (flags);
  redraw_everything ();
}

static void
layout_activate (GtkSwitch          *sw,
                 GParamSpec         *pspec,
                 GtkInspectorVisual *vis)
{
  GtkInspectorVisualPrivate *priv = vis->priv;
  GtkInspectorWindow *iw;
  gboolean draw_layout;

  draw_layout = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (draw_layout)
    {
      if (priv->layout_overlay == NULL)
        {
          priv->layout_overlay = gtk_layout_overlay_new ();
          gtk_inspector_window_add_overlay (iw, priv->layout_overlay);
          g_object_unref (priv->layout_overlay);
        }
    }
  else
    {
      if (priv->layout_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, priv->layout_overlay);
          priv->layout_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
widget_resize_activate (GtkSwitch *sw)
{
  guint flags = gtk_get_debug_flags ();

  if (gtk_switch_get_active (sw))
    flags |= GTK_DEBUG_RESIZE;
  else
    flags &= ~GTK_DEBUG_RESIZE;

  gtk_set_debug_flags (flags);
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
      gchar *filename = g_build_filename (path, dir_entry, "gtk-4.0", "gtk.css", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }

  g_dir_close (dir);
}

static gchar*
get_data_path (const gchar *subdir)
{
  gchar *base_datadir, *full_datadir;
#if defined (GDK_WINDOWING_WIN32) || defined (GDK_WINDOWING_QUARTZ)
  base_datadir = g_strdup (_gtk_get_datadir ());
#else
  base_datadir = g_strdup (GTK_DATADIR);
#endif
  full_datadir = g_build_filename (base_datadir, subdir, NULL);
  g_free (base_datadir);
  return full_datadir;
}

static void
init_theme (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *path;
  gchar **builtin_themes;
  GList *list, *l;
  guint i;
  const gchar * const *dirs;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  /* Builtin themes */
  builtin_themes = g_resources_enumerate_children ("/org/gtk/libgtk/theme", 0, NULL);
  for (i = 0; builtin_themes[i] != NULL; i++)
    {
      if (g_str_has_suffix (builtin_themes[i], "/"))
        g_hash_table_add (t, g_strndup (builtin_themes[i], strlen (builtin_themes[i]) - 1));
    }
  g_strfreev (builtin_themes);

  path = _gtk_get_theme_dir ();
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_home_dir (), ".themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = g_build_filename (dirs[i], "themes", NULL);
      fill_gtk (path, t);
      g_free (path);
    }

  list = NULL;
  g_hash_table_iter_init (&iter, t);
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    list = g_list_insert_sorted (list, theme, (GCompareFunc)strcmp);

  for (l = list; l; l = l->next)
    {
      theme = l->data;
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (vis->priv->theme_combo), theme, theme);
    }

  g_list_free (list);
  g_hash_table_destroy (t);

  g_object_bind_property (gtk_settings_get_default (), "gtk-theme-name",
                          vis->priv->theme_combo, "active-id",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  if (g_getenv ("GTK_THEME") != NULL)
    {
      GtkWidget *row;

      /* theme is hardcoded, nothing we can do */
      gtk_widget_set_sensitive (vis->priv->theme_combo, FALSE);
      row = gtk_widget_get_ancestor (vis->priv->theme_combo, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("Theme is hardcoded by GTK_THEME"));
    }
}

static void
init_dark (GtkInspectorVisual *vis)
{
  g_object_bind_property (gtk_settings_get_default (), "gtk-application-prefer-dark-theme",
                          vis->priv->dark_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  if (g_getenv ("GTK_THEME") != NULL)
    {
      GtkWidget *row;

      /* theme is hardcoded, nothing we can do */
      gtk_widget_set_sensitive (vis->priv->dark_switch, FALSE);
      row = gtk_widget_get_ancestor (vis->priv->theme_combo, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("Theme is hardcoded by GTK_THEME"));
    }
}

static void
fill_icons (const gchar *path,
            GHashTable  *t)
{
  const gchar *dir_entry;
  GDir *dir;

  dir = g_dir_open (path, 0, NULL);
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

  g_dir_close (dir);
}

static void
init_icons (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *path;
  GList *list, *l;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  path = get_data_path ("icons");
  fill_icons (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  fill_icons (path, t);
  g_free (path);

  list = NULL;
  g_hash_table_iter_init (&iter, t);
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    list = g_list_insert_sorted (list, theme, (GCompareFunc)strcmp);

  for (l = list; l; l = l->next)
    {
      theme = l->data;
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (vis->priv->icon_combo), theme, theme);
    }

  g_hash_table_destroy (t);
  g_list_free (list);

  g_object_bind_property (gtk_settings_get_default (), "gtk-icon-theme-name",
                          vis->priv->icon_combo, "active-id",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
fill_cursors (const gchar *path,
              GHashTable  *t)
{
  const gchar *dir_entry;
  GDir *dir;

  dir = g_dir_open (path, 0, NULL);
  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      gchar *filename = g_build_filename (path, dir_entry, "cursors", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_DIR) &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }

  g_dir_close (dir);
}

static void
init_cursors (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *path;
  GList *list, *l;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  path = get_data_path ("icons");
  fill_cursors (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  fill_cursors (path, t);
  g_free (path);

  list = NULL;
  g_hash_table_iter_init (&iter, t);
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    list = g_list_insert_sorted (list, theme, (GCompareFunc)strcmp);

  for (l = list; l; l = l->next)
    {
      theme = l->data;
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (vis->priv->cursor_combo), theme, theme);
    }

  g_hash_table_destroy (t);
  g_list_free (list);

  g_object_bind_property (gtk_settings_get_default (), "gtk-cursor-theme-name",
                          vis->priv->cursor_combo, "active-id",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
cursor_size_changed (GtkAdjustment *adjustment, GtkInspectorVisual *vis)
{
  gint size;

  size = gtk_adjustment_get_value (adjustment);
  g_object_set (gtk_settings_get_default (), "gtk-cursor-theme-size", size, NULL);
}

static void
init_cursor_size (GtkInspectorVisual *vis)
{
  gint size;

  g_object_get (gtk_settings_get_default (), "gtk-cursor-theme-size", &size, NULL);
  if (size == 0)
    size = 32;

  gtk_adjustment_set_value (vis->priv->scale_adjustment, (gdouble)size);
  g_signal_connect (vis->priv->cursor_size_adjustment, "value-changed",
                    G_CALLBACK (cursor_size_changed), vis);
}

static void
init_font (GtkInspectorVisual *vis)
{
  g_object_bind_property (gtk_settings_get_default (), "gtk-font-name",
                          vis->priv->font_button, "font",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
}

static void
init_font_scale (GtkInspectorVisual *vis)
{
  double scale;

  scale = get_font_scale (vis);
  update_font_scale (vis, scale, TRUE, TRUE);
  g_signal_connect (vis->priv->font_scale_adjustment, "value-changed",
                    G_CALLBACK (font_scale_adjustment_changed), vis);
  g_signal_connect (vis->priv->font_scale_entry, "activate",
                    G_CALLBACK (font_scale_entry_activated), vis);
}

#if defined (GDK_WINDOWING_X11)
static void
scale_changed (GtkAdjustment *adjustment, GtkInspectorVisual *vis)
{
  GdkDisplay *display;
  gint scale;

  scale = gtk_adjustment_get_value (adjustment);
  display = gdk_display_get_default ();
  gdk_x11_display_set_surface_scale (display, scale);
}
#endif

static void
init_scale (GtkInspectorVisual *vis)
{
#if defined (GDK_WINDOWING_X11)
  GdkDisplay *display;

  display = gdk_display_get_default ();
  if (GDK_IS_X11_DISPLAY (display))
    {
      gdouble scale;

      scale = gdk_monitor_get_scale_factor (gdk_display_get_primary_monitor (display));
      gtk_adjustment_set_value (vis->priv->scale_adjustment, scale);
      g_signal_connect (vis->priv->scale_adjustment, "value-changed",
                        G_CALLBACK (scale_changed), vis);
    }
  else
#endif
    {
      GtkWidget *row;

      gtk_adjustment_set_value (vis->priv->scale_adjustment, 1);
      gtk_widget_set_sensitive (vis->priv->hidpi_spin, FALSE);
      row = gtk_widget_get_ancestor (vis->priv->hidpi_spin, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("Backend does not support window scaling"));
    }
}

static void
init_animation (GtkInspectorVisual *vis)
{
  g_object_bind_property (gtk_settings_get_default (), "gtk-enable-animations",
                          vis->priv->animation_switch, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
}

static void
update_slowdown (GtkInspectorVisual *vis,
                 gdouble slowdown,
                 gboolean update_adjustment,
                 gboolean update_entry)
{
  _gtk_set_slowdown (slowdown);

  if (update_adjustment)
    gtk_adjustment_set_value (vis->priv->slowdown_adjustment,
                              log2 (slowdown));

  if (update_entry)
    {
      gchar *str = g_strdup_printf ("%0.*f", 2, slowdown);

      gtk_editable_set_text (GTK_EDITABLE (vis->priv->slowdown_entry), str);
      g_free (str);
    }
}

static void
slowdown_adjustment_changed (GtkAdjustment *adjustment,
                             GtkInspectorVisual *vis)
{
  gdouble value = gtk_adjustment_get_value (adjustment);
  gdouble previous = CLAMP (log2 (_gtk_get_slowdown ()),
                            gtk_adjustment_get_lower (adjustment),
                            gtk_adjustment_get_upper (adjustment));

  if (fabs (value - previous) > EPSILON)
    update_slowdown (vis, exp2 (value), FALSE, TRUE);
}

static void
slowdown_entry_activated (GtkEntry *entry,
                          GtkInspectorVisual *vis)
{
  gdouble slowdown;
  gchar *err = NULL;

  slowdown = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    update_slowdown (vis, slowdown, TRUE, FALSE);
}

static void
init_slowdown (GtkInspectorVisual *vis)
{
  update_slowdown (vis, _gtk_get_slowdown (), TRUE, TRUE);
  g_signal_connect (vis->priv->slowdown_adjustment, "value-changed",
                    G_CALLBACK (slowdown_adjustment_changed), vis);
  g_signal_connect (vis->priv->slowdown_entry, "activate",
                    G_CALLBACK (slowdown_entry_activated), vis);
}

static void
update_touchscreen (GtkSwitch *sw)
{
  GtkDebugFlag flags;

  flags = gtk_get_debug_flags ();

  if (gtk_switch_get_active (sw))
    flags |= GTK_DEBUG_TOUCHSCREEN;
  else
    flags &= ~GTK_DEBUG_TOUCHSCREEN;

  gtk_set_debug_flags (flags);
}

static void
init_touchscreen (GtkInspectorVisual *vis)
{
  gtk_switch_set_active (GTK_SWITCH (vis->priv->touchscreen_switch), (gtk_get_debug_flags () & GTK_DEBUG_TOUCHSCREEN) != 0);
  g_signal_connect (vis->priv->touchscreen_switch, "notify::active",
                    G_CALLBACK (update_touchscreen), NULL);

  if (g_getenv ("GTK_TEST_TOUCHSCREEN") != 0)
    {
      GtkWidget *row;

      /* hardcoded, nothing we can do */
      gtk_switch_set_active (GTK_SWITCH (vis->priv->touchscreen_switch), TRUE);
      gtk_widget_set_sensitive (vis->priv->touchscreen_switch, FALSE);
      row = gtk_widget_get_ancestor (vis->priv->touchscreen_switch, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("Setting is hardcoded by GTK_TEST_TOUCHSCREEN"));
    }
}

static gboolean
keynav_failed (GtkWidget *widget, GtkDirectionType direction, GtkInspectorVisual *vis)
{
  GtkWidget *next;
  gdouble value, lower, upper, page;

  if (direction == GTK_DIR_DOWN &&
      widget == vis->priv->visual_box)
    next = vis->priv->debug_box;
  else if (direction == GTK_DIR_DOWN &&
      widget == vis->priv->debug_box)
    next = vis->priv->misc_box;
  else if (direction == GTK_DIR_UP &&
           widget == vis->priv->debug_box)
    next = vis->priv->visual_box;
  else if (direction == GTK_DIR_UP &&
           widget == vis->priv->misc_box)
    next = vis->priv->debug_box;
  else
    next = NULL;

  if (next)
    {
      gtk_widget_child_focus (next, direction);
      return TRUE;
    }

  value = gtk_adjustment_get_value (vis->priv->focus_adjustment);
  lower = gtk_adjustment_get_lower (vis->priv->focus_adjustment);
  upper = gtk_adjustment_get_upper (vis->priv->focus_adjustment);
  page  = gtk_adjustment_get_page_size (vis->priv->focus_adjustment);

  if (direction == GTK_DIR_UP && value > lower)
    {
      gtk_adjustment_set_value (vis->priv->focus_adjustment, lower);
      return TRUE;
    }
  else if (direction == GTK_DIR_DOWN && value < upper - page)
    {
      gtk_adjustment_set_value (vis->priv->focus_adjustment, upper - page);
      return TRUE;
    }

  return FALSE;
}

static void
row_activated (GtkListBox         *box,
               GtkListBoxRow      *row,
               GtkInspectorVisual *vis)
{
  if (gtk_widget_is_ancestor (vis->priv->dark_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->dark_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->animation_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->animation_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->fps_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->fps_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->updates_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->updates_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->baselines_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->baselines_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->layout_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->layout_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->resize_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->resize_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->touchscreen_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->touchscreen_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->priv->software_gl_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->priv->software_gl_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
}

static void
init_gl (GtkInspectorVisual *vis)
{
  GdkDebugFlags flags = gdk_display_get_debug_flags (gdk_display_get_default ());

  gtk_switch_set_active (GTK_SWITCH (vis->priv->software_gl_switch), flags & GDK_DEBUG_GL_SOFTWARE);

  if (flags & GDK_DEBUG_GL_DISABLE)
    {
      GtkWidget *row;

      gtk_widget_set_sensitive (vis->priv->software_gl_switch, FALSE);
      row = gtk_widget_get_ancestor (vis->priv->software_gl_switch, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("GL rendering is disabled"));
    }
}

static void
update_gl_flag (GtkSwitch     *sw,
                GdkDebugFlags  flag)
{
  GdkDebugFlags flags = gdk_display_get_debug_flags (gdk_display_get_default ());

  if (gtk_switch_get_active (sw))
    flags |= flag;
  else
    flags &= ~flag;

  gdk_display_set_debug_flags (gdk_display_get_default (), flags);
}

static void
software_gl_activate (GtkSwitch *sw)
{
  update_gl_flag (sw, GDK_DEBUG_GL_SOFTWARE);
}

static void
gtk_inspector_visual_init (GtkInspectorVisual *vis)
{
  vis->priv = gtk_inspector_visual_get_instance_private (vis);
  gtk_widget_init_template (GTK_WIDGET (vis));
  init_direction (vis);
  init_theme (vis);
  init_dark (vis);
  init_icons (vis);
  init_cursors (vis);
  init_cursor_size (vis);
  init_font (vis);
  init_font_scale (vis);
  init_scale (vis);
  init_animation (vis);
  init_slowdown (vis);
  init_touchscreen (vis);
  init_gl (vis);
}

static void
gtk_inspector_visual_constructed (GObject *object)
{
  GtkInspectorVisual *vis = GTK_INSPECTOR_VISUAL (object);

  G_OBJECT_CLASS (gtk_inspector_visual_parent_class)->constructed (object);

  vis->priv->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (vis));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (vis))),
                                       vis->priv->focus_adjustment);

   g_signal_connect (vis->priv->visual_box, "keynav-failed", G_CALLBACK (keynav_failed), vis);
   g_signal_connect (vis->priv->debug_box, "keynav-failed", G_CALLBACK (keynav_failed), vis);
   g_signal_connect (vis->priv->misc_box, "keynav-failed", G_CALLBACK (keynav_failed), vis);
   g_signal_connect (vis->priv->visual_box, "row-activated", G_CALLBACK (row_activated), vis);
   g_signal_connect (vis->priv->debug_box, "row-activated", G_CALLBACK (row_activated), vis);
   g_signal_connect (vis->priv->misc_box, "row-activated", G_CALLBACK (row_activated), vis);
}

static void
gtk_inspector_visual_finalize (GObject *object)
{
  GtkInspectorVisual *vis = GTK_INSPECTOR_VISUAL (object);
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));

  if (vis->priv->layout_overlay)
    gtk_inspector_window_remove_overlay (iw, vis->priv->layout_overlay);
  if (vis->priv->updates_overlay)
    gtk_inspector_window_remove_overlay (iw, vis->priv->updates_overlay);
  if (vis->priv->fps_overlay)
    gtk_inspector_window_remove_overlay (iw, vis->priv->fps_overlay);

  G_OBJECT_CLASS (gtk_inspector_visual_parent_class)->finalize (object);
}

static void
gtk_inspector_visual_class_init (GtkInspectorVisualClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gtk_inspector_visual_constructed;
  object_class->finalize = gtk_inspector_visual_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/visual.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, direction_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, theme_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, dark_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, cursor_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, cursor_size_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, cursor_size_adjustment);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, icon_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, hidpi_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, scale_adjustment);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, animation_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, slowdown_adjustment);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, slowdown_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, touchscreen_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, visual_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, debug_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, font_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, misc_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, software_gl_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, font_scale_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, font_scale_adjustment);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, fps_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, updates_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, baselines_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, layout_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, resize_switch);

  gtk_widget_class_bind_template_callback (widget_class, fps_activate);
  gtk_widget_class_bind_template_callback (widget_class, updates_activate);
  gtk_widget_class_bind_template_callback (widget_class, direction_changed);
  gtk_widget_class_bind_template_callback (widget_class, baselines_activate);
  gtk_widget_class_bind_template_callback (widget_class, layout_activate);
  gtk_widget_class_bind_template_callback (widget_class, widget_resize_activate);
  gtk_widget_class_bind_template_callback (widget_class, software_gl_activate);
}

// vim: set et sw=2 ts=2:
