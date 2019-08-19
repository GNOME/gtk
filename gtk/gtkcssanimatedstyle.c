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

#include "gtkcssanimatedstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssdynamicprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstaticstyleprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkprivate.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssAnimatedStyle, gtk_css_animated_style, GTK_TYPE_CSS_STYLE)

static GtkCssValue *
gtk_css_animated_style_get_value (GtkCssStyle *style,
                                  guint        id)
{
  /* This is called a lot, so we avoid a dynamic type check here */
  GtkCssAnimatedStyle *animated = (GtkCssAnimatedStyle *) style;

  if (animated->animated_values &&
      id < animated->animated_values->len &&
      g_ptr_array_index (animated->animated_values, id))
    return g_ptr_array_index (animated->animated_values, id);

  return gtk_css_animated_style_get_intrinsic_value (animated, id);
}

static GtkCssSection *
gtk_css_animated_style_get_section (GtkCssStyle *style,
                                    guint        id)
{
  GtkCssAnimatedStyle *animated = GTK_CSS_ANIMATED_STYLE (style);

  return gtk_css_style_get_section (animated->style, id);
}

static gboolean
gtk_css_animated_style_is_static (GtkCssStyle *style)
{
  GtkCssAnimatedStyle *animated = GTK_CSS_ANIMATED_STYLE (style);
  guint i;

  for (i = 0; i < animated->n_animations; i ++)
    {
      if (!_gtk_style_animation_is_static (animated->animations[i]))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_animated_style_dispose (GObject *object)
{
  GtkCssAnimatedStyle *style = GTK_CSS_ANIMATED_STYLE (object);
  guint i;

  if (style->animated_values)
    {
      g_ptr_array_unref (style->animated_values);
      style->animated_values = NULL;
    }

  for (i = 0; i < style->n_animations; i ++)
    g_object_unref (style->animations[i]);

  style->n_animations = 0;
  g_free (style->animations);
  style->animations = NULL;

  G_OBJECT_CLASS (gtk_css_animated_style_parent_class)->dispose (object);
}

static void
gtk_css_animated_style_finalize (GObject *object)
{
  GtkCssAnimatedStyle *style = GTK_CSS_ANIMATED_STYLE (object);

  g_object_unref (style->style);

  G_OBJECT_CLASS (gtk_css_animated_style_parent_class)->finalize (object);
}

static void
gtk_css_animated_style_class_init (GtkCssAnimatedStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_animated_style_dispose;
  object_class->finalize = gtk_css_animated_style_finalize;

  style_class->get_value = gtk_css_animated_style_get_value;
  style_class->get_section = gtk_css_animated_style_get_section;
  style_class->is_static = gtk_css_animated_style_is_static;
}

static void
gtk_css_animated_style_init (GtkCssAnimatedStyle *style)
{
}

void
gtk_css_animated_style_set_animated_value (GtkCssAnimatedStyle *style,
                                           guint                id,
                                           GtkCssValue         *value)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_ANIMATED_STYLE (style));
  gtk_internal_return_if_fail (value != NULL);

  if (style->animated_values == NULL)
    style->animated_values = g_ptr_array_new_with_free_func ((GDestroyNotify)_gtk_css_value_unref);
  if (id >= style->animated_values->len)
    g_ptr_array_set_size (style->animated_values, id + 1);

  if (g_ptr_array_index (style->animated_values, id))
    _gtk_css_value_unref (g_ptr_array_index (style->animated_values, id));
  g_ptr_array_index (style->animated_values, id) = _gtk_css_value_ref (value);

}

GtkCssValue *
gtk_css_animated_style_get_intrinsic_value (GtkCssAnimatedStyle *style,
                                            guint                id)
{
  return gtk_css_style_get_value (style->style, id);
}

static GPtrArray *
gtk_css_animated_style_create_dynamic (GPtrArray   *animations,
                                       GtkCssStyle *style,
                                       gint64       timestamp)
{
  guint i;

  /* XXX: Check animations if they have dynamic values */

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      if (gtk_css_value_is_dynamic (gtk_css_style_get_value (style, i)))
        {
          if (!animations)
            animations = g_ptr_array_new ();

          g_ptr_array_insert (animations, 0, gtk_css_dynamic_new (timestamp));
          break;
        }
    }

  return animations;
}

