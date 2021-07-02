/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 the GTK team
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

#ifndef __GDK_INPUT_WINPOINTER_H__
#define __GDK_INPUT_WINPOINTER_H__

gboolean gdk_winpointer_initialize (void);

void gdk_winpointer_initialize_surface (GdkSurface *surface);
void gdk_winpointer_finalize_surface (GdkSurface *surface);

#endif /* __GDK_INPUT_WINPOINTER_H__ */
