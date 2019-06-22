/*
 * Copyright © 2018 Benjamin Otte
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

#ifndef __GTK_SHORTCUT_CONTROLLER_PRIVATE_H__
#define __GTK_SHORTCUT_CONTROLLER_PRIVATE_H__

#include "gtkshortcutcontroller.h"

void                    gtk_shortcut_controller_set_run_class           (GtkShortcutController  *controller,
                                                                         gboolean                run_class);

#endif /* __GTK_SHORTCUT_CONTROLLER_H__ */
