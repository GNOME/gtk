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
#include "gtkcssvaluesprivate.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkcssdimensionvalueprivate.h"


static void gtk_css_static_style_compute_value (GtkCssStaticStyle    *style,
                                                guint                 id,
                                                GtkCssValue          *specified,
                                                GtkCssSection        *section,
                                                GtkCssComputeContext *context);

#define GET_VALUES(v) (GtkCssValue **)((guint8*)(v) + sizeof (GtkCssValues))

#define DEFINE_VALUES(ENUM, TYPE, NAME) \
void \
gtk_css_## NAME ## _values_compute_changes_and_affects (GtkCssStyle *style1, \
                                                        GtkCssStyle *style2, \
                                                        GtkBitmask    **changes, \
                                                        GtkCssAffects *affects) \
{ \
  GtkCssValue **g1 = GET_VALUES (style1->NAME); \
  GtkCssValue **g2 = GET_VALUES (style2->NAME); \
  GtkCssValue **u1 = GET_VALUES (style1->used); \
  GtkCssValue **u2 = GET_VALUES (style2->used); \
  int i; \
  for (i = 0; i < G_N_ELEMENTS (NAME ## _props); i++) \
    { \
      guint id = NAME ## _props[i]; \
      int j = used_props_map[id]; \
      GtkCssValue *v1 = j < 0 ? g1[i] : u1[j]; \
      GtkCssValue *v2 = j < 0 ? g2[i] : u2[j]; \
      if (!gtk_css_value_equal (v1, v2)) \
        { \
          *changes = _gtk_bitmask_set (*changes, id, TRUE); \
          *affects |= _gtk_css_style_property_get_affects (_gtk_css_style_property_lookup_by_id (id)); \
        } \
    } \
} \
\
static inline void \
gtk_css_ ## NAME ## _values_new_compute (GtkCssStaticStyle    *sstyle, \
                                         GtkCssLookup         *lookup, \
                                         GtkCssComputeContext *context) \
{ \
  GtkCssStyle *style = (GtkCssStyle *)sstyle; \
  int i; \
\
  style->NAME = (GtkCss ## TYPE ## Values *)gtk_css_values_new (GTK_CSS_ ## ENUM ## _VALUES); \
\
  for (i = 0; i < G_N_ELEMENTS (NAME ## _props); i++) \
    { \
      guint id = NAME ## _props[i]; \
      gtk_css_static_style_compute_value (sstyle, \
                                          id, \
                                          lookup->values[id].value, \
                                          lookup->values[id].section, \
                                          context); \
    } \
} \
static GtkBitmask * gtk_css_ ## NAME ## _values_mask; \
static GtkCssValues * gtk_css_ ## NAME ## _initial_values; \
\
static GtkCssValues * gtk_css_ ## NAME ## _create_initial_values (void); \
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

DEFINE_VALUES (CORE, Core, core)
DEFINE_VALUES (BACKGROUND, Background, background)
DEFINE_VALUES (BORDER, Border, border)
DEFINE_VALUES (ICON, Icon, icon)
DEFINE_VALUES (OUTLINE, Outline, outline)
DEFINE_VALUES (FONT, Font, font)
DEFINE_VALUES (FONT_VARIANT, FontVariant, font_variant)
DEFINE_VALUES (ANIMATION, Animation, animation)
DEFINE_VALUES (TRANSITION, Transition, transition)
DEFINE_VALUES (SIZE, Size, size)
DEFINE_VALUES (OTHER, Other, other)

#define VERIFY_MASK(NAME) \
  { \
    GtkBitmask *copy; \
    copy = _gtk_bitmask_intersect (_gtk_bitmask_copy (gtk_css_ ## NAME ## _values_mask), all); \
    g_assert (_gtk_bitmask_equals (copy, gtk_css_ ## NAME ## _values_mask)); \
    _gtk_bitmask_free (copy); \
  } \
 all = _gtk_bitmask_subtract (all, gtk_css_ ## NAME ## _values_mask);
  
/* Verify that every style property is present in one group, and none
 * is present in more than one group.
 */
static void
verify_style_groups (void)
{
  GtkBitmask *all;
  guint id;

  all = _gtk_bitmask_new ();

  for (id = 0; id < GTK_CSS_PROPERTY_N_PROPERTIES; id++)
    all = _gtk_bitmask_set (all, id, TRUE);

  VERIFY_MASK (core);
  VERIFY_MASK (background);
  VERIFY_MASK (border);
  VERIFY_MASK (icon);
  VERIFY_MASK (outline);
  VERIFY_MASK (font);
  VERIFY_MASK (font_variant);
  VERIFY_MASK (animation);
  VERIFY_MASK (transition);
  VERIFY_MASK (size);
  VERIFY_MASK (other);

  g_assert (_gtk_bitmask_is_empty (all));

  _gtk_bitmask_free (all);
}

#undef VERIFY_MASK

static void
verify_used_map (void)
{
  for (guint id = 0; id < GTK_CSS_PROPERTY_N_PROPERTIES; id++)
    {
      if (used_props_map[id] != -1)
        g_assert (used_props[used_props_map[id]] == id);
    }

  for (guint i = 0; i < G_N_ELEMENTS (used_props); i++)
    {
      g_assert (used_props_map[used_props[i]] == i);
    }
}

G_DEFINE_TYPE (GtkCssStaticStyle, gtk_css_static_style, GTK_TYPE_CSS_STYLE)

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

  if (style->sections)
    {
      g_ptr_array_unref (style->sections);
      style->sections = NULL;
    }

  if (style->original_values)
    {
      g_ptr_array_unref (style->original_values);
      style->original_values = NULL;
    }

  G_OBJECT_CLASS (gtk_css_static_style_parent_class)->dispose (object);
}

static GtkCssStaticStyle *
gtk_css_static_style_get_static_style (GtkCssStyle *style)
{
  return (GtkCssStaticStyle *)style;
}

static GtkCssValue *
gtk_css_static_style_get_original_value (GtkCssStyle *style,
                                         guint        id)
{
  GtkCssStaticStyle *sstyle = GTK_CSS_STATIC_STYLE (style);

  if (sstyle->original_values == NULL ||
      id >= sstyle->original_values->len)
    return NULL;

  return g_ptr_array_index (sstyle->original_values, id);
}

static void
gtk_css_static_style_class_init (GtkCssStaticStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_static_style_dispose;

  style_class->get_section = gtk_css_static_style_get_section;
  style_class->get_static_style = gtk_css_static_style_get_static_style;
  style_class->get_original_value = gtk_css_static_style_get_original_value;

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

  verify_style_groups ();
  verify_used_map ();
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

static void
maybe_unref_value (gpointer value)
{
  if (value)
    gtk_css_value_unref (value);
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
gtk_css_static_style_set_value (GtkCssStaticStyle *sstyle,
                                guint              id,
                                GtkCssValue       *value,
                                GtkCssValue       *original_value,
                                GtkCssSection     *section)
{
  GtkCssStyle *style = (GtkCssStyle *)sstyle;

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
    case GTK_CSS_PROPERTY_LINE_HEIGHT:
      gtk_css_take_value (&style->font->line_height, value);
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
    case GTK_CSS_PROPERTY_TEXT_TRANSFORM:
      gtk_css_take_value (&style->font_variant->text_transform, value);
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
    case GTK_CSS_PROPERTY_TRANSFORM_ORIGIN:
      gtk_css_take_value (&style->other->transform_origin, value);
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

  if (sstyle->sections && sstyle->sections->len > id && g_ptr_array_index (sstyle->sections, id))
    {
      gtk_css_section_unref (g_ptr_array_index (sstyle->sections, id));
      g_ptr_array_index (sstyle->sections, id) = NULL;
    }

  if (section)
    {
      if (sstyle->sections == NULL)
        sstyle->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (sstyle->sections->len <= id)
        g_ptr_array_set_size (sstyle->sections, id + 1);
      g_ptr_array_index (sstyle->sections, id) = gtk_css_section_ref (section);
    }

  if (sstyle->original_values && sstyle->original_values->len > id &&
      g_ptr_array_index (sstyle->original_values, id))
    {
      gtk_css_value_unref (g_ptr_array_index (sstyle->original_values, id));
      g_ptr_array_index (sstyle->original_values, id) = NULL;
    }

  if (original_value)
    {
      if (sstyle->original_values == NULL)
        sstyle->original_values = g_ptr_array_new_with_free_func (maybe_unref_value);
      if (sstyle->original_values->len <= id)
        g_ptr_array_set_size (sstyle->original_values, id + 1);
      g_ptr_array_index (sstyle->original_values, id) = gtk_css_value_ref (original_value);
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
      GtkCountingBloomFilter filter = GTK_COUNTING_BLOOM_FILTER_INIT;
      GtkSettings *settings;

      settings = gtk_settings_get_default ();
      default_style = gtk_css_static_style_new_compute (GTK_STYLE_PROVIDER (settings),
                                                        &filter,
                                                        NULL,
                                                        0);
      g_object_set_data_full (G_OBJECT (settings), I_("gtk-default-style"),
                              default_style, clear_default_style);
    }

  return default_style;
}

static GtkCssValues *
gtk_css_core_create_initial_values (void)
{
  return NULL;
}

static GtkCssValues *
gtk_css_background_create_initial_values (void)
{
  GtkCssBackgroundValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssBackgroundValues *)gtk_css_values_new (GTK_CSS_BACKGROUND_INITIAL_VALUES);

  values->background_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_COLOR, &context);
  values->box_shadow = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BOX_SHADOW, &context);
  values->background_clip = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_CLIP, &context);
  values->background_origin = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_ORIGIN, &context);
  values->background_size = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_SIZE, &context);
  values->background_position = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_POSITION, &context);
  values->background_repeat = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_REPEAT, &context);
  values->background_image = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_IMAGE, &context);
  values->background_blend_mode = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_border_create_initial_values (void)
{
  GtkCssBorderValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssBorderValues *)gtk_css_values_new (GTK_CSS_BORDER_INITIAL_VALUES);

  values->border_top_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_STYLE, &context);
  values->border_top_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_WIDTH, &context);
  values->border_left_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_LEFT_STYLE, &context);
  values->border_left_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH, &context);
  values->border_bottom_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE, &context);
  values->border_bottom_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH, &context);
  values->border_right_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE, &context);
  values->border_right_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH, &context);
  values->border_top_left_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS, &context);
  values->border_top_right_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS, &context);
  values->border_bottom_left_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS, &context);
  values->border_bottom_right_radius = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS, &context);
  values->border_top_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_TOP_COLOR, &context);
  values->border_right_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR, &context);
  values->border_bottom_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR, &context);
  values->border_left_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_LEFT_COLOR, &context);
  values->border_image_source = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE, &context);
  values->border_image_repeat = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT, &context);
  values->border_image_slice = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE, &context);
  values->border_image_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_outline_create_initial_values (void)
{
  GtkCssOutlineValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssOutlineValues *)gtk_css_values_new (GTK_CSS_OUTLINE_INITIAL_VALUES);

  values->outline_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_STYLE, &context);
  values->outline_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_WIDTH, &context);
  values->outline_offset = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_OFFSET, &context);
  values->outline_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OUTLINE_COLOR, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_icon_create_initial_values (void)
{
  return NULL;
}

