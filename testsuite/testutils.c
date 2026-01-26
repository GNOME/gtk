/*
 * Copyright © 2021 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#ifdef G_OS_WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "testsuite/testutils.h"
#include "testsuite/diff/diff.h"

char *
diff_bytes (const char *file,
            GBytes     *input1,
            GBytes     *input2)
{
  GInputStream *in1 = g_memory_input_stream_new_from_bytes (input1);
  GInputStream *in2 = g_memory_input_stream_new_from_bytes (input2);
  GOutputStream *out = g_memory_output_stream_new_resizable ();
  char *fbase;
  char *diff;

  fbase = g_path_get_basename (file);

  if (diffreg (fbase, in1, in2, out, 0) == D_SAME)
    diff = NULL;
  else
    diff = g_strndup (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
                      g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (out)));

  g_free (fbase);

  g_object_unref (in1);
  g_object_unref (in2);
  g_object_unref (out);

  return diff;
}

static GInputStream *
file_get_input_stream (const char  *file,
                       GError     **error)
{
  GFile *f;
  GInputStream *in;
  GInputStream *in2;
  GConverter *converter;

  f = g_file_new_for_path (file);
  in = G_INPUT_STREAM (g_file_read (f, NULL, error));
  g_object_unref (f);

  if (in == NULL)
    return NULL;

  if (!g_str_has_suffix (file, ".gz"))
    return in;

  converter = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
  in2 = g_converter_input_stream_new (in, converter);

  g_object_unref (in);
  g_object_unref (converter);

  return in2;
}

static GBytes *
input_stream_to_bytes (GInputStream *in)
{
  GOutputStream *out = g_memory_output_stream_new_resizable ();
  GBytes *bytes;

  g_output_stream_splice (out, in, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, NULL, NULL);

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));
  g_object_unref (out);

  return bytes;
}

char *
diff_node_with_file (const char     *file,
                     GskRenderNode  *node,
                     GError        **error)
{
  GInputStream *old, *new;
  GOutputStream *out;
  char *fbase;
  GBytes *data;
  GskRenderNode *old_node;
  GBytes *bytes1, *bytes2;
  char *diff;
  int ret;

  old = file_get_input_stream (file, error);
  if (!old)
    return NULL;

  if (node)
    bytes2 = gsk_render_node_serialize (node);
  else
    bytes2 = g_bytes_new_static ("", 0);
  new = g_memory_input_stream_new_from_bytes (bytes2);
  out = g_memory_output_stream_new_resizable ();

  fbase = g_path_get_basename (file);

  ret = diffreg (fbase, old, new, out, 0);

  g_free (fbase);
  g_object_unref (old);
  g_object_unref (new);
  g_object_unref (out);

  if (ret == D_SAME)
    return NULL;

  g_test_message ("Node diff failed, retrying with serialize roundtrip");

  old = file_get_input_stream (file, error);
  data = input_stream_to_bytes (old);
  old_node = gsk_render_node_deserialize (data, NULL, NULL);
  g_assert (old_node != NULL);

  g_bytes_unref (data);
  g_object_unref (old);

  bytes1 = gsk_render_node_serialize (old_node);
  if (node)
    bytes2 = gsk_render_node_serialize (node);
  else
    bytes2 = g_bytes_new_static ("", 0);

  diff = diff_bytes (file, bytes1, bytes2);

  gsk_render_node_unref (old_node);

  g_bytes_unref (bytes1);
  g_bytes_unref (bytes2);

  return diff;
}

/*<private>
 * diff_bytes_with_file:
 * @file1: The filename of the original. This is assumed
 *   to be the reference to compare against.
 * @input: some text contained in a `GBytes` that is
 *   supposed to match the file's contents
 * @error: A return location for an error if diffing could
 *   not happen
 * 
 * Diffs generated text with a reference file.
 * 
 * If the diffing runs into any error, NULL is returned and
 * `error` is set. If diffing succeeds, the error is not set
 * and NULL is returned if the file was identical to the
 * contents of the file or the actual diff is returned if
 * they were not.
 * 
 * Returns: NULL on success or failure, the diff on failure
 */
char *
diff_bytes_with_file (const char  *file1,
                      GBytes      *input,
                      GError     **error)
{
  GInputStream *old, *new;
  GOutputStream *out;
  char *fbase, *diff;

  old = file_get_input_stream (file1, error);
  new = g_memory_input_stream_new_from_bytes (input);
  out = g_memory_output_stream_new_resizable ();

  fbase = g_path_get_basename (file1);

  if (diffreg (fbase, old, new, out, 0) == D_SAME)
    diff = NULL;
  else
    diff = g_strndup (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
                      g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (out)));

  g_object_unref (old);
  g_object_unref (new);
  g_object_unref (out);
  g_free (fbase);

  return diff;
}

char *
diff_string_with_file (const char  *file1,
                       const char  *text,
                       gssize       len,
                       GError     **error)
{
  GBytes *bytes;
  char *result;
  
  if (len < 0)
    len = strlen (text);

  bytes = g_bytes_new_static (text, len);

  result = diff_bytes_with_file (file1, bytes, error);

  g_bytes_unref (bytes);

  return result;
}
