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

G_DEFINE_TYPE (GtkCssTransition, _gtk_css_transition, GTK_TYPE_STYLE_ANIMATION)

static void
gtk_css_transition_set_values (GtkStyleAnimation    *animation,
                               gint64                for_time_us,
                               GtkCssComputedValues *values)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (animation);
  GtkCssValue *value, *end;
  double progress;

  end = _gtk_css_computed_values_get_intrinsic_value (values, transition->property);

  if (transition->start_time >= for_time_us)
    value = _gtk_css_value_ref (transition->start);
  else if (transition->end_time > for_time_us)
    {
      progress = (double) (for_time_us - transition->start_time) / (transition->end_time - transition->start_time);
      progress = _gtk_css_ease_value_transform (transition->ease, progress);

      value = _gtk_css_value_transition (transition->start,
                                         end,
                                         transition->property,
                                         progress);
      if (value == NULL)
        value = _gtk_css_value_ref (end);
    }
  else
    value = NULL;

  if (value)
    {
      _gtk_css_computed_values_set_animated_value (values, transition->property, value);
      _gtk_css_value_unref (value);
    }
}

static gboolean
gtk_css_transition_is_finished (GtkStyleAnimation *animation,
                                gint64             at_time_us)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (animation);

  return at_time_us >= transition->end_time;
}

static gboolean
gtk_css_transition_is_static (GtkStyleAnimation *animation,
                              gint64             at_time_us)
{
  GtkCssTransition *transition = GTK_CSS_TRANSITION (animation);

  return at_time_us >= transition->end_time;
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

  animation_class->set_values = gtk_css_transition_set_values;
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
                         gint64       start_time_us,
                         gint64       end_time_us)
{
  GtkCssTransition *transition;

  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (ease != NULL, NULL);
  g_return_val_if_fail (start_time_us <= end_time_us, NULL);

  transition = g_object_new (GTK_TYPE_CSS_TRANSITION, NULL);

  transition->property = property;
  transition->start = _gtk_css_value_ref (start);
  transition->ease = _gtk_css_value_ref (ease);
  transition->start_time = start_time_us;
  transition->end_time = end_time_us;

  return GTK_STYLE_ANIMATION (transition);
}

guint
_gtk_css_transition_get_property (GtkCssTransition *transition)
{
  g_return_val_if_fail (GTK_IS_CSS_TRANSITION (transition), 0);

  return transition->property;
}
