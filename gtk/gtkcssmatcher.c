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

#include "gtkwidgetpath.h"

/* GTK_CSS_MATCHER_WIDGET_PATH */

static gboolean
gtk_css_matcher_widget_path_get_parent (GtkCssMatcher       *matcher,
                                        const GtkCssMatcher *child)
{
  if (child->path.index == 0)
    return FALSE;

  matcher->path.klass = child->path.klass;
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
  matcher->path.path = next->path.path;
  matcher->path.index = next->path.index;
  matcher->path.sibling_index = next->path.sibling_index - 1;

  return TRUE;
}

static GtkStateFlags
gtk_css_matcher_widget_path_get_state (const GtkCssMatcher *matcher)
{
  return gtk_widget_path_iter_get_state (matcher->path.path, matcher->path.index);
}

static gboolean
gtk_css_matcher_widget_path_has_type (const GtkCssMatcher *matcher,
                                      GType                type)
{
  const GtkWidgetPath *siblings;

  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    return g_type_is_a (gtk_widget_path_iter_get_object_type (siblings, matcher->path.sibling_index), type);
  else
    return g_type_is_a (gtk_widget_path_iter_get_object_type (matcher->path.path, matcher->path.index), type);
}

static gboolean
gtk_css_matcher_widget_path_has_class (const GtkCssMatcher *matcher,
                                       GQuark               class_name)
{
  const GtkWidgetPath *siblings;
  
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
gtk_css_matcher_widget_path_has_regions (const GtkCssMatcher *matcher)
{
  const GtkWidgetPath *siblings;
  GSList *regions;
  gboolean result;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    regions = gtk_widget_path_iter_list_regions (siblings, matcher->path.sibling_index);
  else
    regions = gtk_widget_path_iter_list_regions (matcher->path.path, matcher->path.index);
  result = regions != NULL;
  g_slist_free (regions);

  return result;
G_GNUC_END_IGNORE_DEPRECATIONS
}

static gboolean
gtk_css_matcher_widget_path_has_region (const GtkCssMatcher *matcher,
                                        const char          *region,
                                        GtkRegionFlags       flags)
{
  const GtkWidgetPath *siblings;
  GtkRegionFlags region_flags;
  
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  siblings = gtk_widget_path_iter_get_siblings (matcher->path.path, matcher->path.index);
  if (siblings && matcher->path.sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path.path, matcher->path.index))
    {
      if (!gtk_widget_path_iter_has_region (siblings, matcher->path.sibling_index, region, &region_flags))
        return FALSE;
    }
  else
    {
      if (!gtk_widget_path_iter_has_region (matcher->path.path, matcher->path.index, region, &region_flags))
        return FALSE;
    }

  if ((flags & region_flags) != flags)
    return FALSE;

  return TRUE;
G_GNUC_END_IGNORE_DEPRECATIONS
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

  return x / a > 0;
}

static const GtkCssMatcherClass GTK_CSS_MATCHER_WIDGET_PATH = {
  gtk_css_matcher_widget_path_get_parent,
  gtk_css_matcher_widget_path_get_previous,
  gtk_css_matcher_widget_path_get_state,
  gtk_css_matcher_widget_path_has_type,
  gtk_css_matcher_widget_path_has_class,
  gtk_css_matcher_widget_path_has_id,
  gtk_css_matcher_widget_path_has_regions,
  gtk_css_matcher_widget_path_has_region,
  gtk_css_matcher_widget_path_has_position,
  FALSE
};

gboolean
_gtk_css_matcher_init (GtkCssMatcher       *matcher,
                       const GtkWidgetPath *path)
{
  if (gtk_widget_path_length (path) == 0)
    return FALSE;

  matcher->path.klass = &GTK_CSS_MATCHER_WIDGET_PATH;
  matcher->path.path = path;
  matcher->path.index = gtk_widget_path_length (path) - 1;
  matcher->path.sibling_index = gtk_widget_path_iter_get_sibling_index (path, matcher->path.index);

  return TRUE;
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
gtk_css_matcher_any_has_type (const GtkCssMatcher *matcher,
                              GType                type)
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
gtk_css_matcher_any_has_regions (const GtkCssMatcher *matcher)
{
  return TRUE;
}

static gboolean
gtk_css_matcher_any_has_region (const GtkCssMatcher *matcher,
                                const char          *region,
                                GtkRegionFlags       flags)
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
  gtk_css_matcher_any_has_type,
  gtk_css_matcher_any_has_class,
  gtk_css_matcher_any_has_id,
  gtk_css_matcher_any_has_regions,
  gtk_css_matcher_any_has_region,
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
gtk_css_matcher_superset_has_type (const GtkCssMatcher *matcher,
                                   GType                type)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_NAME)
    return _gtk_css_matcher_has_type (matcher->superset.subset, type);
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
gtk_css_matcher_superset_has_regions (const GtkCssMatcher *matcher)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_NAME)
    return _gtk_css_matcher_has_regions (matcher->superset.subset);
  else
    return TRUE;
}

static gboolean
gtk_css_matcher_superset_has_region (const GtkCssMatcher *matcher,
                                     const char          *region,
                                     GtkRegionFlags       flags)
{
  if (matcher->superset.relevant & GTK_CSS_CHANGE_NAME)
    {
      if (matcher->superset.relevant & GTK_CSS_CHANGE_POSITION)
        return _gtk_css_matcher_has_region (matcher->superset.subset, region, flags);
      else
        return _gtk_css_matcher_has_region (matcher->superset.subset, region, 0);
    }
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
  gtk_css_matcher_superset_has_type,
  gtk_css_matcher_superset_has_class,
  gtk_css_matcher_superset_has_id,
  gtk_css_matcher_superset_has_regions,
  gtk_css_matcher_superset_has_region,
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

