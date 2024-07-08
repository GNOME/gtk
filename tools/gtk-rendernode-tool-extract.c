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

static void
extract_texture (GskRenderNode *node,
                 const char    *basename)
{
  GdkTexture *texture;
  char *filename;
  char *path;

  if (gsk_render_node_get_node_type (node) == GSK_TEXTURE_NODE)
    texture = gsk_texture_node_get_texture (node);
  else
    texture = gsk_texture_scale_node_get_texture (node);

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
}

static void
extract_font (GskRenderNode *node,
              const char    *basename)
{
  PangoFont *font;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  hb_blob_t *hb_blob;
  const char *data;
  unsigned int length;
  char *filename;
  char *path;
  char *sum;

  font = gsk_text_node_get_font (node);
  hb_font = pango_font_get_hb_font (font);
  hb_face = hb_font_get_face (hb_font);
  hb_blob = hb_face_reference_blob (hb_face);

  if (hb_blob == hb_blob_get_empty ())
    {
      hb_blob_destroy (hb_blob);
      g_warning ("Failed to extract font data\n");
      return;
    }

  data = hb_blob_get_data (hb_blob, &length);

  sum = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (const guchar *)data, length);

  if (!fonts)
    fonts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (g_hash_table_contains (fonts, sum))
    {
      g_free (sum);
      hb_blob_destroy (hb_blob);
      return;
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
}

static void
extract_from_node (GskRenderNode *node,
                   const char    *basename)
{
  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      for (unsigned int i = 0; i < gsk_container_node_get_n_children (node); i++)
        extract_from_node (gsk_container_node_get_child (node, i), basename);
      break;

    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
      break;

    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
      extract_texture (node, basename);
      break;

    case GSK_TRANSFORM_NODE:
      extract_from_node (gsk_transform_node_get_child (node), basename);
      break;

    case GSK_OPACITY_NODE:
      extract_from_node (gsk_opacity_node_get_child (node), basename);
      break;

    case GSK_COLOR_MATRIX_NODE:
      extract_from_node (gsk_color_matrix_node_get_child (node), basename);
      break;

    case GSK_REPEAT_NODE:
      extract_from_node (gsk_repeat_node_get_child (node), basename);
      break;

    case GSK_CLIP_NODE:
      extract_from_node (gsk_clip_node_get_child (node), basename);
      break;

    case GSK_ROUNDED_CLIP_NODE:
      extract_from_node (gsk_rounded_clip_node_get_child (node), basename);
      break;

    case GSK_SHADOW_NODE:
      extract_from_node (gsk_shadow_node_get_child (node), basename);
      break;

    case GSK_BLEND_NODE:
      extract_from_node (gsk_blend_node_get_bottom_child (node), basename);
      extract_from_node (gsk_blend_node_get_top_child (node), basename);
      break;

    case GSK_CROSS_FADE_NODE:
      extract_from_node (gsk_cross_fade_node_get_start_child (node), basename);
      extract_from_node (gsk_cross_fade_node_get_end_child (node), basename);
      break;

    case GSK_TEXT_NODE:
      extract_font (node, basename);
      break;

    case GSK_BLUR_NODE:
      extract_from_node (gsk_blur_node_get_child (node), basename);
      break;

    case GSK_DEBUG_NODE:
      extract_from_node (gsk_debug_node_get_child (node), basename);
      break;

    case GSK_GL_SHADER_NODE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      for (unsigned int i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
        extract_from_node (gsk_gl_shader_node_get_child (node, i), basename);
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case GSK_MASK_NODE:
      extract_from_node (gsk_mask_node_get_source (node), basename);
      extract_from_node (gsk_mask_node_get_mask (node), basename);
      break;

    case GSK_FILL_NODE:
      extract_from_node (gsk_fill_node_get_child (node), basename);
      break;

    case GSK_STROKE_NODE:
      extract_from_node (gsk_stroke_node_get_child (node), basename);
      break;

    case GSK_SUBSURFACE_NODE:
      extract_from_node (gsk_subsurface_node_get_child (node), basename);
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }
}

static void
file_extract (const char *filename)
{
  GskRenderNode  *node;
  char *basename, *dot;

  node = load_node_file (filename);
  basename = g_path_get_basename (filename);
  dot = strrchr (basename, '.');
  if (dot)
    *dot = 0;

  extract_from_node (node, basename);

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
