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

typedef struct _GtkCssValue           GtkCssValue;

/* A GtkCssValue is a refcounted immutable value type */

GType        _gtk_css_value_get_type                  (void) G_GNUC_CONST;
GtkCssValue *_gtk_css_value_ref                       (GtkCssValue                *value);
void         _gtk_css_value_unref                     (GtkCssValue                *value);
GType        _gtk_css_value_get_content_type          (GtkCssValue                *value);
gboolean     _gtk_css_value_holds                     (GtkCssValue                *value,
						       GType                       type);
GtkCssValue *_gtk_css_value_new_from_gvalue           (const GValue               *g_value);
GtkCssValue *_gtk_css_value_new_take_gvalue           (GValue                     *g_value);
GtkCssValue *_gtk_css_value_new_from_int              (gint                        val);
GtkCssValue *_gtk_css_value_new_from_string           (const char                 *string);
GtkCssValue *_gtk_css_value_new_take_string           (char                       *string);
GtkCssValue *_gtk_css_value_new_take_font_description (PangoFontDescription       *v);
GtkCssValue *_gtk_css_value_new_from_rgba             (const GdkRGBA              *v);
GtkCssValue *_gtk_css_value_new_from_color            (const GdkColor             *v);
GtkCssValue *_gtk_css_value_new_take_symbolic_color   (GtkSymbolicColor           *v);
GtkCssValue *_gtk_css_value_new_from_border           (const GtkBorder            *border);
GtkCssValue *_gtk_css_value_new_take_pattern          (cairo_pattern_t            *v);
GtkCssValue *_gtk_css_value_new_from_pattern          (const cairo_pattern_t      *v);
GtkCssValue *_gtk_css_value_new_take_shadow           (GtkShadow                  *v);
GtkCssValue *_gtk_css_value_new_take_image            (GtkCssImage                *v);
GtkCssValue *_gtk_css_value_new_from_number           (const GtkCssNumber         *v);
GtkCssValue *_gtk_css_value_new_from_background_size  (const GtkCssBackgroundSize *v);
GtkCssValue *_gtk_css_value_new_from_background_position (const GtkCssBackgroundPosition *v);
void         _gtk_css_value_to_gvalue                 (GtkCssValue                *value,
						       GValue                     *g_value);
void         _gtk_css_value_init_gvalue               (GtkCssValue                *value,
						       GValue                     *g_value);

gboolean                  _gtk_css_value_is_special                 (GtkCssValue *value);
GtkCssSpecialValue        _gtk_css_value_get_special_kind           (GtkCssValue *value);
int                       _gtk_css_value_get_int                    (GtkCssValue *value);
double                    _gtk_css_value_get_double                 (GtkCssValue *value);
const char *              _gtk_css_value_get_string                 (GtkCssValue *value);
gpointer                  _gtk_css_value_dup_object                 (GtkCssValue *value);
gpointer                  _gtk_css_value_get_object                 (GtkCssValue *value);
gpointer                  _gtk_css_value_get_boxed                  (GtkCssValue *value);
const char **             _gtk_css_value_get_strv                   (GtkCssValue *value);
GtkCssNumber             *_gtk_css_value_get_number                 (GtkCssValue *value);
GtkSymbolicColor         *_gtk_css_value_get_symbolic_color         (GtkCssValue *value);
GtkCssImage              *_gtk_css_value_get_image                  (GtkCssValue *value);
GtkBorderStyle            _gtk_css_value_get_border_style           (GtkCssValue *value);
GtkCssBackgroundSize     *_gtk_css_value_get_background_size        (GtkCssValue *value);
GtkCssBackgroundPosition *_gtk_css_value_get_background_position    (GtkCssValue *value);
GtkCssBorderCornerRadius *_gtk_css_value_get_border_corner_radius   (GtkCssValue *value);
GtkCssBorderImageRepeat  *_gtk_css_value_get_border_image_repeat    (GtkCssValue *value);
PangoFontDescription *    _gtk_css_value_get_font_description       (GtkCssValue *value);
PangoStyle                _gtk_css_value_get_pango_style            (GtkCssValue *value);
PangoVariant              _gtk_css_value_get_pango_variant          (GtkCssValue *value);
PangoWeight               _gtk_css_value_get_pango_weight           (GtkCssValue *value);
GdkRGBA                  *_gtk_css_value_get_rgba                   (GtkCssValue *value);
cairo_pattern_t          *_gtk_css_value_get_pattern                (GtkCssValue *value);
GtkGradient              *_gtk_css_value_get_gradient               (GtkCssValue *value);
GtkShadow                *_gtk_css_value_get_shadow                 (GtkCssValue *value);

G_END_DECLS

#endif /* __GTK_CSS_VALUE_PRIVATE_H__ */
