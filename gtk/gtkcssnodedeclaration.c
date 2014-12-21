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

#include <string.h>

typedef struct _GtkRegion GtkRegion;

struct _GtkRegion
{
  GQuark class_quark;
  GtkRegionFlags flags;
};

struct _GtkCssNodeDeclaration {
  guint refcount;
  GtkJunctionSides junction_sides;
  GType type;
  GtkStateFlags state;
  guint n_classes;
  guint n_regions;
  /* GQuark classes[n_classes]; */
  /* GtkRegion region[n_regions]; */
};

static inline GQuark *
get_classes (const GtkCssNodeDeclaration *decl)
{
  return (GQuark *) (decl + 1);
}

static inline GtkRegion *
get_regions (const GtkCssNodeDeclaration *decl)
{
  return (GtkRegion *) (get_classes (decl) + decl->n_classes);
}

static inline gsize
sizeof_node (guint n_classes,
             guint n_regions)
{
  return sizeof (GtkCssNodeDeclaration)
       + sizeof (GQuark) * n_classes
       + sizeof (GtkRegion) * n_regions;
}

static inline gsize
sizeof_this_node (GtkCssNodeDeclaration *decl)
{
  return sizeof_node (decl->n_classes, decl->n_regions);
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
    0,
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
gtk_css_node_declaration_set_junction_sides (GtkCssNodeDeclaration **decl,
                                             GtkJunctionSides        junction_sides)
{
  if ((*decl)->junction_sides == junction_sides)
    return FALSE;
  
  gtk_css_node_declaration_make_writable (decl);
  (*decl)->junction_sides = junction_sides;

  return TRUE;
}

GtkJunctionSides
gtk_css_node_declaration_get_junction_sides (const GtkCssNodeDeclaration *decl)
{
  return decl->junction_sides;
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

  if (position)
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

  if (position)
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
gtk_css_node_declaration_has_class (const GtkCssNodeDeclaration *decl,
                                    GQuark                       class_quark)
{
  return find_class (decl, class_quark, NULL);
}

GList *
gtk_css_node_declaration_list_classes (const GtkCssNodeDeclaration *decl)
{
  GQuark *classes;
  GList *result;
  guint i;

  classes = get_classes (decl);
  result = NULL;

  for (i = 0; i < decl->n_classes; i++)
    {
      result = g_list_prepend (result, GUINT_TO_POINTER (classes[i]));
    }

  return result;
}

static gboolean
find_region (const GtkCssNodeDeclaration *decl,
             GQuark                       region_quark,
             guint                       *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  GtkRegion *regions;
  guint pos;

  if (position)
    *position = 0;

  if (decl->n_regions == 0)
    return FALSE;

  min = 0;
  max = decl->n_regions - 1;
  regions = get_regions (decl);

  do
    {
      GQuark item;

      mid = (min + max) / 2;
      item = regions[mid].class_quark;

      if (region_quark == item)
        {
          found = TRUE;
          pos = mid;
          break;
        }
      else if (region_quark > item)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (min <= max);

  if (position)
    *position = pos;

  return found;
}

gboolean
gtk_css_node_declaration_add_region (GtkCssNodeDeclaration **decl,
                                     GQuark                  region_quark,
                                     GtkRegionFlags          flags)
{
  GtkRegion *regions;
  guint pos;

  if (find_region (*decl, region_quark, &pos))
    return FALSE;

  gtk_css_node_declaration_make_writable_resize (decl,
                                                 (char *) &get_regions (*decl)[pos] - (char *) *decl,
                                                 sizeof (GtkRegion),
                                                 0);
  (*decl)->n_regions++;
  regions = get_regions(*decl);
  regions[pos].class_quark = region_quark;
  regions[pos].flags = flags;

  return TRUE;
}

gboolean
gtk_css_node_declaration_remove_region (GtkCssNodeDeclaration **decl,
                                        GQuark                  region_quark)
{
  guint pos;

  if (!find_region (*decl, region_quark, &pos))
    return FALSE;

  gtk_css_node_declaration_make_writable_resize (decl,
                                                 (char *) &get_regions (*decl)[pos] - (char *) *decl,
                                                 0,
                                                 sizeof (GtkRegion));
  (*decl)->n_regions--;

  return TRUE;
}

gboolean
gtk_css_node_declaration_has_region (const GtkCssNodeDeclaration  *decl,
                                     GQuark                        region_quark,
                                     GtkRegionFlags               *flags_return)
{
  guint pos;

  if (!find_region (decl, region_quark, &pos))
    {
      if (flags_return)
        *flags_return = 0;
      return FALSE;
    }

  if (flags_return)
    *flags_return = get_regions (decl)[pos].flags;

  return TRUE;
}

GList *
gtk_css_node_declaration_list_regions (const GtkCssNodeDeclaration *decl)
{
  GtkRegion *regions;
  GList *result;
  guint i;

  regions = get_regions (decl);
  result = NULL;

  for (i = 0; i < decl->n_regions; i++)
    {
      result = g_list_prepend (result, GUINT_TO_POINTER (regions[i].class_quark));
    }

  return result;
}

guint
gtk_css_node_declaration_hash (gconstpointer elem)
{
  const GtkCssNodeDeclaration *decl = elem;
  GQuark *classes;
  GtkRegion *regions;
  guint hash, i;
  
  hash = (guint) decl->type;

  classes = get_classes (decl);
  for (i = 0; i < decl->n_classes; i++)
    {
      hash <<= 5;
      hash += classes[i];
    }

  regions = get_regions (decl);
  for (i = 0; i < decl->n_regions; i++)
    {
      hash <<= 5;
      hash += regions[i].class_quark;
      hash += regions[i].flags;
    }

  hash ^= ((guint) decl->junction_sides) << (sizeof (guint) * 8 - 5);
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
  GtkRegion *regions1, *regions2;
  guint i;

  if (decl1 == decl2)
    return TRUE;

  if (decl1->type != decl2->type)
    return FALSE;

  if (decl1->state != decl2->state)
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

  if (decl1->n_regions != decl2->n_regions)
    return FALSE;

  regions1 = get_regions (decl1);
  regions2 = get_regions (decl2);
  for (i = 0; i < decl1->n_regions; i++)
    {
      if (regions1[i].class_quark != regions2[i].class_quark ||
          regions1[i].flags != regions2[i].flags)
        return FALSE;
    }

  if (decl1->junction_sides != decl2->junction_sides)
    return FALSE;

  return TRUE;
}

void
gtk_css_node_declaration_add_to_widget_path (const GtkCssNodeDeclaration *decl,
                                             GtkWidgetPath               *path,
                                             guint                        pos)
{
  GQuark *classes;
  GtkRegion *regions;
  guint i;

  /* Set widget regions */
  regions = get_regions (decl);
  for (i = 0; i < decl->n_regions; i++)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_widget_path_iter_add_region (path, pos,
                                       g_quark_to_string (regions[i].class_quark),
                                       regions[i].flags);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  /* Set widget classes */
  classes = get_classes (decl);
  for (i = 0; i < decl->n_classes; i++)
    {
      gtk_widget_path_iter_add_class (path, pos,
                                      g_quark_to_string (classes[i]));
    }

  /* Set widget state */
  gtk_widget_path_iter_set_state (path, pos, decl->state);
}

