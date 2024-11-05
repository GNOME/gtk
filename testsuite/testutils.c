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
  char *diff_cmd, *diff;

  diff = NULL;

  diff_cmd = g_find_program_in_path ("diff");
  if (diff_cmd)
    {
      GSubprocess *process;
      GBytes *output;

      process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                                  | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                  error,
                                  diff_cmd, "--strip-trailing-cr", "-u", file1, "-", NULL);
      if (process == NULL)
        return NULL;

      if (!g_subprocess_communicate (process,
                                     input,
                                     NULL,
                                     &output,
                                     NULL,
                                     error))
        {
          g_object_unref (process);
          return NULL;
        }

      if (g_subprocess_get_successful (process))
        {
          g_clear_pointer (&output, g_bytes_unref);
        }
      else if (g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) == 1)
        {
          gsize size;

          /* this is the condition when the files differ */
          diff = g_bytes_unref_to_data (output, &size);
          output = NULL;
        }
      else
        {
          g_clear_pointer (&output, g_bytes_unref);
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                      "The `diff' process exited with error status %d",
                      g_subprocess_get_exit_status (process));
        }

      g_object_unref (process);
    }
  else
    {
      char *buf1;
      const char *buf2;
      gsize len1, len2;

      buf2 = g_bytes_get_data (input, &len2);

      if (!g_file_get_contents (file1, &buf1, &len1, error))
        return NULL;

      if ((len2 != len1) ||
          strncmp (buf2, buf1, len1) != 0)
        diff = g_strdup ("Files differ.\n");

      g_free (buf1);
    }

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
