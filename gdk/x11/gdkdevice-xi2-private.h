#ifndef __GDK_DEVICE_XI2_PRIVATE_H__
#define __GDK_DEVICE_XI2_PRIVATE_H__

void gdk_x11_device_xi2_query_state (GdkDevice        *device,
                                     GdkSurface       *surface,
                                     double           *win_x,
                                     double           *win_y,
                                     GdkModifierType  *mask);
#endif
