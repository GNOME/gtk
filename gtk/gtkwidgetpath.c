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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include "gtkwidget.h"
#include "gtkwidgetpath.h"
#include "gtkstylecontextprivate.h"

/**
 * SECTION:gtkwidgetpath
 * @Short_description: Widget path abstraction
 * @Title: GtkWidgetPath
 * @See_also: #GtkStyleContext
 *
 * GtkWidgetPath is a boxed type that represents a widget hierarchy from
 * the topmost widget, typically a toplevel, to any child. This widget
 * path abstraction is used in #GtkStyleContext on behalf of the real
 * widget in order to query style information.
 *
 * If you are using GTK+ widgets, you probably will not need to use
 * this API directly, as there is gtk_widget_get_path(), and the style
 * context returned by gtk_widget_get_style_context() will be automatically
 * updated on widget hierarchy changes.
 *
 * The widget path generation is generally simple:
 *
 * ## Defining a button within a window
 *
 * |[<!-- language="C" -->
 * {
 *   GtkWidgetPath *path;
 *
 *   path = gtk_widget_path_new ();
 *   gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
 *   gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
 * }
 * ]|
 *
 * Although more complex information, such as widget names, or
 * different classes (property that may be used by other widget
 * types) and intermediate regions may be included:
 *
 * ## Defining the first tab widget in a notebook
 *
 * |[<!-- language="C" -->
 * {
 *   GtkWidgetPath *path;
 *   guint pos;
 *
 *   path = gtk_widget_path_new ();
 *
 *   pos = gtk_widget_path_append_type (path, GTK_TYPE_NOTEBOOK);
 *   gtk_widget_path_iter_add_region (path, pos, "tab", GTK_REGION_EVEN | GTK_REGION_FIRST);
 *
 *   pos = gtk_widget_path_append_type (path, GTK_TYPE_LABEL);
 *   gtk_widget_path_iter_set_name (path, pos, "first tab label");
 * }
 * ]|
 *
 * All this information will be used to match the style information
 * that applies to the described widget.
 **/

G_DEFINE_BOXED_TYPE (GtkWidgetPath, gtk_widget_path,
		     gtk_widget_path_ref, gtk_widget_path_unref)


typedef struct GtkPathElement GtkPathElement;

struct GtkPathElement
{
  GType type;
  GQuark name;
  guint sibling_index;
  GHashTable *regions;
  GArray *classes;
  GtkWidgetPath *siblings;
};

struct _GtkWidgetPath
{
  volatile guint ref_count;

  GArray *elems; /* First element contains the described widget */
};

/**
 * gtk_widget_path_new:
 *
 * Returns an empty widget path.
 *
 * Returns: (transfer full): A newly created, empty, #GtkWidgetPath
 *
 * Since: 3.0
 **/
GtkWidgetPath *
gtk_widget_path_new (void)
{
  GtkWidgetPath *path;

  path = g_slice_new0 (GtkWidgetPath);
  path->elems = g_array_new (FALSE, TRUE, sizeof (GtkPathElement));
  path->ref_count = 1;

  return path;
}

static void
gtk_path_element_copy (GtkPathElement       *dest,
                       const GtkPathElement *src)
{
  memset (dest, 0, sizeof (GtkPathElement));

  dest->type = src->type;
  dest->name = src->name;
  if (src->siblings)
    dest->siblings = gtk_widget_path_ref (src->siblings);
  dest->sibling_index = src->sibling_index;

  if (src->regions)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, src->regions);
      dest->regions = g_hash_table_new (NULL, NULL);

      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (dest->regions, key, value);
    }

  if (src->classes)
    {
      dest->classes = g_array_new (FALSE, FALSE, sizeof (GQuark));
      g_array_append_vals (dest->classes, src->classes->data, src->classes->len);
    }
}

/**
 * gtk_widget_path_copy:
 * @path: a #GtkWidgetPath
 *
 * Returns a copy of @path
 *
 * Returns: (transfer full): a copy of @path
 *
 * Since: 3.0
 **/
