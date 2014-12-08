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

#include <stdlib.h>
#include <string.h>

#include "gtkcssprovider.h"
#include "gtkstylecontextprivate.h"

/* When checking for changes via the tree we need to know if a rule further
   down the tree matched, because if so we need to add "our bit" to the
   Change. For instance in a a match like *.class:active we'll
   get a tree that first checks :active, if that matches we continue down
   to the tree, and if we get a match we add CHANGE_CLASS. However, the
   end of the tree where we have a match is an ANY which doesn't actually
   modify the change, so we don't know if we have a match or not. We fix
   this by setting GTK_CSS_CHANGE_GOT_MATCH which lets us guarantee
   that change != 0 on any match. */
#define GTK_CSS_CHANGE_GOT_MATCH GTK_CSS_CHANGE_RESERVED_BIT

typedef struct _GtkCssSelectorClass GtkCssSelectorClass;

struct _GtkCssSelectorClass {
  const char        *name;

  void              (* print)       (const GtkCssSelector       *selector,
                                     GString                    *string);
  gboolean          (* match)       (const GtkCssSelector       *selector,
                                     const GtkCssMatcher        *matcher);
  void              (* tree_match)  (const GtkCssSelectorTree   *tree,
                                     const GtkCssMatcher        *matcher,
				     GHashTable                  *res);
  GtkCssChange      (* get_change)  (const GtkCssSelector       *selector,
				     GtkCssChange                previous_change);
  GtkCssChange      (* tree_get_change)  (const GtkCssSelectorTree *tree,
					  const GtkCssMatcher      *matcher);
  guint             (* hash_one)    (const GtkCssSelector       *selector);
  int               (* compare_one) (const GtkCssSelector       *a,
				     const GtkCssSelector       *b);

  guint         increase_id_specificity :1;
  guint         increase_class_specificity :1;
  guint         increase_element_specificity :1;
  guint         is_simple :1;
  guint         must_keep_order :1; /* Due to region weirdness these must be kept before a DESCENDANT, so don't reorder */
};

typedef struct {
  GType type;
  const char *name;
} TypeReference;

typedef enum {
  POSITION_FORWARD,
  POSITION_BACKWARD,
  POSITION_ONLY,
  POSITION_SORTED
} PositionType;
#define POSITION_TYPE_BITS 2
#define POSITION_NUMBER_BITS ((sizeof (gpointer) * 8 - POSITION_TYPE_BITS) / 2)

union _GtkCssSelector
{
  const GtkCssSelectorClass     *class;         /* type of check this selector does */
  struct {
    const GtkCssSelectorClass   *class;
    const char                  *name;          /* interned */
  }                              id;
  struct {
    const GtkCssSelectorClass   *class;
    const char                  *name;          /* interned */
    GtkRegionFlags               flags;
  }                              region;
  struct {
    const GtkCssSelectorClass   *class;
    GQuark                       style_class;
  }                              style_class;
  struct {
    const GtkCssSelectorClass   *class;
    const TypeReference         *reference;
  }                              name;
  struct {
    const GtkCssSelectorClass   *class;
    GtkStateFlags                state;
  }                              state;
  struct {
    const GtkCssSelectorClass   *class;
    PositionType                 type :POSITION_TYPE_BITS;
    gssize                       a :POSITION_NUMBER_BITS;
    gssize                       b :POSITION_NUMBER_BITS;
  }                              position;
};

#define GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET G_MAXINT32
struct _GtkCssSelectorTree
{
  GtkCssSelector selector;
  gint32 parent_offset;
  gint32 previous_offset;
  gint32 sibling_offset;
  gint32 matches_offset; /* pointers that we return as matches if selector matches */
};

static gboolean
gtk_css_selector_equal (const GtkCssSelector *a,
			const GtkCssSelector *b)
{
  return
    a->class == b->class &&
    a->class->compare_one (a, b) == 0;
}

static guint
gtk_css_selector_hash_one (const GtkCssSelector *selector)
{
  return GPOINTER_TO_UINT (selector->class) ^ selector->class->hash_one (selector);
}

