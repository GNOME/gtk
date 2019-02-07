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

static const gchar *
object_get_name (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_name (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-name");
}

void
do_enumerate (int *argc, char ***argv)
{
  GtkBuilder *builder;
  GError *error = NULL;
  gint ret;
  GSList *list, *l;
  GObject *object;
  const gchar *name;
  const gchar *filename;

  filename = (*argv)[1];

  builder = gtk_builder_new ();
  ret = gtk_builder_add_from_file (builder, filename, &error);

  if (ret == 0)
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  list = gtk_builder_get_objects (builder);
  for (l = list; l; l = l->next)
    {
      object = l->data;
      name = object_get_name (object);
      if (g_str_has_prefix (name, "___") && g_str_has_suffix (name, "___"))
        continue;

      g_printf ("%s (%s)\n", name, g_type_name_from_instance ((GTypeInstance*)object));
    }
  g_slist_free (list);

  g_object_unref (builder);
}
