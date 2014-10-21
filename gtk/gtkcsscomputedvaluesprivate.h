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

#ifndef __GTK_CSS_COMPUTED_VALUES_PRIVATE_H__
#define __GTK_CSS_COMPUTED_VALUES_PRIVATE_H__

#include <glib-object.h>

#include "gtk/gtkbitmaskprivate.h"
#include "gtk/gtkcsssection.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_COMPUTED_VALUES           (_gtk_css_computed_values_get_type ())
#define GTK_CSS_COMPUTED_VALUES(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_COMPUTED_VALUES, GtkCssComputedValues))
#define GTK_CSS_COMPUTED_VALUES_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_COMPUTED_VALUES, GtkCssComputedValuesClass))
#define GTK_IS_CSS_COMPUTED_VALUES(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_COMPUTED_VALUES))
#define GTK_IS_CSS_COMPUTED_VALUES_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_COMPUTED_VALUES))
#define GTK_CSS_COMPUTED_VALUES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_COMPUTED_VALUES, GtkCssComputedValuesClass))

/* typedef struct _GtkCssComputedValues           GtkCssComputedValues; */
typedef struct _GtkCssComputedValuesClass      GtkCssComputedValuesClass;

struct _GtkCssComputedValues
{
  GObject parent;

  GPtrArray             *values;               /* the unanimated (aka intrinsic) values */
  GPtrArray             *sections;             /* sections the values are defined in */

  GPtrArray             *animated_values;      /* NULL or array of animated values/NULL if not animated */
  gint64                 current_time;         /* the current time in our world */
  GSList                *animations;           /* the running animations, least important one first */

  GtkBitmask            *depends_on_parent;    /* for intrinsic values */
  GtkBitmask            *equals_parent;        /* dito */
  GtkBitmask            *depends_on_color;     /* dito */
  GtkBitmask            *depends_on_font_size; /* dito */
};

struct _GtkCssComputedValuesClass
{
  GObjectClass parent_class;
};

GType                   _gtk_css_computed_values_get_type             (void) G_GNUC_CONST;

GtkCssComputedValues *  _gtk_css_computed_values_new                  (void);

void                    _gtk_css_computed_values_compute_value        (GtkCssComputedValues     *values,
                                                                       GtkStyleProviderPrivate  *provider,
								       int                       scale,
                                                                       GtkCssComputedValues     *parent_values,
                                                                       guint                     id,
                                                                       GtkCssValue              *specified,
                                                                       GtkCssSection            *section);
void                    _gtk_css_computed_values_set_value            (GtkCssComputedValues     *values,
                                                                       guint                     id,
                                                                       GtkCssValue              *value,
                                                                       GtkCssDependencies        dependencies,
                                                                       GtkCssSection            *section);
void                    _gtk_css_computed_values_set_animated_value   (GtkCssComputedValues     *values,
                                                                       guint                     id,
                                                                       GtkCssValue              *value);
                                                                        
GtkCssValue *           _gtk_css_computed_values_get_value            (GtkCssComputedValues     *values,
                                                                       guint                     id);
GtkCssSection *         _gtk_css_computed_values_get_section          (GtkCssComputedValues     *values,
                                                                       guint                     id);
GtkCssValue *           _gtk_css_computed_values_get_intrinsic_value  (GtkCssComputedValues     *values,
                                                                       guint                     id);
GtkBitmask *            _gtk_css_computed_values_get_difference       (GtkCssComputedValues     *values,
                                                                       GtkCssComputedValues     *other);
GtkBitmask *            _gtk_css_computed_values_compute_dependencies (GtkCssComputedValues     *values,
                                                                       const GtkBitmask         *parent_changes);

void                    _gtk_css_computed_values_create_animations    (GtkCssComputedValues     *values,
                                                                       GtkCssComputedValues     *parent_values,
                                                                       gint64                    timestamp,
                                                                       GtkStyleProviderPrivate  *provider,
								       int                       scale,
                                                                       GtkCssComputedValues     *source);
GtkBitmask *            _gtk_css_computed_values_advance              (GtkCssComputedValues     *values,
                                                                       gint64                    timestamp);
void                    _gtk_css_computed_values_cancel_animations    (GtkCssComputedValues     *values);
gboolean                _gtk_css_computed_values_is_static            (GtkCssComputedValues     *values);

char *                  gtk_css_computed_values_to_string             (GtkCssComputedValues     *values);
void                    gtk_css_computed_values_print                 (GtkCssComputedValues     *values,
                                                                       GString                  *string);

G_END_DECLS

#endif /* __GTK_CSS_COMPUTED_VALUES_PRIVATE_H__ */
