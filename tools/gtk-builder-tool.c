/*  Copyright 2015 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtkbuilderprivate.h"
#include "gtk-builder-tool.h"

static void G_GNUC_NORETURN
usage (void)
{
  g_print (_("Usage:\n"
             "  gtk-builder-tool [COMMAND] [OPTION…] FILE\n"
             "\n"
             "Commands:\n"
             "  validate     Validate the file\n"
             "  simplify     Simplify the file\n"
             "  enumerate    List all named objects\n"
             "  preview      Preview the file\n"
             "\n"
             "Simplify Options:\n"
             "  --replace    Replace the file\n"
             "  --3to4       Convert from GTK 3 to GTK 4\n"
             "\n"
             "Preview Options:\n"
             "  --id=ID      Preview only the named object\n"
             "  --css=FILE   Use style from CSS file\n"
             "\n"
             "Perform various tasks on GtkBuilder .ui files.\n"));
  exit (1);
}

/* A simplified version of g_log_writer_default_would_drop(), to avoid
 * bumping up the required version of GLib to 2.68
 */
static gboolean
would_drop (GLogLevelFlags  level,
            const char     *domain)
{
  return (level & (G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING)) == 0;
}

static GLogWriterOutput
log_writer_func (GLogLevelFlags   level,
                 const GLogField *fields,
                 gsize            n_fields,
                 gpointer         user_data)
{
  gsize i;
  const char *domain = NULL;
  const char *message = NULL;

  for (i = 0; i < n_fields; i++)
    {
      if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0)
        domain = fields[i].value;
      else if (g_strcmp0 (fields[i].key, "MESSAGE") == 0)
        message = fields[i].value;
    }

  if (message != NULL && !would_drop (level, domain))
    {
      const char *prefix;
      switch (level & G_LOG_LEVEL_MASK)
        {
        case G_LOG_LEVEL_ERROR:
          prefix = "ERROR";
          break;
        case G_LOG_LEVEL_CRITICAL:
          prefix = "CRITICAL";
          break;
        case G_LOG_LEVEL_WARNING:
          prefix = "WARNING";
          break;
        default:
          prefix = "INFO";
          break;
        }
      g_printerr ("%s-%s: %s\n", domain, prefix, message);
    }

  return G_LOG_WRITER_HANDLED;
}

int
main (int argc, const char *argv[])
{
  g_set_prgname ("gtk-builder-tool");

  g_log_set_writer_func (log_writer_func, NULL, NULL);

  gtk_init ();

  gtk_test_register_all_types ();

  if (argc < 3)
    usage ();

  if (strcmp (argv[2], "--help") == 0)
    usage ();

  argv++;
  argc--;

  if (strcmp (argv[0], "validate") == 0)
    do_validate (&argc, &argv);
  else if (strcmp (argv[0], "simplify") == 0)
    do_simplify (&argc, &argv);
  else if (strcmp (argv[0], "enumerate") == 0)
    do_enumerate (&argc, &argv);
  else if (strcmp (argv[0], "preview") == 0)
    do_preview (&argc, &argv);
  else if (strcmp (argv[0], "precompile") == 0)
    do_precompile (&argc, &argv);
  else
    usage ();

  return 0;
}
