/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>
#include <gmodule.h>

#include "gtkutilsprivate.h"

/* Copied from pango-utils.c */

/* We need to call getc() a lot in a loop. This is suboptimal,
 * as getc() does thread locking on the FILE it is given.
 * To optimize that, lock the file first, then call getc(),
 * then unlock.
 * If locking functions are not present in libc, fall back
 * to the suboptimal getc().
 */
#if !defined(HAVE_FLOCKFILE) && !defined(HAVE__LOCK_FILE)
#  define flockfile(f) (void)1
#  define funlockfile(f) (void)1
#  define getc_unlocked(f) getc(f)
#elif !defined(HAVE_FLOCKFILE) && defined(HAVE__LOCK_FILE)
#  define flockfile(f) _lock_file(f)
#  define funlockfile(f) _unlock_file(f)
#  define getc_unlocked(f) _getc_nolock(f)
#endif

gboolean
gtk_scan_string (const char **pos, GString *out)
{
  const char *p = *pos, *q = *pos;
  char *tmp, *tmp2;
  gboolean quoted;

  while (g_ascii_isspace (*p))
    p++;

  if (!*p)
    return FALSE;
  else if (*p == '"')
    {
      p++;
      quoted = FALSE;
      for (q = p; (*q != '"') || quoted; q++)
        {
          if (!*q)
            return FALSE;
          quoted = (*q == '\\') && !quoted;
        }

      tmp = g_strndup (p, q - p);
      tmp2 = g_strcompress (tmp);
      g_string_truncate (out, 0);
      g_string_append (out, tmp2);
      g_free (tmp);
      g_free (tmp2);
    }

  q++;
  *pos = q;

  return TRUE;
}

gboolean
gtk_skip_space (const char **pos)
{
  const char *p = *pos;

  while (g_ascii_isspace (*p))
    p++;

  *pos = p;

  return !(*p == '\0');
}

gint
gtk_read_line (FILE *stream, GString *str)
{
  gboolean quoted = FALSE;
  gboolean comment = FALSE;
  int n_read = 0;
  int lines = 1;

  flockfile (stream);

  g_string_truncate (str, 0);

  while (1)
    {
      int c;

      c = getc_unlocked (stream);

      if (c == EOF)
        {
          if (quoted)
            g_string_append_c (str, '\\');

          goto done;
        }
      else
        n_read++;

      if (quoted)
        {
          quoted = FALSE;

          switch (c)
            {
            case '#':
              g_string_append_c (str, '#');
              break;
            case '\r':
            case '\n':
              {
                int next_c = getc_unlocked (stream);

                if (!(next_c == EOF ||
                      (c == '\r' && next_c == '\n') ||
                      (c == '\n' && next_c == '\r')))
                  ungetc (next_c, stream);

                lines++;

                break;
              }
            default:
              g_string_append_c (str, '\\');
              g_string_append_c (str, c);
            }
        }
      else
        {
          switch (c)
            {
            case '#':
              comment = TRUE;
              break;
            case '\\':
              if (!comment)
                quoted = TRUE;
              break;
            case '\n':
              {
                int next_c = getc_unlocked (stream);

                if (!(c == EOF ||
                      (c == '\r' && next_c == '\n') ||
                      (c == '\n' && next_c == '\r')))
                  ungetc (next_c, stream);

                goto done;
              }
            default:
              if (!comment)
               g_string_append_c (str, c);
            }
        }
    }

 done:
  funlockfile (stream);

  return (n_read > 0) ? lines : 0;
}

char *
gtk_trim_string (const char *str)
{
  int len;

  g_return_val_if_fail (str != NULL, NULL);

  while (*str && g_ascii_isspace (*str))
    str++;

  len = strlen (str);
  while (len > 0 && g_ascii_isspace (str[len - 1]))
    len--;

  return g_strndup (str, len);
}

char **
gtk_split_file_list (const char *str)
{
  int i = 0;
  int j;
  char **files;

  files = g_strsplit (str, G_SEARCHPATH_SEPARATOR_S, -1);

  while (files[i])
    {
      char *file = gtk_trim_string (files[i]);

      /* If the resulting file is empty, skip it */
      if (file[0] == '\0')
        {
          g_free (file);
          g_free (files[i]);

          for (j = i + 1; files[j]; j++)
            files[j - 1] = files[j];

          files[j - 1] = NULL;

          continue;
        }

#ifndef G_OS_WIN32
      /* '~' is a quite normal and common character in file names on
       * Windows, especially in the 8.3 versions of long file names, which
       * still occur now and then. Also, few Windows user are aware of the
       * Unix shell convention that '~' stands for the home directory,
       * even if they happen to have a home directory.
       */
      if (file[0] == '~' && file[1] == G_DIR_SEPARATOR)
        {
          char *tmp = g_strconcat (g_get_home_dir(), file + 1, NULL);
          g_free (file);
          file = tmp;
        }
      else if (file[0] == '~' && file[1] == '\0')
        {
          g_free (file);
          file = g_strdup (g_get_home_dir ());
        }
#endif

      g_free (files[i]);
      files[i] = file;

      i++;
    }

  return files;
}

GBytes *
gtk_file_load_bytes (GFile         *file,
                     GCancellable  *cancellable,
                     GError       **error)
{
  gchar *contents;
  gsize len;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  if (g_file_has_uri_scheme (file, "resource"))
    {
      gchar *uri, *unescaped;
      GBytes *bytes;

      uri = g_file_get_uri (file);
      unescaped = g_uri_unescape_string (uri + strlen ("resource://"), NULL);
      g_free (uri);

      bytes = g_resources_lookup_data (unescaped, 0, error);
      g_free (unescaped);

      return bytes;
    }

  /* contents is always \0 terminated, but we don't include that in the bytes */
  if (g_file_load_contents (file, cancellable, &contents, &len, NULL, error))
    return g_bytes_new_take (contents, len);

  return NULL;
}
