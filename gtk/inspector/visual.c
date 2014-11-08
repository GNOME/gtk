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

#include "gtkadjustment.h"
#include "gtkbox.h"
#include "gtkcomboboxtext.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkswitch.h"
#include "gtkwindow.h"
#include "gtkcssproviderprivate.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#include "gdk/gdkinternals.h"

struct _GtkInspectorVisualPrivate
{
  GtkWidget *visual_box;
  GtkWidget *theme_combo;
  GtkWidget *dark_switch;
  GtkWidget *icon_combo;
  GtkWidget *direction_combo;
  GtkWidget *font_button;
  GtkWidget *hidpi_spin;
  GtkWidget *animation_switch;
  GtkAdjustment *scale_adjustment;

  GtkWidget *debug_box;
  GtkWidget *updates_switch;
  GtkWidget *baselines_switch;
  GtkWidget *pixelcache_switch;
  GtkWidget *touchscreen_switch;

  GtkWidget *gl_box;
  GtkWidget *gl_combo;
  GtkWidget *software_gl_switch;
  GtkWidget *software_surface_switch;
  GtkWidget *texture_rectangle_switch;

  GtkAdjustment *focus_adjustment;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorVisual, gtk_inspector_visual, GTK_TYPE_SCROLLED_WINDOW)

static void
fix_direction_recurse (GtkWidget *widget,
                       gpointer   data)
{
  GtkTextDirection dir = GPOINTER_TO_INT (data);

  g_object_ref (widget);

  gtk_widget_set_direction (widget, dir);
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), fix_direction_recurse, data);

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

  iw = gtk_widget_get_toplevel (GTK_WIDGET (combo));
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

static void
updates_activate (GtkSwitch *sw)
{
  gdk_window_set_debug_updates (gtk_switch_get_active (sw));
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
pixelcache_activate (GtkSwitch *sw)
{
  guint flags;

  flags = gtk_get_debug_flags ();

  if (gtk_switch_get_active (sw))
    flags |= GTK_DEBUG_PIXEL_CACHE;
  else
    flags &= ~GTK_DEBUG_PIXEL_CACHE;

  gtk_set_debug_flags (flags);
  /* FIXME: this doesn't work, because it is redrawing
   * _from_ the cache. We need to recurse over the tree
   * and invalidate the pixel cache of every widget that
   * has one.
   */
  redraw_everything ();
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
  gchar *theme, *current_theme, *path;
  gint i, pos;
  GSettings *settings;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  /* Builtin themes */
  g_hash_table_add (t, g_strdup ("Adwaita"));
  g_hash_table_add (t, g_strdup ("HighContrast"));
  g_hash_table_add (t, g_strdup ("Raleigh"));

  path = _gtk_css_provider_get_theme_dir ();
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_home_dir (), ".themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  settings = g_settings_new ("org.gnome.desktop.interface");
  current_theme = g_settings_get_string (settings, "gtk-theme");
  g_object_unref (settings);

  g_hash_table_iter_init (&iter, t);
  pos = i = 0;
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    {
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vis->priv->theme_combo), theme);
      if (g_strcmp0 (theme, current_theme) == 0)
        pos = i;
      i++;
    }
  g_hash_table_destroy (t);

  gtk_combo_box_set_active (GTK_COMBO_BOX (vis->priv->theme_combo), pos);

  if (g_getenv ("GTK_THEME") != NULL)
    {
      /* theme is hardcoded, nothing we can do */
      gtk_widget_set_sensitive (vis->priv->theme_combo, FALSE);
      gtk_widget_set_tooltip_text (vis->priv->theme_combo , _("Theme is hardcoded by GTK_THEME"));
    }
}

static void
theme_changed (GtkComboBox        *c,
               GtkInspectorVisual *vis)
{
  gchar *theme;

  theme = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (c));
  g_object_set (gtk_settings_get_default (), "gtk-theme-name", theme, NULL);
  g_free (theme);
}

static void
init_dark (GtkInspectorVisual *vis)
{
  g_object_bind_property (gtk_settings_get_default (), "gtk-application-prefer-dark-theme",
                          vis->priv->dark_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  if (g_getenv ("GTK_THEME") != NULL)
    {
      /* theme is hardcoded, nothing we can do */
      gtk_widget_set_sensitive (vis->priv->dark_switch, FALSE);
      gtk_widget_set_tooltip_text (vis->priv->dark_switch, _("Theme is hardcoded by GTK_THEME"));
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
}

static void
init_icons (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *current_theme, *path;
  gint i, pos;
  GSettings *settings;
  gchar *iconsdir = get_data_path ("icons");

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  fill_icons (iconsdir, t);

  g_free (iconsdir);

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
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vis->priv->icon_combo), theme);
      if (g_strcmp0 (theme, current_theme) == 0)
        pos = i;
      i++;
    }
  g_hash_table_destroy (t);

  gtk_combo_box_set_active (GTK_COMBO_BOX (vis->priv->icon_combo), pos);
}

static void
icons_changed (GtkComboBox        *c,
               GtkInspectorVisual *vis)
{
  gchar *theme = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (c));
  g_object_set (gtk_settings_get_default (), "gtk-icon-theme-name", theme, NULL);
  g_free (theme);
}

