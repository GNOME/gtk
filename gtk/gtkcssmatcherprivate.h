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
#include "gtk/gtkcsstypesprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssMatcherNode GtkCssMatcherNode;
typedef struct _GtkCssMatcherSuperset GtkCssMatcherSuperset;
typedef struct _GtkCssMatcherWidgetPath GtkCssMatcherWidgetPath;
typedef struct _GtkCssMatcherClass GtkCssMatcherClass;

struct _GtkCssMatcherClass {
  gboolean        (* get_parent)                  (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *child);
  gboolean        (* get_previous)                (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *next);

  GtkStateFlags   (* get_state)                   (const GtkCssMatcher   *matcher);
  gboolean        (* has_type)                    (const GtkCssMatcher   *matcher,
                                                   GType                  type);
  gboolean        (* has_class)                   (const GtkCssMatcher   *matcher,
                                                   GQuark                 class_name);
  gboolean        (* has_id)                      (const GtkCssMatcher   *matcher,
                                                   const char            *id);
  gboolean        (* has_regions)                 (const GtkCssMatcher   *matcher);
  gboolean        (* has_region)                  (const GtkCssMatcher   *matcher,
                                                   const char            *region,
                                                   GtkRegionFlags         flags);
  gboolean        (* has_position)                (const GtkCssMatcher   *matcher,
                                                   gboolean               forward,
                                                   int                    a,
                                                   int                    b);
  gboolean is_any;
};

struct _GtkCssMatcherWidgetPath {
  const GtkCssMatcherClass *klass;
  const GtkCssNodeDeclaration *decl;
  const GtkWidgetPath      *path;
  guint                     index;
  guint                     sibling_index;
};

struct _GtkCssMatcherNode {
  const GtkCssMatcherClass *klass;
  GtkCssNode               *node;
};

struct _GtkCssMatcherSuperset {
  const GtkCssMatcherClass *klass;
  const GtkCssMatcher      *subset;
  GtkCssChange              relevant;
};

union _GtkCssMatcher {
  const GtkCssMatcherClass *klass;
  GtkCssMatcherWidgetPath   path;
  GtkCssMatcherNode         node;
  GtkCssMatcherSuperset     superset;
};

gboolean          _gtk_css_matcher_init           (GtkCssMatcher          *matcher,
                                                   const GtkWidgetPath    *path,
                                                   const GtkCssNodeDeclaration *decl) G_GNUC_WARN_UNUSED_RESULT;
void              _gtk_css_matcher_node_init      (GtkCssMatcher          *matcher,
                                                   GtkCssNode             *node);
void              _gtk_css_matcher_any_init       (GtkCssMatcher          *matcher);
void              _gtk_css_matcher_superset_init  (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *subset,
                                                   GtkCssChange            relevant);


static inline gboolean
_gtk_css_matcher_get_parent (GtkCssMatcher       *matcher,
                             const GtkCssMatcher *child)
{
  return child->klass->get_parent (matcher, child);
}

static inline gboolean
_gtk_css_matcher_get_previous (GtkCssMatcher       *matcher,
                               const GtkCssMatcher *next)
{
  return next->klass->get_previous (matcher, next);
}

static inline GtkStateFlags
_gtk_css_matcher_get_state (const GtkCssMatcher *matcher)
{
  return matcher->klass->get_state (matcher);
}

static inline gboolean
_gtk_css_matcher_has_type (const GtkCssMatcher *matcher,
                           GType type)
{
  return matcher->klass->has_type (matcher, type);
}

static inline gboolean
_gtk_css_matcher_has_class (const GtkCssMatcher *matcher,
                            GQuark               class_name)
{
  return matcher->klass->has_class (matcher, class_name);
}

static inline gboolean
_gtk_css_matcher_has_id (const GtkCssMatcher *matcher,
                         const char          *id)
{
  return matcher->klass->has_id (matcher, id);
}


static inline gboolean
_gtk_css_matcher_has_regions (const GtkCssMatcher *matcher)
{
  return matcher->klass->has_regions (matcher);
}

static inline gboolean
_gtk_css_matcher_has_region (const GtkCssMatcher *matcher,
                             const char          *region,
                             GtkRegionFlags       flags)
{
  return matcher->klass->has_region (matcher, region, flags);
}

static inline guint
_gtk_css_matcher_has_position (const GtkCssMatcher *matcher,
                               gboolean             forward,
                               int                  a,
                               int                  b)
{
  return matcher->klass->has_position (matcher, forward, a, b);
}

static inline gboolean
_gtk_css_matcher_matches_any (const GtkCssMatcher *matcher)
{
  return matcher->klass->is_any;
}


G_END_DECLS

#endif /* __GTK_CSS_MATCHER_PRIVATE_H__ */
