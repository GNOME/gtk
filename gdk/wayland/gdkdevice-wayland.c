/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdktypes.h>
#include "gdkprivate-wayland.h"
#include "gdkseat-wayland.h"
#include "gdkwayland.h"
#include "gdkkeysyms.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicepadprivate.h"
#include "gdkdevicetoolprivate.h"
#include "gdkdevicemanagerprivate.h"
#include "gdkseatprivate.h"
#include "pointer-gestures-unstable-v1-client-protocol.h"
#include "tablet-unstable-v2-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"

#include <xkbcommon/xkbcommon.h>

#include <sys/time.h>
#include <sys/mman.h>
#if defined(HAVE_DEV_EVDEV_INPUT_H)
#include <dev/evdev/input.h>
#elif defined(HAVE_LINUX_INPUT_H)
#include <linux/input.h>
#endif

#define BUTTON_BASE (BTN_LEFT - 1) /* Used to translate to 1-indexed buttons */

#ifndef BTN_STYLUS3
#define BTN_STYLUS3 0x149 /* Linux 4.15 */
#endif

typedef struct _GdkWaylandDevicePad GdkWaylandDevicePad;
typedef struct _GdkWaylandDevicePadClass GdkWaylandDevicePadClass;

typedef struct _GdkWaylandTouchData GdkWaylandTouchData;
typedef struct _GdkWaylandPointerFrameData GdkWaylandPointerFrameData;
typedef struct _GdkWaylandPointerData GdkWaylandPointerData;
typedef struct _GdkWaylandTabletData GdkWaylandTabletData;
typedef struct _GdkWaylandTabletToolData GdkWaylandTabletToolData;
typedef struct _GdkWaylandTabletPadGroupData GdkWaylandTabletPadGroupData;
typedef struct _GdkWaylandTabletPadData GdkWaylandTabletPadData;

struct _GdkWaylandTouchData
{
  uint32_t id;
  gdouble x;
  gdouble y;
  GdkWindow *window;
  uint32_t touch_down_serial;
  guint initial_touch : 1;
};

struct _GdkWaylandPointerFrameData
{
  GdkEvent *event;

  /* Specific to the scroll event */
  gdouble delta_x, delta_y;
  int32_t discrete_x, discrete_y;
  gint8 is_scroll_stop;
  enum wl_pointer_axis_source source;
};

struct _GdkWaylandPointerData {
  GdkWindow *focus;

  double surface_x, surface_y;

  GdkModifierType button_modifiers;

  uint32_t time;
  uint32_t enter_serial;
  uint32_t press_serial;

  GdkWindow *grab_window;
  uint32_t grab_time;

  struct wl_surface *pointer_surface;
  struct wp_cursor_shape_device_v1 *shape_device;
  GdkCursor *cursor;
  guint cursor_timeout_id;
  guint cursor_image_index;
  guint cursor_image_delay;

  guint current_output_scale;
  GSList *pointer_surface_outputs;

  /* Accumulated event data for a pointer frame */
  GdkWaylandPointerFrameData frame;
};

struct _GdkWaylandTabletToolData
{
  GdkSeat *seat;
  struct zwp_tablet_tool_v2 *wp_tablet_tool;
  struct wp_cursor_shape_device_v1 *shape_device;
  GdkAxisFlags axes;
  GdkDeviceToolType type;
  guint64 hardware_serial;
  guint64 hardware_id_wacom;

  GdkDeviceTool *tool;
  GdkWaylandTabletData *current_tablet;
};

struct _GdkWaylandTabletPadGroupData
{
  GdkWaylandTabletPadData *pad;
  struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group;
  GList *rings;
  GList *strips;
  GList *buttons;

  guint mode_switch_serial;
  guint n_modes;
  guint current_mode;

  struct {
    guint source;
    gboolean is_stop;
    gdouble value;
  } axis_tmp_info;
};

struct _GdkWaylandTabletPadData
{
  GdkSeat *seat;
  struct zwp_tablet_pad_v2 *wp_tablet_pad;
  GdkDevice *device;

  GdkWaylandTabletData *current_tablet;

  guint enter_serial;
  uint32_t n_buttons;
  gchar *path;

  GList *rings;
  GList *strips;
  GList *mode_groups;
};

struct _GdkWaylandTabletData
{
  struct zwp_tablet_v2 *wp_tablet;
  gchar *name;
  gchar *path;
  uint32_t vid;
  uint32_t pid;

  GdkDevice *master;
  GdkDevice *stylus_device;
  GdkDevice *eraser_device;
  GdkDevice *current_device;
  GdkSeat *seat;
  GdkWaylandPointerData pointer_info;

  GList *pads;

  GdkWaylandTabletToolData *current_tool;

  gint axis_indices[GDK_AXIS_LAST];
  gdouble *axes;
};

struct _GdkWaylandSeat
{
  GdkSeat parent_instance;

  guint32 id;
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;
  struct zwp_pointer_gesture_swipe_v1 *wp_pointer_gesture_swipe;
  struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch;
  struct zwp_tablet_seat_v2 *wp_tablet_seat;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GdkDevice *pointer;
  GdkDevice *wheel_scrolling;
  GdkDevice *finger_scrolling;
  GdkDevice *continuous_scrolling;
  GdkDevice *keyboard;
  GdkDevice *touch_master;
  GdkDevice *touch;
  GdkCursor *cursor;
  GdkKeymap *keymap;

  GHashTable *touches;
  GList *tablets;
  GList *tablet_tools;
  GList *tablet_pads;

  GdkWaylandPointerData pointer_info;
  GdkWaylandPointerData touch_info;

  GdkModifierType key_modifiers;
  GdkWindow *keyboard_focus;
  GdkAtom pending_selection;
  GdkWindow *grab_window;
  uint32_t grab_time;
  gboolean have_server_repeat;
  uint32_t server_repeat_rate;
  uint32_t server_repeat_delay;

  struct wl_callback *repeat_callback;
  guint32 repeat_timer;
  guint32 repeat_key;
  guint32 repeat_count;
  gint64 repeat_deadline;
  GSettings *keyboard_settings;
  uint32_t keyboard_time;
  uint32_t keyboard_key_serial;

  struct gtk_primary_selection_device *gtk_primary_data_device;
  struct zwp_primary_selection_device_v1 *zwp_primary_data_device_v1;
  struct wl_data_device *data_device;
  GdkDragContext *drop_context;

  /* Source/dest for non-local dnd */
  GdkWindow *foreign_dnd_window;

  /* Some tracking on gesture events */
  guint gesture_n_fingers;
  gdouble gesture_scale;

  GdkCursor *grab_cursor;
};

G_DEFINE_TYPE (GdkWaylandSeat, gdk_wayland_seat, GDK_TYPE_SEAT)

struct _GdkWaylandDevice
{
  GdkDevice parent_instance;
  GdkWaylandTouchData *emulating_touch; /* Only used on wd->touch_master */
  GdkWaylandPointerData *pointer;
};

struct _GdkWaylandDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandDevice, gdk_wayland_device, GDK_TYPE_DEVICE)

struct _GdkWaylandDevicePad
{
  GdkWaylandDevice parent_instance;
};

struct _GdkWaylandDevicePadClass
{
  GdkWaylandDeviceClass parent_class;
};

static void gdk_wayland_device_pad_iface_init (GdkDevicePadInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandDevicePad, gdk_wayland_device_pad,
                         GDK_TYPE_WAYLAND_DEVICE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DEVICE_PAD,
                                                gdk_wayland_device_pad_iface_init))

#define GDK_TYPE_WAYLAND_DEVICE_PAD (gdk_wayland_device_pad_get_type ())

#define GDK_TYPE_WAYLAND_DEVICE_MANAGER        (gdk_wayland_device_manager_get_type ())
#define GDK_WAYLAND_DEVICE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManager))
#define GDK_WAYLAND_DEVICE_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManagerClass))
#define GDK_IS_WAYLAND_DEVICE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER))
#define GDK_IS_WAYLAND_DEVICE_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WAYLAND_DEVICE_MANAGER))
#define GDK_WAYLAND_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManagerClass))

#define GDK_SLOT_TO_EVENT_SEQUENCE(s) ((GdkEventSequence *) GUINT_TO_POINTER((s) + 1))
#define GDK_EVENT_SEQUENCE_TO_SLOT(s) (GPOINTER_TO_UINT(s) - 1)

typedef struct _GdkWaylandDeviceManager GdkWaylandDeviceManager;
typedef struct _GdkWaylandDeviceManagerClass GdkWaylandDeviceManagerClass;

struct _GdkWaylandDeviceManager
{
  GdkDeviceManager parent_object;
  GList *devices;
};

struct _GdkWaylandDeviceManagerClass
{
  GdkDeviceManagerClass parent_class;
};

static void init_pointer_data (GdkWaylandPointerData *pointer_data,
                               GdkDisplay            *display_wayland,
                               GdkDevice             *master);

static void
pointer_surface_update_scale (GdkDevice *device);


static void deliver_key_event (GdkWaylandSeat       *seat,
                               uint32_t              time_,
                               uint32_t              key,
                               uint32_t              state,
                               gboolean              from_key_repeat);
GType gdk_wayland_device_manager_get_type (void);

G_DEFINE_TYPE (GdkWaylandDeviceManager,
	       gdk_wayland_device_manager, GDK_TYPE_DEVICE_MANAGER)

static gboolean
gdk_wayland_device_get_history (GdkDevice      *device,
                                GdkWindow      *window,
                                guint32         start,
                                guint32         stop,
                                GdkTimeCoord ***events,
                                gint           *n_events)
{
  return FALSE;
}

static void
gdk_wayland_device_get_state (GdkDevice       *device,
                              GdkWindow       *window,
                              gdouble         *axes,
                              GdkModifierType *mask)
{
  gdouble x, y;

  gdk_window_get_device_position_double (window, device, &x, &y, mask);

  if (axes)
    {
      axes[0] = x;
      axes[1] = y;
    }
}

static void
gdk_wayland_pointer_stop_cursor_animation (GdkWaylandPointerData *pointer)
{
  if (pointer->cursor_timeout_id > 0)
    {
      g_source_remove (pointer->cursor_timeout_id);
      pointer->cursor_timeout_id = 0;
    }

  pointer->cursor_image_index = 0;
}

static GdkWaylandTabletData *
gdk_wayland_device_manager_find_tablet (GdkWaylandSeat *seat,
                                        GdkDevice      *device)
{
  GList *l;

  for (l = seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      if (tablet->master == device ||
          tablet->stylus_device == device ||
          tablet->eraser_device == device)
        return tablet;
    }

  return NULL;
}

static GdkWaylandTabletPadData *
gdk_wayland_device_manager_find_pad (GdkWaylandSeat *seat,
                                     GdkDevice      *device)
{
  GList *l;

  for (l = seat->tablet_pads; l; l = l->next)
    {
      GdkWaylandTabletPadData *pad = l->data;

      if (pad->device == device)
        return pad;
    }

  return NULL;
}


