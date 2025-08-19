#pragma once

#include "gdkcolorstateprivate.h"
#include "gdkwaylanddisplay.h"
#include <wayland-client.h>

typedef struct _GdkWaylandColor GdkWaylandColor;

GdkWaylandColor * gdk_wayland_color_new                         (GdkWaylandDisplay      *display);

void              gdk_wayland_color_set_color_manager           (GdkWaylandColor        *color,
                                                                 struct wl_registry     *registry,
                                                                 uint32_t                id,
                                                                 uint32_t                version);

void              gdk_wayland_color_free                        (GdkWaylandColor        *color);

gboolean          gdk_wayland_color_prepare                     (GdkWaylandColor        *color);

struct wl_proxy * gdk_wayland_color_get_color_manager           (GdkWaylandColor        *color);

void              gdk_wayland_color_set_color_representation    (GdkWaylandColor        *color,
                                                                 struct wl_registry     *registry,
                                                                 uint32_t                id,
                                                                 uint32_t                version);
struct wl_proxy * gdk_wayland_color_get_color_representation_manager
                                                                (GdkWaylandColor        *color);

typedef struct _GdkWaylandColorSurface GdkWaylandColorSurface;

typedef void      (* GdkColorStateChanged)                      (GdkWaylandColorSurface *self,
                                                                 GdkColorState          *cs,
                                                                 gpointer                data);

GdkWaylandColorSurface *
                  gdk_wayland_color_surface_new                 (GdkWaylandColor        *color,
                                                                 struct wl_surface      *wl_surface,
                                                                 GdkColorStateChanged    callback,
                                                                 gpointer                data);

void              gdk_wayland_color_surface_free                (GdkWaylandColorSurface *self);

gboolean          gdk_wayland_color_surface_can_set_color_state (GdkWaylandColorSurface  *self,
                                                                 GdkColorState           *cs,
                                                                 uint32_t                 fourcc,
                                                                 gboolean                 premultiplied,
                                                                 GError                 **error);
void              gdk_wayland_color_surface_set_color_state     (GdkWaylandColorSurface *self,
                                                                 GdkColorState          *cs,
                                                                 uint32_t                fourcc,
                                                                 gboolean                premultiplied);
void              gdk_wayland_color_surface_unset_color_state   (GdkWaylandColorSurface *self);