static gpointer *
gtk_css_selector_tree_get_matches (const GtkCssSelectorTree *tree)
{
  if (tree->matches_offset == GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
    return NULL;

  return (gpointer *) ((guint8 *)tree + tree->matches_offset);
}

static void
gtk_css_selector_tree_found_match (const GtkCssSelectorTree *tree,
				   GHashTable *res)
{
  int i;
  gpointer *matches;

  matches = gtk_css_selector_tree_get_matches (tree);
  if (matches)
    {
      for (i = 0; matches[i] != NULL; i++)
	g_hash_table_insert (res, matches[i], matches[i]);
    }
}

static void
gtk_css_selector_tree_match (const GtkCssSelectorTree *tree,
			     const GtkCssMatcher  *matcher,
			     GHashTable *res)
{
  if (tree == NULL)
    return;

  tree->selector.class->tree_match (tree, matcher, res);
}

static GtkCssChange
gtk_css_selector_tree_get_change (const GtkCssSelectorTree *tree,
				  const GtkCssMatcher      *matcher)
{
  if (tree == NULL)
    return 0;

  return tree->selector.class->tree_get_change (tree, matcher);
}

static gboolean
gtk_css_selector_match (const GtkCssSelector *selector,
                        const GtkCssMatcher  *matcher)
{
  if (selector == NULL)
    return TRUE;

  return selector->class->match (selector, matcher);
}

static int
gtk_css_selector_compare_one (const GtkCssSelector *a, const GtkCssSelector *b)
{
  if (a->class != b->class)
    return strcmp (a->class->name, b->class->name);
  else
    return a->class->compare_one (a, b);
}
  
static const GtkCssSelector *
gtk_css_selector_previous (const GtkCssSelector *selector)
{
  selector = selector + 1;

  return selector->class ? selector : NULL;
}

static const GtkCssSelectorTree *
gtk_css_selector_tree_at_offset (const GtkCssSelectorTree *tree,
				 gint32 offset)
{
  if (offset == GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
    return NULL;

  return (GtkCssSelectorTree *) ((guint8 *)tree + offset);
}

static const GtkCssSelectorTree *
gtk_css_selector_tree_get_parent (const GtkCssSelectorTree *tree)
{
  return gtk_css_selector_tree_at_offset (tree, tree->parent_offset);
}

static const GtkCssSelectorTree *
gtk_css_selector_tree_get_previous (const GtkCssSelectorTree *tree)
{
  return gtk_css_selector_tree_at_offset (tree, tree->previous_offset);
}

static const GtkCssSelectorTree *
gtk_css_selector_tree_get_sibling (const GtkCssSelectorTree *tree)
{
  return gtk_css_selector_tree_at_offset (tree, tree->sibling_offset);
}

static void
gtk_css_selector_tree_match_previous (const GtkCssSelectorTree *tree,
				      const GtkCssMatcher *matcher,
				      GHashTable *res)
{
  const GtkCssSelectorTree *prev;

  for (prev = gtk_css_selector_tree_get_previous (tree);
       prev != NULL;
       prev = gtk_css_selector_tree_get_sibling (prev))
    gtk_css_selector_tree_match (prev, matcher, res);
}

static GtkCssChange
gtk_css_selector_tree_get_previous_change (const GtkCssSelectorTree *tree,
					   const GtkCssMatcher      *matcher)
{
  GtkCssChange previous_change = 0;
  const GtkCssSelectorTree *prev;

  for (prev = gtk_css_selector_tree_get_previous (tree);
       prev != NULL;
       prev = gtk_css_selector_tree_get_sibling (prev))
    previous_change |= gtk_css_selector_tree_get_change (prev, matcher);

  return previous_change;
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

static void
gtk_css_selector_descendant_tree_match (const GtkCssSelectorTree *tree,
					const GtkCssMatcher  *matcher,
					GHashTable *res)
{
  GtkCssMatcher ancestor;

  while (_gtk_css_matcher_get_parent (&ancestor, matcher))
    {
      matcher = &ancestor;

      gtk_css_selector_tree_match_previous (tree, matcher, res);

      /* any matchers are dangerous here, as we may loop forever, but
	 we can terminate now as all possible matches have already been added */
      if (_gtk_css_matcher_matches_any (matcher))
	break;
    }
}

static GtkCssChange
gtk_css_selector_descendant_tree_get_change (const GtkCssSelectorTree *tree,
					     const GtkCssMatcher  *matcher)
{
  GtkCssMatcher ancestor;
  GtkCssChange change, previous_change;

  change = 0;
  previous_change = 0;
  while (_gtk_css_matcher_get_parent (&ancestor, matcher))
    {
      matcher = &ancestor;

      previous_change |= gtk_css_selector_tree_get_previous_change (tree, matcher);

      /* any matchers are dangerous here, as we may loop forever, but
	 we can terminate now as all possible matches have already been added */
      if (_gtk_css_matcher_matches_any (matcher))
	break;
    }

  if (previous_change != 0)
    change |= _gtk_css_change_for_child (previous_change) | GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static guint
gtk_css_selector_descendant_hash_one (const GtkCssSelector *a)
{
  return 0;
}

static int
gtk_css_selector_descendant_compare_one (const GtkCssSelector *a,
					 const GtkCssSelector *b)
{
  return 0;
}
  
static GtkCssChange
gtk_css_selector_descendant_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_child (previous_change);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_DESCENDANT = {
  "descendant",
  gtk_css_selector_descendant_print,
  gtk_css_selector_descendant_match,
  gtk_css_selector_descendant_tree_match,
  gtk_css_selector_descendant_get_change,
  gtk_css_selector_descendant_tree_get_change,
  gtk_css_selector_descendant_hash_one,
  gtk_css_selector_descendant_compare_one,
  FALSE, FALSE, FALSE, FALSE, FALSE
};

/* DESCENDANT FOR REGION */

static void
gtk_css_selector_descendant_for_region_print (const GtkCssSelector *selector,
                                              GString              *string)
{
  g_string_append_c (string, ' ');
}

static gboolean
gtk_css_selector_descendant_for_region_match (const GtkCssSelector *selector,
                                              const GtkCssMatcher  *matcher)
{
  GtkCssMatcher ancestor;

  if (_gtk_css_matcher_has_regions (matcher))
    {
      if (gtk_css_selector_match (gtk_css_selector_previous (selector), matcher))
        return TRUE;
    }

  while (_gtk_css_matcher_get_parent (&ancestor, matcher))
    {
      matcher = &ancestor;

      if (gtk_css_selector_match (gtk_css_selector_previous (selector), matcher))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_css_selector_descendant_for_region_tree_match (const GtkCssSelectorTree *tree,
                                                   const GtkCssMatcher  *matcher,
                                                   GHashTable *res)
{
  GtkCssMatcher ancestor;

  if (_gtk_css_matcher_has_regions (matcher))
    gtk_css_selector_tree_match_previous (tree, matcher, res);

  while (_gtk_css_matcher_get_parent (&ancestor, matcher))
    {
      matcher = &ancestor;

      gtk_css_selector_tree_match_previous (tree, matcher, res);

      /* any matchers are dangerous here, as we may loop forever, but
	 we can terminate now as all possible matches have already been added */
      if (_gtk_css_matcher_matches_any (matcher))
	break;
    }
}

static GtkCssChange
gtk_css_selector_descendant_for_region_tree_get_change (const GtkCssSelectorTree *tree,
					                const GtkCssMatcher  *matcher)
{
  GtkCssMatcher ancestor;
  GtkCssChange change, previous_change;

  previous_change = gtk_css_selector_tree_get_previous_change (tree, matcher);

  change = 0;

  while (_gtk_css_matcher_get_parent (&ancestor, matcher))
    {
      matcher = &ancestor;

      previous_change |= _gtk_css_change_for_child (gtk_css_selector_tree_get_previous_change (tree, matcher));

      /* any matchers are dangerous here, as we may loop forever, but
	 we can terminate now as all possible matches have already been added */
      if (_gtk_css_matcher_matches_any (matcher))
	break;
    }

  if (previous_change != 0)
    change |= previous_change | GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static guint
gtk_css_selector_descendant_for_region_hash_one (const GtkCssSelector *a)
{
  return 0;
}

static int
gtk_css_selector_descendant_for_region_compare_one (const GtkCssSelector *a,
					            const GtkCssSelector *b)
{
  return 0;
}
  
static GtkCssChange
gtk_css_selector_descendant_for_region_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return previous_change | _gtk_css_change_for_child (previous_change);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_DESCENDANT_FOR_REGION = {
  "descendant_for_region",
  gtk_css_selector_descendant_for_region_print,
  gtk_css_selector_descendant_for_region_match,
  gtk_css_selector_descendant_for_region_tree_match,
  gtk_css_selector_descendant_for_region_get_change,
  gtk_css_selector_descendant_for_region_tree_get_change,
  gtk_css_selector_descendant_for_region_hash_one,
  gtk_css_selector_descendant_for_region_compare_one,
  FALSE, FALSE, FALSE, FALSE, FALSE
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

static void
gtk_css_selector_child_tree_match (const GtkCssSelectorTree *tree,
				   const GtkCssMatcher  *matcher,
				   GHashTable *res)
{
  GtkCssMatcher parent;

  if (!_gtk_css_matcher_get_parent (&parent, matcher))
    return;

  gtk_css_selector_tree_match_previous (tree, &parent, res);
}


static GtkCssChange
gtk_css_selector_child_tree_get_change (const GtkCssSelectorTree *tree,
					const GtkCssMatcher  *matcher)
{
  GtkCssMatcher parent;
  GtkCssChange change, previous_change;

  if (!_gtk_css_matcher_get_parent (&parent, matcher))
    return 0;

  change = 0;

  previous_change = gtk_css_selector_tree_get_previous_change (tree, &parent);

  if (previous_change != 0)
    change |= _gtk_css_change_for_child (previous_change) | GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static GtkCssChange
gtk_css_selector_child_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_child (previous_change);
}

static guint
gtk_css_selector_child_hash_one (const GtkCssSelector *a)
{
  return 0;
}

static int
gtk_css_selector_child_compare_one (const GtkCssSelector *a,
				    const GtkCssSelector *b)
{
  return 0;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_CHILD = {
  "child",
  gtk_css_selector_child_print,
  gtk_css_selector_child_match,
  gtk_css_selector_child_tree_match,
  gtk_css_selector_child_get_change,
  gtk_css_selector_child_tree_get_change,
  gtk_css_selector_child_hash_one,
  gtk_css_selector_child_compare_one,
  FALSE, FALSE, FALSE, FALSE, FALSE
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

static void
gtk_css_selector_sibling_tree_match (const GtkCssSelectorTree *tree,
				     const GtkCssMatcher  *matcher,
				     GHashTable *res)
{
  GtkCssMatcher previous;

  while (_gtk_css_matcher_get_previous (&previous, matcher))
    {
      matcher = &previous;

      gtk_css_selector_tree_match_previous (tree, matcher, res);

      /* any matchers are dangerous here, as we may loop forever, but
	 we can terminate now as all possible matches have already been added */
      if (_gtk_css_matcher_matches_any (matcher))
	break;
    }
}

static GtkCssChange
gtk_css_selector_sibling_tree_get_change (const GtkCssSelectorTree *tree,
					  const GtkCssMatcher  *matcher)
{
  GtkCssMatcher previous;
  GtkCssChange change, previous_change;

  change = 0;

  previous_change = 0;
  while (_gtk_css_matcher_get_previous (&previous, matcher))
    {
      matcher = &previous;

      previous_change |= gtk_css_selector_tree_get_previous_change (tree, matcher);

      /* any matchers are dangerous here, as we may loop forever, but
	 we can terminate now as all possible matches have already been added */
      if (_gtk_css_matcher_matches_any (matcher))
	break;
    }

  if (previous_change != 0)
    change |= _gtk_css_change_for_sibling (previous_change) |  GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static GtkCssChange
gtk_css_selector_sibling_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_sibling (previous_change);
}

static guint
gtk_css_selector_sibling_hash_one (const GtkCssSelector *a)
{
  return 0;
}

static int
gtk_css_selector_sibling_compare_one (const GtkCssSelector *a,
				      const GtkCssSelector *b)
{
  return 0;
}
  

static const GtkCssSelectorClass GTK_CSS_SELECTOR_SIBLING = {
  "sibling",
  gtk_css_selector_sibling_print,
  gtk_css_selector_sibling_match,
  gtk_css_selector_sibling_tree_match,
  gtk_css_selector_sibling_get_change,
  gtk_css_selector_sibling_tree_get_change,
  gtk_css_selector_sibling_hash_one,
  gtk_css_selector_sibling_compare_one,
  FALSE, FALSE, FALSE, FALSE, FALSE
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

static void
gtk_css_selector_adjacent_tree_match (const GtkCssSelectorTree *tree,
				      const GtkCssMatcher  *matcher,
				      GHashTable *res)
{
  GtkCssMatcher previous;

  if (!_gtk_css_matcher_get_previous (&previous, matcher))
    return;

  matcher = &previous;

  gtk_css_selector_tree_match_previous (tree, matcher, res);
}

static GtkCssChange
gtk_css_selector_adjacent_tree_get_change (const GtkCssSelectorTree *tree,
					   const GtkCssMatcher  *matcher)
{
  GtkCssMatcher previous;
  GtkCssChange change, previous_change;

  if (!_gtk_css_matcher_get_previous (&previous, matcher))
    return 0;

  change = 0;

  previous_change = gtk_css_selector_tree_get_previous_change (tree, &previous);

  if (previous_change != 0)
    change |= _gtk_css_change_for_sibling (previous_change) |  GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static GtkCssChange
gtk_css_selector_adjacent_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_sibling (previous_change);
}

static guint
gtk_css_selector_adjacent_hash_one (const GtkCssSelector *a)
{
  return 0;
}

static int
gtk_css_selector_adjacent_compare_one (const GtkCssSelector *a,
				       const GtkCssSelector *b)
{
  return 0;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ADJACENT = {
  "adjacent",
  gtk_css_selector_adjacent_print,
  gtk_css_selector_adjacent_match,
  gtk_css_selector_adjacent_tree_match,
  gtk_css_selector_adjacent_get_change,
  gtk_css_selector_adjacent_tree_get_change,
  gtk_css_selector_adjacent_hash_one,
  gtk_css_selector_adjacent_compare_one,
  FALSE, FALSE, FALSE, FALSE, FALSE
};

/* SIMPLE SELECTOR DEFINE */

#define DEFINE_SIMPLE_SELECTOR(n, \
                               c, \
                               print_func, \
                               match_func, \
                               hash_func, \
                               comp_func, \
                               increase_id_specificity, \
                               increase_class_specificity, \
                               increase_element_specificity) \
static void \
gtk_css_selector_ ## n ## _print (const GtkCssSelector *selector, \
                                    GString              *string) \
{ \
  print_func (selector, string); \
} \
\
static void \
gtk_css_selector_not_ ## n ## _print (const GtkCssSelector *selector, \
                                      GString              *string) \
{ \
  g_string_append (string, ":not("); \
  print_func (selector, string); \
  g_string_append (string, ")"); \
} \
\
static gboolean \
gtk_css_selector_ ## n ## _match (const GtkCssSelector *selector, \
                                  const GtkCssMatcher  *matcher) \
{ \
  if (!match_func (selector, matcher)) \
    return FALSE; \
\
  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher); \
} \
\
static gboolean \
gtk_css_selector_not_ ## n ## _match (const GtkCssSelector *selector, \
                                      const GtkCssMatcher  *matcher) \
{ \
  if (match_func (selector, matcher)) \
    return FALSE; \
\
  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher); \
} \
\
static void \
gtk_css_selector_ ## n ## _tree_match (const GtkCssSelectorTree *tree, \
                                       const GtkCssMatcher      *matcher, \
                                       GHashTable               *res) \
{ \
  if (!match_func (&tree->selector, matcher)) \
    return; \
\
  gtk_css_selector_tree_found_match (tree, res); \
\
  gtk_css_selector_tree_match_previous (tree, matcher, res); \
} \
\
static void \
gtk_css_selector_not_ ## n ## _tree_match (const GtkCssSelectorTree *tree, \
                                           const GtkCssMatcher      *matcher, \
                                           GHashTable               *res) \
{ \
  if (match_func (&tree->selector, matcher)) \
    return; \
\
  gtk_css_selector_tree_found_match (tree, res); \
\
  gtk_css_selector_tree_match_previous (tree, matcher, res); \
} \
\
static GtkCssChange \
gtk_css_selector_ ## n ##_tree_get_change (const GtkCssSelectorTree *tree, \
			          	   const GtkCssMatcher      *matcher) \
{ \
  GtkCssChange change, previous_change; \
\
  if (!match_func (&tree->selector, matcher)) \
    return 0; \
\
  change = 0; \
\
  if (tree->matches_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET) \
    change |= GTK_CSS_CHANGE_ ## c | GTK_CSS_CHANGE_GOT_MATCH; \
\
  previous_change = gtk_css_selector_tree_get_previous_change (tree, matcher); \
\
  if (previous_change) \
    change |= previous_change | GTK_CSS_CHANGE_ ## c | GTK_CSS_CHANGE_GOT_MATCH; \
\
  return change; \
} \
\
static GtkCssChange \
gtk_css_selector_ ## n ## _get_change (const GtkCssSelector *selector, GtkCssChange previous_change) \
{ \
  return previous_change | GTK_CSS_CHANGE_ ## c; \
} \
\
static const GtkCssSelectorClass GTK_CSS_SELECTOR_ ## c = { \
  G_STRINGIFY(n), \
  gtk_css_selector_ ## n ## _print, \
  gtk_css_selector_ ## n ## _match, \
  gtk_css_selector_ ## n ## _tree_match, \
  gtk_css_selector_ ## n ## _get_change, \
  gtk_css_selector_ ## n ## _tree_get_change, \
  hash_func, \
  comp_func, \
  increase_id_specificity, increase_class_specificity, increase_element_specificity, \
  TRUE, FALSE \
};\
\
static const GtkCssSelectorClass GTK_CSS_SELECTOR_NOT_ ## c = { \
  "not_" G_STRINGIFY(n), \
  gtk_css_selector_not_ ## n ## _print, \
  gtk_css_selector_not_ ## n ## _match, \
  gtk_css_selector_not_ ## n ## _tree_match, \
  gtk_css_selector_ ## n ## _get_change, \
  gtk_css_selector_ ## n ## _tree_get_change, \
  hash_func, \
  comp_func, \
  increase_id_specificity, increase_class_specificity, increase_element_specificity, \
  TRUE, FALSE \
};

