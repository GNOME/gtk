/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once


void update_popup_layout_state (GdkWaylandPopup *wayland_popup,
                                int              x,
                                int              y,
                                int              width,
                                int              height,
                                GdkPopupLayout  *layout);

void gdk_wayland_surface_configure_popup          (GdkWaylandPopup *popup);
void frame_callback_popup                         (GdkWaylandPopup *popup);
void gdk_wayland_popup_hide_surface               (GdkWaylandPopup *popup);

