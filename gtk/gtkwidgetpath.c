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
  GQuark name;
  GHashTable *regions;
  GArray *classes;
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
      new.name = elem->name;

      if (elem->regions)
        {
          GHashTableIter iter;
          gpointer key, value;

          g_hash_table_iter_init (&iter, elem->regions);
          new.regions = g_hash_table_new (NULL, NULL);

          while (g_hash_table_iter_next (&iter, &key, &value))
            g_hash_table_insert (new.regions, key, value);
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

      if (elem->regions)
        g_hash_table_destroy (elem->regions);
    }

  g_array_free (path->elems, TRUE);
  g_slice_free (GtkWidgetPath, path);
}

guint
gtk_widget_path_length (const GtkWidgetPath *path)
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
gtk_widget_path_iter_get_widget_type (const GtkWidgetPath *path,
                                      guint                pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (pos < path->elems->len, G_TYPE_INVALID);

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->type;
}

void
gtk_widget_path_iter_set_widget_type (GtkWidgetPath *path,
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
gtk_widget_path_iter_get_name (const GtkWidgetPath *path,
                               guint                pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (pos < path->elems->len, NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return g_quark_to_string (elem->name);
}

void
gtk_widget_path_iter_set_name (GtkWidgetPath *path,
                               guint          pos,
                               const gchar   *name)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  elem->name = g_quark_from_string (name);
}

gboolean
gtk_widget_path_iter_has_qname (const GtkWidgetPath *path,
                                guint                pos,
                                GQuark               qname)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (qname != 0, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  return (elem->name == qname);
}

gboolean
gtk_widget_path_iter_has_name (const GtkWidgetPath *path,
                               guint                pos,
                               const gchar         *name)
{
  GQuark qname;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

  return gtk_widget_path_iter_has_qname (path, pos, qname);
}

void
gtk_widget_path_iter_add_class (GtkWidgetPath *path,
                                guint          pos,
                                const gchar   *name)
{
  GtkPathElement *elem;
  gboolean added = FALSE;
  GQuark qname;
  guint i;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  qname = g_quark_from_string (name);

  if (!elem->classes)
    elem->classes = g_array_new (FALSE, FALSE, sizeof (GQuark));

  for (i = 0; i < elem->classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (elem->classes, GQuark, i);

      if (qname == quark)
        {
          /* Already there */
          added = TRUE;
          break;
        }
      if (qname < quark)
        {
          g_array_insert_val (elem->classes, i, qname);
          added = TRUE;
          break;
        }
    }

  if (!added)
    g_array_append_val (elem->classes, qname);
}

void
gtk_widget_path_iter_remove_class (GtkWidgetPath *path,
                                   guint          pos,
                                   const gchar   *name)
{
  GtkPathElement *elem;
  GQuark qname;
  guint i;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  qname = g_quark_try_string (name);

  if (qname == 0)
    return;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->classes)
    return;

  for (i = 0; i < elem->classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (elem->classes, GQuark, i);

      if (quark > qname)
        break;
      else if (quark == qname)
        {
          g_array_remove_index (elem->classes, i);
          break;
        }
    }
}

void
gtk_widget_path_iter_clear_classes (GtkWidgetPath *path,
                                    guint          pos)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->classes)
    return;

  if (elem->classes->len > 0)
    g_array_remove_range (elem->classes, 0, elem->classes->len);
}

GSList *
gtk_widget_path_iter_list_classes (const GtkWidgetPath *path,
                                   guint                pos)
{
  GtkPathElement *elem;
  GSList *list = NULL;
  guint i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (pos < path->elems->len, NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->classes)
    return NULL;

  for (i = 0; i < elem->classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (elem->classes, GQuark, i);
      list = g_slist_prepend (list, (gchar *) g_quark_to_string (quark));
    }

  return g_slist_reverse (list);
}

gboolean
gtk_widget_path_iter_has_qclass (const GtkWidgetPath *path,
                                 guint                pos,
                                 GQuark               qname)
{
  GtkPathElement *elem;
  guint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);
  g_return_val_if_fail (qname != 0, FALSE);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->classes)
    return FALSE;

  for (i = 0; i < elem->classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (elem->classes, GQuark, i);

      if (quark == qname)
        return TRUE;
      else if (quark > qname)
        break;
    }

  return FALSE;
}

gboolean
gtk_widget_path_iter_has_class (const GtkWidgetPath *path,
                                guint                pos,
                                const gchar         *name)
{
  GQuark qname;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

  return gtk_widget_path_iter_has_qclass (path, pos, qname);
}

void
gtk_widget_path_iter_add_region (GtkWidgetPath  *path,
                                 guint           pos,
                                 const gchar    *name,
                                 GtkRegionFlags  flags)
{
  GtkPathElement *elem;
  GQuark qname;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  qname = g_quark_from_string (name);

  if (!elem->regions)
    elem->regions = g_hash_table_new (NULL, NULL);

  g_hash_table_insert (elem->regions,
                       GUINT_TO_POINTER (qname),
                       GUINT_TO_POINTER (flags));
}

void
gtk_widget_path_iter_remove_region (GtkWidgetPath *path,
                                    guint          pos,
                                    const gchar   *name)
{
  GtkPathElement *elem;
  GQuark qname;

  g_return_if_fail (path != NULL);
  g_return_if_fail (pos < path->elems->len);
  g_return_if_fail (name != NULL);

  qname = g_quark_try_string (name);

  if (qname == 0)
    return;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (elem->regions)
    g_hash_table_remove (elem->regions, GUINT_TO_POINTER (qname));
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
gtk_widget_path_iter_list_regions (const GtkWidgetPath *path,
                                   guint                pos)
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
    {
      GQuark qname;

      qname = GPOINTER_TO_UINT (key);
      list = g_slist_prepend (list, (gchar *) g_quark_to_string (qname));
    }

  return list;
}

gboolean
gtk_widget_path_iter_has_qregion (const GtkWidgetPath *path,
                                  guint                pos,
                                  GQuark               qname,
                                  GtkRegionFlags      *flags)
{
  GtkPathElement *elem;
  gpointer value;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);
  g_return_val_if_fail (qname != 0, FALSE);

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->regions)
    return FALSE;

  if (!g_hash_table_lookup_extended (elem->regions,
                                     GUINT_TO_POINTER (qname),
                                     NULL, &value))
    return FALSE;

  if (flags)
    *flags = GPOINTER_TO_UINT (value);

  return TRUE;
}

gboolean
gtk_widget_path_iter_has_region (const GtkWidgetPath *path,
                                 guint                pos,
                                 const gchar         *name,
                                 GtkRegionFlags      *flags)
{
  GQuark qname;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (pos < path->elems->len, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

  return gtk_widget_path_iter_has_qregion (path, pos, qname, flags);
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

#define __GTK_WIDGET_PATH_C__
#include "gtkaliasdef.c"
