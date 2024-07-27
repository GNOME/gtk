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


static void
save_image (const char      *filename,
            const char      *output,
            GdkMemoryFormat  format,
            GdkColorState   *color_state)
{
  GdkTexture *orig;
  GdkTextureDownloader *downloader;
  GBytes *bytes;
  gsize stride;
  GdkTexture *texture;
  GdkMemoryTextureBuilder *builder;

  orig = load_image_file (filename);
  downloader = gdk_texture_downloader_new (orig);

  gdk_texture_downloader_set_format (downloader, format);
  gdk_texture_downloader_set_color_state (downloader, color_state);

  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_bytes (builder, bytes);
  gdk_memory_texture_builder_set_stride (builder, stride);
  gdk_memory_texture_builder_set_format (builder, format);
  gdk_memory_texture_builder_set_color_state (builder, color_state);
  gdk_memory_texture_builder_set_width (builder, gdk_texture_get_width (orig));
  gdk_memory_texture_builder_set_height (builder, gdk_texture_get_height (orig));

  texture = gdk_memory_texture_builder_build (builder);

  if (g_str_has_suffix (output, ".tiff"))
    gdk_texture_save_to_tiff (texture, output);
  else
    gdk_texture_save_to_png (texture, output);

  g_object_unref (texture);
  g_bytes_unref (bytes);
  gdk_texture_downloader_free (downloader);
  g_object_unref (orig);
  g_object_unref (builder);
}

void
do_convert (int          *argc,
            const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  char *format_name = NULL;
  char *colorstate_name = NULL;
  char *cicp_tuple = NULL;
  const GOptionEntry entries[] = {
    { "format", 0, 0, G_OPTION_ARG_STRING, &format_name, N_("Format to use"), N_("FORMAT") },
    { "color-state", 0, 0, G_OPTION_ARG_STRING, &colorstate_name, N_("Color state to use"), N_("COLORSTATE") },
    { "cicp", 0, 0, G_OPTION_ARG_STRING, &cicp_tuple, N_("Color state to use, as cicp tuple"), N_("CICP") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GError *error = NULL;
  GdkMemoryFormat format = GDK_MEMORY_DEFAULT;
  GdkColorState *color_state = NULL;

  g_set_prgname ("gtk4-image-tool convert");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Convert the image to a different format or color state."));

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

  if (g_strv_length (filenames) != 2)
    {
      g_printerr (_("Can only accept a single image file and output file\n"));
      exit (1);
    }

  if (format_name)
    {
      if (!find_format_by_name (format_name, &format))
        {
          char **names;
          char *suggestion;

          names = get_format_names ();
          suggestion = g_strjoinv ("\n  ", names);

          g_printerr (_("Not a memory format: %s\nPossible values:\n  %s\n"),
                      format_name, suggestion);
          exit (1);
        }
    }

  if (colorstate_name)
    {
      color_state = find_color_state_by_name (colorstate_name);
      if (!color_state)
        {
          char **names;
          char *suggestion;

          names = get_color_state_names ();
          suggestion = g_strjoinv ("\n  ", names);

          g_printerr (_("Not a color state: %s\nPossible values:\n  %s\n"),
                      colorstate_name, suggestion);
          exit (1);
        }
    }

  if (cicp_tuple)
    {
      if (color_state)
        {
          g_printerr (_("Can't specify both --color-state and --cicp\n"));
          exit (1);
        }

      color_state = parse_cicp_tuple (cicp_tuple, &error);

      if (!color_state)
        {
          g_printerr (_("Not a supported cicp tuple: %s\n"), error->message);
          exit (1);
        }
    }

  if (!color_state)
    color_state = gdk_color_state_get_srgb ();

  save_image (filenames[0], filenames[1], format, color_state);

  g_strfreev (filenames);
}
