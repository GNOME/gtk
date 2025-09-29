/*
 * Copyright © 2025 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <glib/gstdio.h>

#include "icon-editor-application.h"
#include "icon-editor-window.h"
#include "fontify.h"
#include "profile_conf.h"


struct _IconEditorApplication
{
  GtkApplication parent;

  GtkWindow *help_window;
};

struct _IconEditorApplicationClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(IconEditorApplication, icon_editor_application, GTK_TYPE_APPLICATION);

/* {{{ Actions */

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkApplication *app = user_data;
  gboolean devel;
  g_autofree char *version;
  GtkAboutDialog *dialog;
  g_autofree char *path = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GtkIconPaintable) logo = NULL;

  devel = g_strcmp0 (PROFILE, "devel") == 0;
  version = g_strdup_printf ("%s%s%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             devel ? "-" : "",
                             devel ? VCS_TAG : "",
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  path = g_strconcat ("resource:///org/gtk/Shaper/",
                      g_application_get_application_id (G_APPLICATION (app)),
                      ".svg",
                      NULL);
  file = g_file_new_for_uri (path);
  logo = gtk_icon_paintable_new_for_file (file, 128, 1);
  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "transient-for", gtk_application_get_active_window (app),
                         "program-name", devel ? "Icon Editor (Development)"
                                               : "Icon Editor",
                         "version", version,
                         "copyright", "© 2025 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to edit icons",
                         "authors", (const char *[]){ "Matthias Clasen", NULL },
                         "artists", (const char *[]){ "Jakub Steiner", NULL },
                         "title", "About Icon Editor",
                         "logo", logo,
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
activate_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       data)
{
  g_application_quit (G_APPLICATION (data));
}

static void
activate_inspector (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
activate_help (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  IconEditorApplication *self = ICON_EDITOR_APPLICATION (user_data);
  g_autoptr (GtkBuilder) builder = NULL;
  GtkTextBuffer *buffer;
  g_autoptr (GBytes) bytes = NULL;
  const char *text;
  size_t length;

  if (!self->help_window)
    {
      builder = gtk_builder_new ();
      gtk_builder_add_from_resource (builder, "/org/gtk/Shaper/help-window.ui", NULL);
      self->help_window = GTK_WINDOW (gtk_builder_get_object (builder, "window"));
      buffer = GTK_TEXT_BUFFER (gtk_builder_get_object (builder, "buffer"));

      bytes = g_resources_lookup_data ("/org/gtk/Shaper/icon-format.md",
                                       G_RESOURCE_LOOKUP_FLAGS_NONE,
                                       NULL);

      text = g_bytes_get_data (bytes, &length);
      gtk_text_buffer_set_text (buffer, text, length);

      fontify ("markdown", buffer);
    }

  gtk_window_close (self->help_window);
  gtk_window_present (self->help_window);
}

static GActionEntry app_entries[] =
{
  { "about", activate_about, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "inspector", activate_inspector, NULL, NULL, NULL },
  { "help", activate_help, NULL, NULL, NULL },
};

/* }}} */
/* {{{ GApplication implementation */

static void
icon_editor_application_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };
  const char *open_accels[2] = { "<Ctrl>O", NULL };
  const char *close_accels[2] = { "<Ctrl>C", NULL };
  const char *save_accels[2] = { "<Ctrl>S", NULL };
  const char *help_accels[2] = { "<Ctrl>H", NULL };
  const char *about_accels[2] = { "<Ctrl>A", NULL };
  GtkCssProvider *provider;
  GtkSettings *settings;

  G_APPLICATION_CLASS (icon_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.close", close_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.save", save_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.help", help_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.about", about_accels);

  provider = gtk_css_provider_new ();
  settings = gtk_settings_get_default ();

  g_object_bind_property (settings, "gtk-interface-color-scheme",
                          provider, "prefers-color-scheme",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "gtk-interface-contrast",
                          provider, "prefers-contrast",
                          G_BINDING_SYNC_CREATE);

  gtk_css_provider_load_from_resource (provider, "/org/gtk/Shaper/icon-editor.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

}

static void
icon_editor_application_activate (GApplication *app)
{
  GList *list;
  IconEditorWindow *win;

  if ((list = gtk_application_get_windows (GTK_APPLICATION (app))) != NULL)
    {
      gtk_window_present (GTK_WINDOW (list->data));
      return;
    }

  win = icon_editor_window_new (ICON_EDITOR_APPLICATION (app));

  if (g_strcmp0 (PROFILE, "devel") == 0)
    gtk_widget_add_css_class (GTK_WIDGET (win), "devel");

  gtk_window_present (GTK_WINDOW (win));
}

static void
icon_editor_application_open (GApplication  *app,
                              GFile        **files,
                              int            n_files,
                              const char    *hint)
{
  IconEditorWindow *win;
  int i;

  for (i = 0; i < n_files; i++)
    {
      win = icon_editor_window_new (ICON_EDITOR_APPLICATION (app));

      if (g_strcmp0 (PROFILE, "devel") == 0)
        gtk_widget_add_css_class (GTK_WIDGET (win), "devel");

      icon_editor_window_load (win, files[i]);
      gtk_window_present (GTK_WINDOW (win));
    }
}

/* }}} */
/* {{{ GObject boilerplate */

static void
icon_editor_application_init (IconEditorApplication *app)
{
}

static void
icon_editor_application_dispose (GObject *object)
{
  IconEditorApplication *self = ICON_EDITOR_APPLICATION (object);

  gtk_window_destroy (GTK_WINDOW (self->help_window));

  G_OBJECT_CLASS (icon_editor_application_parent_class)->dispose (object);
}

static void
icon_editor_application_class_init (IconEditorApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = icon_editor_application_dispose;

  application_class->startup = icon_editor_application_startup;
  application_class->activate = icon_editor_application_activate;
  application_class->open = icon_editor_application_open;
}

/* }}} */
/* {{{ Public API */

IconEditorApplication *
icon_editor_application_new (void)
{
  IconEditorApplication *app;
  char version[80];
  gboolean devel;

  devel = g_strcmp0 (PROFILE, "devel") == 0;
  g_snprintf (version, sizeof (version), "%s%s%s",
              PACKAGE_VERSION,
              devel ? "-" : "",
              devel ? VCS_TAG : "");

  app = g_object_new (ICON_EDITOR_APPLICATION_TYPE,
                      "application-id", "org.gtk.Shaper",
                      "resource-base-path", "/org/gtk/Shaper",
                      "flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE,
                      "version", version,
                      NULL);

  return app;
}

/* }}} */

/* vim:set foldmethod=marker: */
