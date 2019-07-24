/* gtkanimation.h: A class representing an animation
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
#include <gtk/gtktimingfunction.h>

G_BEGIN_DECLS

#define GTK_TYPE_ANIMATION (gtk_animation_get_type())

/**
 * GtkAnimation:
 *
 * A base type for representing an animation.
 */
GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkAnimation, gtk_animation, GTK, ANIMATION, GObject)

/**
 * double:
 *
 * A type used to represent elapsed time in seconds.
 */
typedef double double;

/* Properties */
GDK_AVAILABLE_IN_ALL
void                    gtk_animation_set_duration              (GtkAnimation  *animation,
                                                                 double         duration);
GDK_AVAILABLE_IN_ALL
double                  gtk_animation_get_duration              (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
void                    gtk_animation_set_delay                 (GtkAnimation  *animation,
                                                                 double         delay);
GDK_AVAILABLE_IN_ALL
double                  gtk_animation_get_delay                 (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
void                    gtk_animation_set_direction             (GtkAnimation  *animation,
                                                                 GtkAnimationDirection direction);
GDK_AVAILABLE_IN_ALL
GtkAnimationDirection   gtk_animation_get_direction             (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
void                    gtk_animation_set_repeat_count          (GtkAnimation  *animation,
                                                                 int            repeats);
GDK_AVAILABLE_IN_ALL
int                     gtk_animation_get_repeat_count          (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
void                    gtk_animation_set_auto_reverse          (GtkAnimation  *animation,
                                                                 gboolean       auto_reverse);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_animation_get_auto_reverse          (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
void                    gtk_animation_set_timing_function       (GtkAnimation  *animation,
                                                                 GtkTimingFunction *function);
GDK_AVAILABLE_IN_ALL
GtkTimingFunction *     gtk_animation_get_timing_function       (GtkAnimation  *animation);

/* State query */
GDK_AVAILABLE_IN_ALL
double                  gtk_animation_get_elapsed_time          (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
double                  gtk_animation_get_progress              (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
double                  gtk_animation_get_total_duration        (GtkAnimation  *animation);
GDK_AVAILABLE_IN_ALL
int                     gtk_animation_get_current_repeat        (GtkAnimation  *animation);

G_END_DECLS