/* TRANSITIONS */

typedef struct _TransitionInfo TransitionInfo;
struct _TransitionInfo {
  guint index;                  /* index into value arrays */
  gboolean pending;             /* TRUE if we still need to handle it */
};

static void
transition_info_add (TransitionInfo    infos[GTK_CSS_PROPERTY_N_PROPERTIES],
                     GtkStyleProperty *property,
                     guint             index)
{
  if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
    {
      GtkCssShorthandProperty *shorthand = (GtkCssShorthandProperty *) property;
      guint len = _gtk_css_shorthand_property_get_n_subproperties (shorthand);
      guint i;

      for (i = 0; i < len; i++)
        {
          GtkCssStyleProperty *prop = _gtk_css_shorthand_property_get_subproperty (shorthand, i);

          transition_info_add (infos, (GtkStyleProperty *)prop, index);
        }
    }
  else
    {
      guint id;

      if (!_gtk_css_style_property_is_animated ((GtkCssStyleProperty *) property))
        return;

      id = _gtk_css_style_property_get_id ((GtkCssStyleProperty *) property);
      g_assert (id < GTK_CSS_PROPERTY_N_PROPERTIES);
      infos[id].index = index;
      infos[id].pending = TRUE;
    }
}

static void
transition_infos_set (TransitionInfo  infos[GTK_CSS_PROPERTY_N_PROPERTIES],
                      GtkCssValue    *transitions)
{
  guint i;

  for (i = 0; i < _gtk_css_array_value_get_n_values (transitions); i++)
    {
      GtkStyleProperty *property;
      GtkCssValue *prop_value;

      prop_value = _gtk_css_array_value_get_nth (transitions, i);
      if (g_ascii_strcasecmp (_gtk_css_ident_value_get (prop_value), "all") == 0)
        {
          const guint len = _gtk_css_style_property_get_n_properties ();
          guint j;

          for (j = 0; j < len; j++)
            {
              property = (GtkStyleProperty *)_gtk_css_style_property_lookup_by_id (j);
              transition_info_add (infos, property, i);
            }
        }
      else
        {
          property = _gtk_style_property_lookup (_gtk_css_ident_value_get (prop_value));
          if (property)
            transition_info_add (infos, property, i);
        }
    }
}

static GtkStyleAnimation *
gtk_css_animated_style_find_transition (GtkCssAnimatedStyle *style,
                                        guint                property_id)
{
  guint i;

  for (i = 0; i < style->n_animations; i ++)
    {
      GtkStyleAnimation *animation = style->animations[i];

      if (!GTK_IS_CSS_TRANSITION (animation))
        continue;

      if (_gtk_css_transition_get_property ((GtkCssTransition *)animation) == property_id)
        return animation;
    }

  return NULL;
}

