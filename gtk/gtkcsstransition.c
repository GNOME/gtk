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

#include "gtkcsstransitionprivate.h"

#include "gtkcsseasevalueprivate.h"
#include "gtkprogresstrackerprivate.h"

G_DEFINE_TYPE (GtkCssTransition, _gtk_css_transition, GTK_TYPE_STYLE_ANIMATION)

static GtkStyleAnimation *
gtk_css_transition_advance (GtkStyleAnimation    *style_animation,
                           gint64                timestamp)
{
  GtkCssTransition *source = GTK_CSS_TRANSITION (style_animation);

  GtkCssTransition *transition;

  transition = g_object_new (GTK_TYPE_CSS_TRANSITION, NULL);

  transition->property = source->property;
  transition->start = _gtk_css_value_ref (source->start);
  transition->ease = _gtk_css_value_ref (source->ease);

  gtk_progress_tracker_init_copy (&source->tracker, &transition->tracker);
  gtk_progress_tracker_advance_frame (&transition->tracker, timestamp);

  return GTK_STYLE_ANIMATION (transition);
}

static void
gtk_css_transition_apply_values (GtkStyleAnimation   *style_animation,
                                 GtkCssAnimatedStyle *style)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (style_animation);
  GtkCssValue *value, *end;
  double progress;
  GtkProgressState state;

  end = gtk_css_animated_style_get_intrinsic_value (style, transition->property);

  state = gtk_progress_tracker_get_state (&transition->tracker);

  if (state == GTK_PROGRESS_STATE_BEFORE)
    value = _gtk_css_value_ref (transition->start);
  else if (state == GTK_PROGRESS_STATE_DURING)
    {
      progress = gtk_progress_tracker_get_progress (&transition->tracker, FALSE);
      progress = _gtk_css_ease_value_transform (transition->ease, progress);

      value = _gtk_css_value_transition (transition->start,
                                         end,
                                         transition->property,
                                         progress);
    }
  else
    return;

  if (value == NULL)
    value = _gtk_css_value_ref (end);

  gtk_css_animated_style_set_animated_value (style, transition->property, value);
  _gtk_css_value_unref (value);
}

static gboolean
gtk_css_transition_is_finished (GtkStyleAnimation *animation)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (animation);

  return gtk_progress_tracker_get_state (&transition->tracker) == GTK_PROGRESS_STATE_AFTER;
}

static gboolean
gtk_css_transition_is_static (GtkStyleAnimation *animation)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (animation);

  return gtk_progress_tracker_get_state (&transition->tracker) == GTK_PROGRESS_STATE_AFTER;
}

static void
gtk_css_transition_finalize (GObject *object)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (object);

  _gtk_css_value_unref (transition->start);
  _gtk_css_value_unref (transition->ease);

  G_OBJECT_CLASS (_gtk_css_transition_parent_class)->finalize (object);
}

static void
_gtk_css_transition_class_init (GtkCssTransitionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStyleAnimationClass *animation_class = GTK_STYLE_ANIMATION_CLASS (klass);

  object_class->finalize = gtk_css_transition_finalize;

  animation_class->advance = gtk_css_transition_advance;
  animation_class->apply_values = gtk_css_transition_apply_values;
  animation_class->is_finished = gtk_css_transition_is_finished;
  animation_class->is_static = gtk_css_transition_is_static;
}

static void
_gtk_css_transition_init (GtkCssTransition *transition)
{
}

GtkStyleAnimation *
_gtk_css_transition_new (guint        property,
                         GtkCssValue *start,
                         GtkCssValue *ease,
                         gint64       timestamp,
                         gint64       duration_us,
                         gint64       delay_us)
{
  GtkCssTransition *transition;

  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (ease != NULL, NULL);

  transition = g_object_new (GTK_TYPE_CSS_TRANSITION, NULL);

  transition->property = property;
  transition->start = _gtk_css_value_ref (start);
  transition->ease = _gtk_css_value_ref (ease);
  gtk_progress_tracker_start (&transition->tracker, duration_us, delay_us, 1.0);
  gtk_progress_tracker_advance_frame (&transition->tracker, timestamp);

  return GTK_STYLE_ANIMATION (transition);
}

guint
_gtk_css_transition_get_property (GtkCssTransition *transition)
{
  g_return_val_if_fail (GTK_IS_CSS_TRANSITION (transition), 0);

  return transition->property;
}