GtkWidgetPath *
gtk_widget_path_copy (const GtkWidgetPath *path)
{
  GtkWidgetPath *new_path;
  guint i;

  g_return_val_if_fail (path != NULL, NULL);

  new_path = gtk_widget_path_new ();

  g_array_set_size (new_path->elems, path->elems->len);

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem, *dest;

      elem = &g_array_index (path->elems, GtkPathElement, i);
      dest = &g_array_index (new_path->elems, GtkPathElement, i);

      gtk_path_element_copy (dest, elem);
    }

  return new_path;
}

/**
 * gtk_widget_path_ref:
 * @path: a #GtkWidgetPath
 *
 * Increments the reference count on @path.
 *
 * Returns: @path itself.
 *
 * Since: 3.2
 **/
GtkWidgetPath *
gtk_widget_path_ref (GtkWidgetPath *path)
{
  g_return_val_if_fail (path != NULL, path);

  g_atomic_int_add (&path->ref_count, 1);

  return path;
}

/**
 * gtk_widget_path_unref:
 * @path: a #GtkWidgetPath
 *
 * Decrements the reference count on @path, freeing the structure
 * if the reference count reaches 0.
 *
 * Since: 3.2
 **/
void
gtk_widget_path_unref (GtkWidgetPath *path)
{
  guint i;

  g_return_if_fail (path != NULL);

  if (!g_atomic_int_dec_and_test (&path->ref_count))
    return;

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      if (elem->regions)
        g_hash_table_destroy (elem->regions);

      if (elem->classes)
        g_array_free (elem->classes, TRUE);

      if (elem->siblings)
        gtk_widget_path_unref (elem->siblings);
    }

  g_array_free (path->elems, TRUE);
  g_slice_free (GtkWidgetPath, path);
}

/**
 * gtk_widget_path_free:
 * @path: a #GtkWidgetPath
 *
 * Decrements the reference count on @path, freeing the structure
 * if the reference count reaches 0.
 *
 * Since: 3.0
 **/
void
gtk_widget_path_free (GtkWidgetPath *path)
{
  g_return_if_fail (path != NULL);

  gtk_widget_path_unref (path);
}

/**
 * gtk_widget_path_length:
 * @path: a #GtkWidgetPath
 *
 * Returns the number of #GtkWidget #GTypes between the represented
 * widget and its topmost container.
 *
 * Returns: the number of elements in the path
 *
 * Since: 3.0
 **/
gint
gtk_widget_path_length (const GtkWidgetPath *path)
{
  g_return_val_if_fail (path != NULL, 0);

  return path->elems->len;
}

/**
 * gtk_widget_path_to_string:
 * @path: the path
 *
 * Dumps the widget path into a string representation. It tries to match
 * the CSS style as closely as possible (Note that there might be paths
 * that cannot be represented in CSS).
 *
 * The main use of this code is for debugging purposes, so that you can
 * g_print() the path or dump it in a gdb session.
 *
 * Returns: A new string describing @path.
 *
 * Since: 3.2
 **/
char *
gtk_widget_path_to_string (const GtkWidgetPath *path)
{
  GString *string;
  guint i, j;

  g_return_val_if_fail (path != NULL, NULL);

  string = g_string_new ("");

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      if (i > 0)
        g_string_append_c (string, ' ');

      g_string_append (string, g_type_name (elem->type));

      if (elem->name)
        {
          g_string_append_c (string, '(');
          g_string_append (string, g_quark_to_string (elem->name));
          g_string_append_c (string, ')');
        }


      if (elem->siblings)
        g_string_append_printf (string, "[%d/%d]",
                                elem->sibling_index + 1,
                                gtk_widget_path_length (elem->siblings));

      if (elem->classes)
        {
          for (j = 0; j < elem->classes->len; j++)
            {
              g_string_append_c (string, '.');
              g_string_append (string, g_quark_to_string (g_array_index (elem->classes, GQuark, j)));
            }
        }

      if (elem->regions)
        {
          GHashTableIter iter;
          gpointer key, value;

          g_hash_table_iter_init (&iter, elem->regions);
          while (g_hash_table_iter_next (&iter, &key, &value))
            {
              GtkRegionFlags flags = GPOINTER_TO_UINT (value);
              static const char *flag_names[] = {
                "even",
                "odd",
                "first",
                "last",
                "only",
                "sorted"
              };

              g_string_append_c (string, ' ');
              g_string_append (string, g_quark_to_string (GPOINTER_TO_UINT (key)));
              for (j = 0; j < G_N_ELEMENTS(flag_names); j++)
                {
                  if (flags & (1 << j))
                    {
                      g_string_append_c (string, ':');
                      g_string_append (string, flag_names[j]);
                    }
                }
            }
        }
    }

  return g_string_free (string, FALSE);
}

