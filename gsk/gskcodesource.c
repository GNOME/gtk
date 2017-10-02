/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gskcodesourceprivate.h"

struct _GskCodeSource {
  GObject parent_instance;

  char *name;
  GFile *file;

  GBytes *bytes;
};

G_DEFINE_TYPE (GskCodeSource, gsk_code_source, G_TYPE_OBJECT)

static void
gsk_code_source_dispose (GObject *object)
{
  GskCodeSource *source = GSK_CODE_SOURCE (object);

  g_free (source->name);
  if (source->file)
    g_object_unref (source->file);
  if (source->bytes)
    g_bytes_unref (source->bytes);

  G_OBJECT_CLASS (gsk_code_source_parent_class)->dispose (object);
}

static void
gsk_code_source_class_init (GskCodeSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_code_source_dispose;
}

static void
gsk_code_source_init (GskCodeSource *source)
{
}

const char *
gsk_code_source_get_name (GskCodeSource *source)
{
  g_return_val_if_fail (GSK_IS_CODE_SOURCE (source), NULL);

  return source->name;
}

GFile *
gsk_code_source_get_file (GskCodeSource *source)
{
  g_return_val_if_fail (GSK_IS_CODE_SOURCE (source), NULL);

  return source->file;
}

GskCodeSource *
gsk_code_source_new_for_bytes (const char *name,
                               GBytes     *data)
{
  GskCodeSource *result;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);

  result = g_object_new (GSK_TYPE_CODE_SOURCE, NULL);

  result->name = g_strdup (name);
  result->bytes = g_bytes_ref (data);

  return result;
}

static char *
get_name_for_file (GFile *file)
{
  GFileInfo *info;
  char *result;

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

  if (info)
    {
      result = g_strdup (g_file_info_get_display_name (info));
      g_object_unref (info);
    }
  else
    {
      result = g_strdup ("<broken file>");
    }

  return result;
}

GskCodeSource *
gsk_code_source_new_for_file (GFile *file)
{
  GskCodeSource *result;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  result = g_object_new (GSK_TYPE_CODE_SOURCE, NULL);

  result->file = g_object_ref (file);
  result->name = get_name_for_file (file);

  return result;
}

GBytes *
gsk_code_source_load (GskCodeSource  *source,
                      GError        **error)
{
  gchar *data;
  gsize length;

  if (source->bytes)
    return g_bytes_ref (source->bytes);

  g_return_val_if_fail (source->file, NULL);

  if (!g_file_load_contents (source->file,
                             NULL,
                             &data, &length,
                             NULL,
                             error))
    return NULL;

  return g_bytes_new_take (data, length);
}

