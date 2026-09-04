#pragma once

#include "gdksubsurfaceprivate.h"

#include "gdkwaylandcolor-private.h"
#include "gdkdisplay-wayland.h"
#include "wayland-client-protocol.h"


#define GDK_TYPE_WAYLAND_SUBSURFACE              (gdk_wayland_subsurface_get_type ())
GDK_DECLARE_INTERNAL_TYPE (GdkWaylandSubsurface, gdk_wayland_subsurface, GDK, WAYLAND_SUBSURFACE, GdkSubsurface)

struct _GdkWaylandSubsurface
{
  GdkSubsurface parent_instance;

  struct wl_surface *surface;
  struct wl_subsurface *subsurface;
  struct wp_viewport *viewport;
  GdkWaylandColorSurface *color;

  GdkTexture *texture;
  uint32_t fourcc;
  gboolean premultiplied;
  cairo_rectangle_int_t dest;
  graphene_rect_t source;
  enum wl_output_transform transform;

  struct wl_region *opaque_region;

  struct wl_surface *bg_surface;
  struct wl_subsurface *bg_subsurface;
  struct wp_viewport *bg_viewport;
  cairo_rectangle_int_t bg_rect;
  gboolean bg_attached;

  struct zwp_idle_inhibitor_v1 *idle_inhibitor;
};

struct _GdkWaylandSubsurfaceClass
{
  GdkSubsurfaceClass parent_class;
};

void gdk_wayland_subsurface_request_frame        (GdkSubsurface *subsurface);
void gdk_wayland_subsurface_clear_frame_callback (GdkSubsurface *subsurface);

GdkSubsurface * gdk_wayland_surface_create_subsurface (GdkSurface *surface);

gboolean gdk_wayland_subsurface_inhibit_idle   (GdkSubsurface *subsurface);
void     gdk_wayland_subsurface_uninhibit_idle (GdkSubsurface *subsurface);
