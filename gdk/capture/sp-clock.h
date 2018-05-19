/* sp-clock.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SP_CLOCK_H
#define SP_CLOCK_H

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

typedef gint SpClock;
typedef gint64 SpTimeStamp;
typedef gint32 SpTimeSpan;

extern SpClock sp_clock;

static inline SpTimeStamp
sp_clock_get_current_time (void)
{
  struct timespec ts;

  clock_gettime (sp_clock, &ts);

  return (ts.tv_sec * G_GINT64_CONSTANT (1000000000)) + ts.tv_nsec;
}

static inline SpTimeSpan
sp_clock_get_relative_time (SpTimeStamp epoch)
{
  return sp_clock_get_current_time () - epoch;
}

void sp_clock_init (void);

G_END_DECLS

#endif /* SP_CLOCK_H */


