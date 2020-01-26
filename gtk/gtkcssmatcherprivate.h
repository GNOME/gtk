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

typedef enum {
  GTK_CSS_MATCHER_TYPE_NODE,
  GTK_CSS_MATCHER_TYPE_WIDGET_PATH
} GtkCssMatcherType;

struct _GtkCssMatcherClass {
  GtkCssMatcherType type;
  gboolean        (* get_parent)                  (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *child);
  gboolean        (* get_previous)                (GtkCssMatcher          *matcher,
                                                   const GtkCssMatcher    *next);

  const char *    (* get_name)                    (const GtkCssMatcher   *matcher);
  GQuark *        (* get_classes)                 (const GtkCssMatcher   *matcher,
                                                   guint                 *n_classes,
                                                   gboolean              *allocated);

  gboolean        (* has_state)                   (const GtkCssMatcher   *matcher,
                                                   GtkStateFlags          state);
  gboolean        (* has_name)                    (const GtkCssMatcher   *matcher,
                                                   /*interned*/const char*name);
  gboolean        (* has_class)                   (const GtkCssMatcher   *matcher,
                                                   GQuark                 class_name);
  gboolean        (* has_id)                      (const GtkCssMatcher   *matcher,
                                                   const char            *id);
  gboolean        (* has_position)                (const GtkCssMatcher   *matcher,
                                                   gboolean               forward,
                                                   int                    a,
                                                   int                    b);
  void            (* print)                       (const GtkCssMatcher   *matcher,
                                                   GString               *string);
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
  GtkStateFlags             node_state;
  const char               *node_name;
  const char               *node_id;
  const GQuark             *classes;
  guint                     n_classes;
};

union _GtkCssMatcher {
  const GtkCssMatcherClass *klass;
  GtkCssMatcherWidgetPath   path;
  GtkCssMatcherNode         node;
};

gboolean          _gtk_css_matcher_init           (GtkCssMatcher          *matcher,
                                                   const GtkWidgetPath    *path,
                                                   const GtkCssNodeDeclaration *decl) G_GNUC_WARN_UNUSED_RESULT;
void              _gtk_css_matcher_node_init      (GtkCssMatcher          *matcher,
                                                   GtkCssNode             *node);

void              gtk_css_matcher_print           (const GtkCssMatcher    *matcher,
                                                   GString                *string);
char *            gtk_css_matcher_to_string       (const GtkCssMatcher    *matcher);


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

static inline const char *
_gtk_css_matcher_get_name (const GtkCssMatcher *matcher)
{
  return matcher->klass->get_name (matcher);
}

static inline GQuark *
_gtk_css_matcher_get_classes (const GtkCssMatcher *matcher,
                              guint               *n_classes,
                              gboolean            *allocated)
{
  return matcher->klass->get_classes (matcher, n_classes, allocated);
}

static inline gboolean
_gtk_css_matcher_has_state (const GtkCssMatcher *matcher,
                            GtkStateFlags        state)
{
  return matcher->klass->has_state (matcher, state);
}

static inline gboolean
_gtk_css_matcher_has_name (const GtkCssMatcher     *matcher,
                           /*interned*/ const char *name)
{
  return matcher->klass->has_name (matcher, name);
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

static inline guint
_gtk_css_matcher_has_position (const GtkCssMatcher *matcher,
                               gboolean             forward,
                               int                  a,
                               int                  b)
{
  return matcher->klass->has_position (matcher, forward, a, b);
}

G_END_DECLS

#endif /* __GTK_CSS_MATCHER_PRIVATE_H__ */
