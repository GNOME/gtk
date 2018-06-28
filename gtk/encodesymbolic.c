/* encodesymbolic.c
 * Copyright (C) 2014  Alexander Larsson <alexl@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <locale.h>

#include "gdkpixbufutilsprivate.h"

static gchar *output_dir = NULL;

static GOptionEntry args[] = {
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_dir, N_("Output to this directory instead of cwd"), NULL },
  { NULL }
};

int
main (int argc, char **argv)
{
  gchar *path, *basename, *pngpath, *pngfile, *dot;
  GOptionContext *context;
  GdkPixbuf *symbolic;
  GError *error;
  int width, height;
  gchar **sizev;
  GFileOutputStream *out;
  GFile *dest;
  char *data;
  gsize len;

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
#endif

  g_set_prgname ("gtk-encode-symbolic-svg");

  context = g_option_context_new ("PATH WIDTHxHEIGHT");
  g_option_context_add_main_entries (context, args, GETTEXT_PACKAGE);

  g_option_context_parse (context, &argc, &argv, NULL);

  if (argc < 3)
    {
      g_printerr ("%s\n", g_option_context_get_help (context, FALSE, NULL));
      return 1;
    }

  width = 0;
  height = 0;
  sizev = g_strsplit (argv[2], "x", 0);
  if (g_strv_length (sizev) == 2)
    {
      width = atoi(sizev[0]);
      height = atoi(sizev[1]);
    }
  g_strfreev (sizev);

  if (width == 0 || height == 0)
    {
      g_printerr (_("Invalid size %s\n"), argv[2]);
      return 1;
    }

  path = argv[1];
#ifdef G_OS_WIN32
  path = g_locale_to_utf8 (path, -1, NULL, NULL, NULL);
#endif

  error = NULL;
  if (!g_file_get_contents (path, &data, &len, &error))
    {
      g_printerr (_("Can’t load file: %s\n"), error->message);
      return 1;
    }

  symbolic = gtk_make_symbolic_pixbuf_from_data (data, len, width, height, 1.0, &error);
  if (symbolic == NULL)
    {
      g_printerr (_("Can’t load file: %s\n"), error->message);
      return 1;
    }

  g_free (data);

  basename = g_path_get_basename (path);

  dot = strrchr (basename, '.');
  if (dot != NULL)
    *dot = 0;
  pngfile = g_strconcat (basename, ".symbolic.png", NULL);
  g_free (basename);

  if (output_dir != NULL)
    pngpath = g_build_filename (output_dir, pngfile, NULL);
  else
    pngpath = g_strdup (pngfile);

  g_free (pngfile);

  dest = g_file_new_for_path (pngpath);


  out = g_file_replace (dest,
			NULL, FALSE,
			G_FILE_CREATE_REPLACE_DESTINATION,
			NULL, &error);
  if (out == NULL)
    {
      g_printerr (_("Can’t save file %s: %s\n"), pngpath, error->message);
      return 1;
    }

  if (!gdk_pixbuf_save_to_stream (symbolic, G_OUTPUT_STREAM (out), "png", NULL, &error, NULL))
    {
      g_printerr (_("Can’t save file %s: %s\n"), pngpath, error->message);
      return 1;
    }

  if (!g_output_stream_close (G_OUTPUT_STREAM (out), NULL, &error))
    {
      g_printerr (_("Can’t close stream"));
      return 1;
    }

  g_object_unref (out);
  g_free (pngpath);

  return 0;
}
