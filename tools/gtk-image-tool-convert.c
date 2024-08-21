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

// TEMPORARY

#include "gdk/gdkcolordefs.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkhdrmetadataprivate.h"

#undef gdk_color_state_ref
#undef gdk_color_state_unref

static void
multiply (const float m[9],
          const float v[3],
          float       res[3])
{
  for (int i = 0; i < 3; i++)
    res[i] = m[3*i+0]*v[0] + m[3*i+1]*v[1] + m[3*i+2]*v[2];
}

static const float rec2020_to_lms[9] = {
  0.412109, 0.523926, 0.063965,
  0.166748, 0.720459, 0.112793,
  0.024170, 0.075439, 0.900391,
};

static const float lms_to_ictcp[9] = {
  0.500000, 0.500000, 0.000000,
  1.613770, -3.323486, 1.709717,
  4.378174, -4.245605, -0.132568,
};

static const float lms_to_rec2020[9] = {
  3.436607, -2.506452, 0.069845,
  -0.791330, 1.983600, -0.192271,
  -0.025950, -0.098914, 1.124864,
};

static const float ictcp_to_lms[9] = {
  1.000000, 0.008609, 0.111030,
  1.000000, -0.008609, -0.111030,
  1.000000, 0.560031, -0.320627,
};

static void
rec2100_linear_to_ictcp (float in[4],
                         float out[4])
{
  float lms[3];

  multiply (rec2020_to_lms, in, lms);

  lms[0] = pq_oetf (lms[0]);
  lms[1] = pq_oetf (lms[1]);
  lms[2] = pq_oetf (lms[2]);

  multiply (lms_to_ictcp, lms, out);

  out[3] = in[3];
}

static void
ictcp_to_rec2100_linear (float in[4],
                         float out[4])
{
  float lms[3];

  multiply (ictcp_to_lms, in, lms);

  lms[0] = pq_eotf (lms[0]);
  lms[1] = pq_eotf (lms[1]);
  lms[2] = pq_eotf (lms[2]);

  multiply (lms_to_rec2020, lms, out);

  out[3] = in[3];
}

static void
color_map (GdkColorState   *src_color_state,
           GdkHdrMetadata  *src_metadata,
           GdkColorState   *target_color_state,
           GdkHdrMetadata  *target_metadata,
           float            (*values)[4],
           gsize            n_values,
           int              debug)
{
  float ictcp[4];
  float ref_lum = 203;
  float src_max_lum;
  float target_max_lum;
  float src_lum;
  float needed_range;
  float added_range;
  float new_ref_lum;
  float rel_highlight;
  float low;
  float high;

  src_max_lum = src_metadata->max_lum;
  target_max_lum = target_metadata->max_lum;

  if (src_max_lum <= target_max_lum * 1.01 &&
      gdk_color_state_equal (src_color_state, target_color_state))
    {
      return;
    }

  needed_range = src_max_lum / ref_lum;
  added_range = MIN (needed_range, 1.5);
  new_ref_lum = ref_lum / added_range;

  for (gsize i = 0; i < n_values; i++)
    {
      GdkColor tmp;
      float v[4];

      if (src_max_lum <= target_max_lum * 1.01)
        {
          gdk_color_init (&tmp, src_color_state, values[i]);
          gdk_color_to_float (&tmp, target_color_state, values[i]);
          gdk_color_finish (&tmp);
        }
      else
        {
          gdk_color_init (&tmp, src_color_state, values[i]);
          gdk_color_to_float (&tmp, gdk_color_state_get_rec2100_linear (), v);
          gdk_color_finish (&tmp);

          rec2100_linear_to_ictcp (v, ictcp);
          src_lum = pq_eotf (ictcp[0]) * 10000;
          low = MIN (src_lum / added_range, new_ref_lum);
          rel_highlight = CLAMP ((src_lum - new_ref_lum) / (src_max_lum - new_ref_lum), 0, 1);
          high = pow (rel_highlight, 0.5) * (target_max_lum - new_ref_lum);

          if (debug > -1 && debug == i)
            {
              g_print ("needed range: %f\n", needed_range);
              g_print ("added range: %f\n", added_range);
              g_print ("new ref lum: %f\n", new_ref_lum);
              g_print ("rec2100-linear: %f %f %f\n", v[0], v[1], v[2]);
              g_print ("ictcp: %f %f %f\n", ictcp[0], ictcp[1], ictcp[2]);
              g_print ("lum: %f\n", src_lum);
              g_print ("adjusted lum: %f\n", low + high);
            }

          ictcp[0] = pq_oetf ((low + high) / 10000);
          ictcp_to_rec2100_linear (ictcp, v);

          gdk_color_init (&tmp, gdk_color_state_get_rec2100_linear (), v);
          gdk_color_to_float (&tmp, target_color_state, values[i]);

          values[i][0] = CLAMP (values[i][0], 0, 1);
          values[i][1] = CLAMP (values[i][1], 0, 1);
          values[i][2] = CLAMP (values[i][2], 0, 1);
          values[i][3] = CLAMP (values[i][3], 0, 1);

          if (debug > -1 && debug == i)
            {
              g_print ("final color: %f %f %f %f\n",
                       values[i][0],
                       values[i][1],
                       values[i][2],
                       values[i][3]);
            }

          gdk_color_finish (&tmp);
        }
    }
}

// END TEMPORARY



