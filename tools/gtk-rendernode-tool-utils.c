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


static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_printerr (_("Error at %s: %s\n"), string->str, error->message);

  g_string_free (string, TRUE);
}

GskRenderNode *
load_node_file (const char *filename)
{
  GFile *file;
  GBytes *bytes;
  GError *error = NULL;

  file = g_file_new_for_commandline_arg (filename);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_object_unref (file);

  if (bytes == NULL)
    {
      g_printerr (_("Failed to load node file: %s\n"), error->message);
      g_clear_error (&error);
      exit (1);
    }

  return gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
}
