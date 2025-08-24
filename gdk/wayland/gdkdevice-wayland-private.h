#pragma once

#include "gdkfractionalscale-private.h"
#include "gdkwaylanddevice.h"
#include "gdkwaylandseat.h"

#include <gdk/gdkdeviceprivate.h>
#include <gdk/gdkkeysprivate.h>

#include <xkbcommon/xkbcommon.h>

struct _GdkWaylandDevice
{
  GdkDevice parent_instance;
};

struct _GdkWaylandDeviceClass
{
  GdkDeviceClass parent_class;
};

typedef struct _GdkWaylandTouchData GdkWaylandTouchData;
typedef struct _GdkWaylandPointerFrameData GdkWaylandPointerFrameData;
typedef struct _GdkWaylandPointerData GdkWaylandPointerData;
typedef struct _GdkWaylandTabletPadGroupData GdkWaylandTabletPadGroupData;
typedef struct _GdkWaylandTabletPadData GdkWaylandTabletPadData;
typedef struct _GdkWaylandTabletData GdkWaylandTabletData;
typedef struct _GdkWaylandTabletToolData GdkWaylandTabletToolData;

struct _GdkWaylandTouchData
{
  uint32_t id;
  double x;
  double y;
  GdkSurface *surface;
  uint32_t touch_down_serial;
  uint32_t initial_touch : 1;
};

struct _GdkWaylandPointerFrameData
{
  GdkEvent *event;

  /* Specific to the scroll event */
  double delta_x, delta_y;
  int32_t value120_x, value120_y;
  gint8 is_scroll_stop;
  GdkScrollRelativeDirection relative_direction;
  enum wl_pointer_axis_source source;
};

struct _GdkWaylandPointerData {
  GdkSurface *focus;

  double surface_x, surface_y;

  GdkModifierType button_modifiers;

  uint32_t time;
  uint32_t enter_serial;
  uint32_t press_serial;

  GdkSurface *grab_surface;
  uint32_t grab_time;

  struct wl_surface *pointer_surface;
  struct wp_viewport *pointer_surface_viewport;
  struct wp_cursor_shape_device_v1 *shape_device;

  uint32_t cursor_is_default: 1;
  uint32_t has_cursor_surface : 1;

  uint32_t cursor_shape;

  GdkCursor *cursor;
  uint32_t touchpad_event_sequence;

  int32_t cursor_hotspot_x;
  int32_t cursor_hotspot_y;

  GdkFractionalScale preferred_scale;
  struct wp_fractional_scale_v1 *fractional_scale;

  /* Accumulated event data for a pointer frame */
  GdkWaylandPointerFrameData frame;
};

struct _GdkWaylandTabletPadGroupData
{
  GdkWaylandTabletPadData *pad;
  struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group;
  GList *rings;
  GList *strips;
  GList *dials;
  GList *buttons;

  uint32_t mode_switch_serial;
  uint32_t n_modes;
  uint32_t current_mode;

  struct {
    uint32_t source;
    gboolean is_stop;
    double value;
  } axis_tmp_info;
};

struct _GdkWaylandTabletPadData
{
  GdkSeat *seat;
  struct zwp_tablet_pad_v2 *wp_tablet_pad;
  GdkDevice *device;

  GdkWaylandTabletData *current_tablet;

  uint32_t enter_serial;
  uint32_t n_buttons;
  char *path;

  GList *rings;
  GList *strips;
  GList *dials;
  GList *mode_groups;
};

struct _GdkWaylandTabletToolData
{
  GdkSeat *seat;
  struct zwp_tablet_tool_v2 *wp_tablet_tool;
  struct wp_cursor_shape_device_v1 *shape_device;
  GdkAxisFlags axes;
  GdkDeviceToolType type;
  uint64_t hardware_serial;
  uint64_t hardware_id_wacom;

  GdkDeviceTool *tool;
  GdkWaylandTabletData *current_tablet;
};

struct _GdkWaylandTabletData
{
  struct zwp_tablet_v2 *wp_tablet;
  char *name;
  char *path;
  uint32_t vid;
  uint32_t pid;
  uint32_t bustype;

  GdkDevice *logical_device;
  GdkDevice *stylus_device;
  GdkSeat *seat;
  GdkWaylandPointerData pointer_info;
  GList *events;

  GList *pads;

  GdkWaylandTabletToolData *current_tool;

  int axis_indices[GDK_AXIS_LAST];
  double axes[GDK_AXIS_LAST];
};

struct _GdkWaylandSeat
{
  GdkSeat parent_instance;