static void
save_image (const char      *filename,
            const char      *output,
            GdkMemoryFormat  format,
            GdkColorState   *color_state,
            GdkHdrMetadata  *metadata)
{
  GdkTexture *orig;
  GdkColorState *orig_color_state;
  GdkHdrMetadata *orig_metadata;
  gsize width, height;
  GdkTextureDownloader *downloader;
  GBytes *bytes;
  gsize stride;
  GdkTexture *texture;
  GdkMemoryTextureBuilder *builder;

  orig = load_image_file (filename);
  width = gdk_texture_get_width (orig);
  height = gdk_texture_get_height (orig);
  orig_color_state = gdk_texture_get_color_state (orig);
  orig_metadata = gdk_texture_get_hdr_metadata (orig);

  if (metadata && orig_metadata)
    {
      guchar *data;

      downloader = gdk_texture_downloader_new (orig);

      gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R32G32B32A32_FLOAT);
      gdk_texture_downloader_set_color_state (downloader, orig_color_state);
      bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
      gdk_texture_downloader_free (downloader);

      data = (guchar *) g_bytes_get_data (bytes, NULL);

      for (gsize i = 0; i < height; i++)
        {
          float (*row)[4];
          int debug;

          if (i == height / 2)
            debug = width / 2;
          else
            debug = -1;

          row = (gpointer) (data + i * stride);
          color_map (orig_color_state, orig_metadata, color_state, metadata, row, width, debug);
        }

      builder = gdk_memory_texture_builder_new ();
      gdk_memory_texture_builder_set_bytes (builder, bytes);
      gdk_memory_texture_builder_set_stride (builder, stride);
      gdk_memory_texture_builder_set_format (builder, GDK_MEMORY_R32G32B32A32_FLOAT);
      gdk_memory_texture_builder_set_color_state (builder, color_state);
      gdk_memory_texture_builder_set_hdr_metadata (builder, metadata);
      gdk_memory_texture_builder_set_width (builder, width);
      gdk_memory_texture_builder_set_height (builder, height);

      g_object_unref (orig);
      orig = gdk_memory_texture_builder_build (builder);
      g_object_unref (builder);

    }

  downloader = gdk_texture_downloader_new (orig);

  gdk_texture_downloader_set_format (downloader, format);
  gdk_texture_downloader_set_color_state (downloader, color_state);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  gdk_texture_downloader_free (downloader);

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_bytes (builder, bytes);
  gdk_memory_texture_builder_set_stride (builder, stride);
  gdk_memory_texture_builder_set_format (builder, format);
  gdk_memory_texture_builder_set_color_state (builder, color_state);
  gdk_memory_texture_builder_set_hdr_metadata (builder, metadata);
  gdk_memory_texture_builder_set_width (builder, width);
  gdk_memory_texture_builder_set_height (builder, height);

  texture = gdk_memory_texture_builder_build (builder);

  if (g_str_has_suffix (output, ".tiff"))
    gdk_texture_save_to_tiff (texture, output);
  else
    gdk_texture_save_to_png (texture, output);

  g_object_unref (texture);
  g_bytes_unref (bytes);
  g_object_unref (orig);
  g_object_unref (builder);
}

static gboolean
parse_lum (const char *lum,
           float       l[2])
{
  char **s;
  char *endptr0 = NULL;
  char *endptr1 = NULL;

  s = g_strsplit (lum, "-", 0);
  g_print ("lum: '%s' '%s'\n", s[0], s[1]);
  l[0] = g_ascii_strtod (s[0], &endptr0);
  l[1] = g_ascii_strtod (s[1], &endptr1);
  g_strfreev (s);

  return (endptr0 == NULL || endptr0[0] == '\0') &&
         (endptr1 == NULL || endptr1[0] == '\0');
}

static gboolean
find_primaries (GdkColorState *color_state,
                float          p[8])
{
  if (gdk_color_state_equal (color_state, gdk_color_state_get_srgb ()))
    {
      p[0] = 0.640; p[1] = 0.330;
      p[2] = 0.300; p[3] = 0.600;
      p[4] = 0.150; p[5] = 0.060;
      p[6] = 0.3127; p[7] = 0.3290;
      return TRUE;
    }
  else
    return FALSE;
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
  char *mdcv_name = NULL;
  char *lum = NULL;
  const GOptionEntry entries[] = {
    { "format", 0, 0, G_OPTION_ARG_STRING, &format_name, N_("Format to use"), N_("FORMAT") },
    { "color-state", 0, 0, G_OPTION_ARG_STRING, &colorstate_name, N_("Color state to use"), N_("COLORSTATE") },
    { "mdcv", 0, 0, G_OPTION_ARG_STRING, &mdcv_name, N_("MDCV to use"), N_("COLORSTATE") },
    { "luminance", 0, 0, G_OPTION_ARG_STRING, &lum, N_("Luminance range"), N_("MIN-MAX") },
    { "cicp", 0, 0, G_OPTION_ARG_STRING, &cicp_tuple, N_("Color state to use, as cicp tuple"), N_("CICP") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GError *error = NULL;
  GdkMemoryFormat format = GDK_MEMORY_DEFAULT;
  GdkColorState *color_state = NULL;
  GdkHdrMetadata *hdr_metadata = NULL;

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

  if (mdcv_name)
    {
      float primaries[8];
      float luminances[2];

      if (lum && !parse_lum (lum, luminances))
        {
          g_printerr ("Could not parse luminances\n");
          exit (1);
        }

      color_state = find_color_state_by_name (mdcv_name);
      if (mdcv_name && !color_state)
        {
          g_printerr ("MDCV not found\n");
          exit (1);
        }

      if (!find_primaries (color_state, primaries))
        {
          g_printerr ("Sorry no primaries\n");
          exit (1);
        }

      hdr_metadata = gdk_hdr_metadata_new (primaries[0], primaries[1],
                                           primaries[2], primaries[3],
                                           primaries[4], primaries[5],
                                           primaries[6], primaries[7],
                                           luminances[0], luminances[1],
                                           0, 0);
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

  save_image (filenames[0], filenames[1], format, color_state, hdr_metadata);

  g_strfreev (filenames);
}