/* ANY */

static void
print_any (const GtkCssSelector *selector,
           GString              *string)
{
  g_string_append_c (string, '*');
}

static gboolean
match_any (const GtkCssSelector *selector,
           const GtkCssMatcher  *matcher)
{
  return TRUE;
}

static guint
hash_any (const GtkCssSelector *a)
{
  return 0;
}

static int
comp_any (const GtkCssSelector *a,
	  const GtkCssSelector *b)
{
  return 0;
}

#undef GTK_CSS_CHANGE_ANY
#define GTK_CSS_CHANGE_ANY 0
DEFINE_SIMPLE_SELECTOR(any, ANY, print_any, match_any, hash_any, comp_any, FALSE, FALSE, FALSE)
#undef GTK_CSS_CHANGE_ANY

/* NAME */

static GHashTable *type_refs_ht = NULL;
static guint type_refs_last_serial = 0;

static TypeReference *
get_type_reference (const char *name)
{
  TypeReference *ref;


  if (type_refs_ht == NULL)
    type_refs_ht = g_hash_table_new (g_str_hash, g_str_equal);

  ref = g_hash_table_lookup (type_refs_ht, name);

  if (ref != NULL)
    return ref;

  ref = g_slice_new (TypeReference);
  ref->name = g_intern_string (name);
  ref->type = g_type_from_name (ref->name);

  g_hash_table_insert (type_refs_ht,
		       (gpointer)ref->name, ref);

  return ref;
}

