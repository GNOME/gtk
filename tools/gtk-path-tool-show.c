/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
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
#include "gtk-path-tool.h"

#include "path-view.h"

static void
show_path (GskPath       *path,
           GskFillRule    fill_rule,
           const GdkRGBA *fg_color,
           const GdkRGBA *bg_color)
{
  GtkWidget *window, *sw, *child;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), _("Path Preview"));

//  gtk_window_set_default_size (GTK_WINDOW (window), 700, 500);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_window_set_child (GTK_WINDOW (window), sw);

  child = path_view_new (path);
  g_object_set (child,
                "fill-rule", fill_rule,
                "fg-color", fg_color,
                "bg-color", bg_color,
                NULL);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), child);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);
}

void
do_show (int          *argc,
         const char ***argv)
{
  GError *error = NULL;
  const char *fill = "winding";
  const char *fg_color = "black";
  const char *bg_color = "white";
  char **args = NULL;
  GOptionContext *context;
  const GOptionEntry entries[] = {
    { "fill-rule", 0, 0, G_OPTION_ARG_STRING, &fill, N_("Fill rule (winding, even-odd)"), N_("VALUE") },
    { "fg-color", 0, 0, G_OPTION_ARG_STRING, &fg_color, N_("Foreground color"), N_("COLOR") },
    { "bg-color", 0, 0, G_OPTION_ARG_STRING, &bg_color, N_("Background color"), N_("COLOR") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, NULL, N_("PATH") },
    { NULL, }
  };
  GskPath *path;
  GskFillRule fill_rule;
  GdkRGBA fg;
  GdkRGBA bg;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr ("%s\n", _("Could not initialize windowing system"));
      exit (1);
    }

  g_set_prgname ("gtk4-path-tool show");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Display the path."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (args == NULL)
    {
      g_printerr ("%s\n", _("No path specified"));
      exit (1);
    }

  if (g_strv_length (args) > 1)
    {
      g_printerr ("%s\n", _("Can only show a single path"));
      exit (1);
    }

  path = get_path (args[0]);

  fill_rule = get_enum_value (GSK_TYPE_FILL_RULE, _("fill rule"), fill);
  get_color (&fg, fg_color);
  get_color (&bg, bg_color);

  show_path (path, fill_rule, &fg, &bg);

  gsk_path_unref (path);

  g_strfreev (args);
}
