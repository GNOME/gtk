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

#ifndef __GTK_CSS_STATIC_STYLE_PRIVATE_H__
#define __GTK_CSS_STATIC_STYLE_PRIVATE_H__

#include "gtk/gtkcssmatcherprivate.h"
#include "gtk/gtkcssstyleprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STATIC_STYLE           (gtk_css_static_style_get_type ())
#define GTK_CSS_STATIC_STYLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyle))
#define GTK_CSS_STATIC_STYLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyleClass))
#define GTK_IS_CSS_STATIC_STYLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STATIC_STYLE))
#define GTK_IS_CSS_STATIC_STYLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STATIC_STYLE))
#define GTK_CSS_STATIC_STYLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyleClass))

typedef struct _GtkCssStaticStyleClass      GtkCssStaticStyleClass;

/* inherited */
typedef struct {
  int ref_count;
  GtkCssValue *color;
  GtkCssValue *dpi;
  GtkCssValue *font_size;
  GtkCssValue *icon_theme;
  GtkCssValue *icon_palette;
} GtkCssCoreValues;

typedef struct {
  int ref_count;
  GtkCssValue *background_color;
  GtkCssValue *box_shadow;
  GtkCssValue *background_clip;
  GtkCssValue *background_origin;
  GtkCssValue *background_size;
  GtkCssValue *background_position;
  GtkCssValue *background_repeat;
  GtkCssValue *background_image;
  GtkCssValue *background_blend_mode;
} GtkCssBackgroundValues;

typedef struct {
  int ref_count;
  GtkCssValue *border_top_style;
  GtkCssValue *border_top_width;
  GtkCssValue *border_left_style;
  GtkCssValue *border_left_width;
  GtkCssValue *border_bottom_style;
  GtkCssValue *border_bottom_width;
  GtkCssValue *border_right_style;
  GtkCssValue *border_right_width;
  GtkCssValue *border_top_left_radius;
  GtkCssValue *border_top_right_radius;
  GtkCssValue *border_bottom_right_radius;
  GtkCssValue *border_bottom_left_radius;
  GtkCssValue *border_top_color;
  GtkCssValue *border_right_color;
  GtkCssValue *border_bottom_color;
  GtkCssValue *border_left_color;
  GtkCssValue *border_image_source;
  GtkCssValue *border_image_repeat;
  GtkCssValue *border_image_slice;
  GtkCssValue *border_image_width;
} GtkCssBorderValues;

/* inherited */
typedef struct {
  int ref_count;
  GtkCssValue *icon_size;
  GtkCssValue *icon_shadow;
  GtkCssValue *icon_style;
} GtkCssIconValues;


typedef struct {
  int ref_count;
  GtkCssValue *outline_style;
  GtkCssValue *outline_width;
  GtkCssValue *outline_offset;
  GtkCssValue *outline_top_left_radius;
  GtkCssValue *outline_top_right_radius;
  GtkCssValue *outline_bottom_right_radius;
  GtkCssValue *outline_bottom_left_radius;
  GtkCssValue *outline_color;
} GtkCssOutlineValues;

/* inherited */
typedef struct {
  int ref_count;
  GtkCssValue *font_family;
  GtkCssValue *font_style;
  GtkCssValue *font_weight;
  GtkCssValue *font_stretch;
  GtkCssValue *letter_spacing;
  GtkCssValue *text_shadow;
  GtkCssValue *caret_color;
  GtkCssValue *secondary_caret_color;
  GtkCssValue *font_feature_settings;
  GtkCssValue *font_variation_settings;
} GtkCssFontValues;

typedef struct {
  int ref_count;
  GtkCssValue *text_decoration_line;
  GtkCssValue *text_decoration_color;
  GtkCssValue *text_decoration_style;
  GtkCssValue *font_kerning;
  GtkCssValue *font_variant_ligatures;
  GtkCssValue *font_variant_position;
  GtkCssValue *font_variant_caps;
  GtkCssValue *font_variant_numeric;
  GtkCssValue *font_variant_alternates;
  GtkCssValue *font_variant_east_asian;
} GtkCssFontVariantValues;

typedef struct {
  int ref_count;
  GtkCssValue *animation_name;
  GtkCssValue *animation_duration;
  GtkCssValue *animation_timing_function;
  GtkCssValue *animation_iteration_count;
  GtkCssValue *animation_direction;
  GtkCssValue *animation_play_state;
  GtkCssValue *animation_delay;
  GtkCssValue *animation_fill_mode;
} GtkCssAnimationValues;

typedef struct {
  int ref_count;
  GtkCssValue *transition_property;
  GtkCssValue *transition_duration;
  GtkCssValue *transition_timing_function;
  GtkCssValue *transition_delay;
} GtkCssTransitionValues;

typedef struct {
  int ref_count;
  GtkCssValue *margin_top;
  GtkCssValue *margin_left;
  GtkCssValue *margin_bottom;
  GtkCssValue *margin_right;
  GtkCssValue *padding_top;
  GtkCssValue *padding_left;
  GtkCssValue *padding_bottom;
  GtkCssValue *padding_right;
  GtkCssValue *border_spacing;
  GtkCssValue *min_width;
  GtkCssValue *min_height;
} GtkCssSizeValues;

typedef struct {
  int ref_count;
  GtkCssValue *icon_source;
  GtkCssValue *icon_transform;
  GtkCssValue *icon_filter;
  GtkCssValue *transform;
  GtkCssValue *opacity;
  GtkCssValue *filter;
} GtkCssOtherValues;

struct _GtkCssStaticStyle
{
  GtkCssStyle parent;

  GtkCssCoreValues        *core;
  GtkCssBackgroundValues  *background;
  GtkCssBorderValues      *border;
  GtkCssIconValues        *icon;
  GtkCssOutlineValues     *outline;
  GtkCssFontValues        *font;
  GtkCssFontVariantValues *font_variant;
  GtkCssAnimationValues   *animation;
  GtkCssTransitionValues  *transition;
  GtkCssSizeValues        *size;
  GtkCssOtherValues       *other;

  GPtrArray             *sections;             /* sections the values are defined in */

  GtkCssChange           change;               /* change as returned by value lookup */
};

struct _GtkCssStaticStyleClass
{
  GtkCssStyleClass parent_class;
};

GType                   gtk_css_static_style_get_type           (void) G_GNUC_CONST;

GtkCssStyle *           gtk_css_static_style_get_default        (void);
GtkCssStyle *           gtk_css_static_style_new_compute        (GtkStyleProvider       *provider,
                                                                 const GtkCssMatcher    *matcher,
                                                                 GtkCssStyle            *parent,
                                                                 GtkCssChange            change);

GtkCssChange            gtk_css_static_style_get_change         (GtkCssStaticStyle      *style);

G_END_DECLS

#endif /* __GTK_CSS_STATIC_STYLE_PRIVATE_H__ */
