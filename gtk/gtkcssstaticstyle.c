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

#include "config.h"

#include "gtkcssstaticstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkcssdimensionvalueprivate.h"

static void gtk_css_static_style_compute_value (GtkCssStaticStyle *style,
                                                GtkStyleProvider  *provider,
                                                GtkCssStyle       *parent_style,
                                                guint              id,
                                                GtkCssValue       *specified,
                                                GtkCssSection     *section);

static int core_props[] = {
  GTK_CSS_PROPERTY_COLOR,
  GTK_CSS_PROPERTY_DPI,
  GTK_CSS_PROPERTY_FONT_SIZE,
  GTK_CSS_PROPERTY_ICON_THEME,
  GTK_CSS_PROPERTY_ICON_PALETTE
};

static int background_props[] = {
  GTK_CSS_PROPERTY_BACKGROUND_COLOR,
  GTK_CSS_PROPERTY_BOX_SHADOW,
  GTK_CSS_PROPERTY_BACKGROUND_CLIP,
  GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
  GTK_CSS_PROPERTY_BACKGROUND_SIZE,
  GTK_CSS_PROPERTY_BACKGROUND_POSITION,
  GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
  GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
  GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE
};

static int border_props[] = {
  GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
  GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
  GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
  GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
  GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
  GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
  GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
  GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH
};

static int icon_props[] = {
  GTK_CSS_PROPERTY_ICON_SIZE,
  GTK_CSS_PROPERTY_ICON_SHADOW,
  GTK_CSS_PROPERTY_ICON_STYLE,
};

static int outline_props[] = {
  GTK_CSS_PROPERTY_OUTLINE_STYLE,
  GTK_CSS_PROPERTY_OUTLINE_WIDTH,
  GTK_CSS_PROPERTY_OUTLINE_OFFSET,
  GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_COLOR,
};

static int font_props[] = {
  GTK_CSS_PROPERTY_FONT_FAMILY,
  GTK_CSS_PROPERTY_FONT_STYLE,
  GTK_CSS_PROPERTY_FONT_WEIGHT,
  GTK_CSS_PROPERTY_FONT_STRETCH,
  GTK_CSS_PROPERTY_LETTER_SPACING,
  GTK_CSS_PROPERTY_TEXT_SHADOW,  
  GTK_CSS_PROPERTY_CARET_COLOR,
  GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR,
  GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS,
  GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS,
};
static int font_variant_props[] = {
  GTK_CSS_PROPERTY_TEXT_DECORATION_LINE,
  GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR,
  GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE,
  GTK_CSS_PROPERTY_FONT_KERNING,
  GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES,
  GTK_CSS_PROPERTY_FONT_VARIANT_POSITION,
  GTK_CSS_PROPERTY_FONT_VARIANT_CAPS,
  GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC,
  GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES,
  GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN,
};

static int animation_props[] = {
  GTK_CSS_PROPERTY_ANIMATION_NAME,
  GTK_CSS_PROPERTY_ANIMATION_DURATION,
  GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT,
  GTK_CSS_PROPERTY_ANIMATION_DIRECTION,
  GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE,
  GTK_CSS_PROPERTY_ANIMATION_DELAY,
  GTK_CSS_PROPERTY_ANIMATION_FILL_MODE,
};

static int transition_props[] = {
  GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
  GTK_CSS_PROPERTY_TRANSITION_DURATION,
  GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_TRANSITION_DELAY,
};

static int size_props[] = {
  GTK_CSS_PROPERTY_MARGIN_TOP,
  GTK_CSS_PROPERTY_MARGIN_LEFT,
  GTK_CSS_PROPERTY_MARGIN_BOTTOM,
  GTK_CSS_PROPERTY_MARGIN_RIGHT,
  GTK_CSS_PROPERTY_PADDING_TOP,
  GTK_CSS_PROPERTY_PADDING_LEFT,
  GTK_CSS_PROPERTY_PADDING_BOTTOM,
  GTK_CSS_PROPERTY_PADDING_RIGHT,
  GTK_CSS_PROPERTY_BORDER_SPACING,
  GTK_CSS_PROPERTY_MIN_WIDTH,
  GTK_CSS_PROPERTY_MIN_HEIGHT,
};

static int other_props[] = {
  GTK_CSS_PROPERTY_ICON_SOURCE,
  GTK_CSS_PROPERTY_ICON_TRANSFORM,
  GTK_CSS_PROPERTY_ICON_FILTER,
  GTK_CSS_PROPERTY_TRANSFORM,
  GTK_CSS_PROPERTY_OPACITY,
  GTK_CSS_PROPERTY_FILTER,
};


typedef struct {
  int ref_count;
  GtkCssValue *values[0];
} GtkCssValues;

#ifdef STYLE_ACCOUNTING
static int num_lookups;
static int num_empty;
static int num_core;
static int num_background;
static int num_border;
static int num_icon;
static int num_outline;
static int num_font;
static int num_font_variant;
static int num_animation;
static int num_transition;
static int num_size;
static int num_other;

static void
dump_style_counts (void)
{
  g_print ("%d lookups \t%.2f%% empty\n", num_lookups, num_empty * 100 / (double)num_lookups);
  g_print ("core          \t%.2f%% shared\n", (num_lookups - num_core) * 100. / (double)num_lookups);
  g_print ("background    \t%.2f%% shared\n", (num_lookups - num_background) * 100. / (double)num_lookups);
  g_print ("border        \t%.2f%% shared\n", (num_lookups - num_border) * 100. / (double)num_lookups);
  g_print ("icon          \t%.2f%% shared\n", (num_lookups - num_icon) * 100. / (double)num_lookups);
  g_print ("outline       \t%.2f%% shared\n", (num_lookups - num_outline) * 100. / (double)num_lookups);
  g_print ("font          \t%.2f%% shared\n", (num_lookups - num_font) * 100. / (double)num_lookups);
  g_print ("font variant  \t%.2f%% shared\n", (num_lookups - num_font_variant) * 100. / (double)num_lookups);
  g_print ("animation     \t%.2f%% shared\n", (num_lookups - num_animation) * 100. / (double)num_lookups);
  g_print ("transition    \t%.2f%% shared\n", (num_lookups - num_transition) * 100. / (double)num_lookups);
  g_print ("size          \t%.2f%% shared\n", (num_lookups - num_size) * 100. / (double)num_lookups);
  g_print ("other         \t%.2f%% shared\n", (num_lookups - num_other) * 100. / (double)num_lookups);
}

