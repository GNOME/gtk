/*  Copyright 2026 Benjamin Otte
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
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtk-rendernode-tool.h"
#include "gtk-tool-utils.h"

typedef struct
{
  GdkMemoryFormat format;
  GdkColorState *color_state;
} ConvertData;

static GdkTexture *
convert_texture (GskRenderReplay *replay,
                 GdkTexture      *texture,
                 gpointer         user_data)
{
  ConvertData *data = user_data;
  GdkTexture *result;
  GdkTextureDownloader *downloader;
  GBytes *bytes;
  GdkMemoryTextureBuilder *builder;
  gsize offsets[4];
  gsize strides[4];
  gsize p;

  downloader = gdk_texture_downloader_new (texture);

  if (data->format < GDK_MEMORY_N_FORMATS)
    gdk_texture_downloader_set_format (downloader, data->format);
  else
    gdk_texture_downloader_set_format (downloader, gdk_texture_get_format (texture));

  if (data->color_state)
    gdk_texture_downloader_set_color_state (downloader, data->color_state);
  else
    gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));

  bytes = gdk_texture_downloader_download_bytes_with_planes (downloader, offsets, strides);

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_bytes (builder, bytes);
  for (p = 0; p < G_N_ELEMENTS (offsets); p++)
    {
      gdk_memory_texture_builder_set_offset (builder, p, offsets[p]);
      gdk_memory_texture_builder_set_stride_for_plane (builder, p, strides[p]);
    }
  gdk_memory_texture_builder_set_format (builder, gdk_texture_downloader_get_format (downloader));
  gdk_memory_texture_builder_set_color_state (builder, gdk_texture_downloader_get_color_state (downloader));
  gdk_memory_texture_builder_set_width (builder, gdk_texture_get_width (texture));
  gdk_memory_texture_builder_set_height (builder, gdk_texture_get_height (texture));

  result = gdk_memory_texture_builder_build (builder);

  g_bytes_unref (bytes);
  gdk_texture_downloader_free (downloader);
  g_object_unref (builder);

  return result;
}

GskRenderNode *
filter_texture (GskRenderNode  *node,
                int             argc,
                const char    **argv)
{
  ConvertData data = {
    .format = GDK_MEMORY_N_FORMATS
  };
  char *format_name = NULL;
  char *colorstate_name = NULL;
  char *cicp_tuple = NULL;
  const GOptionEntry entries[] = {
    { "format", 0, 0, G_OPTION_ARG_STRING, &format_name, N_("Format to use"), N_("FORMAT") },
    { "color-state", 0, 0, G_OPTION_ARG_STRING, &colorstate_name, N_("Color state to use"), N_("COLORSTATE") },
    { "cicp", 0, 0, G_OPTION_ARG_STRING, &cicp_tuple, N_("Color state to use, as cicp tuple"), N_("CICP") },
    { NULL },
  };
  GOptionContext *context;
  GskRenderReplay *replay;
  GskRenderNode *result;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Convert textures"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("texture: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (argc != 1)
    {
      g_printerr ("texture: Unexpected arguments\n");
      exit (1);
    }

  if (format_name)
    {
      if (!find_format_by_name (format_name, &data.format))
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
      data.color_state = find_color_state_by_name (colorstate_name);
      if (!data.color_state)
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
      if (data.color_state)
        {
          g_printerr (_("Can't specify both --color-state and --cicp\n"));
          exit (1);
        }

      data.color_state = parse_cicp_tuple (cicp_tuple, &error);

      if (!data.color_state)
        {
          g_printerr (_("Not a supported cicp tuple: %s\n"), error->message);
          exit (1);
        }
    }

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_texture_filter (replay,
                                        convert_texture,
                                        &data,
                                        NULL);
  result = gsk_render_replay_filter_node (replay, node);
  gsk_render_replay_free (replay);

  gsk_render_node_unref (node);

  return result;
}
