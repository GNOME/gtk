/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_CSS_TRANSITION_PRIVATE_H__
#define __GTK_CSS_TRANSITION_PRIVATE_H__

#include "gtkstyleanimationprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_TRANSITION           (_gtk_css_transition_get_type ())
#define GTK_CSS_TRANSITION(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_TRANSITION, GtkCssTransition))
#define GTK_CSS_TRANSITION_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_TRANSITION, GtkCssTransitionClass))
#define GTK_IS_CSS_TRANSITION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_TRANSITION))
#define GTK_IS_CSS_TRANSITION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_TRANSITION))
#define GTK_CSS_TRANSITION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_TRANSITION, GtkCssTransitionClass))

typedef struct _GtkCssTransition           GtkCssTransition;
typedef struct _GtkCssTransitionClass      GtkCssTransitionClass;

struct _GtkCssTransition
{
  GtkStyleAnimation parent;

  guint        property;
  GtkCssValue *start;
  GtkCssValue *ease;
  gint64       start_time;
  gint64       end_time;
};

struct _GtkCssTransitionClass
{
  GtkStyleAnimationClass parent_class;
};

GType                   _gtk_css_transition_get_type        (void) G_GNUC_CONST;

GtkStyleAnimation *     _gtk_css_transition_new             (guint               property,
                                                             GtkCssValue        *start,
                                                             GtkCssValue        *ease,
                                                             gint64              start_time_us,
                                                             gint64              end_time_us);

guint                   _gtk_css_transition_get_property    (GtkCssTransition   *transition);

G_END_DECLS

#endif /* __GTK_CSS_TRANSITION_PRIVATE_H__ */
