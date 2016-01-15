/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnodestylecacheprivate.h"

struct _GtkCssNodeStyleCache {
  guint        ref_count;
  GtkCssStyle *style;
  GHashTable  *children;
};

GtkCssNodeStyleCache *
gtk_css_node_style_cache_new (GtkCssStyle *style)
{
  GtkCssNodeStyleCache *result;

  result = g_slice_new0 (GtkCssNodeStyleCache);

  result->ref_count = 1;
  result->style = g_object_ref (style);

  return result;
}

void
gtk_css_node_style_cache_unref (GtkCssNodeStyleCache *cache)
{
  cache->ref_count--; 

  if (cache->ref_count > 0)
    return;

  g_object_unref (cache->style);
}

GtkCssStyle *
gtk_css_node_style_cache_get_style (GtkCssNodeStyleCache *cache)
{
  return cache->style;
}

GtkCssNodeStyleCache *
gtk_css_node_style_cache_insert (GtkCssNodeStyleCache   *parent,
                                 GtkCssNodeDeclaration  *decl,
                                 gboolean                is_first,
                                 gboolean                is_last,
                                 GtkCssStyle            *style)
{
  return gtk_css_node_style_cache_new (style);
}

GtkCssNodeStyleCache *
gtk_css_node_style_cache_lookup (GtkCssNodeStyleCache   *parent,
                                 GtkCssNodeDeclaration  *decl,
                                 gboolean                is_first,
                                 gboolean                is_last)
{
  return NULL;
}

