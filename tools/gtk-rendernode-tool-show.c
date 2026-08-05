/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-rendernode-tool.h"
#include "gtk-tool-utils.h"


static void
set_window_title (GtkWindow  *window,
                  const char *filename)
{
  char *name;

  name = g_path_get_basename (filename);
  gtk_window_set_title (window, name);
  g_free (name);
}

static void
show_node (GskRenderNode *node,
           const char    *filename,
           gboolean       decorated,
           gboolean       offload)
{
  graphene_rect_t node_bounds;
  GdkPaintable *paintable;
  GtkWidget *sw;
  GtkWidget *handle;
  GtkWidget *window;
  GtkSnapshot *snapshot;
  GtkWidget *picture;

  gsk_render_node_get_bounds (node, &node_bounds);

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- node_bounds.origin.x, - node_bounds.origin.y));
  gtk_snapshot_append_node (snapshot, node);
  paintable = gtk_snapshot_free_to_paintable (snapshot, NULL);

  picture = gtk_picture_new_for_paintable (paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
  gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_SCALE_DOWN);
  gtk_picture_set_isolate_contents (GTK_PICTURE (picture), decorated);

  if (offload)
    picture = gtk_graphics_offload_new (picture);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), picture);

  handle = gtk_window_handle_new ();
  gtk_window_handle_set_child (GTK_WINDOW_HANDLE (handle), sw);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), decorated);
  gtk_window_set_resizable (GTK_WINDOW (window), decorated);
  if (!decorated)
    gtk_widget_remove_css_class (window, "background");
  if (filename)
    set_window_title (GTK_WINDOW (window), filename);
  gtk_window_set_child (GTK_WINDOW (window), handle);

  gtk_window_present (GTK_WINDOW (window));
  gtk_tool_inhibit ();
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_tool_uninhibit), NULL);

  g_clear_object (&paintable);
}

void
do_show (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  gboolean decorate = FALSE;
  gboolean offload = FALSE;
  const GOptionEntry entries[] = {
    { "offload", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &offload, N_("Put node into offload container"), NULL },
    { "decorate", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &decorate, N_("Add a titlebar"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GskRenderNode *node;
  GError *error = NULL;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-rendernode-tool show");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Show the render node."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No .node file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 1)
    {
      g_printerr (_("Can only preview a single .node file\n"));
      exit (1);
    }

  node = load_node_file (filenames[0]);

  show_node (node, filenames[0], decorate, offload);

  gtk_tool_run ();

  g_strfreev (filenames);
  g_clear_pointer (&node, gsk_render_node_unref);
}

GskRenderNode *
filter_show (GskRenderNode  *node,
             int             argc,
             const char    **argv)
{
  GOptionContext *context;
  gboolean decorate = FALSE;
  gboolean offload = FALSE;
  const GOptionEntry entries[] = {
    { "offload", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &offload, N_("Put node into offload container"), NULL },
    { "decorate", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &decorate, N_("Add a titlebar"), NULL },
    { NULL, }
  };
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Show the render node."));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("show: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  show_node (node, NULL, decorate, offload);

  return node;
}
