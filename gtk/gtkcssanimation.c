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

#include "gtkcssanimationprivate.h"

#include "gtkcsseasevalueprivate.h"

#include <math.h>

G_DEFINE_TYPE (GtkCssAnimation, _gtk_css_animation, GTK_TYPE_STYLE_ANIMATION)

/* NB: Return value can be negative (if animation hasn't started yet) */
static gint64
gtk_css_animation_get_elapsed (GtkCssAnimation *animation,
                               gint64           for_time_us)
{
  if (animation->play_state == GTK_CSS_PLAY_STATE_PAUSED)
    return animation->timestamp;
  else
    return for_time_us - animation->timestamp;
}
/* NB: Return value can be negative and +-Inf */
static double
gtk_css_animation_get_iteration (GtkCssAnimation *animation,
                                 gint64           for_time_us)
{
  return (double) gtk_css_animation_get_elapsed (animation, for_time_us) / animation->duration;
}

static gboolean
gtk_css_animation_is_executing_at_iteration (GtkCssAnimation *animation,
                                             double           iteration)
{
  switch (animation->fill_mode)
    {
    case GTK_CSS_FILL_NONE:
      return iteration >= 0 && iteration <= animation->iteration_count;
    case GTK_CSS_FILL_FORWARDS:
      return iteration >= 0;
    case GTK_CSS_FILL_BACKWARDS:
      return iteration <= animation->iteration_count;
    case GTK_CSS_FILL_BOTH:
      return TRUE;
    default:
      g_return_val_if_reached (FALSE);
    }
}

static double
gtk_css_animation_get_progress_from_iteration (GtkCssAnimation *animation,
                                               double           iteration)
{
  double d;

  iteration = CLAMP (iteration, 0, animation->iteration_count);

  switch (animation->direction)
    {
    case GTK_CSS_DIRECTION_NORMAL:
      if (iteration == animation->iteration_count)
        return 1;
      else
        return iteration - floor (iteration);
    case GTK_CSS_DIRECTION_REVERSE:
      if (iteration == animation->iteration_count)
        return 1;
      else
        return ceil (iteration) - iteration;
    case GTK_CSS_DIRECTION_ALTERNATE:
      d = floor (iteration);
      if (fmod (d, 2))
        return 1 + d - iteration;
      else
        return iteration - d;
    case GTK_CSS_DIRECTION_ALTERNATE_REVERSE:
      d = floor (iteration);
      if (fmod (d, 2))
        return iteration - d;
      else
        return 1 + d - iteration;
    default:
      g_return_val_if_reached (0);
    }
}

static void
gtk_css_animation_set_values (GtkStyleAnimation    *style_animation,
                              gint64                for_time_us,
                              GtkCssAnimatedStyle  *style)
{
  GtkCssAnimation *animation = GTK_CSS_ANIMATION (style_animation);
  double iteration, progress;
  guint i;

  iteration = gtk_css_animation_get_iteration (animation, for_time_us);

  if (!gtk_css_animation_is_executing_at_iteration (animation, iteration))
    return;

  progress = gtk_css_animation_get_progress_from_iteration (animation, iteration);
  progress = _gtk_css_ease_value_transform (animation->ease, progress);
  
  for (i = 0; i < _gtk_css_keyframes_get_n_properties (animation->keyframes); i++)
    {
      GtkCssValue *value;
      guint property_id;
      
      property_id = _gtk_css_keyframes_get_property_id (animation->keyframes, i);

      value = _gtk_css_keyframes_get_value (animation->keyframes,
                                            i,
                                            progress,
                                            gtk_css_animated_style_get_intrinsic_value (style, property_id));
      gtk_css_animated_style_set_animated_value (style, property_id, value);
      _gtk_css_value_unref (value);
    }
}

static gboolean
gtk_css_animation_is_finished (GtkStyleAnimation *style_animation,
                               gint64             at_time_us)
{
  return FALSE;
}

static gboolean
gtk_css_animation_is_static (GtkStyleAnimation *style_animation,
                             gint64             at_time_us)
{
  GtkCssAnimation *animation = GTK_CSS_ANIMATION (style_animation);
  double iteration;

  if (animation->play_state == GTK_CSS_PLAY_STATE_PAUSED)
    return TRUE;

  iteration = gtk_css_animation_get_iteration (animation, at_time_us);

  return iteration >= animation->iteration_count;
}

static void
gtk_css_animation_finalize (GObject *object)
{
  GtkCssAnimation *animation = GTK_CSS_ANIMATION (object);

  g_free (animation->name);
  _gtk_css_keyframes_unref (animation->keyframes);
  _gtk_css_value_unref (animation->ease);

  G_OBJECT_CLASS (_gtk_css_animation_parent_class)->finalize (object);
}

static void
_gtk_css_animation_class_init (GtkCssAnimationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStyleAnimationClass *animation_class = GTK_STYLE_ANIMATION_CLASS (klass);

  object_class->finalize = gtk_css_animation_finalize;

  animation_class->set_values = gtk_css_animation_set_values;
  animation_class->is_finished = gtk_css_animation_is_finished;
  animation_class->is_static = gtk_css_animation_is_static;
}

static void
_gtk_css_animation_init (GtkCssAnimation *animation)
{
}

GtkStyleAnimation *
_gtk_css_animation_new (const char      *name,
                        GtkCssKeyframes *keyframes,
                        gint64           timestamp,
                        gint64           delay_us,
                        gint64           duration_us,
                        GtkCssValue     *ease,
                        GtkCssDirection  direction,
                        GtkCssPlayState  play_state,
                        GtkCssFillMode   fill_mode,
                        double           iteration_count)
{
  GtkCssAnimation *animation;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (keyframes != NULL, NULL);
  g_return_val_if_fail (ease != NULL, NULL);
  g_return_val_if_fail (iteration_count >= 0, NULL);

  animation = g_object_new (GTK_TYPE_CSS_ANIMATION, NULL);

  animation->name = g_strdup (name);
  animation->keyframes = _gtk_css_keyframes_ref (keyframes);
  if (play_state == GTK_CSS_PLAY_STATE_PAUSED)
    animation->timestamp = - delay_us;
  else
    animation->timestamp = timestamp + delay_us;

  animation->duration = duration_us;
  animation->ease = _gtk_css_value_ref (ease);
  animation->direction = direction;
  animation->play_state = play_state;
  animation->fill_mode = fill_mode;
  animation->iteration_count = iteration_count;

  return GTK_STYLE_ANIMATION (animation);
}

const char *
_gtk_css_animation_get_name (GtkCssAnimation *animation)
{
  g_return_val_if_fail (GTK_IS_CSS_ANIMATION (animation), NULL);

  return animation->name;
}

GtkStyleAnimation *
_gtk_css_animation_copy (GtkCssAnimation *animation,
                         gint64           at_time_us,
                         GtkCssPlayState  play_state)
{
  g_return_val_if_fail (GTK_IS_CSS_ANIMATION (animation), NULL);

  if (animation->play_state == play_state)
    return g_object_ref (animation);

  return _gtk_css_animation_new (animation->name,
                                 animation->keyframes,
                                 at_time_us,
                                 - gtk_css_animation_get_elapsed (animation, at_time_us),
                                 animation->duration,
                                 animation->ease,
                                 animation->direction,
                                 play_state,
                                 animation->fill_mode,
                                 animation->iteration_count);
}

