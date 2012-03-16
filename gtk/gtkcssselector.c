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

#include <string.h>

#include "gtkcssprovider.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetpath.h"

typedef struct _GtkCssSelectorClass GtkCssSelectorClass;

struct _GtkCssSelectorClass {
  const char        *name;

  void              (* print)   (const GtkCssSelector       *selector,
                                 GString                    *string);
  gboolean          (* match)   (const GtkCssSelector       *selector,
                                 GtkStateFlags               state,
                                 const GtkWidgetPath        *path,
                                 guint                       id,
                                 guint                       sibling);

  guint         increase_id_specificity :1;
  guint         increase_class_specificity :1;
  guint         increase_element_specificity :1;
};

struct _GtkCssSelector
{
  const GtkCssSelectorClass *class;       /* type of check this selector does */
  gconstpointer              data;        /* data for matching:
                                             - interned string for CLASS, NAME and ID
                                             - GUINT_TO_POINTER() for PSEUDOCLASS_REGION/STATE */
};

static gboolean
gtk_css_selector_match (const GtkCssSelector *selector,
                        GtkStateFlags         state,
                        const GtkWidgetPath  *path,
                        guint                 id,
                        guint                 sibling)
{
  if (selector == NULL)
    return TRUE;

  return selector->class->match (selector, state, path, id, sibling);
}

static const GtkCssSelector *
gtk_css_selector_previous (const GtkCssSelector *selector)
{
  selector = selector + 1;

  return selector->class ? selector : NULL;
}

/* DESCENDANT */

static void
gtk_css_selector_descendant_print (const GtkCssSelector *selector,
                                   GString              *string)
{
  g_string_append_c (string, ' ');
}

static gboolean
gtk_css_selector_descendant_match (const GtkCssSelector *selector,
                                   GtkStateFlags         state,
                                   const GtkWidgetPath  *path,
                                   guint                 id,
                                   guint                 sibling)
{
  while (id-- > 0)
    {
      if (gtk_css_selector_match (gtk_css_selector_previous (selector),
                                  0,
                                  path,
                                  id,
                                  gtk_widget_path_iter_get_sibling_index (path, id)))
        return TRUE;
    }

  return FALSE;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_DESCENDANT = {
  "descendant",
  gtk_css_selector_descendant_print,
  gtk_css_selector_descendant_match,
  FALSE, FALSE, FALSE
};

/* CHILD */

static void
gtk_css_selector_child_print (const GtkCssSelector *selector,
                              GString              *string)
{
  g_string_append (string, " > ");
}

static gboolean
gtk_css_selector_child_match (const GtkCssSelector *selector,
                              GtkStateFlags         state,
                              const GtkWidgetPath  *path,
                              guint                 id,
                              guint                 sibling)
{
  if (id == 0)
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector),
                                 0,
                                 path,
                                 id - 1,
                                 gtk_widget_path_iter_get_sibling_index (path, id - 1));
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_CHILD = {
  "child",
  gtk_css_selector_child_print,
  gtk_css_selector_child_match,
  FALSE, FALSE, FALSE
};

/* SIBLING */

static void
gtk_css_selector_sibling_print (const GtkCssSelector *selector,
                                GString              *string)
{
  g_string_append (string, " ~ ");
}

static gboolean
gtk_css_selector_sibling_match (const GtkCssSelector *selector,
                                GtkStateFlags         state,
                                const GtkWidgetPath  *path,
                                guint                 id,
                                guint                 sibling)
{
  while (sibling-- > 0)
    {
      if (gtk_css_selector_match (gtk_css_selector_previous (selector),
                                  0,
                                  path,
                                  id,
                                  sibling))
        return TRUE;
    }

  return FALSE;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_SIBLING = {
  "sibling",
  gtk_css_selector_sibling_print,
  gtk_css_selector_sibling_match,
  FALSE, FALSE, FALSE
};

/* ADJACENT */

static void
gtk_css_selector_adjacent_print (const GtkCssSelector *selector,
                                 GString              *string)
{
  g_string_append (string, " + ");
}

static gboolean
gtk_css_selector_adjacent_match (const GtkCssSelector *selector,
                                 GtkStateFlags         state,
                                 const GtkWidgetPath  *path,
                                 guint                 id,
                                 guint                 sibling)
{
  if (sibling == 0)
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector),
                                 0,
                                 path,
                                 id,
                                 sibling - 1);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ADJACENT = {
  "adjacent",
  gtk_css_selector_adjacent_print,
  gtk_css_selector_adjacent_match,
  FALSE, FALSE, FALSE
};

