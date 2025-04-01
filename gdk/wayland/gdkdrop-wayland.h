/*
 * Copyright © 2025 Red Hat, Inc
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

#pragma once

#include "gdkdevice.h"
#include "gdkdrag.h"
#include "gdkdrop.h"
#include "gdkcontentformats.h"

#include <wayland-client.h>

/* the magic mime type we use for local DND operations.
 * We offer it to every dnd operation, but will strip it out on the drop
 * site unless we can prove it's a local DND - then we will use only
 * this type
 */
#define GDK_WAYLAND_LOCAL_DND_MIME_TYPE "application/x-gtk-local-dnd"

GdkDrop *        gdk_wayland_drop_new                      (GdkDevice             *device,
                                                            GdkDrag               *drag,
                                                            GdkContentFormats     *formats,
                                                            GdkSurface            *surface,
                                                            struct wl_data_offer  *offer,
                                                            uint32_t               serial);
void             gdk_wayland_drop_set_source_actions       (GdkDrop               *drop,
                                                            uint32_t               source_actions);
void             gdk_wayland_drop_set_action               (GdkDrop               *drop,
                                                            uint32_t               action);