static void
update_type_references (void)
{
  GHashTableIter iter;
  guint serial;
  gpointer value;

  serial = g_type_get_type_registration_serial ();

  if (serial == type_refs_last_serial)
    return;

  type_refs_last_serial = serial;

  if (type_refs_ht == NULL)
    return;

  g_hash_table_iter_init (&iter, type_refs_ht);
  while (g_hash_table_iter_next (&iter,
				 NULL, &value))
    {
      TypeReference *ref = value;
      if (ref->type == G_TYPE_INVALID)
	ref->type = g_type_from_name (ref->name);
    }
}

static void
print_name (const GtkCssSelector *selector,
            GString              *string)
{
  g_string_append (string, selector->name.reference->name);
}

static gboolean
match_name (const GtkCssSelector *selector,
            const GtkCssMatcher  *matcher)
{
  return _gtk_css_matcher_has_type (matcher, selector->name.reference->type);
}

static guint
hash_name (const GtkCssSelector *a)
{
  return g_str_hash (a->name.reference->name);
}

static int
comp_name (const GtkCssSelector *a,
           const GtkCssSelector *b)
{
  return strcmp (a->name.reference->name,
		 b->name.reference->name);
}

DEFINE_SIMPLE_SELECTOR(name, NAME, print_name, match_name, hash_name, comp_name, FALSE, FALSE, TRUE)

/* REGION */

static void
gtk_css_selector_region_print (const GtkCssSelector *selector,
                               GString              *string)
{
  char *region_names[] = {
    "even",
    "odd",
    "first-child",
    "last-child",
    "only-child",
    "sorted"
  };
  guint i;

  g_string_append (string, selector->region.name);

  for (i = 0; i < G_N_ELEMENTS (region_names); i++)
    {
      if (selector->region.flags & (1 << i))
        {
          g_string_append_c (string, ':');
          g_string_append (string, region_names[i]);
        }
    }
}

static gboolean
gtk_css_selector_region_match (const GtkCssSelector *selector,
                               const GtkCssMatcher  *matcher)
{
  if (!_gtk_css_matcher_has_region (matcher, selector->region.name, selector->region.flags))
    return FALSE;

  return gtk_css_selector_match (gtk_css_selector_previous (selector), matcher);
}

static void
gtk_css_selector_region_tree_match (const GtkCssSelectorTree *tree,
				    const GtkCssMatcher  *matcher,
				    GHashTable *res)
{
  if (!_gtk_css_matcher_has_region (matcher, tree->selector.region.name, tree->selector.region.flags))
    return;

  gtk_css_selector_tree_found_match (tree, res);

  gtk_css_selector_tree_match_previous (tree, matcher, res);
}