#define init_style_counts() atexit (dump_style_counts);
#define style_accounting_new_style(NAME)  num_ ## NAME ++;
#else
#define init_style_counts()
#define style_accounting_new_style(NAME)
#endif

#define DEFINE_VALUES(TYPE, NAME) \
static inline TYPE * \
gtk_css_ ## NAME ## _values_ref (TYPE *style) \
{ \
  style->ref_count++; \
  return style; \
} \
\
static void \
gtk_css_ ## NAME ## _values_free (TYPE *style) \
{ \
  GtkCssValues *group = (GtkCssValues *)style; \
  int i; \
\
  for (i = 0; i < G_N_ELEMENTS(NAME ## _props); i++) \
    gtk_css_value_unref (group->values[i]); \
\
  g_free (style); \
} \
\
static inline void \
gtk_css_ ## NAME ## _values_unref (TYPE *style) \
{ \
  style->ref_count--; \
  if (style->ref_count == 0) \
    gtk_css_ ## NAME ## _values_free (style); \
} \
\
static inline TYPE * \
gtk_css_ ## NAME ## _values_new (void) \
{ \
  TYPE *style; \
\
  style = g_new0 (TYPE, 1); \
  style->ref_count = 1; \
  style_accounting_new_style (NAME); \
\
  return style; \
} \
\
static inline void \
gtk_css_ ## NAME ## _values_new_compute (GtkCssStaticStyle *style, \
                                         GtkStyleProvider *provider, \
                                         GtkCssStyle *parent_style, \
                                         GtkCssLookup *lookup) \
{ \
  int i; \
\
  style->NAME = gtk_css_ ## NAME ## _values_new (); \
\
  for (i = 0; i < G_N_ELEMENTS (NAME ## _props); i++) \
    { \
      guint id = NAME ## _props[i]; \
      gtk_css_static_style_compute_value (style, \
                                          provider, \
                                          parent_style, \
                                          id, \
                                          lookup->values[id].value, \
                                          lookup->values[id].section); \
    } \
} \
\
static GtkBitmask * gtk_css_ ## NAME ## _values_mask; \
static TYPE * gtk_css_ ## NAME ## _initial_values; \
\
static TYPE * gtk_css_ ## NAME ## _create_initial_values (void); \
\
static void \
gtk_css_ ## NAME ## _values_init (void) \
{ \
  int i; \
  gtk_css_ ## NAME ## _values_mask = _gtk_bitmask_new (); \
  for (i = 0; i < G_N_ELEMENTS(NAME ## _props); i++) \
    { \
      guint id = NAME ## _props[i]; \
      gtk_css_ ## NAME ## _values_mask = _gtk_bitmask_set (gtk_css_ ## NAME ## _values_mask, id, TRUE); \
    } \
\
  gtk_css_ ## NAME ## _initial_values = gtk_css_ ## NAME ## _create_initial_values (); \
} \
\
static inline gboolean \
gtk_css_ ## NAME ## _values_unset (const GtkCssLookup *lookup) \
{ \
  const GtkBitmask *set_values = _gtk_css_lookup_get_set_values (lookup); \
  return !_gtk_bitmask_intersects (set_values, gtk_css_ ## NAME ## _values_mask); \
}

DEFINE_VALUES (GtkCssCoreValues, core)
DEFINE_VALUES (GtkCssBackgroundValues, background)
DEFINE_VALUES (GtkCssBorderValues, border)
DEFINE_VALUES (GtkCssIconValues, icon)
DEFINE_VALUES (GtkCssOutlineValues, outline)
DEFINE_VALUES (GtkCssFontValues, font)
DEFINE_VALUES (GtkCssFontVariantValues, font_variant)
DEFINE_VALUES (GtkCssAnimationValues, animation)
DEFINE_VALUES (GtkCssTransitionValues, transition)
DEFINE_VALUES (GtkCssSizeValues, size)
DEFINE_VALUES (GtkCssOtherValues, other)

G_DEFINE_TYPE (GtkCssStaticStyle, gtk_css_static_style, GTK_TYPE_CSS_STYLE)

static GtkCssValue *
gtk_css_static_style_get_value (GtkCssStyle *style,
                                guint        id)
{
  /* This is called a lot, so we avoid a dynamic type check here */
  GtkCssStaticStyle *sstyle = (GtkCssStaticStyle *) style;

  switch (id)
    {
    case GTK_CSS_PROPERTY_COLOR:
      return sstyle->core->color;
    case GTK_CSS_PROPERTY_DPI:
      return sstyle->core->dpi;
    case GTK_CSS_PROPERTY_FONT_SIZE:
      return sstyle->core->font_size;
    case GTK_CSS_PROPERTY_ICON_THEME:
      return sstyle->core->icon_theme;
    case GTK_CSS_PROPERTY_ICON_PALETTE:
      return sstyle->core->icon_palette;
    case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      return sstyle->background->background_color;
    case GTK_CSS_PROPERTY_FONT_FAMILY:
      return sstyle->font->font_family;
    case GTK_CSS_PROPERTY_FONT_STYLE:
      return sstyle->font->font_style;
    case GTK_CSS_PROPERTY_FONT_WEIGHT:
      return sstyle->font->font_weight;
    case GTK_CSS_PROPERTY_FONT_STRETCH:
      return sstyle->font->font_stretch;
    case GTK_CSS_PROPERTY_LETTER_SPACING:
      return sstyle->font->letter_spacing;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_LINE:
      return sstyle->font_variant->text_decoration_line;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR:
      return sstyle->font_variant->text_decoration_color ? sstyle->font_variant->text_decoration_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE:
      return sstyle->font_variant->text_decoration_style;
    case GTK_CSS_PROPERTY_FONT_KERNING:
      return sstyle->font_variant->font_kerning;
    case GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES:
      return sstyle->font_variant->font_variant_ligatures;
    case GTK_CSS_PROPERTY_FONT_VARIANT_POSITION:
      return sstyle->font_variant->font_variant_position;
    case GTK_CSS_PROPERTY_FONT_VARIANT_CAPS:
      return sstyle->font_variant->font_variant_caps;
    case GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC:
      return sstyle->font_variant->font_variant_numeric;
    case GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES:
      return sstyle->font_variant->font_variant_alternates;
    case GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN:
      return sstyle->font_variant->font_variant_east_asian;
    case GTK_CSS_PROPERTY_TEXT_SHADOW:
      return sstyle->font->text_shadow;
    case GTK_CSS_PROPERTY_BOX_SHADOW:
      return sstyle->background->box_shadow;
    case GTK_CSS_PROPERTY_MARGIN_TOP:
      return sstyle->size->margin_top;
    case GTK_CSS_PROPERTY_MARGIN_LEFT:
      return sstyle->size->margin_left;
    case GTK_CSS_PROPERTY_MARGIN_BOTTOM:
      return sstyle->size->margin_bottom;
    case GTK_CSS_PROPERTY_MARGIN_RIGHT:
      return sstyle->size->margin_right;
    case GTK_CSS_PROPERTY_PADDING_TOP:
      return sstyle->size->padding_top;
    case GTK_CSS_PROPERTY_PADDING_LEFT:
      return sstyle->size->padding_left;
    case GTK_CSS_PROPERTY_PADDING_BOTTOM:
      return sstyle->size->padding_bottom;
    case GTK_CSS_PROPERTY_PADDING_RIGHT:
      return sstyle->size->padding_right;
    case GTK_CSS_PROPERTY_BORDER_TOP_STYLE:
      return sstyle->border->border_top_style;
    case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
      return sstyle->border->border_top_width;
    case GTK_CSS_PROPERTY_BORDER_LEFT_STYLE:
      return sstyle->border->border_left_style;
    case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
      return sstyle->border->border_left_width;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE:
      return sstyle->border->border_bottom_style;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
      return sstyle->border->border_bottom_width;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE:
      return sstyle->border->border_right_style;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
      return sstyle->border->border_right_width;
    case GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS:
      return sstyle->border->border_top_left_radius;
    case GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS:
      return sstyle->border->border_top_right_radius;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS:
      return sstyle->border->border_bottom_right_radius;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS:
      return sstyle->border->border_bottom_left_radius;
    case GTK_CSS_PROPERTY_OUTLINE_STYLE:
      return sstyle->outline->outline_style;
    case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
      return sstyle->outline->outline_width;
    case GTK_CSS_PROPERTY_OUTLINE_OFFSET:
      return sstyle->outline->outline_offset;
    case GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS:
      return sstyle->outline->outline_top_left_radius;
    case GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS:
      return sstyle->outline->outline_top_right_radius;
    case GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS:
      return sstyle->outline->outline_bottom_right_radius;
    case GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS:
      return sstyle->outline->outline_bottom_left_radius;
    case GTK_CSS_PROPERTY_BACKGROUND_CLIP:
      return sstyle->background->background_clip;
    case GTK_CSS_PROPERTY_BACKGROUND_ORIGIN:
      return sstyle->background->background_origin;
    case GTK_CSS_PROPERTY_BACKGROUND_SIZE:
      return sstyle->background->background_size;
    case GTK_CSS_PROPERTY_BACKGROUND_POSITION:
      return sstyle->background->background_position;
    case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      return sstyle->border->border_top_color ? sstyle->border->border_top_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      return sstyle->border->border_right_color ? sstyle->border->border_right_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      return sstyle->border->border_bottom_color ? sstyle->border->border_bottom_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      return sstyle->border->border_left_color ? sstyle->border->border_left_color: sstyle->core->color;
    case GTK_CSS_PROPERTY_OUTLINE_COLOR:
      return sstyle->outline->outline_color ? sstyle->outline->outline_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_BACKGROUND_REPEAT:
      return sstyle->background->background_repeat;
    case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      return sstyle->background->background_image;
    case GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE:
      return sstyle->background->background_blend_mode;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      return sstyle->border->border_image_source;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT:
      return sstyle->border->border_image_repeat;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE:
      return sstyle->border->border_image_slice;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH:
      return sstyle->border->border_image_width;
    case GTK_CSS_PROPERTY_ICON_SOURCE:
      return sstyle->other->icon_source;
    case GTK_CSS_PROPERTY_ICON_SIZE:
      return sstyle->icon->icon_size;
    case GTK_CSS_PROPERTY_ICON_SHADOW:
      return sstyle->icon->icon_shadow;
    case GTK_CSS_PROPERTY_ICON_STYLE:
      return sstyle->icon->icon_style;
    case GTK_CSS_PROPERTY_ICON_TRANSFORM:
      return sstyle->other->icon_transform;
    case GTK_CSS_PROPERTY_ICON_FILTER:
      return sstyle->other->icon_filter;
    case GTK_CSS_PROPERTY_BORDER_SPACING:
      return sstyle->size->border_spacing;
    case GTK_CSS_PROPERTY_TRANSFORM:
      return sstyle->other->transform;
    case GTK_CSS_PROPERTY_MIN_WIDTH:
      return sstyle->size->min_width;
    case GTK_CSS_PROPERTY_MIN_HEIGHT:
      return sstyle->size->min_height;
    case GTK_CSS_PROPERTY_TRANSITION_PROPERTY:
      return sstyle->transition->transition_property;
    case GTK_CSS_PROPERTY_TRANSITION_DURATION:
      return sstyle->transition->transition_duration;
    case GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION:
      return sstyle->transition->transition_timing_function;
    case GTK_CSS_PROPERTY_TRANSITION_DELAY:
      return sstyle->transition->transition_delay;
    case GTK_CSS_PROPERTY_ANIMATION_NAME:
      return sstyle->animation->animation_name;
    case GTK_CSS_PROPERTY_ANIMATION_DURATION:
      return sstyle->animation->animation_duration;
    case GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION:
      return sstyle->animation->animation_timing_function;
    case GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT:
      return sstyle->animation->animation_iteration_count;
    case GTK_CSS_PROPERTY_ANIMATION_DIRECTION:
      return sstyle->animation->animation_direction;
    case GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE:
      return sstyle->animation->animation_play_state;
    case GTK_CSS_PROPERTY_ANIMATION_DELAY:
      return sstyle->animation->animation_delay;
    case GTK_CSS_PROPERTY_ANIMATION_FILL_MODE:
      return sstyle->animation->animation_fill_mode;
    case GTK_CSS_PROPERTY_OPACITY:
      return sstyle->other->opacity;
    case GTK_CSS_PROPERTY_FILTER:
      return sstyle->other->filter;
    case GTK_CSS_PROPERTY_CARET_COLOR:
      return sstyle->font->caret_color ? sstyle->font->caret_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR:
      return sstyle->font->secondary_caret_color ? sstyle->font->secondary_caret_color : sstyle->core->color;
    case GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS:
      return sstyle->font->font_feature_settings;
    case GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS:
      return sstyle->font->font_variation_settings;

    default:
      g_assert_not_reached ();
    }
}

static GtkCssSection *
gtk_css_static_style_get_section (GtkCssStyle *style,
                                    guint        id)
{
  GtkCssStaticStyle *sstyle = GTK_CSS_STATIC_STYLE (style);

  if (sstyle->sections == NULL ||
      id >= sstyle->sections->len)
    return NULL;

  return g_ptr_array_index (sstyle->sections, id);
}

static void
gtk_css_static_style_dispose (GObject *object)
{
  GtkCssStaticStyle *style = GTK_CSS_STATIC_STYLE (object);

  gtk_css_core_values_unref (style->core);
  gtk_css_background_values_unref (style->background);
  gtk_css_border_values_unref (style->border);
  gtk_css_icon_values_unref (style->icon);
  gtk_css_outline_values_unref (style->outline);
  gtk_css_font_values_unref (style->font);
  gtk_css_font_variant_values_unref (style->font_variant);
  gtk_css_animation_values_unref (style->animation);
  gtk_css_transition_values_unref (style->transition);
  gtk_css_size_values_unref (style->size);
  gtk_css_other_values_unref (style->other);
 
  if (style->sections)
    {
      g_ptr_array_unref (style->sections);
      style->sections = NULL;
    }

  G_OBJECT_CLASS (gtk_css_static_style_parent_class)->dispose (object);
}

static GtkCssStaticStyle *
gtk_css_static_style_get_static_style (GtkCssStyle *style)
{
  return (GtkCssStaticStyle *)style;
}

static void
gtk_css_static_style_class_init (GtkCssStaticStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_static_style_dispose;

  style_class->get_value = gtk_css_static_style_get_value;
  style_class->get_section = gtk_css_static_style_get_section;
  style_class->get_static_style = gtk_css_static_style_get_static_style;

  gtk_css_core_values_init ();
  gtk_css_background_values_init ();
  gtk_css_border_values_init ();
  gtk_css_icon_values_init ();
  gtk_css_outline_values_init ();
  gtk_css_font_values_init ();
  gtk_css_font_variant_values_init ();
  gtk_css_animation_values_init ();
  gtk_css_transition_values_init ();
  gtk_css_size_values_init ();
  gtk_css_other_values_init ();

  init_style_counts ();
}

static void
gtk_css_static_style_init (GtkCssStaticStyle *style)
{
}

static void
maybe_unref_section (gpointer section)
{
  if (section)
    gtk_css_section_unref (section);
}

static inline void
gtk_css_take_value (GtkCssValue **variable,
                   GtkCssValue  *value)
{
  if (*variable)
    gtk_css_value_unref (*variable);
  *variable = value;
}

static void
gtk_css_static_style_set_value (GtkCssStaticStyle *style,
                                guint              id,
                                GtkCssValue       *value,
                                GtkCssSection     *section)
{
  switch (id)
    {
    case GTK_CSS_PROPERTY_COLOR:
      gtk_css_take_value (&style->core->color, value);
      break;
    case GTK_CSS_PROPERTY_DPI:
      gtk_css_take_value (&style->core->dpi, value);
      break;
    case GTK_CSS_PROPERTY_FONT_SIZE:
      gtk_css_take_value (&style->core->font_size, value);
      break;
    case GTK_CSS_PROPERTY_ICON_THEME:
      gtk_css_take_value (&style->core->icon_theme, value);
      break;
    case GTK_CSS_PROPERTY_ICON_PALETTE:
      gtk_css_take_value (&style->core->icon_palette, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      gtk_css_take_value (&style->background->background_color, value);
      break;
    case GTK_CSS_PROPERTY_FONT_FAMILY:
      gtk_css_take_value (&style->font->font_family, value);
      break;
    case GTK_CSS_PROPERTY_FONT_STYLE:
      gtk_css_take_value (&style->font->font_style, value);
      break;
    case GTK_CSS_PROPERTY_FONT_WEIGHT:
      gtk_css_take_value (&style->font->font_weight, value);
      break;
    case GTK_CSS_PROPERTY_FONT_STRETCH:
      gtk_css_take_value (&style->font->font_stretch, value);
      break;
    case GTK_CSS_PROPERTY_LETTER_SPACING:
      gtk_css_take_value (&style->font->letter_spacing, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_LINE:
      gtk_css_take_value (&style->font_variant->text_decoration_line, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR:
      gtk_css_take_value (&style->font_variant->text_decoration_color, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE:
      gtk_css_take_value (&style->font_variant->text_decoration_style, value);
      break;
    case GTK_CSS_PROPERTY_FONT_KERNING:
      gtk_css_take_value (&style->font_variant->font_kerning, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES:
      gtk_css_take_value (&style->font_variant->font_variant_ligatures, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_POSITION:
      gtk_css_take_value (&style->font_variant->font_variant_position, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_CAPS:
      gtk_css_take_value (&style->font_variant->font_variant_caps, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC:
      gtk_css_take_value (&style->font_variant->font_variant_numeric, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES:
      gtk_css_take_value (&style->font_variant->font_variant_alternates, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN:
      gtk_css_take_value (&style->font_variant->font_variant_east_asian, value);
      break;
    case GTK_CSS_PROPERTY_TEXT_SHADOW:
      gtk_css_take_value (&style->font->text_shadow, value);
      break;
    case GTK_CSS_PROPERTY_BOX_SHADOW:
      gtk_css_take_value (&style->background->box_shadow, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_TOP:
      gtk_css_take_value (&style->size->margin_top, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_LEFT:
      gtk_css_take_value (&style->size->margin_left, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_BOTTOM:
      gtk_css_take_value (&style->size->margin_bottom, value);
      break;
    case GTK_CSS_PROPERTY_MARGIN_RIGHT:
      gtk_css_take_value (&style->size->margin_right, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_TOP:
      gtk_css_take_value (&style->size->padding_top, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_LEFT:
      gtk_css_take_value (&style->size->padding_left, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_BOTTOM:
      gtk_css_take_value (&style->size->padding_bottom, value);
      break;
    case GTK_CSS_PROPERTY_PADDING_RIGHT:
      gtk_css_take_value (&style->size->padding_right, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_STYLE:
      gtk_css_take_value (&style->border->border_top_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
      gtk_css_take_value (&style->border->border_top_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_LEFT_STYLE:
      gtk_css_take_value (&style->border->border_left_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
      gtk_css_take_value (&style->border->border_left_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE:
      gtk_css_take_value (&style->border->border_bottom_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
      gtk_css_take_value (&style->border->border_bottom_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE:
      gtk_css_take_value (&style->border->border_right_style, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
      gtk_css_take_value (&style->border->border_right_width, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS:
      gtk_css_take_value (&style->border->border_top_left_radius, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS:
      gtk_css_take_value (&style->border->border_top_right_radius, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS:
      gtk_css_take_value (&style->border->border_bottom_right_radius, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS:
      gtk_css_take_value (&style->border->border_bottom_left_radius, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_STYLE:
      gtk_css_take_value (&style->outline->outline_style, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
      gtk_css_take_value (&style->outline->outline_width, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_OFFSET:
      gtk_css_take_value (&style->outline->outline_offset, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS:
      gtk_css_take_value (&style->outline->outline_top_left_radius, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS:
      gtk_css_take_value (&style->outline->outline_top_right_radius, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS:
      gtk_css_take_value (&style->outline->outline_bottom_right_radius, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS:
      gtk_css_take_value (&style->outline->outline_bottom_left_radius, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_CLIP:
      gtk_css_take_value (&style->background->background_clip, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_ORIGIN:
      gtk_css_take_value (&style->background->background_origin, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_SIZE:
      gtk_css_take_value (&style->background->background_size, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_POSITION:
      gtk_css_take_value (&style->background->background_position, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      gtk_css_take_value (&style->border->border_top_color, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      gtk_css_take_value (&style->border->border_right_color, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      gtk_css_take_value (&style->border->border_bottom_color, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      gtk_css_take_value (&style->border->border_left_color, value);
      break;
    case GTK_CSS_PROPERTY_OUTLINE_COLOR:
      gtk_css_take_value (&style->outline->outline_color, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_REPEAT:
      gtk_css_take_value (&style->background->background_repeat, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      gtk_css_take_value (&style->background->background_image, value);
      break;
    case GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE:
      gtk_css_take_value (&style->background->background_blend_mode, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      gtk_css_take_value (&style->border->border_image_source, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT:
      gtk_css_take_value (&style->border->border_image_repeat, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE:
      gtk_css_take_value (&style->border->border_image_slice, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH:
      gtk_css_take_value (&style->border->border_image_width, value);
      break;
    case GTK_CSS_PROPERTY_ICON_SOURCE:
      gtk_css_take_value (&style->other->icon_source, value);
      break;
    case GTK_CSS_PROPERTY_ICON_SIZE:
      gtk_css_take_value (&style->icon->icon_size, value);
      break;
    case GTK_CSS_PROPERTY_ICON_SHADOW:
      gtk_css_take_value (&style->icon->icon_shadow, value);
      break;
    case GTK_CSS_PROPERTY_ICON_STYLE:
      gtk_css_take_value (&style->icon->icon_style, value);
      break;
    case GTK_CSS_PROPERTY_ICON_TRANSFORM:
      gtk_css_take_value (&style->other->icon_transform, value);
      break;
    case GTK_CSS_PROPERTY_ICON_FILTER:
      gtk_css_take_value (&style->other->icon_filter, value);
      break;
    case GTK_CSS_PROPERTY_BORDER_SPACING:
      gtk_css_take_value (&style->size->border_spacing, value);
      break;
    case GTK_CSS_PROPERTY_TRANSFORM:
      gtk_css_take_value (&style->other->transform, value);
      break;
    case GTK_CSS_PROPERTY_MIN_WIDTH:
      gtk_css_take_value (&style->size->min_width, value);
      break;
    case GTK_CSS_PROPERTY_MIN_HEIGHT:
      gtk_css_take_value (&style->size->min_height, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_PROPERTY:
      gtk_css_take_value (&style->transition->transition_property, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_DURATION:
      gtk_css_take_value (&style->transition->transition_duration, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION:
      gtk_css_take_value (&style->transition->transition_timing_function, value);
      break;
    case GTK_CSS_PROPERTY_TRANSITION_DELAY:
      gtk_css_take_value (&style->transition->transition_delay, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_NAME:
      gtk_css_take_value (&style->animation->animation_name, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_DURATION:
      gtk_css_take_value (&style->animation->animation_duration, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION:
      gtk_css_take_value (&style->animation->animation_timing_function, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT:
      gtk_css_take_value (&style->animation->animation_iteration_count, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_DIRECTION:
      gtk_css_take_value (&style->animation->animation_direction, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE:
      gtk_css_take_value (&style->animation->animation_play_state, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_DELAY:
      gtk_css_take_value (&style->animation->animation_delay, value);
      break;
    case GTK_CSS_PROPERTY_ANIMATION_FILL_MODE:
      gtk_css_take_value (&style->animation->animation_fill_mode, value);
      break;
    case GTK_CSS_PROPERTY_OPACITY:
      gtk_css_take_value (&style->other->opacity, value);
      break;
    case GTK_CSS_PROPERTY_FILTER:
      gtk_css_take_value (&style->other->filter, value);
      break;
    case GTK_CSS_PROPERTY_CARET_COLOR:
      gtk_css_take_value (&style->font->caret_color, value);
      break;
    case GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR:
      gtk_css_take_value (&style->font->secondary_caret_color, value);
      break;
    case GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS:
      gtk_css_take_value (&style->font->font_feature_settings, value);
      break;
    case GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS:
      gtk_css_take_value (&style->font->font_variation_settings, value);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (style->sections && style->sections->len > id && g_ptr_array_index (style->sections, id))
    {
      gtk_css_section_unref (g_ptr_array_index (style->sections, id));
      g_ptr_array_index (style->sections, id) = NULL;
    }

  if (section)
    {
      if (style->sections == NULL)
        style->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (style->sections->len <= id)
        g_ptr_array_set_size (style->sections, id + 1);
      g_ptr_array_index (style->sections, id) = gtk_css_section_ref (section);
    }
}

static GtkCssStyle *default_style;

static void
clear_default_style (gpointer data)
{
  g_set_object (&default_style, NULL);
}

GtkCssStyle *
gtk_css_static_style_get_default (void)
{
  /* FIXME: This really depends on the screen, but we don't have
   * a screen at hand when we call this function, and in practice,
   * the default style is always replaced by something else
   * before we use it.
   */
  if (default_style == NULL)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_default ();
      default_style = gtk_css_static_style_new_compute (GTK_STYLE_PROVIDER (settings),
                                                        NULL,
                                                        NULL,
                                                        TRUE);
      g_object_set_data_full (G_OBJECT (settings), I_("gtk-default-style"),
                              default_style, clear_default_style);
    }

  return default_style;
}

static GtkCssCoreValues *
gtk_css_core_create_initial_values (void)
{
  return NULL;

}
static GtkCssBackgroundValues *
gtk_css_background_create_initial_values (void)
{
  GtkCssBackgroundValues *style;

  /* none of the initial values depend on the context */
  style = gtk_css_background_values_new ();
  style->background_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_COLOR, NULL, NULL, NULL);
  style->box_shadow = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BOX_SHADOW, NULL, NULL, NULL);
  style->background_clip = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_CLIP, NULL, NULL, NULL);
  style->background_origin = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_ORIGIN, NULL, NULL, NULL);
  style->background_size = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_SIZE, NULL, NULL, NULL);
  style->background_position = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_POSITION, NULL, NULL, NULL);
  style->background_repeat = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_REPEAT, NULL, NULL, NULL);
  style->background_image = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_IMAGE, NULL, NULL, NULL);
  style->background_blend_mode = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE, NULL, NULL, NULL);

  return style;
}

static GtkCssBorderValues *
gtk_css_border_create_initial_values (void)
{
  GtkCssBorderValues *border;

  border = gtk_css_border_values_new ();

  border->border_top_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_STYLE, NULL, NULL, NULL);
  border->border_top_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_WIDTH, NULL, NULL, NULL);
  border->border_left_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_LEFT_STYLE, NULL, NULL, NULL);
  border->border_left_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH, NULL, NULL, NULL);
  border->border_bottom_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE, NULL, NULL, NULL);
  border->border_bottom_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH, NULL, NULL, NULL);
  border->border_right_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE, NULL, NULL, NULL);
  border->border_right_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH, NULL, NULL, NULL);
  border->border_top_left_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS, NULL, NULL, NULL);
  border->border_top_right_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS, NULL, NULL, NULL);
  border->border_bottom_left_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS, NULL, NULL, NULL);
  border->border_bottom_right_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS, NULL, NULL, NULL);
  border->border_top_color = NULL;
  border->border_right_color = NULL;
  border->border_bottom_color = NULL;
  border->border_left_color = NULL;
  border->border_image_source = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE, NULL, NULL, NULL);
  border->border_image_repeat = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT, NULL, NULL, NULL);
  border->border_image_slice = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE, NULL, NULL, NULL);
  border->border_image_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH, NULL, NULL, NULL);

  return border;
}

static GtkCssOutlineValues *
gtk_css_outline_create_initial_values (void)
{
  GtkCssOutlineValues *values;

  values = gtk_css_outline_values_new ();

  values->outline_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_STYLE, NULL, NULL, NULL);
  values->outline_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_WIDTH, NULL, NULL, NULL);
  values->outline_offset = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_OFFSET, NULL, NULL, NULL);
  values->outline_top_left_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS, NULL, NULL, NULL);
  values->outline_top_right_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS, NULL, NULL, NULL);
  values->outline_bottom_right_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS, NULL, NULL, NULL);
  values->outline_bottom_left_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS, NULL, NULL, NULL);
  values->outline_color = NULL;

  return values;
}

static GtkCssIconValues *
gtk_css_icon_create_initial_values (void)
{
  return NULL;
}

static GtkCssFontValues *
gtk_css_font_create_initial_values (void)
{
  return NULL;
}

static GtkCssFontVariantValues *
gtk_css_font_variant_create_initial_values (void)
{
  GtkCssFontVariantValues *values;

  values = gtk_css_font_variant_values_new ();

  values->text_decoration_line = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TEXT_DECORATION_LINE, NULL, NULL, NULL);
  values->text_decoration_color = NULL;
  values->text_decoration_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE, NULL, NULL, NULL);
  values->font_kerning = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_KERNING, NULL, NULL, NULL);
  values->font_variant_ligatures = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES, NULL, NULL, NULL);
  values->font_variant_position = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_POSITION, NULL, NULL, NULL);
  values->font_variant_caps = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_CAPS, NULL, NULL, NULL);
  values->font_variant_numeric = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC, NULL, NULL, NULL);
  values->font_variant_alternates = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES, NULL, NULL, NULL);
  values->font_variant_east_asian = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN, NULL, NULL, NULL);

  return values;
}

static GtkCssAnimationValues *
gtk_css_animation_create_initial_values (void)
{
  GtkCssAnimationValues *values;

  values = gtk_css_animation_values_new ();

  values->animation_name = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_NAME, NULL, NULL, NULL);
  values->animation_duration = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_DURATION, NULL, NULL, NULL);
  values->animation_timing_function = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION, NULL, NULL, NULL);
  values->animation_iteration_count = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT, NULL, NULL, NULL);
  values->animation_direction = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_DIRECTION, NULL, NULL, NULL);
  values->animation_play_state = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE, NULL, NULL, NULL);
  values->animation_delay = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_DELAY, NULL, NULL, NULL);
  values->animation_fill_mode = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_FILL_MODE, NULL, NULL, NULL);

  return values;
}

static GtkCssTransitionValues *
gtk_css_transition_create_initial_values (void)
{
  GtkCssTransitionValues *values;

  values = gtk_css_transition_values_new ();

  values->transition_property = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_PROPERTY, NULL, NULL, NULL);
  values->transition_duration = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_DURATION, NULL, NULL, NULL);
  values->transition_timing_function = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION, NULL, NULL, NULL);
  values->transition_delay = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_DELAY, NULL, NULL, NULL);

  return values;
}

static GtkCssSizeValues *
gtk_css_size_create_initial_values (void)
{
  GtkCssSizeValues *values;

  values = gtk_css_size_values_new ();

  values->margin_top = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_TOP, NULL, NULL, NULL);
  values->margin_left = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_LEFT, NULL, NULL, NULL);
  values->margin_bottom = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_BOTTOM, NULL, NULL, NULL);
  values->margin_right = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_RIGHT, NULL, NULL, NULL);
  values->padding_top = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_TOP, NULL, NULL, NULL);
  values->padding_left = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_LEFT, NULL, NULL, NULL);
  values->padding_bottom = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_BOTTOM, NULL, NULL, NULL);
  values->padding_right = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_RIGHT, NULL, NULL, NULL);
  values->border_spacing = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_SPACING, NULL, NULL, NULL);
  values->min_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MIN_WIDTH, NULL, NULL, NULL);
  values->min_height = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MIN_HEIGHT, NULL, NULL, NULL);

  return values;
}

static GtkCssOtherValues *
gtk_css_other_create_initial_values (void)
{
  GtkCssOtherValues *values;

  values = gtk_css_other_values_new ();

  values->icon_source = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ICON_SOURCE, NULL, NULL, NULL);
  values->icon_transform = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ICON_TRANSFORM, NULL, NULL, NULL);
  values->icon_filter = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ICON_FILTER, NULL, NULL, NULL);
  values->transform = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSFORM, NULL, NULL, NULL);
  values->opacity = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OPACITY, NULL, NULL, NULL);
  values->filter = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FILTER, NULL, NULL, NULL);

  return values;
}

