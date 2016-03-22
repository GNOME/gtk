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
#include "gtkprogresstrackerprivate.h"

#include <math.h>

G_DEFINE_TYPE (GtkCssAnimation, _gtk_css_animation, GTK_TYPE_STYLE_ANIMATION)

static gboolean
gtk_css_animation_is_executing (GtkCssAnimation *animation)
{
  GtkProgressState state = gtk_progress_tracker_get_state (&animation->tracker);

  switch (animation->fill_mode)
    {
    case GTK_CSS_FILL_NONE:
      return state == GTK_PROGRESS_STATE_DURING;
    case GTK_CSS_FILL_FORWARDS:
      return state != GTK_PROGRESS_STATE_BEFORE;
    case GTK_CSS_FILL_BACKWARDS:
      return state != GTK_PROGRESS_STATE_AFTER;
    case GTK_CSS_FILL_BOTH:
      return TRUE;
    default:
      g_return_val_if_reached (FALSE);
    }
}

static double
gtk_css_animation_get_progress (GtkCssAnimation *animation)
{
  gboolean reverse, odd_iteration;
  gint cycle = gtk_progress_tracker_get_iteration_cycle (&animation->tracker);
  odd_iteration = cycle % 2 > 0;

  switch (animation->direction)
    {
    case GTK_CSS_DIRECTION_NORMAL:
      reverse = FALSE;
      break;
    case GTK_CSS_DIRECTION_REVERSE:
      reverse = TRUE;
      break;
    case GTK_CSS_DIRECTION_ALTERNATE:
      reverse = odd_iteration;
      break;
    case GTK_CSS_DIRECTION_ALTERNATE_REVERSE:
      reverse = !odd_iteration;
      break;
    default:
      g_return_val_if_reached (0.0);
    }

  return gtk_progress_tracker_get_progress (&animation->tracker, reverse);
}

GtkStyleAnimation *
gtk_css_animation_advance (GtkStyleAnimation    *style_animation,
                           gint64                timestamp)
{
  GtkCssAnimation *animation = GTK_CSS_ANIMATION (style_animation);

  return _gtk_css_animation_advance_with_play_state (animation,
                                                     timestamp,
                                                     animation->play_state);
}

static void
gtk_css_animation_apply_values (GtkStyleAnimation    *style_animation,
                                GtkCssAnimatedStyle  *style)
{
  GtkCssAnimation *animation = GTK_CSS_ANIMATION (style_animation);
  double progress;
  guint i;

  if (!gtk_css_animation_is_executing (animation))
    return;

  progress = gtk_css_animation_get_progress (animation);
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
gtk_css_animation_is_finished (GtkStyleAnimation *style_animation)
{
  return FALSE;
}

static gboolean
gtk_css_animation_is_static (GtkStyleAnimation *style_animation)
{
  GtkCssAnimation *animation = GTK_CSS_ANIMATION (style_animation);

  if (animation->play_state == GTK_CSS_PLAY_STATE_PAUSED)
    return TRUE;

  return gtk_progress_tracker_get_state (&animation->tracker) == GTK_PROGRESS_STATE_AFTER;
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

  animation_class->advance = gtk_css_animation_advance;
  animation_class->apply_values = gtk_css_animation_apply_values;
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
  animation->ease = _gtk_css_value_ref (ease);
  animation->direction = direction;
  animation->play_state = play_state;
  animation->fill_mode = fill_mode;

  gtk_progress_tracker_start (&animation->tracker, duration_us, delay_us, iteration_count);
  if (animation->play_state == GTK_CSS_PLAY_STATE_PAUSED)
    gtk_progress_tracker_skip_frame (&animation->tracker, timestamp);
  else
    gtk_progress_tracker_advance_frame (&animation->tracker, timestamp);

  return GTK_STYLE_ANIMATION (animation);
}

const char *
_gtk_css_animation_get_name (GtkCssAnimation *animation)
{
  g_return_val_if_fail (GTK_IS_CSS_ANIMATION (animation), NULL);

  return animation->name;
}

GtkStyleAnimation *
_gtk_css_animation_advance_with_play_state (GtkCssAnimation *source,
                                            gint64           timestamp,
                                            GtkCssPlayState  play_state)
{
  GtkCssAnimation *animation;

  g_return_val_if_fail (GTK_IS_CSS_ANIMATION (source), NULL);

  animation = g_object_new (GTK_TYPE_CSS_ANIMATION, NULL);

  animation->name = g_strdup (source->name);
  animation->keyframes = _gtk_css_keyframes_ref (source->keyframes);
  animation->ease = _gtk_css_value_ref (source->ease);
  animation->direction = source->direction;
  animation->play_state = play_state;
  animation->fill_mode = source->fill_mode;

  gtk_progress_tracker_init_copy (&source->tracker, &animation->tracker);
  if (animation->play_state == GTK_CSS_PLAY_STATE_PAUSED)
    gtk_progress_tracker_skip_frame (&animation->tracker, timestamp);
  else
    gtk_progress_tracker_advance_frame (&animation->tracker, timestamp);

  return GTK_STYLE_ANIMATION (animation);
}
