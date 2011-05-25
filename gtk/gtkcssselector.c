/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssselectorprivate.h"

#include "gtkstylecontextprivate.h"

struct _GtkCssSelector
{
  GtkCssSelector *  previous;        /* link to next element in selector or NULL if last */
  GtkCssCombinator  combine;         /* how to combine with the previous element */
  const char *      name;            /* quarked name of element we match or NULL if any */
  GType             type;            /* cache for type belonging to name - G_TYPE_INVALID if uncached */
  GQuark *          ids;             /* 0-terminated list of required ids or NULL if none */
  GQuark *          classes;         /* 0-terminated list of required classes or NULL if none */
  GtkRegionFlags    pseudo_classes;  /* required pseudo classes */
  GtkStateFlags     state;           /* required state flags (currently not checked when matching) */
};

GtkCssSelector *
_gtk_css_selector_new (GtkCssSelector         *previous,
                       GtkCssCombinator        combine,
                       const char *            name,
                       GQuark *                ids,
                       GQuark *                classes,
                       GtkRegionFlags          pseudo_classes,
                       GtkStateFlags           state)
{
  GtkCssSelector *selector;

  selector = g_slice_new0 (GtkCssSelector);
  selector->previous = previous;
  selector->combine = combine;
  selector->name = name ? g_quark_to_string (g_quark_from_string (name)) : NULL;
  selector->type = !name || _gtk_style_context_check_region_name (name) ? G_TYPE_NONE : G_TYPE_INVALID;
  selector->ids = ids;
  selector->classes = classes;
  selector->pseudo_classes = pseudo_classes;
  selector->state = state;

  return selector;
}

void
_gtk_css_selector_free (GtkCssSelector *selector)
{
  g_return_if_fail (selector != NULL);

  if (selector->previous)
    _gtk_css_selector_free (selector->previous);

  g_free (selector->ids);
  g_free (selector->classes);

  g_slice_free (GtkCssSelector, selector);
}

void
_gtk_css_selector_print (const GtkCssSelector *selector,
                         GString *             str)
{
  if (selector->previous)
    {
      _gtk_css_selector_print (selector->previous, str);
      switch (selector->combine)
        {
          case GTK_CSS_COMBINE_DESCANDANT:
            g_string_append (str, " ");
            break;
          case GTK_CSS_COMBINE_CHILD:
            g_string_append (str, " > ");
            break;
          default:
            g_assert_not_reached ();
        }
    }

  if (selector->name)
    g_string_append (str, selector->name);
  else if (selector->ids == NULL && 
           selector->classes == NULL && 
           selector->pseudo_classes == 0 &&
           selector->state == 0)
    g_string_append (str, "*");

  if (selector->ids)
    {
      GQuark *id;

      for (id = selector->ids; *id != 0; id++)
        {
          g_string_append_c (str, '#');
          g_string_append (str, g_quark_to_string (*id));
        }
    }

  if (selector->classes)
    {
      GQuark *class;

      for (class = selector->classes; *class != 0; class++)
        {
          g_string_append_c (str, '.');
          g_string_append (str, g_quark_to_string (*class));
        }
    }

  if (selector->pseudo_classes)
    {
      static const char * flag_names[] = {
        "nth-child(even)",
        "nth-child(odd)",
        "first-child",
        "last-child",
        "sorted"
      };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (flag_names); i++)
        {
          if (selector->pseudo_classes & (1 << i))
            {
              g_string_append_c (str, ':');
              g_string_append (str, flag_names[i]);
            }
        }
    }

  if (selector->state)
    {
      static const char * state_names[] = {
        "active",
        "hover",
        "selected",
        "insensitive",
        "inconsistent",
        "focus"
      };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (state_names); i++)
        {
          if (selector->state & (1 << i))
            {
              g_string_append_c (str, ':');
              g_string_append (str, state_names[i]);
            }
        }
    }
}

