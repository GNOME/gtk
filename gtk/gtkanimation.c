/* gtkanimation.c: An animation
 *
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkanimation
 * @Title: GtkAnimation
 * @Short_description: The base class for animations
 *
 * ...
 */

#include "config.h"

#include "gtkanimationprivate.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

typedef struct {
  double duration;
  double delay;
  GtkAnimationDirection direction;
  int repeat_count;
  gboolean auto_reverse;
  GtkTimingFunction *timing;
} GtkAnimationPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkAnimation, gtk_animation, G_TYPE_OBJECT)

enum {
  PROP_DURATION = 1,
  PROP_DELAY,
  PROP_DIRECTION,
  PROP_REPEAT_COUNT,
  PROP_AUTO_REVERSE,
  PROP_TIMING_FUNCTION,

  N_PROPS
};

static GParamSpec *animation_props[N_PROPS];

static void
gtk_animation_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (gtk_animation_parent_class)->finalize (gobject);
}

static void
gtk_animation_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkAnimation *self = GTK_ANIMATION (gobject);

  switch (prop_id)
    {
    case PROP_DURATION:
      gtk_animation_set_duration (self, g_value_get_double (value));
      break;

    case PROP_DELAY:
      gtk_animation_set_delay (self, g_value_get_double (value));
      break;

    case PROP_DIRECTION:
      gtk_animation_set_direction (self, g_value_get_enum (value));
      break;

    case PROP_REPEAT_COUNT:
      break;

    case PROP_AUTO_REVERSE:
      break;

    case PROP_TIMING_FUNCTION:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_animation_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkAnimation *self = GTK_ANIMATION (gobject);
  GtkAnimationPrivate *priv = gtk_animation_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DURATION:
      break;

    case PROP_DELAY:
      break;

    case PROP_DIRECTION:
      break;

    case PROP_REPEAT_COUNT:
      break;

    case PROP_AUTO_REVERSE:
      break;

    case PROP_TIMING_FUNCTION:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_animation_class_init (GtkAnimationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_animation_set_property:
  gobject_class->get_property = gtk_animation_get_property;
  gobject_class->finalize = gtk_animation_finalize;

  animation_props[PROP_DURATION] =
    g_param_spec_uint64 ("duration",
                         P_("Duration"),
                         P_("The duration of the animation, in milliseconds"),
                         0, G_MAXUINT64, 0,
                         G_PARAM_READ_WRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);
  animation_props[PROP_DELAY] =
    g_param_spec_uint64 ("delay",
                         P_("Delay"),
                         P_("The delay before the animation starts, in milliseconds"),
                         0, G_MAXUINT64, 0,
                         G_PARAM_READ_WRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);
  animation_props[PROP_DIRECTION] =
    g_param_spec_enum ("direction",
                       P_("Direction"),
                       P_("The direction of the animation's progress"),
                       GTK_TYPE_ANIMATION_DIRECTION,
                       GTK_ANIMATION_DIRECTION_FORWARD,
                       G_PARAM_READ_WRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, animation_props);
}

static void
gtk_animation_init (GtkAnimation *self)
{
}

/*< private >
 * gtk_animation_advance:
 * @animation: a #GtkAnimation
 * @frame_time: the frame time, coming from the frame clock
 *
 * Advances the given @animation by @frame_time milliseconds.
 */
void
gtk_animation_advance (GtkAnimation *animation,
                       gint64         frame_time)
{
}

void
gtk_animation_set_duration (GtkAnimation *animation,
                            guint64       milliseconds)
{

}

guint64
gtk_animation_get_duration (GtkAnimation *animation)
{
}

void
gtk_animation_set_delay (GtkAnimation *animation,
                          double         seconds)
{
}

double
gtk_animation_get_delay (GtkAnimation *animation)
{
}

void
gtk_animation_set_direction (GtkAnimation          *animation,
                             GtkAnimationDirection  direction)
{
}

GtkAnimationDirection
gtk_animation_get_direction (GtkAnimation *animation)
{
}

void
gtk_animation_set_repeat_count (GtkAnimation *animation,
                                int           repeats)
{
}

int
gtk_animation_get_repeat_count (GtkAnimation *animation)
{
}

void
gtk_animation_set_auto_reverse (GtkAnimation *animation,
                                gboolean      auto_reverse)
{
}

gboolean
gtk_animation_get_auto_reverse (GtkAnimation *animation)
{
}

void
gtk_animation_set_timing_function (GtkAnimation      *animation,
                                   GtkTimingFunction *function)
{
}

GtkTimingFunction *
gtk_animation_get_timing_function (GtkAnimation *animation)
{
}

double
gtk_animation_get_elapsed_time (GtkAnimation *animation)
{
}

double
gtk_animation_get_progress (GtkAnimation *animation)
{
}

double
gtk_animation_get_total_duration (GtkAnimation *animation)
{
}

int
gtk_animation_get_current_repeat (GtkAnimation *animation)
{
}