static GtkCssChange
gtk_css_selector_region_tree_get_change (const GtkCssSelectorTree *tree,
					 const GtkCssMatcher  *matcher)
{
  GtkCssChange change, previous_change;

  if (!_gtk_css_matcher_has_region (matcher, tree->selector.region.name, tree->selector.region.flags))
    return 0;

  change = 0;

  if (tree->matches_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
    change |= GTK_CSS_CHANGE_REGION | GTK_CSS_CHANGE_GOT_MATCH;

  previous_change = gtk_css_selector_tree_get_previous_change (tree, matcher);

  if (previous_change != 0)
    change |= previous_change | GTK_CSS_CHANGE_REGION | GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static GtkCssChange
gtk_css_selector_region_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return previous_change | GTK_CSS_CHANGE_REGION;
}

static guint
gtk_css_selector_region_hash_one (const GtkCssSelector *a)
{
  return g_str_hash (a->region.name) ^ a->region.flags;
}

static int
gtk_css_selector_region_compare_one (const GtkCssSelector *a,
				     const GtkCssSelector *b)
{
  int diff;

  diff = strcmp (a->region.name, b->region.name);
  if (diff)
    return diff;

  return a->region.flags - b->region.flags;
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_REGION = {
  "region",
  gtk_css_selector_region_print,
  gtk_css_selector_region_match,
  gtk_css_selector_region_tree_match,
  gtk_css_selector_region_get_change,
  gtk_css_selector_region_tree_get_change,
  gtk_css_selector_region_hash_one,
  gtk_css_selector_region_compare_one,
  FALSE, FALSE, TRUE, TRUE, FALSE
};

/* CLASS */

static void
print_class (const GtkCssSelector *selector,
             GString              *string)
{
  g_string_append_c (string, '.');
  g_string_append (string, g_quark_to_string (selector->style_class.style_class));
}

static gboolean
match_class (const GtkCssSelector *selector,
             const GtkCssMatcher  *matcher)
{
  return _gtk_css_matcher_has_class (matcher, selector->style_class.style_class);
}

static guint
hash_class (const GtkCssSelector *a)
{
  return g_str_hash (g_quark_to_string (a->style_class.style_class));
}

static int
comp_class (const GtkCssSelector *a,
            const GtkCssSelector *b)
{
  return strcmp (g_quark_to_string (a->style_class.style_class),
		 g_quark_to_string (b->style_class.style_class));
}

DEFINE_SIMPLE_SELECTOR(class, CLASS, print_class, match_class, hash_class, comp_class, FALSE, TRUE, FALSE)

/* ID */

static void
print_id (const GtkCssSelector *selector,
          GString              *string)
{
  g_string_append_c (string, '#');
  g_string_append (string, selector->id.name);
}

static gboolean
match_id (const GtkCssSelector *selector,
          const GtkCssMatcher  *matcher)
{
  return _gtk_css_matcher_has_id (matcher, selector->id.name);
}

static guint
hash_id (const GtkCssSelector *a)
{
  return g_str_hash (a->id.name);
}

static int
comp_id (const GtkCssSelector *a,
	 const GtkCssSelector *b)
{
  return strcmp (a->id.name, b->id.name);
}

DEFINE_SIMPLE_SELECTOR(id, ID, print_id, match_id, hash_id, comp_id, TRUE, FALSE, FALSE)

/* PSEUDOCLASS FOR STATE */

static void
print_pseudoclass_state (const GtkCssSelector *selector,
                         GString              *string)
{
  static const char * state_names[] = {
    "active",
    "hover",
    "selected",
    "insensitive",
    "inconsistent",
    "focus",
    "backdrop",
    "dir(ltr)",
    "dir(rtl)",
    "link",
    "visited",
    "checked"
  };
  guint i;

  g_string_append_c (string, ':');

  for (i = 0; i < G_N_ELEMENTS (state_names); i++)
    {
      if (selector->state.state == (1 << i))
        {
          g_string_append (string, state_names[i]);
          return;
        }
    }

  g_assert_not_reached ();
}

static gboolean
match_pseudoclass_state (const GtkCssSelector *selector,
                         const GtkCssMatcher  *matcher)
{
  return (_gtk_css_matcher_get_state (matcher) & selector->state.state) == selector->state.state;
}

static guint
hash_pseudoclass_state (const GtkCssSelector *selector)
{
  return selector->state.state;
}


static int
comp_pseudoclass_state (const GtkCssSelector *a,
		        const GtkCssSelector *b)
{
  return a->state.state - b->state.state;
}

#define GTK_CSS_CHANGE_PSEUDOCLASS_STATE GTK_CSS_CHANGE_STATE
DEFINE_SIMPLE_SELECTOR(pseudoclass_state, PSEUDOCLASS_STATE, print_pseudoclass_state,
                       match_pseudoclass_state, hash_pseudoclass_state, comp_pseudoclass_state,
                       FALSE, TRUE, FALSE)
#undef GTK_CSS_CHANGE_PSEUDOCLASS_STATE

/* PSEUDOCLASS FOR POSITION */

static void
print_pseudoclass_position (const GtkCssSelector *selector,
                            GString              *string)
{
  switch (selector->position.type)
    {
    case POSITION_FORWARD:
      if (selector->position.a == 0)
        {
          if (selector->position.b == 1)
            g_string_append (string, ":first-child");
          else
            g_string_append_printf (string, ":nth-child(%d)", selector->position.b);
        }
      else if (selector->position.a == 2 && selector->position.b == 0)
        g_string_append (string, ":nth-child(even)");
      else if (selector->position.a == 2 && selector->position.b == 1)
        g_string_append (string, ":nth-child(odd)");
      else
        {
          g_string_append (string, ":nth-child(");
          if (selector->position.a == 1)
            g_string_append (string, "n");
          else if (selector->position.a == -1)
            g_string_append (string, "-n");
          else
            g_string_append_printf (string, "%dn", selector->position.a);
          if (selector->position.b > 0)
            g_string_append_printf (string, "+%d)", selector->position.b);
          else if (selector->position.b < 0)
            g_string_append_printf (string, "%d)", selector->position.b);
          else
            g_string_append (string, ")");
        }
      break;
    case POSITION_BACKWARD:
      if (selector->position.a == 0)
        {
          if (selector->position.b == 1)
            g_string_append (string, ":last-child");
          else
            g_string_append_printf (string, ":nth-last-child(%d)", selector->position.b);
        }
      else if (selector->position.a == 2 && selector->position.b == 0)
        g_string_append (string, ":nth-last-child(even)");
      else if (selector->position.a == 2 && selector->position.b == 1)
        g_string_append (string, ":nth-last-child(odd)");
      else
        {
          g_string_append (string, ":nth-last-child(");
          if (selector->position.a == 1)
            g_string_append (string, "n");
          else if (selector->position.a == -1)
            g_string_append (string, "-n");
          else
            g_string_append_printf (string, "%dn", selector->position.a);
          if (selector->position.b > 0)
            g_string_append_printf (string, "+%d)", selector->position.b);
          else if (selector->position.b < 0)
            g_string_append_printf (string, "%d)", selector->position.b);
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
match_pseudoclass_position (const GtkCssSelector *selector,
		            const GtkCssMatcher  *matcher)
{
  switch (selector->position.type)
    {
    case POSITION_FORWARD:
      if (!_gtk_css_matcher_has_position (matcher, TRUE, selector->position.a, selector->position.b))
        return FALSE;
      break;
    case POSITION_BACKWARD:
      if (!_gtk_css_matcher_has_position (matcher, FALSE, selector->position.a, selector->position.b))
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

  return TRUE;
}

static guint
hash_pseudoclass_position (const GtkCssSelector *a)
{
  return (((a->position.type << POSITION_NUMBER_BITS) | a->position.a) << POSITION_NUMBER_BITS) | a->position.b;
}

static int
comp_pseudoclass_position (const GtkCssSelector *a,
			   const GtkCssSelector *b)
{
  int diff;
  
  diff = a->position.type - b->position.type;
  if (diff)
    return diff;

  diff = a->position.a - b->position.a;
  if (diff)
    return diff;

  return a->position.b - b->position.b;
}

#define GTK_CSS_CHANGE_PSEUDOCLASS_POSITION GTK_CSS_CHANGE_POSITION
DEFINE_SIMPLE_SELECTOR(pseudoclass_position, PSEUDOCLASS_POSITION, print_pseudoclass_position,
                       match_pseudoclass_position, hash_pseudoclass_position, comp_pseudoclass_position,
                       FALSE, TRUE, FALSE)
#undef GTK_CSS_CHANGE_PSEUDOCLASS_POSITION
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
                      GtkCssSelector            *selector)
{
  guint size;

  size = gtk_css_selector_size (selector);
  selector = g_realloc (selector, sizeof (GtkCssSelector) * (size + 1) + sizeof (gpointer));
  if (size == 0)
    selector[1].class = NULL;
  else
    memmove (selector + 1, selector, sizeof (GtkCssSelector) * size + sizeof (gpointer));

  memset (selector, 0, sizeof (GtkCssSelector));
  selector->class = class;

  return selector;
}

static GtkCssSelector *
parse_selector_class (GtkCssParser   *parser,
                      GtkCssSelector *selector,
                      gboolean        negate)
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

  selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_CLASS
                                          : &GTK_CSS_SELECTOR_CLASS,
                                   selector);
  selector->style_class.style_class = g_quark_from_string (name);

  g_free (name);

  return selector;
}

static GtkCssSelector *
parse_selector_id (GtkCssParser   *parser,
                   GtkCssSelector *selector,
                   gboolean        negate)
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

  selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_ID
                                          : &GTK_CSS_SELECTOR_ID,
                                   selector);
  selector->id.name = g_intern_string (name);

  g_free (name);

  return selector;
}

static GtkCssSelector *
parse_selector_pseudo_class_nth_child (GtkCssParser   *parser,
                                       GtkCssSelector *selector,
                                       PositionType    type,
                                       gboolean        negate)
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

  selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_POSITION
                                          : &GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                   selector);
  selector->position.type = type;
  selector->position.a = a;
  selector->position.b = b;

  return selector;
}

