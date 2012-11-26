/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 *
 * Author: Tomas Bzatek <tbzatek@redhat.com>
 */

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gdesktopappinfo.h>
#endif
#include <gtk.h>

static gchar **args = NULL;

static GOptionEntry entries[] = {
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, NULL },
  { NULL}
};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context = NULL;
  gchar *summary;
  gchar *app_name;
#ifdef G_OS_UNIX
  gchar *desktop_file_name;
#endif
  GAppInfo *info = NULL;
  GAppLaunchContext *launch_context;
  GList *l;
  GFile *f;

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
  textdomain (GETTEXT_PACKAGE);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
#endif

  /* Translators: this message will appear immediately after the */
  /* usage string - Usage: COMMAND [OPTION]… <THIS_MESSAGE>    */
  context =
    g_option_context_new (_("APPLICATION [URI…] — launch an APPLICATION with URI."));

  /* Translators: this message will appear after the usage string */
  /* and before the list of options.                              */
  summary = _("Launch specified application by its desktop file info\n"
              "optionally passing list of URIs as arguments.");
  g_option_context_set_summary (context, summary);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (FALSE));

  g_option_context_parse (context, &argc, &argv, &error);

  g_option_context_free (context);

  if (error != NULL)
    {
      g_printerr (_("Error parsing commandline options: %s\n"), error->message);
      g_printerr ("\n");
      g_printerr (_("Try \"%s --help\" for more information."),
                  g_get_prgname ());
      g_printerr ("\n");
      g_error_free(error);
      return 1;
    }

  if (!args)
    {
      /* Translators: the %s is the program name. This error message */
      /* means the user is calling gtk-launch without any argument.  */
      g_printerr (_("%s: missing application name"), g_get_prgname ());
      g_printerr ("\n");
      g_printerr (_("Try \"%s --help\" for more information."),
                  g_get_prgname ());
      g_printerr ("\n");
      return 1;
    }


  gtk_init (&argc, &argv);

  app_name = *args;
#ifdef G_OS_UNIX
  if (g_str_has_suffix (app_name, ".desktop"))
    desktop_file_name = g_strdup (app_name);
  else 
    desktop_file_name = g_strconcat (app_name, ".desktop", NULL);
  info = G_APP_INFO (g_desktop_app_info_new (desktop_file_name));
  g_free (desktop_file_name);
#else
#warning Please add support for creating AppInfo from id for your OS
  g_printerr (_("Creating AppInfo from id not supported on non unix operating systems"));
#endif
  args++;

  if (!info)
    {
      /* Translators: the first %s is the program name, the second one */
      /* is the application name.                                      */
      g_printerr (_("%s: no such application %s"),
                  g_get_prgname (), app_name);
      g_printerr ("\n");
      return 2;
    }

  l = NULL;
  for (; *args; args++)
    {
      f = g_file_new_for_commandline_arg (*args);
      l = g_list_append (l, f);
    }

  launch_context = (GAppLaunchContext*) gdk_display_get_app_launch_context (gdk_display_get_default ());
  if (!g_app_info_launch (info, l, launch_context, &error))
    {
       /* Translators: the first %s is the program name, the second one  */
       /* is the error message.                                          */
       g_printerr (_("%s: error launching application: %s\n"),
                   g_get_prgname (), error->message);
       return 3;
    }
  g_object_unref (info);
  g_object_unref (launch_context);
  g_list_free_full (l, g_object_unref);

  return 0;
}