static void
init_font (GtkInspectorVisual *vis)
{
  g_object_bind_property (gtk_settings_get_default (), "gtk-font-name",
                          vis->priv->font_button, "font-name",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
}

#if defined (GDK_WINDOWING_X11) && defined (HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE)
static void
scale_changed (GtkAdjustment *adjustment, GtkInspectorVisual *vis)
{
  GdkDisplay *display;
  gint scale;

  scale = gtk_adjustment_get_value (adjustment);
  display = gdk_display_get_default ();
  gdk_x11_display_set_window_scale (display, scale);
}
#endif

static void
init_scale (GtkInspectorVisual *vis)
{
#if defined (GDK_WINDOWING_X11) && defined (HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE)
  GdkScreen *screen;

  screen = gdk_screen_get_default ();
  if (GDK_IS_X11_SCREEN (screen))
    {
      gdouble scale;

      scale = gdk_screen_get_monitor_scale_factor (screen, 0);
      gtk_adjustment_set_value (vis->priv->scale_adjustment, scale);
      g_signal_connect (vis->priv->scale_adjustment, "value-changed",
                        G_CALLBACK (scale_changed), vis);
    }
  else
#endif
    {
      gtk_adjustment_set_value (vis->priv->scale_adjustment, 1);
      gtk_widget_set_sensitive (vis->priv->hidpi_spin, FALSE);
      gtk_widget_set_tooltip_text (vis->priv->hidpi_spin,
                                   _("Backend does not support window scaling"));
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
      /* hardcoded, nothing we can do */
      gtk_switch_set_active (GTK_SWITCH (vis->priv->touchscreen_switch), TRUE);
      gtk_widget_set_sensitive (vis->priv->touchscreen_switch, FALSE);
      gtk_widget_set_tooltip_text (vis->priv->touchscreen_switch, _("Setting is hardcoded by GTK_TEST_TOUCHSCREEN"));
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
    next = vis->priv->gl_box;
  else if (direction == GTK_DIR_UP &&
           widget == vis->priv->debug_box)
    next = vis->priv->visual_box;
  else if (direction == GTK_DIR_UP &&
           widget == vis->priv->gl_box)
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
init_gl (GtkInspectorVisual *vis)
{
  GdkGLFlags flags;

  flags = gdk_gl_get_flags ();

  if (flags & GDK_GL_ALWAYS)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (vis->priv->gl_combo), "always");
  else if (flags & GDK_GL_DISABLE)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (vis->priv->gl_combo), "disable");
  else
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (vis->priv->gl_combo), "maybe");
  gtk_widget_set_sensitive (vis->priv->gl_combo, FALSE);
  gtk_widget_set_tooltip_text (vis->priv->gl_combo,
                               _("Not settable at runtime.\nUse GDK_GL=always or GDK_GL=disable instead"));

  gtk_switch_set_active (GTK_SWITCH (vis->priv->software_gl_switch),
                         flags & GDK_GL_SOFTWARE_DRAW_GL);
  gtk_switch_set_active (GTK_SWITCH (vis->priv->software_surface_switch),
                         flags & GDK_GL_SOFTWARE_DRAW_SURFACE);
  gtk_switch_set_active (GTK_SWITCH (vis->priv->texture_rectangle_switch),
                         flags & GDK_GL_TEXTURE_RECTANGLE);

  if (flags & GDK_GL_DISABLE)
    {
      gtk_widget_set_sensitive (vis->priv->software_gl_switch, FALSE);
      gtk_widget_set_sensitive (vis->priv->software_surface_switch, FALSE);
      gtk_widget_set_sensitive (vis->priv->texture_rectangle_switch, FALSE);
      gtk_widget_set_tooltip_text (vis->priv->software_gl_switch, _("GL rendering is disabled"));
      gtk_widget_set_tooltip_text (vis->priv->software_surface_switch, _("GL rendering is disabled"));
      gtk_widget_set_tooltip_text (vis->priv->texture_rectangle_switch, _("GL rendering is disabled"));
    }
}

static void
update_gl_flag (GtkSwitch  *sw,
                GdkGLFlags  flag)
{
  GdkGLFlags flags;

  flags = gdk_gl_get_flags ();

  if (gtk_switch_get_active (sw))
    flags |= flag;
  else
    flags &= ~flag;

  gdk_gl_set_flags (flags);
}

static void
software_gl_activate (GtkSwitch *sw)
{
  update_gl_flag (sw, GDK_GL_SOFTWARE_DRAW_GL);
}

static void
software_surface_activate (GtkSwitch *sw)
{
  update_gl_flag (sw, GDK_GL_SOFTWARE_DRAW_SURFACE);
}

static void
texture_rectangle_activate (GtkSwitch *sw)
{
  update_gl_flag (sw, GDK_GL_TEXTURE_RECTANGLE);
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
  init_font (vis);
  init_scale (vis);
  init_animation (vis);
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
   g_signal_connect (vis->priv->gl_box, "keynav-failed", G_CALLBACK (keynav_failed), vis);
}

static void
gtk_inspector_visual_class_init (GtkInspectorVisualClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gtk_inspector_visual_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/visual.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, updates_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, direction_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, baselines_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, pixelcache_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, dark_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, theme_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, icon_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, hidpi_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, animation_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, scale_adjustment);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, touchscreen_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, visual_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, debug_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, font_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, gl_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, gl_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, software_gl_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, software_surface_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorVisual, texture_rectangle_switch);

  gtk_widget_class_bind_template_callback (widget_class, updates_activate);
  gtk_widget_class_bind_template_callback (widget_class, direction_changed);
  gtk_widget_class_bind_template_callback (widget_class, baselines_activate);
  gtk_widget_class_bind_template_callback (widget_class, pixelcache_activate);
  gtk_widget_class_bind_template_callback (widget_class, theme_changed);
  gtk_widget_class_bind_template_callback (widget_class, icons_changed);
  gtk_widget_class_bind_template_callback (widget_class, software_gl_activate);
  gtk_widget_class_bind_template_callback (widget_class, software_surface_activate);
  gtk_widget_class_bind_template_callback (widget_class, texture_rectangle_activate);
}

// vim: set et sw=2 ts=2:
