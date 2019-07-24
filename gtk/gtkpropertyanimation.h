/* gtkpropertyanimation.h: An animation controlling a single object property
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

#include <gtk/gtkanimation.h>

G_BEGIN_DECLS

/**
 * GtkPropertyAnimation:
 *
 * A animation that operates on a single property of a #GObject.
 */
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkPropertyAnimation, gtk_property_animation, GTK, PROPERTY_ANIMATION, GtkAnimation)

GDK_AVAILABLE_IN_ALL
GtkAnimation *          gtk_property_animation_new             (const char           *property_name);

/* Interval */
GDK_AVAILABLE_IN_ALL
void                    gtk_property_animation_set_from        (GtkPropertyAnimation *animation,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
void                    gtk_property_animation_set_from_value  (GtkPropertyAnimation *animation,
                                                                 const GValue        *value);
GDK_AVAILABLE_IN_ALL
void                    gtk_property_animation_set_to          (GtkPropertyAnimation *animation,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
void                    gtk_property_animation_set_to_value    (GtkPropertyAnimation *animation,
                                                                 const GValue         *value);

G_END_DECLS
