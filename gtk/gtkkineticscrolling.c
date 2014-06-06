/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Lieven van der Heide
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gtkkineticscrolling.h"

#include <stdio.h>

#include "fallback-c89.c"

/*
 * All our curves are second degree linear differential equations, and
 * so they can always be written as linear combinations of 2 base
 * solutions. c1 and c2 are the coefficients to these two base solutions,
 * and are computed from the initial position and velocity.
 *
 * In the case of simple deceleration, the differential equation is
 *
 *   y'' = -my'
 *
 * With m the resistence factor. For this we use the following 2
 * base solutions:
 *
 *   f1(x) = 1
 *   f2(x) = exp(-mx)
 *
 * In the case of overshoot, the differential equation is
 *
 *   y'' = -my' - ky
 *
 * With m the resistance, and k the spring stiffness constant. We let
 * k = m^2 / 4, so that the system is critically damped (ie, returns to its
 * equilibrium position as quickly as possible, without oscillating), and offset
 * the whole thing, such that the equilibrium position is at 0. This gives the
 * base solutions
 *
 *   f1(x) = exp(-mx / 2)
 *   f2(x) = t exp(-mx / 2)
*/

typedef enum {
  GTK_KINETIC_SCROLLING_PHASE_DECELERATING,
  GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING,
  GTK_KINETIC_SCROLLING_PHASE_FINISHED,
} GtkKineticScrollingPhase;

struct _GtkKineticScrolling
{
  GtkKineticScrollingPhase phase;
  gdouble lower;
  gdouble upper;
  gdouble overshoot_width;
  gdouble decel_friction;
  gdouble overshoot_friction;

  gdouble c1;
  gdouble c2;
  gdouble equilibrium_position;

  gdouble t;
  gdouble position;
  gdouble velocity;
};

static void gtk_kinetic_scrolling_init_overshoot (GtkKineticScrolling *data,
                                                  gdouble              equilibrium_position,
                                                  gdouble              initial_position,
                                                  gdouble              initial_velocity);

GtkKineticScrolling *
gtk_kinetic_scrolling_new (gdouble lower,
                           gdouble upper,
                           gdouble overshoot_width,
                           gdouble decel_friction,
                           gdouble overshoot_friction,
                           gdouble initial_position,
                           gdouble initial_velocity)
{
  GtkKineticScrolling *data;

  data = g_slice_new0 (GtkKineticScrolling);
  data->lower = lower;
  data->upper = upper;
  data->decel_friction = decel_friction;
  data->overshoot_friction = overshoot_friction;
  if(initial_position < lower)
    {
      gtk_kinetic_scrolling_init_overshoot (data,
                                            lower,
                                            initial_position,
                                            initial_velocity);
    }
  else if(initial_position > upper)
    {
      gtk_kinetic_scrolling_init_overshoot (data,
                                            upper,
                                            initial_position,
                                            initial_velocity);
    }
  else
    {
      data->phase = GTK_KINETIC_SCROLLING_PHASE_DECELERATING;
      data->c1 = initial_velocity / decel_friction + initial_position;
      data->c2 = -initial_velocity / decel_friction;
      data->t = 0;
      data->position = initial_position;
      data->velocity = initial_velocity;
    }

  return data;
}

void
gtk_kinetic_scrolling_free (GtkKineticScrolling *kinetic)
{
  g_slice_free (GtkKineticScrolling, kinetic);
}

static void
gtk_kinetic_scrolling_init_overshoot (GtkKineticScrolling *data,
                                      gdouble              equilibrium_position,
                                      gdouble              initial_position,
                                      gdouble              initial_velocity)
{
  data->phase = GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING;
  data->equilibrium_position = equilibrium_position;
  data->c1 = initial_position - equilibrium_position;
  data->c2 = initial_velocity + data->overshoot_friction / 2 * data->c1;
  data->t = 0;
}

gboolean
gtk_kinetic_scrolling_tick (GtkKineticScrolling *data,
                            gdouble              time_delta,
                            gdouble             *position)
{
  switch(data->phase)
    {
    case GTK_KINETIC_SCROLLING_PHASE_DECELERATING:
      {
        gdouble exp_part;

        data->t += time_delta;

        exp_part = exp (-data->decel_friction * data->t);
        data->position = data->c1 + data->c2 * exp_part;
        data->velocity = -data->decel_friction * data->c2 * exp_part;

        if(data->position < data->lower)
          {
            gtk_kinetic_scrolling_init_overshoot(data,data->lower,data->position,data->velocity);
          }
        else if(data->position > data->upper)
          {
            gtk_kinetic_scrolling_init_overshoot(data, data->upper, data->position, data->velocity);
          }
        else if(fabs(data->velocity) < 1)
          {
            data->phase = GTK_KINETIC_SCROLLING_PHASE_FINISHED;
            data->position = round(data->position);
            data->velocity = 0;
          }
        break;
      }

    case GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING:
      {
        gdouble exp_part, position;

        data->t += time_delta;
        exp_part = exp(-data->overshoot_friction / 2 * data->t);
        position = exp_part * (data->c1 + data->c2 * data->t);

        if (position < data->lower - 50 || position > data->upper + 50)
          {
            position = CLAMP (position, data->lower - 50, data->upper + 50);
            gtk_kinetic_scrolling_init_overshoot (data, data->equilibrium_position, position, 0);
          }
        else
          data->velocity = data->c2 * exp_part - data->overshoot_friction / 2 * position;

        data->position = position + data->equilibrium_position;

        if(fabs(position) < 0.1)
          {
            data->phase = GTK_KINETIC_SCROLLING_PHASE_FINISHED;
            data->position = data->equilibrium_position;
            data->velocity = 0;
          }
        break;
      }

    case GTK_KINETIC_SCROLLING_PHASE_FINISHED:
      break;
    }

  if (position)
    *position = data->position;

  return data->phase != GTK_KINETIC_SCROLLING_PHASE_FINISHED;
}