static GtkCssValues *
gtk_css_font_create_initial_values (void)
{
  return NULL;
}

static GtkCssValues *
gtk_css_font_variant_create_initial_values (void)
{
  GtkCssFontVariantValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssFontVariantValues *)gtk_css_values_new (GTK_CSS_FONT_VARIANT_INITIAL_VALUES);

  values->text_decoration_line = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TEXT_DECORATION_LINE, &context);
  values->text_decoration_color = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR, &context);
  values->text_decoration_style = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE, &context);
  values->text_transform = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TEXT_TRANSFORM, &context);
  values->font_kerning = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_KERNING, &context);
  values->font_variant_ligatures = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES, &context);
  values->font_variant_position = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_POSITION, &context);
  values->font_variant_caps = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_CAPS, &context);
  values->font_variant_numeric = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC, &context);
  values->font_variant_alternates = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES, &context);
  values->font_variant_east_asian = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_animation_create_initial_values (void)
{
  GtkCssAnimationValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssAnimationValues *)gtk_css_values_new (GTK_CSS_ANIMATION_INITIAL_VALUES);

  values->animation_name = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_NAME, &context);
  values->animation_duration = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_DURATION, &context);
  values->animation_timing_function = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION, &context);
  values->animation_iteration_count = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT, &context);
  values->animation_direction = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_DIRECTION, &context);
  values->animation_play_state = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE, &context);
  values->animation_delay = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_DELAY, &context);
  values->animation_fill_mode = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ANIMATION_FILL_MODE, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_transition_create_initial_values (void)
{
  GtkCssTransitionValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssTransitionValues *)gtk_css_values_new (GTK_CSS_TRANSITION_INITIAL_VALUES);

  values->transition_property = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_PROPERTY, &context);
  values->transition_duration = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_DURATION, &context);
  values->transition_timing_function = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION, &context);
  values->transition_delay = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSITION_DELAY, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_size_create_initial_values (void)
{
  GtkCssSizeValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssSizeValues *)gtk_css_values_new (GTK_CSS_SIZE_INITIAL_VALUES);

  values->margin_top = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_TOP, &context);
  values->margin_left = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_LEFT, &context);
  values->margin_bottom = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_BOTTOM, &context);
  values->margin_right = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MARGIN_RIGHT, &context);
  values->padding_top = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_TOP, &context);
  values->padding_left = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_LEFT, &context);
  values->padding_bottom = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_BOTTOM, &context);
  values->padding_right = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_PADDING_RIGHT, &context);
  values->border_spacing = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_BORDER_SPACING, &context);
  values->min_width = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MIN_WIDTH, &context);
  values->min_height = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_MIN_HEIGHT, &context);

  return (GtkCssValues *)values;
}

