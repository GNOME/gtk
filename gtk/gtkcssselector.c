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

typedef struct _GtkCssSelectorClass GtkCssSelectorClass;

struct _GtkCssSelectorClass {
  const char        *name;

  void              (* print)       (const GtkCssSelector       *selector,
                                     GString                    *string);
  gboolean          (* match)       (const GtkCssSelector       *selector,
                                     const GtkCssMatcher        *matcher);
  GtkCssChange      (* get_change)  (const GtkCssSelector       *selector);

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
                        const GtkCssMatcher  *matcher)
{
  if (selector == NULL)
    return TRUE;

  return selector->class->match (selector, matcher);
}

static GtkCssChange
gtk_css_selector_get_change (const GtkCssSelector *selector)
{
  if (selector == NULL)
    return 0;

  return selector->class->get_change (selector);
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
                                   const GtkCssMatcher  *matcher)
{
  GtkCssMatcher ancestor;

  while (_gtk_css_matcher_get_parent (&ancestor, matcher))
    {
      matcher = &ancestor;

      if (gtk_css_selector_match (gtk_css_selector_previous (selector), matcher))
        return TRUE;
    }

  return FALSE;
}

static GtkCssChange
gtk_css_selector_descendant_get_change (const GtkCssSelector *selector)
{
  return _gtk_css_change_for_child (gtk_css_selector_get_change (gtk_css_selector_previous (selector)));
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_DESCENDANT = {
  "descendant",
  gtk_css_selector_descendant_print,
  gtk_css_selector_descendant_match,
  gtk_css_selector_descendant_get_change,
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
                              const GtkCssMatcher  *matcher)
{
  GtkCssMatcher parent;

  if (!_gtk_css_matcher_get_parent (&parent, matcher))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), &parent);
}

static GtkCssChange
gtk_css_selector_child_get_change (const GtkCssSelector *selector)
{
  return _gtk_css_change_for_child (gtk_css_selector_get_change (gtk_css_selector_previous (selector)));
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_CHILD = {
  "child",
  gtk_css_selector_child_print,
  gtk_css_selector_child_match,
  gtk_css_selector_child_get_change,
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
                                const GtkCssMatcher  *matcher)
{
  GtkCssMatcher previous;

  while (_gtk_css_matcher_get_previous (&previous, matcher))
    {
      matcher = &previous;

      if (gtk_css_selector_match (gtk_css_selector_previous (selector), matcher))
        return TRUE;
    }

  return FALSE;
}

static GtkCssChange
gtk_css_selector_sibling_get_change (const GtkCssSelector *selector)
{
  return _gtk_css_change_for_sibling (gtk_css_selector_get_change (gtk_css_selector_previous (selector)));
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_SIBLING = {
  "sibling",
  gtk_css_selector_sibling_print,
  gtk_css_selector_sibling_match,
  gtk_css_selector_sibling_get_change,
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
                                 const GtkCssMatcher  *matcher)
{
  GtkCssMatcher previous;

  if (!_gtk_css_matcher_get_previous (&previous, matcher))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), &previous);
}

