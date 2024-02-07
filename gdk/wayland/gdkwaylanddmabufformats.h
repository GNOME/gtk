/* gdkwaylanddmabufformats.h
 *
 * Copyright 2023 Red Hat, Inc.
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

#if !defined (__GDKWAYLAND_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/wayland/gdkwayland.h> can be included directly."
#endif

#include <gdk/gdkdmabufformats.h>

G_BEGIN_DECLS

GDK_AVAILABLE_IN_4_14
dev_t              gdk_wayland_dmabuf_formats_get_main_device   (GdkDmabufFormats *formats);

GDK_AVAILABLE_IN_4_14
dev_t              gdk_wayland_dmabuf_formats_get_target_device (GdkDmabufFormats *formats,
                                                                 gsize             idx);

GDK_AVAILABLE_IN_4_14
gboolean           gdk_wayland_dmabuf_formats_is_scanout        (GdkDmabufFormats *formats,
                                                                 gsize             idx);

G_END_DECLS