static GtkRegionFlags
try_parse_selector_region_pseudo_class (GtkCssParser *parser)
{
  static const struct {
    const char     *name;
    GtkRegionFlags  flags;
  } region_flags[] = {
    { "even",           GTK_REGION_EVEN },
    { "odd",            GTK_REGION_ODD },
    { "first-child",    GTK_REGION_FIRST },
    { "last-child",     GTK_REGION_LAST },
    { "only-child",     GTK_REGION_ONLY },
    { "sorted",         GTK_REGION_SORTED }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (region_flags); i++)
    {
      if (_gtk_css_parser_try (parser, region_flags[i].name, FALSE))
        return region_flags[i].flags;
    }

  return 0;
}

static GtkCssSelector *
parse_selector_pseudo_class (GtkCssParser   *parser,
                             GtkCssSelector *selector,
                             gboolean        negate)
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
    { "backdrop",     GTK_STATE_FLAG_BACKDROP, },
    { "dir(ltr)",     GTK_STATE_FLAG_DIR_LTR, },
    { "dir(rtl)",     GTK_STATE_FLAG_DIR_RTL, },
    { "link",         GTK_STATE_FLAG_LINK, },
    { "visited",      GTK_STATE_FLAG_VISITED, },
    { "checked",      GTK_STATE_FLAG_CHECKED, }
  };
  guint i;

  if (_gtk_css_parser_try (parser, "nth-child", FALSE))
    return parse_selector_pseudo_class_nth_child (parser, selector, POSITION_FORWARD, negate);
  else if (_gtk_css_parser_try (parser, "nth-last-child", FALSE))
    return parse_selector_pseudo_class_nth_child (parser, selector, POSITION_BACKWARD, negate);

  for (i = 0; i < G_N_ELEMENTS (pseudo_classes); i++)
    {
      if (_gtk_css_parser_try (parser, pseudo_classes[i].name, FALSE))
        {
          if (pseudo_classes[i].state_flag)
            {
              selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_STATE
                                                      : &GTK_CSS_SELECTOR_PSEUDOCLASS_STATE,
                                               selector);
              selector->state.state = pseudo_classes[i].state_flag;
            }
          else
            {
              selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_POSITION
                                                      : &GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                               selector);
              selector->position.type = pseudo_classes[i].position_type;
              selector->position.a = pseudo_classes[i].position_a;
              selector->position.b = pseudo_classes[i].position_b;
            }
          return selector;
        }
    }
      
  _gtk_css_parser_error (parser, "Missing name of pseudo-class");
  if (selector)
    _gtk_css_selector_free (selector);
  return NULL;
}

static GtkCssSelector *
parse_selector_negation (GtkCssParser   *parser,
                         GtkCssSelector *selector)
{
  char *name;

  name = _gtk_css_parser_try_ident (parser, FALSE);
  if (name)
    {
      selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NOT_NAME,
                                       selector);
      selector->name.reference = get_type_reference (name);
      g_free (name);
    }
  else if (_gtk_css_parser_try (parser, "*", FALSE))
    selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NOT_ANY, selector);
  else if (_gtk_css_parser_try (parser, "#", FALSE))
    selector = parse_selector_id (parser, selector, TRUE);
  else if (_gtk_css_parser_try (parser, ".", FALSE))
    selector = parse_selector_class (parser, selector, TRUE);
  else if (_gtk_css_parser_try (parser, ":", FALSE))
    selector = parse_selector_pseudo_class (parser, selector, TRUE);
  else
    {
      _gtk_css_parser_error (parser, "Not a valid selector for :not()");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }
  
  _gtk_css_parser_skip_whitespace (parser);

  if (!_gtk_css_parser_try (parser, ")", FALSE))
    {
      _gtk_css_parser_error (parser, "Missing closing bracket for :not()");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  return selector;
}

