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

void
_gtk_css_matcher_init (GtkCssMatcher       *matcher,
                       const GtkWidgetPath *path,
                       GtkStateFlags        state)
{
  matcher->path = path;
  matcher->state_flags = state;
  matcher->index = gtk_widget_path_length (path) - 1;
  matcher->sibling_index = gtk_widget_path_iter_get_sibling_index (path, matcher->index);
}

gboolean
_gtk_css_matcher_get_parent (GtkCssMatcher       *matcher,
                             const GtkCssMatcher *child)
{
  if (child->index == 0)
    return FALSE;

  matcher->path = child->path;
  matcher->state_flags = 0;
  matcher->index = child->index - 1;
  matcher->sibling_index = gtk_widget_path_iter_get_sibling_index (matcher->path, matcher->index);

  return TRUE;
}

gboolean
_gtk_css_matcher_get_previous (GtkCssMatcher       *matcher,
                               const GtkCssMatcher *next)
{
  if (next->sibling_index == 0)
    return FALSE;

  matcher->path = next->path;
  matcher->state_flags = 0;
  matcher->index = next->index;
  matcher->sibling_index = next->sibling_index - 1;

  return TRUE;
}

GtkStateFlags
_gtk_css_matcher_get_state (const GtkCssMatcher *matcher)
{
  return matcher->state_flags;
}

gboolean
_gtk_css_matcher_has_name (const GtkCssMatcher *matcher,
                           const char          *name)
{
  const GtkWidgetPath *siblings;
  GType type;
  
  type = g_type_from_name (name);
  siblings = gtk_widget_path_iter_get_siblings (matcher->path, matcher->index);
  if (siblings && matcher->sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path, matcher->index))
    return g_type_is_a (gtk_widget_path_iter_get_object_type (siblings, matcher->sibling_index), type);
  else
    return g_type_is_a (gtk_widget_path_iter_get_object_type (matcher->path, matcher->index), type);
}

gboolean
_gtk_css_matcher_has_class (const GtkCssMatcher *matcher,
                            const char          *class_name)
{
  const GtkWidgetPath *siblings;
  
  siblings = gtk_widget_path_iter_get_siblings (matcher->path, matcher->index);
  if (siblings && matcher->sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path, matcher->index))
    return gtk_widget_path_iter_has_class (siblings, matcher->sibling_index, class_name);
  else
    return gtk_widget_path_iter_has_class (matcher->path, matcher->index, class_name);
}

gboolean
_gtk_css_matcher_has_id (const GtkCssMatcher *matcher,
                         const char          *id)
{
  const GtkWidgetPath *siblings;
  
  siblings = gtk_widget_path_iter_get_siblings (matcher->path, matcher->index);
  if (siblings && matcher->sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path, matcher->index))
    return gtk_widget_path_iter_has_name (siblings, matcher->sibling_index, id);
  else
    return gtk_widget_path_iter_has_name (matcher->path, matcher->index, id);
}

gboolean
_gtk_css_matcher_has_regions (const GtkCssMatcher *matcher)
{
  const GtkWidgetPath *siblings;
  GSList *regions;
  gboolean result;
  
  siblings = gtk_widget_path_iter_get_siblings (matcher->path, matcher->index);
  if (siblings && matcher->sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path, matcher->index))
    regions = gtk_widget_path_iter_list_regions (siblings, matcher->sibling_index);
  else
    regions = gtk_widget_path_iter_list_regions (matcher->path, matcher->index);
  result = regions != NULL;
  g_slist_free (regions);

  return result;
}

gboolean
_gtk_css_matcher_has_region (const GtkCssMatcher *matcher,
                             const char          *region,
                             GtkRegionFlags       flags)
{
  const GtkWidgetPath *siblings;
  GtkRegionFlags region_flags;
  
  siblings = gtk_widget_path_iter_get_siblings (matcher->path, matcher->index);
  if (siblings && matcher->sibling_index != gtk_widget_path_iter_get_sibling_index (matcher->path, matcher->index))
    {
      if (!gtk_widget_path_iter_has_region (siblings, matcher->sibling_index, region, &region_flags))
        return FALSE;
    }
  else
    {
      if (!gtk_widget_path_iter_has_region (matcher->path, matcher->index, region, &region_flags))
        return FALSE;
    }

  if ((flags & region_flags) != flags)
    return FALSE;

  return TRUE;
}

guint
_gtk_css_matcher_get_sibling_index (const GtkCssMatcher *matcher)
{
  return matcher->sibling_index;
}

guint
_gtk_css_matcher_get_n_siblings (const GtkCssMatcher *matcher)
{
  const GtkWidgetPath *siblings;

  siblings = gtk_widget_path_iter_get_siblings (matcher->path, matcher->index);
  if (!siblings)
    return 0;

  return gtk_widget_path_length (siblings);
}