/**
 * gtk_widget_path_prepend_type:
 * @path: a #GtkWidgetPath
 * @type: widget type to prepend
 *
 * Prepends a widget type to the widget hierachy represented by @path.
 *
 * Since: 3.0
 **/
void
gtk_widget_path_prepend_type (GtkWidgetPath *path,
                              GType          type)
{
  GtkPathElement new = { 0 };

  g_return_if_fail (path != NULL);

  new.type = type;
  g_array_prepend_val (path->elems, new);
}

/**
 * gtk_widget_path_append_type:
 * @path: a #GtkWidgetPath
 * @type: widget type to append
 *
 * Appends a widget type to the widget hierarchy represented by @path.
 *
 * Returns: the position where the element was inserted
 *
 * Since: 3.0
 **/
gint
gtk_widget_path_append_type (GtkWidgetPath *path,
                             GType          type)
{
  GtkPathElement new = { 0 };

  g_return_val_if_fail (path != NULL, 0);

  new.type = type;
  g_array_append_val (path->elems, new);

  return path->elems->len - 1;
}

/**
 * gtk_widget_path_append_with_siblings:
 * @path: the widget path to append to
 * @siblings: a widget path describing a list of siblings. This path
 *   may not contain any siblings itself and it must not be modified
 *   afterwards.
 * @sibling_index: index into @siblings for where the added element is
 *   positioned.
 *
 * Appends a widget type with all its siblings to the widget hierarchy
 * represented by @path. Using this function instead of
 * gtk_widget_path_append_type() will allow the CSS theming to use
 * sibling matches in selectors and apply :nth-child() pseudo classes.
 * In turn, it requires a lot more care in widget implementations as
 * widgets need to make sure to call gtk_widget_reset_style() on all
 * involved widgets when the @siblings path changes.
 *
 * Returns: the position where the element was inserted.
 *
 * Since: 3.2
 **/
gint
gtk_widget_path_append_with_siblings (GtkWidgetPath *path,
                                      GtkWidgetPath *siblings,
                                      guint          sibling_index)
{
  GtkPathElement new;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (siblings != NULL, 0);
  g_return_val_if_fail (sibling_index < gtk_widget_path_length (siblings), 0);

  gtk_path_element_copy (&new, &g_array_index (siblings->elems, GtkPathElement, sibling_index));
  new.siblings = gtk_widget_path_ref (siblings);
  new.sibling_index = sibling_index;
  g_array_append_val (path->elems, new);

  return path->elems->len - 1;
}

/**
 * gtk_widget_path_iter_get_siblings:
 * @path: a #GtkWidgetPath
 * @pos: position to get the siblings for, -1 for the path head
 *
 * Returns the list of siblings for the element at @pos. If the element
 * was not added with siblings, %NULL is returned.
 *
 * Returns: %NULL or the list of siblings for the element at @pos.
 **/
const GtkWidgetPath *
gtk_widget_path_iter_get_siblings (const GtkWidgetPath *path,
                                   gint                 pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (path->elems->len != 0, G_TYPE_INVALID);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->siblings;
}

/**
 * gtk_widget_path_iter_get_sibling_index:
 * @path: a #GtkWidgetPath
 * @pos: position to get the sibling index for, -1 for the path head
 *
 * Returns the index into the list of siblings for the element at @pos as
 * returned by gtk_widget_path_iter_get_siblings(). If that function would
 * return %NULL because the element at @pos has no siblings, this function
 * will return 0.
 *
 * Returns: 0 or the index into the list of siblings for the element at @pos.
 **/