static GtkCssSelector *
parse_simple_selector (GtkCssParser   *parser,
                       GtkCssSelector *selector)
{
  gboolean parsed_something = FALSE;
  guint region_offset = 0; 
  char *name;

  name = _gtk_css_parser_try_ident (parser, FALSE);
  if (name)
    {
      if (_gtk_style_context_check_region_name (name))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_REGION,
                                           selector);
          selector->region.name = g_intern_string (name);
          region_offset = gtk_css_selector_size (selector);
        }
      else
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NAME,
                                           selector);
	  selector->name.reference = get_type_reference (name);
        }
      g_free (name);
      parsed_something = TRUE;
    }
  else if (_gtk_css_parser_try (parser, "*", FALSE))
    {
      selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ANY, selector);
      parsed_something = TRUE;
    }

  do {
      if (_gtk_css_parser_try (parser, "#", FALSE))
        selector = parse_selector_id (parser, selector, FALSE);
      else if (_gtk_css_parser_try (parser, ".", FALSE))
        selector = parse_selector_class (parser, selector, FALSE);
      else if (_gtk_css_parser_try (parser, ":not(", TRUE))
        selector = parse_selector_negation (parser, selector);
      else if (_gtk_css_parser_try (parser, ":", FALSE))
        {
          GtkRegionFlags region_flags;
          if (region_offset &&
              (region_flags = try_parse_selector_region_pseudo_class (parser)))
            {
              selector[gtk_css_selector_size (selector) - region_offset].region.flags |= region_flags;
            }
          else
            {
              selector = parse_selector_pseudo_class (parser, selector, FALSE);
            }
        }
      else if (!parsed_something)
        {
          _gtk_css_parser_error (parser, "Expected a valid selector");
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }
      else
        break;

      parsed_something = TRUE;
    }
  while (selector && !_gtk_css_parser_is_eof (parser));

  _gtk_css_parser_skip_whitespace (parser);

  /* This is the big region hack where we change the descendant matcher
   * to a version that respects regions.
   */
  if (selector)
    {
      if ((selector[0].class == &GTK_CSS_SELECTOR_ANY || selector[0].class == &GTK_CSS_SELECTOR_REGION)
          && selector[1].class == &GTK_CSS_SELECTOR_DESCENDANT)
        selector[1].class = &GTK_CSS_SELECTOR_DESCENDANT_FOR_REGION;
    }

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
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ADJACENT, selector);
      else if (_gtk_css_parser_try (parser, "~", TRUE))
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_SIBLING, selector);
      else if (_gtk_css_parser_try (parser, ">", TRUE))
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_CHILD, selector);
      else
        selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_DESCENDANT, selector);
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
_gtk_css_selector_tree_match_get_change (const GtkCssSelectorTree *tree)
{
  GtkCssChange change = 0;

  update_type_references ();

  while (tree)
    {
      change = tree->selector.class->get_change (&tree->selector, change);
      tree = gtk_css_selector_tree_get_parent (tree);
    }

  return change;
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

  update_type_references ();

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


/******************** SelectorTree handling *****************/

static GHashTable *
gtk_css_selectors_count_initial_init (void)
{
  return g_hash_table_new ((GHashFunc)gtk_css_selector_hash_one, (GEqualFunc)gtk_css_selector_equal);
}

static void
gtk_css_selectors_count_initial (const GtkCssSelector *selector, GHashTable *hash_one)
{
  if (!selector->class->is_simple || selector->class->must_keep_order)
    {
      guint count = GPOINTER_TO_INT (g_hash_table_lookup (hash_one, selector));
      g_hash_table_replace (hash_one, (gpointer)selector, GUINT_TO_POINTER (count + 1));
      return;
    }

  for (;
       selector && selector->class->is_simple && !selector->class->must_keep_order;
       selector = gtk_css_selector_previous (selector))
    {
      guint count = GPOINTER_TO_INT (g_hash_table_lookup (hash_one, selector));
      g_hash_table_replace (hash_one, (gpointer)selector, GUINT_TO_POINTER (count + 1));
    }
}

static gboolean
gtk_css_selectors_has_initial_selector (const GtkCssSelector *selector, const GtkCssSelector *initial)
{
  if (!selector->class->is_simple || selector->class->must_keep_order)
    return gtk_css_selector_equal (selector, initial);

  for (;
       selector && selector->class->is_simple && !selector->class->must_keep_order;
       selector = gtk_css_selector_previous (selector))
    {
      if (gtk_css_selector_equal (selector, initial))
	return TRUE;
    }

  return FALSE;
}

static GtkCssSelector *
gtk_css_selectors_skip_initial_selector (GtkCssSelector *selector, const GtkCssSelector *initial)
{
  GtkCssSelector *found;
  GtkCssSelector tmp;

  /* If the initial simple selector is not first, move it there so we can skip it
     without losing any other selectors */
  if (!gtk_css_selector_equal (selector, initial))
    {
      for (found = selector; found && found->class->is_simple; found = (GtkCssSelector *)gtk_css_selector_previous (found))
	{
	  if (gtk_css_selector_equal (found, initial))
	    break;
	}

      g_assert (found != NULL && found->class->is_simple);

      tmp = *found;
      *found = *selector;
      *selector = tmp;
    }

  return (GtkCssSelector *)gtk_css_selector_previous (selector);
}

static int
direct_ptr_compare (const void *_a, const void *_b)
{
  gpointer *a = (gpointer *)_a;
  gpointer *b = (gpointer *)_b;
  if (*a < *b)
    return -1;
  else if (*a == *b)
    return 0;
  return 1;
}

GPtrArray *
_gtk_css_selector_tree_match_all (const GtkCssSelectorTree *tree,
				  const GtkCssMatcher *matcher)
{
  GHashTable *res;
  GPtrArray *array;
  GHashTableIter iter;
  gpointer key;

  update_type_references ();

  res = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (; tree != NULL;
       tree = gtk_css_selector_tree_get_sibling (tree))
    gtk_css_selector_tree_match (tree, matcher, res);

  array = g_ptr_array_sized_new (g_hash_table_size (res));

  g_hash_table_iter_init (&iter, res);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_ptr_array_add (array, key);

  g_hash_table_destroy (res);

  qsort (array->pdata, array->len, sizeof (gpointer), direct_ptr_compare);

  return array;
}

GtkCssChange
_gtk_css_selector_tree_get_change_all (const GtkCssSelectorTree *tree,
				       const GtkCssMatcher *matcher)
{
  GtkCssChange change;

  change = 0;

  for (; tree != NULL;
       tree = gtk_css_selector_tree_get_sibling (tree))
    change |= gtk_css_selector_tree_get_change (tree, matcher);

  /* Never return reserved bit set */
  return change & ~GTK_CSS_CHANGE_RESERVED_BIT;
}

#ifdef PRINT_TREE
static void
_gtk_css_selector_tree_print (const GtkCssSelectorTree *tree, GString *str, char *prefix)
{
  gboolean first = TRUE;
  int len, i;

  for (; tree != NULL; tree = gtk_css_selector_tree_get_sibling (tree), first = FALSE)
    {
      if (!first)
	g_string_append (str, prefix);

      if (first)
	{
	  if (gtk_css_selector_tree_get_sibling (tree))
	    g_string_append (str, "─┬─");
	  else
	    g_string_append (str, "───");
	}
      else
	{
	  if (gtk_css_selector_tree_get_sibling (tree))
	    g_string_append (str, " ├─");
	  else
	    g_string_append (str, " └─");
	}

      len = str->len;
      tree->selector.class->print (&tree->selector, str);
      len = str->len - len;

      if (gtk_css_selector_tree_get_previous (tree))
	{
	  GString *prefix2 = g_string_new (prefix);

	  if (gtk_css_selector_tree_get_sibling (tree))
	    g_string_append (prefix2, " │ ");
	  else
	    g_string_append (prefix2, "   ");
	  for (i = 0; i < len; i++)
	    g_string_append_c (prefix2, ' ');

	  _gtk_css_selector_tree_print (gtk_css_selector_tree_get_previous (tree), str, prefix2->str);
	  g_string_free (prefix2, TRUE);
	}
      else
	g_string_append (str, "\n");
    }
}
#endif

void
_gtk_css_selector_tree_match_print (const GtkCssSelectorTree *tree,
				    GString *str)
{
  const GtkCssSelectorTree *parent;

  g_return_if_fail (tree != NULL);

  tree->selector.class->print (&tree->selector, str);

  parent = gtk_css_selector_tree_get_parent (tree);
  if (parent != NULL)
    _gtk_css_selector_tree_match_print (parent, str);
}

void
_gtk_css_selector_tree_free (GtkCssSelectorTree *tree)
{
  if (tree == NULL)
    return;

  g_free (tree);
}


typedef struct {
  gpointer match;
  GtkCssSelector *current_selector;
  GtkCssSelectorTree **selector_match;
} GtkCssSelectorRuleSetInfo;

static GtkCssSelectorTree *
get_tree (GByteArray *array, gint32 offset)
{
  return (GtkCssSelectorTree *) (array->data + offset);
}

static GtkCssSelectorTree *
alloc_tree (GByteArray *array, gint32 *offset)
{
  GtkCssSelectorTree tree = { { NULL} };

  *offset = array->len;
  g_byte_array_append (array, (guint8 *)&tree, sizeof (GtkCssSelectorTree));
  return get_tree (array, *offset);
}

static gint32
subdivide_infos (GByteArray *array, GList *infos, gint32 parent_offset)
{
  GHashTable *ht;
  GList *l;
  GList *matched;
  GList *remaining;
  gint32 tree_offset;
  GtkCssSelectorTree *tree;
  GtkCssSelectorRuleSetInfo *info;
  GtkCssSelector max_selector;
  GHashTableIter iter;
  guint max_count;
  gpointer key, value;
  GPtrArray *exact_matches;
  gint32 res;

  if (infos == NULL)
    return GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET;

  ht = gtk_css_selectors_count_initial_init ();

  for (l = infos; l != NULL; l = l->next)
    {
      info = l->data;
      gtk_css_selectors_count_initial (info->current_selector, ht);
    }

  /* Pick the selector with highest count, and use as decision on this level
     as that makes it possible to skip the largest amount of checks later */

  max_count = 0;

  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkCssSelector *selector = key;
      if (GPOINTER_TO_UINT (value) > max_count ||
	  (GPOINTER_TO_UINT (value) == max_count &&
	  gtk_css_selector_compare_one (selector, &max_selector) < 0))
	{
	  max_count = GPOINTER_TO_UINT (value);
	  max_selector = *selector;
	}
    }

  matched = NULL;
  remaining = NULL;

  tree = alloc_tree (array, &tree_offset);
  tree->parent_offset = parent_offset;
  tree->selector = max_selector;

  exact_matches = NULL;
  for (l = infos; l != NULL; l = l->next)
    {
      info = l->data;

      if (gtk_css_selectors_has_initial_selector (info->current_selector, &max_selector))
	{
	  info->current_selector = gtk_css_selectors_skip_initial_selector (info->current_selector, &max_selector);
	  if (info->current_selector == NULL)
	    {
	      /* Matches current node */
	      if (exact_matches == NULL)
		exact_matches = g_ptr_array_new ();
	      g_ptr_array_add (exact_matches, info->match);
	      if (info->selector_match != NULL)
		*info->selector_match = GUINT_TO_POINTER (tree_offset);
	    }
	  else
	    matched = g_list_prepend (matched, info);
	}
      else
	{
	  remaining = g_list_prepend (remaining, info);
	}
    }

  if (exact_matches)
    {
      g_ptr_array_add (exact_matches, NULL); /* Null terminate */
      res = array->len;
      g_byte_array_append (array, (guint8 *)exact_matches->pdata,
			   exact_matches->len * sizeof (gpointer));
      g_ptr_array_free (exact_matches, TRUE);
    }
  else
    res = GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET;
  get_tree (array, tree_offset)->matches_offset = res;

  res = subdivide_infos (array, matched, tree_offset);
  get_tree (array, tree_offset)->previous_offset = res;

  res = subdivide_infos (array, remaining, parent_offset);
  get_tree (array, tree_offset)->sibling_offset = res;

  g_list_free (matched);
  g_list_free (remaining);
  g_hash_table_unref (ht);

  return tree_offset;
}

