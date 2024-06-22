/*  Copyright 2024 Red Hat, Inc.
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
#include "gtk-image-tool.h"

static const char *
get_format_name (GdkMemoryFormat format)
{
  GEnumClass *class;
  GEnumValue *value;
  const char *name;

  class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);
  value = g_enum_get_value (class, format);
  name = value->value_nick;
  g_type_class_unref (class);

  return name;
}

static void
file_info (const char *filename)
{
  GdkTexture *texture;

  texture = load_image_file (filename);

  g_print ("%s %dx%d\n", _("Size:"), gdk_texture_get_width (texture), gdk_texture_get_height (texture));
  g_print ("%s %s\n", _("Format:"), get_format_name (gdk_texture_get_format (texture)));
  g_print ("%s %s\n", _("Color state:"), get_color_state_name (gdk_texture_get_color_state (texture)));

  g_object_unref (texture);
}

void
do_info (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;

  g_set_prgname ("gtk4-image-tool info");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Provide information about the image."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No image file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 1)
    {
      g_printerr (_("Can only accept a single image file\n"));
      exit (1);
    }

  file_info (filenames[0]);

  g_strfreev (filenames);
}