static GtkCssValues *
gtk_css_other_create_initial_values (void)
{
  GtkCssOtherValues *values;
  GtkCssComputeContext context = { NULL, };

  values = (GtkCssOtherValues *)gtk_css_values_new (GTK_CSS_OTHER_INITIAL_VALUES);

  values->icon_source = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ICON_SOURCE, &context);
  values->icon_transform = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ICON_TRANSFORM, &context);
  values->icon_filter = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_ICON_FILTER, &context);
  values->transform = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSFORM, &context);
  values->transform_origin = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_TRANSFORM_ORIGIN, &context);
  values->opacity = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_OPACITY, &context);
  values->filter = _gtk_css_initial_value_new_compute (GTK_CSS_PROPERTY_FILTER, &context);

  return (GtkCssValues *)values;
}

static void
gtk_css_lookup_resolve (GtkCssLookup      *lookup,
                        GtkStyleProvider  *provider,
                        GtkCssStaticStyle *sstyle,
                        GtkCssStyle       *parent_style)
{
  GtkCssStyle *style = (GtkCssStyle *)sstyle;
  GtkCssValue *shorthands[GTK_CSS_SHORTHAND_PROPERTY_N_PROPERTIES] = { NULL, };
  GtkCssComputeContext context = { NULL, };

  gtk_internal_return_if_fail (lookup != NULL);
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));
  gtk_internal_return_if_fail (GTK_IS_CSS_STATIC_STYLE (style));
  gtk_internal_return_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style));

  if (lookup->custom_values)
    {
      GHashTableIter iter;
      gpointer id;
      GtkCssVariableValue *value;

      g_clear_pointer (&style->variables, gtk_css_variable_set_unref);
      style->variables = gtk_css_variable_set_new ();

      g_hash_table_iter_init (&iter, lookup->custom_values);

      while (g_hash_table_iter_next (&iter, &id, (gpointer) &value))
        gtk_css_variable_set_add (style->variables, GPOINTER_TO_INT (id), value);

      gtk_css_variable_set_resolve_cycles (style->variables);

      if (parent_style)
        {
          gtk_css_variable_set_set_parent (style->variables,
                                           parent_style->variables);
        }
    }
  else if (parent_style && parent_style->variables)
    {
      g_clear_pointer (&style->variables, gtk_css_variable_set_unref);
      style->variables = gtk_css_variable_set_ref (parent_style->variables);
    }

  context.provider = provider;
  context.style = (GtkCssStyle *) sstyle;
  context.parent_style = parent_style;
  context.variables = NULL;
  context.shorthands = shorthands;

  if (_gtk_bitmask_is_empty (_gtk_css_lookup_get_set_values (lookup)))
    {
      style->background = (GtkCssBackgroundValues *)gtk_css_values_ref (gtk_css_background_initial_values);
      style->border = (GtkCssBorderValues *)gtk_css_values_ref (gtk_css_border_initial_values);
      style->outline = (GtkCssOutlineValues *)gtk_css_values_ref (gtk_css_outline_initial_values);
      style->font_variant = (GtkCssFontVariantValues *)gtk_css_values_ref (gtk_css_font_variant_initial_values);
      style->animation = (GtkCssAnimationValues *)gtk_css_values_ref (gtk_css_animation_initial_values);
      style->transition = (GtkCssTransitionValues *)gtk_css_values_ref (gtk_css_transition_initial_values);
      style->size = (GtkCssSizeValues *)gtk_css_values_ref (gtk_css_size_initial_values);
      style->other = (GtkCssOtherValues *)gtk_css_values_ref (gtk_css_other_initial_values);

      if (parent_style)
        {
          style->core = (GtkCssCoreValues *)gtk_css_values_ref ((GtkCssValues *)parent_style->core);
          style->icon = (GtkCssIconValues *)gtk_css_values_ref ((GtkCssValues *)parent_style->icon);
          style->font = (GtkCssFontValues *)gtk_css_values_ref ((GtkCssValues *)parent_style->font);
        }
      else
        {
          gtk_css_core_values_new_compute (sstyle, lookup, &context);
          gtk_css_icon_values_new_compute (sstyle, lookup, &context);
          gtk_css_font_values_new_compute (sstyle, lookup, &context);
        }

      goto resolve;
    }

  if (parent_style && gtk_css_core_values_unset (lookup))
    style->core = (GtkCssCoreValues *)gtk_css_values_ref ((GtkCssValues *)parent_style->core);
  else
    gtk_css_core_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_background_values_unset (lookup))
    style->background = (GtkCssBackgroundValues *)gtk_css_values_ref (gtk_css_background_initial_values);
  else
    gtk_css_background_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_border_values_unset (lookup))
    style->border = (GtkCssBorderValues *)gtk_css_values_ref (gtk_css_border_initial_values);
  else
    gtk_css_border_values_new_compute (sstyle, lookup, &context);

  if (parent_style && gtk_css_icon_values_unset (lookup))
    style->icon = (GtkCssIconValues *)gtk_css_values_ref ((GtkCssValues *)parent_style->icon);
  else
    gtk_css_icon_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_outline_values_unset (lookup))
    style->outline = (GtkCssOutlineValues *)gtk_css_values_ref (gtk_css_outline_initial_values);
  else
    gtk_css_outline_values_new_compute (sstyle, lookup, &context);

  if (parent_style && gtk_css_font_values_unset (lookup))
    style->font = (GtkCssFontValues *)gtk_css_values_ref ((GtkCssValues *)parent_style->font);
  else
    gtk_css_font_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_font_variant_values_unset (lookup))
    style->font_variant = (GtkCssFontVariantValues *)gtk_css_values_ref (gtk_css_font_variant_initial_values);
  else
    gtk_css_font_variant_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_animation_values_unset (lookup))
    style->animation = (GtkCssAnimationValues *)gtk_css_values_ref (gtk_css_animation_initial_values);
  else
    gtk_css_animation_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_transition_values_unset (lookup))
    style->transition = (GtkCssTransitionValues *)gtk_css_values_ref (gtk_css_transition_initial_values);
  else
    gtk_css_transition_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_size_values_unset (lookup))
    style->size = (GtkCssSizeValues *)gtk_css_values_ref (gtk_css_size_initial_values);
  else
    gtk_css_size_values_new_compute (sstyle, lookup, &context);

  if (gtk_css_other_values_unset (lookup))
    style->other = (GtkCssOtherValues *)gtk_css_values_ref (gtk_css_other_initial_values);
  else
    gtk_css_other_values_new_compute (sstyle, lookup, &context);

