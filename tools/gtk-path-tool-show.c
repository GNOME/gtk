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
show_path (GskPath       *path1,
           GskPath       *path2,
           gboolean       do_fill,
           GskFillRule    fill_rule,
           GskStroke     *stroke,
           const GdkRGBA *fg_color,
           const GdkRGBA *bg_color,
           const GdkRGBA *point_color,
           const GdkRGBA *intersection_color,
           gboolean       show_points,
           gboolean       show_controls,
           gboolean       show_intersections,
           double         zoom)
{
  GtkWidget *window, *sw, *child;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), _("Path Preview"));

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_window_set_child (GTK_WINDOW (window), sw);

  child = g_object_new (PATH_TYPE_VIEW, NULL);
  g_object_set (child,
                "path1", path1,
                "path2", path2,
                "do-fill", do_fill,
                "fill-rule", fill_rule,
                "stroke", stroke,
                "fg-color", fg_color,
                "bg-color", bg_color,
                "point-color", point_color,
                "intersection-color", intersection_color,
                "show-points", show_points,
                "show-controls", show_controls,
                "show-intersections", show_intersections,
                "zoom", zoom,
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
  gboolean do_fill = TRUE;
  gboolean show_points = FALSE;
  gboolean show_controls = FALSE;
  gboolean show_intersections = FALSE;
  const char *fill = "winding";
  const char *fg_color = "black";
  const char *bg_color = "white";
  const char *point_color = "red";
  const char *intersection_color = "lightgreen";
  double zoom = 1;
  double line_width = 1;
  const char *cap = "butt";
  const char *join = "miter";
  double miter_limit = 4;
  const char *dashes = NULL;
  double dash_offset = 0;
  char **args = NULL;
  GOptionContext *context;
  GOptionGroup *options;
  const GOptionEntry entries[] = {
    { "fill", 0, 0, G_OPTION_ARG_NONE, &do_fill, N_("Fill the path (the default)"), NULL },
    { "stroke", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &do_fill, N_("Stroke the path"), NULL },
    { "points", 0, 0, G_OPTION_ARG_NONE, &show_points, N_("Show path points"), NULL },
    { "controls", 0, 0, G_OPTION_ARG_NONE, &show_controls, N_("Show control points"), NULL },
    { "intersections", 0, 0, G_OPTION_ARG_NONE, &show_intersections, N_("Show intersections"), NULL },
    { "fg-color", 0, 0, G_OPTION_ARG_STRING, &fg_color, N_("Foreground color"), N_("COLOR") },
    { "bg-color", 0, 0, G_OPTION_ARG_STRING, &bg_color, N_("Background color"), N_("COLOR") },
    { "point-color", 0, 0, G_OPTION_ARG_STRING, &point_color, N_("Point color"), N_("COLOR") },
    { "intersection-color", 0, 0, G_OPTION_ARG_STRING, &intersection_color, N_("Intersection color"), N_("COLOR") },
    { "zoom", 0, 0, G_OPTION_ARG_DOUBLE, &zoom, N_("Zoom level (number)"), N_("VALUE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, NULL, N_("PATH…") },
    { NULL, }
  };
  const GOptionEntry fill_entries[] = {
    { "fill-rule", 0, 0, G_OPTION_ARG_STRING, &fill, N_("Fill rule (winding, even-odd)"), N_("VALUE") },
    { NULL, }
  };
  const GOptionEntry stroke_entries[] = {
    { "line-width", 0, 0, G_OPTION_ARG_DOUBLE, &line_width, N_("Line width (number)"), N_("VALUE") },
    { "line-cap", 0, 0, G_OPTION_ARG_STRING, &cap, N_("Line cap (butt, round, square)"), N_("VALUE") },
    { "line-join", 0, 0, G_OPTION_ARG_STRING, &join, N_("Line join (miter, miter-clip, round, bevel)"), N_("VALUE") },
    { "miter-limit", 0, 0, G_OPTION_ARG_DOUBLE, &miter_limit, N_("Miter limit (number)"), N_("VALUE") },
    { "dashes", 0, 0, G_OPTION_ARG_STRING, &dashes, N_("Dash pattern (comma-separated numbers)"), N_("VALUE") },
    { "dash-offset", 0, 0, G_OPTION_ARG_DOUBLE, &dash_offset, N_("Dash offset (number)"), N_("VALUE") },
    { NULL, }
  };
  GskPath *path1;
  GskPath *path2;
  GskFillRule fill_rule;
  GdkRGBA fg, bg, pc, ic;
  GskLineCap line_cap;
  GskLineJoin line_join;
  GskStroke *stroke;

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

  options = g_option_group_new ("fill",
                                _("Options related to filling"),
                                _("Show help for fill options"),
                                NULL, NULL);
  g_option_group_add_entries (options, fill_entries);
  g_option_group_set_translation_domain (options, GETTEXT_PACKAGE);
  g_option_context_add_group (context, options);

  options = g_option_group_new ("stroke",
                                _("Options related to stroking"),
                                _("Show help for stroke options"),
                                NULL, NULL);
  g_option_group_add_entries (options, stroke_entries);
  g_option_group_set_translation_domain (options, GETTEXT_PACKAGE);
  g_option_context_add_group (context, options);

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

  if (g_strv_length (args) > 2)
    {
      g_printerr ("%s\n", _("Can only show one or two paths"));
      exit (1);
    }

  path1 = get_path (args[0]);
  if (g_strv_length (args) > 1)
    path2 = get_path (args[1]);
  else
    path2 = NULL;

  fill_rule = get_enum_value (GSK_TYPE_FILL_RULE, _("fill rule"), fill);
  get_color (&fg, fg_color);
  get_color (&bg, bg_color);
  get_color (&pc, point_color);
  get_color (&ic, intersection_color);

  line_cap = get_enum_value (GSK_TYPE_LINE_CAP, _("line cap"), cap);
  line_join = get_enum_value (GSK_TYPE_LINE_JOIN, _("line join"), join);

  stroke = gsk_stroke_new (line_width);
  gsk_stroke_set_line_cap (stroke, line_cap);
  gsk_stroke_set_line_join (stroke, line_join);
  gsk_stroke_set_miter_limit (stroke, miter_limit);
  gsk_stroke_set_dash_offset (stroke, dash_offset);
  _gsk_stroke_set_dashes (stroke, dashes);

  show_path (path1, path2,
             do_fill,
             fill_rule,
             stroke,
             &fg, &bg, &pc, &ic,
             show_points,
             show_controls,
             show_intersections,
             zoom);

  g_clear_pointer (&path1, gsk_path_unref);
  g_clear_pointer (&path2, gsk_path_unref);
  gsk_stroke_free (stroke);

  g_strfreev (args);
}
