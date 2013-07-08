/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_WAYLAND_SELECTION_H__
#define __GDK_WAYLAND_SELECTION_H__

#if !defined (__GDKWAYLAND_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkwayland.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#if defined (GTK_COMPILATION) || defined (GDK_COMPILATION)
#define gdk_wayland_device_get_selection_type_atoms gdk_wayland_device_get_selection_type_atoms_libgtk_only
GDK_AVAILABLE_IN_ALL
int
gdk_wayland_device_get_selection_type_atoms (GdkDevice  *device,
                                             GdkAtom   **atoms_out);

typedef void (*GdkDeviceWaylandRequestContentCallback) (GdkDevice *device, const gchar *data, gsize len, gpointer userdata);

#define gdk_wayland_device_request_selection_content gdk_wayland_device_request_selection_content_libgtk_only
GDK_AVAILABLE_IN_ALL
gboolean
gdk_wayland_device_request_selection_content (GdkDevice                              *device,
                                              const gchar                            *requested_mime_type,
                                              GdkDeviceWaylandRequestContentCallback  cb,
                                              gpointer                                userdata);

typedef gchar *(*GdkDeviceWaylandOfferContentCallback) (GdkDevice *device, const gchar *mime_type, gssize *len, gpointer userdata);

#define gdk_wayland_device_offer_selection_content gdk_wayland_device_offer_selection_content_libgtk_only
GDK_AVAILABLE_IN_ALL
gboolean
gdk_wayland_device_offer_selection_content (GdkDevice                             *gdk_device,
                                            const gchar                          **mime_types,
                                            gint                                   nr_mime_types,
                                            GdkDeviceWaylandOfferContentCallback   cb,
                                            gpointer                               userdata);

#define gdk_wayland_device_clear_selection_content gdk_wayland_device_clear_selection_content_libgtk_only
GDK_AVAILABLE_IN_ALL
gboolean
gdk_wayland_device_clear_selection_content (GdkDevice *gdk_device);

#endif

G_END_DECLS

#endif /* __GDK_WAYLAND_SELECTION_H__ */
