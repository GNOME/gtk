/*
 * Copyright © 2014 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnodedeclarationprivate.h"
#include "gtkwidgetpathprivate.h"

#include <string.h>

struct _GtkCssNodeDeclaration {
  guint refcount;
  GType type;
  const /* interned */ char *name;
  const /* interned */ char *id;
  GtkStateFlags state;
  guint n_classes;
  /* GQuark classes[n_classes]; */
};

static inline GQuark *
get_classes (const GtkCssNodeDeclaration *decl)
{
  return (GQuark *) (decl + 1);
}

static inline gsize
sizeof_node (guint n_classes)
{
  return sizeof (GtkCssNodeDeclaration)
       + sizeof (GQuark) * n_classes;
}

static inline gsize
sizeof_this_node (GtkCssNodeDeclaration *decl)
{
  return sizeof_node (decl->n_classes);
}

static void
gtk_css_node_declaration_make_writable (GtkCssNodeDeclaration **decl)
{
  if ((*decl)->refcount == 1)
    return;

  (*decl)->refcount--;

  *decl = g_memdup (*decl, sizeof_this_node (*decl));
  (*decl)->refcount = 1;
}

static void
gtk_css_node_declaration_make_writable_resize (GtkCssNodeDeclaration **decl,
                                               gsize                   offset,
                                               gsize                   bytes_added,
                                               gsize                   bytes_removed)
{
  gsize old_size = sizeof_this_node (*decl);
  gsize new_size = old_size + bytes_added - bytes_removed;

  if ((*decl)->refcount == 1)
    {
      if (bytes_removed > 0 && old_size - offset - bytes_removed > 0)
        memmove (((char *) *decl) + offset, ((char *) *decl) + offset + bytes_removed, old_size - offset - bytes_removed);
      *decl = g_realloc (*decl, new_size);
      if (bytes_added > 0 && old_size - offset > 0)
        memmove (((char *) *decl) + offset + bytes_added, ((char *) *decl) + offset, old_size - offset);
    }
  else
    {
      GtkCssNodeDeclaration *old = *decl;

      old->refcount--;
  
      *decl = g_malloc (new_size);
      memcpy (*decl, old, offset);
      if (old_size - offset - bytes_removed > 0)
        memcpy (((char *) *decl) + offset + bytes_added, ((char *) old) + offset + bytes_removed, old_size - offset - bytes_removed);
      (*decl)->refcount = 1;
    }
}

GtkCssNodeDeclaration *
gtk_css_node_declaration_new (void)
{
  static GtkCssNodeDeclaration empty = {
    1, /* need to own a ref ourselves so the copy-on-write path kicks in when people change things */
    0,
    NULL,
    NULL,
    0,
    0
  };

  return gtk_css_node_declaration_ref (&empty);
}

GtkCssNodeDeclaration *
gtk_css_node_declaration_ref (GtkCssNodeDeclaration *decl)
{
  decl->refcount++;

  return decl;
}

void
gtk_css_node_declaration_unref (GtkCssNodeDeclaration *decl)
{
  decl->refcount--;
  if (decl->refcount > 0)
    return;

  g_free (decl);
}

gboolean
gtk_css_node_declaration_set_type (GtkCssNodeDeclaration **decl,
                                   GType                   type)
{
  if ((*decl)->type == type)
    return FALSE;

  gtk_css_node_declaration_make_writable (decl);
  (*decl)->type = type;

  return TRUE;
}

GType
gtk_css_node_declaration_get_type (const GtkCssNodeDeclaration *decl)
{
  return decl->type;
}

gboolean
gtk_css_node_declaration_set_name (GtkCssNodeDeclaration   **decl,
                                   /*interned*/ const char  *name)
{
  if ((*decl)->name == name)
    return FALSE;

  gtk_css_node_declaration_make_writable (decl);
  (*decl)->name = name;

  return TRUE;
}

/*interned*/ const char *
gtk_css_node_declaration_get_name (const GtkCssNodeDeclaration *decl)
{
  return decl->name;
}

gboolean
gtk_css_node_declaration_set_id (GtkCssNodeDeclaration **decl,
                                 const char             *id)
{
  id = g_intern_string (id);

  if ((*decl)->id == id)
    return FALSE;

  gtk_css_node_declaration_make_writable (decl);
  (*decl)->id = id;

  return TRUE;
}

const char *
gtk_css_node_declaration_get_id (const GtkCssNodeDeclaration *decl)
{
  return decl->id;
}

gboolean
gtk_css_node_declaration_set_state (GtkCssNodeDeclaration **decl,
                                    GtkStateFlags           state)
{
  if ((*decl)->state == state)
    return FALSE;
  
  gtk_css_node_declaration_make_writable (decl);
  (*decl)->state = state;

  return TRUE;
}

GtkStateFlags
gtk_css_node_declaration_get_state (const GtkCssNodeDeclaration *decl)
{
  return decl->state;
}

