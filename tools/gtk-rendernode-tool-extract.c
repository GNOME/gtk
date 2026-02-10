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

static gboolean verbose;
static char *directory = NULL;

static guint texture_count;
static guint font_count;

static GHashTable *fonts;

static GdkTexture *
extract_texture (GskRenderReplay *replay,
                 GdkTexture      *texture,
                 gpointer         user_data)
{
  const char *basename = user_data;
  char *filename;
  char *path;

  do {
    filename = g_strdup_printf ("%s-texture-%u.png", basename, texture_count);
    path = g_build_path ("/", directory, filename, NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS))
      break;

    g_free (path);
    g_free (filename);
    texture_count++;
  } while (TRUE);

  if (verbose)
    g_print ("Writing %dx%d texture to %s\n",
             gdk_texture_get_width (texture),
             gdk_texture_get_height (texture),
             filename);

  if (!gdk_texture_save_to_png (texture, path))
    {
      g_printerr (_("Failed to write %s\n"), filename);
    }

  g_free (path);
  g_free (filename);

  texture_count++;

  return g_object_ref (texture);
}

static PangoFont *
extract_font (GskRenderReplay *replay,
              PangoFont       *font,
              gpointer         user_data)
{
  const char *basename = user_data;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  hb_blob_t *hb_blob;
  const char *data;
  unsigned int length;
  char *filename;
  char *path;
  char *sum;

  hb_font = pango_font_get_hb_font (font);
  hb_face = hb_font_get_face (hb_font);
  hb_blob = hb_face_reference_blob (hb_face);

  if (hb_blob == hb_blob_get_empty ())
    {
      hb_blob_destroy (hb_blob);
      g_warning ("Failed to extract font data\n");
      return g_object_ref (font);
    }

  data = hb_blob_get_data (hb_blob, &length);

  sum = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (const guchar *)data, length);

  if (!fonts)
    fonts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (g_hash_table_contains (fonts, sum))
    {
      g_free (sum);
      hb_blob_destroy (hb_blob);
      return g_object_ref (font);
    }

  g_hash_table_add (fonts, sum);

  do {
    filename = g_strdup_printf ("%s-font-%u.ttf", basename, font_count);
    path = g_build_path ("/", directory, filename, NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS))
      break;

    g_free (path);
    g_free (filename);
    font_count++;
  } while (TRUE);

  if (verbose)
    {
      PangoFontDescription *desc;

      desc = pango_font_describe (font);
      g_print ("Writing font %s to %s\n",
               pango_font_description_get_family (desc),
               filename);
      pango_font_description_free (desc);
    }

  if (!g_file_set_contents (path, data, length, NULL))
    {
      g_printerr (_("Failed to write %s\n"), filename);
    }

  hb_blob_destroy (hb_blob);

  g_free (path);
  g_free (filename);

  font_count++;

  return g_object_ref (font);
}

static void
file_extract (const char *filename)
{
  GskRenderNode *node, *result;
  GskRenderReplay *replay;
  char *basename, *dot;

  node = load_node_file (filename);
  basename = g_path_get_basename (filename);
  dot = strrchr (basename, '.');
  if (dot)
    *dot = 0;

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_texture_filter (replay,
                                        extract_texture,
                                        basename,
                                        g_free);
  gsk_render_replay_set_font_filter (replay,
                                     extract_font,
                                     g_strdup (basename),
                                     g_free);

  result = gsk_render_replay_filter_node (replay, node);

  gsk_render_node_unref (result);
  gsk_render_replay_free (replay);
  gsk_render_node_unref (node);
}

void
do_extract (int          *argc,
            const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { "dir", 0, 0, G_OPTION_ARG_FILENAME, &directory, N_("Directory to use"), N_("DIRECTORY") },
    { "verbose", 0, 0, G_OPTION_ARG_NONE, &verbose, N_("Be verbose"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;

  g_set_prgname ("gtk4-rendernode-tool extract");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Extract data urls from the render node."));

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
      g_printerr (_("Can only accept a single .node file\n"));
      exit (1);
    }

  if (directory == NULL)
    directory = g_strdup (".");

  file_extract (filenames[0]);

  g_strfreev (filenames);
  g_free (directory);
}
