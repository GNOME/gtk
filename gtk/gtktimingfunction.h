/* gtktimingfunction.h: Timing functions for animations
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

#pragma once

#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_TIMING_FUNCTION (gtk_timing_function_get_type())

/**
 * GtkTimingFunction:
 *
 * An opaque type representing a timing, or "easing" function.
 */
typedef struct _GtkTimingFunction       GtkTimingFunction;

GDK_AVAILABLE_IN_ALL
GType                           gtk_timing_function_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
gboolean                        gtk_timing_function_equal               (GtkTimingFunction      *a,
                                                                         GtkTimingFunction      *b);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_timing_function_ref                 (GtkTimingFunction      *self);
GDK_AVAILABLE_IN_ALL
void                            gtk_timing_function_unref               (GtkTimingFunction      *self);

GDK_AVAILABLE_IN_ALL
char *                          gtk_timing_function_to_string           (GtkTimingFunction      *self);
GDK_AVAILABLE_IN_ALL
void                            gtk_timing_function_print               (GtkTimingFunction      *self,
                                                                         GString                *buf);
GDK_AVAILABLE_IN_ALL
gboolean                        gtk_timing_function_parse               (const char             *string,
                                                                         GtkTimingFunction     **out_func);
GDK_AVAILABLE_IN_ALL
double                          gtk_timing_function_transform_time      (GtkTimingFunction      *self,
                                                                         double                  elapsed,
                                                                         double                  duration);
GDK_AVAILABLE_IN_ALL
gboolean                        gtk_timing_function_get_control_points  (GtkTimingFunction      *self,
                                                                         double                 *x_1,
                                                                         double                 *y_1,
                                                                         double                 *x_2,
                                                                         double                 *y_2);
GDK_AVAILABLE_IN_ALL
gboolean                        gtk_timing_function_get_steps           (GtkTimingFunction      *self,
                                                                         int                    *n_steps,
                                                                         GtkStepPosition        *position);

GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_linear                              (void);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_cubic_bezier                        (double                  x_1,
                                                                         double                  y_1,
                                                                         double                  x_2,
                                                                         double                  y_2);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_ease                                (void);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_ease_in                             (void);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_ease_out                            (void);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_ease_in_out                         (void);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_steps                               (int                     n_steps,
                                                                         GtkStepPosition         position);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_step_start                          (void);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *             gtk_step_end                            (void);

G_END_DECLS
