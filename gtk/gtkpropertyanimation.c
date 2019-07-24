/* gtkpropertyanimation.c: An animation controlling a single property
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

#include "gtkpropertyanimation.h"

#include "gtktransitionprivate.h"

enum
{
  INITIAL,
  FINAL,
  N_STATES
};

struct _GtkPropertyAnimation
{
  GtkTransition parent_instance;

  GValue interval[N_STATES];
};

G_DEFINE_TYPE (GtkPropertyAnimation, gtk_property_animation, GTK_TYPE_ANIMATION)

static void
gtk_property_animation_class_init (GtkPropertyAnimationClass *klass)
{
}

static void
gtk_property_animation_init (GtkPropertyAnimation *self)
{
}
