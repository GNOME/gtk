/* GTK - The GIMP Toolkit
 * Copyright (C) 2018 Red Hat Software
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

#pragma once

#include <glib-object.h>
#include <gdk/gdkdisplay.h>
#include <wayland-client-protocol.h>

G_BEGIN_DECLS

GType         gtk_im_context_wayland_get_type (void) G_GNUC_CONST;

struct wl_proxy *gtk_im_context_wayland_get_text_protocol (GdkDisplay *display);


G_END_DECLS