static const struct
{
  const char *cursor_name;
  unsigned int shape;
  unsigned int version;
} shape_map[] = {
  { "default", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, 1 },
  { "context-menu", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU, 1 },
  { "help", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP, 1 },
  { "pointer", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER, 1 },
  { "progress", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS },
  { "wait", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT, 1 },
  { "cell", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL, 1 },
  { "crosshair", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR, 1 },
  { "text", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT, 1 },
  { "vertical-text", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT, 1 },
  { "alias", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS, 1 },
  { "copy", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY, 1 },
  { "move", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE, 1 },
  { "no-drop", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP, 1 },
  { "not-allowed", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED, 1 },
  { "grab", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB, 1 },
  { "grabbing", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING, 1 },
  { "e-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE, 1 },
  { "n-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE, 1 },
  { "ne-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE, 1 },
  { "nw-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE, 1 },
  { "s-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE, 1 },
  { "se-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE, 1 },
  { "sw-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE, 1 },
  { "w-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE, 1 },
  { "ew-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE, 1 },
  { "ns-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE, 1 },
  { "nesw-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE, 1 },
  { "nwse-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE, 1 },
  { "col-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE, 1 },
  { "row-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE, 1 },
  { "all-scroll", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL, 1 },
  { "zoom-in", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN, 1 },
  { "zoom-out", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT, 1 },
  { "all-scroll", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL, 1 },
  /* the following a v2 additions, with a fallback for v1 */
  { "dnd-ask", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK, 2 },
  { "dnd-ask", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU, 1 },
  { "all-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE, 2 },
  { "all-resize", WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE, 1 },
};

static unsigned int
_gdk_wayland_cursor_get_shape (GdkCursor *cursor,
                               int        version)
{
  gsize i;
  const char *cursor_name;

  cursor_name = _gdk_wayland_cursor_get_name (cursor);
  if (cursor_name == NULL ||
      g_str_equal (cursor_name, "none"))
    return 0;

  for (i = 0; i < G_N_ELEMENTS (shape_map); i++)
    {
      if (g_str_equal (shape_map[i].cursor_name, cursor_name) &&
          version >= shape_map[i].version)
        return shape_map[i].shape;
    }

  return 0;
}

static gboolean
gdk_wayland_device_update_window_cursor (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  struct wl_buffer *buffer;
  int x, y, w, h, scale;
  guint next_image_index, next_image_delay;
  gboolean retval = G_SOURCE_REMOVE;
  GdkWaylandTabletData *tablet;
  unsigned int shape;

  tablet = gdk_wayland_device_manager_find_tablet (seat, device);

  if (!pointer->cursor)
    {
      pointer->cursor_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (tablet && !tablet->current_tool)
    {
      pointer->cursor_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (GDK_WAYLAND_DISPLAY (seat->display)->cursor_shape)
    {
      shape = _gdk_wayland_cursor_get_shape (pointer->cursor,
                                             wp_cursor_shape_manager_v1_get_version (GDK_WAYLAND_DISPLAY (seat->display)->cursor_shape));
      if (shape != 0)
        {
          if (tablet && tablet->current_tool->shape_device)
            {
              wp_cursor_shape_device_v1_set_shape (tablet->current_tool->shape_device, pointer->enter_serial, shape);
              return G_SOURCE_REMOVE;
            }
          else if (seat->wl_pointer && pointer->shape_device)
            {
              wp_cursor_shape_device_v1_set_shape (pointer->shape_device, pointer->enter_serial, shape);
              return G_SOURCE_REMOVE;
            }
        }
    }

  if (pointer->cursor)
    {
      buffer = _gdk_wayland_cursor_get_buffer (pointer->cursor,
                                               pointer->cursor_image_index,
                                               &x, &y, &w, &h, &scale);
    }

  if (tablet)
    {
      zwp_tablet_tool_v2_set_cursor (tablet->current_tool->wp_tablet_tool,
                                     pointer->enter_serial,
                                     pointer->pointer_surface,
                                     x, y);
    }
  else if (seat->wl_pointer)
    {
      wl_pointer_set_cursor (seat->wl_pointer,
                             pointer->enter_serial,
                             pointer->pointer_surface,
                             x, y);
    }
  else
    {
      pointer->cursor_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (buffer)
    {
      wl_surface_attach (pointer->pointer_surface, buffer, 0, 0);
      wl_surface_set_buffer_scale (pointer->pointer_surface, scale);
      wl_surface_damage (pointer->pointer_surface,  0, 0, w, h);
      wl_surface_commit (pointer->pointer_surface);
    }
  else
    {
      wl_surface_attach (pointer->pointer_surface, NULL, 0, 0);
      wl_surface_commit (pointer->pointer_surface);
    }

  next_image_index =
    _gdk_wayland_cursor_get_next_image_index (pointer->cursor,
                                              pointer->cursor_image_index,
                                              &next_image_delay);

  if (next_image_index != pointer->cursor_image_index)
    {
      if (next_image_delay != pointer->cursor_image_delay ||
          pointer->cursor_timeout_id == 0)
        {
          guint id;

          gdk_wayland_pointer_stop_cursor_animation (pointer);

          /* Queue timeout for next frame */
          id = g_timeout_add (next_image_delay,
                              (GSourceFunc) gdk_wayland_device_update_window_cursor,
                              device);
          g_source_set_name_by_id (id, "[gtk+] gdk_wayland_device_update_window_cursor");
          pointer->cursor_timeout_id = id;
        }
      else
        retval = G_SOURCE_CONTINUE;

      pointer->cursor_image_index = next_image_index;
      pointer->cursor_image_delay = next_image_delay;
    }
  else
    gdk_wayland_pointer_stop_cursor_animation (pointer);

  return retval;
}

static void
gdk_wayland_device_set_window_cursor (GdkDevice *device,
                                      GdkWindow *window,
                                      GdkCursor *cursor)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;

  if (device == seat->touch_master)
    return;

  if (seat->grab_cursor)
    cursor = seat->grab_cursor;

  /* Setting the cursor to NULL means that we should use
   * the default cursor
   */
  if (!cursor)
    {
      guint scale = pointer->current_output_scale;
      cursor =
        _gdk_wayland_display_get_cursor_for_name_with_scale (seat->display,
                                                             "default",
                                                             scale);
    }
  else
    _gdk_wayland_cursor_set_scale (cursor, pointer->current_output_scale);

  if (cursor == pointer->cursor)
    return;

  gdk_wayland_pointer_stop_cursor_animation (pointer);

  if (pointer->cursor)
    g_object_unref (pointer->cursor);

  pointer->cursor = g_object_ref (cursor);

  gdk_wayland_device_update_window_cursor (device);
}

static void
gdk_wayland_device_warp (GdkDevice *device,
                         GdkScreen *screen,
                         gdouble    x,
                         gdouble    y)
{
}

static void
get_coordinates (GdkDevice *device,
                 double    *x,
                 double    *y,
                 double    *x_root,
                 double    *y_root)
{
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  int root_x, root_y;

  if (x)
    *x = pointer->surface_x;
  if (y)
    *y = pointer->surface_y;

  if (pointer->focus)
    {
      gdk_window_get_root_coords (pointer->focus,
                                  pointer->surface_x,
                                  pointer->surface_y,
                                  &root_x, &root_y);
    }
  else
    {
      root_x = pointer->surface_x;
      root_y = pointer->surface_y;
    }

  if (x_root)
    *x_root = root_x;
  if (y_root)
    *y_root = root_y;
}

static GdkModifierType
device_get_modifiers (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkModifierType mask;

  mask = seat->key_modifiers;

  if (pointer)
    mask |= pointer->button_modifiers;

  return mask;
}

static void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkWindow        *window,
                                GdkWindow       **root_window,
                                GdkWindow       **child_window,
                                gdouble          *root_x,
                                gdouble          *root_y,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask)
{
  GdkWaylandSeat *seat;
  GdkWaylandPointerData *pointer;
  GdkScreen *default_screen;

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  default_screen = gdk_display_get_default_screen (seat->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    /* Set child only if actually a child of the given window, as XIQueryPointer() does */
    *child_window = g_list_find (window->children, pointer->focus) ? pointer->focus : NULL;
  if (mask)
    *mask = device_get_modifiers (device);

  get_coordinates (device, win_x, win_y, root_x, root_y);
}

static void
emulate_crossing (GdkWindow       *window,
                  GdkWindow       *subwindow,
                  GdkDevice       *device,
                  GdkDevice       *source,
                  GdkEventType     type,
                  GdkCrossingMode  mode,
                  guint32          time_)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->crossing.window = window ? g_object_ref (window) : NULL;
  event->crossing.subwindow = subwindow ? g_object_ref (subwindow) : NULL;
  event->crossing.time = time_;
  event->crossing.mode = mode;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  gdk_window_get_device_position_double (window, device,
                                         &event->crossing.x, &event->crossing.y,
                                         &event->crossing.state);
  event->crossing.x_root = event->crossing.x;
  event->crossing.y_root = event->crossing.y;

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
emulate_touch_crossing (GdkWindow           *window,
                        GdkWindow           *subwindow,
                        GdkDevice           *device,
                        GdkDevice           *source,
                        GdkWaylandTouchData *touch,
                        GdkEventType         type,
                        GdkCrossingMode      mode,
                        guint32              time_)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->crossing.window = window ? g_object_ref (window) : NULL;
  event->crossing.subwindow = subwindow ? g_object_ref (subwindow) : NULL;
  event->crossing.time = time_;
  event->crossing.mode = mode;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  event->crossing.x = touch->x;
  event->crossing.y = touch->y;
  event->crossing.x_root = event->crossing.x;
  event->crossing.y_root = event->crossing.y;

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
emulate_focus (GdkWindow *window,
               GdkDevice *device,
               gboolean   focus_in,
               guint32    time_)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (window);
  event->focus_change.in = focus_in;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
device_emit_grab_crossing (GdkDevice       *device,
                           GdkWindow       *from,
                           GdkWindow       *to,
                           GdkCrossingMode  mode,
                           guint32          time_)
{
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      if (from)
        emulate_focus (from, device, FALSE, time_);
      if (to)
        emulate_focus (to, device, TRUE, time_);
    }
  else
    {
      if (from)
        emulate_crossing (from, to, device, device, GDK_LEAVE_NOTIFY, mode, time_);
      if (to)
        emulate_crossing (to, from, device, device, GDK_ENTER_NOTIFY, mode, time_);
    }
}

GdkWindow *
gdk_wayland_device_get_focus (GdkDevice *device)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandPointerData *pointer;

  if (device == wayland_seat->master_keyboard)
    return wayland_seat->keyboard_focus;
  else
    {
      pointer = GDK_WAYLAND_DEVICE (device)->pointer;

      if (pointer)
        return pointer->focus;
    }

  return NULL;
}

static void
device_maybe_emit_grab_crossing (GdkDevice *device,
                                 GdkWindow *window,
                                 guint32    time)
{
  GdkWindow *native = gdk_wayland_device_get_focus (device);
  GdkWindow *focus = gdk_window_get_toplevel (window);

  if (focus != native)
    device_emit_grab_crossing (device, focus, window, GDK_CROSSING_GRAB, time);
}

static GdkWindow*
device_maybe_emit_ungrab_crossing (GdkDevice      *device,
                                   guint32         time)
{
  GdkDeviceGrabInfo *grab;
  GdkWindow *focus = NULL;
  GdkWindow *native = NULL;
  GdkWindow *prev_focus = NULL;

  focus = gdk_wayland_device_get_focus (device);
  grab = _gdk_display_get_last_device_grab (gdk_device_get_display (device), device);

  if (grab)
    {
      grab->serial_end = grab->serial_start;
      prev_focus = grab->window;
      native = grab->native_window;
    }

  if (focus != native)
    device_emit_grab_crossing (device, prev_focus, focus, GDK_CROSSING_UNGRAB, time);

  return prev_focus;
}

static GdkGrabStatus
gdk_wayland_device_grab (GdkDevice    *device,
                         GdkWindow    *window,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkWindow    *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;

  if (gdk_window_get_window_type (window) == GDK_WINDOW_TEMP &&
      gdk_window_is_visible (window))
    {
      g_warning ("Window %p is already mapped at the time of grabbing. "
                 "gdk_seat_grab() should be used to simultanously grab input "
                 "and show this popup. You may find oddities ahead.",
                 window);
    }

  device_maybe_emit_grab_crossing (device, window, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      if (gdk_window_get_window_type (window) == GDK_WINDOW_TOPLEVEL)
        {
          gdk_wayland_window_inhibit_shortcuts (window,
                                                gdk_device_get_seat (device));
        }

      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */
      if (pointer->grab_window != NULL &&
          time_ != 0 && pointer->grab_time > time_)
        {
          return GDK_GRAB_ALREADY_GRABBED;
        }

      if (time_ == 0)
        time_ = pointer->time;

      pointer->grab_window = window;
      pointer->grab_time = time_;
      _gdk_wayland_window_set_grab_seat (window, GDK_SEAT (wayland_seat));

      g_clear_object (&wayland_seat->cursor);

      if (cursor)
        wayland_seat->cursor = g_object_ref (cursor);

      gdk_wayland_device_update_window_cursor (device);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_device_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkWindow *prev_focus;

  prev_focus = device_maybe_emit_ungrab_crossing (device, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      if (prev_focus)
        gdk_wayland_window_restore_shortcuts (prev_focus,
                                              gdk_device_get_seat (device));
    }
  else
    {
      /* Device is a pointer */
      gdk_wayland_device_update_window_cursor (device);

      if (pointer->grab_window)
        _gdk_wayland_window_set_grab_seat (pointer->grab_window,
                                           NULL);
    }
}

static GdkWindow *
gdk_wayland_device_window_at_position (GdkDevice       *device,
                                       gdouble         *win_x,
                                       gdouble         *win_y,
                                       GdkModifierType *mask,
                                       gboolean         get_toplevel)
{
  GdkWaylandPointerData *pointer;

  pointer = GDK_WAYLAND_DEVICE(device)->pointer;

  if (!pointer)
    return NULL;

  if (win_x)
    *win_x = pointer->surface_x;
  if (win_y)
    *win_y = pointer->surface_y;
  if (mask)
    *mask = device_get_modifiers (device);

  return pointer->focus;
}

static void
gdk_wayland_device_select_window_events (GdkDevice    *device,
                                         GdkWindow    *window,
                                         GdkEventMask  event_mask)
{
}

static void
gdk_wayland_device_class_init (GdkWaylandDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_wayland_device_get_history;
  device_class->get_state = gdk_wayland_device_get_state;
  device_class->set_window_cursor = gdk_wayland_device_set_window_cursor;
  device_class->warp = gdk_wayland_device_warp;
  device_class->query_state = gdk_wayland_device_query_state;
  device_class->grab = gdk_wayland_device_grab;
  device_class->ungrab = gdk_wayland_device_ungrab;
  device_class->window_at_position = gdk_wayland_device_window_at_position;
  device_class->select_window_events = gdk_wayland_device_select_window_events;
}

static void
gdk_wayland_device_init (GdkWaylandDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

static gint
gdk_wayland_device_pad_get_n_groups (GdkDevicePad *pad)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadData *data;

  data = gdk_wayland_device_manager_find_pad (GDK_WAYLAND_SEAT (seat),
                                              GDK_DEVICE (pad));
  g_assert (data != NULL);

  return g_list_length (data->mode_groups);
}

static gint
gdk_wayland_device_pad_get_group_n_modes (GdkDevicePad *pad,
                                          gint          n_group)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadGroupData *group;
  GdkWaylandTabletPadData *data;

  data = gdk_wayland_device_manager_find_pad (GDK_WAYLAND_SEAT (seat),
                                              GDK_DEVICE (pad));
  g_assert (data != NULL);

  group = g_list_nth_data (data->mode_groups, n_group);
  if (!group)
    return -1;

  return group->n_modes;
}

static gint
gdk_wayland_device_pad_get_n_features (GdkDevicePad        *pad,
                                       GdkDevicePadFeature  feature)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadData *data;

  data = gdk_wayland_device_manager_find_pad (GDK_WAYLAND_SEAT (seat),
                                              GDK_DEVICE (pad));
  g_assert (data != NULL);

  switch (feature)
    {
    case GDK_DEVICE_PAD_FEATURE_BUTTON:
      return data->n_buttons;
    case GDK_DEVICE_PAD_FEATURE_RING:
      return g_list_length (data->rings);
    case GDK_DEVICE_PAD_FEATURE_STRIP:
      return g_list_length (data->strips);
    default:
      return -1;
    }
}

static gint
gdk_wayland_device_pad_get_feature_group (GdkDevicePad        *pad,
                                          GdkDevicePadFeature  feature,
                                          gint                 idx)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (pad));
  GdkWaylandTabletPadGroupData *group;
  GdkWaylandTabletPadData *data;
  GList *l;
  gint i;

  data = gdk_wayland_device_manager_find_pad (GDK_WAYLAND_SEAT (seat),
                                              GDK_DEVICE (pad));
  g_assert (data != NULL);

  for (l = data->mode_groups, i = 0; l; l = l->next, i++)
    {
      group = l->data;

      switch (feature)
        {
        case GDK_DEVICE_PAD_FEATURE_BUTTON:
          if (g_list_find (group->buttons, GINT_TO_POINTER (idx)))
            return i;
          break;
        case GDK_DEVICE_PAD_FEATURE_RING:
          {
            gpointer ring;

            ring = g_list_nth_data (data->rings, idx);
            if (ring && g_list_find (group->rings, ring))
              return i;
            break;
          }
        case GDK_DEVICE_PAD_FEATURE_STRIP:
          {
            gpointer strip;
            strip = g_list_nth_data (data->strips, idx);
            if (strip && g_list_find (group->strips, strip))
              return i;
            break;
          }
        default:
          break;
        }
    }

  return -1;
}

static void
gdk_wayland_device_pad_iface_init (GdkDevicePadInterface *iface)
{
  iface->get_n_groups = gdk_wayland_device_pad_get_n_groups;
  iface->get_group_n_modes = gdk_wayland_device_pad_get_group_n_modes;
  iface->get_n_features = gdk_wayland_device_pad_get_n_features;
  iface->get_feature_group = gdk_wayland_device_pad_get_feature_group;
}

static void
gdk_wayland_device_pad_class_init (GdkWaylandDevicePadClass *klass)
{
}

static void
gdk_wayland_device_pad_init (GdkWaylandDevicePad *pad)
{
}

/**
 * gdk_wayland_device_get_wl_seat:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_seat of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_seat
 *
 * Since: 3.10
 */
struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_seat;
}

/**
 * gdk_wayland_device_get_wl_pointer:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_pointer of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_pointer
 *
 * Since: 3.10
 */
struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_pointer;
}

/**
 * gdk_wayland_device_get_wl_keyboard:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_keyboard of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_keyboard
 *
 * Since: 3.10
 */
struct wl_keyboard *
gdk_wayland_device_get_wl_keyboard (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_keyboard;
}

GdkKeymap *
_gdk_wayland_device_get_keymap (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->keymap;
}

static void
emit_selection_owner_change (GdkWindow *window,
                             GdkAtom    atom)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_OWNER_CHANGE);
  event->owner_change.window = g_object_ref (window);
  event->owner_change.owner = NULL;
  event->owner_change.reason = GDK_OWNER_CHANGE_NEW_OWNER;
  event->owner_change.selection = atom;
  event->owner_change.time = GDK_CURRENT_TIME;
  event->owner_change.selection_time = GDK_CURRENT_TIME;

  gdk_event_put (event);
  gdk_event_free (event);
}

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device data offer, data device %p, offer %p",
                       data_device, offer));

  gdk_wayland_selection_ensure_offer (seat->display, offer);
}

static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               serial,
                   struct wl_surface     *surface,
                   wl_fixed_t             x,
                   wl_fixed_t             y,
                   struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;
  GdkWindow *dest_window, *dnd_owner;
  GdkAtom selection;

  if (!surface)
    return;

  dest_window = wl_surface_get_user_data (surface);

  if (!GDK_IS_WINDOW (dest_window))
    return;

  GDK_NOTE (EVENTS,
            g_message ("data device enter, data device %p serial %u, surface %p, x %f y %f, offer %p",
                       data_device, serial, surface, wl_fixed_to_double (x), wl_fixed_to_double (y), offer));

  /* Update pointer state, so device state queries work during DnD */
  seat->pointer_info.focus = g_object_ref (dest_window);
  seat->pointer_info.surface_x = wl_fixed_to_double (x);
  seat->pointer_info.surface_y = wl_fixed_to_double (y);

  selection = gdk_drag_get_selection (seat->drop_context);
  dnd_owner = gdk_selection_owner_get_for_display (seat->display, selection);

  if (!dnd_owner)
    dnd_owner = seat->foreign_dnd_window;

  gdk_wayland_selection_set_offer (seat->display, selection, offer);

  gdk_wayland_drop_context_update_targets (seat->drop_context);

  _gdk_wayland_drag_context_set_source_window (seat->drop_context, dnd_owner);

  _gdk_wayland_drag_context_set_dest_window (seat->drop_context,
                                             dest_window, serial);
  _gdk_wayland_drag_context_set_coords (seat->drop_context,
                                        wl_fixed_to_double (x),
                                        wl_fixed_to_double (y));
  _gdk_wayland_drag_context_emit_event (seat->drop_context, GDK_DRAG_ENTER,
                                        GDK_CURRENT_TIME);

  emit_selection_owner_change (dest_window, selection);
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device leave, data device %p", data_device));

  if (!gdk_drag_context_get_dest_window (seat->drop_context))
    return;

  g_object_unref (seat->pointer_info.focus);
  seat->pointer_info.focus = NULL;

  _gdk_wayland_drag_context_set_coords (seat->drop_context, -1, -1);
  _gdk_wayland_drag_context_emit_event (seat->drop_context, GDK_DRAG_LEAVE,
                                        GDK_CURRENT_TIME);
  _gdk_wayland_drag_context_set_dest_window (seat->drop_context, NULL, 0);
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    wl_fixed_t             x,
                    wl_fixed_t             y)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device motion, data_device = %p, time = %d, x = %f, y = %f",
                       data_device, time, wl_fixed_to_double (x), wl_fixed_to_double (y)));

  if (!gdk_drag_context_get_dest_window (seat->drop_context))
    return;

  /* Update pointer state, so device state queries work during DnD */
  seat->pointer_info.surface_x = wl_fixed_to_double (x);
  seat->pointer_info.surface_y = wl_fixed_to_double (y);

  gdk_wayland_drop_context_update_targets (seat->drop_context);
  _gdk_wayland_drag_context_set_coords (seat->drop_context,
                                        wl_fixed_to_double (x),
                                        wl_fixed_to_double (y));
  _gdk_wayland_drag_context_emit_event (seat->drop_context,
                                        GDK_DRAG_MOTION, time);
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device drop, data device %p", data_device));

  _gdk_wayland_drag_context_emit_event (seat->drop_context,
                                        GDK_DROP_START, GDK_CURRENT_TIME);
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;
  GdkAtom selection;

  GDK_NOTE (EVENTS,
            g_message ("data device selection, data device %p, data offer %p",
                       wl_data_device, offer));

  selection = gdk_atom_intern_static_string ("CLIPBOARD");
  gdk_wayland_selection_set_offer (seat->display, selection, offer);

  /* If we already have keyboard focus, the selection was targeted at the
   * focused surface. If we don't we will receive keyboard focus directly after
   * this, so lets wait and find out what window will get the focus before
   * emitting the owner-changed event.
   */
  if (seat->keyboard_focus)
    emit_selection_owner_change (seat->keyboard_focus, selection);
  else
    seat->pending_selection = selection;
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_enter,
  data_device_leave,
  data_device_motion,
  data_device_drop,
  data_device_selection
};

static void
primary_selection_data_offer (void     *data,
                              gpointer  primary_selection_device,
                              gpointer  primary_offer)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("primary selection offer, device %p, data offer %p",
                       primary_selection_device, primary_offer));

  gdk_wayland_selection_ensure_primary_offer (seat->display, primary_offer);
}

static void
gtk_primary_selection_data_offer (void                                *data,
                                  struct gtk_primary_selection_device *primary_selection_device,
                                  struct gtk_primary_selection_offer  *primary_offer)
{
  primary_selection_data_offer (data,
                                (gpointer) primary_selection_device,
                                (gpointer) primary_offer);
}

static void
zwp_primary_selection_v1_data_offer (void                                   *data,
                                     struct zwp_primary_selection_device_v1 *primary_selection_device,
                                     struct zwp_primary_selection_offer_v1  *primary_offer)
{
  primary_selection_data_offer (data,
                                (gpointer) primary_selection_device,
                                (gpointer) primary_offer);
}

static void
primary_selection_selection (void     *data,
                             gpointer  primary_selection_device,
                             gpointer  primary_offer)
{
  GdkWaylandSeat *seat = data;
  GdkAtom selection;

  if (!seat->keyboard_focus)
    return;

  GDK_NOTE (EVENTS,
            g_message ("primary selection selection, device %p, data offer %p",
                       primary_selection_device, primary_offer));

  selection = gdk_atom_intern_static_string ("PRIMARY");
  gdk_wayland_selection_set_offer (seat->display, selection, primary_offer);
  emit_selection_owner_change (seat->keyboard_focus, selection);
}

static void
gtk_primary_selection_selection (void                                *data,
                                 struct gtk_primary_selection_device *primary_selection_device,
                                 struct gtk_primary_selection_offer  *primary_offer)
{
  primary_selection_selection (data,
                               (gpointer) primary_selection_device,
                               (gpointer) primary_offer);
}

static void
zwp_primary_selection_v1_selection (void                                   *data,
                                    struct zwp_primary_selection_device_v1 *primary_selection_device,
                                    struct zwp_primary_selection_offer_v1  *primary_offer)
{
  primary_selection_selection (data,
                               (gpointer) primary_selection_device,
                               (gpointer) primary_offer);
}

static const struct gtk_primary_selection_device_listener gtk_primary_device_listener = {
  gtk_primary_selection_data_offer,
  gtk_primary_selection_selection,
};

static const struct zwp_primary_selection_device_v1_listener zwp_primary_device_v1_listener = {
  zwp_primary_selection_v1_data_offer,
  zwp_primary_selection_v1_selection,
};

static GdkDevice * get_scroll_device (GdkWaylandSeat              *seat,
                                      enum wl_pointer_axis_source  source);

static GdkEvent *
create_scroll_event (GdkWaylandSeat        *seat,
                     GdkWaylandPointerData *pointer_info,
                     GdkDevice             *device,
                     GdkDevice             *source_device,
                     gboolean               emulated)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = g_object_ref (pointer_info->focus);
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source_device);
  event->scroll.time = pointer_info->time;
  event->scroll.state = device_get_modifiers (device);
  gdk_event_set_screen (event, display->screen);

  gdk_event_set_pointer_emulated (event, emulated);

  get_coordinates (device,
                   &event->scroll.x,
                   &event->scroll.y,
                   &event->scroll.x_root,
                   &event->scroll.y_root);

  return event;
}

