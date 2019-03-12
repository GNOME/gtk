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
  const gchar *quit_accels[2] = { "<Ctrl>Q", NULL };

  G_APPLICATION_CLASS (node_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.quit",
                                         quit_accels);
}

static void
node_editor_application_activate (GApplication *app)
{
  NodeEditorWindow *win;

  win = node_editor_window_new (NODE_EDITOR_APPLICATION (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
node_editor_application_class_init (NodeEditorApplicationClass *class)
{
  G_APPLICATION_CLASS (class)->startup = node_editor_application_startup;
  G_APPLICATION_CLASS (class)->activate = node_editor_application_activate;
}

NodeEditorApplication *
node_editor_application_new (void)
{
  return g_object_new (NODE_EDITOR_APPLICATION_TYPE,
                       "application-id", "org.gtk.gtk4.NodeEditor",
                       NULL);
}
