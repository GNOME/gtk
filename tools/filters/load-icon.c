/*  Copyright 2025 Red Hat, Inc.
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
#include "rntcontext.h"

static GskRenderNode *
icon_load (const char    *filename,
           int            size,
           int            scale,
           int            width,
           int            height,
           const GdkRGBA *colors,
           gsize          n_colors)
{
  GFile *file;
  GtkIconPaintable *paintable;
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  file = g_file_new_for_commandline_arg (filename);
  paintable = gtk_icon_paintable_new_for_file (file, size, scale);

  snapshot = gtk_snapshot_new ();

  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                            snapshot,
                                            width, height,
                                            colors, n_colors);

  node = gtk_snapshot_free_to_node (snapshot);

  g_object_unref (paintable);
  g_object_unref (file);

  return node;
}

GskRenderNode *
filter_load_icon (GskRenderNode  *node,
                  RntContext     *ctx,
                  int             argc,
                  const char    **argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  gboolean recolor = FALSE;
  const GdkRGBA fg_default = { 0.7450980392156863, 0.7450980392156863, 0.7450980392156863, 1.0};
  const GdkRGBA success_default = { 0.3046921492332342,0.6015716792553597, 0.023437857633325704, 1.0};
  const GdkRGBA warning_default = {0.9570458533607996, 0.47266346227206835, 0.2421911955443656, 1.0 };
  const GdkRGBA error_default = { 0.796887159533074, 0 ,0, 1.0 };
  GdkRGBA colors[4] = {
    [GTK_SYMBOLIC_COLOR_FOREGROUND] = { 0, 0, 0, 1 },
    [GTK_SYMBOLIC_COLOR_ERROR] =      { 0, 0, 1, 1 },
    [GTK_SYMBOLIC_COLOR_WARNING] =    { 0, 1, 0, 1 },
    [GTK_SYMBOLIC_COLOR_SUCCESS] =    { 1, 0, 0, 1 },
  };
  char *fc = NULL;
  char *sc = NULL;
  char *wc = NULL;
  char *ec = NULL;
  char *resize = NULL;
  guint64 width = 0;
  guint64 height = 0;
  int size = 16;
  int scale = 1;
  const GOptionEntry entries[] = {
    { "recolor", 0, 0, G_OPTION_ARG_NONE, &recolor, N_("Recolor the node"), NULL },
    { "fg", 0, 0, G_OPTION_ARG_STRING, &fc, N_("Foreground color"), N_("COLOR") },
    { "success", 0, 0, G_OPTION_ARG_STRING, &sc, N_("Success color"), N_("COLOR") },
    { "warning", 0, 0, G_OPTION_ARG_STRING, &wc, N_("Warning color"), N_("COLOR") },
    { "error", 0, 0, G_OPTION_ARG_STRING, &ec, N_("Error color"), N_("COLOR") },
    { "resize", 0, 0, G_OPTION_ARG_STRING, &resize, N_("Resize"), N_("SIZE") },
    { "scale", 0, 0, G_OPTION_ARG_INT, &scale, N_("Resize"), N_("NUM") },
    { "size", 0, 0, G_OPTION_ARG_INT, &size, N_("Size"), N_("NUM") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;
  gsize i;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Load icons from filenames"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (recolor)
    {
      colors[GTK_SYMBOLIC_COLOR_FOREGROUND] = fg_default;
      colors[GTK_SYMBOLIC_COLOR_SUCCESS] = success_default;
      colors[GTK_SYMBOLIC_COLOR_WARNING] = warning_default;
      colors[GTK_SYMBOLIC_COLOR_ERROR] = error_default;
      if (fc && !gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_FOREGROUND], fc))
        {
          g_printerr ("Failed to parse as color: %s\n", fc);
          exit (1);
        }
      if (sc && !gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_SUCCESS], sc))
        {
          g_printerr ("Failed to parse as color: %s\n", fc);
          exit (1);
        }
      if (wc && !gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_WARNING], wc))
        {
          g_printerr ("Failed to parse as color: %s\n", fc);
          exit (1);
        }
      if (ec && !gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_ERROR], ec))
        {
          g_printerr ("Failed to parse as color: %s\n", fc);
          exit (1);
        }
    }

  if (resize)
    {
      char *p = strchr (resize, 'x');
      if (p)
        {
          *p = '\0';
          p++;

          if (!g_ascii_string_to_unsigned (resize, 10, 1, 1024, &width, &error) ||
              !g_ascii_string_to_unsigned (p, 10, 1, 1024, &height, &error))
            {
              g_printerr ("%s\n", error->message);
              g_error_free (error);
              exit (1);
            }
        }
      else
        {
          if (!g_ascii_string_to_unsigned (resize, 10, 1, 1024, &width, &error))
            {
              g_printerr ("%s\n", error->message);
              g_error_free (error);
              exit (1);
            }

          height = width;
        }
    }
  else
    {
      width = size;
      height = size;
    }

  if (filenames == NULL)
    {
      g_printerr (_("No filename specified\n"));
      exit (1);
    }

  for (i = g_strv_length (filenames); i-- > 0;)
    {
      if (node != rnt_context_get_null_node (ctx))
        rnt_context_push_node (ctx, node);
      node = icon_load (filenames[i], size, scale, width, height, colors, 4);
    }

  g_strfreev (filenames);

  return node;
}
