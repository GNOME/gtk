/* GTK+ - accessibility implementations
 * Copyright (C) 2018  Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_WINDOW_ACCESSIBLE_PRIVATE_H__
#define __GTK_WINDOW_ACCESSIBLE_PRIVATE_H__

#include <gtk/a11y/gtkwindowaccessible.h>

G_BEGIN_DECLS

void            _gtk_window_accessible_set_is_active    (GtkWindow              *window,
                                                         gboolean                is_active);

G_END_DECLS

#endif /* __GTK_WINDOW_ACCESSIBLE_PRIVATE_H__ */
