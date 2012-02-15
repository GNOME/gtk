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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssselectorprivate.h"

#include "gtkcssprovider.h"
#include "gtkstylecontextprivate.h"

typedef enum {
  GTK_CSS_COMBINE_DESCANDANT,
  GTK_CSS_COMBINE_CHILD
} GtkCssCombinator;

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

static GtkCssSelector *
gtk_css_selector_new (GtkCssSelector         *previous,
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

static gboolean
parse_selector_class (GtkCssParser *parser, GArray *classes)
{
  GQuark qname;
  char *name;
    
  name = _gtk_css_parser_try_name (parser, FALSE);

  if (name == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid name for class");
      return FALSE;
    }

  qname = g_quark_from_string (name);
  g_array_append_val (classes, qname);
  g_free (name);
  return TRUE;
}

static gboolean
parse_selector_name (GtkCssParser *parser, GArray *names)
{
  GQuark qname;
  char *name;
    
  name = _gtk_css_parser_try_name (parser, FALSE);

  if (name == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid name for id");
      return FALSE;
    }

  qname = g_quark_from_string (name);
  g_array_append_val (names, qname);
  g_free (name);
  return TRUE;
}

static gboolean
parse_selector_pseudo_class (GtkCssParser   *parser,
                             GtkRegionFlags *region_to_modify,
                             GtkStateFlags  *state_to_modify)
{
  struct {
    const char *name;
    GtkRegionFlags region_flag;
    GtkStateFlags state_flag;
  } pseudo_classes[] = {
    { "first-child",  GTK_REGION_FIRST, 0 },
    { "last-child",   GTK_REGION_LAST, 0 },
    { "only-child",   GTK_REGION_ONLY, 0 },
    { "sorted",       GTK_REGION_SORTED, 0 },
    { "active",       0, GTK_STATE_FLAG_ACTIVE },
    { "prelight",     0, GTK_STATE_FLAG_PRELIGHT },
    { "hover",        0, GTK_STATE_FLAG_PRELIGHT },
    { "selected",     0, GTK_STATE_FLAG_SELECTED },
    { "insensitive",  0, GTK_STATE_FLAG_INSENSITIVE },
    { "inconsistent", 0, GTK_STATE_FLAG_INCONSISTENT },
    { "focused",      0, GTK_STATE_FLAG_FOCUSED },
    { "focus",        0, GTK_STATE_FLAG_FOCUSED },
    { "backdrop",     0, GTK_STATE_FLAG_BACKDROP },
    { NULL, }
  }, nth_child_classes[] = {
    { "first",        GTK_REGION_FIRST, 0 },
    { "last",         GTK_REGION_LAST, 0 },
    { "even",         GTK_REGION_EVEN, 0 },
    { "odd",          GTK_REGION_ODD, 0 },
    { NULL, }
  }, *classes;
  guint i;
  char *name;
  GError *error;

  name = _gtk_css_parser_try_ident (parser, FALSE);
  if (name == NULL)
    {
      _gtk_css_parser_error (parser, "Missing name of pseudo-class");
      return FALSE;
    }

  if (_gtk_css_parser_try (parser, "(", TRUE))
    {
      char *function = name;

      name = _gtk_css_parser_try_ident (parser, TRUE);
      if (!_gtk_css_parser_try (parser, ")", FALSE))
        {
          _gtk_css_parser_error (parser, "Missing closing bracket for pseudo-class");
          return FALSE;
        }

      if (g_ascii_strcasecmp (function, "nth-child") != 0)
        {
          error = g_error_new (GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                               "Unknown pseudo-class '%s(%s)'", function, name ? name : "");
          _gtk_css_parser_take_error (parser, error);
          g_free (function);
          g_free (name);
          return FALSE;
        }
      
      g_free (function);
    
      if (name == NULL)
        {
          error = g_error_new (GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                               "Unknown pseudo-class 'nth-child(%s)'", name);
          _gtk_css_parser_take_error (parser, error);
          return FALSE;
        }

      classes = nth_child_classes;
    }
  else
    classes = pseudo_classes;

  for (i = 0; classes[i].name != NULL; i++)
    {
      if (g_ascii_strcasecmp (name, classes[i].name) == 0)
        {
          if ((*region_to_modify & classes[i].region_flag) ||
              (*state_to_modify & classes[i].state_flag))
            {
              if (classes == nth_child_classes)
                _gtk_css_parser_error (parser, "Duplicate pseudo-class 'nth-child(%s)'", name);
              else
                _gtk_css_parser_error (parser, "Duplicate pseudo-class '%s'", name);
            }
          *region_to_modify |= classes[i].region_flag;
          *state_to_modify |= classes[i].state_flag;

          g_free (name);
          return TRUE;
        }
    }

  if (classes == nth_child_classes)
    error = g_error_new (GTK_CSS_PROVIDER_ERROR,
                         GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                         "Unknown pseudo-class 'nth-child(%s)'", name);
  else
    error = g_error_new (GTK_CSS_PROVIDER_ERROR,
                         GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                         "Unknown pseudo-class '%s'", name);

  g_free (name);
  _gtk_css_parser_take_error (parser, error);

  return FALSE;
}

static gboolean
parse_simple_selector (GtkCssParser *parser,
                       char **name,
                       GArray *ids,
                       GArray *classes,
                       GtkRegionFlags *pseudo_classes,
                       GtkStateFlags *state)
{
  gboolean parsed_something;
  
  *name = _gtk_css_parser_try_ident (parser, FALSE);
  if (*name)
    parsed_something = TRUE;
  else
    parsed_something = _gtk_css_parser_try (parser, "*", FALSE);

  do {
      if (_gtk_css_parser_try (parser, "#", FALSE))
        {
          if (!parse_selector_name (parser, ids))
            return FALSE;
        }
      else if (_gtk_css_parser_try (parser, ".", FALSE))
        {
          if (!parse_selector_class (parser, classes))
            return FALSE;
        }
      else if (_gtk_css_parser_try (parser, ":", FALSE))
        {
          if (!parse_selector_pseudo_class (parser, pseudo_classes, state))
            return FALSE;
        }
      else if (!parsed_something)
        {
          _gtk_css_parser_error (parser, "Expected a valid selector");
          return FALSE;
        }
      else
        break;

      parsed_something = TRUE;
    }
  while (!_gtk_css_parser_is_eof (parser));

  _gtk_css_parser_skip_whitespace (parser);
  return TRUE;
}

GtkCssSelector *
_gtk_css_selector_parse (GtkCssParser *parser)
{
  GtkCssSelector *selector = NULL;

  do {
      char *name = NULL;
      GArray *ids = g_array_new (TRUE, FALSE, sizeof (GQuark));
      GArray *classes = g_array_new (TRUE, FALSE, sizeof (GQuark));
      GtkRegionFlags pseudo_classes = 0;
      GtkStateFlags state = 0;
      GtkCssCombinator combine = GTK_CSS_COMBINE_DESCANDANT;

      if (selector)
        {
          if (_gtk_css_parser_try (parser, ">", TRUE))
            combine = GTK_CSS_COMBINE_CHILD;
        }

      if (!parse_simple_selector (parser, &name, ids, classes, &pseudo_classes, &state))
        {
          g_array_free (ids, TRUE);
          g_array_free (classes, TRUE);
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }

      selector = gtk_css_selector_new (selector,
                                       combine,
                                       name,
                                       (GQuark *) g_array_free (ids, ids->len == 0),
                                       (GQuark *) g_array_free (classes, classes->len == 0),
                                       pseudo_classes,
                                       state);
      g_free (name);
    }
  while (!_gtk_css_parser_is_eof (parser) &&
         !_gtk_css_parser_begins_with (parser, ',') &&
         !_gtk_css_parser_begins_with (parser, '{'));

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
        "only-child",
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
        "focus",
        "backdrop"
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
  if (n_siblings == 1)
    flags |= GTK_REGION_ONLY;

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
 * @state: The state to match
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
                           GtkStateFlags              state)
{
  GSList *list;
  gboolean match;
  guint length;

  g_return_val_if_fail (selector != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if ((selector->state & state) != selector->state)
    return FALSE;

  length = gtk_widget_path_length (path);
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