resolve:
  gtk_css_style_resolve_used_values (style, &context);

  for (unsigned int i = 0; i < GTK_CSS_SHORTHAND_PROPERTY_N_PROPERTIES; i++)
    {
      if (shorthands[i])
        gtk_css_value_unref (shorthands[i]);
    }
}

GtkCssStyle *
gtk_css_static_style_new_compute (GtkStyleProvider             *provider,
                                  const GtkCountingBloomFilter *filter,
                                  GtkCssNode                   *node,
                                  GtkCssChange                  change)
{
  GtkCssStaticStyle *result;
  GtkCssLookup lookup;
  GtkCssNode *parent;

  _gtk_css_lookup_init (&lookup);

  if (node)
    gtk_style_provider_lookup (provider,
                               filter,
                               node,
                               &lookup,
                               change == 0 ? &change : NULL);

  result = g_object_new (GTK_TYPE_CSS_STATIC_STYLE, NULL);

  result->change = change;

  if (node)
    parent = gtk_css_node_get_parent (node);
  else
    parent = NULL;

  gtk_css_lookup_resolve (&lookup,
                          provider,
                          result,
                          parent ? gtk_css_node_get_style (parent) : NULL);

  _gtk_css_lookup_destroy (&lookup);

  return GTK_CSS_STYLE (result);
}