/* ANY */

static void
gtk_css_selector_any_print (const GtkCssSelector *selector,
                            GString              *string)
{
  g_string_append_c (string, '*');
}

static gboolean
gtk_css_selector_any_match (const GtkCssSelector *selector,
                            GtkStateFlags         state,
                            const GtkWidgetPath  *path,
                            guint                 id,
                            guint                 sibling)
{
  const GtkCssSelector *previous = gtk_css_selector_previous (selector);
  GSList *regions;
  
  if (previous &&
      previous->class == &GTK_CSS_SELECTOR_DESCENDANT &&
      (regions = gtk_widget_path_iter_list_regions (path, id)) != NULL)
    {
      g_slist_free (regions);
      if (gtk_css_selector_match (gtk_css_selector_previous (previous), state, path, id, sibling))
        return TRUE;
    }
  
  return gtk_css_selector_match (previous, state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ANY = {
  "any",
  gtk_css_selector_any_print,
  gtk_css_selector_any_match,
  FALSE, FALSE, FALSE
};

/* NAME */

static void
gtk_css_selector_name_print (const GtkCssSelector *selector,
                             GString              *string)
{
  g_string_append (string, selector->data);
}

static gboolean
gtk_css_selector_name_match (const GtkCssSelector *selector,
                             GtkStateFlags         state,
                             const GtkWidgetPath  *path,
                             guint                 id,
                             guint                 sibling)
{
  GType type = g_type_from_name (selector->data);

  if (!g_type_is_a (gtk_widget_path_iter_get_object_type (path, id), type))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_NAME = {
  "name",
  gtk_css_selector_name_print,
  gtk_css_selector_name_match,
  FALSE, FALSE, TRUE
};

/* REGION */

static void
gtk_css_selector_region_print (const GtkCssSelector *selector,
                               GString              *string)
{
  g_string_append (string, selector->data);
}

static gboolean
gtk_css_selector_region_match (const GtkCssSelector *selector,
                               GtkStateFlags         state,
                               const GtkWidgetPath  *path,
                               guint                 id,
                               guint                 sibling)
{
  const GtkCssSelector *previous;

  if (!gtk_widget_path_iter_has_region (path, id, selector->data, NULL))
    return FALSE;

  previous = gtk_css_selector_previous (selector);
  if (previous && previous->class == &GTK_CSS_SELECTOR_DESCENDANT &&
      gtk_css_selector_match (gtk_css_selector_previous (previous), state, path, id, sibling))
    return TRUE;

  return gtk_css_selector_match (previous, state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_REGION = {
  "region",
  gtk_css_selector_region_print,
  gtk_css_selector_region_match,
  FALSE, FALSE, TRUE
};

/* CLASS */

static void
gtk_css_selector_class_print (const GtkCssSelector *selector,
                              GString              *string)
{
  g_string_append_c (string, '.');
  g_string_append (string, selector->data);
}

static gboolean
gtk_css_selector_class_match (const GtkCssSelector *selector,
                              GtkStateFlags         state,
                              const GtkWidgetPath  *path,
                              guint                 id,
                              guint                 sibling)
{
  if (!gtk_widget_path_iter_has_class (path, id, selector->data))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_CLASS = {
  "class",
  gtk_css_selector_class_print,
  gtk_css_selector_class_match,
  FALSE, TRUE, FALSE
};

/* ID */

static void
gtk_css_selector_id_print (const GtkCssSelector *selector,
                           GString              *string)
{
  g_string_append_c (string, '#');
  g_string_append (string, selector->data);
}

static gboolean
gtk_css_selector_id_match (const GtkCssSelector *selector,
                           GtkStateFlags         state,
                           const GtkWidgetPath  *path,
                           guint                 id,
                           guint                 sibling)
{
  if (!gtk_widget_path_iter_has_name (path, id, selector->data))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ID = {
  "id",
  gtk_css_selector_id_print,
  gtk_css_selector_id_match,
  TRUE, FALSE, FALSE
};

/* PSEUDOCLASS FOR STATE */

static void
gtk_css_selector_pseudoclass_state_print (const GtkCssSelector *selector,
                                          GString              *string)
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
  guint i, state;

  state = GPOINTER_TO_UINT (selector->data);
  g_string_append_c (string, ':');

  for (i = 0; i < G_N_ELEMENTS (state_names); i++)
    {
      if (state == (1 << i))
        {
          g_string_append (string, state_names[i]);
          return;
        }
    }

  g_assert_not_reached ();
}

static gboolean
gtk_css_selector_pseudoclass_state_match (const GtkCssSelector *selector,
                                          GtkStateFlags         state,
                                          const GtkWidgetPath  *path,
                                          guint                 id,
                                          guint                 sibling)
{
  if (!(GPOINTER_TO_UINT (selector->data) & state))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_PSEUDOCLASS_STATE = {
  "pseudoclass-state",
  gtk_css_selector_pseudoclass_state_print,
  gtk_css_selector_pseudoclass_state_match,
  FALSE, TRUE, FALSE
};

/* PSEUDOCLASS FOR REGION */

static void
gtk_css_selector_pseudoclass_region_print (const GtkCssSelector *selector,
                                           GString              *string)
{
  static const char * flag_names[] = {
    "nth-child(even)",
    "nth-child(odd)",
    "first-child",
    "last-child",
    "only-child",
    "sorted"
  };
  guint i, state;

  state = GPOINTER_TO_UINT (selector->data);
  g_string_append_c (string, ':');

  for (i = 0; i < G_N_ELEMENTS (flag_names); i++)
    {
      if (state == (1 << i))
        {
          g_string_append (string, flag_names[i]);
          return;
        }
    }

  g_assert_not_reached ();
}

static gboolean
gtk_css_selector_pseudoclass_region_match_for_region (const GtkCssSelector *selector,
                                                      GtkStateFlags         state,
                                                      const GtkWidgetPath  *path,
                                                      guint                 id,
                                                      guint                 sibling)
{
  GtkRegionFlags selector_flags, path_flags;
  const GtkCssSelector *previous;
  
  selector_flags = GPOINTER_TO_UINT (selector->data);
  selector = gtk_css_selector_previous (selector);

  if (!gtk_widget_path_iter_has_region (path, id, selector->data, &path_flags))
    return FALSE;

  if ((selector_flags & path_flags) != selector_flags)
    return FALSE;

  previous = gtk_css_selector_previous (selector);
  if (previous && previous->class == &GTK_CSS_SELECTOR_DESCENDANT &&
      gtk_css_selector_match (gtk_css_selector_previous (previous), state, path, id, sibling))
    return TRUE;

  return gtk_css_selector_match (previous, state, path, id, sibling);
}

static gboolean
gtk_css_selector_pseudoclass_region_match (const GtkCssSelector *selector,
                                           GtkStateFlags         state,
                                           const GtkWidgetPath  *path,
                                           guint                 id,
                                           guint                 sibling)
{
  GtkRegionFlags region;
  const GtkWidgetPath *siblings;
  guint n_siblings;
  const GtkCssSelector *previous;

  previous = gtk_css_selector_previous (selector);
  if (previous && previous->class == &GTK_CSS_SELECTOR_REGION)
    return gtk_css_selector_pseudoclass_region_match_for_region (selector, state, path, id, sibling);

  siblings = gtk_widget_path_iter_get_siblings (path, id);
  if (siblings == NULL)
    return 0;

  region = GPOINTER_TO_UINT (selector->data);
  n_siblings = gtk_widget_path_length (siblings);

  switch (region)
    {
    case GTK_REGION_EVEN:
      if (!(sibling % 2))
        return FALSE;
      break;
    case GTK_REGION_ODD:
      if (sibling % 2)
        return FALSE;
      break;
    case GTK_REGION_FIRST:
      if (sibling != 0)
        return FALSE;
      break;
    case GTK_REGION_LAST:
      if (sibling + 1 != n_siblings)
        return FALSE;
      break;
    case GTK_REGION_ONLY:
      if (n_siblings != 1)
        return FALSE;
      break;
    case GTK_REGION_SORTED:
      return FALSE;
    default:
      g_assert_not_reached ();
      return FALSE;
    }

  return gtk_css_selector_match (previous, state, path, id, sibling);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_PSEUDOCLASS_REGION = {
  "pseudoclass-region",
  gtk_css_selector_pseudoclass_region_print,
  gtk_css_selector_pseudoclass_region_match,
  FALSE, TRUE, FALSE
};

/* API */

static guint
gtk_css_selector_size (const GtkCssSelector *selector)
{
  guint size = 0;

  while (selector)
    {
      selector = gtk_css_selector_previous (selector);
      size++;
    }

  return size;
}

static GtkCssSelector *
gtk_css_selector_new (const GtkCssSelectorClass *class,
                      GtkCssSelector            *selector,
                      gconstpointer              data)
{
  guint size;

  size = gtk_css_selector_size (selector);
  selector = g_realloc (selector, sizeof (GtkCssSelector) * (size + 1) + sizeof (gpointer));
  if (size == 0)
    selector[1].class = NULL;
  else
    memmove (selector + 1, selector, sizeof (GtkCssSelector) * size + sizeof (gpointer));

  selector->class = class;
  selector->data = data;

  return selector;
}

static GtkCssSelector *
parse_selector_class (GtkCssParser *parser, GtkCssSelector *selector)
{
  char *name;
    
  name = _gtk_css_parser_try_name (parser, FALSE);

  if (name == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid name for class");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_CLASS,
                                   selector,
                                   g_intern_string (name));

  g_free (name);

  return selector;
}

static GtkCssSelector *
parse_selector_id (GtkCssParser *parser, GtkCssSelector *selector)
{
  char *name;
    
  name = _gtk_css_parser_try_name (parser, FALSE);

  if (name == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid name for id");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ID,
                                   selector,
                                   g_intern_string (name));

  g_free (name);

  return selector;
}

static GtkCssSelector *
parse_selector_pseudo_class (GtkCssParser   *parser,
                             GtkCssSelector *selector)
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
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  if (_gtk_css_parser_try (parser, "(", TRUE))
    {
      char *function = name;

      name = _gtk_css_parser_try_ident (parser, TRUE);
      if (!_gtk_css_parser_try (parser, ")", FALSE))
        {
          _gtk_css_parser_error (parser, "Missing closing bracket for pseudo-class");
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }

      if (g_ascii_strcasecmp (function, "nth-child") != 0)
        {
          error = g_error_new (GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                               "Unknown pseudo-class '%s(%s)'", function, name ? name : "");
          _gtk_css_parser_take_error (parser, error);
          g_free (function);
          g_free (name);
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }
      
      g_free (function);
    
      if (name == NULL)
        {
          error = g_error_new (GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                               "Unknown pseudo-class 'nth-child(%s)'", name);
          _gtk_css_parser_take_error (parser, error);
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }

      classes = nth_child_classes;
    }
  else
    classes = pseudo_classes;

  for (i = 0; classes[i].name != NULL; i++)
    {
      if (g_ascii_strcasecmp (name, classes[i].name) == 0)
        {
          g_free (name);

          if (classes[i].region_flag)
            selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_PSEUDOCLASS_REGION,
                                             selector,
                                             GUINT_TO_POINTER (classes[i].region_flag));
          else
            selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_PSEUDOCLASS_STATE,
                                             selector,
                                             GUINT_TO_POINTER (classes[i].state_flag));

          return selector;
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

  _gtk_css_parser_take_error (parser, error);
  g_free (name);
  if (selector)
    _gtk_css_selector_free (selector);

  return NULL;
}

static GtkCssSelector *
try_parse_name (GtkCssParser   *parser,
                GtkCssSelector *selector)
{
  char *name;

  name = _gtk_css_parser_try_ident (parser, FALSE);
  if (name)
    {
      selector = gtk_css_selector_new (_gtk_style_context_check_region_name (name)
                                       ? &GTK_CSS_SELECTOR_REGION
                                       : &GTK_CSS_SELECTOR_NAME,
                                       selector,
                                       g_intern_string (name));
      g_free (name);
    }
  else if (_gtk_css_parser_try (parser, "*", FALSE))
    selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ANY, selector, NULL);
  
  return selector;
}

static GtkCssSelector *
parse_simple_selector (GtkCssParser   *parser,
                       GtkCssSelector *selector)
{
  guint size = gtk_css_selector_size (selector);
  
  selector = try_parse_name (parser, selector);

  do {
      if (_gtk_css_parser_try (parser, "#", FALSE))
        selector = parse_selector_id (parser, selector);
      else if (_gtk_css_parser_try (parser, ".", FALSE))
        selector = parse_selector_class (parser, selector);
      else if (_gtk_css_parser_try (parser, ":", FALSE))
        selector = parse_selector_pseudo_class (parser, selector);
      else if (gtk_css_selector_size (selector) == size)
        {
          _gtk_css_parser_error (parser, "Expected a valid selector");
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }
      else
        break;
    }
  while (selector && !_gtk_css_parser_is_eof (parser));

  _gtk_css_parser_skip_whitespace (parser);

  return selector;
}

GtkCssSelector *
_gtk_css_selector_parse (GtkCssParser *parser)
{
  GtkCssSelector *selector = NULL;

  while ((selector = parse_simple_selector (parser, selector)) &&
         !_gtk_css_parser_is_eof (parser) &&
         !_gtk_css_parser_begins_with (parser, ',') &&
         !_gtk_css_parser_begins_with (parser, '{'))
    {
      if (_gtk_css_parser_try (parser, "+", TRUE))
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ADJACENT, selector, NULL);
      else if (_gtk_css_parser_try (parser, "~", TRUE))
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_SIBLING, selector, NULL);
      else if (_gtk_css_parser_try (parser, ">", TRUE))
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_CHILD, selector, NULL);
      else
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_DESCENDANT, selector, NULL);
    }

  return selector;
}

void
_gtk_css_selector_free (GtkCssSelector *selector)
{
  g_return_if_fail (selector != NULL);

  g_free (selector);
}

void
_gtk_css_selector_print (const GtkCssSelector *selector,
                         GString *             str)
{
  const GtkCssSelector *previous;

  g_return_if_fail (selector != NULL);

  previous = gtk_css_selector_previous (selector);
  if (previous)
    _gtk_css_selector_print (previous, str);

  selector->class->print (selector, str);
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
  guint length;

  g_return_val_if_fail (selector != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  length = gtk_widget_path_length (path);
  if (length == 0)
    return FALSE;

  return gtk_css_selector_match (selector,
                                 state,
                                 path,
                                 length - 1,
                                 gtk_widget_path_iter_get_sibling_index (path, length - 1));
}

/* Computes specificity according to CSS 2.1.
 * The arguments must be initialized to 0 */
static void
_gtk_css_selector_get_specificity (const GtkCssSelector *selector,
                                   guint                *ids,
                                   guint                *classes,
                                   guint                *elements)
{
  for (; selector; selector = gtk_css_selector_previous (selector))
    {
      const GtkCssSelectorClass *klass = selector->class;

      if (klass->increase_id_specificity)
        (*ids)++;
      if (klass->increase_class_specificity)
        (*classes)++;
      if (klass->increase_element_specificity)
        (*elements)++;
    }
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
_gtk_css_selector_get_state_flags (const GtkCssSelector *selector)
{
  GtkStateFlags state = 0;

  g_return_val_if_fail (selector != NULL, 0);

  for (; selector && selector->class == &GTK_CSS_SELECTOR_NAME; selector = gtk_css_selector_previous (selector))
    state |= GPOINTER_TO_UINT (selector->data);

  return state;
}