static void
flush_discrete_scroll_event (GdkWaylandSeat     *seat,
                             GdkScrollDirection  direction)
{
  GdkEvent *event;
  GdkDevice *source;

  source = get_scroll_device (seat, seat->pointer_info.frame.source);
  event = create_scroll_event (seat, &seat->pointer_info,
                               seat->master_pointer, source, TRUE);
  event->scroll.direction = direction;

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
flush_smooth_scroll_event (GdkWaylandSeat *seat,
                           gdouble         delta_x,
                           gdouble         delta_y,
                           gboolean        is_stop)
{
  GdkEvent *event;
  GdkDevice *source;

  source = get_scroll_device (seat, seat->pointer_info.frame.source);
  event = create_scroll_event (seat, &seat->pointer_info,
                               seat->master_pointer, source, FALSE);
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  event->scroll.is_stop = is_stop;

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
flush_scroll_event (GdkWaylandSeat             *seat,
                    GdkWaylandPointerFrameData *pointer_frame)
{
  gboolean is_stop = FALSE;

  if (pointer_frame->discrete_x || pointer_frame->discrete_y)
    {
      GdkScrollDirection direction;

      if (pointer_frame->discrete_x > 0)
        direction = GDK_SCROLL_LEFT;
      else if (pointer_frame->discrete_x < 0)
        direction = GDK_SCROLL_RIGHT;
      else if (pointer_frame->discrete_y > 0)
        direction = GDK_SCROLL_DOWN;
      else
        direction = GDK_SCROLL_UP;

      flush_discrete_scroll_event (seat, direction);
      pointer_frame->discrete_x = 0;
      pointer_frame->discrete_y = 0;
    }

  if (pointer_frame->is_scroll_stop ||
      pointer_frame->delta_x != 0 ||
      pointer_frame->delta_y != 0)
    {
      /* Axes can stop independently, if we stop on one axis but have a
       * delta on the other, we don't count it as a stop event.
       */
      if (pointer_frame->is_scroll_stop &&
          pointer_frame->delta_x == 0 &&
          pointer_frame->delta_y == 0)
        is_stop = TRUE;

      flush_smooth_scroll_event (seat,
                                 pointer_frame->delta_x,
                                 pointer_frame->delta_y,
                                 is_stop);

      pointer_frame->delta_x = 0;
      pointer_frame->delta_y = 0;
      pointer_frame->is_scroll_stop = FALSE;
    }
}

static void
gdk_wayland_seat_flush_frame_event (GdkWaylandSeat *seat)
{
  if (seat->pointer_info.frame.event)
    {
      _gdk_wayland_display_deliver_event (gdk_seat_get_display (GDK_SEAT (seat)),
                                          seat->pointer_info.frame.event);
      seat->pointer_info.frame.event = NULL;
    }
  else
    {
      flush_scroll_event (seat, &seat->pointer_info.frame);
      seat->pointer_info.frame.source = 0;
    }
}

static GdkEvent *
gdk_wayland_seat_get_frame_event (GdkWaylandSeat *seat,
                                  GdkEventType    evtype)
{
  if (seat->pointer_info.frame.event &&
      seat->pointer_info.frame.event->type != evtype)
    gdk_wayland_seat_flush_frame_event (seat);

  seat->pointer_info.frame.event = gdk_event_new (evtype);
  return seat->pointer_info.frame.event;
}

static void
pointer_handle_enter (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface,
                      wl_fixed_t         sx,
                      wl_fixed_t         sy)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (seat->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (display_wayland, serial);

  seat->pointer_info.focus = wl_surface_get_user_data (surface);
  g_object_ref (seat->pointer_info.focus);

  seat->pointer_info.button_modifiers = 0;

  seat->pointer_info.surface_x = wl_fixed_to_double (sx);
  seat->pointer_info.surface_y = wl_fixed_to_double (sy);
  seat->pointer_info.enter_serial = serial;

  event = gdk_wayland_seat_get_frame_event (seat, GDK_ENTER_NOTIFY);
  event->crossing.window = g_object_ref (seat->pointer_info.focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  gdk_wayland_device_update_window_cursor (seat->master_pointer);

  get_coordinates (seat->master_pointer,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  GDK_NOTE (EVENTS,
            g_message ("enter, seat %p surface %p",
                       seat, seat->pointer_info.focus));

  if (display_wayland->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_leave (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  if (!seat->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (display_wayland, serial);

  event = gdk_wayland_seat_get_frame_event (seat, GDK_LEAVE_NOTIFY);
  event->crossing.window = g_object_ref (seat->pointer_info.focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  gdk_wayland_device_update_window_cursor (seat->master_pointer);

  get_coordinates (seat->master_pointer,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  GDK_NOTE (EVENTS,
            g_message ("leave, seat %p surface %p",
                       seat, seat->pointer_info.focus));

  g_object_unref (seat->pointer_info.focus);
  seat->pointer_info.focus = NULL;
  if (seat->cursor)
    gdk_wayland_pointer_stop_cursor_animation (&seat->pointer_info);

  if (display_wayland->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_motion (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           time,
                       wl_fixed_t         sx,
                       wl_fixed_t         sy)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = time;
  seat->pointer_info.surface_x = wl_fixed_to_double (sx);
  seat->pointer_info.surface_y = wl_fixed_to_double (sy);

  event = gdk_wayland_seat_get_frame_event (seat, GDK_MOTION_NOTIFY);
  event->motion.window = g_object_ref (seat->pointer_info.focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));
  event->motion.time = time;
  event->motion.axes = NULL;
  event->motion.state = device_get_modifiers (seat->master_pointer);
  event->motion.is_hint = 0;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (seat->master_pointer,
                   &event->motion.x,
                   &event->motion.y,
                   &event->motion.x_root,
                   &event->motion.y_root);

  GDK_NOTE (EVENTS,
            g_message ("motion %f %f, seat %p state %d",
                       wl_fixed_to_double (sx), wl_fixed_to_double (sy),
		       seat, event->motion.state));

  if (display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_button (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           serial,
                       uint32_t           time,
                       uint32_t           button,
                       uint32_t           state)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;
  uint32_t modifier;
  int gdk_button;

  if (!seat->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (display, serial);

  switch (button)
    {
    case BTN_LEFT:
      gdk_button = GDK_BUTTON_PRIMARY;
      break;
    case BTN_MIDDLE:
      gdk_button = GDK_BUTTON_MIDDLE;
      break;
    case BTN_RIGHT:
      gdk_button = GDK_BUTTON_SECONDARY;
      break;
    default:
       /* For compatibility reasons, all additional buttons go after the old 4-7 scroll ones */
      gdk_button = button - BUTTON_BASE + 4;
      break;
    }

  seat->pointer_info.time = time;
  if (state)
    seat->pointer_info.press_serial = serial;

  event = gdk_wayland_seat_get_frame_event (seat,
                                            state ? GDK_BUTTON_PRESS :
                                            GDK_BUTTON_RELEASE);
  event->button.window = g_object_ref (seat->pointer_info.focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));
  event->button.time = time;
  event->button.axes = NULL;
  event->button.state = device_get_modifiers (seat->master_pointer);
  event->button.button = gdk_button;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (seat->master_pointer,
                   &event->button.x,
                   &event->button.y,
                   &event->button.x_root,
                   &event->button.y_root);

  modifier = 1 << (8 + gdk_button - 1);
  if (state)
    seat->pointer_info.button_modifiers |= modifier;
  else
    seat->pointer_info.button_modifiers &= ~modifier;

  GDK_NOTE (EVENTS,
	    g_message ("button %d %s, seat %p state %d",
		       event->button.button,
		       state ? "press" : "release",
                       seat,
                       event->button.state));

  if (display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

#ifdef G_ENABLE_DEBUG

const char *
get_axis_name (uint32_t axis)
{
  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      return "horizontal";
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      return "vertical";
    default:
      return "unknown";
    }
}

#endif

static void
pointer_handle_axis (void              *data,
                     struct wl_pointer *pointer,
                     uint32_t           time,
                     uint32_t           axis,
                     wl_fixed_t         value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!seat->pointer_info.focus)
    return;

  /* get the delta and convert it into the expected range */
  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->delta_y = wl_fixed_to_double (value) / 10.0;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->delta_x = wl_fixed_to_double (value) / 10.0;
      break;
    default:
      g_return_if_reached ();
    }

  seat->pointer_info.time = time;

  GDK_NOTE (EVENTS,
            g_message ("scroll, axis %s, value %f, seat %p",
                       get_axis_name (axis), wl_fixed_to_double (value) / 10.0,
                       seat));

  if (display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_frame (void              *data,
                      struct wl_pointer *pointer)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("frame, seat %p", seat));

  gdk_wayland_seat_flush_frame_event (seat);
}

#ifdef G_ENABLE_DEBUG

static const char *
get_axis_source_name (enum wl_pointer_axis_source source)
{
  switch (source)
    {
    case WL_POINTER_AXIS_SOURCE_WHEEL:
      return "wheel";
    case WL_POINTER_AXIS_SOURCE_FINGER:
      return "finger";
    case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
      return "continuous";
    default:
      return "unknown";
    }
}

#endif

static void
pointer_handle_axis_source (void                        *data,
                            struct wl_pointer           *pointer,
                            enum wl_pointer_axis_source  source)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  pointer_frame->source = source;

  GDK_NOTE (EVENTS,
            g_message ("axis source %s, seat %p", get_axis_source_name (source), seat));
}

static void
pointer_handle_axis_stop (void              *data,
                          struct wl_pointer *pointer,
                          uint32_t           time,
                          uint32_t           axis)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = time;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->delta_y = 0;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->delta_x = 0;
      break;
    default:
      g_return_if_reached ();
    }

  pointer_frame->is_scroll_stop = TRUE;

  GDK_NOTE (EVENTS,
            g_message ("axis %s stop, seat %p", get_axis_name (axis), seat));
}

static void
pointer_handle_axis_discrete (void              *data,
                              struct wl_pointer *pointer,
                              uint32_t           axis,
                              int32_t            value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_info.frame;

  if (!seat->pointer_info.focus)
    return;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->discrete_y = value;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->discrete_x = value;
      break;
    default:
      g_return_if_reached ();
    }

  GDK_NOTE (EVENTS,
            g_message ("discrete scroll, axis %s, value %d, seat %p",
                       get_axis_name (axis), value, seat));
}

static void
keyboard_handle_keymap (void               *data,
                        struct wl_keyboard *keyboard,
                        uint32_t            format,
                        int                 fd,
                        uint32_t            size)
{
  GdkWaylandSeat *seat = data;
  PangoDirection direction;

  direction = gdk_keymap_get_direction (seat->keymap);

  _gdk_wayland_keymap_update_from_fd (seat->keymap, format, fd, size);

  g_signal_emit_by_name (seat->keymap, "keys-changed");
  g_signal_emit_by_name (seat->keymap, "state-changed");

  if (direction != gdk_keymap_get_direction (seat->keymap))
    g_signal_emit_by_name (seat->keymap, "direction-changed");
}

static void
keyboard_handle_enter (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface,
                       struct wl_array    *keys)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (display, serial);

  seat->keyboard_focus = wl_surface_get_user_data (surface);
  g_object_ref (seat->keyboard_focus);
  seat->repeat_key = 0;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (seat->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = TRUE;
  gdk_event_set_device (event, seat->master_keyboard);
  gdk_event_set_source_device (event, seat->keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));

  GDK_NOTE (EVENTS,
            g_message ("focus in, seat %p surface %p",
                       seat, seat->keyboard_focus));

  _gdk_wayland_display_deliver_event (seat->display, event);

  if (seat->pending_selection != GDK_NONE)
    {
      emit_selection_owner_change (seat->keyboard_focus,
                                   seat->pending_selection);
      seat->pending_selection = GDK_NONE;
    }
}

static void stop_key_repeat (GdkWaylandSeat *seat);

static void
keyboard_handle_leave (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!seat->keyboard_focus)
    return;

  /* gdk_window_is_destroyed() might already return TRUE for
   * seat->keyboard_focus here, which would happen if we destroyed the
   * window before loosing keyboard focus.
   */
  stop_key_repeat (seat);

  _gdk_wayland_display_update_serial (display, serial);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (seat->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = FALSE;
  gdk_event_set_device (event, seat->master_keyboard);
  gdk_event_set_source_device (event, seat->keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_keyboard));

  g_object_unref (seat->keyboard_focus);
  seat->keyboard_focus = NULL;
  seat->repeat_key = 0;
  seat->key_modifiers = 0;

  GDK_NOTE (EVENTS,
            g_message ("focus out, seat %p surface %p",
                       seat, seat->keyboard_focus));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static gboolean keyboard_repeat (gpointer data);

static void
translate_keyboard_string (GdkEventKey *event)
{
  gunichar c = 0;
  gchar buf[7];

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->string = NULL;

  if (event->keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib */
      if (event->state & GDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ')
            c &= 0x1F;
          else if (c == '2')
            {
              event->string = g_memdup ("\0\0", 2);
              event->length = 1;
              buf[0] = '\0';
              return;
            }
          else if (c >= '3' && c <= '7')
            c -= ('3' - '\033');
          else if (c == '8')
            c = '\177';
          else if (c == '/')
            c = '_' & 0x1F;
        }

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->string = g_locale_from_utf8 (buf, len,
                                          NULL, &bytes_written,
                                          NULL);
      if (event->string)
        event->length = bytes_written;
    }
  else if (event->keyval == GDK_KEY_Escape)
    {
      event->length = 1;
      event->string = g_strdup ("\033");
    }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_KP_Enter)
    {
      event->length = 1;
      event->string = g_strdup ("\r");
    }

  if (!event->string)
    {
      event->length = 0;
      event->string = g_strdup ("");
    }
}

static GSettings *
get_keyboard_settings (GdkWaylandSeat *seat)
{
  if (!seat->keyboard_settings)
    {
      GSettingsSchemaSource *source;
      GSettingsSchema *schema;

      source = g_settings_schema_source_get_default ();
      schema = g_settings_schema_source_lookup (source, "org.gnome.settings-daemon.peripherals.keyboard", FALSE);
      if (schema != NULL)
        {
          seat->keyboard_settings = g_settings_new_full (schema, NULL, NULL);
          g_settings_schema_unref (schema);
        }
    }

  return seat->keyboard_settings;
}

static gboolean
get_key_repeat (GdkWaylandSeat *seat,
                guint          *delay,
                guint          *interval)
{
  gboolean repeat;

  if (seat->have_server_repeat)
    {
      if (seat->server_repeat_rate > 0)
        {
          repeat = TRUE;
          *delay = seat->server_repeat_delay;
          *interval = (1000 / seat->server_repeat_rate);
        }
      else
        {
          repeat = FALSE;
        }
    }
  else
    {
      GSettings *keyboard_settings = get_keyboard_settings (seat);

      if (keyboard_settings)
        {
          repeat = g_settings_get_boolean (keyboard_settings, "repeat");
          *delay = g_settings_get_uint (keyboard_settings, "delay");
          *interval = g_settings_get_uint (keyboard_settings, "repeat-interval");
        }
      else
        {
          repeat = TRUE;
          *delay = 400;
          *interval = 80;
        }
    }

  return repeat;
}

static void
stop_key_repeat (GdkWaylandSeat *seat)
{
  if (seat->repeat_timer)
    {
      g_source_remove (seat->repeat_timer);
      seat->repeat_timer = 0;
    }

  g_clear_pointer (&seat->repeat_callback, wl_callback_destroy);
}

static void
deliver_key_event (GdkWaylandSeat *seat,
                   uint32_t        time_,
                   uint32_t        key,
                   uint32_t        state,
                   gboolean        from_key_repeat)
{
  GdkEvent *event;
  struct xkb_state *xkb_state;
  struct xkb_keymap *xkb_keymap;
  GdkKeymap *keymap;
  xkb_keysym_t sym;
  int layout;
  guint delay, interval, timeout;
  gint64 begin_time, now;

  begin_time = g_get_monotonic_time ();

  stop_key_repeat (seat);

  keymap = seat->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);

  sym = xkb_state_key_get_one_sym (xkb_state, key);
  layout = xkb_state_key_get_layout (xkb_state, key);
  if (sym == XKB_KEY_NoSymbol)
    return;

  seat->pointer_info.time = time_;
  seat->key_modifiers = gdk_keymap_get_modifier_state (keymap);

  event = gdk_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
  event->key.window = seat->keyboard_focus ? g_object_ref (seat->keyboard_focus) : NULL;
  gdk_event_set_device (event, seat->master_keyboard);
  gdk_event_set_source_device (event, seat->keyboard);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->key.time = time_;
  event->key.state = device_get_modifiers (seat->master_pointer);
  event->key.group = layout;
  event->key.hardware_keycode = key;
  gdk_event_set_scancode (event, key);
  event->key.keyval = sym;
  event->key.is_modifier = _gdk_wayland_keymap_key_is_modifier (keymap, key);

  translate_keyboard_string (&event->key);

  _gdk_wayland_display_deliver_event (seat->display, event);

  GDK_NOTE (EVENTS,
            g_message ("keyboard %s event%s, code %d, sym %d, "
                       "string %s, mods 0x%x",
                       (state ? "press" : "release"),
                       (from_key_repeat ? " (repeat)" : ""),
                       event->key.hardware_keycode, event->key.keyval,
                       event->key.string, event->key.state));

  if (!xkb_keymap_key_repeats (xkb_keymap, key))
    return;

  if (!get_key_repeat (seat, &delay, &interval))
    return;

  if (!from_key_repeat)
    {
      if (state) /* Another key is pressed */
        {
          seat->repeat_key = key;
        }
      else if (seat->repeat_key == key) /* Repeated key is released */
        {
          seat->repeat_key = 0;
        }
    }

  if (!seat->repeat_key)
    return;

  seat->repeat_count++;

  interval *= 1000L;
  delay *= 1000L;

  now = g_get_monotonic_time ();

  if (seat->repeat_count == 1)
    seat->repeat_deadline = begin_time + delay;
  else if (seat->repeat_deadline + interval > now)
    seat->repeat_deadline += interval;
  else
    /* frame delay caused us to miss repeat deadline */
    seat->repeat_deadline = now;

  timeout = (seat->repeat_deadline - now) / 1000L;

  seat->repeat_timer =
    gdk_threads_add_timeout (timeout, keyboard_repeat, seat);
  g_source_set_name_by_id (seat->repeat_timer, "[gtk+] keyboard_repeat");
}

static void
sync_after_repeat_callback (void               *data,
                            struct wl_callback *callback,
                            uint32_t            time)
{
  GdkWaylandSeat *seat = data;

  g_clear_pointer (&seat->repeat_callback, wl_callback_destroy);
  deliver_key_event (seat, seat->keyboard_time, seat->repeat_key, 1, TRUE);
}

static const struct wl_callback_listener sync_after_repeat_callback_listener = {
  sync_after_repeat_callback
};

static gboolean
keyboard_repeat (gpointer data)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  /* Ping the server and wait for the timeout.  We won't process
   * key repeat until it responds, since a hung server could lead
   * to a delayed key release event. We don't want to generate
   * repeat events long after the user released the key, just because
   * the server is tardy in telling us the user released the key.
   */
  seat->repeat_callback = wl_display_sync (display->wl_display);

  wl_callback_add_listener (seat->repeat_callback,
                            &sync_after_repeat_callback_listener,
                            seat);

  seat->repeat_timer = 0;
  return G_SOURCE_REMOVE;
}

static void
keyboard_handle_key (void               *data,
                     struct wl_keyboard *keyboard,
                     uint32_t            serial,
                     uint32_t            time,
                     uint32_t            key,
                     uint32_t            state_w)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!seat->keyboard_focus)
    return;

  seat->keyboard_time = time;
  seat->keyboard_key_serial = serial;
  seat->repeat_count = 0;
  _gdk_wayland_display_update_serial (display, serial);
  deliver_key_event (data, time, key + 8, state_w, FALSE);

}

static void
keyboard_handle_modifiers (void               *data,
                           struct wl_keyboard *keyboard,
                           uint32_t            serial,
                           uint32_t            mods_depressed,
                           uint32_t            mods_latched,
                           uint32_t            mods_locked,
                           uint32_t            group)
{
  GdkWaylandSeat *seat = data;
  GdkKeymap *keymap;
  struct xkb_state *xkb_state;
  PangoDirection direction;

  keymap = seat->keymap;
  direction = gdk_keymap_get_direction (keymap);
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);

  xkb_state_update_mask (xkb_state, mods_depressed, mods_latched, mods_locked, group, 0, 0);

  seat->key_modifiers = gdk_keymap_get_modifier_state (keymap);

  g_signal_emit_by_name (keymap, "state-changed");
  if (direction != gdk_keymap_get_direction (keymap))
    g_signal_emit_by_name (keymap, "direction-changed");
}

