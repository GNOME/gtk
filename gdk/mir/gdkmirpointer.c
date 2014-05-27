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
#include "gdkscreen.h"

typedef struct GdkMirPointer      GdkMirPointer;
typedef struct GdkMirPointerClass GdkMirPointerClass;

#define GDK_TYPE_MIR_POINTER              (gdk_mir_pointer_get_type ())
#define GDK_MIR_POINTER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_POINTER, GdkMirPointer))
#define GDK_MIR_POINTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_POINTER, GdkMirPointerClass))
#define GDK_IS_MIR_POINTER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_POINTER))
#define GDK_IS_MIR_POINTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_POINTER))
#define GDK_MIR_POINTER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_POINTER, GdkMirPointerClass))

struct GdkMirPointer
{
  GdkDevice parent_instance;

  /* Location of pointer */
  gdouble x;
  gdouble y;

  /* Window this pointer is over */
  GdkWindow *over_window;

  /* Current modifier mask */
  GdkModifierType modifier_mask;
};

struct GdkMirPointerClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkMirPointer, gdk_mir_pointer, GDK_TYPE_DEVICE)

GdkDevice *
_gdk_mir_pointer_new (GdkDeviceManager *device_manager, const gchar *name)
{
  return g_object_new (GDK_TYPE_MIR_POINTER,
                       "display", gdk_device_manager_get_display (device_manager),
                       "device-manager", device_manager,
                       "name", name,
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       NULL);
}

void
_gdk_mir_pointer_set_location (GdkDevice *pointer,
                               gdouble x,
                               gdouble y,
                               GdkWindow *window,
                               GdkModifierType mask)
{
  GdkMirPointer *p = GDK_MIR_POINTER (pointer);

  p->x = x;
  p->y = y;
  if (p->over_window)
    g_object_unref (p->over_window);
  p->over_window = g_object_ref (window);
  p->modifier_mask = mask;
}

static gboolean
gdk_mir_pointer_get_history (GdkDevice      *device,
                             GdkWindow      *window,
                             guint32         start,
                             guint32         stop,
                             GdkTimeCoord ***events,
                             gint           *n_events)
{
  g_printerr ("gdk_mir_pointer_get_history\n");
  return FALSE;
}

static void
gdk_mir_pointer_get_state (GdkDevice       *device,
                           GdkWindow       *window,
                           gdouble         *axes,
                           GdkModifierType *mask)
{
  g_printerr ("gdk_mir_pointer_get_state\n");
  GdkMirPointer *p = GDK_MIR_POINTER (device);

  if (axes)
    {
      axes[0] = p->x;
      axes[1] = p->y;
    }
  if (mask)
    *mask = p->modifier_mask;
}

static void
gdk_mir_pointer_set_window_cursor (GdkDevice *device,
                                   GdkWindow *window,
                                   GdkCursor *cursor)
{
  g_printerr ("gdk_mir_pointer_set_window_cursor\n");
  /* Mir doesn't support cursors */
}

static void
gdk_mir_pointer_warp (GdkDevice *device,
                      GdkScreen *screen,
                      gdouble    x,
                      gdouble    y)
{
  g_printerr ("gdk_mir_pointer_warp\n");
  /* Mir doesn't support warping */
}

static void
gdk_mir_pointer_query_state (GdkDevice        *device,
                             GdkWindow        *window,
                             GdkWindow       **root_window,
                             GdkWindow       **child_window,
                             gdouble          *root_x,
                             gdouble          *root_y,
                             gdouble          *win_x,
                             gdouble          *win_y,
                             GdkModifierType  *mask)
{
  g_printerr ("gdk_mir_pointer_query_state\n");
  GdkMirPointer *p = GDK_MIR_POINTER (device);

  if (root_window)
    *root_window = gdk_screen_get_root_window (gdk_display_get_default_screen (gdk_device_get_display (device)));
  if (child_window)
    *child_window = p->over_window;
  if (root_x)
    *root_x = p->x;
  if (root_y)
    *root_y = p->y;
  if (win_x)
    *win_x = p->x; // FIXME
  if (win_y)
    *win_y = p->y;
  if (mask)
    *mask = p->modifier_mask;
}

static GdkGrabStatus
gdk_mir_pointer_grab (GdkDevice    *device,
                      GdkWindow    *window,
                      gboolean      owner_events,
                      GdkEventMask  event_mask,
                      GdkWindow    *confine_to,
                      GdkCursor    *cursor,
                      guint32       time_)
{
  g_printerr ("gdk_mir_pointer_grab\n");
  /* Mir doesn't do grabs, so sure, you have the grab */
  return GDK_GRAB_SUCCESS;
}

static void
gdk_mir_pointer_ungrab (GdkDevice *device,
                        guint32    time_)
{
  g_printerr ("gdk_mir_pointer_ungrab\n");
  /* Mir doesn't do grabs */
}

static GdkWindow *
gdk_mir_pointer_window_at_position (GdkDevice       *device,
                                    gdouble         *win_x,
                                    gdouble         *win_y,
                                    GdkModifierType *mask,
                                    gboolean         get_toplevel)
{
  //g_printerr ("gdk_mir_pointer_window_at_position\n");
  GdkMirPointer *p = GDK_MIR_POINTER (device);

  if (win_x)
    *win_x = p->x;
  if (win_y)
    *win_y = p->y;
  if (mask)
    *mask = p->modifier_mask;

  return p->over_window;
}

static void
gdk_mir_pointer_select_window_events (GdkDevice    *device,
                                      GdkWindow    *window,
                                      GdkEventMask  event_mask)
{
  g_printerr ("gdk_mir_pointer_select_window_events\n");
  // FIXME?
}

static void
gdk_mir_pointer_init (GdkMirPointer *device)
{
}

static void
gdk_mir_pointer_class_init (GdkMirPointerClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_mir_pointer_get_history;
  device_class->get_state = gdk_mir_pointer_get_state;
  device_class->set_window_cursor = gdk_mir_pointer_set_window_cursor;
  device_class->warp = gdk_mir_pointer_warp;
  device_class->query_state = gdk_mir_pointer_query_state;
  device_class->grab = gdk_mir_pointer_grab;
  device_class->ungrab = gdk_mir_pointer_ungrab;
  device_class->window_at_position = gdk_mir_pointer_window_at_position;
  device_class->select_window_events = gdk_mir_pointer_select_window_events;
}
