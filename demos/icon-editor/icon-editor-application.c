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

#include "profile_conf.h"


struct _IconEditorApplication
{
  GtkApplication parent;
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
  char *version;
  GtkWidget *dialog;

  version = g_strdup_printf ("%s%s%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
                             g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "",
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "transient-for", gtk_application_get_active_window (app),
                         "program-name", g_strcmp0 (PROFILE, "devel") == 0
                                         ? "GTK Icon Editor (Development)"
                                         : "GTK Icon Editor",
                         "version", version,
                         "copyright", "© 2025 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to edit icons",
                         "authors", (const char *[]){ "Matthias Clasen", NULL },
                         "title", "About GTK Icon Editor",
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));

  g_free (version);
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
  GtkBuilder *builder;
  GtkWidget *window;
  GtkTextBuffer *buffer;
  GBytes *bytes;
  const char *text;
  gsize len;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gtk/icon-editor/help-window.ui", NULL);
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  buffer = GTK_TEXT_BUFFER (gtk_builder_get_object (builder, "buffer"));

  bytes = g_resources_lookup_data ("/org/gtk/icon-editor/icon-format.md",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   NULL);
  text = g_bytes_get_data (bytes, &len);
  gtk_text_buffer_set_text (buffer, text, len);
  g_bytes_unref (bytes);

  gtk_window_present (GTK_WINDOW (window));
  g_object_unref (builder);
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
  const char *save_accels[2] = { "<Ctrl>S", NULL };
  GtkCssProvider *provider;
  GtkSettings *settings;

  G_APPLICATION_CLASS (icon_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.save", save_accels);

  provider = gtk_css_provider_new ();
  settings = gtk_settings_get_default ();

  g_object_bind_property (settings, "gtk-interface-color-scheme",
                          provider, "prefers-color-scheme",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "gtk-interface-contrast",
                          provider, "prefers-contrast",
                          G_BINDING_SYNC_CREATE);

  gtk_css_provider_load_from_resource (provider, "/org/gtk/icon-editor/icon-editor.css");
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
icon_editor_application_class_init (IconEditorApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

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

  g_snprintf (version, sizeof (version), "%s%s%s\n",
              PACKAGE_VERSION,
              g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
              g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "");

  app = g_object_new (ICON_EDITOR_APPLICATION_TYPE,
                      "application-id", "org.gtk.IconEditor",
                      "flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE,
                      "version", version,
                      NULL);

  return app;
}

/* }}} */

/* vim:set foldmethod=marker: */
