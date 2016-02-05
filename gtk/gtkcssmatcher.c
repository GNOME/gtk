/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssmatcherprivate.h"

#include "gtkcssnodedeclarationprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetpath.h"

/* GTK_CSS_MATCHER_WIDGET_PATH */

static gboolean
gtk_css_matcher_widget_path_get_parent (GtkCssMatcher       *matcher,
                                        const GtkCssMatcher *child)
{
  if (child->path.index == 0)
    return FALSE;

  matcher->path.klass = child->path.klass;
  matcher->path.decl = NULL;
  matcher->path.path = child->path.path;
  matcher->path.index = child->path.index - 1;
  matcher->path.sibling_index = gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index);

  return TRUE;
}

static gboolean
gtk_css_matcher_widget_path_get_previous (GtkCssMatcher       *matcher,
                                          const GtkCssMatcher *next)
{
  if (next->path.sibling_index == 0)
    return FALSE;

  matcher->path.klass = next->path.klass;
  matcher->path.decl = NULL;
  matcher->path.path = next->path.path;
  matcher->path.index = next->path.index;
  matcher->path.sibling_index = next->path.sibling_index - 1;

  return TRUE;
}

static GtkStateFlags
gtk_css_matcher_widget_path_get_state (const GtkCssMatcher *matcher)
{
  const GtkWidgetPath *siblings;
  
  if (matcher->path.decl)
    return gtk_css_node_declaration_get_state (matcher->path.decl);

  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    return gtk_widget_path_iter_get_state (siblings, matcher->path.sibling_index);
  else
    return gtk_widget_path_iter_get_state (matcher->path.path, matcher->path.index);
}

static gboolean
gtk_css_matcher_widget_path_has_name (const GtkCssMatcher     *matcher,
                                      /*interned*/ const char *name)
{
  const GtkWidgetPath *siblings;

  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    {
      const char *path_name = gtk_widget_path_iter_get_object_name (siblings, matcher->path.sibling_index);

      if (path_name == NULL)
        path_name = g_type_name (gtk_widget_path_iter_get_object_type (siblings, matcher->path.sibling_index));

      return path_name == name;
    }
  else
    {
      const char *path_name = gtk_widget_path_iter_get_object_name (matcher->path.path, matcher->path.index);

      if (path_name == NULL)
        path_name = g_type_name (gtk_widget_path_iter_get_object_type (matcher->path.path, matcher->path.index));

      return path_name == name;
    }
}

static gboolean
gtk_css_matcher_widget_path_has_class (const GtkCssMatcher *matcher,
                                       GQuark               class_name)
{
  const GtkWidgetPath *siblings;
  
  if (matcher->path.decl &&
      gtk_css_node_declaration_has_class (matcher->path.decl, class_name))
    return TRUE;

  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    return gtk_widget_path_iter_has_qclass (siblings, matcher->path.sibling_index, class_name);
  else
    return gtk_widget_path_iter_has_qclass (matcher->path.path, matcher->path.index, class_name);
}

static gboolean
gtk_css_matcher_widget_path_has_id (const GtkCssMatcher *matcher,
                                    const char          *id)
{
  const GtkWidgetPath *siblings;
  
  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    return gtk_widget_path_iter_has_name (siblings, matcher->path.sibling_index, id);
  else
    return gtk_widget_path_iter_has_name (matcher->path.path, matcher->path.index, id);
}

static gboolean
gtk_css_matcher_widget_path_has_position (const GtkCssMatcher *matcher,
                                          gboolean             forward,
                                          int                  a,
                                          int                  b)
{
  const GtkWidgetPath *siblings;
  int x;

  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (!siblings)
    return FALSE;

  if (forward)
    x = matcher->path.sibling_index + 1;
  else
    x = gtk_widget_path_length (siblings) - matcher->path.sibling_index;

  x -= b;

  if (a == 0)
    return x == 0;

  if (x % a)
    return FALSE;

  return x / a >= 0;
}

static const GtkCssMatcherClass GTK_CSS_MATCHER_WIDGET_PATH = {
  gtk_css_matcher_widget_path_get_parent,
  gtk_css_matcher_widget_path_get_previous,
  gtk_css_matcher_widget_path_get_state,
  gtk_css_matcher_widget_path_has_name,
  gtk_css_matcher_widget_path_has_class,
  gtk_css_matcher_widget_path_has_id,
  gtk_css_matcher_widget_path_has_position,
  FALSE
};