static void
keyboard_handle_repeat_info (void               *data,
                             struct wl_keyboard *keyboard,
                             int32_t             rate,
                             int32_t             delay)
{
  GdkWaylandSeat *seat = data;

  seat->have_server_repeat = TRUE;
  seat->server_repeat_rate = rate;
  seat->server_repeat_delay = delay;
}

static GdkWaylandTouchData *
gdk_wayland_seat_add_touch (GdkWaylandSeat    *seat,
                            uint32_t           id,
                            struct wl_surface *surface)
{
  GdkWaylandTouchData *touch;

  touch = g_new0 (GdkWaylandTouchData, 1);
  touch->id = id;
  touch->window = wl_surface_get_user_data (surface);
  touch->initial_touch = (g_hash_table_size (seat->touches) == 0);

  g_hash_table_insert (seat->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

static GdkWaylandTouchData *
gdk_wayland_seat_get_touch (GdkWaylandSeat *seat,
                            uint32_t        id)
{
  return g_hash_table_lookup (seat->touches, GUINT_TO_POINTER (id));
}

static void
gdk_wayland_seat_remove_touch (GdkWaylandSeat *seat,
                               uint32_t        id)
{
  g_hash_table_remove (seat->touches, GUINT_TO_POINTER (id));
}

static GdkEvent *
_create_touch_event (GdkWaylandSeat       *seat,
                     GdkWaylandTouchData  *touch,
                     GdkEventType          evtype,
                     uint32_t              time)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  gint x_root, y_root;
  GdkEvent *event;

  event = gdk_event_new (evtype);
  event->touch.window = g_object_ref (touch->window);
  gdk_event_set_device (event, seat->touch_master);
  gdk_event_set_source_device (event, seat->touch);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->touch.time = time;
  event->touch.state = device_get_modifiers (seat->touch_master);
  gdk_event_set_screen (event, display->screen);
  event->touch.sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);

  if (touch->initial_touch)
    {
      gdk_event_set_pointer_emulated (event, TRUE);
      event->touch.emulating_pointer = TRUE;
    }

  gdk_window_get_root_coords (touch->window,
                              touch->x, touch->y,
                              &x_root, &y_root);

  event->touch.x = touch->x;
  event->touch.y = touch->y;
  event->touch.x_root = x_root;
  event->touch.y_root = y_root;

  return event;
}

static void
mimic_pointer_emulating_touch_info (GdkDevice           *device,
                                    GdkWaylandTouchData *touch)
{
  GdkWaylandPointerData *pointer;

  pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  g_set_object (&pointer->focus, touch->window);
  pointer->press_serial = pointer->enter_serial = touch->touch_down_serial;
  pointer->surface_x = touch->x;
  pointer->surface_y = touch->y;
}

static void
touch_handle_master_pointer_crossing (GdkWaylandSeat      *seat,
                                      GdkWaylandTouchData *touch,
                                      uint32_t             time)
{
  GdkWaylandPointerData *pointer;

  pointer = GDK_WAYLAND_DEVICE (seat->touch_master)->pointer;

  if (pointer->focus == touch->window)
    return;

  if (pointer->focus)
    {
      emulate_touch_crossing (pointer->focus, NULL,
                              seat->touch_master, seat->touch, touch,
                              GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL, time);
    }

  if (touch->window)
    {
      emulate_touch_crossing (touch->window, NULL,
                              seat->touch_master, seat->touch, touch,
                              GDK_ENTER_NOTIFY, GDK_CROSSING_NORMAL, time);
    }
}

static void
touch_handle_down (void              *data,
                   struct wl_touch   *wl_touch,
                   uint32_t           serial,
                   uint32_t           time,
                   struct wl_surface *wl_surface,
                   int32_t            id,
                   wl_fixed_t         x,
                   wl_fixed_t         y)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  _gdk_wayland_display_update_serial (display, serial);

  if (!wl_surface)
    return;

  touch = gdk_wayland_seat_add_touch (seat, id, wl_surface);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);
  touch->touch_down_serial = serial;

  event = _create_touch_event (seat, touch, GDK_TOUCH_BEGIN, time);

  if (touch->initial_touch)
    {
      touch_handle_master_pointer_crossing (seat, touch, time);
      GDK_WAYLAND_DEVICE(seat->touch_master)->emulating_touch = touch;
      mimic_pointer_emulating_touch_info (seat->touch_master, touch);
    }

  GDK_NOTE (EVENTS,
            g_message ("touch begin %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
touch_handle_up (void            *data,
                 struct wl_touch *wl_touch,
                 uint32_t         serial,
                 uint32_t         time,
                 int32_t          id)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  _gdk_wayland_display_update_serial (display, serial);

  touch = gdk_wayland_seat_get_touch (seat, id);
  if (!touch)
    return;

  event = _create_touch_event (seat, touch, GDK_TOUCH_END, time);

  GDK_NOTE (EVENTS,
            g_message ("touch end %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (seat->display, event);

  if (touch->initial_touch)
    GDK_WAYLAND_DEVICE(seat->touch_master)->emulating_touch = NULL;

  gdk_wayland_seat_remove_touch (seat, id);
}

static void
touch_handle_motion (void            *data,
                     struct wl_touch *wl_touch,
                     uint32_t         time,
                     int32_t          id,
                     wl_fixed_t       x,
                     wl_fixed_t       y)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = gdk_wayland_seat_get_touch (seat, id);
  if (!touch)
    return;

  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);

  if (touch->initial_touch)
    mimic_pointer_emulating_touch_info (seat->touch_master, touch);

  event = _create_touch_event (seat, touch, GDK_TOUCH_UPDATE, time);

  GDK_NOTE (EVENTS,
            g_message ("touch update %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
touch_handle_frame (void            *data,
                    struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel (void            *data,
                     struct wl_touch *wl_touch)
{
  GdkWaylandSeat *wayland_seat = data;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GdkEvent *event;

  if (GDK_WAYLAND_DEVICE (wayland_seat->touch_master)->emulating_touch)
    {
      touch = GDK_WAYLAND_DEVICE (wayland_seat->touch_master)->emulating_touch;
      GDK_WAYLAND_DEVICE (wayland_seat->touch_master)->emulating_touch = NULL;
    }

  g_hash_table_iter_init (&iter, wayland_seat->touches);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      event = _create_touch_event (wayland_seat, touch, GDK_TOUCH_CANCEL,
                                   GDK_CURRENT_TIME);
      _gdk_wayland_display_deliver_event (wayland_seat->display, event);
      g_hash_table_iter_remove (&iter);
    }

  GDK_NOTE (EVENTS, g_message ("touch cancel"));
}

static void
emit_gesture_swipe_event (GdkWaylandSeat          *seat,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint32                  n_fingers,
                          gdouble                  dx,
                          gdouble                  dy)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = _time;

  event = gdk_event_new (GDK_TOUCHPAD_SWIPE);
  event->touchpad_swipe.phase = phase;
  event->touchpad_swipe.window = g_object_ref (seat->pointer_info.focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->touchpad_swipe.time = _time;
  event->touchpad_swipe.state = device_get_modifiers (seat->master_pointer);
  gdk_event_set_screen (event, display->screen);
  event->touchpad_swipe.dx = dx;
  event->touchpad_swipe.dy = dy;
  event->touchpad_swipe.n_fingers = n_fingers;

  get_coordinates (seat->master_pointer,
                   &event->touchpad_swipe.x,
                   &event->touchpad_swipe.y,
                   &event->touchpad_swipe.x_root,
                   &event->touchpad_swipe.y_root);

  GDK_NOTE (EVENTS,
            g_message ("swipe event %d, coords: %f %f, seat %p state %d",
                       event->type, event->touchpad_swipe.x,
                       event->touchpad_swipe.y, seat,
                       event->touchpad_swipe.state));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_swipe_begin (void                                *data,
                     struct zwp_pointer_gesture_swipe_v1 *swipe,
                     uint32_t                             serial,
                     uint32_t                             time,
                     struct wl_surface                   *surface,
                     uint32_t                             fingers)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  _gdk_wayland_display_update_serial (display, serial);

  emit_gesture_swipe_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_swipe_update (void                                *data,
                      struct zwp_pointer_gesture_swipe_v1 *swipe,
                      uint32_t                             time,
                      wl_fixed_t                           dx,
                      wl_fixed_t                           dy)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_swipe_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE,
                            time,
                            seat->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy));
}

static void
gesture_swipe_end (void                                *data,
                   struct zwp_pointer_gesture_swipe_v1 *swipe,
                   uint32_t                             serial,
                   uint32_t                             time,
                   int32_t                              cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkTouchpadGesturePhase phase;

  _gdk_wayland_display_update_serial (display, serial);

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_swipe_event (seat, phase, time,
                            seat->gesture_n_fingers, 0, 0);
}

static void
emit_gesture_pinch_event (GdkWaylandSeat          *seat,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint                    n_fingers,
                          gdouble                  dx,
                          gdouble                  dy,
                          gdouble                  scale,
                          gdouble                  angle_delta)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!seat->pointer_info.focus)
    return;

  seat->pointer_info.time = _time;

  event = gdk_event_new (GDK_TOUCHPAD_PINCH);
  event->touchpad_pinch.phase = phase;
  event->touchpad_pinch.window = g_object_ref (seat->pointer_info.focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->touchpad_pinch.time = _time;
  event->touchpad_pinch.state = device_get_modifiers (seat->master_pointer);
  gdk_event_set_screen (event, display->screen);
  event->touchpad_pinch.dx = dx;
  event->touchpad_pinch.dy = dy;
  event->touchpad_pinch.scale = scale;
  event->touchpad_pinch.angle_delta = angle_delta * G_PI / 180;
  event->touchpad_pinch.n_fingers = n_fingers;

  get_coordinates (seat->master_pointer,
                   &event->touchpad_pinch.x,
                   &event->touchpad_pinch.y,
                   &event->touchpad_pinch.x_root,
                   &event->touchpad_pinch.y_root);

  GDK_NOTE (EVENTS,
            g_message ("pinch event %d, coords: %f %f, seat %p state %d",
                       event->type, event->touchpad_pinch.x,
                       event->touchpad_pinch.y, seat,
                       event->touchpad_pinch.state));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_pinch_begin (void                                *data,
                     struct zwp_pointer_gesture_pinch_v1 *pinch,
                     uint32_t                             serial,
                     uint32_t                             time,
                     struct wl_surface                   *surface,
                     uint32_t                             fingers)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  _gdk_wayland_display_update_serial (display, serial);
  emit_gesture_pinch_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0, 1, 0);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_pinch_update (void                                *data,
                      struct zwp_pointer_gesture_pinch_v1 *pinch,
                      uint32_t                             time,
                      wl_fixed_t                           dx,
                      wl_fixed_t                           dy,
                      wl_fixed_t                           scale,
                      wl_fixed_t                           rotation)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_pinch_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE, time,
                            seat->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy),
                            wl_fixed_to_double (scale),
                            wl_fixed_to_double (rotation));
}

static void
gesture_pinch_end (void                                *data,
                   struct zwp_pointer_gesture_pinch_v1 *pinch,
                   uint32_t                             serial,
                   uint32_t                             time,
                   int32_t                              cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkTouchpadGesturePhase phase;

  _gdk_wayland_display_update_serial (display, serial);

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_pinch_event (seat, phase,
                            time, seat->gesture_n_fingers,
                            0, 0, 1, 0);
}

static GdkDevice *
tablet_select_device_for_tool (GdkWaylandTabletData *tablet,
                               GdkDeviceTool        *tool)
{
  GdkDevice *device;

  if (gdk_device_tool_get_tool_type (tool) == GDK_DEVICE_TOOL_TYPE_ERASER)
    device = tablet->eraser_device;
  else
    device = tablet->stylus_device;

  return device;
}

static void
_gdk_wayland_seat_remove_tool (GdkWaylandSeat           *seat,
                               GdkWaylandTabletToolData *tool)
{
  g_clear_pointer (&tool->shape_device, wp_cursor_shape_device_v1_destroy);

  seat->tablet_tools = g_list_remove (seat->tablet_tools, tool);

  gdk_seat_tool_removed (GDK_SEAT (seat), tool->tool);

  zwp_tablet_tool_v2_destroy (tool->wp_tablet_tool);
  g_object_unref (tool->tool);
  g_free (tool);
}

static void
_gdk_wayland_seat_remove_tablet (GdkWaylandSeat       *seat,
                                 GdkWaylandTabletData *tablet)
{
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);

  seat->tablets = g_list_remove (seat->tablets, tablet);

  zwp_tablet_v2_destroy (tablet->wp_tablet);

  while (tablet->pads)
    {
      GdkWaylandTabletPadData *pad = tablet->pads->data;

      pad->current_tablet = NULL;
      tablet->pads = g_list_remove (tablet->pads, pad);
    }

  device_manager->devices =
    g_list_remove (device_manager->devices, tablet->master);
  device_manager->devices =
    g_list_remove (device_manager->devices, tablet->stylus_device);
  device_manager->devices =
    g_list_remove (device_manager->devices, tablet->eraser_device);

  g_signal_emit_by_name (device_manager, "device-removed",
                         tablet->stylus_device);
  g_signal_emit_by_name (device_manager, "device-removed",
                         tablet->eraser_device);
  g_signal_emit_by_name (device_manager, "device-removed",
                         tablet->master);

  _gdk_device_set_associated_device (tablet->master, NULL);
  _gdk_device_set_associated_device (tablet->stylus_device, NULL);
  _gdk_device_set_associated_device (tablet->eraser_device, NULL);

  if (tablet->pointer_info.focus)
    g_object_unref (tablet->pointer_info.focus);

  if (tablet->axes)
    g_free (tablet->axes);

  wl_surface_destroy (tablet->pointer_info.pointer_surface);
  g_object_unref (tablet->master);
  g_object_unref (tablet->stylus_device);
  g_object_unref (tablet->eraser_device);
  g_free (tablet);
}

static void
_gdk_wayland_seat_remove_tablet_pad (GdkWaylandSeat          *seat,
                                     GdkWaylandTabletPadData *pad)
{
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);

  seat->tablet_pads = g_list_remove (seat->tablet_pads, pad);

  if (pad->device)
    {
      device_manager->devices =
        g_list_remove (device_manager->devices, pad->device);
      g_signal_emit_by_name (device_manager, "device-removed", pad->device);

      _gdk_device_set_associated_device (pad->device, NULL);

      g_object_unref (pad->device);
    }

  g_free (pad);
}

static GdkWaylandTabletPadGroupData *
tablet_pad_lookup_button_group (GdkWaylandTabletPadData *pad,
                                uint32_t                 button)
{
  GdkWaylandTabletPadGroupData *group;
  GList *l;

  for (l = pad->mode_groups; l; l = l->next)
    {
      group = l->data;

      if (g_list_find (group->buttons, GUINT_TO_POINTER (button)))
        return group;
    }

  return NULL;
}

static void
tablet_handle_name (void                 *data,
                    struct zwp_tablet_v2 *wp_tablet,
                    const char           *name)
{
  GdkWaylandTabletData *tablet = data;

  tablet->name = g_strdup (name);
}

static void
tablet_handle_id (void                 *data,
                  struct zwp_tablet_v2 *wp_tablet,
                  uint32_t              vid,
                  uint32_t              pid)
{
  GdkWaylandTabletData *tablet = data;

  tablet->vid = vid;
  tablet->pid = pid;
}

static void
tablet_handle_path (void                 *data,
                    struct zwp_tablet_v2 *wp_tablet,
                    const char           *path)
{
  GdkWaylandTabletData *tablet = data;

  tablet->path = g_strdup (path);
}

static void
tablet_handle_done (void                 *data,
                    struct zwp_tablet_v2 *wp_tablet)
{
  GdkWaylandTabletData *tablet = data;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tablet->seat);
  GdkDisplay *display = gdk_seat_get_display (GDK_SEAT (seat));
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);
  GdkDevice *master, *stylus_device, *eraser_device;
  gchar *master_name, *eraser_name;
  gchar *vid, *pid;

  vid = g_strdup_printf ("%.4x", tablet->vid);
  pid = g_strdup_printf ("%.4x", tablet->pid);

  master_name = g_strdup_printf ("Master pointer for %s", tablet->name);
  master = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                         "name", master_name,
                         "type", GDK_DEVICE_TYPE_MASTER,
                         "input-source", GDK_SOURCE_MOUSE,
                         "input-mode", GDK_MODE_SCREEN,
                         "has-cursor", TRUE,
                         "display", display,
                         "device-manager", device_manager,
                         "seat", seat,
                         NULL);
  GDK_WAYLAND_DEVICE (master)->pointer = &tablet->pointer_info;

  eraser_name = g_strconcat (tablet->name, " (Eraser)", NULL);

  stylus_device = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                "name", tablet->name,
                                "type", GDK_DEVICE_TYPE_SLAVE,
                                "input-source", GDK_SOURCE_PEN,
                                "input-mode", GDK_MODE_SCREEN,
                                "has-cursor", FALSE,
                                "display", display,
                                "device-manager", device_manager,
                                "seat", seat,
                                "vendor-id", vid,
                                "product-id", pid,
                                NULL);

  eraser_device = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                "name", eraser_name,
                                "type", GDK_DEVICE_TYPE_SLAVE,
                                "input-source", GDK_SOURCE_ERASER,
                                "input-mode", GDK_MODE_SCREEN,
                                "has-cursor", FALSE,
                                "display", display,
                                "device-manager", device_manager,
                                "seat", seat,
                                "vendor-id", vid,
                                "product-id", pid,
                                NULL);

  tablet->master = master;
  device_manager->devices =
    g_list_prepend (device_manager->devices, tablet->master);
  g_signal_emit_by_name (device_manager, "device-added", master);

  init_pointer_data (&tablet->pointer_info, display, tablet->master);

  tablet->stylus_device = stylus_device;
  device_manager->devices =
    g_list_prepend (device_manager->devices, tablet->stylus_device);
  g_signal_emit_by_name (device_manager, "device-added", stylus_device);

  tablet->eraser_device = eraser_device;
  device_manager->devices =
    g_list_prepend (device_manager->devices, tablet->eraser_device);
  g_signal_emit_by_name (device_manager, "device-added", eraser_device);

  _gdk_device_set_associated_device (master, seat->master_keyboard);
  _gdk_device_set_associated_device (stylus_device, master);
  _gdk_device_set_associated_device (eraser_device, master);

  g_free (eraser_name);
  g_free (master_name);
  g_free (vid);
  g_free (pid);
}

static void
tablet_handle_removed (void                 *data,
                       struct zwp_tablet_v2 *wp_tablet)
{
  GdkWaylandTabletData *tablet = data;

  _gdk_wayland_seat_remove_tablet (GDK_WAYLAND_SEAT (tablet->seat), tablet);
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
  pointer_handle_frame,
  pointer_handle_axis_source,
  pointer_handle_axis_stop,
  pointer_handle_axis_discrete,
};

static const struct wl_keyboard_listener keyboard_listener = {
  keyboard_handle_keymap,
  keyboard_handle_enter,
  keyboard_handle_leave,
  keyboard_handle_key,
  keyboard_handle_modifiers,
  keyboard_handle_repeat_info,
};

