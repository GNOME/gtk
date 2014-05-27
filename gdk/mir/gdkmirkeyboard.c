/*
 * Copyright © 2014 Canonical Ltd
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

#include "config.h"

#include "gdkdeviceprivate.h"

typedef struct GdkMirKeyboard      GdkMirKeyboard;
typedef struct GdkMirKeyboardClass GdkMirKeyboardClass;

#define GDK_TYPE_MIR_KEYBOARD              (gdk_mir_keyboard_get_type ())
#define GDK_MIR_KEYBOARD(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_KEYBOARD, GdkMirKeyboard))
#define GDK_MIR_KEYBOARD_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_KEYBOARD, GdkMirKeyboardClass))
#define GDK_IS_MIR_KEYBOARD(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_KEYBOARD))
#define GDK_IS_MIR_KEYBOARD_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_KEYBOARD))
#define GDK_MIR_KEYBOARD_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_KEYBOARD, GdkMirKeyboardClass))

struct GdkMirKeyboard
{
  GdkDevice parent_instance;
};

struct GdkMirKeyboardClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkMirKeyboard, gdk_mir_keyboard, GDK_TYPE_DEVICE)

GdkDevice *
_gdk_mir_keyboard_new (GdkDeviceManager *device_manager, const gchar *name)
{
  return g_object_new (GDK_TYPE_MIR_KEYBOARD,
                       "display", gdk_device_manager_get_display (device_manager),
                       "device-manager", device_manager,
                       "name", name,
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       NULL);
}

static gboolean
gdk_mir_keyboard_get_history (GdkDevice      *device,
                              GdkWindow      *window,
                              guint32         start,
                              guint32         stop,
                              GdkTimeCoord ***events,
                              gint           *n_events)
{
  g_printerr ("gdk_mir_keyboard_get_history\n");
  return FALSE;
}

static void
gdk_mir_keyboard_get_state (GdkDevice      *device,
                            GdkWindow       *window,
                            gdouble         *axes,
                            GdkModifierType *mask)
{
  g_printerr ("gdk_mir_keyboard_get_state\n");
}

static void
gdk_mir_keyboard_set_window_cursor (GdkDevice *device,
                                    GdkWindow *window,
                                    GdkCursor *cursor)
{
  //g_printerr ("gdk_mir_keyboard_set_window_cursor\n");
  /* Keyboards don't have cursors... */
}

static void
gdk_mir_keyboard_warp (GdkDevice *device,
                       GdkScreen *screen,
                       gdouble    x,
                       gdouble    y)
{
  //g_printerr ("gdk_mir_keyboard_warp\n");
  /* Can't warp a keyboard... */
}

static void
gdk_mir_keyboard_query_state (GdkDevice        *device,
                              GdkWindow        *window,
                              GdkWindow       **root_window,
                              GdkWindow       **child_window,
                              gdouble          *root_x,
                              gdouble          *root_y,
                              gdouble          *win_x,
                              gdouble          *win_y,
                              GdkModifierType  *mask)
{
  g_printerr ("gdk_mir_keyboard_query_state\n");
}

static GdkGrabStatus
gdk_mir_keyboard_grab (GdkDevice    *device,
                       GdkWindow    *window,
                       gboolean      owner_events,
                       GdkEventMask  event_mask,
                       GdkWindow    *confine_to,
                       GdkCursor    *cursor,
                       guint32       time_)
{
  //g_printerr ("gdk_mir_keyboard_grab\n");
  /* Mir doesn't do grabs, so sure, you have the grab */
  return GDK_GRAB_SUCCESS;
}

static void
gdk_mir_keyboard_ungrab (GdkDevice *device,
                         guint32    time_)
{
  //g_printerr ("gdk_mir_keyboard_ungrab\n");
  /* Mir doesn't do grabs */
}

static GdkWindow *
gdk_mir_keyboard_window_at_position (GdkDevice       *device,
                                     gdouble         *win_x,
                                     gdouble         *win_y,
                                     GdkModifierType *mask,
                                     gboolean         get_toplevel)
{
  //g_printerr ("gdk_mir_keyboard_window_at_position (%f, %f)\n", *win_x, *win_y);
  /* Keyboard don't have locations... */
  return NULL; // FIXME: Or the window with the keyboard focus?
}

static void
gdk_mir_keyboard_select_window_events (GdkDevice    *device,
                                       GdkWindow    *window,
                                       GdkEventMask  event_mask)
{
  g_printerr ("gdk_mir_keyboard_select_window_events\n");
}

static void
gdk_mir_keyboard_init (GdkMirKeyboard *device)
{
}

static void
gdk_mir_keyboard_class_init (GdkMirKeyboardClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_mir_keyboard_get_history;
  device_class->get_state = gdk_mir_keyboard_get_state;
  device_class->set_window_cursor = gdk_mir_keyboard_set_window_cursor;
  device_class->warp = gdk_mir_keyboard_warp;
  device_class->query_state = gdk_mir_keyboard_query_state;
  device_class->grab = gdk_mir_keyboard_grab;
  device_class->ungrab = gdk_mir_keyboard_ungrab;
  device_class->window_at_position = gdk_mir_keyboard_window_at_position;
  device_class->select_window_events = gdk_mir_keyboard_select_window_events;
}
