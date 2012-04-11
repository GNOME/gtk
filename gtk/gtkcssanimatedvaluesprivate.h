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

#ifndef __GTK_CSS_ANIMATED_VALUES_PRIVATE_H__
#define __GTK_CSS_ANIMATED_VALUES_PRIVATE_H__

#include "gtk/gtkcsscomputedvaluesprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_ANIMATED_VALUES           (_gtk_css_animated_values_get_type ())
#define GTK_CSS_ANIMATED_VALUES(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_ANIMATED_VALUES, GtkCssAnimatedValues))
#define GTK_CSS_ANIMATED_VALUES_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_ANIMATED_VALUES, GtkCssAnimatedValuesClass))
#define GTK_IS_CSS_ANIMATED_VALUES(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_ANIMATED_VALUES))
#define GTK_IS_CSS_ANIMATED_VALUES_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_ANIMATED_VALUES))
#define GTK_CSS_ANIMATED_VALUES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_ANIMATED_VALUES, GtkCssAnimatedValuesClass))

typedef struct _GtkCssAnimatedValues           GtkCssAnimatedValues;
typedef struct _GtkCssAnimatedValuesClass      GtkCssAnimatedValuesClass;

struct _GtkCssAnimatedValues
{
  GtkCssComputedValues parent;

  gint64 current_time;                  /* the current time in our world */
  GtkCssComputedValues *computed;       /* the computed values we'd have without animations */
  GSList *animations;                   /* the running animations */
};

struct _GtkCssAnimatedValuesClass
{
  GtkCssComputedValuesClass parent_class;
};

GType                   _gtk_css_animated_values_get_type             (void) G_GNUC_CONST;

GtkCssComputedValues *  _gtk_css_animated_values_new                  (GtkCssComputedValues     *computed,
                                                                       GtkCssComputedValues     *source,
                                                                       gint64                    timestamp);

GtkBitmask *            _gtk_css_animated_values_advance              (GtkCssAnimatedValues     *values,
                                                                       gint64                    timestamp) G_GNUC_WARN_UNUSED_RESULT;
gboolean                _gtk_css_animated_values_is_finished          (GtkCssAnimatedValues     *values);


G_END_DECLS

#endif /* __GTK_CSS_ANIMATED_VALUES_PRIVATE_H__ */