static const struct wl_touch_listener touch_listener = {
  touch_handle_down,
  touch_handle_up,
  touch_handle_motion,
  touch_handle_frame,
  touch_handle_cancel
};

static const struct zwp_pointer_gesture_swipe_v1_listener gesture_swipe_listener = {
  gesture_swipe_begin,
  gesture_swipe_update,
  gesture_swipe_end
};

static const struct zwp_pointer_gesture_pinch_v1_listener gesture_pinch_listener = {
  gesture_pinch_begin,
  gesture_pinch_update,
  gesture_pinch_end
};

static const struct zwp_tablet_v2_listener tablet_listener = {
  tablet_handle_name,
  tablet_handle_id,
  tablet_handle_path,
  tablet_handle_done,
  tablet_handle_removed,
};

static void
seat_handle_capabilities (void                    *data,
                          struct wl_seat          *wl_seat,
                          enum wl_seat_capability  caps)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);

  GDK_NOTE (MISC,
            g_message ("seat %p with %s%s%s", wl_seat,
                       (caps & WL_SEAT_CAPABILITY_POINTER) ? " pointer, " : "",
                       (caps & WL_SEAT_CAPABILITY_KEYBOARD) ? " keyboard, " : "",
                       (caps & WL_SEAT_CAPABILITY_TOUCH) ? " touch" : ""));

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !seat->wl_pointer)
    {
      seat->wl_pointer = wl_seat_get_pointer (wl_seat);
      wl_pointer_set_user_data (seat->wl_pointer, seat);
      wl_pointer_add_listener (seat->wl_pointer, &pointer_listener, seat);

      seat->pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                    "name", "Wayland Pointer",
                                    "type", GDK_DEVICE_TYPE_SLAVE,
                                    "input-source", GDK_SOURCE_MOUSE,
                                    "input-mode", GDK_MODE_SCREEN,
                                    "has-cursor", TRUE,
                                    "display", seat->display,
                                    "device-manager", seat->device_manager,
                                    "seat", seat,
                                    NULL);
      _gdk_device_set_associated_device (seat->pointer, seat->master_pointer);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->pointer);

      if (display_wayland->pointer_gestures)
        {
          seat->wp_pointer_gesture_swipe =
            zwp_pointer_gestures_v1_get_swipe_gesture (display_wayland->pointer_gestures,
                                                       seat->wl_pointer);
          zwp_pointer_gesture_swipe_v1_set_user_data (seat->wp_pointer_gesture_swipe,
                                                      seat);
          zwp_pointer_gesture_swipe_v1_add_listener (seat->wp_pointer_gesture_swipe,
                                                     &gesture_swipe_listener, seat);

          seat->wp_pointer_gesture_pinch =
            zwp_pointer_gestures_v1_get_pinch_gesture (display_wayland->pointer_gestures,
                                                       seat->wl_pointer);
          zwp_pointer_gesture_pinch_v1_set_user_data (seat->wp_pointer_gesture_pinch,
                                                      seat);
          zwp_pointer_gesture_pinch_v1_add_listener (seat->wp_pointer_gesture_pinch,
                                                     &gesture_pinch_listener, seat);
        }

      if (display_wayland->cursor_shape)
        {
          seat->pointer_info.shape_device =
              wp_cursor_shape_manager_v1_get_pointer (display_wayland->cursor_shape, seat->wl_pointer);
        }

      g_signal_emit_by_name (device_manager, "device-added", seat->pointer);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && seat->wl_pointer)
    {
      wl_pointer_release (seat->wl_pointer);
      seat->wl_pointer = NULL;
      _gdk_device_set_associated_device (seat->pointer, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, seat->pointer);

      g_signal_emit_by_name (device_manager, "device-removed", seat->pointer);
      g_clear_object (&seat->pointer);

      if (seat->wheel_scrolling)
        {
          _gdk_device_set_associated_device (seat->wheel_scrolling, NULL);

          device_manager->devices =
            g_list_remove (device_manager->devices, seat->wheel_scrolling);

          g_signal_emit_by_name (device_manager, "device-removed", seat->wheel_scrolling);
          g_clear_object (&seat->wheel_scrolling);
        }

      if (seat->finger_scrolling)
        {
          _gdk_device_set_associated_device (seat->finger_scrolling, NULL);

          device_manager->devices =
            g_list_remove (device_manager->devices, seat->finger_scrolling);

          g_signal_emit_by_name (device_manager, "device-removed", seat->finger_scrolling);
          g_clear_object (&seat->finger_scrolling);
        }

      if (seat->continuous_scrolling)
        {
          _gdk_device_set_associated_device (seat->continuous_scrolling, NULL);

          device_manager->devices =
            g_list_remove (device_manager->devices, seat->continuous_scrolling);

          g_signal_emit_by_name (device_manager, "device-removed", seat->continuous_scrolling);
          g_clear_object (&seat->continuous_scrolling);
        }

        g_clear_pointer (&seat->pointer_info.shape_device, wp_cursor_shape_device_v1_destroy);
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !seat->wl_keyboard)
    {
      seat->wl_keyboard = wl_seat_get_keyboard (wl_seat);
      wl_keyboard_set_user_data (seat->wl_keyboard, seat);
      wl_keyboard_add_listener (seat->wl_keyboard, &keyboard_listener, seat);

      seat->keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                     "name", "Wayland Keyboard",
                                     "type", GDK_DEVICE_TYPE_SLAVE,
                                     "input-source", GDK_SOURCE_KEYBOARD,
                                     "input-mode", GDK_MODE_SCREEN,
                                     "has-cursor", FALSE,
                                     "display", seat->display,
                                     "device-manager", seat->device_manager,
                                     "seat", seat,
                                     NULL);
      _gdk_device_reset_axes (seat->keyboard);
      _gdk_device_set_associated_device (seat->keyboard, seat->master_keyboard);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->keyboard);

      g_signal_emit_by_name (device_manager, "device-added", seat->keyboard);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && seat->wl_keyboard)
    {
      wl_keyboard_release (seat->wl_keyboard);
      seat->wl_keyboard = NULL;
      _gdk_device_set_associated_device (seat->keyboard, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, seat->keyboard);

      g_signal_emit_by_name (device_manager, "device-removed", seat->keyboard);
      g_clear_object (&seat->keyboard);
    }

  if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !seat->wl_touch)
    {
      seat->wl_touch = wl_seat_get_touch (wl_seat);
      wl_touch_set_user_data (seat->wl_touch, seat);
      wl_touch_add_listener (seat->wl_touch, &touch_listener, seat);

      seat->touch_master = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                         "name", "Wayland Touch Master Pointer",
                                         "type", GDK_DEVICE_TYPE_MASTER,
                                         "input-source", GDK_SOURCE_MOUSE,
                                         "input-mode", GDK_MODE_SCREEN,
                                         "has-cursor", TRUE,
                                         "display", seat->display,
                                         "device-manager", seat->device_manager,
                                         "seat", seat,
                                         NULL);
      GDK_WAYLAND_DEVICE (seat->touch_master)->pointer = &seat->touch_info;
      _gdk_device_set_associated_device (seat->touch_master, seat->master_keyboard);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->touch_master);
      g_signal_emit_by_name (device_manager, "device-added", seat->touch_master);

      seat->touch = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                  "name", "Wayland Touch",
                                  "type", GDK_DEVICE_TYPE_SLAVE,
                                  "input-source", GDK_SOURCE_TOUCHSCREEN,
                                  "input-mode", GDK_MODE_SCREEN,
                                  "has-cursor", FALSE,
                                  "display", seat->display,
                                  "device-manager", seat->device_manager,
                                  "seat", seat,
                                  NULL);
      _gdk_device_set_associated_device (seat->touch, seat->touch_master);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->touch);

      g_signal_emit_by_name (device_manager, "device-added", seat->touch);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && seat->wl_touch)
    {
      wl_touch_release (seat->wl_touch);
      seat->wl_touch = NULL;
      _gdk_device_set_associated_device (seat->touch_master, NULL);
      _gdk_device_set_associated_device (seat->touch, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, seat->touch_master);
      device_manager->devices =
        g_list_remove (device_manager->devices, seat->touch);

      g_signal_emit_by_name (device_manager, "device-removed", seat->touch_master);
      g_signal_emit_by_name (device_manager, "device-removed", seat->touch);
      g_clear_object (&seat->touch_master);
      g_clear_object (&seat->touch);
    }

  if (seat->master_pointer)
    gdk_drag_context_set_device (seat->drop_context, seat->master_pointer);
  else if (seat->touch_master)
    gdk_drag_context_set_device (seat->drop_context, seat->touch_master);
}

static GdkDevice *
get_scroll_device (GdkWaylandSeat              *seat,
                   enum wl_pointer_axis_source  source)
{
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);

  if (!seat->pointer)
    return NULL;

  switch (source)
    {
    case WL_POINTER_AXIS_SOURCE_WHEEL:
      if (seat->wheel_scrolling == NULL)
        {
          seat->wheel_scrolling = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                                "name", "Wayland Wheel Scrolling",
                                                "type", GDK_DEVICE_TYPE_SLAVE,
                                                "input-source", GDK_SOURCE_MOUSE,
                                                "input-mode", GDK_MODE_SCREEN,
                                                "has-cursor", TRUE,
                                                "display", seat->display,
                                                "device-manager", seat->device_manager,
                                                "seat", seat,
                                                NULL);
          _gdk_device_set_associated_device (seat->wheel_scrolling, seat->master_pointer);

          device_manager->devices =
            g_list_append (device_manager->devices, seat->wheel_scrolling);

          g_signal_emit_by_name (device_manager, "device-added", seat->wheel_scrolling);
        }
      return seat->wheel_scrolling;

    case WL_POINTER_AXIS_SOURCE_FINGER:
      if (seat->finger_scrolling == NULL)
        {
          seat->finger_scrolling = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                                 "name", "Wayland Finger Scrolling",
                                                 "type", GDK_DEVICE_TYPE_SLAVE,
                                                 "input-source", GDK_SOURCE_TOUCHPAD,
                                                 "input-mode", GDK_MODE_SCREEN,
                                                 "has-cursor", TRUE,
                                                 "display", seat->display,
                                                 "device-manager", seat->device_manager,
                                                 "seat", seat,
                                                 NULL);
          _gdk_device_set_associated_device (seat->finger_scrolling, seat->master_pointer);

          device_manager->devices =
            g_list_append (device_manager->devices, seat->finger_scrolling);

          g_signal_emit_by_name (device_manager, "device-added", seat->finger_scrolling);
        }
      return seat->finger_scrolling;

    case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
      if (seat->continuous_scrolling == NULL)
        {
          seat->continuous_scrolling = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                                     "name", "Wayland Continuous Scrolling",
                                                     "type", GDK_DEVICE_TYPE_SLAVE,
                                                     "input-source", GDK_SOURCE_TRACKPOINT,
                                                     "input-mode", GDK_MODE_SCREEN,
                                                     "has-cursor", TRUE,
                                                     "display", seat->display,
                                                     "device-manager", seat->device_manager,
                                                     "seat", seat,
                                                     NULL);
          _gdk_device_set_associated_device (seat->continuous_scrolling, seat->master_pointer);

          device_manager->devices =
            g_list_append (device_manager->devices, seat->continuous_scrolling);

          g_signal_emit_by_name (device_manager, "device-added", seat->continuous_scrolling);
        }
      return seat->continuous_scrolling;

    default:
      return seat->pointer;
    }
}

static void
seat_handle_name (void           *data,
                  struct wl_seat *seat,
                  const char     *name)
{
  /* We don't care about the name. */
  GDK_NOTE (MISC,
            g_message ("seat %p name %s", seat, name));
}

static const struct wl_seat_listener seat_listener = {
  seat_handle_capabilities,
  seat_handle_name,
};

static void
tablet_tool_handle_type (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool,
                         uint32_t                   tool_type)
{
  GdkWaylandTabletToolData *tool = data;

  switch (tool_type)
    {
    case ZWP_TABLET_TOOL_V2_TYPE_PEN:
      tool->type = GDK_DEVICE_TOOL_TYPE_PEN;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_BRUSH:
      tool->type = GDK_DEVICE_TOOL_TYPE_BRUSH;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH:
      tool->type = GDK_DEVICE_TOOL_TYPE_AIRBRUSH;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_PENCIL:
      tool->type = GDK_DEVICE_TOOL_TYPE_PENCIL;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_ERASER:
      tool->type = GDK_DEVICE_TOOL_TYPE_ERASER;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_MOUSE:
      tool->type = GDK_DEVICE_TOOL_TYPE_MOUSE;
      break;
    case ZWP_TABLET_TOOL_V2_TYPE_LENS:
      tool->type = GDK_DEVICE_TOOL_TYPE_LENS;
      break;
    default:
      tool->type = GDK_DEVICE_TOOL_TYPE_UNKNOWN;
      break;
    };
}

static void
tablet_tool_handle_hardware_serial (void                      *data,
                                    struct zwp_tablet_tool_v2 *wp_tablet_tool,
                                    uint32_t                   serial_hi,
                                    uint32_t                   serial_lo)
{
  GdkWaylandTabletToolData *tool = data;

  tool->hardware_serial = ((guint64) serial_hi) << 32 | serial_lo;
}

static void
tablet_tool_handle_hardware_id_wacom (void                      *data,
                                      struct zwp_tablet_tool_v2 *wp_tablet_tool,
                                      uint32_t                   id_hi,
                                      uint32_t                   id_lo)
{
  GdkWaylandTabletToolData *tool = data;

  tool->hardware_id_wacom = ((guint64) id_hi) << 32 | id_lo;
}

static void
tablet_tool_handle_capability (void                      *data,
                               struct zwp_tablet_tool_v2 *wp_tablet_tool,
                               uint32_t                   capability)
{
  GdkWaylandTabletToolData *tool = data;

  switch (capability)
    {
    case ZWP_TABLET_TOOL_V2_CAPABILITY_TILT:
      tool->axes |= GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE:
      tool->axes |= GDK_AXIS_FLAG_PRESSURE;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE:
      tool->axes |= GDK_AXIS_FLAG_DISTANCE;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION:
      tool->axes |= GDK_AXIS_FLAG_ROTATION;
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER:
      tool->axes |= GDK_AXIS_FLAG_SLIDER;
      break;
    }
}

static void
tablet_tool_handle_done (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;

  tool->tool = gdk_device_tool_new (tool->hardware_serial,
                                    tool->hardware_id_wacom,
                                    tool->type, tool->axes);
  gdk_seat_tool_added (tool->seat, tool->tool);
}

static void
tablet_tool_handle_removed (void                      *data,
                            struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;

  _gdk_wayland_seat_remove_tool (GDK_WAYLAND_SEAT (tool->seat), tool);
}

static void
gdk_wayland_tablet_flush_frame_event (GdkWaylandTabletData *tablet,
                                      guint32               time)
{
  GdkEventType event_type;
  GdkWindow *window;
  GdkEvent *event;

  event = tablet->pointer_info.frame.event;
  tablet->pointer_info.frame.event = NULL;

  if (!event)
    return;

  event_type = event->type;
  window = g_object_ref (gdk_event_get_window (event));

  switch (event_type)
    {
    case GDK_MOTION_NOTIFY:
      event->motion.time = time;
      event->motion.axes =
        g_memdup (tablet->axes,
                  sizeof (gdouble) *
                  gdk_device_get_n_axes (tablet->current_device));
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.time = time;
      event->button.axes =
        g_memdup (tablet->axes,
                  sizeof (gdouble) *
                  gdk_device_get_n_axes (tablet->current_device));
      break;
    case GDK_SCROLL:
      event->scroll.time = time;
      break;
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      event->proximity.time = time;
      break;
    default:
      return;
    }

  if (event_type == GDK_PROXIMITY_OUT)
    emulate_crossing (window, NULL, tablet->master,
                      tablet->current_device, GDK_LEAVE_NOTIFY,
                      GDK_CROSSING_NORMAL, time);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (tablet->seat),
                                      event);

  if (event_type == GDK_PROXIMITY_IN)
    emulate_crossing (window, NULL, tablet->master,
                      tablet->current_device, GDK_ENTER_NOTIFY,
                      GDK_CROSSING_NORMAL, time);

  g_object_unref (window);
}

static GdkEvent *
gdk_wayland_tablet_get_frame_event (GdkWaylandTabletData *tablet,
                                    GdkEventType          evtype)
{
  if (tablet->pointer_info.frame.event &&
      tablet->pointer_info.frame.event->type != evtype)
    gdk_wayland_tablet_flush_frame_event (tablet, GDK_CURRENT_TIME);

  tablet->pointer_info.frame.event = gdk_event_new (evtype);
  return tablet->pointer_info.frame.event;
}

static void
gdk_wayland_device_tablet_clone_tool_axes (GdkWaylandTabletData *tablet,
                                           GdkDeviceTool        *tool)
{
  gint axis_pos;

  g_object_freeze_notify (G_OBJECT (tablet->current_device));
  _gdk_device_reset_axes (tablet->current_device);

  _gdk_device_add_axis (tablet->current_device, GDK_NONE, GDK_AXIS_X, 0, 0, 0);
  _gdk_device_add_axis (tablet->current_device, GDK_NONE, GDK_AXIS_Y, 0, 0, 0);

  if (tool->tool_axes & (GDK_AXIS_FLAG_XTILT | GDK_AXIS_FLAG_YTILT))
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_XTILT, -90, 90, 0);
      tablet->axis_indices[GDK_AXIS_XTILT] = axis_pos;

      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_YTILT, -90, 90, 0);
      tablet->axis_indices[GDK_AXIS_YTILT] = axis_pos;
    }
  if (tool->tool_axes & GDK_AXIS_FLAG_DISTANCE)
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_DISTANCE, 0, 65535, 0);
      tablet->axis_indices[GDK_AXIS_DISTANCE] = axis_pos;
    }
  if (tool->tool_axes & GDK_AXIS_FLAG_PRESSURE)
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_PRESSURE, 0, 65535, 0);
      tablet->axis_indices[GDK_AXIS_PRESSURE] = axis_pos;
    }

  if (tool->tool_axes & GDK_AXIS_FLAG_ROTATION)
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_ROTATION, 0, 360, 0);
      tablet->axis_indices[GDK_AXIS_ROTATION] = axis_pos;
    }

  if (tool->tool_axes & GDK_AXIS_FLAG_SLIDER)
    {
      axis_pos = _gdk_device_add_axis (tablet->current_device, GDK_NONE,
                                       GDK_AXIS_SLIDER, -65535, 65535, 0);
      tablet->axis_indices[GDK_AXIS_SLIDER] = axis_pos;
    }

  if (tablet->axes)
    g_free (tablet->axes);

  tablet->axes =
    g_new0 (gdouble, gdk_device_get_n_axes (tablet->current_device));

  g_object_thaw_notify (G_OBJECT (tablet->current_device));
}

static void
gdk_wayland_mimic_device_axes (GdkDevice *master,
                               GdkDevice *slave)
{
  gdouble axis_min, axis_max, axis_resolution;
  GdkAtom axis_label;
  GdkAxisUse axis_use;
  gint axis_count;
  gint i;

  g_object_freeze_notify (G_OBJECT (master));
  _gdk_device_reset_axes (master);
  axis_count = gdk_device_get_n_axes (slave);

  for (i = 0; i < axis_count; i++)
    {
      _gdk_device_get_axis_info (slave, i, &axis_label, &axis_use, &axis_min,
                                 &axis_max, &axis_resolution);
      _gdk_device_add_axis (master, axis_label, axis_use, axis_min,
                            axis_max, axis_resolution);
    }

  g_object_thaw_notify (G_OBJECT (master));
}

