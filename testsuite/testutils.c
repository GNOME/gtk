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

#include <gio/gio.h>

#include "testsuite/testutils.h"

char *
diff_with_file (const char  *file1,
                const char  *text,
                gssize       len,
                GError     **error)
{
  char *diff = NULL;

  g_return_val_if_fail (file1 != NULL, NULL);
  g_return_val_if_fail (text != NULL, NULL);

  if (g_find_program_in_path ("diff"))
    {
      GSubprocess *process;
      GBytes *input, *output, *error_output;

      if (len < 0)
        len = strlen (text);

      process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                                  | G_SUBPROCESS_FLAGS_STDOUT_PIPE
                                  | G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                  error,
                                  "diff", "-u", file1, "-", NULL);
      if (process == NULL)
        return NULL;

      input = g_bytes_new_static (text, len);
      if (!g_subprocess_communicate (process,
                                     input,
                                     NULL,
                                     &output,
                                     &error_output,
                                     error))
        {
          g_object_unref (process);
          g_bytes_unref (input);
          return NULL;
        }
      g_bytes_unref (input);

      if (error_output)
        {
          const char *error_text = g_bytes_get_data (error_output, NULL);
          if (error_text && error_text[0])
            {
              g_test_message ("%s", error_text);
            }

          g_bytes_unref (error_output);
        }

      if (!g_subprocess_get_successful (process) &&
          /* this is the condition when the files differ */
          !(g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) == 1))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "The `diff' process exited with error status %d",
                       g_subprocess_get_exit_status (process));
        }
      else
        {
          diff = g_strdup (g_bytes_get_data (output, NULL));
        }

      g_object_unref (process);
      g_clear_pointer (&output, g_bytes_unref);

      return diff;
    }
  else
    {
      char *buf1;
      gsize len1;

      if (!g_file_get_contents (file1, &buf1, &len1, error))
        return NULL;

      if ((len != -1 && len != len1) ||
          strncmp (text, buf1, len1) != 0)
        diff = g_strdup ("Files differ.\n");

      g_free (buf1);
    }

  return diff;
}
