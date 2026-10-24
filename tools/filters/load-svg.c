/*  Copyright 2026 Red Hat, Inc.
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

#include "rntcontext.h"
#include "rntfilter.h"

static GskRenderNode *
svg_load (const char *filename,
          int         width,
          int         height)
{
  GFile *file;
  GBytes *bytes;
  GtkSvg *svg;
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  file = g_file_new_for_commandline_arg (filename);

  svg = gtk_svg_new ();
  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (bytes)
    {
      gtk_svg_load_from_bytes (svg, bytes);
      g_bytes_unref (bytes);
    }

  snapshot = gtk_snapshot_new ();

  if (width == 0 || height == 0)
    {
      width = gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (svg));
      height = gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (svg));
    }

  gdk_paintable_snapshot (GDK_PAINTABLE (svg), snapshot, width, height);

  node = gtk_snapshot_free_to_node (snapshot);

  g_object_unref (svg);
  g_object_unref (file);

  return node;
}

GskRenderNode *
filter_load_svg (GskRenderNode  *node,
                 RntContext     *ctx,
                 int             argc,
                 const char    **argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  char *resize = NULL;
  guint64 width = 0;
  guint64 height = 0;
  const GOptionEntry entries[] = {
    { "resize", 0, 0, G_OPTION_ARG_STRING, &resize, N_("Resize"), N_("SIZE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;
  gsize i;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Load from SVGs"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      rnt_context_error (ctx, "%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  g_option_context_free (context);

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
              rnt_context_error (ctx, "%s\n", error->message);
              g_error_free (error);
              return NULL;
            }
        }
      else
        {
          if (!g_ascii_string_to_unsigned (resize, 10, 1, 1024, &width, &error))
            {
              rnt_context_error (ctx, "%s\n", error->message);
              g_error_free (error);
              return NULL;
            }

          height = width;
        }
    }
  else
    {
      width = 0;
      height = 0;
    }

  if (filenames == NULL)
    {
      rnt_context_error (ctx, _("No filename specified\n"));
      return NULL;
    }

  gsk_render_node_ref (node);

  for (i = g_strv_length (filenames); i-- > 0;)
    {
      if (node != rnt_context_get_null_node (ctx))
        rnt_context_push_node (ctx, node);
      node = svg_load (filenames[i], width, height);
    }

  g_strfreev (filenames);

  return node;
}