struct _GtkCssSelectorTreeBuilder {
  GList  *infos;
};

GtkCssSelectorTreeBuilder *
_gtk_css_selector_tree_builder_new (void)
{
  return g_new0 (GtkCssSelectorTreeBuilder, 1);
}

void
_gtk_css_selector_tree_builder_free  (GtkCssSelectorTreeBuilder *builder)
{
  g_list_free_full (builder->infos, g_free);
  g_free (builder);
}

void
_gtk_css_selector_tree_builder_add (GtkCssSelectorTreeBuilder *builder,
				    GtkCssSelector            *selectors,
				    GtkCssSelectorTree       **selector_match,
				    gpointer                   match)
{
  GtkCssSelectorRuleSetInfo *info = g_new0 (GtkCssSelectorRuleSetInfo, 1);

  info->match = match;
  info->current_selector = selectors;
  info->selector_match = selector_match;
  builder->infos = g_list_prepend (builder->infos, info);
}

/* Convert all offsets to node-relative */
static void
fixup_offsets (GtkCssSelectorTree *tree, guint8 *data)
{
  while (tree != NULL)
    {
      if (tree->parent_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->parent_offset -= ((guint8 *)tree - data);

      if (tree->previous_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->previous_offset -= ((guint8 *)tree - data);

      if (tree->sibling_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->sibling_offset -= ((guint8 *)tree - data);

      if (tree->matches_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->matches_offset -= ((guint8 *)tree - data);

      fixup_offsets ((GtkCssSelectorTree *)gtk_css_selector_tree_get_previous (tree), data);

      tree = (GtkCssSelectorTree *)gtk_css_selector_tree_get_sibling (tree);
    }
}

GtkCssSelectorTree *
_gtk_css_selector_tree_builder_build (GtkCssSelectorTreeBuilder *builder)
{
  GtkCssSelectorTree *tree;
  GByteArray *array;
  guint8 *data;
  guint len;
  GList *l;
  GtkCssSelectorRuleSetInfo *info;

  array = g_byte_array_new ();
  subdivide_infos (array, builder->infos, GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET);

  len = array->len;
  data = g_byte_array_free (array, FALSE);

  /* shrink to final size */
  data = g_realloc (data, len);

  tree = (GtkCssSelectorTree *)data;

  fixup_offsets (tree, data);

  /* Convert offsets to final pointers */
  for (l = builder->infos; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->selector_match)
	*info->selector_match = (GtkCssSelectorTree *)(data + GPOINTER_TO_UINT (*info->selector_match));
    }

#ifdef PRINT_TREE
  {
    GString *s = g_string_new ("");
    _gtk_css_selector_tree_print (tree, s, "");
    g_print ("%s", s->str);
    g_string_free (s, TRUE);
  }
#endif

  return tree;
}