guint
gtk_widget_path_iter_get_sibling_index (const GtkWidgetPath *path,
                                        gint                 pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (path->elems->len != 0, G_TYPE_INVALID);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->sibling_index;
}

/**
 * gtk_widget_path_iter_get_object_type:
 * @path: a #GtkWidgetPath
 * @pos: position to get the object type for, -1 for the path head
 *
 * Returns the object #GType that is at position @pos in the widget
 * hierarchy defined in @path.
 *
 * Returns: a widget type
 *
 * Since: 3.0
 **/
GType
gtk_widget_path_iter_get_object_type (const GtkWidgetPath *path,
                                      gint                 pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (path->elems->len != 0, G_TYPE_INVALID);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->type;
}

/**
 * gtk_widget_path_iter_set_object_type:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @type: object type to set
 *
 * Sets the object type for a given position in the widget hierarchy
 * defined by @path.
 *
 * Since: 3.0
 **/
void
gtk_widget_path_iter_set_object_type (GtkWidgetPath *path,
                                      gint           pos,
                                      GType          type)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  elem->type = type;
}

/**
 * gtk_widget_path_iter_get_name:
 * @path: a #GtkWidgetPath
 * @pos: position to get the widget name for, -1 for the path head
 *
 * Returns the name corresponding to the widget found at
 * the position @pos in the widget hierarchy defined by
 * @path
 *
 * Returns: The widget name, or %NULL if none was set.
 **/
const gchar *
gtk_widget_path_iter_get_name (const GtkWidgetPath *path,
                               gint                 pos)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path->elems->len != 0, NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return g_quark_to_string (elem->name);
}

/**
 * gtk_widget_path_iter_set_name:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: widget name
 *
 * Sets the widget name for the widget found at position @pos
 * in the widget hierarchy defined by @path.
 *
 * Since: 3.0
 **/
void
gtk_widget_path_iter_set_name (GtkWidgetPath *path,
                               gint           pos,
                               const gchar   *name)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);
  g_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  elem->name = g_quark_from_string (name);
}

/**
 * gtk_widget_path_iter_has_qname:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 * @qname: widget name as a #GQuark
 *
 * See gtk_widget_path_iter_has_name(). This is a version
 * that operates on #GQuarks.
 *
 * Returns: %TRUE if the widget at @pos has this name
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_path_iter_has_qname (const GtkWidgetPath *path,
                                gint                 pos,
                                GQuark               qname)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path->elems->len != 0, FALSE);
  g_return_val_if_fail (qname != 0, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  return (elem->name == qname);
}

/**
 * gtk_widget_path_iter_has_name:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 * @name: a widget name
 *
 * Returns %TRUE if the widget at position @pos has the name @name,
 * %FALSE otherwise.
 *
 * Returns: %TRUE if the widget at @pos has this name
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_path_iter_has_name (const GtkWidgetPath *path,
                               gint                 pos,
                               const gchar         *name)
{
  GQuark qname;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path->elems->len != 0, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

  return gtk_widget_path_iter_has_qname (path, pos, qname);
}

/**
 * gtk_widget_path_iter_add_class:
 * @path: a #GtkWidget
 * @pos: position to modify, -1 for the path head
 * @name: a class name
 *
 * Adds the class @name to the widget at position @pos in
 * the hierarchy defined in @path. See
 * gtk_style_context_add_class().
 *
 * Since: 3.0
 **/
void
gtk_widget_path_iter_add_class (GtkWidgetPath *path,
                                gint           pos,
                                const gchar   *name)
{
  GtkPathElement *elem;
  gboolean added = FALSE;
  GQuark qname;
  guint i;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);
  g_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

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

/**
 * gtk_widget_path_iter_remove_class:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: class name
 *
 * Removes the class @name from the widget at position @pos in
 * the hierarchy defined in @path.
 *
 * Since: 3.0
 **/
void
gtk_widget_path_iter_remove_class (GtkWidgetPath *path,
                                   gint           pos,
                                   const gchar   *name)
{
  GtkPathElement *elem;
  GQuark qname;
  guint i;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);
  g_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

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

/**
 * gtk_widget_path_iter_clear_classes:
 * @path: a #GtkWidget
 * @pos: position to modify, -1 for the path head
 *
 * Removes all classes from the widget at position @pos in the
 * hierarchy defined in @path.
 *
 * Since: 3.0
 **/
