#ifndef __GDK_DEVICE_WAYLAND_PRIVATE_H__
#define __GDK_DEVICE_WAYLAND_PRIVATE_H__

#include "gdkwaylanddevice.h"

void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkSurface       *surface,
                                double           *win_x,
                                double           *win_y,
                                GdkModifierType  *mask);

#endif
