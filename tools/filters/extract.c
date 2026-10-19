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

typedef struct _Extraction Extraction;

struct _Extraction
{
  gboolean verbose;
  char *directory;
  char *basename;

  guint texture_count;
  guint font_count;
  GHashTable *fonts;
};

static GdkTexture *
extract_texture (GskRenderReplay *replay,
                 GdkTexture      *texture,
                 gpointer         user_data)
{
  Extraction *e = user_data;
  char *filename;
  char *path;

  do {
    filename = g_strdup_printf ("%s-texture-%u.png", e->basename, e->texture_count);
    path = g_build_path ("/", e->directory, filename, NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS))
      break;

    g_free (path);
    g_free (filename);
    e->texture_count++;
  } while (TRUE);

  if (e->verbose)
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

  e->texture_count++;

  return g_object_ref (texture);
}

static PangoFont *
extract_font (GskRenderReplay *replay,
              PangoFont       *font,
              gpointer         user_data)
{
  Extraction *e = user_data;
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

  if (g_hash_table_contains (e->fonts, sum))
    {
      g_free (sum);
      hb_blob_destroy (hb_blob);
      return g_object_ref (font);
    }

  g_hash_table_add (e->fonts, sum);

  do {
    filename = g_strdup_printf ("%s-font-%u.ttf", e->basename, e->font_count);
    path = g_build_path ("/", e->directory, filename, NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS))
      break;

    g_free (path);
    g_free (filename);
    e->font_count++;
  } while (TRUE);

  if (e->verbose)
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

  e->font_count++;

  return g_object_ref (font);
}

static void
node_extract (Extraction    *e,
              GskRenderNode *node)
{
  GskRenderNode *result;
  GskRenderReplay *replay;

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_texture_filter (replay,
                                        extract_texture,
                                        e,
                                        NULL);
  gsk_render_replay_set_font_filter (replay,
                                     extract_font,
                                     e,
                                     NULL);

  result = gsk_render_replay_filter_node (replay, node);

  gsk_render_node_unref (result);
  gsk_render_replay_free (replay);
}

GskRenderNode *
filter_extract (GskRenderNode  *node,
                RntContext     *ctx,
                int             argc,
                const char    **argv)
{
  GOptionContext *context;
  Extraction e = {
    .verbose = FALSE,
    .directory = NULL,
    .basename = NULL,
    .texture_count = 0,
    .font_count = 0,
    .fonts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL),
  };
  const GOptionEntry entries[] = {
    { "dir", 0, 0, G_OPTION_ARG_FILENAME, &e.directory, N_("Directory to use"), N_("DIRECTORY") },
    { "basename", 0, 0, G_OPTION_ARG_FILENAME, &e.basename, N_("Basename for extracted files"), N_("NAME") },
    { "verbose", 0, 0, G_OPTION_ARG_NONE, &e.verbose, N_("Be verbose"), NULL },
    { NULL, }
  };
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Extract textures and glyphs"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (e.directory == NULL)
    e.directory = g_strdup (".");
  if (e.basename == NULL)
    e.basename = g_strdup ("extract");

  node_extract (&e, node);

  g_free (e.directory);
  g_free (e.basename);
  g_hash_table_unref (e.fonts);

  return node;
}