void
gtk_widget_path_iter_clear_classes (GtkWidgetPath *path,
                                    gint           pos)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (!elem->classes)
    return;

  if (elem->classes->len > 0)
    g_array_remove_range (elem->classes, 0, elem->classes->len);
}

/**
 * gtk_widget_path_iter_list_classes:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 *
 * Returns a list with all the class names defined for the widget
 * at position @pos in the hierarchy defined in @path.
 *
 * Returns: (transfer container) (element-type utf8): The list of
 *          classes, This is a list of strings, the #GSList contents
 *          are owned by GTK+, but you should use g_slist_free() to
 *          free the list itself.
 *
 * Since: 3.0
 **/
GSList *
gtk_widget_path_iter_list_classes (const GtkWidgetPath *path,
                                   gint                 pos)
{
  GtkPathElement *elem;
  GSList *list = NULL;
  guint i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path->elems->len != 0, NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

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

/**
 * gtk_widget_path_iter_has_qclass:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 * @qname: class name as a #GQuark
 *
 * See gtk_widget_path_iter_has_class(). This is a version that operates
 * with GQuarks.
 *
 * Returns: %TRUE if the widget at @pos has the class defined.
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_path_iter_has_qclass (const GtkWidgetPath *path,
                                 gint                 pos,
                                 GQuark               qname)
{
  GtkPathElement *elem;
  guint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path->elems->len != 0, FALSE);
  g_return_val_if_fail (qname != 0, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

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

/**
 * gtk_widget_path_iter_has_class:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 * @name: class name
 *
 * Returns %TRUE if the widget at position @pos has the class @name
 * defined, %FALSE otherwise.
 *
 * Returns: %TRUE if the class @name is defined for the widget at @pos
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_path_iter_has_class (const GtkWidgetPath *path,
                                gint                 pos,
                                const gchar         *name)
{
  GQuark qname;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path->elems->len != 0, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

  return gtk_widget_path_iter_has_qclass (path, pos, qname);
}

/**
 * gtk_widget_path_iter_add_region:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: region name
 * @flags: flags affecting the region
 *
 * Adds the region @name to the widget at position @pos in
 * the hierarchy defined in @path. See
 * gtk_style_context_add_region().
 *
 * Region names must only contain lowercase letters
 * and “-”, starting always with a lowercase letter.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14: The use of regions is deprecated.
 **/
void
gtk_widget_path_iter_add_region (GtkWidgetPath  *path,
                                 gint            pos,
                                 const gchar    *name,
                                 GtkRegionFlags  flags)
{
  GtkPathElement *elem;
  GQuark qname;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);
  g_return_if_fail (name != NULL);
  g_return_if_fail (_gtk_style_context_check_region_name (name));

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  qname = g_quark_from_string (name);

  if (!elem->regions)
    elem->regions = g_hash_table_new (NULL, NULL);

  g_hash_table_insert (elem->regions,
                       GUINT_TO_POINTER (qname),
                       GUINT_TO_POINTER (flags));
}

/**
 * gtk_widget_path_iter_remove_region:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: region name
 *
 * Removes the region @name from the widget at position @pos in
 * the hierarchy defined in @path.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14: The use of regions is deprecated.
 **/
void
gtk_widget_path_iter_remove_region (GtkWidgetPath *path,
                                    gint           pos,
                                    const gchar   *name)
{
  GtkPathElement *elem;
  GQuark qname;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);
  g_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  qname = g_quark_try_string (name);

  if (qname == 0)
    return;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (elem->regions)
    g_hash_table_remove (elem->regions, GUINT_TO_POINTER (qname));
}

/**
 * gtk_widget_path_iter_clear_regions:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 *
 * Removes all regions from the widget at position @pos in the
 * hierarchy defined in @path.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14: The use of regions is deprecated.
 **/
void
gtk_widget_path_iter_clear_regions (GtkWidgetPath *path,
                                    gint           pos)
{
  GtkPathElement *elem;

  g_return_if_fail (path != NULL);
  g_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  if (elem->regions)
    g_hash_table_remove_all (elem->regions);
}