gboolean
_gtk_css_matcher_init (GtkCssMatcher               *matcher,
                       const GtkWidgetPath         *path,
                       const GtkCssNodeDeclaration *decl)
{
  if (gtk_widget_path_length (path) == 0)
    return FALSE;

  matcher->path.klass = &GTK_CSS_MATCHER_WIDGET_PATH;
  matcher->path.decl = decl;
  matcher->path.path = path;
  matcher->path.index = gtk_widget_path_length (path) - 1;
  matcher->path.sibling_index = gtk_widget_path_iter_get_sibling_index (path, matcher->path.index);

  return TRUE;
}

/* GTK_CSS_MATCHER_NODE */

static gboolean
gtk_css_matcher_node_get_parent (GtkCssMatcher       *matcher,
                                 const GtkCssMatcher *child)
{
  GtkCssNode *node;
  
  node = gtk_css_node_get_parent (child->node.node);
  if (node == NULL)
    return FALSE;

  return gtk_css_node_init_matcher (node, matcher);
}

static GtkCssNode *
get_previous_visible_sibling (GtkCssNode *node)
{
  do {
    node = gtk_css_node_get_previous_sibling (node);
  } while (node && !gtk_css_node_get_visible (node));

  return node;
}

static GtkCssNode *
get_next_visible_sibling (GtkCssNode *node)
{
  do {
    node = gtk_css_node_get_next_sibling (node);
  } while (node && !gtk_css_node_get_visible (node));

  return node;
}

static gboolean
gtk_css_matcher_node_get_previous (GtkCssMatcher       *matcher,
                                   const GtkCssMatcher *next)
{
  GtkCssNode *node;
  
  node = get_previous_visible_sibling (next->node.node);
  if (node == NULL)
    return FALSE;

  return gtk_css_node_init_matcher (node, matcher);
}

static GtkStateFlags
gtk_css_matcher_node_get_state (const GtkCssMatcher *matcher)
{
  return gtk_css_node_get_state (matcher->node.node);
}

static gboolean
gtk_css_matcher_node_has_name (const GtkCssMatcher     *matcher,
                               /*interned*/ const char *name)
{
  return gtk_css_node_get_name (matcher->node.node) == name;
}

static gboolean
gtk_css_matcher_node_has_class (const GtkCssMatcher *matcher,
                                GQuark               class_name)
{
  return gtk_css_node_has_class (matcher->node.node, class_name);
}

static gboolean
gtk_css_matcher_node_has_id (const GtkCssMatcher *matcher,
                             const char          *id)
{
  /* assume all callers pass an interned string */
  return gtk_css_node_get_id (matcher->node.node) == id;
}

static gboolean
gtk_css_matcher_node_nth_child (GtkCssNode *node,
                                GtkCssNode *(* prev_node_func) (GtkCssNode *),
                                int         a,
                                int         b)
{
  int pos, x;

  /* special-case the common "first-child" and "last-child" */
  if (a == 0)
    {
      while (b > 0 && node != NULL)
        {
          b--;
          node = prev_node_func (node);
        }

      return b == 0 && node == NULL;
    }

  /* count nodes */
  for (pos = 0; node != NULL; pos++)
    node = prev_node_func (node);

  /* solve pos = a * X + b
   * and return TRUE if X is integer >= 0 */
  x = pos - b;

  if (x % a)
    return FALSE;

  return x / a >= 0;
}

static gboolean
gtk_css_matcher_node_has_position (const GtkCssMatcher *matcher,
                                   gboolean             forward,
                                   int                  a,
                                   int                  b)
{
  return gtk_css_matcher_node_nth_child (matcher->node.node,
                                         forward ? get_previous_visible_sibling 
                                                 : get_next_visible_sibling,
                                         a, b);
}

static const GtkCssMatcherClass GTK_CSS_MATCHER_NODE = {
  gtk_css_matcher_node_get_parent,
  gtk_css_matcher_node_get_previous,
  gtk_css_matcher_node_get_state,
  gtk_css_matcher_node_has_name,
  gtk_css_matcher_node_has_class,
  gtk_css_matcher_node_has_id,
  gtk_css_matcher_node_has_position,
  FALSE
};

void
_gtk_css_matcher_node_init (GtkCssMatcher *matcher,
                            GtkCssNode    *node)
{
  matcher->node.klass = &GTK_CSS_MATCHER_NODE;
  matcher->node.node = node;
}

/* GTK_CSS_MATCHER_WIDGET_ANY */

static gboolean
gtk_css_matcher_any_get_parent (GtkCssMatcher       *matcher,
                                const GtkCssMatcher *child)
{
  _gtk_css_matcher_any_init (matcher);

  return TRUE;
}

static gboolean
gtk_css_matcher_any_get_previous (GtkCssMatcher       *matcher,
                                  const GtkCssMatcher *next)
{
  _gtk_css_matcher_any_init (matcher);

  return TRUE;
}

