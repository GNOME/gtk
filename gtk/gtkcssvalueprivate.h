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
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#ifndef __GTK_CSS_VALUE_PRIVATE_H__
#define __GTK_CSS_VALUE_PRIVATE_H__

#include <glib-object.h>
#include "gtkcsstypesprivate.h"
#include "gtksymboliccolor.h"
#include "gtkcssimageprivate.h"
#include "gtkshadowprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_VALUE           (_gtk_css_value_get_type ())
#define GTK_CSS_VALUE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_VALUE, GtkCssValue))
#define GTK_CSS_VALUE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_VALUE, GtkCssValueClass))
#define GTK_IS_CSS_VALUE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_VALUE))
#define GTK_IS_CSS_VALUE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_VALUE))
#define GTK_CSS_VALUE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_VALUE, GtkCssValueClass))

/* A GtkCssValue is a refcounted immutable value type */

typedef struct _GtkCssValue           GtkCssValue;
typedef struct _GtkCssValueClass      GtkCssValueClass;

/* using define instead of struct here so compilers get the packing right */
#define GTK_CSS_VALUE_BASE \
  const GtkCssValueClass *class; \
  volatile gint ref_count;

struct _GtkCssValueClass {
  void          (* free)                              (GtkCssValue                *value);

  gboolean      (* equal)                             (const GtkCssValue          *value1,
                                                       const GtkCssValue          *value2);
  void          (* print)                             (const GtkCssValue          *value,
                                                       GString                    *string);
};

GType        _gtk_css_value_get_type                  (void) G_GNUC_CONST;

GtkCssValue *_gtk_css_value_alloc                     (const GtkCssValueClass     *klass,
                                                       gsize                       size);
#define _gtk_css_value_new(_name, _klass) ((_name *) _gtk_css_value_alloc ((_klass), sizeof (_name)))

GtkCssValue *_gtk_css_value_ref                       (GtkCssValue                *value);
void         _gtk_css_value_unref                     (GtkCssValue                *value);

gboolean     _gtk_css_value_equal                     (const GtkCssValue          *value1,
                                                       const GtkCssValue          *value2);

void         _gtk_css_value_print                     (const GtkCssValue          *value,
                                                       GString                    *string);

GType        _gtk_css_value_get_content_type          (const GtkCssValue          *value);
gboolean     _gtk_css_value_holds                     (const GtkCssValue          *value,
						       GType                       type);
GtkCssValue *_gtk_css_value_new_from_gvalue           (const GValue               *g_value);
GtkCssValue *_gtk_css_value_new_from_int              (gint                        val);
GtkCssValue *_gtk_css_value_new_from_enum             (GType                       type,
                                                       gint                        val);
GtkCssValue *_gtk_css_value_new_from_double           (double                      d);
GtkCssValue *_gtk_css_value_new_take_string           (char                       *string);
GtkCssValue *_gtk_css_value_new_take_strv             (char                      **strv);
GtkCssValue *_gtk_css_value_new_from_boxed            (GType                       type,
                                                       gpointer                    boxed);
GtkCssValue *_gtk_css_value_new_from_rgba             (const GdkRGBA              *v);
GtkCssValue *_gtk_css_value_new_from_color            (const GdkColor             *v);
GtkCssValue *_gtk_css_value_new_take_symbolic_color   (GtkSymbolicColor           *v);
GtkCssValue *_gtk_css_value_new_take_pattern          (cairo_pattern_t            *v);
GtkCssValue *_gtk_css_value_new_take_shadow           (GtkShadow                  *v);
GtkCssValue *_gtk_css_value_new_take_image            (GtkCssImage                *v);
GtkCssValue *_gtk_css_value_new_from_theming_engine   (GtkThemingEngine           *v);
GtkCssValue *_gtk_css_value_new_take_binding_sets     (GPtrArray                  *array);
GtkCssValue *_gtk_css_value_new_from_background_size  (const GtkCssBackgroundSize *v);
GtkCssValue *_gtk_css_value_new_from_background_position (const GtkCssBackgroundPosition *v);
GtkCssValue *_gtk_css_value_new_from_border_corner_radius (const GtkCssBorderCornerRadius *v);
GtkCssValue *_gtk_css_value_new_from_border_image_repeat (const GtkCssBorderImageRepeat *v);
GtkCssValue *_gtk_css_value_new_from_border_style     (GtkBorderStyle              style);
void         _gtk_css_value_init_gvalue               (const GtkCssValue          *value,
						       GValue                     *g_value);

int                             _gtk_css_value_get_int                    (const GtkCssValue *value);
int                             _gtk_css_value_get_enum                   (const GtkCssValue *value);
double                          _gtk_css_value_get_double                 (const GtkCssValue *value);
const char *                    _gtk_css_value_get_string                 (const GtkCssValue *value);
gpointer                        _gtk_css_value_dup_object                 (const GtkCssValue *value);
gpointer                        _gtk_css_value_get_object                 (const GtkCssValue *value);
gpointer                        _gtk_css_value_get_boxed                  (const GtkCssValue *value);
const char **                   _gtk_css_value_get_strv                   (const GtkCssValue *value);
GtkSymbolicColor               *_gtk_css_value_get_symbolic_color         (const GtkCssValue *value);
GtkCssImage                    *_gtk_css_value_get_image                  (const GtkCssValue *value);
GtkBorderStyle                  _gtk_css_value_get_border_style           (const GtkCssValue *value);
const GtkCssBackgroundSize     *_gtk_css_value_get_background_size        (const GtkCssValue *value);
const GtkCssBackgroundPosition *_gtk_css_value_get_background_position    (const GtkCssValue *value);
const GtkCssBorderCornerRadius *_gtk_css_value_get_border_corner_radius   (const GtkCssValue *value);
const GtkCssBorderImageRepeat  *_gtk_css_value_get_border_image_repeat    (const GtkCssValue *value);
PangoStyle                      _gtk_css_value_get_pango_style            (const GtkCssValue *value);
PangoVariant                    _gtk_css_value_get_pango_variant          (const GtkCssValue *value);
PangoWeight                     _gtk_css_value_get_pango_weight           (const GtkCssValue *value);
const GdkRGBA                  *_gtk_css_value_get_rgba                   (const GtkCssValue *value);
cairo_pattern_t                *_gtk_css_value_get_pattern                (const GtkCssValue *value);
GtkGradient                    *_gtk_css_value_get_gradient               (const GtkCssValue *value);
GtkShadow                      *_gtk_css_value_get_shadow                 (const GtkCssValue *value);

G_END_DECLS

#endif /* __GTK_CSS_VALUE_PRIVATE_H__ */