G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_TOP_STYLE == GTK_CSS_PROPERTY_BORDER_TOP_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE == GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE == GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_LEFT_STYLE == GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_OUTLINE_STYLE == GTK_CSS_PROPERTY_OUTLINE_WIDTH - 1);

static void
gtk_css_static_style_compute_value (GtkCssStaticStyle    *style,
                                    guint                 id,
                                    GtkCssValue          *specified,
                                    GtkCssSection        *section,
                                    GtkCssComputeContext *context)
{
  GtkCssValue *value, *original_value;
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
            gtk_css_static_style_set_value (style, id, gtk_css_dimension_value_new (0, GTK_CSS_NUMBER), NULL, section);
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
      value = gtk_css_value_compute (specified, id, context);

      if (gtk_css_value_contains_variables (specified))
        original_value = specified;
      else
        original_value = NULL;
    }
  else if (context->parent_style && _gtk_css_style_property_is_inherit (_gtk_css_style_property_lookup_by_id (id)))
    {
      GtkCssValue *parent_original_value;

      /* Just take the style from the parent */
      value = gtk_css_value_ref (gtk_css_style_get_computed_value (context->parent_style, id));

      parent_original_value = gtk_css_style_get_original_value (context->parent_style, id);

      if (parent_original_value)
        original_value = parent_original_value;
      else
        original_value = NULL;
    }
  else
    {
      value = _gtk_css_initial_value_new_compute (id, context);
      original_value = NULL;
    }

  gtk_css_static_style_set_value (style, id, value, original_value, section);
}

GtkCssChange
gtk_css_static_style_get_change (GtkCssStaticStyle *style)
{
  g_return_val_if_fail (GTK_IS_CSS_STATIC_STYLE (style), GTK_CSS_CHANGE_ANY);

  return style->change;
}

void
gtk_css_custom_values_compute_changes_and_affects (GtkCssStyle    *style1,
                                                   GtkCssStyle    *style2,
                                                   GtkBitmask    **changes,
                                                   GtkCssAffects  *affects)
{
  if (gtk_css_variable_set_equal (style1->variables, style2->variables))
    return;

  *changes = _gtk_bitmask_set (*changes, GTK_CSS_PROPERTY_CUSTOM, TRUE);
}