static void
tablet_tool_handle_proximity_in (void                      *data,
                                 struct zwp_tablet_tool_v2 *wp_tablet_tool,
                                 uint32_t                   serial,
                                 struct zwp_tablet_v2      *wp_tablet,
                                 struct wl_surface         *wl_surface)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = zwp_tablet_v2_get_user_data (wp_tablet);
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tablet->seat);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);
  GdkWindow *window;
  GdkEvent *event;

  if (!wl_surface)
    return;

  window = wl_surface_get_user_data (wl_surface);
  if (!GDK_IS_WINDOW (window))
    return;

  tool->current_tablet = tablet;
  tablet->current_tool = tool;

  _gdk_wayland_display_update_serial (display_wayland, serial);
  tablet->pointer_info.enter_serial = serial;

  tablet->pointer_info.focus = g_object_ref (window);
  tablet->current_device =
    tablet_select_device_for_tool (tablet, tool->tool);

  gdk_device_update_tool (tablet->current_device, tool->tool);
  gdk_wayland_device_tablet_clone_tool_axes (tablet, tool->tool);
  gdk_wayland_mimic_device_axes (tablet->master, tablet->current_device);

  event = gdk_wayland_tablet_get_frame_event (tablet, GDK_PROXIMITY_IN);
  event->proximity.window = g_object_ref (tablet->pointer_info.focus);
  gdk_event_set_device (event, tablet->master);
  gdk_event_set_source_device (event, tablet->current_device);
  gdk_event_set_device_tool (event, tool->tool);

  GDK_NOTE (EVENTS,
            g_message ("proximity in, seat %p surface %p tool %d",
                       seat, tablet->pointer_info.focus,
                       gdk_device_tool_get_tool_type (tool->tool)));
}

static void
tablet_tool_handle_proximity_out (void                      *data,
                                  struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEvent *event;
#ifdef G_ENABLE_DEBUG
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tool->seat);
#endif

  if (!tablet)
    return;

  GDK_NOTE (EVENTS,
            g_message ("proximity out, seat %p, tool %d", seat,
                       gdk_device_tool_get_tool_type (tool->tool)));

  event = gdk_wayland_tablet_get_frame_event (tablet, GDK_PROXIMITY_OUT);
  event->proximity.window = g_object_ref (tablet->pointer_info.focus);
  gdk_event_set_device (event, tablet->master);
  gdk_event_set_source_device (event, tablet->current_device);
  gdk_event_set_device_tool (event, tool->tool);

  gdk_wayland_pointer_stop_cursor_animation (&tablet->pointer_info);

  gdk_wayland_device_update_window_cursor (tablet->master);
  g_object_unref (tablet->pointer_info.focus);
  tablet->pointer_info.focus = NULL;

  gdk_device_update_tool (tablet->current_device, NULL);
  g_clear_object (&tablet->pointer_info.cursor);
}

static void
tablet_create_button_event_frame (GdkWaylandTabletData *tablet,
                                  GdkEventType          evtype,
                                  guint                 button)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tablet->seat);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  event = gdk_wayland_tablet_get_frame_event (tablet, evtype);
  event->button.window = g_object_ref (tablet->pointer_info.focus);
  gdk_event_set_device (event, tablet->master);
  gdk_event_set_source_device (event, tablet->current_device);
  gdk_event_set_device_tool (event, tablet->current_tool->tool);
  event->button.time = tablet->pointer_info.time;
  event->button.state = device_get_modifiers (tablet->master);
  event->button.button = button;
  gdk_event_set_screen (event, display_wayland->screen);

  get_coordinates (tablet->master,
                   &event->button.x,
                   &event->button.y,
                   &event->button.x_root,
                   &event->button.y_root);
}

static void
tablet_tool_handle_down (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool,
                         uint32_t                   serial)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tool->seat);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);

  if (!tablet || !tablet->pointer_info.focus)
    return;

  _gdk_wayland_display_update_serial (display_wayland, serial);
  tablet->pointer_info.press_serial = serial;

  tablet_create_button_event_frame (tablet, GDK_BUTTON_PRESS, GDK_BUTTON_PRIMARY);
  tablet->pointer_info.button_modifiers |= GDK_BUTTON1_MASK;
}

static void
tablet_tool_handle_up (void                      *data,
                       struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;

  if (!tablet || !tablet->pointer_info.focus)
    return;

  tablet_create_button_event_frame (tablet, GDK_BUTTON_RELEASE, GDK_BUTTON_PRIMARY);
  tablet->pointer_info.button_modifiers &= ~GDK_BUTTON1_MASK;
}

static void
tablet_tool_handle_motion (void                      *data,
                           struct zwp_tablet_tool_v2 *wp_tablet_tool,
                           wl_fixed_t                 sx,
                           wl_fixed_t                 sy)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (tool->seat);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!tablet)
    return;

  tablet->pointer_info.surface_x = wl_fixed_to_double (sx);
  tablet->pointer_info.surface_y = wl_fixed_to_double (sy);

  GDK_NOTE (EVENTS,
            g_message ("tablet motion %f %f",
                       tablet->pointer_info.surface_x,
                       tablet->pointer_info.surface_y));

  event = gdk_wayland_tablet_get_frame_event (tablet, GDK_MOTION_NOTIFY);
  event->motion.window = g_object_ref (tablet->pointer_info.focus);
  gdk_event_set_device (event, tablet->master);
  gdk_event_set_source_device (event, tablet->current_device);
  gdk_event_set_device_tool (event, tool->tool);
  event->motion.time = tablet->pointer_info.time;
  event->motion.state = device_get_modifiers (tablet->master);
  event->motion.is_hint = FALSE;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (tablet->master,
                   &event->motion.x,
                   &event->motion.y,
                   &event->motion.x_root,
                   &event->motion.y_root);
}

static void
tablet_tool_handle_pressure (void                      *data,
                             struct zwp_tablet_tool_v2 *wp_tablet_tool,
                             uint32_t                   pressure)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  gint axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_PRESSURE];

  _gdk_device_translate_axis (tablet->current_device, axis_index,
                              pressure, &tablet->axes[axis_index]);

  GDK_NOTE (EVENTS,
            g_message ("tablet tool %d pressure %d",
                       gdk_device_tool_get_tool_type (tool->tool), pressure));
}

static void
tablet_tool_handle_distance (void                      *data,
                             struct zwp_tablet_tool_v2 *wp_tablet_tool,
                             uint32_t                   distance)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  gint axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_DISTANCE];

  _gdk_device_translate_axis (tablet->current_device, axis_index,
                              distance, &tablet->axes[axis_index]);

  GDK_NOTE (EVENTS,
            g_message ("tablet tool %d distance %d",
                       gdk_device_tool_get_tool_type (tool->tool), distance));
}

static void
tablet_tool_handle_tilt (void                      *data,
                         struct zwp_tablet_tool_v2 *wp_tablet_tool,
                         wl_fixed_t                 xtilt,
                         wl_fixed_t                 ytilt)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  gint xtilt_axis_index;
  gint ytilt_axis_index;

  if (!tablet)
    return;

  xtilt_axis_index = tablet->axis_indices[GDK_AXIS_XTILT];
  ytilt_axis_index = tablet->axis_indices[GDK_AXIS_YTILT];

  _gdk_device_translate_axis (tablet->current_device, xtilt_axis_index,
                              wl_fixed_to_double (xtilt),
                              &tablet->axes[xtilt_axis_index]);
  _gdk_device_translate_axis (tablet->current_device, ytilt_axis_index,
                              wl_fixed_to_double (ytilt),
                              &tablet->axes[ytilt_axis_index]);

  GDK_NOTE (EVENTS,
            g_message ("tablet tool %d tilt %f/%f",
                       gdk_device_tool_get_tool_type (tool->tool),
                       wl_fixed_to_double (xtilt), wl_fixed_to_double (ytilt)));
}

static void
tablet_tool_handle_button (void                      *data,
                           struct zwp_tablet_tool_v2 *wp_tablet_tool,
                           uint32_t                   serial,
                           uint32_t                   button,
                           uint32_t                   state)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEventType evtype;
  guint n_button;

  if (!tablet || !tablet->pointer_info.focus)
    return;

  tablet->pointer_info.press_serial = serial;

  if (button == BTN_STYLUS)
    n_button = GDK_BUTTON_MIDDLE;
  else if (button == BTN_STYLUS2)
    n_button = GDK_BUTTON_SECONDARY;
  else if (button == BTN_STYLUS3)
    n_button = 8; /* Back */
  else
    return;

  if (state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED)
    evtype = GDK_BUTTON_PRESS;
  else if (state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_RELEASED)
    evtype = GDK_BUTTON_RELEASE;
  else
    return;

  tablet_create_button_event_frame (tablet, evtype, n_button);
}

static void
tablet_tool_handle_rotation (void                      *data,
                             struct zwp_tablet_tool_v2 *wp_tablet_tool,
                             wl_fixed_t                 degrees)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  gint axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_ROTATION];

  _gdk_device_translate_axis (tablet->current_device, axis_index,
                              wl_fixed_to_double (degrees),
                              &tablet->axes[axis_index]);

  GDK_NOTE (EVENTS,
            g_message ("tablet tool %d rotation %f",
                       gdk_device_tool_get_tool_type (tool->tool),
                       wl_fixed_to_double (degrees)));
}

static void
tablet_tool_handle_slider (void                      *data,
                           struct zwp_tablet_tool_v2 *wp_tablet_tool,
                           int32_t                    position)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  gint axis_index;

  if (!tablet)
    return;

  axis_index = tablet->axis_indices[GDK_AXIS_SLIDER];

  _gdk_device_translate_axis (tablet->current_device, axis_index,
                              position, &tablet->axes[axis_index]);

  GDK_NOTE (EVENTS,
            g_message ("tablet tool %d slider %d",
                       gdk_device_tool_get_tool_type (tool->tool), position));
}

static void
tablet_tool_handle_wheel (void                      *data,
                          struct zwp_tablet_tool_v2 *wp_tablet_tool,
                          int32_t                    degrees,
                          int32_t                    clicks)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkWaylandSeat *seat;
  GdkEvent *event;

  if (!tablet)
    return;

  GDK_NOTE (EVENTS,
            g_message ("tablet tool %d wheel %d/%d",
                       gdk_device_tool_get_tool_type (tool->tool), degrees, clicks));

  if (clicks == 0)
    return;

  seat = GDK_WAYLAND_SEAT (tablet->seat);

  /* Send smooth event */
  event = create_scroll_event (seat, &tablet->pointer_info,
                               tablet->master, tablet->current_device, FALSE);
  gdk_event_set_device_tool (event, tablet->current_tool->tool);
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_y = clicks;
  _gdk_wayland_display_deliver_event (seat->display, event);

  /* Send discrete event */
  event = create_scroll_event (seat, &tablet->pointer_info,
                               tablet->master, tablet->current_device, TRUE);
  gdk_event_set_device_tool (event, tablet->current_tool->tool);
  event->scroll.direction = (clicks > 0) ? GDK_SCROLL_DOWN : GDK_SCROLL_UP;
  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
tablet_tool_handle_frame (void                      *data,
                          struct zwp_tablet_tool_v2 *wl_tablet_tool,
                          uint32_t                   time)
{
  GdkWaylandTabletToolData *tool = data;
  GdkWaylandTabletData *tablet = tool->current_tablet;
  GdkEvent *frame_event;

  if (!tablet)
    return;

  GDK_NOTE (EVENTS,
            g_message ("tablet frame, time %d", time));

  frame_event = tablet->pointer_info.frame.event;

  if (frame_event && frame_event->type == GDK_PROXIMITY_OUT)
    {
      tool->current_tablet = NULL;
      tablet->current_tool = NULL;
    }

  tablet->pointer_info.time = time;
  gdk_wayland_tablet_flush_frame_event (tablet, time);
}

static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
  tablet_tool_handle_type,
  tablet_tool_handle_hardware_serial,
  tablet_tool_handle_hardware_id_wacom,
  tablet_tool_handle_capability,
  tablet_tool_handle_done,
  tablet_tool_handle_removed,
  tablet_tool_handle_proximity_in,
  tablet_tool_handle_proximity_out,
  tablet_tool_handle_down,
  tablet_tool_handle_up,
  tablet_tool_handle_motion,
  tablet_tool_handle_pressure,
  tablet_tool_handle_distance,
  tablet_tool_handle_tilt,
  tablet_tool_handle_rotation,
  tablet_tool_handle_slider,
  tablet_tool_handle_wheel,
  tablet_tool_handle_button,
  tablet_tool_handle_frame,
};

static void
tablet_pad_ring_handle_source (void                          *data,
                               struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring,
                               uint32_t                       source)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad ring handle source, ring = %p source = %d",
                       wp_tablet_pad_ring, source));

  group->axis_tmp_info.source = source;
}

static void
tablet_pad_ring_handle_angle (void                          *data,
                              struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring,
                              wl_fixed_t                     angle)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad ring handle angle, ring = %p angle = %f",
                       wp_tablet_pad_ring, wl_fixed_to_double (angle)));

  group->axis_tmp_info.value = wl_fixed_to_double (angle);
}

static void
tablet_pad_ring_handle_stop (void                          *data,
                             struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad ring handle stop, ring = %p", wp_tablet_pad_ring));

  group->axis_tmp_info.is_stop = TRUE;
}

static void
tablet_pad_ring_handle_frame (void                          *data,
                              struct zwp_tablet_pad_ring_v2 *wp_tablet_pad_ring,
                              uint32_t                       time)
{
  GdkWaylandTabletPadGroupData *group = data;
  GdkWaylandTabletPadData *pad = group->pad;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad ring handle frame, ring = %p", wp_tablet_pad_ring));

  event = gdk_event_new (GDK_PAD_RING);
  g_set_object (&event->pad_axis.window, seat->keyboard_focus);
  event->pad_axis.time = time;
  event->pad_axis.group = g_list_index (pad->mode_groups, group);
  event->pad_axis.index = g_list_index (pad->rings, wp_tablet_pad_ring);
  event->pad_axis.mode = group->current_mode;
  event->pad_axis.value = group->axis_tmp_info.value;
  gdk_event_set_device (event, pad->device);
  gdk_event_set_source_device (event, pad->device);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static const struct zwp_tablet_pad_ring_v2_listener tablet_pad_ring_listener = {
  tablet_pad_ring_handle_source,
  tablet_pad_ring_handle_angle,
  tablet_pad_ring_handle_stop,
  tablet_pad_ring_handle_frame,
};

static void
tablet_pad_strip_handle_source (void                           *data,
                                struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip,
                                uint32_t                        source)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad strip handle source, strip = %p source = %d",
                       wp_tablet_pad_strip, source));

  group->axis_tmp_info.source = source;
}

static void
tablet_pad_strip_handle_position (void                           *data,
                                  struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip,
                                  uint32_t                        position)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad strip handle position, strip = %p position = %d",
                       wp_tablet_pad_strip, position));

  group->axis_tmp_info.value = (gdouble) position / 65535;
}

static void
tablet_pad_strip_handle_stop (void                           *data,
                              struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad strip handle stop, strip = %p",
                       wp_tablet_pad_strip));

  group->axis_tmp_info.is_stop = TRUE;
}

static void
tablet_pad_strip_handle_frame (void                           *data,
                               struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip,
                               uint32_t                        time)
{
  GdkWaylandTabletPadGroupData *group = data;
  GdkWaylandTabletPadData *pad = group->pad;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad strip handle frame, strip = %p",
                       wp_tablet_pad_strip));

  event = gdk_event_new (GDK_PAD_STRIP);
  g_set_object (&event->pad_axis.window, seat->keyboard_focus);
  event->pad_axis.time = time;
  event->pad_axis.group = g_list_index (pad->mode_groups, group);
  event->pad_axis.index = g_list_index (pad->strips, wp_tablet_pad_strip);
  event->pad_axis.mode = group->current_mode;
  event->pad_axis.value = group->axis_tmp_info.value;

  gdk_event_set_device (event, pad->device);
  gdk_event_set_source_device (event, pad->device);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static const struct zwp_tablet_pad_strip_v2_listener tablet_pad_strip_listener = {
  tablet_pad_strip_handle_source,
  tablet_pad_strip_handle_position,
  tablet_pad_strip_handle_stop,
  tablet_pad_strip_handle_frame,
};

static void
tablet_pad_group_handle_buttons (void                           *data,
                                 struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                                 struct wl_array                *buttons)
{
  GdkWaylandTabletPadGroupData *group = data;
  uint32_t *p;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad group handle buttons, pad group = %p, n_buttons = %ld",
                       wp_tablet_pad_group, buttons->size));

  wl_array_for_each (p, buttons)
    {
      group->buttons = g_list_prepend (group->buttons, GUINT_TO_POINTER (*p));
    }

  group->buttons = g_list_reverse (group->buttons);
}

static void
tablet_pad_group_handle_ring (void                           *data,
                              struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                              struct zwp_tablet_pad_ring_v2  *wp_tablet_pad_ring)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad group handle ring, pad group = %p, ring = %p",
                       wp_tablet_pad_group, wp_tablet_pad_ring));

  zwp_tablet_pad_ring_v2_add_listener (wp_tablet_pad_ring,
                                       &tablet_pad_ring_listener, group);
  zwp_tablet_pad_ring_v2_set_user_data (wp_tablet_pad_ring, group);

  group->rings = g_list_append (group->rings, wp_tablet_pad_ring);
  group->pad->rings = g_list_append (group->pad->rings, wp_tablet_pad_ring);
}

static void
tablet_pad_group_handle_strip (void                           *data,
                               struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                               struct zwp_tablet_pad_strip_v2 *wp_tablet_pad_strip)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad group handle strip, pad group = %p, strip = %p",
                       wp_tablet_pad_group, wp_tablet_pad_strip));

  zwp_tablet_pad_strip_v2_add_listener (wp_tablet_pad_strip,
                                       &tablet_pad_strip_listener, group);
  zwp_tablet_pad_strip_v2_set_user_data (wp_tablet_pad_strip, group);

  group->strips = g_list_append (group->strips, wp_tablet_pad_strip);
  group->pad->strips = g_list_append (group->pad->strips, wp_tablet_pad_strip);
}

static void
tablet_pad_group_handle_modes (void                           *data,
                               struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                               uint32_t                        modes)
{
  GdkWaylandTabletPadGroupData *group = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad group handle modes, pad group = %p, n_modes = %d",
                       wp_tablet_pad_group, modes));

  group->n_modes = modes;
}

static void
tablet_pad_group_handle_done (void                           *data,
                              struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group)
{
  GDK_NOTE (EVENTS,
            g_message ("tablet pad group handle done, pad group = %p",
                       wp_tablet_pad_group));
}

