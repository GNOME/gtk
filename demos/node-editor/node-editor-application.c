/*
 * Copyright © 2019 Benjamin Otte
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

#include "node-editor-application.h"

#include "node-editor-window.h"

static const char *css =
"textview.editor {"
"  color: rgb(192, 197, 206);"
"  caret-color: currentColor;"
"}"
"textview.editor > text {"
"  background-color: rgb(43, 48, 59);"
"}"
;

struct _NodeEditorApplication
{
  GtkApplication parent;
};

struct _NodeEditorApplicationClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(NodeEditorApplication, node_editor_application, GTK_TYPE_APPLICATION);

static void
node_editor_application_init (NodeEditorApplication *app)
{
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       data)
{
  g_application_quit (G_APPLICATION (data));
}

static GActionEntry app_entries[] =
{
  { "quit", quit_activated, NULL, NULL, NULL }
};

static void
node_editor_application_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };
  const char *open_accels[2] = { "<Ctrl>O", NULL };
  GtkCssProvider *provider;

  G_APPLICATION_CLASS (node_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);


  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
node_editor_application_activate (GApplication *app)
{
  NodeEditorWindow *win;

  win = node_editor_window_new (NODE_EDITOR_APPLICATION (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
node_editor_application_open (GApplication  *app,
                              GFile        **files,
                              gint           n_files,
                              const gchar   *hint)
{
  NodeEditorWindow *win;
  gint i;

  for (i = 0; i < n_files; i++)
    {
      win = node_editor_window_new (NODE_EDITOR_APPLICATION (app));
      node_editor_window_load (win, files[i]);
      gtk_window_present (GTK_WINDOW (win));
    }
}

static void
node_editor_application_class_init (NodeEditorApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->startup = node_editor_application_startup;
  application_class->activate = node_editor_application_activate;
  application_class->open = node_editor_application_open;
}

NodeEditorApplication *
node_editor_application_new (void)
{
  return g_object_new (NODE_EDITOR_APPLICATION_TYPE,
                       "application-id", "org.gtk.gtk4.NodeEditor",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
