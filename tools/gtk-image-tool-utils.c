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


GdkTexture *
load_image_file (const char *filename)
{
  GError *error = NULL;
  GdkTexture *texture;

  texture = gdk_texture_new_from_filename (filename, &error);
  if (!texture)
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  return texture;
}

gboolean
find_format_by_name (const char      *name,
                     GdkMemoryFormat *format)
{
  GEnumClass *class;
  GEnumValue *value;

  class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  value = g_enum_get_value_by_nick (class, name);
  if (value)
    *format = value->value;

  g_type_class_unref (class);

  return value != NULL;
}

char **
get_format_names (void)
{
  GEnumClass *class;
  GStrvBuilder *builder;
  char **res;

  builder = g_strv_builder_new ();
  class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (int i = 0; i < class->n_values; i++)
    {
      if (class->values[i].value == GDK_MEMORY_N_FORMATS)
        continue;
      g_strv_builder_add (builder, class->values[i].value_nick);
    }

  g_type_class_unref (class);

  res = g_strv_builder_end (builder);

  g_strv_builder_unref (builder);

  return res;
}

GdkColorState *
find_color_state_by_name (const char *name)
{
  GdkCicpParams *params = NULL;
  GdkColorState *color_state = NULL;
  GError *error = NULL;

  if (g_strcmp0 (name, "srgb") == 0)
    color_state = gdk_color_state_get_srgb ();
  else if (g_strcmp0 (name, "srgb-linear") == 0)
    color_state = gdk_color_state_get_srgb_linear ();
  else if (g_strcmp0 (name, "rec2100-pq") == 0)
    color_state = gdk_color_state_get_rec2100_pq ();
  else if (g_strcmp0 (name, "rec2100-linear") == 0)
    color_state = gdk_color_state_get_rec2100_linear ();
  else if (g_strcmp0 (name, "display-p3") == 0)
    {
      params = gdk_cicp_params_new ();

      gdk_cicp_params_set_color_primaries (params, 12);
      gdk_cicp_params_set_transfer_function (params, 13);
      gdk_cicp_params_set_matrix_coefficients (params, 0);
      gdk_cicp_params_set_range (params, GDK_CICP_RANGE_FULL);

      color_state = gdk_cicp_params_build_color_state (params, &error);
    }
  else if (g_strcmp0 (name, "rec2020") == 0)
    {
      params = gdk_cicp_params_new ();

      gdk_cicp_params_set_color_primaries (params, 9);
      gdk_cicp_params_set_transfer_function (params, 1);
      gdk_cicp_params_set_matrix_coefficients (params, 0);
      gdk_cicp_params_set_range (params, GDK_CICP_RANGE_FULL);

      color_state = gdk_cicp_params_build_color_state (params, &error);
    }
  else if (g_strcmp0 (name, "rec2100-hlg") == 0)
    {
      params = gdk_cicp_params_new ();

      gdk_cicp_params_set_color_primaries (params, 9);
      gdk_cicp_params_set_transfer_function (params, 18);
      gdk_cicp_params_set_matrix_coefficients (params, 0);
      gdk_cicp_params_set_range (params, GDK_CICP_RANGE_FULL);

      color_state = gdk_cicp_params_build_color_state (params, &error);
    }
  else if (g_strcmp0 (name, "yuv") == 0 ||
           g_strcmp0 (name, "bt601") == 0)
    {
      params = gdk_cicp_params_new ();

      gdk_cicp_params_set_color_primaries (params, 1);
      gdk_cicp_params_set_transfer_function (params, 13);
      gdk_cicp_params_set_matrix_coefficients (params, 6);
      gdk_cicp_params_set_range (params, GDK_CICP_RANGE_NARROW);

      color_state = gdk_cicp_params_build_color_state (params, &error);
    }
  else if (g_strcmp0 (name, "bt709") == 0)
    {
      params = gdk_cicp_params_new ();

      gdk_cicp_params_set_color_primaries (params, 1);
      gdk_cicp_params_set_transfer_function (params, 1);
      gdk_cicp_params_set_matrix_coefficients (params, 6);
      gdk_cicp_params_set_range (params, GDK_CICP_RANGE_NARROW);

      color_state = gdk_cicp_params_build_color_state (params, &error);
    }

  if (error)
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_clear_object (&params);

  return color_state;
}

char **
get_color_state_names (void)
{
  static const char *names[] = {
    "srgb", "srgb-linear", "display-p3", "rec2020",
    "rec2100-pq", "rec2100-linear", "rec2100-hlg",
    "yuv", "bt601", "bt709",
    NULL,
  };

  return g_strdupv ((char **) names);
}

char *
get_color_state_cicp (GdkColorState *color_state)
{
  GdkCicpParams *params;
  char *str = NULL;

  params = gdk_color_state_create_cicp_params (color_state);

  if (params)
    {
      str = g_strdup_printf ("%u/%u/%u/%u",
                              gdk_cicp_params_get_color_primaries (params),
                              gdk_cicp_params_get_transfer_function (params),
                              gdk_cicp_params_get_matrix_coefficients (params),
                              gdk_cicp_params_get_range (params));
      g_object_unref (params);
    }

  return str;
}

char *
get_color_state_name (GdkColorState *color_state)
{
  char **names;
  char *name = NULL;

  names = get_color_state_names ();

  for (int i = 0; names[i]; i++)
    {
      GdkColorState *cs = find_color_state_by_name (names[i]);
      if (gdk_color_state_equal (cs, color_state))
        {
          name = g_strdup (names[i]);
          break;
        }
    }

  g_strfreev (names);

  return name;
}

GdkColorState *
parse_cicp_tuple (const char  *cicp_tuple,
                  GError     **error)
{
  char **tokens;
  guint64 num[4];
  GdkCicpParams *params;
  GdkColorState *color_state;

  tokens = g_strsplit (cicp_tuple, "/", 0);

  if (g_strv_length (tokens) != 4 ||
      !g_ascii_string_to_unsigned (tokens[0], 10, 0, 255, &num[0], NULL) ||
      !g_ascii_string_to_unsigned (tokens[1], 10, 0, 255, &num[1], NULL) ||
      !g_ascii_string_to_unsigned (tokens[2], 10, 0, 255, &num[2], NULL) ||
      !g_ascii_string_to_unsigned (tokens[3], 10, 0, 255, &num[3], NULL))
    {
      g_strfreev (tokens);
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("cicp must be 4 numbers, separated by /\n"));
      return NULL;
    }

  g_strfreev (tokens);

  params = gdk_cicp_params_new ();

  gdk_cicp_params_set_color_primaries (params, (guint) num[0]);
  gdk_cicp_params_set_transfer_function (params, (guint) num[1]);
  gdk_cicp_params_set_matrix_coefficients (params, (guint) num[2]);
  gdk_cicp_params_set_range (params, num[3] == 0 ? GDK_CICP_RANGE_NARROW : GDK_CICP_RANGE_FULL);
  color_state = gdk_cicp_params_build_color_state (params, error);

  g_object_unref (params);

  return color_state;
}