static gboolean
find_class (const GtkCssNodeDeclaration *decl,
            GQuark                       class_quark,
            guint                       *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  GQuark *classes;
  guint pos;

  *position = 0;

  if (decl->n_classes == 0)
    return FALSE;

  min = 0;
  max = decl->n_classes - 1;
  classes = get_classes (decl);

  do
    {
      GQuark item;

      mid = (min + max) / 2;
      item = classes[mid];

      if (class_quark == item)
        {
          found = TRUE;
          pos = mid;
          break;
        }
      else if (class_quark > item)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (min <= max);

  *position = pos;

  return found;
}

gboolean
gtk_css_node_declaration_add_class (GtkCssNodeDeclaration **decl,
                                    GQuark                  class_quark)
{
  guint pos;

  if (find_class (*decl, class_quark, &pos))
    return FALSE;

  gtk_css_node_declaration_make_writable_resize (decl,
                                                 (char *) &get_classes (*decl)[pos] - (char *) *decl,
                                                 sizeof (GQuark),
                                                 0);
  (*decl)->n_classes++;
  get_classes(*decl)[pos] = class_quark;

  return TRUE;
}

gboolean
gtk_css_node_declaration_remove_class (GtkCssNodeDeclaration **decl,
                                       GQuark                  class_quark)
{
  guint pos;

  if (!find_class (*decl, class_quark, &pos))
    return FALSE;

  gtk_css_node_declaration_make_writable_resize (decl,
                                                 (char *) &get_classes (*decl)[pos] - (char *) *decl,
                                                 0,
                                                 sizeof (GQuark));
  (*decl)->n_classes--;

  return TRUE;
}

gboolean
gtk_css_node_declaration_clear_classes (GtkCssNodeDeclaration **decl)
{
  if ((*decl)->n_classes == 0)
    return FALSE;

  gtk_css_node_declaration_make_writable_resize (decl,
                                                 (char *) get_classes (*decl) - (char *) *decl,
                                                 0,
                                                 sizeof (GQuark) * (*decl)->n_classes);
  (*decl)->n_classes = 0;

  return TRUE;
}

gboolean
gtk_css_node_declaration_has_class (const GtkCssNodeDeclaration *decl,
                                    GQuark                       class_quark)
{
  guint pos;
  GQuark *classes = get_classes (decl);

  switch (decl->n_classes)
    {
    case 3:
      if (classes[2] == class_quark)
        return TRUE;
      G_GNUC_FALLTHROUGH;

    case 2:
      if (classes[1] == class_quark)
        return TRUE;
      G_GNUC_FALLTHROUGH;

    case 1:
      if (classes[0] == class_quark)
        return TRUE;
      G_GNUC_FALLTHROUGH;

    case 0:
      return FALSE;

    default:
      return find_class (decl, class_quark, &pos);
    }
}

const GQuark *
gtk_css_node_declaration_get_classes (const GtkCssNodeDeclaration *decl,
                                      guint                       *n_classes)
{
  *n_classes = decl->n_classes;

  return get_classes (decl);
}

guint
gtk_css_node_declaration_hash (gconstpointer elem)
{
  const GtkCssNodeDeclaration *decl = elem;
  GQuark *classes;
  guint hash, i;
  
  hash = (guint) decl->type;
  hash ^= GPOINTER_TO_UINT (decl->name);
  hash <<= 5;
  hash ^= GPOINTER_TO_UINT (decl->id);

  classes = get_classes (decl);
  for (i = 0; i < decl->n_classes; i++)
    {
      hash <<= 5;
      hash += classes[i];
    }

  hash ^= decl->state;

  return hash;
}

gboolean
gtk_css_node_declaration_equal (gconstpointer elem1,
                                gconstpointer elem2)
{
  const GtkCssNodeDeclaration *decl1 = elem1;
  const GtkCssNodeDeclaration *decl2 = elem2;
  GQuark *classes1, *classes2;
  guint i;

  if (decl1 == decl2)
    return TRUE;

  if (decl1->type != decl2->type)
    return FALSE;

  if (decl1->name != decl2->name)
    return FALSE;

  if (decl1->state != decl2->state)
    return FALSE;

  if (decl1->id != decl2->id)
    return FALSE;

  if (decl1->n_classes != decl2->n_classes)
    return FALSE;

  classes1 = get_classes (decl1);
  classes2 = get_classes (decl2);
  for (i = 0; i < decl1->n_classes; i++)
    {
      if (classes1[i] != classes2[i])
        return FALSE;
    }

  return TRUE;
}

void
gtk_css_node_declaration_add_to_widget_path (const GtkCssNodeDeclaration *decl,
                                             GtkWidgetPath               *path,
                                             guint                        pos)
{
  GQuark *classes;
  guint i;

  /* Set name and id */
  gtk_widget_path_iter_set_object_name (path, pos, decl->name);
  if (decl->id)
    gtk_widget_path_iter_set_name (path, pos, decl->id);

  /* Set widget classes */
  classes = get_classes (decl);
  for (i = 0; i < decl->n_classes; i++)
    {
      gtk_widget_path_iter_add_qclass (path, pos, classes[i]);
    }

  /* Set widget state */
  gtk_widget_path_iter_set_state (path, pos, decl->state);
}

/* Append the declaration to the string, in selector format */
void
gtk_css_node_declaration_print (const GtkCssNodeDeclaration *decl,
                                GString                     *string)
{
  static const char *state_names[] = {
    "active",
    "hover",
    "selected",
    "disabled",
    "indeterminate",
    "focus",
    "backdrop",
    "dir(ltr)",
    "dir(rtl)",
    "link",
    "visited",
    "checked",
    "drop(active)"
  };
  const GQuark *classes;
  guint i;

  if (decl->name)
    g_string_append (string, decl->name);
  else
    g_string_append (string, g_type_name (decl->type));

  if (decl->id)
    {
      g_string_append_c (string, '#');
      g_string_append (string, decl->id);
    }

  classes = get_classes (decl);
  for (i = 0; i < decl->n_classes; i++)
    {
      g_string_append_c (string, '.');
      g_string_append (string, g_quark_to_string (classes[i]));
    }

  for (i = 0; i < G_N_ELEMENTS (state_names); i++)
    {
      if (decl->state & (1 << i))
        {
          g_string_append_c (string, ':');
          g_string_append (string, state_names[i]);
        }
    }
}

char *
gtk_css_node_declaration_to_string (const GtkCssNodeDeclaration *decl)
{
  GString *s = g_string_new (NULL);

  gtk_css_node_declaration_print (decl, s);

  return g_string_free (s, FALSE);
}