static void
tablet_pad_group_handle_mode (void                           *data,
                              struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group,
                              uint32_t                        time,
                              uint32_t                        serial,
                              uint32_t                        mode)
{
  GdkWaylandTabletPadGroupData *group = data;
  GdkWaylandTabletPadData *pad = group->pad;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;
  guint n_group;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad group handle mode, pad group = %p, mode = %d",
                       wp_tablet_pad_group, mode));

  group->mode_switch_serial = serial;
  group->current_mode = mode;
  n_group = g_list_index (pad->mode_groups, group);

  event = gdk_event_new (GDK_PAD_GROUP_MODE);
  g_set_object (&event->pad_button.window, seat->keyboard_focus);
  event->pad_group_mode.group = n_group;
  event->pad_group_mode.mode = mode;
  event->pad_group_mode.time = time;
  gdk_event_set_device (event, pad->device);
  gdk_event_set_source_device (event, pad->device);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static const struct zwp_tablet_pad_group_v2_listener tablet_pad_group_listener = {
  tablet_pad_group_handle_buttons,
  tablet_pad_group_handle_ring,
  tablet_pad_group_handle_strip,
  tablet_pad_group_handle_modes,
  tablet_pad_group_handle_done,
  tablet_pad_group_handle_mode,
};

static void
tablet_pad_handle_group (void                           *data,
                         struct zwp_tablet_pad_v2       *wp_tablet_pad,
                         struct zwp_tablet_pad_group_v2 *wp_tablet_pad_group)
{
  GdkWaylandTabletPadData *pad = data;
  GdkWaylandTabletPadGroupData *group;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle group, pad group = %p, group = %p",
                       wp_tablet_pad_group, wp_tablet_pad_group));

  group = g_new0 (GdkWaylandTabletPadGroupData, 1);
  group->wp_tablet_pad_group = wp_tablet_pad_group;
  group->pad = pad;

  zwp_tablet_pad_group_v2_add_listener (wp_tablet_pad_group,
                                        &tablet_pad_group_listener, group);
  zwp_tablet_pad_group_v2_set_user_data (wp_tablet_pad_group, group);
  pad->mode_groups = g_list_append (pad->mode_groups, group);
}

static void
tablet_pad_handle_path (void                     *data,
                        struct zwp_tablet_pad_v2 *wp_tablet_pad,
                        const char               *path)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle path, pad = %p, path = %s",
                       wp_tablet_pad, path));

  pad->path = g_strdup (path);
}

static void
tablet_pad_handle_buttons (void                     *data,
                           struct zwp_tablet_pad_v2 *wp_tablet_pad,
                           uint32_t                  buttons)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle buttons, pad = %p, n_buttons = %d",
                       wp_tablet_pad, buttons));

  pad->n_buttons = buttons;
}

static void
tablet_pad_handle_done (void                     *data,
                        struct zwp_tablet_pad_v2 *wp_tablet_pad)
{
  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle done, pad = %p", wp_tablet_pad));
}

static void
tablet_pad_handle_button (void                     *data,
                          struct zwp_tablet_pad_v2 *wp_tablet_pad,
                          uint32_t                  time,
                          uint32_t                  button,
                          uint32_t                  state)
{
  GdkWaylandTabletPadData *pad = data;
  GdkWaylandTabletPadGroupData *group;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkEvent *event;
  gint n_group;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle button, pad = %p, button = %d, state = %d",
                       wp_tablet_pad, button, state));

  group = tablet_pad_lookup_button_group (pad, button);
  n_group = g_list_index (pad->mode_groups, group);

  event = gdk_event_new (state == ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED ?
                         GDK_PAD_BUTTON_PRESS :
                         GDK_PAD_BUTTON_RELEASE);
  g_set_object (&event->pad_button.window, seat->keyboard_focus);
  event->pad_button.button = button;
  event->pad_button.group = n_group;
  event->pad_button.mode = group->current_mode;
  event->pad_button.time = time;
  gdk_event_set_device (event, pad->device);
  gdk_event_set_source_device (event, pad->device);

  _gdk_wayland_display_deliver_event (gdk_seat_get_display (pad->seat),
                                      event);
}

static void
tablet_pad_handle_enter (void                     *data,
                         struct zwp_tablet_pad_v2 *wp_tablet_pad,
                         uint32_t                  serial,
                         struct zwp_tablet_v2     *wp_tablet,
                         struct wl_surface        *surface)
{
  GdkWaylandTabletPadData *pad = data;
  GdkWaylandTabletData *tablet = zwp_tablet_v2_get_user_data (wp_tablet);
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (pad->seat);
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle enter, pad = %p, tablet = %p surface = %p",
                       wp_tablet_pad, wp_tablet, surface));

  if (pad->device && pad->current_tablet != tablet)
    {
      device_manager->devices =
        g_list_remove (device_manager->devices, pad->device);
      g_signal_emit_by_name (device_manager, "device-removed", pad->device);
      _gdk_device_set_associated_device (pad->device, NULL);
      g_clear_object (&pad->device);
    }

  /* Relate pad and tablet */
  tablet->pads = g_list_append (tablet->pads, pad);
  pad->current_tablet = tablet;

  if (!pad->device)
    {
      GdkWaylandTabletPadData *pad = data;
      gchar *name, *vid, *pid;

      name = g_strdup_printf ("%s Pad %d",
                              tablet->name,
                              g_list_index (tablet->pads, pad) + 1);
      vid = g_strdup_printf ("%.4x", tablet->vid);
      pid = g_strdup_printf ("%.4x", tablet->pid);

      pad->device =
        g_object_new (GDK_TYPE_WAYLAND_DEVICE_PAD,
                      "name", name,
                      "vendor-id", vid,
                      "product-id", pid,
                      "type", GDK_DEVICE_TYPE_SLAVE,
                      "input-source", GDK_SOURCE_TABLET_PAD,
                      "input-mode", GDK_MODE_SCREEN,
                      "display", gdk_seat_get_display (pad->seat),
                      "device-manager", device_manager,
                      "seat", seat,
                      NULL);

      _gdk_device_set_associated_device (pad->device, seat->master_keyboard);
      device_manager->devices =
        g_list_prepend (device_manager->devices, pad->device);
      g_signal_emit_by_name (device_manager, "device-added", pad->device);

      g_free (name);
      g_free (vid);
      g_free (pid);
    }
}

static void
tablet_pad_handle_leave (void                     *data,
                         struct zwp_tablet_pad_v2 *wp_tablet_pad,
                         uint32_t                  serial,
                         struct wl_surface        *surface)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle leave, pad = %p, surface = %p",
                       wp_tablet_pad, surface));

  if (pad->current_tablet)
    {
      pad->current_tablet->pads = g_list_remove (pad->current_tablet->pads, pad);
      pad->current_tablet = NULL;
    }
}

static void
tablet_pad_handle_removed (void                     *data,
                           struct zwp_tablet_pad_v2 *wp_tablet_pad)
{
  GdkWaylandTabletPadData *pad = data;

  GDK_NOTE (EVENTS,
            g_message ("tablet pad handle removed, pad = %p", wp_tablet_pad));

  /* Remove from the current tablet */
  if (pad->current_tablet)
    {
      pad->current_tablet->pads = g_list_remove (pad->current_tablet->pads, pad);
      pad->current_tablet = NULL;
    }

  _gdk_wayland_seat_remove_tablet_pad (GDK_WAYLAND_SEAT (pad->seat), pad);
}

static const struct zwp_tablet_pad_v2_listener tablet_pad_listener = {
  tablet_pad_handle_group,
  tablet_pad_handle_path,
  tablet_pad_handle_buttons,
  tablet_pad_handle_done,
  tablet_pad_handle_button,
  tablet_pad_handle_enter,
  tablet_pad_handle_leave,
  tablet_pad_handle_removed,
};

static void
tablet_seat_handle_tablet_added (void                      *data,
                                 struct zwp_tablet_seat_v2 *wp_tablet_seat,
                                 struct zwp_tablet_v2      *wp_tablet)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTabletData *tablet;

  tablet = g_new0 (GdkWaylandTabletData, 1);
  tablet->seat = GDK_SEAT (seat);

  tablet->wp_tablet = wp_tablet;

  seat->tablets = g_list_prepend (seat->tablets, tablet);

  zwp_tablet_v2_add_listener (wp_tablet, &tablet_listener, tablet);
  zwp_tablet_v2_set_user_data (wp_tablet, tablet);
}

static void
tablet_seat_handle_tool_added (void                      *data,
                               struct zwp_tablet_seat_v2 *wp_tablet_seat,
                               struct zwp_tablet_tool_v2 *wp_tablet_tool)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTabletToolData *tool;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  tool = g_new0 (GdkWaylandTabletToolData, 1);
  tool->wp_tablet_tool = wp_tablet_tool;
  tool->seat = GDK_SEAT (seat);

  zwp_tablet_tool_v2_add_listener (wp_tablet_tool, &tablet_tool_listener, tool);
  zwp_tablet_tool_v2_set_user_data (wp_tablet_tool, tool);

  seat->tablet_tools = g_list_prepend (seat->tablet_tools, tool);

  if (display->cursor_shape)
    {
      tool->shape_device =
          wp_cursor_shape_manager_v1_get_tablet_tool_v2 (
              display->cursor_shape, tool->wp_tablet_tool);
    }
}

static void
tablet_seat_handle_pad_added (void                      *data,
                              struct zwp_tablet_seat_v2 *wp_tablet_seat,
                              struct zwp_tablet_pad_v2  *wp_tablet_pad)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTabletPadData *pad;

  pad = g_new0 (GdkWaylandTabletPadData, 1);
  pad->wp_tablet_pad = wp_tablet_pad;
  pad->seat = GDK_SEAT (seat);

  zwp_tablet_pad_v2_add_listener (wp_tablet_pad, &tablet_pad_listener, pad);
  zwp_tablet_pad_v2_set_user_data (wp_tablet_pad, pad);

  seat->tablet_pads = g_list_prepend (seat->tablet_pads, pad);
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
  tablet_seat_handle_tablet_added,
  tablet_seat_handle_tool_added,
  tablet_seat_handle_pad_added,
};

static void
on_monitors_changed (GdkScreen      *screen,
                     GdkWaylandSeat *seat)
{
  pointer_surface_update_scale (seat->master_pointer);
}

static void
init_devices (GdkWaylandSeat *seat)
{
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  /* pointer */
  seat->master_pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                       "name", "Core Pointer",
                                       "type", GDK_DEVICE_TYPE_MASTER,
                                       "input-source", GDK_SOURCE_MOUSE,
                                       "input-mode", GDK_MODE_SCREEN,
                                       "has-cursor", TRUE,
                                       "display", seat->display,
                                       "device-manager", device_manager,
                                       "seat", seat,
                                       NULL);

  GDK_WAYLAND_DEVICE (seat->master_pointer)->pointer = &seat->pointer_info;

  device_manager->devices =
    g_list_prepend (device_manager->devices, seat->master_pointer);
  g_signal_emit_by_name (device_manager, "device-added", seat->master_pointer);

  g_signal_connect (display->screen, "monitors-changed",
                    G_CALLBACK (on_monitors_changed), seat);

  /* keyboard */
  seat->master_keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                        "name", "Core Keyboard",
                                        "type", GDK_DEVICE_TYPE_MASTER,
                                        "input-source", GDK_SOURCE_KEYBOARD,
                                        "input-mode", GDK_MODE_SCREEN,
                                        "has-cursor", FALSE,
                                        "display", seat->display,
                                        "device-manager", device_manager,
                                        "seat", seat,
                                        NULL);
  _gdk_device_reset_axes (seat->master_keyboard);

  device_manager->devices =
    g_list_prepend (device_manager->devices, seat->master_keyboard);
  g_signal_emit_by_name (device_manager, "device-added", seat->master_keyboard);

  /* link both */
  _gdk_device_set_associated_device (seat->master_pointer, seat->master_keyboard);
  _gdk_device_set_associated_device (seat->master_keyboard, seat->master_pointer);
}

static void
pointer_surface_update_scale (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandPointerData *pointer = GDK_WAYLAND_DEVICE (device)->pointer;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (seat->display);
  guint32 scale;
  GSList *l;

  if (display_wayland->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      return;
    }

  if (!pointer->pointer_surface_outputs)
    return;

  scale = 1;
  for (l = pointer->pointer_surface_outputs; l != NULL; l = l->next)
    {
      guint32 output_scale =
        _gdk_wayland_screen_get_output_scale (display_wayland->screen,
                                              l->data);
      scale = MAX (scale, output_scale);
    }

  if (pointer->current_output_scale == scale)
    return;
  pointer->current_output_scale = scale;

  if (pointer->cursor)
    _gdk_wayland_cursor_set_scale (pointer->cursor, scale);

  gdk_wayland_device_update_window_cursor (device);
}

static void
pointer_surface_enter (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)

{
  GdkDevice *device = data;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandTabletData *tablet;

  GDK_NOTE (EVENTS,
            g_message ("pointer surface of seat %p entered output %p",
                       seat, output));

  tablet = gdk_wayland_device_manager_find_tablet (seat, device);

  if (tablet)
    {
      tablet->pointer_info.pointer_surface_outputs =
        g_slist_append (tablet->pointer_info.pointer_surface_outputs, output);
    }
  else
    {
      seat->pointer_info.pointer_surface_outputs =
        g_slist_append (seat->pointer_info.pointer_surface_outputs, output);
    }

  pointer_surface_update_scale (device);
}

static void
pointer_surface_leave (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)
{
  GdkDevice *device = data;
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWaylandTabletData *tablet;

  GDK_NOTE (EVENTS,
            g_message ("pointer surface of seat %p left output %p",
                       seat, output));

  tablet = gdk_wayland_device_manager_find_tablet (seat, device);

  if (tablet)
    {
      tablet->pointer_info.pointer_surface_outputs =
        g_slist_remove (tablet->pointer_info.pointer_surface_outputs, output);
    }
  else
    {
      seat->pointer_info.pointer_surface_outputs =
        g_slist_remove (seat->pointer_info.pointer_surface_outputs, output);
    }

  pointer_surface_update_scale (device);
}

static const struct wl_surface_listener pointer_surface_listener = {
  pointer_surface_enter,
  pointer_surface_leave
};

static GdkWindow *
create_foreign_dnd_window (GdkDisplay *display)
{
  GdkWindowAttr attrs;
  GdkScreen *screen;
  guint mask;

  screen = gdk_display_get_default_screen (display);

  attrs.x = attrs.y = 0;
  attrs.width = attrs.height = 1;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.window_type = GDK_WINDOW_TEMP;
  attrs.visual = gdk_screen_get_system_visual (screen);

  mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  return gdk_window_new (gdk_screen_get_root_window (screen), &attrs, mask);
}

static void
gdk_wayland_pointer_data_finalize (GdkWaylandPointerData *pointer)
{
  g_clear_object (&pointer->focus);
  g_clear_object (&pointer->cursor);
  wl_surface_destroy (pointer->pointer_surface);
  g_slist_free (pointer->pointer_surface_outputs);
}

static void
gdk_wayland_seat_finalize (GObject *object)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (object);
  GList *l;

  for (l = seat->tablet_tools; l != NULL; l = l->next)
    _gdk_wayland_seat_remove_tool (seat, l->data);

  for (l = seat->tablet_pads; l != NULL; l = l->next)
    _gdk_wayland_seat_remove_tablet_pad (seat, l->data);

  for (l = seat->tablets; l != NULL; l = l->next)
    _gdk_wayland_seat_remove_tablet (seat, l->data);

  seat_handle_capabilities (seat, seat->wl_seat, 0);
  g_object_unref (seat->keymap);
  gdk_wayland_pointer_data_finalize (&seat->pointer_info);
  /* FIXME: destroy data_device */
  g_clear_object (&seat->keyboard_settings);
  g_clear_object (&seat->drop_context);
  g_hash_table_destroy (seat->touches);
  gdk_window_destroy (seat->foreign_dnd_window);
  zwp_tablet_seat_v2_destroy (seat->wp_tablet_seat);
  stop_key_repeat (seat);

  G_OBJECT_CLASS (gdk_wayland_seat_parent_class)->finalize (object);
}

static GdkSeatCapabilities
gdk_wayland_seat_get_capabilities (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkSeatCapabilities caps = 0;

  if (wayland_seat->master_pointer)
    caps |= GDK_SEAT_CAPABILITY_POINTER;
  if (wayland_seat->master_keyboard)
    caps |= GDK_SEAT_CAPABILITY_KEYBOARD;
  if (wayland_seat->touch_master)
    caps |= GDK_SEAT_CAPABILITY_TOUCH;

  return caps;
}

static void
gdk_wayland_seat_set_grab_window (GdkWaylandSeat *seat,
                                  GdkWindow      *window)
{
  if (seat->grab_window)
    {
      _gdk_wayland_window_set_grab_seat (seat->grab_window, NULL);
      g_object_remove_weak_pointer (G_OBJECT (seat->grab_window),
                                    (gpointer *) &seat->grab_window);
      seat->grab_window = NULL;
    }

  if (window)
    {
      seat->grab_window = window;
      g_object_add_weak_pointer (G_OBJECT (window),
                                 (gpointer *) &seat->grab_window);
      _gdk_wayland_window_set_grab_seat (window, GDK_SEAT (seat));
    }
}

