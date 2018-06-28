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

#include "gtkwidgetpath.h"

#include <string.h>

#include "gtkcssnodedeclarationprivate.h"
#include "gtkprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"
#include "gtkwidgetpathprivate.h"

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
  GtkCssNodeDeclaration *decl;
  guint sibling_index;
  GtkWidgetPath *siblings;
};

struct _GtkWidgetPath
{
  guint ref_count;

  GArray *elems; /* First element contains the described widget */
};

/**
 * gtk_widget_path_new:
 *
 * Returns an empty widget path.
 *
 * Returns: (transfer full): A newly created, empty, #GtkWidgetPath
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

  dest->decl = gtk_css_node_declaration_ref (src->decl);
  if (src->siblings)
    dest->siblings = gtk_widget_path_ref (src->siblings);
  dest->sibling_index = src->sibling_index;
}

/**
 * gtk_widget_path_copy:
 * @path: a #GtkWidgetPath
 *
 * Returns a copy of @path
 *
 * Returns: (transfer full): a copy of @path
 **/
GtkWidgetPath *
gtk_widget_path_copy (const GtkWidgetPath *path)
{
  GtkWidgetPath *new_path;
  guint i;

  gtk_internal_return_val_if_fail (path != NULL, NULL);

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
 **/
GtkWidgetPath *
gtk_widget_path_ref (GtkWidgetPath *path)
{
  gtk_internal_return_val_if_fail (path != NULL, path);

  path->ref_count += 1;

  return path;
}

/**
 * gtk_widget_path_unref:
 * @path: a #GtkWidgetPath
 *
 * Decrements the reference count on @path, freeing the structure
 * if the reference count reaches 0.
 **/
void
gtk_widget_path_unref (GtkWidgetPath *path)
{
  guint i;

  gtk_internal_return_if_fail (path != NULL);

  path->ref_count -= 1;
  if (path->ref_count > 0)
    return;

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      gtk_css_node_declaration_unref (elem->decl);
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
 **/
void
gtk_widget_path_free (GtkWidgetPath *path)
{
  gtk_internal_return_if_fail (path != NULL);

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
 **/
gint
gtk_widget_path_length (const GtkWidgetPath *path)
{
  gtk_internal_return_val_if_fail (path != NULL, 0);

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
 **/
char *
gtk_widget_path_to_string (const GtkWidgetPath *path)
{
  GString *string;
  guint i, j, n;

  gtk_internal_return_val_if_fail (path != NULL, NULL);

  string = g_string_new ("");

  for (i = 0; i < path->elems->len; i++)
    {
      GtkPathElement *elem;
      GtkStateFlags state;
      const GQuark *classes;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      if (i > 0)
        g_string_append_c (string, ' ');

      if (gtk_css_node_declaration_get_name (elem->decl))
        g_string_append (string, gtk_css_node_declaration_get_name (elem->decl));
      else
        g_string_append (string, g_type_name (gtk_css_node_declaration_get_type (elem->decl)));

      if (gtk_css_node_declaration_get_id (elem->decl))
        {
          g_string_append_c (string, '(');
          g_string_append (string, gtk_css_node_declaration_get_id (elem->decl));
          g_string_append_c (string, ')');
        }

      state = gtk_css_node_declaration_get_state (elem->decl);
      if (state)
        {
          GFlagsClass *fclass;

          fclass = g_type_class_ref (GTK_TYPE_STATE_FLAGS);
          for (j = 0; j < fclass->n_values; j++)
            {
              if (state & fclass->values[j].value)
                {
                  g_string_append_c (string, ':');
                  g_string_append (string, fclass->values[j].value_nick);
                }
            }
          g_type_class_unref (fclass);
        }

      if (elem->siblings)
        g_string_append_printf (string, "[%d/%d]",
                                elem->sibling_index + 1,
                                gtk_widget_path_length (elem->siblings));

      classes = gtk_css_node_declaration_get_classes (elem->decl, &n);
      for (j = 0; j < n; j++)
        {
          g_string_append_c (string, '.');
          g_string_append (string, g_quark_to_string (classes[j]));
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
 **/
void
gtk_widget_path_prepend_type (GtkWidgetPath *path,
                              GType          type)
{
  GtkPathElement new = { NULL };

  gtk_internal_return_if_fail (path != NULL);

  new.decl = gtk_css_node_declaration_new ();
  gtk_css_node_declaration_set_type (&new.decl, type);

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
 **/
gint
gtk_widget_path_append_type (GtkWidgetPath *path,
                             GType          type)
{
  GtkPathElement new = { NULL };

  gtk_internal_return_val_if_fail (path != NULL, 0);

  new.decl = gtk_css_node_declaration_new ();
  gtk_css_node_declaration_set_type (&new.decl, type);
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
 **/
gint
gtk_widget_path_append_with_siblings (GtkWidgetPath *path,
                                      GtkWidgetPath *siblings,
                                      guint          sibling_index)
{
  GtkPathElement new;

  gtk_internal_return_val_if_fail (path != NULL, 0);
  gtk_internal_return_val_if_fail (siblings != NULL, 0);
  gtk_internal_return_val_if_fail (sibling_index < gtk_widget_path_length (siblings), 0);

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

  gtk_internal_return_val_if_fail (path != NULL, NULL);
  gtk_internal_return_val_if_fail (path->elems->len != 0, NULL);

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

  gtk_internal_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  gtk_internal_return_val_if_fail (path->elems->len != 0, G_TYPE_INVALID);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return elem->sibling_index;
}

/**
 * gtk_widget_path_iter_get_object_name:
 * @path: a #GtkWidgetPath
 * @pos: position to get the object name for, -1 for the path head
 *
 * Returns the object name that is at position @pos in the widget
 * hierarchy defined in @path.
 *
 * Returns: (nullable): the name or %NULL
 **/
const char *
gtk_widget_path_iter_get_object_name (const GtkWidgetPath *path,
                                      gint                 pos)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, NULL);
  gtk_internal_return_val_if_fail (path->elems->len != 0, NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return gtk_css_node_declaration_get_name (elem->decl);
}

/**
 * gtk_widget_path_iter_set_object_name:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: (allow-none): object name to set or %NULL to unset
 *
 * Sets the object name for a given position in the widget hierarchy
 * defined by @path.
 *
 * When set, the object name overrides the object type when matching
 * CSS.
 **/
void
gtk_widget_path_iter_set_object_name (GtkWidgetPath *path,
                                      gint           pos,
                                      const char    *name)
{
  GtkPathElement *elem;

  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  gtk_css_node_declaration_set_name (&elem->decl, g_intern_string (name));
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
 **/
GType
gtk_widget_path_iter_get_object_type (const GtkWidgetPath *path,
                                      gint                 pos)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, G_TYPE_INVALID);
  gtk_internal_return_val_if_fail (path->elems->len != 0, G_TYPE_INVALID);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return gtk_css_node_declaration_get_type (elem->decl);
}

/**
 * gtk_widget_path_iter_set_object_type:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @type: object type to set
 *
 * Sets the object type for a given position in the widget hierarchy
 * defined by @path.
 **/
void
gtk_widget_path_iter_set_object_type (GtkWidgetPath *path,
                                      gint           pos,
                                      GType          type)
{
  GtkPathElement *elem;

  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  gtk_css_node_declaration_set_type (&elem->decl, type);
}

/**
 * gtk_widget_path_iter_get_state:
 * @path: a #GtkWidgetPath
 * @pos: position to get the state for, -1 for the path head
 *
 * Returns the state flags corresponding to the widget found at
 * the position @pos in the widget hierarchy defined by
 * @path
 *
 * Returns: The state flags
 **/
GtkStateFlags
gtk_widget_path_iter_get_state (const GtkWidgetPath *path,
                                gint                 pos)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, 0);
  gtk_internal_return_val_if_fail (path->elems->len != 0, 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return gtk_css_node_declaration_get_state (elem->decl);
}

/**
 * gtk_widget_path_iter_set_state:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @state: state flags
 *
 * Sets the widget name for the widget found at position @pos
 * in the widget hierarchy defined by @path.
 *
 * If you want to update just a single state flag, you need to do
 * this manually, as this function updates all state flags.
 *
 * ## Setting a flag
 *
 * |[<!-- language="C" -->
 * gtk_widget_path_iter_set_state (path, pos, gtk_widget_path_iter_get_state (path, pos) | flag);
 * ]|
 *
 * ## Unsetting a flag
 *
 * |[<!-- language="C" -->
 * gtk_widget_path_iter_set_state (path, pos, gtk_widget_path_iter_get_state (path, pos) & ~flag);
 * ]|
 **/
void
gtk_widget_path_iter_set_state (GtkWidgetPath *path,
                                gint           pos,
                                GtkStateFlags  state)
{
  GtkPathElement *elem;

  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  gtk_css_node_declaration_set_state (&elem->decl, state);
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
 * Returns: (nullable): The widget name, or %NULL if none was set.
 **/
const gchar *
gtk_widget_path_iter_get_name (const GtkWidgetPath *path,
                               gint                 pos)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, NULL);
  gtk_internal_return_val_if_fail (path->elems->len != 0, NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  return gtk_css_node_declaration_get_id (elem->decl);
}

/**
 * gtk_widget_path_iter_set_name:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: widget name
 *
 * Sets the widget name for the widget found at position @pos
 * in the widget hierarchy defined by @path.
 **/
void
gtk_widget_path_iter_set_name (GtkWidgetPath *path,
                               gint           pos,
                               const gchar   *name)
{
  GtkPathElement *elem;

  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);
  gtk_internal_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  gtk_css_node_declaration_set_id (&elem->decl, g_intern_string (name));
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
 **/
gboolean
gtk_widget_path_iter_has_qname (const GtkWidgetPath *path,
                                gint                 pos,
                                GQuark               qname)
{
  gtk_internal_return_val_if_fail (path != NULL, FALSE);
  gtk_internal_return_val_if_fail (path->elems->len != 0, FALSE);
  gtk_internal_return_val_if_fail (qname != 0, FALSE);

  return gtk_widget_path_iter_has_name (path, pos, g_quark_to_string (qname));
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
 **/
gboolean
gtk_widget_path_iter_has_name (const GtkWidgetPath *path,
                               gint                 pos,
                               const gchar         *name)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, FALSE);
  gtk_internal_return_val_if_fail (path->elems->len != 0, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  name = g_intern_string (name);
  elem = &g_array_index (path->elems, GtkPathElement, pos);

  return gtk_css_node_declaration_get_id (elem->decl) == name;
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
 **/
void
gtk_widget_path_iter_add_class (GtkWidgetPath *path,
                                gint           pos,
                                const gchar   *name)
{
  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);
  gtk_internal_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  gtk_widget_path_iter_add_qclass (path, pos, g_quark_from_string (name));
}

void
gtk_widget_path_iter_add_qclass (GtkWidgetPath *path,
                                 gint           pos,
                                 GQuark         qname)
{
  GtkPathElement *elem;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  gtk_css_node_declaration_add_class (&elem->decl, qname);
}

/**
 * gtk_widget_path_iter_remove_class:
 * @path: a #GtkWidgetPath
 * @pos: position to modify, -1 for the path head
 * @name: class name
 *
 * Removes the class @name from the widget at position @pos in
 * the hierarchy defined in @path.
 **/
void
gtk_widget_path_iter_remove_class (GtkWidgetPath *path,
                                   gint           pos,
                                   const gchar   *name)
{
  GtkPathElement *elem;
  GQuark qname;

  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);
  gtk_internal_return_if_fail (name != NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  qname = g_quark_try_string (name);
  if (qname == 0)
    return;

  gtk_css_node_declaration_remove_class (&elem->decl, qname);
}

/**
 * gtk_widget_path_iter_clear_classes:
 * @path: a #GtkWidget
 * @pos: position to modify, -1 for the path head
 *
 * Removes all classes from the widget at position @pos in the
 * hierarchy defined in @path.
 **/
void
gtk_widget_path_iter_clear_classes (GtkWidgetPath *path,
                                    gint           pos)
{
  GtkPathElement *elem;

  gtk_internal_return_if_fail (path != NULL);
  gtk_internal_return_if_fail (path->elems->len != 0);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  gtk_css_node_declaration_clear_classes (&elem->decl);
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
 **/
GSList *
gtk_widget_path_iter_list_classes (const GtkWidgetPath *path,
                                   gint                 pos)
{
  GtkPathElement *elem;
  GSList *list = NULL;
  const GQuark *classes;
  guint i, n;

  gtk_internal_return_val_if_fail (path != NULL, NULL);
  gtk_internal_return_val_if_fail (path->elems->len != 0, NULL);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);
  classes = gtk_css_node_declaration_get_classes (elem->decl, &n);

  for (i = 0; i < n; i++)
    {
      list = g_slist_prepend (list, (gchar *) g_quark_to_string (classes[i]));
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
 **/
gboolean
gtk_widget_path_iter_has_qclass (const GtkWidgetPath *path,
                                 gint                 pos,
                                 GQuark               qname)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, FALSE);
  gtk_internal_return_val_if_fail (path->elems->len != 0, FALSE);
  gtk_internal_return_val_if_fail (qname != 0, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  elem = &g_array_index (path->elems, GtkPathElement, pos);

  return gtk_css_node_declaration_has_class (elem->decl, qname);
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
 **/
gboolean
gtk_widget_path_iter_has_class (const GtkWidgetPath *path,
                                gint                 pos,
                                const gchar         *name)
{
  GQuark qname;

  gtk_internal_return_val_if_fail (path != NULL, FALSE);
  gtk_internal_return_val_if_fail (path->elems->len != 0, FALSE);
  gtk_internal_return_val_if_fail (name != NULL, FALSE);

  if (pos < 0 || pos >= path->elems->len)
    pos = path->elems->len - 1;

  qname = g_quark_try_string (name);

  if (qname == 0)
    return FALSE;

  return gtk_widget_path_iter_has_qclass (path, pos, qname);
}

/**
 * gtk_widget_path_get_object_type:
 * @path: a #GtkWidget
 *
 * Returns the topmost object type, that is, the object type this path
 * is representing.
 *
 * Returns: The object type
 **/
GType
gtk_widget_path_get_object_type (const GtkWidgetPath *path)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, G_TYPE_INVALID);

  elem = &g_array_index (path->elems, GtkPathElement,
                         path->elems->len - 1);
  return gtk_css_node_declaration_get_type (elem->decl);
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
 **/
gboolean
gtk_widget_path_is_type (const GtkWidgetPath *path,
                         GType                type)
{
  GtkPathElement *elem;

  gtk_internal_return_val_if_fail (path != NULL, FALSE);

  elem = &g_array_index (path->elems, GtkPathElement,
                         path->elems->len - 1);

  return g_type_is_a (gtk_css_node_declaration_get_type (elem->decl), type);
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
 **/
gboolean
gtk_widget_path_has_parent (const GtkWidgetPath *path,
                            GType                type)
{
  guint i;

  gtk_internal_return_val_if_fail (path != NULL, FALSE);

  for (i = 0; i < path->elems->len - 1; i++)
    {
      GtkPathElement *elem;

      elem = &g_array_index (path->elems, GtkPathElement, i);

      if (g_type_is_a (gtk_css_node_declaration_get_type (elem->decl), type))
        return TRUE;
    }

  return FALSE;
}