static GtkCssChange
gtk_css_selector_adjacent_get_change (const GtkCssSelector *selector)
{
  return _gtk_css_change_for_sibling (gtk_css_selector_get_change (gtk_css_selector_previous (selector)));
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ADJACENT = {
  "adjacent",
  gtk_css_selector_adjacent_print,
  gtk_css_selector_adjacent_match,
  gtk_css_selector_adjacent_get_change,
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
                            const GtkCssMatcher  *matcher)
{
  const GtkCssSelector *previous = gtk_css_selector_previous (selector);
  
  if (previous &&
      previous->class == &GTK_CSS_SELECTOR_DESCENDANT &&
      _gtk_css_matcher_has_regions (matcher))
    {
      if (gtk_css_selector_match (gtk_css_selector_previous (previous), matcher))
        return TRUE;
    }
  
  return gtk_css_selector_match (previous, matcher);
}

static GtkCssChange
gtk_css_selector_any_get_change (const GtkCssSelector *selector)
{
  return gtk_css_selector_get_change (gtk_css_selector_previous (selector));
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ANY = {
  "any",
  gtk_css_selector_any_print,
  gtk_css_selector_any_match,
  gtk_css_selector_any_get_change,
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
                             const GtkCssMatcher  *matcher)
{
  if (!_gtk_css_matcher_has_name (matcher, selector->data))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher);
}

static GtkCssChange
gtk_css_selector_name_get_change (const GtkCssSelector *selector)
{
  return gtk_css_selector_get_change (gtk_css_selector_previous (selector)) | GTK_CSS_CHANGE_NAME;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_NAME = {
  "name",
  gtk_css_selector_name_print,
  gtk_css_selector_name_match,
  gtk_css_selector_name_get_change,
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
                               const GtkCssMatcher  *matcher)
{
  const GtkCssSelector *previous;

  if (!_gtk_css_matcher_has_region (matcher, selector->data, 0))
    return FALSE;

  previous = gtk_css_selector_previous (selector);
  if (previous && previous->class == &GTK_CSS_SELECTOR_DESCENDANT &&
      gtk_css_selector_match (gtk_css_selector_previous (previous), matcher))
    return TRUE;

  return gtk_css_selector_match (previous, matcher);
}

static GtkCssChange
gtk_css_selector_region_get_change (const GtkCssSelector *selector)
{
  GtkCssChange change;

  change = gtk_css_selector_get_change (gtk_css_selector_previous (selector));
  change |= GTK_CSS_CHANGE_REGION;
  change |= _gtk_css_change_for_child (change);

  return change;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_REGION = {
  "region",
  gtk_css_selector_region_print,
  gtk_css_selector_region_match,
  gtk_css_selector_region_get_change,
  FALSE, FALSE, TRUE
};

/* CLASS */

static void
gtk_css_selector_class_print (const GtkCssSelector *selector,
                              GString              *string)
{
  g_string_append_c (string, '.');
  g_string_append (string, g_quark_to_string (GPOINTER_TO_UINT (selector->data)));
}

static gboolean
gtk_css_selector_class_match (const GtkCssSelector *selector,
                              const GtkCssMatcher  *matcher)
{
  if (!_gtk_css_matcher_has_class (matcher, GPOINTER_TO_UINT (selector->data)))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher);
}

static GtkCssChange
gtk_css_selector_class_get_change (const GtkCssSelector *selector)
{
  return gtk_css_selector_get_change (gtk_css_selector_previous (selector)) | GTK_CSS_CHANGE_CLASS;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_CLASS = {
  "class",
  gtk_css_selector_class_print,
  gtk_css_selector_class_match,
  gtk_css_selector_class_get_change,
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
                           const GtkCssMatcher  *matcher)
{
  if (!_gtk_css_matcher_has_id (matcher, selector->data))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher);
}

static GtkCssChange
gtk_css_selector_id_get_change (const GtkCssSelector *selector)
{
  return gtk_css_selector_get_change (gtk_css_selector_previous (selector)) | GTK_CSS_CHANGE_ID;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ID = {
  "id",
  gtk_css_selector_id_print,
  gtk_css_selector_id_match,
  gtk_css_selector_id_get_change,
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
                                          const GtkCssMatcher  *matcher)
{
  GtkStateFlags state = GPOINTER_TO_UINT (selector->data);

  if ((_gtk_css_matcher_get_state (matcher) & state) != state)
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher);
}

static GtkCssChange
gtk_css_selector_pseudoclass_state_get_change (const GtkCssSelector *selector)
{
  return gtk_css_selector_get_change (gtk_css_selector_previous (selector)) | GTK_CSS_CHANGE_STATE;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_PSEUDOCLASS_STATE = {
  "pseudoclass-state",
  gtk_css_selector_pseudoclass_state_print,
  gtk_css_selector_pseudoclass_state_match,
  gtk_css_selector_pseudoclass_state_get_change,
  FALSE, TRUE, FALSE
};

/* PSEUDOCLASS FOR POSITION */

typedef enum {
  POSITION_FORWARD,
  POSITION_BACKWARD,
  POSITION_ONLY,
  POSITION_SORTED
} PositionType;
#define POSITION_TYPE_BITS 2
#define POSITION_NUMBER_BITS ((sizeof (gpointer) * 8 - POSITION_TYPE_BITS) / 2)

static gconstpointer
encode_position (PositionType type,
                 int          a,
                 int          b)
{
  union {
    gconstpointer p;
    struct {
      gssize type :POSITION_TYPE_BITS;
      gssize a :POSITION_NUMBER_BITS;
      gssize b :POSITION_NUMBER_BITS;
    } data;
  } result;
  G_STATIC_ASSERT (sizeof (gconstpointer) == sizeof (result));

  g_assert (type < (1 << POSITION_TYPE_BITS));

  result.data.type = type;
  result.data.a = a;
  result.data.b = b;

  return result.p;
}

static void
decode_position (const GtkCssSelector *selector,
                 PositionType         *type,
                 int                  *a,
                 int                  *b)
{
  union {
    gconstpointer p;
    struct {
      gssize type :POSITION_TYPE_BITS;
      gssize a :POSITION_NUMBER_BITS;
      gssize b :POSITION_NUMBER_BITS;
    } data;
  } result;
  G_STATIC_ASSERT (sizeof (gconstpointer) == sizeof (result));

  result.p = selector->data;

  *type = result.data.type & ((1 << POSITION_TYPE_BITS) - 1);
  *a = result.data.a;
  *b = result.data.b;
}

static void
gtk_css_selector_pseudoclass_position_print (const GtkCssSelector *selector,
                                             GString              *string)
{
  PositionType type;
  int a, b;

  decode_position (selector, &type, &a, &b);
  switch (type)
    {
    case POSITION_FORWARD:
      if (a == 0)
        {
          if (b == 1)
            g_string_append (string, ":first-child");
          else
            g_string_append_printf (string, ":nth-child(%d)", b);
        }
      else if (a == 2 && b == 0)
        g_string_append (string, ":nth-child(even)");
      else if (a == 2 && b == 1)
        g_string_append (string, ":nth-child(odd)");
      else
        {
          g_string_append (string, ":nth-child(");
          if (a == 1)
            g_string_append (string, "n");
          else if (a == -1)
            g_string_append (string, "-n");
          else
            g_string_append_printf (string, "%dn", a);
          if (b > 0)
            g_string_append_printf (string, "+%d)", b);
          else if (b < 0)
            g_string_append_printf (string, "%d)", b);
          else
            g_string_append (string, ")");
        }
      break;
    case POSITION_BACKWARD:
      if (a == 0)
        {
          if (b == 1)
            g_string_append (string, ":last-child");
          else
            g_string_append_printf (string, ":nth-last-child(%d)", b);
        }
      else if (a == 2 && b == 0)
        g_string_append (string, ":nth-last-child(even)");
      else if (a == 2 && b == 1)
        g_string_append (string, ":nth-last-child(odd)");
      else
        {
          g_string_append (string, ":nth-last-child(");
          if (a == 1)
            g_string_append (string, "n");
          else if (a == -1)
            g_string_append (string, "-n");
          else
            g_string_append_printf (string, "%dn", a);
          if (b > 0)
            g_string_append_printf (string, "+%d)", b);
          else if (b < 0)
            g_string_append_printf (string, "%d)", b);
          else
            g_string_append (string, ")");
        }
      break;
    case POSITION_ONLY:
      g_string_append (string, ":only-child");
      break;
    case POSITION_SORTED:
      g_string_append (string, ":sorted");
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
gtk_css_selector_pseudoclass_position_match_for_region (const GtkCssSelector *selector,
                                                        const GtkCssMatcher  *matcher)
{
  GtkRegionFlags selector_flags;
  const GtkCssSelector *previous;
  PositionType type;
  int a, b;

  decode_position (selector, &type, &a, &b);
  switch (type)
    {
    case POSITION_FORWARD:
      if (a == 0 && b == 1)
        selector_flags = GTK_REGION_FIRST;
      else if (a == 2 && b == 0)
        selector_flags = GTK_REGION_EVEN;
      else if (a == 2 && b == 1)
        selector_flags = GTK_REGION_ODD;
      else
        return FALSE;
      break;
    case POSITION_BACKWARD:
      if (a == 0 && b == 1)
        selector_flags = GTK_REGION_LAST;
      else
        return FALSE;
      break;
    case POSITION_ONLY:
      selector_flags = GTK_REGION_ONLY;
      break;
    case POSITION_SORTED:
      selector_flags = GTK_REGION_SORTED;
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  selector = gtk_css_selector_previous (selector);

  if (!_gtk_css_matcher_has_region (matcher, selector->data, selector_flags))
    return FALSE;

  previous = gtk_css_selector_previous (selector);
  if (previous && previous->class == &GTK_CSS_SELECTOR_DESCENDANT &&
      gtk_css_selector_match (gtk_css_selector_previous (previous), matcher))
    return TRUE;

  return gtk_css_selector_match (previous, matcher);
}

static gboolean
gtk_css_selector_pseudoclass_position_match (const GtkCssSelector *selector,
                                             const GtkCssMatcher  *matcher)
{
  const GtkCssSelector *previous;
  PositionType type;
  int a, b;

  previous = gtk_css_selector_previous (selector);
  if (previous && previous->class == &GTK_CSS_SELECTOR_REGION)
    return gtk_css_selector_pseudoclass_position_match_for_region (selector, matcher);

  decode_position (selector, &type, &a, &b);
  switch (type)
    {
    case POSITION_FORWARD:
      if (!_gtk_css_matcher_has_position (matcher, TRUE, a, b))
        return FALSE;
      break;
    case POSITION_BACKWARD:
      if (!_gtk_css_matcher_has_position (matcher, FALSE, a, b))
        return FALSE;
      break;
    case POSITION_ONLY:
      if (!_gtk_css_matcher_has_position (matcher, TRUE, 0, 1) ||
          !_gtk_css_matcher_has_position (matcher, FALSE, 0, 1))
        return FALSE;
      break;
    case POSITION_SORTED:
      return FALSE;
    default:
      g_assert_not_reached ();
      return FALSE;
    }

  return gtk_css_selector_match (previous, matcher);
}

static GtkCssChange
gtk_css_selector_pseudoclass_position_get_change (const GtkCssSelector *selector)
{
  return gtk_css_selector_get_change (gtk_css_selector_previous (selector)) | GTK_CSS_CHANGE_POSITION;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION = {
  "pseudoclass-position",
  gtk_css_selector_pseudoclass_position_print,
  gtk_css_selector_pseudoclass_position_match,
  gtk_css_selector_pseudoclass_position_get_change,
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
                                   GUINT_TO_POINTER (g_quark_from_string (name)));

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
parse_selector_pseudo_class_nth_child (GtkCssParser   *parser,
                                       GtkCssSelector *selector,
                                       PositionType    type)
{
  int a, b;

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing opening bracket for pseudo-class");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  if (_gtk_css_parser_try (parser, "even", TRUE))
    {
      a = 2;
      b = 0;
    }
  else if (_gtk_css_parser_try (parser, "odd", TRUE))
    {
      a = 2;
      b = 1;
    }
  else if (type == POSITION_FORWARD &&
           _gtk_css_parser_try (parser, "first", TRUE))
    {
      a = 0;
      b = 1;
    }
  else if (type == POSITION_FORWARD &&
           _gtk_css_parser_try (parser, "last", TRUE))
    {
      a = 0;
      b = 1;
      type = POSITION_BACKWARD;
    }
  else
    {
      int multiplier;

      if (_gtk_css_parser_try (parser, "+", TRUE))
        multiplier = 1;
      else if (_gtk_css_parser_try (parser, "-", TRUE))
        multiplier = -1;
      else
        multiplier = 1;

      if (_gtk_css_parser_try_int (parser, &a))
        {
          if (a < 0)
            {
              _gtk_css_parser_error (parser, "Expected an integer");
              if (selector)
                _gtk_css_selector_free (selector);
              return NULL;
            }
          a *= multiplier;
        }
      else if (_gtk_css_parser_has_prefix (parser, "n"))
        {
          a = multiplier;
        }
      else
        {
          _gtk_css_parser_error (parser, "Expected an integer");
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }

      if (_gtk_css_parser_try (parser, "n", TRUE))
        {
          if (_gtk_css_parser_try (parser, "+", TRUE))
            multiplier = 1;
          else if (_gtk_css_parser_try (parser, "-", TRUE))
            multiplier = -1;
          else
            multiplier = 1;

          if (_gtk_css_parser_try_int (parser, &b))
            {
              if (b < 0)
                {
                  _gtk_css_parser_error (parser, "Expected an integer");
                  if (selector)
                    _gtk_css_selector_free (selector);
                  return NULL;
                }
            }
          else
            b = 0;

          b *= multiplier;
        }
      else
        {
          b = a;
          a = 0;
        }
    }

  if (!_gtk_css_parser_try (parser, ")", FALSE))
    {
      _gtk_css_parser_error (parser, "Missing closing bracket for pseudo-class");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                   selector,
                                   encode_position (type, a, b));

  return selector;
}

static GtkCssSelector *
parse_selector_pseudo_class (GtkCssParser   *parser,
                             GtkCssSelector *selector)
{
  static const struct {
    const char    *name;
    GtkStateFlags  state_flag;
    PositionType   position_type;
    int            position_a;
    int            position_b;
  } pseudo_classes[] = {
    { "first-child",  0,                           POSITION_FORWARD,  0, 1 },
    { "last-child",   0,                           POSITION_BACKWARD, 0, 1 },
    { "only-child",   0,                           POSITION_ONLY,     0, 0 },
    { "sorted",       0,                           POSITION_SORTED,   0, 0 },
    { "active",       GTK_STATE_FLAG_ACTIVE, },
    { "prelight",     GTK_STATE_FLAG_PRELIGHT, },
    { "hover",        GTK_STATE_FLAG_PRELIGHT, },
    { "selected",     GTK_STATE_FLAG_SELECTED, },
    { "insensitive",  GTK_STATE_FLAG_INSENSITIVE, },
    { "inconsistent", GTK_STATE_FLAG_INCONSISTENT, },
    { "focused",      GTK_STATE_FLAG_FOCUSED, },
    { "focus",        GTK_STATE_FLAG_FOCUSED, },
    { "backdrop",     GTK_STATE_FLAG_BACKDROP, }
  };
  guint i;

  if (_gtk_css_parser_try (parser, "nth-child", FALSE))
    return parse_selector_pseudo_class_nth_child (parser, selector, POSITION_FORWARD);
  else if (_gtk_css_parser_try (parser, "nth-last-child", FALSE))
    return parse_selector_pseudo_class_nth_child (parser, selector, POSITION_BACKWARD);

  for (i = 0; i < G_N_ELEMENTS (pseudo_classes); i++)
    {
      if (_gtk_css_parser_try (parser, pseudo_classes[i].name, FALSE))
        {
          if (pseudo_classes[i].state_flag)
            selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_PSEUDOCLASS_STATE,
                                             selector,
                                             GUINT_TO_POINTER (pseudo_classes[i].state_flag));
          else
            selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                             selector,
                                             encode_position (pseudo_classes[i].position_type,
                                                              pseudo_classes[i].position_a,
                                                              pseudo_classes[i].position_b));
          return selector;
        }
    }
      
  _gtk_css_parser_error (parser, "Missing name of pseudo-class");
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

GtkCssChange
_gtk_css_selector_get_change (const GtkCssSelector *selector)
{
  g_return_val_if_fail (selector != NULL, 0);

  return gtk_css_selector_get_change (selector);
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
_gtk_css_selector_matches (const GtkCssSelector *selector,
                           const GtkCssMatcher  *matcher)
{

  g_return_val_if_fail (selector != NULL, FALSE);
  g_return_val_if_fail (matcher != NULL, FALSE);

  return gtk_css_selector_match (selector, matcher);
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