static GPtrArray *
gtk_css_animated_style_create_css_transitions (GPtrArray   *animations,
                                               GtkCssStyle *base_style,
                                               gint64       timestamp,
                                               GtkCssStyle *source)
{
  TransitionInfo transitions[GTK_CSS_PROPERTY_N_PROPERTIES] = { { 0, } };
  GtkCssValue *durations, *delays, *timing_functions;
  gboolean source_is_animated;
  guint i;

  durations = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_TRANSITION_DURATION);
  delays = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_TRANSITION_DELAY);
  timing_functions = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION);

  if (_gtk_css_array_value_get_n_values (durations) == 1 &&
      _gtk_css_array_value_get_n_values (delays) == 1 &&
      _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, 0), 100) +
      _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, 0), 100) == 0)
    return animations;

  transition_infos_set (transitions, gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_TRANSITION_PROPERTY));

  source_is_animated = GTK_IS_CSS_ANIMATED_STYLE (source);
  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      GtkStyleAnimation *animation;
      GtkCssValue *start, *end;
      double duration, delay;

      if (!transitions[i].pending)
        continue;

      duration = _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, transitions[i].index), 100);
      delay = _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, transitions[i].index), 100);
      if (duration + delay == 0.0)
        continue;

      if (source_is_animated)
        {
          start = gtk_css_animated_style_get_intrinsic_value ((GtkCssAnimatedStyle *)source, i);
          end = gtk_css_style_get_value (base_style, i);

          if (_gtk_css_value_equal (start, end))
            {
              animation = gtk_css_animated_style_find_transition ((GtkCssAnimatedStyle *)source, i);
              if (animation)
                {
                  animation = _gtk_style_animation_advance (animation, timestamp);
                  if (!animations)
                    animations = g_ptr_array_new ();

                  g_ptr_array_add (animations, animation);
                }

              continue;
            }
        }

      if (_gtk_css_value_equal (gtk_css_style_get_value (source, i),
                                gtk_css_style_get_value (base_style, i)))
        continue;

      animation = _gtk_css_transition_new (i,
                                           gtk_css_style_get_value (source, i),
                                           _gtk_css_array_value_get_nth (timing_functions, i),
                                           timestamp,
                                           duration * G_USEC_PER_SEC,
                                           delay * G_USEC_PER_SEC);
      if (!animations)
        animations = g_ptr_array_new ();

      g_ptr_array_add (animations, animation);
    }

  return animations;
}

static GtkStyleAnimation *
gtk_css_animated_style_find_animation (GtkStyleAnimation **animations,
                                       guint               n_animations,
                                       const char         *name)
{
  guint i;

  for (i = 0; i < n_animations; i ++)
    {
      GtkStyleAnimation *animation = animations[i];

      if (!GTK_IS_CSS_ANIMATION (animation))
        continue;

      if (g_str_equal (_gtk_css_animation_get_name ((GtkCssAnimation *)animation), name))
        return animation;
    }

  return NULL;
}

static GPtrArray *
gtk_css_animated_style_create_css_animations (GPtrArray        *animations,
                                              GtkCssStyle      *base_style,
                                              GtkCssStyle      *parent_style,
                                              gint64            timestamp,
                                              GtkStyleProvider *provider,
                                              GtkCssStyle      *source)
{
  GtkCssValue *durations, *delays, *timing_functions, *animation_names;
  GtkCssValue *iteration_counts, *directions, *play_states, *fill_modes;
  guint i;

  animation_names = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_NAME);
  durations = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_DURATION);
  delays = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_DELAY);
  timing_functions = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION);
  iteration_counts = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT);
  directions = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_DIRECTION);
  play_states = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE);
  fill_modes = gtk_css_style_get_value (base_style, GTK_CSS_PROPERTY_ANIMATION_FILL_MODE);

  for (i = 0; i < _gtk_css_array_value_get_n_values (animation_names); i++)
    {
      GtkStyleAnimation *animation = NULL;
      GtkCssKeyframes *keyframes;
      const char *name;

      name = _gtk_css_ident_value_get (_gtk_css_array_value_get_nth (animation_names, i));
      if (g_ascii_strcasecmp (name, "none") == 0)
        continue;

      if (animations)
        animation = gtk_css_animated_style_find_animation ((GtkStyleAnimation **)animations->pdata, animations->len, name);

      if (animation)
        continue;

      if (GTK_IS_CSS_ANIMATED_STYLE (source))
        animation = gtk_css_animated_style_find_animation ((GtkStyleAnimation **)GTK_CSS_ANIMATED_STYLE (source)->animations,
                                                           GTK_CSS_ANIMATED_STYLE (source)->n_animations,
                                                           name);

      if (animation)
        {
          animation = _gtk_css_animation_advance_with_play_state (GTK_CSS_ANIMATION (animation),
                                                                  timestamp,
                                                                  _gtk_css_play_state_value_get (_gtk_css_array_value_get_nth (play_states, i)));
        }
      else
        {
          keyframes = gtk_style_provider_get_keyframes (provider, name);
          if (keyframes == NULL)
            continue;

          keyframes = _gtk_css_keyframes_compute (keyframes, provider, base_style, parent_style);

          animation = _gtk_css_animation_new (name,
                                              keyframes,
                                              timestamp,
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (delays, i), 100) * G_USEC_PER_SEC,
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (durations, i), 100) * G_USEC_PER_SEC,
                                              _gtk_css_array_value_get_nth (timing_functions, i),
                                              _gtk_css_direction_value_get (_gtk_css_array_value_get_nth (directions, i)),
                                              _gtk_css_play_state_value_get (_gtk_css_array_value_get_nth (play_states, i)),
                                              _gtk_css_fill_mode_value_get (_gtk_css_array_value_get_nth (fill_modes, i)),
                                              _gtk_css_number_value_get (_gtk_css_array_value_get_nth (iteration_counts, i), 100));
          _gtk_css_keyframes_unref (keyframes);
        }

      if (!animations)
        animations = g_ptr_array_new ();

      g_ptr_array_add (animations, animation);
    }

  return animations;
}