char *
_gtk_css_selector_to_string (const GtkCssSelector *selector)
{
  GString *string;

  g_return_val_if_fail (selector != NULL, NULL);

  string = g_string_new (NULL);

  _gtk_css_selector_print (selector, string);

  return g_string_free (string, FALSE);
}

static GtkRegionFlags
compute_region_flags_for_index (const GtkWidgetPath *path,
                                guint                id)
{
  const GtkWidgetPath *siblings;
  guint sibling_id, n_siblings;
  GtkRegionFlags flags;
  
  siblings = gtk_widget_path_iter_get_siblings (path, id);
  if (siblings == NULL)
    return 0;

  sibling_id = gtk_widget_path_iter_get_sibling_index (path, id);
  n_siblings = gtk_widget_path_length (siblings);

  flags = (sibling_id % 2) ? GTK_REGION_EVEN : GTK_REGION_ODD;
  if (sibling_id == 0)
    flags |= GTK_REGION_FIRST;
  if (sibling_id + 1 == n_siblings)
    flags |= GTK_REGION_LAST;

  return flags;
}

static gboolean
gtk_css_selector_matches_type (const GtkCssSelector      *selector,
                               const GtkWidgetPath       *path,
                               guint                      id)
{
  if (selector->pseudo_classes)
    {
      GtkRegionFlags flags = compute_region_flags_for_index (path, id);

      if ((selector->pseudo_classes & flags) != selector->pseudo_classes)
        return FALSE;
    }

  if (selector->name == NULL)
    return TRUE;

  if (selector->type == G_TYPE_NONE)
    return FALSE;

  /* ugh, assigning to a const variable */
  if (selector->type == G_TYPE_INVALID)
    ((GtkCssSelector *) selector)->type = g_type_from_name (selector->name);

  if (selector->type == G_TYPE_INVALID)
    return FALSE;

  return g_type_is_a (gtk_widget_path_iter_get_object_type (path, id), selector->type);
}

static gboolean
gtk_css_selector_matches_region (const GtkCssSelector      *selector,
                                 const GtkWidgetPath       *path,
                                 guint                      id,
                                 const char *               region)
{
  GtkRegionFlags flags;

  if (selector->name == NULL)
    return TRUE;
  
  if (selector->name != region)
    return FALSE;

  if (!gtk_widget_path_iter_has_region (path, id, region, &flags))
    {
      /* This function must be called with existing regions */
      g_assert_not_reached ();
    }

  return (selector->pseudo_classes & flags) == selector->pseudo_classes;
}

static gboolean
gtk_css_selector_matches_rest (const GtkCssSelector      *selector,
                               const GtkWidgetPath       *path,
                               guint                      id)
{
  if (selector->ids)
    {
      GQuark *name;
      
      for (name = selector->ids; *name; name++)
        {
          if (!gtk_widget_path_iter_has_qname (path, id, *name))
            return FALSE;
        }
    }

  if (selector->classes)
    {
      GQuark *class;
      
      for (class = selector->classes; *class; class++)
        {
          if (!gtk_widget_path_iter_has_qclass (path, id, *class))
            return FALSE;
        }
    }

  return TRUE;
}

static gboolean
gtk_css_selector_matches_previous (const GtkCssSelector      *selector,
                                   const GtkWidgetPath       *path,
                                   guint                      id,
                                   GSList                    *regions);

static gboolean
gtk_css_selector_matches_from (const GtkCssSelector      *selector,
                               const GtkWidgetPath       *path,
                               guint                      id,
                               GSList                    *regions)
{
  GSList *l;

  if (!gtk_css_selector_matches_rest (selector, path, id))
    return FALSE;

  for (l = regions; l; l = l->next)
    {
      const char *region = l->data;

      if (gtk_css_selector_matches_region (selector, path, id, region))
        {
          GSList *remaining;
          gboolean match;

          remaining = g_slist_copy (regions);
          remaining = g_slist_remove (remaining, region);
          match = gtk_css_selector_matches_previous (selector,
                                                     path,
                                                     id,
                                                     remaining);
          g_slist_free (remaining);
          if (match)
            return TRUE;
        }
    }

  if (gtk_css_selector_matches_type (selector, path, id))
    {
      GSList *regions;
      gboolean match;
      
      if (id <= 0)
        return selector->previous == NULL;

      regions = gtk_widget_path_iter_list_regions (path, id - 1);
      match = gtk_css_selector_matches_previous (selector,
                                                 path,
                                                 id - 1,
                                                 regions);
      g_slist_free (regions);
      return match;
    }

  return FALSE;
}

