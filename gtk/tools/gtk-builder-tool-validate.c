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

static GType
make_fake_type (const char *type_name,
                const char *parent_name)
{
  GType parent_type;
  GTypeQuery query;

  parent_type = g_type_from_name (parent_name);
  if (parent_type == G_TYPE_INVALID)
    {
      g_printerr ("Failed to lookup template parent type %s\n", parent_name);
      exit (1);
    }

  g_type_query (parent_type, &query);
  return g_type_register_static_simple (parent_type,
                                        type_name,
                                        query.class_size,
                                        NULL,
                                        query.instance_size,
                                        NULL,
                                        0);
}

static void
do_validate_template (const char *filename,
                      const char *type_name,
                      const char *parent_name)
{
  GType template_type;
  GObject *object;
  GtkBuilder *builder;
  GError *error = NULL;
  int ret;

  /* Only make a fake type if it doesn't exist yet.
   * This lets us e.g. validate the GtkFileChooserWidget template.
   */
  template_type = g_type_from_name (type_name);
  if (template_type == G_TYPE_INVALID)
    template_type = make_fake_type (type_name, parent_name);

  object = g_object_new (template_type, NULL);
  if (!object)
    {
      g_printerr ("Failed to create an instance of the template type %s\n", type_name);
      exit (1);
    }

  builder = gtk_builder_new ();
  ret = gtk_builder_extend_with_template (builder, object , template_type, " ", 1, &error);
  if (ret)
    ret = gtk_builder_add_from_file (builder, filename, &error);
  g_object_unref (builder);

  if (ret == 0)
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }
}

static gboolean
parse_template_error (const char   *message,
                      char        **class_name,
                      char        **parent_name)
{
  char *p;

  p = strstr (message, "(class '");
  if (p)
    {
      *class_name = g_strdup (p + strlen ("(class '"));
      p = strstr (*class_name, "'");
      if (p)
        *p = '\0';
    }
  p = strstr (message, ", parent '");
  if (p)
    {
      *parent_name = g_strdup (p + strlen (", parent '"));
      p = strstr (*parent_name, "'");
      if (p)
        *p = '\0';
    }

  return TRUE;
}

static gboolean
validate_file (const char *filename)
{
  GtkBuilder *builder;
  GError *error = NULL;
  int ret;
  char *class_name = NULL;
  char *parent_name = NULL;

  builder = gtk_builder_new ();
  ret = gtk_builder_add_from_file (builder, filename, &error);
  g_object_unref (builder);

  if (ret == 0)
    {
      if (g_error_matches (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_UNHANDLED_TAG)  &&
          parse_template_error (error->message, &class_name, &parent_name))
        {
          do_validate_template (filename, class_name, parent_name);
        }
      else
        {
          g_printerr ("%s\n", error->message);
          return FALSE;
        }
    }

  return TRUE;
}

void
do_validate (int *argc, const char ***argv)
{
  int i;

  for (i = 1; i < *argc; i++)
    {
      if (!validate_file ((*argv)[i]))
        exit (1);
    }
}