static GdkGrabStatus
gdk_wayland_seat_grab (GdkSeat                *seat,
                       GdkWindow              *window,
                       GdkSeatCapabilities     capabilities,
                       gboolean                owner_events,
                       GdkCursor              *cursor,
                       const GdkEvent         *event,
                       GdkSeatGrabPrepareFunc  prepare_func,
                       gpointer                prepare_func_data)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
  GdkDisplay *display = gdk_seat_get_display (seat);
  GdkWindow *native;
  GList *l;

  native = gdk_window_get_toplevel (window);

  while (native->window_type == GDK_WINDOW_OFFSCREEN)
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
          (!_gdk_window_has_impl (native) &&
           !gdk_window_is_viewable (native)))
        return GDK_GRAB_NOT_VIEWABLE;

      native = gdk_window_get_toplevel (native);
    }

  if (native == NULL || GDK_WINDOW_DESTROYED (native))
    return GDK_GRAB_NOT_VIEWABLE;

  gdk_wayland_seat_set_grab_window (wayland_seat, native);
  wayland_seat->grab_time = evtime;

  if (prepare_func)
    (prepare_func) (seat, window, prepare_func_data);

  if (!gdk_window_is_visible (window))
    {
      gdk_wayland_seat_set_grab_window (wayland_seat, NULL);
      g_critical ("Window %p has not been made visible in GdkSeatGrabPrepareFunc",
                  window);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  if (wayland_seat->master_pointer &&
      capabilities & GDK_SEAT_CAPABILITY_POINTER)
    {
      device_maybe_emit_grab_crossing (wayland_seat->master_pointer,
                                       native, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->master_pointer,
                                    window,
                                    native,
                                    GDK_OWNERSHIP_NONE,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK & ~GDK_POINTER_MOTION_HINT_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);

      gdk_wayland_seat_set_global_cursor (seat, cursor);
      g_set_object (&wayland_seat->cursor, cursor);
      gdk_wayland_device_update_window_cursor (wayland_seat->master_pointer);
    }

  if (wayland_seat->touch_master &&
      capabilities & GDK_SEAT_CAPABILITY_TOUCH)
    {
      device_maybe_emit_grab_crossing (wayland_seat->touch_master,
                                       native, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->touch_master,
                                    window,
                                    native,
                                    GDK_OWNERSHIP_NONE,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK & ~GDK_POINTER_MOTION_HINT_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);
    }

  if (wayland_seat->master_keyboard &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      device_maybe_emit_grab_crossing (wayland_seat->master_keyboard,
                                       native, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->master_keyboard,
                                    window,
                                    native,
                                    GDK_OWNERSHIP_NONE,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK & ~GDK_POINTER_MOTION_HINT_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);

      /* Inhibit shortcuts on toplevels if the seat grab is for the keyboard only */
      if (capabilities == GDK_SEAT_CAPABILITY_KEYBOARD &&
          native->window_type == GDK_WINDOW_TOPLEVEL)
        gdk_wayland_window_inhibit_shortcuts (window, seat);
    }

  if (wayland_seat->tablets &&
      capabilities & GDK_SEAT_CAPABILITY_TABLET_STYLUS)
    {
      for (l = wayland_seat->tablets; l; l = l->next)
        {
          GdkWaylandTabletData *tablet = l->data;

          if (tablet->current_tool)
            device_maybe_emit_grab_crossing (tablet->master, native, evtime);

          _gdk_display_add_device_grab (display,
                                        tablet->master,
                                        window,
                                        native,
                                        GDK_OWNERSHIP_NONE,
                                        owner_events,
                                        GDK_ALL_EVENTS_MASK & ~GDK_POINTER_MOTION_HINT_MASK,
                                        _gdk_display_get_next_serial (display),
                                        evtime,
                                        FALSE);

          gdk_wayland_device_update_window_cursor (tablet->master);
        }
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_seat_ungrab (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkDisplay *display = gdk_seat_get_display (seat);
  GdkDeviceGrabInfo *grab;
  GList *l;

  g_clear_object (&wayland_seat->grab_cursor);

  gdk_wayland_seat_set_grab_window (wayland_seat, NULL);

  if (wayland_seat->master_pointer)
    {
      device_maybe_emit_ungrab_crossing (wayland_seat->master_pointer,
                                         GDK_CURRENT_TIME);

      gdk_wayland_device_update_window_cursor (wayland_seat->master_pointer);
    }

  if (wayland_seat->master_keyboard)
    {
      GdkWindow *prev_focus;

      prev_focus = device_maybe_emit_ungrab_crossing (wayland_seat->master_keyboard,
                                                      GDK_CURRENT_TIME);
      if (prev_focus)
        gdk_wayland_window_restore_shortcuts (prev_focus, seat);
    }

  if (wayland_seat->touch_master)
    {
      grab = _gdk_display_get_last_device_grab (display, wayland_seat->touch_master);

      if (grab)
        grab->serial_end = grab->serial_start;
    }

  for (l = wayland_seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      grab = _gdk_display_get_last_device_grab (display, tablet->master);

      if (grab)
        grab->serial_end = grab->serial_start;
    }
}

static GdkDevice *
gdk_wayland_seat_get_master (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);

  if (capabilities == GDK_SEAT_CAPABILITY_POINTER)
    return wayland_seat->master_pointer;
  else if (capabilities == GDK_SEAT_CAPABILITY_KEYBOARD)
    return wayland_seat->master_keyboard;
  else if (capabilities == GDK_SEAT_CAPABILITY_TOUCH)
    return wayland_seat->touch_master;

  return NULL;
}

static GList *
gdk_wayland_seat_get_slaves (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GList *slaves = NULL;

  if (wayland_seat->finger_scrolling && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    slaves = g_list_prepend (slaves, wayland_seat->finger_scrolling);
  if (wayland_seat->continuous_scrolling && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    slaves = g_list_prepend (slaves, wayland_seat->continuous_scrolling);
  if (wayland_seat->wheel_scrolling && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    slaves = g_list_prepend (slaves, wayland_seat->wheel_scrolling);
  if (wayland_seat->pointer && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    slaves = g_list_prepend (slaves, wayland_seat->pointer);
  if (wayland_seat->keyboard && (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD))
    slaves = g_list_prepend (slaves, wayland_seat->keyboard);
  if (wayland_seat->touch && (capabilities & GDK_SEAT_CAPABILITY_TOUCH))
    slaves = g_list_prepend (slaves, wayland_seat->touch);

  if (wayland_seat->tablets && (capabilities & GDK_SEAT_CAPABILITY_TABLET_STYLUS))
    {
      GList *l;

      for (l = wayland_seat->tablets; l; l = l->next)
        {
          GdkWaylandTabletData *tablet = l->data;

          slaves = g_list_prepend (slaves, tablet->stylus_device);
          slaves = g_list_prepend (slaves, tablet->eraser_device);
        }
    }

  /* There is no specific capability for pads, return
   * them anyways if all devices are requested
   */
  if (capabilities == GDK_SEAT_CAPABILITY_ALL)
    {
      GList *l;

      for (l = wayland_seat->tablet_pads; l; l = l->next)
        {
          GdkWaylandTabletPadData *pad = l->data;

          slaves = g_list_prepend (slaves, pad->device);
        }
    }

  return slaves;
}

static void
gdk_wayland_seat_class_init (GdkWaylandSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->finalize = gdk_wayland_seat_finalize;

  seat_class->get_capabilities = gdk_wayland_seat_get_capabilities;
  seat_class->grab = gdk_wayland_seat_grab;
  seat_class->ungrab = gdk_wayland_seat_ungrab;
  seat_class->get_master = gdk_wayland_seat_get_master;
  seat_class->get_slaves = gdk_wayland_seat_get_slaves;
}

static void
gdk_wayland_seat_init (GdkWaylandSeat *seat)
{
}

static void
init_pointer_data (GdkWaylandPointerData *pointer_data,
                   GdkDisplay            *display,
                   GdkDevice             *master)
{
  GdkWaylandDisplay *display_wayland;

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  pointer_data->current_output_scale = 1;
  pointer_data->pointer_surface =
    wl_compositor_create_surface (display_wayland->compositor);
  wl_surface_add_listener (pointer_data->pointer_surface,
                           &pointer_surface_listener,
                           master);
}

static void
device_manager_device_added (GdkDeviceManager *device_manager,
                             GdkDevice        *device,
                             GdkSeat          *seat)
{
  g_signal_emit_by_name (seat, "device-added", device);
}

static void
device_manager_device_removed (GdkDeviceManager *device_manager,
                               GdkDevice        *device,
                               GdkSeat          *seat)
{
  g_signal_emit_by_name (seat, "device-removed", device);
}

void
_gdk_wayland_device_manager_add_seat (GdkDeviceManager *device_manager,
                                      guint32           id,
				      struct wl_seat   *wl_seat)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWaylandSeat *seat;

  display = gdk_device_manager_get_display (device_manager);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  seat = g_object_new (GDK_TYPE_WAYLAND_SEAT,
                       "display", display,
                       NULL);
  seat->id = id;
  seat->keymap = _gdk_wayland_keymap_new ();
  seat->display = display;
  seat->device_manager = device_manager;
  seat->touches = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) g_free);
  seat->foreign_dnd_window = create_foreign_dnd_window (display);
  seat->wl_seat = wl_seat;

  seat->pending_selection = GDK_NONE;

  wl_seat_add_listener (seat->wl_seat, &seat_listener, seat);
  wl_seat_set_user_data (seat->wl_seat, seat);

  if (display_wayland->zwp_primary_selection_manager_v1)
    {
      seat->zwp_primary_data_device_v1 =
        zwp_primary_selection_device_manager_v1_get_device (display_wayland->zwp_primary_selection_manager_v1,
                                                            seat->wl_seat);
      zwp_primary_selection_device_v1_add_listener (seat->zwp_primary_data_device_v1,
                                                    &zwp_primary_device_v1_listener,
                                                    seat);
    }
  else if (display_wayland->gtk_primary_selection_manager)
    {
      seat->gtk_primary_data_device =
        gtk_primary_selection_device_manager_get_device (display_wayland->gtk_primary_selection_manager,
                                                         seat->wl_seat);
      gtk_primary_selection_device_add_listener (seat->gtk_primary_data_device,
                                                 &gtk_primary_device_listener,
                                                 seat);
    }

  seat->data_device =
    wl_data_device_manager_get_data_device (display_wayland->data_device_manager,
                                            seat->wl_seat);
  seat->drop_context = _gdk_wayland_drop_context_new (display,
                                                      seat->data_device);
  wl_data_device_add_listener (seat->data_device,
                               &data_device_listener, seat);

  init_devices (seat);
  init_pointer_data (&seat->pointer_info, display, seat->master_pointer);

  if (display_wayland->tablet_manager)
    {
      seat->wp_tablet_seat =
        zwp_tablet_manager_v2_get_tablet_seat (display_wayland->tablet_manager,
                                               wl_seat);
      zwp_tablet_seat_v2_add_listener (seat->wp_tablet_seat, &tablet_seat_listener,
                                       seat);
    }

  g_signal_connect (seat->device_manager, "device-added",
                    G_CALLBACK (device_manager_device_added), seat);
  g_signal_connect (seat->device_manager, "device-removed",
                    G_CALLBACK (device_manager_device_removed), seat);

  gdk_display_add_seat (display, GDK_SEAT (seat));
}

void
_gdk_wayland_device_manager_remove_seat (GdkDeviceManager *manager,
                                         guint32           id)
{
  GdkDisplay *display = gdk_device_manager_get_display (manager);
  GList *l, *seats;

  seats = gdk_display_list_seats (display);

  for (l = seats; l != NULL; l = l->next)
    {
      GdkWaylandSeat *seat = l->data;

      if (seat->id != id)
        continue;

      g_signal_handlers_disconnect_by_func (manager,
                                            device_manager_device_added,
                                            seat);
      g_signal_handlers_disconnect_by_func (manager,
                                            device_manager_device_removed,
                                            seat);
      gdk_display_remove_seat (display, GDK_SEAT (seat));
      break;
    }

  g_list_free (seats);
}

static void
free_device (gpointer data)
{
  g_object_unref (data);
}

static void
gdk_wayland_device_manager_finalize (GObject *object)
{
  GdkWaylandDeviceManager *device_manager;

  device_manager = GDK_WAYLAND_DEVICE_MANAGER (object);

  g_list_free_full (device_manager->devices, free_device);

  G_OBJECT_CLASS (gdk_wayland_device_manager_parent_class)->finalize (object);
}

static GList *
gdk_wayland_device_manager_list_devices (GdkDeviceManager *device_manager,
                                         GdkDeviceType     type)
{
  GdkWaylandDeviceManager *wayland_device_manager;
  GList *devices = NULL, *l;

  wayland_device_manager = GDK_WAYLAND_DEVICE_MANAGER (device_manager);

  for (l = wayland_device_manager->devices; l; l = l->next)
    {
      if (gdk_device_get_device_type (l->data) == type)
        devices = g_list_prepend (devices, l->data);
    }

  return devices;
}

static GdkDevice *
gdk_wayland_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkWaylandDeviceManager *wayland_device_manager;
  GdkWaylandSeat *seat;
  GdkDevice *device;

  wayland_device_manager = GDK_WAYLAND_DEVICE_MANAGER (device_manager);

  /* Find the master pointer of the first GdkWaylandSeat we find */
  device = wayland_device_manager->devices->data;
  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));

  return seat->master_pointer;
}

static void
gdk_wayland_device_manager_class_init (GdkWaylandDeviceManagerClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_wayland_device_manager_finalize;
  device_manager_class->list_devices = gdk_wayland_device_manager_list_devices;
  device_manager_class->get_client_pointer = gdk_wayland_device_manager_get_client_pointer;
}

static void
gdk_wayland_device_manager_init (GdkWaylandDeviceManager *device_manager)
{
}

GdkDeviceManager *
_gdk_wayland_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_WAYLAND_DEVICE_MANAGER,
                       "display", display,
                       NULL);
}

uint32_t
_gdk_wayland_device_get_implicit_grab_serial (GdkWaylandDevice *device,
                                              const GdkEvent   *event)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (device));
  GdkEventSequence *sequence = NULL;
  GdkWaylandTouchData *touch = NULL;

  if (event)
    sequence = gdk_event_get_event_sequence (event);

  if (sequence)
    touch = gdk_wayland_seat_get_touch (GDK_WAYLAND_SEAT (seat),
                                        GDK_EVENT_SEQUENCE_TO_SLOT (sequence));

  if (touch)
    return touch->touch_down_serial;

  if (event)
    {
      GdkDevice *source = gdk_event_get_source_device (event);
      GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
      GList *l;

      for (l = wayland_seat->tablets; l; l = l->next)
        {
          GdkWaylandTabletData *tablet = l->data;

          if (tablet->current_device == source)
            return tablet->pointer_info.press_serial;
        }
    }

    return GDK_WAYLAND_SEAT (seat)->pointer_info.press_serial;
}

uint32_t
_gdk_wayland_seat_get_last_implicit_grab_serial (GdkSeat           *seat,
                                                 GdkEventSequence **sequence)
{
  GdkWaylandSeat *wayland_seat;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GList *l;
  uint32_t serial;

  wayland_seat = GDK_WAYLAND_SEAT (seat);
  g_hash_table_iter_init (&iter, wayland_seat->touches);

  if (sequence)
    *sequence = NULL;

  serial = wayland_seat->keyboard_key_serial;

  if (wayland_seat->pointer_info.press_serial > serial)
    serial = wayland_seat->pointer_info.press_serial;

  for (l = wayland_seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      if (tablet->pointer_info.press_serial > serial)
        serial = tablet->pointer_info.press_serial;
    }

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      if (touch->touch_down_serial > serial)
        {
          if (sequence)
            *sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);
          serial = touch->touch_down_serial;
        }
    }

  return serial;
}

void
gdk_wayland_device_unset_touch_grab (GdkDevice        *gdk_device,
                                     GdkEventSequence *sequence)
{
  GdkWaylandSeat *seat;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  touch = gdk_wayland_seat_get_touch (seat,
                                      GDK_EVENT_SEQUENCE_TO_SLOT (sequence));

  if (GDK_WAYLAND_DEVICE (seat->touch_master)->emulating_touch == touch)
    {
      GDK_WAYLAND_DEVICE (seat->touch_master)->emulating_touch = NULL;
      emulate_touch_crossing (touch->window, NULL,
                              seat->touch_master, seat->touch,
                              touch, GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL,
                              GDK_CURRENT_TIME);
    }

  event = _create_touch_event (seat, touch, GDK_TOUCH_CANCEL,
                               GDK_CURRENT_TIME);
  _gdk_wayland_display_deliver_event (seat->display, event);
}

void
gdk_wayland_seat_set_global_cursor (GdkSeat   *seat,
                                    GdkCursor *cursor)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkDevice *pointer;

  pointer = gdk_seat_get_pointer (seat);

  g_set_object (&wayland_seat->grab_cursor, cursor);
  gdk_wayland_device_set_window_cursor (pointer,
                                        gdk_wayland_device_get_focus (pointer),
                                        NULL);
}

struct wl_data_device *
gdk_wayland_device_get_data_device (GdkDevice *gdk_device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  return seat->data_device;
}

void
gdk_wayland_seat_set_selection (GdkSeat               *seat,
                                struct wl_data_source *source)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkWaylandDisplay *display_wayland;

  display_wayland = GDK_WAYLAND_DISPLAY (wayland_seat->display);

  wl_data_device_set_selection (wayland_seat->data_device, source,
                                _gdk_wayland_display_get_serial (display_wayland));
}

void
gdk_wayland_seat_set_primary (GdkSeat  *seat,
                              gpointer  source)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkWaylandDisplay *display_wayland;
  guint32 serial;

  if (source)
    {
      display_wayland = GDK_WAYLAND_DISPLAY (gdk_seat_get_display (seat));
      serial = _gdk_wayland_display_get_serial (display_wayland);
      if (wayland_seat->zwp_primary_data_device_v1)
        {
          zwp_primary_selection_device_v1_set_selection (wayland_seat->zwp_primary_data_device_v1,
                                                         source, serial);
        }
      else if (wayland_seat->gtk_primary_data_device)
        {
          gtk_primary_selection_device_set_selection (wayland_seat->gtk_primary_data_device,
                                                      source, serial);
        }
    }
}

/**
 * gdk_wayland_seat_get_wl_seat:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_seat of a #GdkSeat.
 *
 * Returns: (transfer none): a Wayland wl_seat
 *
 * Since: 3.20
 */
struct wl_seat *
gdk_wayland_seat_get_wl_seat (GdkSeat *seat)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SEAT (seat), NULL);

  return GDK_WAYLAND_SEAT (seat)->wl_seat;
}

GdkDragContext *
gdk_wayland_device_get_drop_context (GdkDevice *device)
{
  GdkSeat *seat = gdk_device_get_seat (device);

  return GDK_WAYLAND_SEAT (seat)->drop_context;
}

/**
 * gdk_wayland_device_get_node_path:
 * @device: a #GdkDevice
 *
 * Returns the /dev/input/event* path of this device.
 * For #GdkDevices that possibly coalesce multiple hardware
 * devices (eg. mouse, keyboard, touch,...), this function
 * will return %NULL.
 *
 * This is most notably implemented for devices of type
 * %GDK_SOURCE_PEN, %GDK_SOURCE_ERASER and %GDK_SOURCE_TABLET_PAD.
 *
 * Returns: the /dev/input/event* path of this device
 **/
const gchar *
gdk_wayland_device_get_node_path (GdkDevice *device)
{
  GdkWaylandTabletData *tablet;
  GdkWaylandTabletPadData *pad;

  GdkSeat *seat;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  seat = gdk_device_get_seat (device);
  tablet = gdk_wayland_device_manager_find_tablet (GDK_WAYLAND_SEAT (seat),
                                                   device);
  if (tablet)
    return tablet->path;

  pad = gdk_wayland_device_manager_find_pad (GDK_WAYLAND_SEAT (seat), device);
  if (pad)
    return pad->path;

  return NULL;
}

/**
 * gdk_wayland_device_pad_set_feedback:
 * @device: a %GDK_SOURCE_TABLET_PAD device
 * @feature: Feature to set the feedback label for
 * @feature_idx: 0-indexed index of the feature to set the feedback label for
 * @label: Feedback label
 *
 * Sets the feedback label for the given feature/index. This may be used by the
 * compositor to provide user feedback of the actions available/performed.
 **/
void
gdk_wayland_device_pad_set_feedback (GdkDevice           *device,
                                     GdkDevicePadFeature  feature,
                                     guint                feature_idx,
                                     const gchar         *label)
{
  GdkWaylandTabletPadData *pad;
  GdkWaylandTabletPadGroupData *group;
  GdkSeat *seat;

  seat = gdk_device_get_seat (device);
  pad = gdk_wayland_device_manager_find_pad (GDK_WAYLAND_SEAT (seat),
                                             device);
  if (!pad)
    return;

  if (feature == GDK_DEVICE_PAD_FEATURE_BUTTON)
    {
      group = tablet_pad_lookup_button_group (pad, feature_idx);
      if (!group)
        return;

      zwp_tablet_pad_v2_set_feedback (pad->wp_tablet_pad, feature_idx, label,
                                      group->mode_switch_serial);
    }
  else if (feature == GDK_DEVICE_PAD_FEATURE_RING)
    {
      struct zwp_tablet_pad_ring_v2 *wp_pad_ring;

      wp_pad_ring = g_list_nth_data (pad->rings, feature_idx);
      if (!wp_pad_ring)
        return;

      group = zwp_tablet_pad_ring_v2_get_user_data (wp_pad_ring);
      zwp_tablet_pad_ring_v2_set_feedback (wp_pad_ring, label,
                                           group->mode_switch_serial);

    }
  else if (feature == GDK_DEVICE_PAD_FEATURE_STRIP)
    {
      struct zwp_tablet_pad_strip_v2 *wp_pad_strip;

      wp_pad_strip = g_list_nth_data (pad->strips, feature_idx);
      if (!wp_pad_strip)
        return;

      group = zwp_tablet_pad_strip_v2_get_user_data (wp_pad_strip);
      zwp_tablet_pad_strip_v2_set_feedback (wp_pad_strip, label,
                                            group->mode_switch_serial);
    }
}
