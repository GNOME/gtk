/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include "gtkwidget.h"
#include "gtkwidgetpath.h"

typedef struct GtkPathElement GtkPathElement;

struct GtkPathElement
{
  GType type;
  gchar *name;
};

struct GtkWidgetPath
{
  GSList *elems;
  GSList *last; /* Last element contains the described widget */
};

GtkWidgetPath *
gtk_widget_path_new (void)
{
  GtkWidgetPath *path;

  path = g_slice_new0 (GtkWidgetPath);

  return path;
}

static GtkPathElement *
path_element_new (GType        type,
                  const gchar *name)
{
  GtkPathElement *elem;

  elem = g_slice_new (GtkPathElement);
  elem->type = type;
  elem->name = g_strdup (name);

  return elem;
}

static void
path_element_free (GtkPathElement *elem)
{
  g_free (elem->name);
  g_slice_free (GtkPathElement, elem);
}

void
gtk_widget_path_prepend_widget_desc (GtkWidgetPath *path,
                                     GType          type,
                                     const gchar   *name)
{
  g_return_if_fail (path != NULL);
  g_return_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET));

  if (!path->elems)
    {
      path->elems = g_slist_prepend (NULL, path_element_new (type, name));
      path->last = path->elems;
    }
  else
    {
      path->last->next = g_slist_alloc ();
      path->last->next->data = path_element_new (type, name);
      path->last = path->last->next;
    }
}

GtkWidgetPath *
gtk_widget_path_copy (GtkWidgetPath *path)
{
  GtkWidgetPath *new_path;
  GSList *elems;

  new_path = gtk_widget_path_new ();
  elems = path->elems;

  while (elems)
    {
      GtkPathElement *elem;
      GSList *link;

      elem = elems->data;
      link = g_slist_alloc ();
      link->data = path_element_new (elem->type, elem->name);

      if (!new_path->elems)
        new_path->last = new_path->elems = link;
      else
        {
          new_path->last->next = link;
          new_path->last = link;
        }

      elems = elems->next;
    }

  return new_path;
}

void
gtk_widget_path_free (GtkWidgetPath *path)
{
  g_return_if_fail (path != NULL);

  g_slist_foreach (path->elems, (GFunc) path_element_free, NULL);
  g_slist_free (path->elems);

  g_slice_free (GtkWidgetPath, path);
}

gboolean
gtk_widget_path_has_parent (GtkWidgetPath *path,
                            GType          type)
{
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), FALSE);

  GSList *elems = path->elems;

  while (elems)
    {
      GtkPathElement *elem;

      elem = elems->data;

      if (elem->type == type ||
          g_type_is_a (elem->type, type))
        return TRUE;

      elems = elems->next;
    }

  return FALSE;
}

void
gtk_widget_path_foreach (GtkWidgetPath            *path,
                         GtkWidgetPathForeachFunc  func,
                         gpointer                  user_data)
{
  GSList *elems;

  g_return_if_fail (path != NULL);
  g_return_if_fail (func != NULL);

  elems = path->elems;

  while (elems)
    {
      GtkPathElement *elem;

      elem = elems->data;
      elems = elems->next;

      if ((func) (elem->type, elem->name, user_data))
        return;
    }
}