static void
gtk_css_lookup_resolve (GtkCssLookup      *lookup,
                        GtkStyleProvider  *provider,
                        GtkCssStaticStyle *style,
                        GtkCssStyle       *parent_style)
{
  gboolean parent_is_static;

  gtk_internal_return_if_fail (lookup != NULL);
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  gtk_internal_return_if_fail (GTK_IS_CSS_STATIC_STYLE (style));
  gtk_internal_return_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style));

  parent_is_static = GTK_IS_CSS_STATIC_STYLE (parent_style);

#ifdef STYLE_ACCOUNTING
  num_lookups++;
#endif

  if (_gtk_bitmask_is_empty (_gtk_css_lookup_get_set_values (lookup)))
    {

#ifdef STYLE_ACCOUNTING
      num_empty++;
#endif

      style->background = gtk_css_background_values_ref (gtk_css_background_initial_values);
      style->border = gtk_css_border_values_ref (gtk_css_border_initial_values);
      style->outline = gtk_css_outline_values_ref (gtk_css_outline_initial_values);
      style->font_variant = gtk_css_font_variant_values_ref (gtk_css_font_variant_initial_values);
      style->animation = gtk_css_animation_values_ref (gtk_css_animation_initial_values);
      style->transition = gtk_css_transition_values_ref (gtk_css_transition_initial_values);
      style->size = gtk_css_size_values_ref (gtk_css_size_initial_values);
      style->other = gtk_css_other_values_ref (gtk_css_other_initial_values);

      if (parent_is_static)
        {
          style->core = gtk_css_core_values_ref (GTK_CSS_STATIC_STYLE (parent_style)->core);
          style->icon = gtk_css_icon_values_ref (GTK_CSS_STATIC_STYLE (parent_style)->icon);
          style->font = gtk_css_font_values_ref (GTK_CSS_STATIC_STYLE (parent_style)->font);
        }
      else
        {
          gtk_css_core_values_new_compute (style, provider, parent_style, lookup);
          gtk_css_icon_values_new_compute (style, provider, parent_style, lookup);
          gtk_css_font_values_new_compute (style, provider, parent_style, lookup);
        }

      return;
    }

  if (parent_is_static && gtk_css_core_values_unset (lookup))
    style->core = gtk_css_core_values_ref (GTK_CSS_STATIC_STYLE (parent_style)->core);
  else
    gtk_css_core_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_background_values_unset (lookup))
    style->background = gtk_css_background_values_ref (gtk_css_background_initial_values);
  else
    gtk_css_background_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_border_values_unset (lookup))
    style->border = gtk_css_border_values_ref (gtk_css_border_initial_values);
  else
    gtk_css_border_values_new_compute (style, provider, parent_style, lookup);

  if (parent_is_static && gtk_css_icon_values_unset (lookup))
    style->icon = gtk_css_icon_values_ref (GTK_CSS_STATIC_STYLE (parent_style)->icon);
  else
    gtk_css_icon_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_outline_values_unset (lookup))
    style->outline = gtk_css_outline_values_ref (gtk_css_outline_initial_values);
  else
    gtk_css_outline_values_new_compute (style, provider, parent_style, lookup);

  if (parent_is_static && gtk_css_font_values_unset (lookup))
    style->font = gtk_css_font_values_ref (GTK_CSS_STATIC_STYLE (parent_style)->font);
  else
    gtk_css_font_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_font_variant_values_unset (lookup))
    style->font_variant = gtk_css_font_variant_values_ref (gtk_css_font_variant_initial_values);
  else
    gtk_css_font_variant_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_animation_values_unset (lookup))
    style->animation = gtk_css_animation_values_ref (gtk_css_animation_initial_values);
  else
    gtk_css_animation_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_transition_values_unset (lookup))
    style->transition = gtk_css_transition_values_ref (gtk_css_transition_initial_values);
  else
    gtk_css_transition_values_new_compute (style, provider, parent_style, lookup);
  if (gtk_css_size_values_unset (lookup))
    style->size = gtk_css_size_values_ref (gtk_css_size_initial_values);
  else
    gtk_css_size_values_new_compute (style, provider, parent_style, lookup);

  if (gtk_css_other_values_unset (lookup))
    style->other = gtk_css_other_values_ref (gtk_css_other_initial_values);
  else
    gtk_css_other_values_new_compute (style, provider, parent_style, lookup);
}

