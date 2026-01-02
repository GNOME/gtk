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

  if (g_str_has_suffix (file1, ".gz"))
    {
      char *buf1;
      size_t len1;
      GConverter *decompressor;
      GBytes *compressed, *decompressed;

      if (!g_file_get_contents (file1, &buf1, &len1, error))
        return NULL;
      decompressor = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
      compressed = g_bytes_new_take (buf1, len1);
      decompressed = g_converter_convert_bytes (decompressor, compressed, error);
      g_object_unref (decompressor);
      g_bytes_unref (compressed);
      if (!decompressed)
        return NULL;
      old = g_memory_input_stream_new_from_bytes (decompressed);
      g_bytes_unref (decompressed);
    }
  else
    {
      GFile *file;
      file = g_file_new_for_path (file1);
      old = G_INPUT_STREAM (g_file_read (file, NULL, error));
      // Maybe wrap old in BufferedInputStream?
      g_object_unref (file);
      if (!old)
        return NULL;
    }

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
