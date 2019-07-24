/* gtkanimationprivate.h: Animation type
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

#include "gtkanimation.h"

G_BEGIN_DECLS

struct _GtkAnimationClass
{
  GObjectClass parent_class;

  void (* start)     (GtkAnimation *animation);
  void (* advance)   (GtkAnimation *animation,
                      gint64        frame_time);
  void (* stop)      (GtkAnimation *animation,
                      gboolean      is_finished);
  void (* iteration) (GtkAnimation *animation);
};

void    gtk_animation_advance   (GtkAnimation *animation,
                                 gint64        frame_time);

G_END_DECLS
