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

#ifndef __GTK_CSS_MATCHER_PRIVATE_H__
#define __GTK_CSS_MATCHER_PRIVATE_H__

#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

typedef struct _GtkCssMatcher GtkCssMatcher;

struct _GtkCssMatcher {
  const GtkWidgetPath *path;
  GtkStateFlags        state_flags;
  guint                index;
  guint                sibling_index;
};

void              _gtk_css_matcher_init           (GtkCssMatcher          *matcher,
                                                   const GtkWidgetPath    *path,
                                                   GtkStateFlags           state);
gboolean          _gtk_css_matcher_get_parent     (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *child);
gboolean          _gtk_css_matcher_get_previous   (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *next);

GtkStateFlags     _gtk_css_matcher_get_state      (const GtkCssMatcher   *matcher);
gboolean          _gtk_css_matcher_has_name       (const GtkCssMatcher   *matcher,
                                                   const char            *name);
gboolean          _gtk_css_matcher_has_class      (const GtkCssMatcher   *matcher,
                                                   const char            *class_name);
gboolean          _gtk_css_matcher_has_id         (const GtkCssMatcher   *matcher,
                                                   const char            *id);
gboolean          _gtk_css_matcher_has_regions    (const GtkCssMatcher   *matcher);
gboolean          _gtk_css_matcher_has_region     (const GtkCssMatcher   *matcher,
                                                   const char            *region,
                                                   GtkRegionFlags         flags);
guint             _gtk_css_matcher_get_sibling_index
                                                  (const GtkCssMatcher   *matcher);
guint             _gtk_css_matcher_get_n_siblings (const GtkCssMatcher   *matcher);

G_END_DECLS

#endif /* __GTK_CSS_MATCHER_PRIVATE_H__ */