  uint32_t id;
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;
  struct zwp_pointer_gesture_swipe_v1 *wp_pointer_gesture_swipe;
  struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch;
  struct zwp_pointer_gesture_hold_v1 *wp_pointer_gesture_hold;
  struct zwp_tablet_seat_v2 *wp_tablet_seat;

  GdkDisplay *display;

  GdkDevice *logical_pointer;
  GdkDevice *logical_keyboard;
  GdkDevice *pointer;
  GdkDevice *wheel_scrolling;
  GdkDevice *finger_scrolling;
  GdkDevice *continuous_scrolling;
  GdkDevice *keyboard;
  GdkDevice *logical_touch;
  GdkDevice *touch;
  GdkCursor *cursor;
  GdkKeymap *keymap;

  GHashTable *touches;
  GList *tablets;
  GList *tablet_tools;
  GList *tablet_pads;

  GdkWaylandPointerData pointer_info;
  GdkWaylandPointerData touch_info;

  uint32_t latest_touch_down_serial;

  GdkModifierType key_modifiers;
  GdkSurface *keyboard_focus;
  GdkSurface *grab_surface;
  uint32_t grab_time;
  gboolean have_server_repeat;
  uint32_t server_repeat_rate;
  uint32_t server_repeat_delay;

  struct wl_data_offer *pending_offer;
  GdkContentFormatsBuilder *pending_builder;
  GdkDragAction pending_source_actions;
  GdkDragAction pending_action;

  struct wl_callback *repeat_callback;
  uint32_t repeat_timer;
  uint32_t repeat_key;
  uint32_t repeat_count;
  int64_t repeat_deadline;
  uint32_t keyboard_time;
  uint32_t keyboard_key_serial;

  GdkClipboard *clipboard;
  GdkClipboard *primary_clipboard;
  struct wl_data_device *data_device;
  GdkDrag *drag;
  GdkDrop *drop;

  /* Some tracking on gesture events */
  uint32_t gesture_n_fingers;
  double gesture_scale;

  GdkCursor *grab_cursor;
};

#define GDK_TYPE_WAYLAND_DEVICE_PAD (gdk_wayland_device_pad_get_type ())
GType gdk_wayland_device_pad_get_type (void);

GdkWaylandPointerData * gdk_wayland_device_get_pointer (GdkWaylandDevice *wayland_device);

void gdk_wayland_device_set_pointer (GdkWaylandDevice      *wayland_device,
                                     GdkWaylandPointerData *pointer);

GdkWaylandTouchData * gdk_wayland_device_get_emulating_touch (GdkWaylandDevice *wayland_device);

void gdk_wayland_device_set_emulating_touch (GdkWaylandDevice    *wayland_device,
                                             GdkWaylandTouchData *touch);

void gdk_wayland_device_query_state (GdkDevice        *device,
                                     GdkSurface       *surface,
                                     double           *win_x,
                                     double           *win_y,
                                     GdkModifierType  *mask);

void gdk_wayland_device_pad_set_feedback (GdkDevice           *device,
                                          GdkDevicePadFeature  feature,
                                          uint32_t             feature_idx,
                                          const char          *label);

GdkWaylandTabletPadData * gdk_wayland_seat_find_pad (GdkWaylandSeat *seat,
                                                     GdkDevice      *device);

GdkWaylandTabletData * gdk_wayland_seat_find_tablet (GdkWaylandSeat *seat,
                                                     GdkDevice      *device);

GdkWaylandTouchData * gdk_wayland_seat_get_touch (GdkWaylandSeat *seat,
                                                  uint32_t        id);

void gdk_wayland_device_maybe_emit_grab_crossing (GdkDevice  *device,
                                                  GdkSurface *window,
                                                  uint32_t    time);

GdkSurface * gdk_wayland_device_maybe_emit_ungrab_crossing (GdkDevice *device,
                                                            uint32_t   time_);

void gdk_wayland_device_update_surface_cursor (GdkDevice *device);

GdkModifierType gdk_wayland_device_get_modifiers (GdkDevice *device);

GdkKeymap *_gdk_wayland_device_get_keymap (GdkDevice *device);

GdkSurface * gdk_wayland_device_get_focus (GdkDevice *device);

struct wl_data_device * gdk_wayland_device_get_data_device (GdkDevice *gdk_device);
void gdk_wayland_device_set_selection (GdkDevice             *gdk_device,
                                       struct wl_data_source *source);

GdkDrag* gdk_wayland_device_get_drop_context (GdkDevice *gdk_device);

void gdk_wayland_device_unset_touch_grab (GdkDevice        *device,
                                          GdkEventSequence *sequence);