GtkCssStyle *
gtk_css_static_style_new_compute (GtkStyleProvider    *provider,
                                  const GtkCssMatcher *matcher,
                                  GtkCssStyle         *parent,
                                  GtkCssChange         change)
{
  GtkCssStaticStyle *result;
  GtkCssLookup lookup;

  _gtk_css_lookup_init (&lookup);

  if (matcher)
    gtk_style_provider_lookup (provider,
                               matcher,
                               &lookup,
                               change == 0 ? &change : NULL);

  result = g_object_new (GTK_TYPE_CSS_STATIC_STYLE, NULL);

  result->change = change;

  gtk_css_lookup_resolve (&lookup,
                          provider,
                          result,
                          parent);

  _gtk_css_lookup_destroy (&lookup);

  return GTK_CSS_STYLE (result);
}

G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_TOP_STYLE == GTK_CSS_PROPERTY_BORDER_TOP_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE == GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE == GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_LEFT_STYLE == GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_OUTLINE_STYLE == GTK_CSS_PROPERTY_OUTLINE_WIDTH - 1);

static void
gtk_css_static_style_compute_value (GtkCssStaticStyle *style,
                                    GtkStyleProvider  *provider,
                                    GtkCssStyle       *parent_style,
                                    guint              id,
                                    GtkCssValue       *specified,
                                    GtkCssSection     *section)
{
  GtkCssValue *value;
  GtkBorderStyle border_style;

  gtk_internal_return_if_fail (id < GTK_CSS_PROPERTY_N_PROPERTIES);

  /* special case according to http://dev.w3.org/csswg/css-backgrounds/#the-border-width */
  switch (id)
    {
      /* We have them ordered in gtkcssstylepropertyimpl.c accordingly, so the
       * border styles are already computed when we compute the border widths.
       *
       * Note that we rely on ..._STYLE == ..._WIDTH - 1 here.
       */
      case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
      case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
      case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
      case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
      case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
        border_style = _gtk_css_border_style_value_get (gtk_css_style_get_value ((GtkCssStyle *)style, id - 1));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          {
            gtk_css_static_style_set_value (style, id, gtk_css_dimension_value_new (0, GTK_CSS_NUMBER), section);
            return;
          }
        break;

      default:
        /* Go ahead */
        break;
    }

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified)
    {
      value = _gtk_css_value_compute (specified, id, provider, (GtkCssStyle *)style, parent_style);
    }
  else if (parent_style && _gtk_css_style_property_is_inherit (_gtk_css_style_property_lookup_by_id (id)))
    {
      /* Just take the style from the parent */
      value = _gtk_css_value_ref (gtk_css_style_get_value (parent_style, id));
    }
  else
    {
      value = _gtk_css_initial_value_new_compute (id, provider, (GtkCssStyle *)style, parent_style);
    }

  gtk_css_static_style_set_value (style, id, value, section);
}

GtkCssChange
gtk_css_static_style_get_change (GtkCssStaticStyle *style)
{
  g_return_val_if_fail (GTK_IS_CSS_STATIC_STYLE (style), GTK_CSS_CHANGE_ANY);

  return style->change;
}
