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
  if (g_strcmp0 (name, "srgb") == 0)
    return gdk_color_state_get_srgb ();
  else if (g_strcmp0 (name, "srgb-linear") == 0)
    return gdk_color_state_get_srgb_linear ();
  else if (g_strcmp0 (name, "hsl") == 0)
    return gdk_color_state_get_hsl ();
  else if (g_strcmp0 (name, "hwl") == 0)
    return gdk_color_state_get_hwb ();
  else if (g_strcmp0 (name, "oklab") == 0)
    return gdk_color_state_get_oklab ();
  else if (g_strcmp0 (name, "oklch") == 0)
    return gdk_color_state_get_oklch ();
  else if (g_strcmp0 (name, "display-p3") == 0)
    return gdk_color_state_get_display_p3 ();
  else if (g_strcmp0 (name, "xyz") == 0)
    return gdk_color_state_get_xyz ();
  else if (g_strcmp0 (name, "rec2020") == 0)
    return gdk_color_state_get_rec2020 ();
  else if (g_strcmp0 (name, "rec2100-pq") == 0)
    return gdk_color_state_get_rec2100_pq ();
  else if (g_strcmp0 (name, "rec2100-linear") == 0)
    return gdk_color_state_get_rec2100_linear ();

  return NULL;
}

char **
get_color_state_names (void)
{
  GStrvBuilder *builder;
  char **res;

  builder = g_strv_builder_new ();

  g_strv_builder_add_many (builder,
                           "srgb", "srgb-linear", "hsl", "hwb", "oklab", "oklch",
                           "display-p3", "xyz", "rec2020", "rec2100-pq", "rec2100-linear",
                           NULL);
  res = g_strv_builder_end (builder);

  g_strv_builder_unref (builder);

  return res;
}

