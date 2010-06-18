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
  GHashTable *regions;
};

struct GtkWidgetPath
{
  GArray *elems; /* First element contains the described widget */
};

GtkWidgetPath *
gtk_widget_path_new (void)
{
  GtkWidgetPath *path;

  path = g_slice_new0 (GtkWidgetPath);
  path->elems = g_array_new (FALSE, TRUE, sizeof (GtkPathElement));

  return path;
}

GtkWidgetPath *
gtk_widget_path_copy (const GtkWidgetPath *path)
{
  GtkWidgetPath *new_path;
  guint i;

  g_return_val_if_fail (path != NULL, NULL);

  new_path = gtk_widget_path_new ();

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem, new = { 0 };

      elem = &g_array_index (path->elems, GtkPathElement, i);

      new.type = elem->type;
      new.name = g_strdup (elem->name);

      if (elem->regions)
        {
          GHashTableIter iter;
          gpointer key, value;

          g_hash_table_iter_init (&iter, elem->regions);
          new.regions = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               (GDestroyNotify) g_free,
                                               NULL);

          while (g_hash_table_iter_next (&iter, &key, &value))
            g_hash_table_insert (new.regions,
                                 g_strdup ((const gchar *) key),
                                 value);
        }

      g_array_append_val (new_path->elems, new);
    }

  return new_path;
}

void
gtk_widget_path_free (GtkWidgetPath *path)
{
  guint i;

  g_return_if_fail (path != NULL);

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);
      g_free (elem->name);

      if (elem->regions)
        g_hash_table_destroy (elem->regions);
    }

  g_array_free (path->elems, TRUE);
  g_slice_free (GtkWidgetPath, path);
}

guint
gtk_widget_path_length (GtkWidgetPath *path)
{
  g_return_val_if_fail (path != NULL, 0);

  return path->elems->len;
}

guint
gtk_widget_path_prepend_type (GtkWidgetPath *path,
                              GType          type)
{
  GtkPathElement new = { 0 };

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), 0);

  new.type = type;
  g_array_append_val (path->elems, new);

  return path->elems->len - 1;
}

GType
gtk_widget_path_get_element_type (GtkWidgetPath *path,
                                  guint          pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (pos < path->elems->len, G_TYPE_INVALID);

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->type;
}

void
gtk_widget_path_set_element_type (GtkWidgetPath *path,
                                  guint          pos,
                                  GType          type)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET));

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  elem->type = type;
}

G_CONST_RETURN gchar *
gtk_widget_path_get_element_name (GtkWidgetPath *path,
                                  guint          pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (pos < path->elems->len, NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->name;
}

void
gtk_widget_path_set_element_name (GtkWidgetPath *path,
                                  guint          pos,
                                  const gchar   *name)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (elem->name)
    g_free (elem->name);

  elem->name = g_strdup (name);
}

void
gtk_widget_path_iter_add_region (GtkWidgetPath      *path,
                                 guint               pos,
                                 const gchar        *name,
                                 GtkChildClassFlags  flags)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->regions)
    elem->regions = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           (GDestroyNotify) g_free,
                                           NULL);

  g_hash_table_insert (elem->regions,
                       g_strdup (name),
                       GUINT_TO_POINTER (flags));
}

void
gtk_widget_path_iter_remove_region (GtkWidgetPath *path,
                                    guint          pos,
                                    const gchar   *name)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (elem->regions)
    g_hash_table_remove (elem->regions, name);
}

void
gtk_widget_path_iter_clear_regions (GtkWidgetPath *path,
                                    guint          pos)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (elem->regions)
    g_hash_table_remove_all (elem->regions);
}

GSList *
gtk_widget_path_iter_list_regions (GtkWidgetPath *path,
                                   guint          pos)
{
  GtkPathElement *elem;
  GHashTableIter iter;
  GSList *list = NULL;
  gpointer key;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (pos < path->elems->len, NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->regions)
    return NULL;

  g_hash_table_iter_init (&iter, elem->regions);

  while (g_hash_table_iter_next (&iter, &key, NULL))
    list = g_slist_prepend (list, key);

  return list;
}

gboolean
gtk_widget_path_iter_has_region (GtkWidgetPath      *path,
                                 guint               pos,
                                 const gchar        *name,
                                 GtkChildClassFlags *flags)
{
  GtkPathElement *elem;
  gpointer value;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->regions)
    return FALSE;

  if (!g_hash_table_lookup_extended (elem->regions, name, NULL, &value))
    return FALSE;

  if (flags)
    *flags = GPOINTER_TO_UINT (value);

  return TRUE;
}

GType
gtk_widget_path_get_widget_type (const GtkWidgetPath *path)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);

  elem = &g_array_index (path->elems, GtkPathElement, 0);
  return elem->type;
}

gboolean
gtk_widget_path_is_type (const GtkWidgetPath *path,
                         GType                type)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), FALSE);

  elem = &g_array_index (path->elems, GtkPathElement, 0);

  if (elem->type == type ||
      g_type_is_a (elem->type, type))
    return TRUE;

  return FALSE;
}

gboolean
gtk_widget_path_has_parent (const GtkWidgetPath *path,
                            GType                type)
{
  guint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), FALSE);

  for (i = 1; i < path->elems->len; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      if (elem->type == type ||
          g_type_is_a (elem->type, type))
        return TRUE;
    }

  return FALSE;
}
