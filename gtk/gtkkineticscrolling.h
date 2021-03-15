/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2014 Lieven van der Heide
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
 */

#ifndef __GTK_KINETIC_SCROLLING_H__
#define __GTK_KINETIC_SCROLLING_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  GTK_KINETIC_SCROLLING_CHANGE_NONE = 0,
  GTK_KINETIC_SCROLLING_CHANGE_LOWER = 1 << 0,
  GTK_KINETIC_SCROLLING_CHANGE_UPPER = 1 << 1,
  GTK_KINETIC_SCROLLING_CHANGE_IN_OVERSHOOT = 1 << 2,
} GtkKineticScrollingChange;

typedef struct _GtkKineticScrolling GtkKineticScrolling;

GtkKineticScrolling *    gtk_kinetic_scrolling_new  (gdouble               lower,
                                                     gdouble               upper,
                                                     gdouble               overshoot_width,
                                                     gdouble               decel_friction,
                                                     gdouble               overshoot_friction,
                                                     gdouble               initial_position,
                                                     gdouble               initial_velocity);
void                     gtk_kinetic_scrolling_free (GtkKineticScrolling  *kinetic);

GtkKineticScrollingChange gtk_kinetic_scrolling_update_size (GtkKineticScrolling *data,
                                                             gdouble              lower,
                                                             gdouble              upper);

gboolean                 gtk_kinetic_scrolling_tick (GtkKineticScrolling  *data,
                                                     gdouble               time_delta,
                                                     gdouble              *position,
                                                     gdouble              *velocity);

G_END_DECLS

#endif /* __GTK_KINETIC_SCROLLING_H__ */
