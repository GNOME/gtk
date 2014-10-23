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

#include "gtkstyleanimationprivate.h"

G_DEFINE_ABSTRACT_TYPE (GtkStyleAnimation, _gtk_style_animation, G_TYPE_OBJECT)

static void
gtk_style_animation_real_set_values (GtkStyleAnimation    *animation,
                                     gint64                for_time_us,
                                     GtkCssAnimatedStyle  *style)
{
}

static gboolean
gtk_style_animation_real_is_finished (GtkStyleAnimation *animation,
                                      gint64             at_time_us)
{
  return TRUE;
}

static gboolean
gtk_style_animation_real_is_static (GtkStyleAnimation *animation,
                                    gint64             at_time_us)
{
  return FALSE;
}

static void
_gtk_style_animation_class_init (GtkStyleAnimationClass *klass)
{
  klass->set_values = gtk_style_animation_real_set_values;
  klass->is_finished = gtk_style_animation_real_is_finished;
  klass->is_static = gtk_style_animation_real_is_static;
}

static void
_gtk_style_animation_init (GtkStyleAnimation *animation)
{
}

void
_gtk_style_animation_set_values (GtkStyleAnimation    *animation,
                                 gint64                for_time_us,
                                 GtkCssAnimatedStyle  *style)
{
  GtkStyleAnimationClass *klass;

  g_return_if_fail (GTK_IS_STYLE_ANIMATION (animation));
  g_return_if_fail (GTK_IS_CSS_ANIMATED_STYLE (style));

  klass = GTK_STYLE_ANIMATION_GET_CLASS (animation);

  klass->set_values (animation, for_time_us, style);
}

gboolean
_gtk_style_animation_is_finished (GtkStyleAnimation *animation,
                                  gint64             at_time_us)
{
  GtkStyleAnimationClass *klass;

  g_return_val_if_fail (GTK_IS_STYLE_ANIMATION (animation), TRUE);

  klass = GTK_STYLE_ANIMATION_GET_CLASS (animation);

  return klass->is_finished (animation, at_time_us);
}

/**
 * _gtk_style_animation_is_static:
 * @animation: The animation to query
 * @at_time_us: The timestamp to query for
 *
 * Checks if @animation will not change its values anymore after
 * @at_time_us. This happens for example when the animation has reached its
 * final value or when it has been paused.
 *
 * Returns: %TRUE if @animation will not change anymore after @at_time_us
 **/
gboolean
_gtk_style_animation_is_static (GtkStyleAnimation *animation,
                                gint64             at_time_us)
{
  GtkStyleAnimationClass *klass;

  g_return_val_if_fail (GTK_IS_STYLE_ANIMATION (animation), TRUE);

  klass = GTK_STYLE_ANIMATION_GET_CLASS (animation);

  return klass->is_static (animation, at_time_us);
}