static gboolean
gtk_css_selector_matches_previous (const GtkCssSelector      *selector,
                                   const GtkWidgetPath       *path,
                                   guint                      id,
                                   GSList                    *regions)
{
  if (!selector->previous)
    return TRUE;

  if (gtk_css_selector_matches_from (selector->previous,
                                     path,
                                     id,
                                     regions))
    return TRUE;

  if (selector->combine == GTK_CSS_COMBINE_DESCANDANT)
    {
      /* with this magic we run the loop while id >= 0 */
      while (id-- != 0)
        {
          GSList *list;
          gboolean match;

          list = gtk_widget_path_iter_list_regions (path, id);
          match = gtk_css_selector_matches_from (selector->previous,
                                                 path,
                                                 id,
                                                 list);
          g_slist_free (list);
          if (match)
            return TRUE;
        }
    }

  return FALSE;
}

/**
 * _gtk_css_selector_matches:
 * @selector: the selector
 * @path: the path to check
 * @length: How many elements of the path are to be used
 *
 * Checks if the @selector matches the given @path. If @length is
 * smaller than the number of elements in @path, it is assumed that
 * only the first @length element of @path are valid and the rest
 * does not exist. This is useful for doing parent matches for the
 * 'inherit' keyword.
 *
 * Returns: %TRUE if the selector matches @path
 **/
gboolean
_gtk_css_selector_matches (const GtkCssSelector      *selector,
                           const GtkWidgetPath       *path,
                           guint                      length)
{
  GSList *list;
  gboolean match;

  g_return_val_if_fail (selector != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (length <= gtk_widget_path_length (path), FALSE);

  if (length == 0)
    return FALSE;

  list = gtk_widget_path_iter_list_regions (path, length - 1);
  match = gtk_css_selector_matches_from (selector,
                                         path,
                                         length - 1,
                                         list);
  g_slist_free (list);
  return match;
}

static guint
count_bits (guint v)
{
  /* http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
  v = v - ((v >> 1) & 0x55555555);
  v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
  return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

/* Computes specificity according to CSS 2.1.
 * The arguments must be initialized to 0 */
static void
_gtk_css_selector_get_specificity (const GtkCssSelector *selector,
                                   guint                *ids,
                                   guint                *classes,
                                   guint                *elements)
{
  GQuark *count;

  if (selector->previous)
    _gtk_css_selector_get_specificity (selector->previous, ids, classes, elements);

  if (selector->ids)
    for (count = selector->ids; *count; count++)
      (*ids)++;

  if (selector->classes)
    for (count = selector->classes; *count; count++)
      (*classes)++;
  
  *classes += count_bits (selector->state) + count_bits (selector->pseudo_classes);

  if (selector->name)
    (*elements)++;
}

int
_gtk_css_selector_compare (const GtkCssSelector *a,
                           const GtkCssSelector *b)
{
  guint a_ids = 0, a_classes = 0, a_elements = 0;
  guint b_ids = 0, b_classes = 0, b_elements = 0;
  int compare;

  _gtk_css_selector_get_specificity (a, &a_ids, &a_classes, &a_elements);
  _gtk_css_selector_get_specificity (b, &b_ids, &b_classes, &b_elements);

  compare = a_ids - b_ids;
  if (compare)
    return compare;

  compare = a_classes - b_classes;
  if (compare)
    return compare;

  return a_elements - b_elements;
}

GtkStateFlags
_gtk_css_selector_get_state_flags (GtkCssSelector *selector)
{
  g_return_val_if_fail (selector != NULL, 0);

  return selector->state;
}