/**
 * gtk_widget_path_iter_list_regions:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 *
 * Returns a list with all the region names defined for the widget
 * at position @pos in the hierarchy defined in @path.
 *
 * Returns: (transfer container) (element-type utf8): The list of
 *          regions, This is a list of strings, the #GSList contents
 *          are owned by GTK+, but you should use g_slist_free() to
 *          free the list itself.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14: The use of regions is deprecated.
 **/
GSList *
gtk_widget_path_iter_list_regions (const GtkWidgetPath *path,
                                   gint                 pos)
{
  GtkPathElement *elem;
  GHashTableIter iter;
  GSList *list = NULL;
  gpointer key;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path->elems->len != 0, NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

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

/**
 * gtk_widget_path_iter_has_qregion:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 * @qname: region name as a #GQuark
 * @flags: (out): return location for the region flags
 *
 * See gtk_widget_path_iter_has_region(). This is a version that operates
 * with GQuarks.
 *
 * Returns: %TRUE if the widget at @pos has the region defined.
 *
 * Since: 3.0
 *
 * Deprecated: 3.14: The use of regions is deprecated.
 **/
gboolean
gtk_widget_path_iter_has_qregion (const GtkWidgetPath *path,
                                  gint                 pos,
                                  GQuark               qname,
                                  GtkRegionFlags      *flags)
{
  GtkPathElement *elem;
  gpointer value;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path->elems->len != 0, FALSE);
  g_return_val_if_fail (qname != 0, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

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

/**
 * gtk_widget_path_iter_has_region:
 * @path: a #GtkWidgetPath
 * @pos: position to query, -1 for the path head
 * @name: region name
 * @flags: (out): return location for the region flags
 *
 * Returns %TRUE if the widget at position @pos has the class @name
 * defined, %FALSE otherwise.
 *
 * Returns: %TRUE if the class @name is defined for the widget at @pos
 *
 * Since: 3.0
 *
 * Deprecated: 3.14: The use of regions is deprecated.
 **/
gboolean
gtk_widget_path_iter_has_region (const GtkWidgetPath *path,
                                 gint                 pos,
                                 const gchar         *name,
                                 GtkRegionFlags      *flags)
{
  GQuark qname;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (path->elems->len != 0, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gtk_widget_path_iter_has_qregion (path, pos, qname, flags);
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gtk_widget_path_get_object_type:
 * @path: a #GtkWidget
 *
 * Returns the topmost object type, that is, the object type this path
 * is representing.
 *
 * Returns: The object type
 *
 * Since: 3.0
 **/
GType
gtk_widget_path_get_object_type (const GtkWidgetPath *path)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, G_TYPE_INVALID);

  elem = &g_array_index (path->elems, GtkPathElement,
                         path->elems->len - 1);
  return elem->type;
}

/**
 * gtk_widget_path_is_type:
 * @path: a #GtkWidgetPath
 * @type: widget type to match
 *
 * Returns %TRUE if the widget type represented by this path
 * is @type, or a subtype of it.
 *
 * Returns: %TRUE if the widget represented by @path is of type @type
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_path_is_type (const GtkWidgetPath *path,
                         GType                type)
{
  GtkPathElement *elem;

  g_return_val_if_fail (path != NULL, FALSE);

  elem = &g_array_index (path->elems, GtkPathElement,
                         path->elems->len - 1);

  if (elem->type == type ||
      g_type_is_a (elem->type, type))
    return TRUE;

  return FALSE;
}

/**
 * gtk_widget_path_has_parent:
 * @path: a #GtkWidgetPath
 * @type: widget type to check in parents
 *
 * Returns %TRUE if any of the parents of the widget represented
 * in @path is of type @type, or any subtype of it.
 *
 * Returns: %TRUE if any parent is of type @type
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_path_has_parent (const GtkWidgetPath *path,
                            GType                type)
{
  guint i;

  g_return_val_if_fail (path != NULL, FALSE);

  for (i = 0; i < path->elems->len - 1; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      if (elem->type == type ||
          g_type_is_a (elem->type, type))
        return TRUE;
    }

  return FALSE;
}