/* PUBLIC API */

static void
gtk_css_animated_style_apply_animations (GtkCssAnimatedStyle *style)
{
  guint i;

  for (i = 0; i < style->n_animations; i ++)
    {
      GtkStyleAnimation *animation = style->animations[i];

      _gtk_style_animation_apply_values (animation, style);
    }
}

GtkCssStyle *
gtk_css_animated_style_new (GtkCssStyle      *base_style,
                            GtkCssStyle      *parent_style,
                            gint64            timestamp,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *previous_style)
{
  GtkCssAnimatedStyle *result;
  GPtrArray *animations = NULL;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (base_style), NULL);
  gtk_internal_return_val_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style), NULL);
  gtk_internal_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);
  gtk_internal_return_val_if_fail (previous_style == NULL || GTK_IS_CSS_STYLE (previous_style), NULL);

  if (timestamp == 0)
    return g_object_ref (base_style);

  if (previous_style != NULL)
    animations = gtk_css_animated_style_create_css_transitions (animations, base_style, timestamp, previous_style);

  animations = gtk_css_animated_style_create_css_animations (animations, base_style, parent_style, timestamp, provider, previous_style);
  animations = gtk_css_animated_style_create_dynamic (animations, base_style, timestamp);

  if (animations == NULL)
    return g_object_ref (base_style);

  result = g_object_new (GTK_TYPE_CSS_ANIMATED_STYLE, NULL);

  result->style = g_object_ref (base_style);
  result->current_time = timestamp;
  result->n_animations = animations->len;
  result->animations = g_ptr_array_free (animations, FALSE);

  gtk_css_animated_style_apply_animations (result);

  return GTK_CSS_STYLE (result);
}

GtkCssStyle *
gtk_css_animated_style_new_advance (GtkCssAnimatedStyle *source,
                                    GtkCssStyle         *base,
                                    gint64               timestamp)
{
  GtkCssAnimatedStyle *result;
  GPtrArray *animations;
  guint i;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_ANIMATED_STYLE (source), NULL);
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (base), NULL);

  if (timestamp == 0 || timestamp == source->current_time)
    return g_object_ref (source->style);

  gtk_internal_return_val_if_fail (timestamp > source->current_time, NULL);

  animations = NULL;
  for (i = 0; i < source->n_animations; i ++)
    {
      GtkStyleAnimation *animation = source->animations[i];

      if (_gtk_style_animation_is_finished (animation))
        continue;

      if (!animations)
        animations = g_ptr_array_new ();

      animation = _gtk_style_animation_advance (animation, timestamp);
      g_ptr_array_add (animations, animation);
    }

  if (animations == NULL)
    return g_object_ref (source->style);

  result = g_object_new (GTK_TYPE_CSS_ANIMATED_STYLE, NULL);

  result->style = g_object_ref (base);
  result->current_time = timestamp;
  result->n_animations = animations->len;
  result->animations = g_ptr_array_free (animations, FALSE);

  gtk_css_animated_style_apply_animations (result);

  return GTK_CSS_STYLE (result);
}
