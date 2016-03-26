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
