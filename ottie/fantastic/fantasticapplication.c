/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "fantasticapplication.h"

#include "fantasticwindow.h"

struct _FantasticApplication
{
  GtkApplication parent;
};

struct _FantasticApplicationClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(FantasticApplication, fantastic_application, GTK_TYPE_APPLICATION);

static void
fantastic_application_init (FantasticApplication *app)
{
}

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkApplication *app = user_data;
  char *version;
  GString *s;
  char *os_name;
  char *os_version;
  GtkWidget *dialog;

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  os_version = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  s = g_string_new ("");
  if (os_name && os_version)
    g_string_append_printf (s, "OS\t%s %s\n\n", os_name, os_version);

  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (s, "\tPango\t%s\n",
                          pango_version_string ());
  g_string_append_printf (s, "\tGTK\t%d.%d.%d\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());

  version = g_strdup_printf ("%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "transient-for", gtk_application_get_active_window (app),
                         "program-name", "Fantastic",
                         "version", version,
                         "copyright", "© 2020 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Edit Lottie files",
                         "authors", (const char *[]){ "Benjamin Otte", NULL},
                         "logo-icon-name", "org.gtk.gtk4.Fantastic.Devel",
                         "title", "About Fantastic",
                         "system-information", s->str,
                         NULL);
  gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog),
                                       "Artwork by", (const char *[]) { "Jakub Steiner", NULL });

  gtk_window_present (GTK_WINDOW (dialog));

  g_string_free (s, TRUE);
  g_free (version);
  g_free (os_name);
  g_free (os_version);
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

static GActionEntry app_entries[] =
{
  { "about", activate_about, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "inspector", activate_inspector, NULL, NULL, NULL },
};

static void
fantastic_application_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };
  const char *open_accels[2] = { "<Ctrl>O", NULL };

  G_APPLICATION_CLASS (fantastic_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);
}

static void
fantastic_application_activate (GApplication *app)
{
  FantasticWindow *win;

  win = fantastic_window_new (FANTASTIC_APPLICATION (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
fantastic_application_open (GApplication  *app,
                              GFile        **files,
                              int            n_files,
                              const char    *hint)
{
  FantasticWindow *win;
  int i;

  for (i = 0; i < n_files; i++)
    {
      win = fantastic_window_new (FANTASTIC_APPLICATION (app));
      fantastic_window_load (win, files[i]);
      gtk_window_present (GTK_WINDOW (win));
    }
}

static void
fantastic_application_class_init (FantasticApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->startup = fantastic_application_startup;
  application_class->activate = fantastic_application_activate;
  application_class->open = fantastic_application_open;
}

FantasticApplication *
fantastic_application_new (void)
{
  return g_object_new (FANTASTIC_APPLICATION_TYPE,
                       "application-id", "org.gtk.gtk4.Fantastic",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
