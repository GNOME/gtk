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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdk/gdk.h"

#include "gtkprivate.h"

#define STRICT
#include <windows.h>
#undef STRICT

static HMODULE gtk_dll;

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved)
{
  switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
      gtk_dll = (HMODULE) hinstDLL;
      break;
    }

  return TRUE;
}

const gchar *
_gtk_get_libdir (void)
{
  static char *gtk_libdir = NULL;
  if (gtk_libdir == NULL)
    {
      gchar *root = g_win32_get_package_installation_directory_of_module (gtk_dll);
      gchar *slash = strrchr (root, '\\');
      if (slash != NULL &&
          g_ascii_strcasecmp (slash + 1, ".libs") == 0)
        gtk_libdir = GTK_LIBDIR;
      else
        gtk_libdir = g_build_filename (root, "lib", NULL);
      g_free (root);
    }

  return gtk_libdir;
}

const gchar *
_gtk_get_localedir (void)
{
  static char *gtk_localedir = NULL;
  if (gtk_localedir == NULL)
    {
      const gchar *p;
      gchar *root, *temp;

      /* GTK_LOCALEDIR ends in either /lib/locale or
       * /share/locale. Scan for that slash.
       */
      p = GTK_LOCALEDIR + strlen (GTK_LOCALEDIR);
      while (*--p != '/')
        ;
      while (*--p != '/')
        ;

      root = g_win32_get_package_installation_directory_of_module (gtk_dll);
      temp = g_build_filename (root, p, NULL);
      g_free (root);

      /* gtk_localedir is passed to bindtextdomain() which isn't
       * UTF-8-aware.
       */
      gtk_localedir = g_win32_locale_filename_from_utf8 (temp);
      g_free (temp);
    }
  return gtk_localedir;
}

const gchar *
_gtk_get_datadir (void)
{
  static char *gtk_datadir = NULL;
  if (gtk_datadir == NULL)
    {
      gchar *root = g_win32_get_package_installation_directory_of_module (gtk_dll);
      gtk_datadir = g_build_filename (root, "share", NULL);
      g_free (root);
    }

  return gtk_datadir;
}

const gchar *
_gtk_get_sysconfdir (void)
{
  static char *gtk_sysconfdir = NULL;
  if (gtk_sysconfdir == NULL)
    {
      gchar *root = g_win32_get_package_installation_directory_of_module (gtk_dll);
      gtk_sysconfdir = g_build_filename (root, "etc", NULL);
      g_free (root);
    }

  return gtk_sysconfdir;
}

const gchar *
_gtk_get_data_prefix (void)
{
  static char *gtk_data_prefix = NULL;
  if (gtk_data_prefix == NULL)
    gtk_data_prefix = g_win32_get_package_installation_directory_of_module (gtk_dll);

  return gtk_data_prefix;
}
