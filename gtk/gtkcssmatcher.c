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

void
gtk_css_matcher_print (const GtkCssMatcher *matcher,
                       GString             *string)
{
  matcher->klass->print (matcher, string);
}

char *
gtk_css_matcher_to_string (const GtkCssMatcher *matcher)
{
  GString *string = g_string_new ("");
  gtk_css_matcher_print (matcher, string);
  return g_string_free (string, FALSE);
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

  _gtk_css_matcher_node_init (matcher, node);
  return TRUE;
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

  _gtk_css_matcher_node_init (matcher, node);
  return TRUE;
}

static gboolean
gtk_css_matcher_node_has_state (const GtkCssMatcher *matcher,
                                GtkStateFlags        state)
{
  return (gtk_css_node_get_state (matcher->node.node) & state) == state;
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

static void
gtk_css_matcher_node_print (const GtkCssMatcher *matcher,
                            GString             *string)
{
  gtk_css_node_print (matcher->node.node, 0, string, 0);
}

static const GtkCssMatcherClass GTK_CSS_MATCHER_NODE = {
  GTK_CSS_MATCHER_TYPE_NODE,
  gtk_css_matcher_node_get_parent,
  gtk_css_matcher_node_get_previous,
  gtk_css_matcher_node_has_state,
  gtk_css_matcher_node_has_name,
  gtk_css_matcher_node_has_class,
  gtk_css_matcher_node_has_id,
  gtk_css_matcher_node_has_position,
  gtk_css_matcher_node_print
};

void
_gtk_css_matcher_node_init (GtkCssMatcher *matcher,
                            GtkCssNode    *node)
{
  matcher->node.klass = &GTK_CSS_MATCHER_NODE;
  matcher->node.node = node;
}