static GtkStateFlags
gtk_css_matcher_any_get_state (const GtkCssMatcher *matcher)
{
  /* XXX: This gets tricky when we implement :not() */

  return GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED
    | GTK_STATE_FLAG_INSENSITIVE | GTK_STATE_FLAG_INCONSISTENT
    | GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_BACKDROP | GTK_STATE_FLAG_LINK
    | GTK_STATE_FLAG_VISITED;
}

static gboolean
gtk_css_matcher_any_has_name (const GtkCssMatcher     *matcher,
                              /*interned*/ const char *name)
{
  return TRUE;
}

static gboolean
gtk_css_matcher_any_has_class (const GtkCssMatcher *matcher,
                               GQuark               class_name)
{
  return TRUE;
}

static gboolean
gtk_css_matcher_any_has_id (const GtkCssMatcher *matcher,
                                    const char          *id)
{
  return TRUE;
}

static gboolean
gtk_css_matcher_any_has_position (const GtkCssMatcher *matcher,
                                  gboolean             forward,
                                  int                  a,
                                  int                  b)
{
  return TRUE;
}

static const GtkCssMatcherClass GTK_CSS_MATCHER_ANY = {
  gtk_css_matcher_any_get_parent,
  gtk_css_matcher_any_get_previous,
  gtk_css_matcher_any_get_state,
  gtk_css_matcher_any_has_name,
  gtk_css_matcher_any_has_class,
  gtk_css_matcher_any_has_id,
  gtk_css_matcher_any_has_position,
  TRUE
};

void
_gtk_css_matcher_any_init (GtkCssMatcher *matcher)
{
  matcher->klass = &GTK_CSS_MATCHER_ANY;
}

/* GTK_CSS_MATCHER_WIDGET_SUPERSET */

static gboolean
gtk_css_matcher_superset_get_parent (GtkCssMatcher       *matcher,
                                     const GtkCssMatcher *child)
{
  _gtk_css_matcher_any_init (matcher);

  return TRUE;
}

static gboolean
gtk_css_matcher_superset_get_previous (GtkCssMatcher       *matcher,
                                       const GtkCssMatcher *next)
{
  _gtk_css_matcher_any_init (matcher);

  return TRUE;
}

static GtkStateFlags
gtk_css_matcher_superset_get_state (const GtkCssMatcher *matcher)
{
  /* XXX: This gets tricky when we implement :not() */

  if (matcher->superset.relevant & GTK_CSS_CHANGE_STATE)
    return _gtk_css_matcher_get_state (matcher->superset.subset);
  else
    return GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED
      | GTK_STATE_FLAG_INSENSITIVE | GTK_STATE_FLAG_INCONSISTENT
      | GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_BACKDROP | GTK_STATE_FLAG_LINK
      | GTK_STATE_FLAG_VISITED;
}

static gboolean
gtk_css_matcher_superset_has_name (const GtkCssMatcher     *matcher,
                                   /*interned*/ const char *name)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_NAME)
    return _gtk_css_matcher_has_name (matcher->superset.subset, name);
  else
    return TRUE;
}

static gboolean
gtk_css_matcher_superset_has_class (const GtkCssMatcher *matcher,
                                    GQuark               class_name)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_CLASS)
    return _gtk_css_matcher_has_class (matcher->superset.subset, class_name);
  else
    return TRUE;
}

static gboolean
gtk_css_matcher_superset_has_id (const GtkCssMatcher *matcher,
                                 const char          *id)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_NAME)
    return _gtk_css_matcher_has_id (matcher->superset.subset, id);
  else
    return TRUE;
}

static gboolean
gtk_css_matcher_superset_has_position (const GtkCssMatcher *matcher,
                                       gboolean             forward,
                                       int                  a,
                                       int                  b)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_POSITION)
    return _gtk_css_matcher_has_position (matcher->superset.subset, forward, a, b);
  else
    return TRUE;
}

static const GtkCssMatcherClass GTK_CSS_MATCHER_SUPERSET = {
  gtk_css_matcher_superset_get_parent,
  gtk_css_matcher_superset_get_previous,
  gtk_css_matcher_superset_get_state,
  gtk_css_matcher_superset_has_name,
  gtk_css_matcher_superset_has_class,
  gtk_css_matcher_superset_has_id,
  gtk_css_matcher_superset_has_position,
  FALSE
};

void
_gtk_css_matcher_superset_init (GtkCssMatcher       *matcher,
                                const GtkCssMatcher *subset,
                                GtkCssChange         relevant)
{
  g_return_if_fail (subset != NULL);
  g_return_if_fail ((relevant & ~(GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_STATE)) == 0);

  matcher->superset.klass = &GTK_CSS_MATCHER_SUPERSET;
  matcher->superset.subset = subset;
  matcher->superset.relevant = relevant;
}

