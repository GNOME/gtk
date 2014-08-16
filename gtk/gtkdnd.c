/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gdk/gdk.h"

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "gdk/x11/gdkx.h"
#ifdef XINPUT_2
#include <X11/extensions/XInput2.h>
#endif
#endif

#include "gtkdnd.h"
#include "gtkiconhelperprivate.h"
#include "gtkicontheme.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkplug.h"
#include "gtktooltip.h"
#include "gtkwindow.h"
#include "gtkintl.h"
#include "gtkselectionprivate.h"


/**
 * SECTION:gtkdnd
 * @Short_description: Functions for controlling drag and drop handling
 * @Title: Drag and Drop
 *
 * GTK+ has a rich set of functions for doing inter-process communication
 * via the drag-and-drop metaphor.
 *
 * As well as the functions listed here, applications may need to use some
 * facilities provided for [Selections][gtk3-Selections]. Also, the Drag and
 * Drop API makes use of signals in the #GtkWidget class.
 */


static GSList *source_widgets = NULL;

typedef struct _GtkDragSourceSite GtkDragSourceSite;
typedef struct _GtkDragSourceInfo GtkDragSourceInfo;
typedef struct _GtkDragDestSite GtkDragDestSite;
typedef struct _GtkDragDestInfo GtkDragDestInfo;
typedef struct _GtkDragAnim GtkDragAnim;


typedef enum 
{
  GTK_DRAG_STATUS_DRAG,
  GTK_DRAG_STATUS_WAIT,
  GTK_DRAG_STATUS_DROP
} GtkDragStatus;

struct _GtkDragSourceSite 
{
  GdkModifierType    start_button_mask;
  GtkTargetList     *target_list;        /* Targets for drag data */
  GdkDragAction      actions;            /* Possible actions */

  GtkIconHelper     *icon_helper;
  GtkGesture        *drag_gesture;

  /* Stored button press information to detect drag beginning */
  gint               state;
  gint               x, y;
};
  
struct _GtkDragSourceInfo 
{
  GtkWidget         *widget;
  GtkTargetList     *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;     /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *fallback_icon; /* Window for drag used on other screens */
  GtkWidget         *ipc_widget;  /* GtkInvisible for grab, message passing */
  GdkCursor         *cursor;      /* Cursor for drag */
  gint hot_x, hot_y;              /* Hot spot for drag */
  gint button;                    /* mouse button starting drag */

  GtkDragStatus      status;      /* drag status */
  GdkEvent          *last_event;  /* pending event */

  gint               start_x, start_y; /* Initial position */
  gint               cur_x, cur_y;     /* Current Position */
  GdkScreen         *cur_screen;       /* Current screen for pointer */

  guint32            grab_time;   /* timestamp for initial grab */
  GList             *selections;  /* selections we've claimed */
  
  GtkDragDestInfo   *proxy_dest;  /* Set if this is a proxy drag */

  guint              update_idle;      /* Idle function to update the drag */
  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_window */
  guint              have_grab : 1;    /* Do we still have the pointer grab */
  GtkIconHelper     *icon_helper;
  GdkCursor         *drag_cursors[6];
};

struct _GtkDragDestSite
{
  GtkDestDefaults    flags;
  GtkTargetList     *target_list;
  GdkDragAction      actions;
  GdkWindow         *proxy_window;
  GdkDragProtocol    proxy_protocol;
  guint              do_proxy     : 1;
  guint              proxy_coords : 1;
  guint              have_drag    : 1;
  guint              track_motion : 1;
};

struct _GtkDragDestInfo
{
  GtkWidget         *widget;              /* Widget in which drag is in */
  GdkDragContext    *context;             /* Drag context */
  GtkDragSourceInfo *proxy_source;        /* Set if this is a proxy drag */
  GtkSelectionData  *proxy_data;          /* Set while retrieving proxied data */
  guint32            proxy_drop_time;     /* Timestamp for proxied drop */
  guint              proxy_drop_wait : 1; /* Set if we are waiting for a
                                           * status reply before sending
                                           * a proxied drop on.
                                           */
  guint              dropped : 1;         /* Set after we receive a drop */
  gint               drop_x, drop_y;      /* Position of drop */
};

#define DROP_ABORT_TIME 300000

#define ANIM_TIME (0.5 * 1000 * 1000) /* half a second */

struct _GtkDragAnim 
{
  GtkDragSourceInfo *info;
  gint64 start_time;
};

typedef gboolean (* GtkDragDestCallback) (GtkWidget      *widget,
                                          GdkDragContext *context,
                                          gint            x,
                                          gint            y,
                                          guint32         time);

/* Enumeration for some targets we handle internally */

enum {
  TARGET_DELETE = 0x40000002
};

/* Forward declarations */
static void          gtk_drag_get_event_actions (const GdkEvent  *event,
                                                 gint             button,
                                                 GdkDragAction    actions,
                                                 GdkDragAction   *suggested_action,
                                                 GdkDragAction   *possible_actions);
static GdkCursor *   gtk_drag_get_cursor         (GtkWidget      *widget,
                                                  GdkDisplay     *display,
                                                  GdkDragAction   action,
                                                  GtkDragSourceInfo *info);
static void          gtk_drag_update_cursor      (GtkDragSourceInfo *info);
static GtkWidget    *gtk_drag_get_ipc_widget            (GtkWidget *widget);
static GtkWidget    *gtk_drag_get_ipc_widget_for_screen (GdkScreen *screen);
static void          gtk_drag_release_ipc_widget (GtkWidget      *widget);

static void     gtk_drag_selection_received     (GtkWidget        *widget,
                                                 GtkSelectionData *selection_data,
                                                 guint             time,
                                                 gpointer          data);
static gboolean gtk_drag_find_widget            (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 GtkDragDestInfo  *info,
                                                 gint              x,
                                                 gint              y,
                                                 guint32           time,
                                                 GtkDragDestCallback callback);
static void     gtk_drag_proxy_begin            (GtkWidget        *widget,
                                                 GtkDragDestInfo  *dest_info,
                                                 guint32           time);
static void     gtk_drag_dest_realized          (GtkWidget        *widget);
static void     gtk_drag_dest_hierarchy_changed (GtkWidget        *widget,
                                                 GtkWidget        *previous_toplevel);
static void     gtk_drag_dest_site_destroy      (gpointer          data);
static void     gtk_drag_dest_leave             (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 guint             time);
static gboolean gtk_drag_dest_motion            (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 gint              x,
                                                 gint              y,
                                                 guint             time);
static gboolean gtk_drag_dest_drop              (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 gint              x,
                                                 gint              y,
                                                 guint             time);

static GtkDragDestInfo *  gtk_drag_get_dest_info     (GdkDragContext *context,
                                                      gboolean        create);
static GtkDragSourceInfo *gtk_drag_get_source_info   (GdkDragContext *context,
                                                      gboolean        create);
static void               gtk_drag_clear_source_info (GdkDragContext *context);

static void gtk_drag_source_check_selection    (GtkDragSourceInfo *info, 
                                                GdkAtom            selection,
                                                guint32            time);
static void gtk_drag_source_release_selections (GtkDragSourceInfo *info,
                                                guint32            time);
static void gtk_drag_drop                      (GtkDragSourceInfo *info,
                                                guint32            time);
static void gtk_drag_drop_finished             (GtkDragSourceInfo *info,
                                                GtkDragResult      result,
                                                guint              time);
static void gtk_drag_cancel                    (GtkDragSourceInfo *info,
                                                GtkDragResult      result,
                                                guint32            time);

static gboolean gtk_drag_source_event_cb       (GtkWidget         *widget,
                                                GdkEvent          *event,
                                                gpointer           data);
static void gtk_drag_source_site_destroy       (gpointer           data);
static void gtk_drag_selection_get             (GtkWidget         *widget, 
                                                GtkSelectionData  *selection_data,
                                                guint              sel_info,
                                                guint32            time,
                                                gpointer           data);
static void gtk_drag_anim_destroy              (GtkDragAnim       *anim);
static gboolean gtk_drag_anim_timeout          (gpointer           data);
static void gtk_drag_remove_icon               (GtkDragSourceInfo *info);
static void gtk_drag_source_info_destroy       (GtkDragSourceInfo *info);
static void gtk_drag_add_update_idle           (GtkDragSourceInfo *info);

static void gtk_drag_update                    (GtkDragSourceInfo *info,
                                                GdkScreen         *screen,
                                                gint               x_root,
                                                gint               y_root,
                                                const GdkEvent    *event);
static gboolean gtk_drag_motion_cb             (GtkWidget         *widget, 
                                                GdkEventMotion    *event, 
                                                gpointer           data);
static gboolean gtk_drag_key_cb                (GtkWidget         *widget, 
                                                GdkEventKey       *event, 
                                                gpointer           data);
static gboolean gtk_drag_grab_broken_event_cb  (GtkWidget          *widget,
                                                GdkEventGrabBroken *event,
                                                gpointer            data);
static void     gtk_drag_grab_notify_cb        (GtkWidget         *widget,
                                                gboolean           was_grabbed,
                                                gpointer           data);
static gboolean gtk_drag_button_release_cb     (GtkWidget         *widget, 
                                                GdkEventButton    *event, 
                                                gpointer           data);
static gboolean gtk_drag_abort_timeout         (gpointer           data);

static void     set_icon_helper (GdkDragContext    *context,
                                 GtkIconHelper     *helper,
                                 gint               hot_x,
                                 gint               hot_y,
                                 gboolean           force_window);

/************************
 * Cursor and Icon data *
 ************************/

static struct {
  GdkDragAction action;
  const gchar  *name;
  GdkPixbuf    *pixbuf;
  GdkCursor    *cursor;
} drag_cursors[] = {
  { GDK_ACTION_DEFAULT, NULL },
  { GDK_ACTION_ASK,   "dnd-ask",  NULL, NULL },
  { GDK_ACTION_COPY,  "dnd-copy", NULL, NULL },
  { GDK_ACTION_MOVE,  "dnd-move", NULL, NULL },
  { GDK_ACTION_LINK,  "dnd-link", NULL, NULL },
  { 0              ,  "dnd-none", NULL, NULL },
};

/*********************
 * Utility functions *
 *********************/

static void
set_can_change_screen (GtkWidget *widget,
                       gboolean   can_change_screen)
{
  can_change_screen = can_change_screen != FALSE;
  
  g_object_set_data (G_OBJECT (widget), I_("gtk-dnd-can-change-screen"),
                     GUINT_TO_POINTER (can_change_screen));
}

static gboolean
get_can_change_screen (GtkWidget *widget)
{
  return g_object_get_data (G_OBJECT (widget), "gtk-dnd-can-change-screen") != NULL;

}

static GtkWidget *
gtk_drag_get_ipc_widget_for_screen (GdkScreen *screen)
{
  GtkWidget *result;
  GSList *drag_widgets = g_object_get_data (G_OBJECT (screen), 
                                            "gtk-dnd-ipc-widgets");
  
  if (drag_widgets)
    {
      GSList *tmp = drag_widgets;
      result = drag_widgets->data;
      drag_widgets = drag_widgets->next;
      g_object_set_data (G_OBJECT (screen),
                         I_("gtk-dnd-ipc-widgets"),
                         drag_widgets);
      g_slist_free_1 (tmp);
    }
  else
    {
      result = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_screen (GTK_WINDOW (result), screen);
      gtk_window_resize (GTK_WINDOW (result), 1, 1);
      gtk_window_move (GTK_WINDOW (result), -99, -99);
      gtk_widget_show (result);
    }  

  return result;
}

static GtkWidget *
gtk_drag_get_ipc_widget (GtkWidget *widget)
{
  GtkWidget *result;
  GtkWidget *toplevel;

  result = gtk_drag_get_ipc_widget_for_screen (gtk_widget_get_screen (widget));
  
  toplevel = gtk_widget_get_toplevel (widget);
  
  if (GTK_IS_WINDOW (toplevel))
    {
      if (gtk_window_has_group (GTK_WINDOW (toplevel)))
        gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                     GTK_WINDOW (result));
    }

  return result;
}

#if defined (GDK_WINDOWING_X11)

/*
 * We want to handle a handful of keys during DND, e.g. Escape to abort.
 * Grabbing the keyboard has the unfortunate side-effect of preventing
 * useful things such as using Alt-Tab to cycle between windows or
 * switching workspaces. Therefore, we just grab the few keys we are
 * interested in. Note that we need to put the grabs on the root window
 * in order for them to still work when the focus is moved to another
 * app/workspace.
 *
 * GDK needs a little help to successfully deliver root key events...
 */

static GdkFilterReturn
root_key_filter (GdkXEvent *xevent,
                 GdkEvent  *event,
                 gpointer   data)
{
  XEvent *ev = (XEvent *) xevent;

  if ((ev->type == KeyPress || ev->type == KeyRelease) &&
      ev->xkey.root == ev->xkey.window)
    ev->xkey.window = (Window)data;
  else if (ev->type == GenericEvent)
    {
      XGenericEventCookie *cookie;
      XIDeviceEvent *dev;

      cookie = &ev->xcookie;
      dev = (XIDeviceEvent *) cookie->data;

      if (dev->evtype == XI_KeyPress ||
          dev->evtype == XI_KeyRelease)
        dev->event = (Window)data;
    }

  return GDK_FILTER_CONTINUE;
}

typedef struct {
  gint keysym;
  gint modifiers;
} GrabKey;

static GrabKey grab_keys[] = {
  { XK_Escape, 0 },
  { XK_space, 0 },
  { XK_KP_Space, 0 },
  { XK_Return, 0 },
  { XK_KP_Enter, 0 },
  { XK_Up, 0 },
  { XK_Up, Mod1Mask },
  { XK_Down, 0 },
  { XK_Down, Mod1Mask },
  { XK_Left, 0 },
  { XK_Left, Mod1Mask },
  { XK_Right, 0 },
  { XK_Right, Mod1Mask },
  { XK_KP_Up, 0 },
  { XK_KP_Up, Mod1Mask },
  { XK_KP_Down, 0 },
  { XK_KP_Down, Mod1Mask },
  { XK_KP_Left, 0 },
  { XK_KP_Left, Mod1Mask },
  { XK_KP_Right, 0 },
  { XK_KP_Right, Mod1Mask }
};

static void
grab_dnd_keys (GtkWidget *widget,
               GdkDevice *device,
               guint32    time)
{
  guint i;
  GdkWindow *window, *root;
  gint keycode;
#ifdef XINPUT_2
  gint deviceid;
  XIGrabModifiers mods;
  gint num_mods;
  XIEventMask evmask;
  unsigned char mask[(XI_LASTEVENT + 7)/8];
  gboolean using_xi2;

  deviceid = gdk_x11_device_get_id (device);

  if (GDK_IS_X11_DEVICE_MANAGER_XI2 (gdk_display_get_device_manager (gtk_widget_get_display (widget))))
    using_xi2 = TRUE;
  else
    using_xi2 = FALSE;
#endif

  window = gtk_widget_get_window (widget);
  if (!GDK_IS_X11_WINDOW (window))
    {
      gdk_device_grab (device,
                       gtk_widget_get_window (widget),
                       GDK_OWNERSHIP_APPLICATION, FALSE,
                       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                       NULL, time);
      return;
    }


  root = gdk_screen_get_root_window (gtk_widget_get_screen (widget));

  gdk_error_trap_push ();

  for (i = 0; i < G_N_ELEMENTS (grab_keys); ++i)
    {
      keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (window), grab_keys[i].keysym);
      if (keycode == NoSymbol)
        continue;

#ifdef XINPUT_2
      if (using_xi2)
        {
          memset (mask, 0, sizeof (mask));
          XISetMask (mask, XI_KeyPress);
          XISetMask (mask, XI_KeyRelease);

          evmask.deviceid = deviceid;
          evmask.mask_len = sizeof (mask);
          evmask.mask = mask;

          num_mods = 1;
          mods.modifiers = grab_keys[i].modifiers;

          XIGrabKeycode (GDK_WINDOW_XDISPLAY (window),
                         deviceid,
                         keycode,
                         GDK_WINDOW_XID (root),
                         GrabModeAsync,
                         GrabModeAsync,
                         False,
                         &evmask,
                         num_mods,
                         &mods);
        }
      else
#endif
        XGrabKey (GDK_WINDOW_XDISPLAY (window),
                  keycode, grab_keys[i].modifiers,
                  GDK_WINDOW_XID (root),
                  FALSE,
                  GrabModeAsync,
                  GrabModeAsync);
    }

  gdk_flush ();
  gdk_error_trap_pop_ignored ();

  gdk_window_add_filter (NULL, root_key_filter, (gpointer) GDK_WINDOW_XID (window));
}

static void
ungrab_dnd_keys (GtkWidget *widget,
                 GdkDevice *device,
                 guint32    time)
{
  guint i;
  GdkWindow *window, *root;
  gint keycode;
#ifdef XINPUT_2
  XIGrabModifiers mods;
  gint num_mods;
  gint deviceid;
  gboolean using_xi2;

  deviceid = gdk_x11_device_get_id (device);
  if (GDK_IS_X11_DEVICE_MANAGER_XI2 (gdk_display_get_device_manager (gtk_widget_get_display (widget))))
    using_xi2 = TRUE;
  else
    using_xi2 = FALSE;
#endif

  window = gtk_widget_get_window (widget);
  if (!GDK_IS_X11_WINDOW (window))
    {
      gdk_device_ungrab (device, time);
      return;
    }

  root = gdk_screen_get_root_window (gtk_widget_get_screen (widget));

  gdk_window_remove_filter (NULL, root_key_filter, (gpointer) GDK_WINDOW_XID (window));

  gdk_error_trap_push ();

  for (i = 0; i < G_N_ELEMENTS (grab_keys); ++i)
    {
      keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (window), grab_keys[i].keysym);
      if (keycode == NoSymbol)
        continue;

#ifdef XINPUT_2
      if (using_xi2)
        {
          num_mods = 1;
          mods.modifiers = grab_keys[i].modifiers;

          XIUngrabKeycode (GDK_WINDOW_XDISPLAY (window),
                           deviceid,
                           keycode,
                           GDK_WINDOW_XID (root),
                           num_mods,
                           &mods);
        }
      else
#endif
        XUngrabKey (GDK_WINDOW_XDISPLAY (window),
                    keycode, grab_keys[i].modifiers,
                    GDK_WINDOW_XID (root));
    }

  gdk_flush ();
  gdk_error_trap_pop_ignored ();
}

#else /* !GDK_WINDOWING_X11 */

static void
grab_dnd_keys (GtkWidget *widget,
               GdkDevice *device,
               guint32    time)
{
  gdk_device_grab (device,
                   gtk_widget_get_window (widget),
                   GDK_OWNERSHIP_APPLICATION, FALSE,
                   GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                   NULL, time);
}

static void
ungrab_dnd_keys (GtkWidget *widget,
                 GdkDevice *device,
                 guint32    time)
{
  gdk_device_ungrab (device, time);
}

#endif /* GDK_WINDOWING_X11 */


/*
 * gtk_drag_release_ipc_widget:
 * @widget: the widget to release
 *
 * Releases widget retrieved with gtk_drag_get_ipc_widget().
 */
static void
gtk_drag_release_ipc_widget (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GdkDragContext *context = g_object_get_data (G_OBJECT (widget), "drag-context");
  GSList *drag_widgets = g_object_get_data (G_OBJECT (screen),
                                            "gtk-dnd-ipc-widgets");
  GdkDevice *pointer, *keyboard;

  if (context)
    {
      pointer = gdk_drag_context_get_device (context);
      keyboard = gdk_device_get_associated_device (pointer);

      if (keyboard)
        ungrab_dnd_keys (widget, keyboard, GDK_CURRENT_TIME);
    }

  if (gtk_window_has_group (window))
    gtk_window_group_remove_window (gtk_window_get_group (window),
                                    window);
  drag_widgets = g_slist_prepend (drag_widgets, widget);
  g_object_set_data (G_OBJECT (screen),
                     I_("gtk-dnd-ipc-widgets"),
                     drag_widgets);
}

static guint32
gtk_drag_get_event_time (GdkEvent *event)
{
  guint32 tm = GDK_CURRENT_TIME;
  
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
        tm = event->motion.time; break;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        tm = event->button.time; break;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
        tm = event->key.time; break;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
        tm = event->crossing.time; break;
      case GDK_PROPERTY_NOTIFY:
        tm = event->property.time; break;
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
        tm = event->selection.time; break;
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
        tm = event->proximity.time; break;
      default:                  /* use current time */
        break;
      }
  
  return tm;
}

static void
gtk_drag_get_event_actions (const GdkEvent *event,
                            gint            button,
                            GdkDragAction   actions,
                            GdkDragAction  *suggested_action,
                            GdkDragAction  *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if (event)
    {
      GdkModifierType state = 0;
      
      switch (event->type)
        {
        case GDK_MOTION_NOTIFY:
          state = event->motion.state;
          break;
        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
          state = event->button.state;
          break;
        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
          state = event->key.state;
          break;
        case GDK_ENTER_NOTIFY:
        case GDK_LEAVE_NOTIFY:
          state = event->crossing.state;
          break;
        default:
          break;
        }

      if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
        {
          *suggested_action = GDK_ACTION_ASK;
          *possible_actions = actions;
        }
      else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
        {
          if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
            {
              if (actions & GDK_ACTION_LINK)
                {
                  *suggested_action = GDK_ACTION_LINK;
                  *possible_actions = GDK_ACTION_LINK;
                }
            }
          else if (state & GDK_CONTROL_MASK)
            {
              if (actions & GDK_ACTION_COPY)
                {
                  *suggested_action = GDK_ACTION_COPY;
                  *possible_actions = GDK_ACTION_COPY;
                }
              return;
            }
          else
            {
              if (actions & GDK_ACTION_MOVE)
                {
                  *suggested_action = GDK_ACTION_MOVE;
                  *possible_actions = GDK_ACTION_MOVE;
                }
              return;
            }
        }
      else
        {
          *possible_actions = actions;

          if ((state & (GDK_MOD1_MASK)) && (actions & GDK_ACTION_ASK))
            *suggested_action = GDK_ACTION_ASK;
          else if (actions & GDK_ACTION_COPY)
            *suggested_action =  GDK_ACTION_COPY;
          else if (actions & GDK_ACTION_MOVE)
            *suggested_action = GDK_ACTION_MOVE;
          else if (actions & GDK_ACTION_LINK)
            *suggested_action = GDK_ACTION_LINK;
        }
    }
  else
    {
      *possible_actions = actions;
      
      if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

static gboolean
gtk_drag_can_use_rgba_cursor (GdkDisplay *display, 
                              gint        width,
                              gint        height)
{
  guint max_width, max_height;
  
  if (!gdk_display_supports_cursor_color (display))
    return FALSE;

  if (!gdk_display_supports_cursor_alpha (display))
    return FALSE;

  gdk_display_get_maximal_cursor_size (display, 
                                       &max_width,
                                       &max_height);
  if (width > max_width || height > max_height)
    {
       /* can't use rgba cursor (too large) */
      return FALSE;
    }

  return TRUE;
}

static void
ensure_drag_cursor_pixbuf (int i)
{
  if (drag_cursors[i].pixbuf == NULL)
    {
      char *path = g_strconcat ("/org/gtk/libgtk/cursor/",  drag_cursors[i].name, ".png", NULL);
      GInputStream *stream = g_resources_open_stream (path, 0, NULL);
      if (stream != NULL)
        {
          drag_cursors[i].pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
          g_object_unref (stream);
        }
      g_free (path);
    }
}

static void
get_surface_size (cairo_surface_t *surface,
                  gint            *width,
                  gint            *height)
{
  gdouble x_scale, y_scale;

  x_scale = y_scale = 1;

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_get_device_scale (surface, &x_scale, &y_scale);
#endif

  /* Assume any set scaling is icon scale */
  *width =
    ceil (cairo_image_surface_get_width (surface) / x_scale);
  *height =
    ceil (cairo_image_surface_get_height (surface) / y_scale);
}

static GdkCursor *
gtk_drag_get_cursor (GtkWidget         *widget,
                     GdkDisplay        *display,
                     GdkDragAction      action,
                     GtkDragSourceInfo *info)
{
  gint i;

  /* reconstruct the cursors for each new drag (thus !info),
   * to catch cursor theme changes 
   */ 
  if (!info)
    {
      for (i = 0 ; i < G_N_ELEMENTS (drag_cursors) - 1; i++)
        if (drag_cursors[i].cursor != NULL)
          {
            g_object_unref (drag_cursors[i].cursor);
            drag_cursors[i].cursor = NULL;
          }
    }
 
  for (i = 0 ; i < G_N_ELEMENTS (drag_cursors) - 1; i++)
    if (drag_cursors[i].action == action)
      break;

  if (drag_cursors[i].cursor != NULL)
    {
      if (display != gdk_cursor_get_display (drag_cursors[i].cursor))
        {
          g_object_unref (drag_cursors[i].cursor);
          drag_cursors[i].cursor = NULL;
        }
    }
  
  if (drag_cursors[i].cursor == NULL)
    drag_cursors[i].cursor = gdk_cursor_new_from_name (display, drag_cursors[i].name);
  
  if (drag_cursors[i].cursor == NULL)
    {
      ensure_drag_cursor_pixbuf (i);
      drag_cursors[i].cursor = gdk_cursor_new_from_pixbuf (display, drag_cursors[i].pixbuf, 0, 0);
    }

  if (info && info->icon_helper) 
    {
      gint cursor_width, cursor_height;
      gint icon_width, icon_height;
      gint width, height;
      cairo_surface_t *icon_surface, *cursor_surface;
      gdouble hot_x, hot_y;
      gint icon_x, icon_y, ref_x, ref_y;
      gint scale;

      if (info->drag_cursors[i] != NULL)
        {
          if (display == gdk_cursor_get_display (info->drag_cursors[i]))
            return info->drag_cursors[i];
          
          g_object_unref (info->drag_cursors[i]);
          info->drag_cursors[i] = NULL;
        }

      scale = gtk_widget_get_scale_factor (widget);
      _gtk_icon_helper_get_size (info->icon_helper,
                                 gtk_widget_get_style_context (widget),
                                 &icon_width, &icon_height);
      icon_surface = _gtk_icon_helper_ensure_surface (info->icon_helper,
                                                      gtk_widget_get_style_context (widget));

      icon_x = info->hot_x;
      icon_y = info->hot_y;

      hot_x = hot_y = 0;
      cursor_surface = gdk_cursor_get_surface (drag_cursors[i].cursor,
                                               &hot_x, &hot_y);
      if (!cursor_surface)
        {
          ensure_drag_cursor_pixbuf (i);
          cursor_surface = gdk_cairo_surface_create_from_pixbuf (drag_cursors[i].pixbuf, 1, NULL);
        }
      else
        {
#if 0     
          /* The code below is an attempt to let cursor themes
           * determine the attachment of the icon to enable things
           * like the following:
           *
           *    +-----+
           *    |     |
           *    |     ||
           *    +-----+|
           *        ---+
           * 
           * It does not work since Xcursor doesn't allow to attach
           * any additional information to cursors in a retrievable
           * way  (there are comments, but no way to get at them
           * short of searching for the actual cursor file).
           * If this code ever gets used, the icon_window placement
           * must be changed to recognize these placement options
           * as well. Note that this code ignores info->hot_x/y.
           */ 
          for (j = 0; j < 10; j++)
            {
              const gchar *opt;
              gchar key[32];
              gchar **toks;
              GtkAnchorType icon_anchor;

              g_snprintf (key, 32, "comment%d", j);
              opt = gdk_pixbuf_get_option (cursor_pixbuf, key);
              if (opt && g_str_has_prefix ("icon-attach:", opt))
                {
                  toks = g_strsplit (opt + strlen ("icon-attach:"), "'", -1);
                  if (g_strv_length (toks) != 3)
                    {
                      g_strfreev (toks);
                      break;
                    }
                  icon_anchor = atoi (toks[0]);
                  icon_x = atoi (toks[1]);
                  icon_y = atoi (toks[2]);
                  
                  switch (icon_anchor)
                    {
                    case GTK_ANCHOR_NORTH:
                    case GTK_ANCHOR_CENTER:
                    case GTK_ANCHOR_SOUTH:
                      icon_x += icon_width / 2;
                      break;
                    case GTK_ANCHOR_NORTH_EAST:
                    case GTK_ANCHOR_EAST:
                    case GTK_ANCHOR_SOUTH_EAST:
                      icon_x += icon_width;
                      break;
                    default: ;
                    }
                  
                  switch (icon_anchor)
                    {
                    case GTK_ANCHOR_WEST:
                    case GTK_ANCHOR_CENTER:
                    case GTK_ANCHOR_EAST:
                      icon_y += icon_height / 2;
                      break;
                    case GTK_ANCHOR_SOUTH_WEST:
                    case GTK_ANCHOR_SOUTH:
                    case GTK_ANCHOR_SOUTH_EAST:
                      icon_x += icon_height;
                      break;
                    default: ;
                    }

                  g_strfreev (toks);
                  break;
                }
            }
#endif
        }

      get_surface_size (cursor_surface, &cursor_width, &cursor_height);
      
      ref_x = MAX (hot_x, icon_x);
      ref_y = MAX (hot_y, icon_y);
      width = ref_x + MAX (cursor_width - hot_x, icon_width - icon_x);
      height = ref_y + MAX (cursor_height - hot_y, icon_height - icon_y);

      if (gtk_drag_can_use_rgba_cursor (display, width * scale, height * scale))
        {
          cairo_surface_t *surface;
          cairo_t *cr;

          /* Composite cursor and icon so that both hotspots
           * end up at (ref_x, ref_y)
           */
          surface =
              gdk_window_create_similar_image_surface (NULL,
                                                       CAIRO_FORMAT_ARGB32,
                                                       width * scale, height * scale, scale);

          cr = cairo_create (surface);
          cairo_set_source_surface (cr, icon_surface,
                                    ref_x - icon_x, ref_y - icon_y);
          cairo_paint (cr);

          cairo_set_source_surface (cr, cursor_surface,
                                    ref_x - hot_x, ref_y - hot_y);
          cairo_paint (cr);

          cairo_destroy (cr);

          info->drag_cursors[i] = 
            gdk_cursor_new_from_surface (display, surface, ref_x, ref_y);
          
          cairo_surface_destroy (surface);
        }
      
      cairo_surface_destroy (cursor_surface);
      cairo_surface_destroy (icon_surface);
      
      if (info->drag_cursors[i] != NULL)
        return info->drag_cursors[i];
    }
 
  return drag_cursors[i].cursor;
}

static void
gtk_drag_update_cursor (GtkDragSourceInfo *info)
{
  GdkCursor *cursor;
  gint i;

  if (!info->have_grab)
    return;

  for (i = 0 ; i < G_N_ELEMENTS (drag_cursors) - 1; i++)
    if (info->cursor == drag_cursors[i].cursor ||
        info->cursor == info->drag_cursors[i])
      break;
  
  if (i == G_N_ELEMENTS (drag_cursors))
    return;

  cursor = gtk_drag_get_cursor (info->widget,
                                gdk_cursor_get_display (info->cursor), 
                                drag_cursors[i].action, info);
  
  if (cursor != info->cursor)
    {
      GdkDevice *pointer;

      pointer = gdk_drag_context_get_device (info->context);
      gdk_device_grab (pointer,
                       gtk_widget_get_window (info->ipc_widget),
                       GDK_OWNERSHIP_APPLICATION, FALSE,
                       GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                       cursor, info->grab_time);
      info->cursor = cursor;
    }
}

/********************
 * Destination side *
 ********************/

/**
 * gtk_drag_get_data: (method)
 * @widget: the widget that will receive the
 *   #GtkWidget::drag-data-received signal
 * @context: the drag context
 * @target: the target (form of the data) to retrieve
 * @time_: a timestamp for retrieving the data. This will
 *   generally be the time received in a #GtkWidget::drag-motion"
 *   or #GtkWidget::drag-drop" signal
 *
 * Gets the data associated with a drag. When the data
 * is received or the retrieval fails, GTK+ will emit a
 * #GtkWidget::drag-data-received signal. Failure of the retrieval
 * is indicated by the length field of the @selection_data
 * signal parameter being negative. However, when gtk_drag_get_data()
 * is called implicitely because the %GTK_DEST_DEFAULT_DROP was set,
 * then the widget will not receive notification of failed
 * drops.
 */
void
gtk_drag_get_data (GtkWidget      *widget,
                   GdkDragContext *context,
                   GdkAtom         target,
                   guint32         time_)
{
  GtkWidget *selection_widget;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  selection_widget = gtk_drag_get_ipc_widget (widget);

  g_object_ref (context);
  g_object_ref (widget);

  g_signal_connect (selection_widget, "selection-received",
                    G_CALLBACK (gtk_drag_selection_received), widget);

  g_object_set_data (G_OBJECT (selection_widget), I_("drag-context"), context);

  gtk_selection_convert (selection_widget,
                         gdk_drag_get_selection (context),
                         target,
                         time_);
}

/**
 * gtk_drag_get_source_widget: (method)
 * @context: a (destination side) drag context
 *
 * Determines the source widget for a drag.
 *
 * Returns: (transfer none): if the drag is occurring
 *     within a single application, a pointer to the source widget.
 *     Otherwise, %NULL.
 */
GtkWidget *
gtk_drag_get_source_widget (GdkDragContext *context)
{
  GSList *tmp_list;

  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);
  
  tmp_list = source_widgets;
  while (tmp_list)
    {
      GtkWidget *ipc_widget = tmp_list->data;

      if (gtk_widget_get_window (ipc_widget) == gdk_drag_context_get_source_window (context))
        {
          GtkDragSourceInfo *info;
          info = g_object_get_data (G_OBJECT (ipc_widget), "gtk-info");

          return info ? info->widget : NULL;
        }

      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * gtk_drag_finish: (method)
 * @context: the drag context
 * @success: a flag indicating whether the drop was successful
 * @del: a flag indicating whether the source should delete the
 *   original data. (This should be %TRUE for a move)
 * @time_: the timestamp from the #GtkWidget::drag-drop signal
 *
 * Informs the drag source that the drop is finished, and
 * that the data of the drag will no longer be required.
 */
void 
gtk_drag_finish (GdkDragContext *context,
                 gboolean        success,
                 gboolean        del,
                 guint32         time)
{
  GdkAtom target = GDK_NONE;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  if (success && del)
    {
      target = gdk_atom_intern_static_string ("DELETE");
    }

  if (target != GDK_NONE)
    {
      GtkWidget *selection_widget = gtk_drag_get_ipc_widget_for_screen (gdk_window_get_screen (gdk_drag_context_get_source_window (context)));

      g_object_ref (context);
      
      g_object_set_data (G_OBJECT (selection_widget), I_("drag-context"), context);
      g_signal_connect (selection_widget, "selection-received",
                        G_CALLBACK (gtk_drag_selection_received),
                        NULL);
      
      gtk_selection_convert (selection_widget,
                             gdk_drag_get_selection (context),
                             target,
                             time);
    }
  
  if (!(success && del))
    gdk_drop_finish (context, success, time);
}

static gboolean
gtk_drag_highlight_draw (GtkWidget *widget,
                         cairo_t   *cr,
                         gpointer   data)
{
  GtkAllocation alloc;
  GtkStyleContext *context;

  if (GTK_IS_WINDOW (widget))
    {
      /* We don't want to draw the drag highlight around the
       * CSD window decorations
       */
      gtk_widget_get_allocation (gtk_bin_get_child (GTK_BIN (widget)), &alloc);
    }
  else
    {
      alloc.x = 0;
      alloc.y = 0;
      alloc.width = gtk_widget_get_allocated_width (widget);
      alloc.height = gtk_widget_get_allocated_height (widget);
    }

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_DND);

  gtk_render_frame (context, cr, alloc.x, alloc.y, alloc.width, alloc.height);

  gtk_style_context_restore (context);

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   alloc.x + 0.5, alloc.y + 0.5,
                   alloc.width - 1, alloc.height - 1);
  cairo_stroke (cr);

  return FALSE;
}

/**
 * gtk_drag_highlight: (method)
 * @widget: a widget to highlight
 *
 * Draws a highlight around a widget. This will attach
 * handlers to #GtkWidget::draw, so the highlight
 * will continue to be displayed until gtk_drag_unhighlight()
 * is called.
 */
void 
gtk_drag_highlight (GtkWidget  *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_signal_connect_after (widget, "draw",
                          G_CALLBACK (gtk_drag_highlight_draw),
                          NULL);

  gtk_widget_queue_draw (widget);
}

/**
 * gtk_drag_unhighlight: (method)
 * @widget: a widget to remove the highlight from
 *
 * Removes a highlight set by gtk_drag_highlight() from
 * a widget.
 */
void 
gtk_drag_unhighlight (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_signal_handlers_disconnect_by_func (widget,
                                        gtk_drag_highlight_draw,
                                        NULL);
  
  gtk_widget_queue_draw (widget);
}

static void
gtk_drag_dest_set_internal (GtkWidget       *widget,
                            GtkDragDestSite *site)
{
  GtkDragDestSite *old_site;
  
  g_return_if_fail (widget != NULL);

  /* HACK, do this in the destroy */
  old_site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_realized,
                                            old_site);
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_hierarchy_changed,
                                            old_site);

      site->track_motion = old_site->track_motion;
    }

  if (gtk_widget_get_realized (widget))
    gtk_drag_dest_realized (widget);

  g_signal_connect (widget, "realize",
                    G_CALLBACK (gtk_drag_dest_realized), site);
  g_signal_connect (widget, "hierarchy-changed",
                    G_CALLBACK (gtk_drag_dest_hierarchy_changed), site);

  g_object_set_data_full (G_OBJECT (widget), I_("gtk-drag-dest"),
                          site, gtk_drag_dest_site_destroy);
}

/**
 * gtk_drag_dest_set: (method)
 * @widget: a #GtkWidget
 * @flags: which types of default drag behavior to use
 * @targets: (allow-none) (array length=n_targets): a pointer to an array of
 *     #GtkTargetEntrys indicating the drop types that this @widget will
 *     accept, or %NULL. Later you can access the list with
 *     gtk_drag_dest_get_target_list() and gtk_drag_dest_find_target().
 * @n_targets: the number of entries in @targets
 * @actions: a bitmask of possible actions for a drop onto this @widget.
 *
 * Sets a widget as a potential drop destination, and adds default behaviors.
 *
 * The default behaviors listed in @flags have an effect similar
 * to installing default handlers for the widget’s drag-and-drop signals
 * (#GtkWidget::drag-motion, #GtkWidget::drag-drop, ...). They all exist
 * for convenience. When passing #GTK_DEST_DEFAULT_ALL for instance it is
 * sufficient to connect to the widget’s #GtkWidget::drag-data-received
 * signal to get primitive, but consistent drag-and-drop support.
 *
 * Things become more complicated when you try to preview the dragged data,
 * as described in the documentation for #GtkWidget::drag-motion. The default
 * behaviors described by @flags make some assumptions, that can conflict
 * with your own signal handlers. For instance #GTK_DEST_DEFAULT_DROP causes
 * invokations of gdk_drag_status() in the context of #GtkWidget::drag-motion,
 * and invokations of gtk_drag_finish() in #GtkWidget::drag-data-received.
 * Especially the later is dramatic, when your own #GtkWidget::drag-motion
 * handler calls gtk_drag_get_data() to inspect the dragged data.
 *
 * There’s no way to set a default action here, you can use the
 * #GtkWidget::drag-motion callback for that. Here’s an example which selects
 * the action to use depending on whether the control key is pressed or not:
 * |[<!-- language="C" -->
 * static void
 * drag_motion (GtkWidget *widget,
 *              GdkDragContext *context,
 *              gint x,
 *              gint y,
 *              guint time)
 * {
 *   GdkModifierType mask;
 *
 *   gdk_window_get_pointer (gtk_widget_get_window (widget),
 *                           NULL, NULL, &mask);
 *   if (mask & GDK_CONTROL_MASK)
 *     gdk_drag_status (context, GDK_ACTION_COPY, time);
 *   else
 *     gdk_drag_status (context, GDK_ACTION_MOVE, time);
 * }
 * ]|
 */
void
gtk_drag_dest_set (GtkWidget            *widget,
                   GtkDestDefaults       flags,
                   const GtkTargetEntry *targets,
                   gint                  n_targets,
                   GdkDragAction         actions)
{
  GtkDragDestSite *site;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_slice_new0 (GtkDragDestSite);

  site->flags = flags;
  site->have_drag = FALSE;
  if (targets)
    site->target_list = gtk_target_list_new (targets, n_targets);
  else
    site->target_list = NULL;
  site->actions = actions;
  site->do_proxy = FALSE;
  site->proxy_window = NULL;
  site->track_motion = FALSE;

  gtk_drag_dest_set_internal (widget, site);
}

/**
 * gtk_drag_dest_set_proxy: (method)
 * @widget: a #GtkWidget
 * @proxy_window: the window to which to forward drag events
 * @protocol: the drag protocol which the @proxy_window accepts
 *   (You can use gdk_drag_get_protocol() to determine this)
 * @use_coordinates: If %TRUE, send the same coordinates to the
 *   destination, because it is an embedded
 *   subwindow.
 *
 * Sets this widget as a proxy for drops to another window.
 */
void 
gtk_drag_dest_set_proxy (GtkWidget      *widget,
                         GdkWindow      *proxy_window,
                         GdkDragProtocol protocol,
                         gboolean        use_coordinates)
{
  GtkDragDestSite *site;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!proxy_window || GDK_IS_WINDOW (proxy_window));

  site = g_slice_new (GtkDragDestSite);

  site->flags = 0;
  site->have_drag = FALSE;
  site->target_list = NULL;
  site->actions = 0;
  site->proxy_window = proxy_window;
  if (proxy_window)
    g_object_ref (proxy_window);
  site->do_proxy = TRUE;
  site->proxy_protocol = protocol;
  site->proxy_coords = use_coordinates;
  site->track_motion = FALSE;

  gtk_drag_dest_set_internal (widget, site);
}

/**
 * gtk_drag_dest_unset: (method)
 * @widget: a #GtkWidget
 *
 * Clears information about a drop destination set with
 * gtk_drag_dest_set(). The widget will no longer receive
 * notification of drags.
 */
void 
gtk_drag_dest_unset (GtkWidget *widget)
{
  GtkDragDestSite *old_site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_site = g_object_get_data (G_OBJECT (widget),
                                "gtk-drag-dest");
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_realized,
                                            old_site);
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_hierarchy_changed,
                                            old_site);
    }

  g_object_set_data (G_OBJECT (widget), I_("gtk-drag-dest"), NULL);
}

/**
 * gtk_drag_dest_get_target_list: (method)
 * @widget: a #GtkWidget
 * 
 * Returns the list of targets this widget can accept from
 * drag-and-drop.
 * 
 * Returns: (transfer none): the #GtkTargetList, or %NULL if none
 */
GtkTargetList*
gtk_drag_dest_get_target_list (GtkWidget *widget)
{
  GtkDragDestSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");

  return site ? site->target_list : NULL;  
}

/**
 * gtk_drag_dest_set_target_list: (method)
 * @widget: a #GtkWidget that’s a drag destination
 * @target_list: (allow-none): list of droppable targets, or %NULL for none
 * 
 * Sets the target types that this widget can accept from drag-and-drop.
 * The widget must first be made into a drag destination with
 * gtk_drag_dest_set().
 */
void
gtk_drag_dest_set_target_list (GtkWidget     *widget,
                               GtkTargetList *target_list)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  
  if (!site)
    {
      g_warning ("Can't set a target list on a widget until you've called gtk_drag_dest_set() "
                 "to make the widget into a drag destination");
      return;
    }

  if (target_list)
    gtk_target_list_ref (target_list);
  
  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  site->target_list = target_list;
}

/**
 * gtk_drag_dest_add_text_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Add the text targets supported by #GtkSelectionData to
 * the target list of the drag destination. The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_text_targets() and
 * gtk_drag_dest_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_dest_add_text_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_add_image_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Add the image targets supported by #GtkSelectionData to
 * the target list of the drag destination. The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_image_targets() and
 * gtk_drag_dest_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_dest_add_image_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, FALSE);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Add the URI targets supported by #GtkSelectionData to
 * the target list of the drag destination. The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_uri_targets() and
 * gtk_drag_dest_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_dest_add_uri_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_set_track_motion: (method)
 * @widget: a #GtkWidget that’s a drag destination
 * @track_motion: whether to accept all targets
 *
 * Tells the widget to emit #GtkWidget::drag-motion and
 * #GtkWidget::drag-leave events regardless of the targets and the
 * %GTK_DEST_DEFAULT_MOTION flag.
 *
 * This may be used when a widget wants to do generic
 * actions regardless of the targets that the source offers.
 *
 * Since: 2.10
 */
void
gtk_drag_dest_set_track_motion (GtkWidget *widget,
                                gboolean   track_motion)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  
  g_return_if_fail (site != NULL);

  site->track_motion = track_motion != FALSE;
}

/**
 * gtk_drag_dest_get_track_motion: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Returns whether the widget has been configured to always
 * emit #GtkWidget::drag-motion signals.
 *
 * Returns: %TRUE if the widget always emits
 *   #GtkWidget::drag-motion events
 *
 * Since: 2.10
 */
gboolean
gtk_drag_dest_get_track_motion (GtkWidget *widget)
{
  GtkDragDestSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");

  if (site)
    return site->track_motion;

  return FALSE;
}

/*
 * _gtk_drag_dest_handle_event:
 * @toplevel: Toplevel widget that received the event
 * @event: the event to handle
 *
 * Called from widget event handling code on Drag events
 * for destinations.
 */
void
_gtk_drag_dest_handle_event (GtkWidget *toplevel,
                             GdkEvent  *event)
{
  GtkDragDestInfo *info;
  GdkDragContext *context;

  g_return_if_fail (toplevel != NULL);
  g_return_if_fail (event != NULL);

  context = event->dnd.context;

  info = gtk_drag_get_dest_info (context, TRUE);

  /* Find the widget for the event */
  switch (event->type)
    {
    case GDK_DRAG_ENTER:
      break;
      
    case GDK_DRAG_LEAVE:
      if (info->widget)
        {
          gtk_drag_dest_leave (info->widget, context, event->dnd.time);
          info->widget = NULL;
        }
      break;
      
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
        GdkWindow *window;
        gint tx, ty;
        gboolean found;

        if (event->type == GDK_DROP_START)
          {
            info->dropped = TRUE;
            /* We send a leave here so that the widget unhighlights
             * properly.
             */
            if (info->widget)
              {
                gtk_drag_dest_leave (info->widget, context, event->dnd.time);
                info->widget = NULL;
              }
          }

        window = gtk_widget_get_window (toplevel);

#ifdef GDK_WINDOWING_X11
        /* Hackaround for: http://bugzilla.gnome.org/show_bug.cgi?id=136112
         *
         * Currently gdk_window_get_position doesn't provide reliable
         * information for embedded windows, so we call the much more
         * expensive gdk_window_get_origin().
         */
        if (GTK_IS_PLUG (toplevel))
          gdk_window_get_origin (window, &tx, &ty);
        else
#endif /* GDK_WINDOWING_X11 */
          gdk_window_get_position (window, &tx, &ty);

        found = gtk_drag_find_widget (toplevel,
                                      context,
                                      info,
                                      event->dnd.x_root - tx,
                                      event->dnd.y_root - ty,
                                      event->dnd.time,
                                      (event->type == GDK_DRAG_MOTION) ?
                                      gtk_drag_dest_motion :
                                      gtk_drag_dest_drop);

        if (info->widget && !found)
          {
            gtk_drag_dest_leave (info->widget, context, event->dnd.time);
            info->widget = NULL;
          }
        
        /* Send a reply.
         */
        if (event->type == GDK_DRAG_MOTION)
          {
            if (!found)
              gdk_drag_status (context, 0, event->dnd.time);
          }
        else if (event->type == GDK_DROP_START && !info->proxy_source)
          {
            gdk_drop_reply (context, found, event->dnd.time);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * gtk_drag_dest_find_target: (method)
 * @widget: drag destination widget
 * @context: drag context
 * @target_list: (allow-none): list of droppable targets, or %NULL to use
 *    gtk_drag_dest_get_target_list (@widget).
 *
 * Looks for a match between the supported targets of @context and the
 * @dest_target_list, returning the first matching target, otherwise
 * returning %GDK_NONE. @dest_target_list should usually be the return
 * value from gtk_drag_dest_get_target_list(), but some widgets may
 * have different valid targets for different parts of the widget; in
 * that case, they will have to implement a drag_motion handler that
 * passes the correct target list to this function.
 *
 * Returns: (transfer none): first target that the source offers
 *     and the dest can accept, or %GDK_NONE
 */
GdkAtom
gtk_drag_dest_find_target (GtkWidget      *widget,
                           GdkDragContext *context,
                           GtkTargetList  *target_list)
{
  GList *tmp_target;
  GList *tmp_source = NULL;
  GtkWidget *source_widget;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GDK_NONE);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_NONE);


  source_widget = gtk_drag_get_source_widget (context);

  if (target_list == NULL)
    target_list = gtk_drag_dest_get_target_list (widget);
  
  if (target_list == NULL)
    return GDK_NONE;
  
  tmp_target = target_list->list;
  while (tmp_target)
    {
      GtkTargetPair *pair = tmp_target->data;
      tmp_source = gdk_drag_context_list_targets (context);
      while (tmp_source)
        {
          if (tmp_source->data == GUINT_TO_POINTER (pair->target))
            {
              if ((!(pair->flags & GTK_TARGET_SAME_APP) || source_widget) &&
                  (!(pair->flags & GTK_TARGET_SAME_WIDGET) || (source_widget == widget)) &&
                  (!(pair->flags & GTK_TARGET_OTHER_APP) || !source_widget) &&
                  (!(pair->flags & GTK_TARGET_OTHER_WIDGET) || (source_widget != widget)))
                return pair->target;
              else
                break;
            }
          tmp_source = tmp_source->next;
        }
      tmp_target = tmp_target->next;
    }

  return GDK_NONE;
}

static void
gtk_drag_selection_received (GtkWidget        *widget,
                             GtkSelectionData *selection_data,
                             guint             time,
                             gpointer          data)
{
  GdkDragContext *context;
  GtkDragDestInfo *info;
  GtkWidget *drop_widget;
  GdkAtom target;

  drop_widget = data;

  context = g_object_get_data (G_OBJECT (widget), "drag-context");
  info = gtk_drag_get_dest_info (context, FALSE);

  if (info->proxy_data && 
      gtk_selection_data_get_target (info->proxy_data) == gtk_selection_data_get_target (selection_data))
    {
      gtk_selection_data_set (info->proxy_data,
                              gtk_selection_data_get_data_type (selection_data),
                              gtk_selection_data_get_format (selection_data),
                              gtk_selection_data_get_data (selection_data),
                              gtk_selection_data_get_length (selection_data));
      gtk_main_quit ();
      return;
    }

  target = gtk_selection_data_get_target (selection_data);
  if (target == gdk_atom_intern_static_string ("DELETE"))
    {
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else if ((target == gdk_atom_intern_static_string ("XmTRANSFER_SUCCESS")) ||
           (target == gdk_atom_intern_static_string ("XmTRANSFER_FAILURE")))
    {
      /* Do nothing */
    }
  else
    {
      GtkDragDestSite *site;

      site = g_object_get_data (G_OBJECT (drop_widget), "gtk-drag-dest");

      if (site && site->target_list)
        {
          guint target_info;

          if (gtk_target_list_find (site->target_list, 
                                    target,
                                    &target_info))
            {
              if (!(site->flags & GTK_DEST_DEFAULT_DROP) ||
                  gtk_selection_data_get_length (selection_data) >= 0)
                g_signal_emit_by_name (drop_widget,
                                       "drag-data-received",
                                       context, info->drop_x, info->drop_y,
                                       selection_data,
                                       target_info, time);
            }
        }
      else
        {
          g_signal_emit_by_name (drop_widget,
                                 "drag-data-received",
                                 context, info->drop_x, info->drop_y,
                                 selection_data,
                                 0, time);
        }
      
      if (site && site->flags & GTK_DEST_DEFAULT_DROP)
        {

          gtk_drag_finish (context, 
                           (gtk_selection_data_get_length (selection_data) >= 0),
                           (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
                           time);
        }
      
      g_object_unref (drop_widget);
    }

  g_signal_handlers_disconnect_by_func (widget,
                                        gtk_drag_selection_received,
                                        data);
  
  g_object_set_data (G_OBJECT (widget), I_("drag-context"), NULL);
  g_object_unref (context);

  gtk_drag_release_ipc_widget (widget);
}

static gboolean
gtk_drag_find_widget (GtkWidget           *widget,
                      GdkDragContext      *context,
                      GtkDragDestInfo     *info,
                      gint                 x,
                      gint                 y,
                      guint32              time,
                      GtkDragDestCallback  callback)
{
  if (!gtk_widget_get_mapped (widget) ||
      !gtk_widget_get_sensitive (widget))
    return FALSE;

  /* Get the widget at the pointer coordinates and travel up
   * the widget hierarchy from there.
   */
  widget = _gtk_widget_find_at_coords (gtk_widget_get_window (widget),
                                       x, y, &x, &y);
  if (!widget)
    return FALSE;

  while (widget)
    {
      GtkWidget *parent;
      GList *hierarchy = NULL;
      gboolean found = FALSE;

      if (!gtk_widget_get_mapped (widget) ||
          !gtk_widget_get_sensitive (widget))
        return FALSE;

      /* need to reference the entire hierarchy temporarily in case the
       * ::drag-motion/::drag-drop callbacks change the widget hierarchy.
       */
      for (parent = widget;
           parent;
           parent = gtk_widget_get_parent (parent))
        {
          hierarchy = g_list_prepend (hierarchy, g_object_ref (parent));
        }

      /* If the current widget is registered as a drop site, check to
       * emit "drag-motion" to check if we are actually in a drop
       * site.
       */
      if (g_object_get_data (G_OBJECT (widget), "gtk-drag-dest"))
        {
          found = callback (widget, context, x, y, time);

          /* If so, send a "drag-leave" to the last widget */
          if (found)
            {
              if (info->widget && info->widget != widget)
                {
                  gtk_drag_dest_leave (info->widget, context, time);
                }

              info->widget = widget;
            }
        }

      if (!found)
        {
          /* Get the parent before unreffing the hierarchy because
           * invoking the callback might have destroyed the widget
           */
          parent = gtk_widget_get_parent (widget);

          /* The parent might be going away when unreffing the
           * hierarchy, so also protect againt that
           */
          if (parent)
            g_object_add_weak_pointer (G_OBJECT (parent), (gpointer *) &parent);
        }

      g_list_free_full (hierarchy, g_object_unref);

      if (found)
        return TRUE;

      if (parent)
        g_object_remove_weak_pointer (G_OBJECT (parent), (gpointer *) &parent);
      else
        return FALSE;

      if (!gtk_widget_translate_coordinates (widget, parent, x, y, &x, &y))
        return FALSE;

      widget = parent;
    }

  return FALSE;
}

static void
gtk_drag_proxy_begin (GtkWidget       *widget,
                      GtkDragDestInfo *dest_info,
                      guint32          time)
{
  GtkDragSourceInfo *source_info;
  GList *tmp_list;
  GdkDragContext *context;
  GtkWidget *ipc_widget;

  if (dest_info->proxy_source)
    {
      gdk_drag_abort (dest_info->proxy_source->context, time);
      gtk_drag_source_info_destroy (dest_info->proxy_source);
      dest_info->proxy_source = NULL;
    }
  
  ipc_widget = gtk_drag_get_ipc_widget (widget);
  context = gdk_drag_begin (gtk_widget_get_window (ipc_widget),
                            gdk_drag_context_list_targets (dest_info->context));

  source_info = gtk_drag_get_source_info (context, TRUE);

  source_info->ipc_widget = ipc_widget;
  source_info->widget = g_object_ref (widget);

  source_info->target_list = gtk_target_list_new (NULL, 0);
  tmp_list = gdk_drag_context_list_targets (dest_info->context);
  while (tmp_list)
    {
      gtk_target_list_add (source_info->target_list,
                           GDK_POINTER_TO_ATOM (tmp_list->data), 0, 0);
      tmp_list = tmp_list->next;
    }

  source_info->proxy_dest = dest_info;
  
  g_signal_connect (ipc_widget,
                    "selection-get",
                    G_CALLBACK (gtk_drag_selection_get),
                    source_info);
  
  dest_info->proxy_source = source_info;
}

static void
gtk_drag_dest_info_destroy (gpointer data)
{
  g_slice_free (GtkDragDestInfo, data);
}

static GtkDragDestInfo *
gtk_drag_get_dest_info (GdkDragContext *context,
                        gboolean        create)
{
  GtkDragDestInfo *info;
  static GQuark info_quark = 0;
  if (!info_quark)
    info_quark = g_quark_from_static_string ("gtk-dest-info");
  
  info = g_object_get_qdata (G_OBJECT (context), info_quark);
  if (!info && create)
    {
      info = g_slice_new0 (GtkDragDestInfo);
      info->context = context;
      g_object_set_qdata_full (G_OBJECT (context), info_quark,
                               info, gtk_drag_dest_info_destroy);
    }

  return info;
}

static GQuark dest_info_quark = 0;

static GtkDragSourceInfo *
gtk_drag_get_source_info (GdkDragContext *context,
                          gboolean        create)
{
  GtkDragSourceInfo *info;
  if (!dest_info_quark)
    dest_info_quark = g_quark_from_static_string ("gtk-source-info");
  
  info = g_object_get_qdata (G_OBJECT (context), dest_info_quark);
  if (!info && create)
    {
      info = g_new0 (GtkDragSourceInfo, 1);
      info->context = context;
      g_object_set_qdata (G_OBJECT (context), dest_info_quark, info);
    }

  return info;
}

static void
gtk_drag_clear_source_info (GdkDragContext *context)
{
  g_object_set_qdata (G_OBJECT (context), dest_info_quark, NULL);
}

static void
gtk_drag_dest_realized (GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (gtk_widget_is_toplevel (toplevel))
    gdk_window_register_dnd (gtk_widget_get_window (toplevel));
}

static void
gtk_drag_dest_hierarchy_changed (GtkWidget *widget,
                                 GtkWidget *previous_toplevel)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (gtk_widget_is_toplevel (toplevel) && gtk_widget_get_realized (toplevel))
    gdk_window_register_dnd (gtk_widget_get_window (toplevel));
}

static void
gtk_drag_dest_site_destroy (gpointer data)
{
  GtkDragDestSite *site = data;

  if (site->proxy_window)
    g_object_unref (site->proxy_window);
    
  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  g_slice_free (GtkDragDestSite, site);
}

/*
 * Default drag handlers
 */
static void  
gtk_drag_dest_leave (GtkWidget      *widget,
                     GdkDragContext *context,
                     guint           time)
{
  GtkDragDestSite *site;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_if_fail (site != NULL);

  if (site->do_proxy)
    {
      GtkDragDestInfo *info = gtk_drag_get_dest_info (context, FALSE);

      if (info->proxy_source && info->proxy_source->widget == widget && !info->dropped)
        {
          gdk_drag_abort (info->proxy_source->context, time);
          gtk_drag_source_info_destroy (info->proxy_source);
          info->proxy_source = NULL;
        }
      
      return;
    }
  else
    {
      if ((site->flags & GTK_DEST_DEFAULT_HIGHLIGHT) && site->have_drag)
        gtk_drag_unhighlight (widget);

      if (!(site->flags & GTK_DEST_DEFAULT_MOTION) || site->have_drag ||
          site->track_motion)
        g_signal_emit_by_name (widget, "drag-leave", context, time);
      
      site->have_drag = FALSE;
    }
}

static gboolean
gtk_drag_dest_motion (GtkWidget      *widget,
                      GdkDragContext *context,
                      gint            x,
                      gint            y,
                      guint           time)
{
  GtkDragDestSite *site;
  GdkDragAction action = 0;
  gboolean retval;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  if (site->do_proxy)
    {
      GdkAtom selection;
      GdkEvent *current_event;
      GdkWindow *dest_window;
      GdkDragProtocol proto;
        
      GtkDragDestInfo *info = gtk_drag_get_dest_info (context, FALSE);

      if (!info->proxy_source || info->proxy_source->widget != widget)
        gtk_drag_proxy_begin (widget, info, time);

      current_event = gtk_get_current_event ();

      if (site->proxy_window)
        {
          dest_window = site->proxy_window;
          proto = site->proxy_protocol;
        }
      else
        {
          gdk_drag_find_window_for_screen (info->proxy_source->context,
                                           NULL,
                                           gdk_window_get_screen (current_event->dnd.window),
                                           current_event->dnd.x_root, 
                                           current_event->dnd.y_root,
                                           &dest_window, &proto);
        }
      
      gdk_drag_motion (info->proxy_source->context, 
                       dest_window, proto,
                       current_event->dnd.x_root, 
                       current_event->dnd.y_root, 
                       gdk_drag_context_get_suggested_action (context),
                       gdk_drag_context_get_actions (context),
                       time);

      if (!site->proxy_window && dest_window)
        g_object_unref (dest_window);

      selection = gdk_drag_get_selection (info->proxy_source->context);
      if (selection && 
          selection != gdk_drag_get_selection (info->context))
        gtk_drag_source_check_selection (info->proxy_source, selection, time);

      gdk_event_free (current_event);
      
      return TRUE;
    }

  if (site->track_motion || site->flags & GTK_DEST_DEFAULT_MOTION)
    {
      if (gdk_drag_context_get_suggested_action (context) & site->actions)
        action = gdk_drag_context_get_suggested_action (context);
      else
        {
          gint i;
          
          for (i = 0; i < 8; i++)
            {
              if ((site->actions & (1 << i)) &&
                  (gdk_drag_context_get_actions (context) & (1 << i)))
                {
                  action = (1 << i);
                  break;
                }
            }
        }

      if (action && gtk_drag_dest_find_target (widget, context, NULL))
        {
          if (!site->have_drag)
            {
              site->have_drag = TRUE;
              if (site->flags & GTK_DEST_DEFAULT_HIGHLIGHT)
                gtk_drag_highlight (widget);
            }

          gdk_drag_status (context, action, time);
        }
      else
        {
          gdk_drag_status (context, 0, time);
          if (!site->track_motion)
            return TRUE;
        }
    }

  g_signal_emit_by_name (widget, "drag-motion",
                         context, x, y, time, &retval);

  return (site->flags & GTK_DEST_DEFAULT_MOTION) ? TRUE : retval;
}

static gboolean
gtk_drag_dest_drop (GtkWidget      *widget,
                    GdkDragContext *context,
                    gint            x,
                    gint            y,
                    guint           time)
{
  GtkDragDestSite *site;
  GtkDragDestInfo *info;

  site = g_object_get_data (G_OBJECT (widget), "gtk-drag-dest");
  g_return_val_if_fail (site != NULL, FALSE);

  info = gtk_drag_get_dest_info (context, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  info->drop_x = x;
  info->drop_y = y;

  if (site->do_proxy)
    {
      if (info->proxy_source || 
          (gdk_drag_context_get_protocol (info->context) == GDK_DRAG_PROTO_ROOTWIN))
        {
          gtk_drag_drop (info->proxy_source, time);
        }
      else
        {
          /* We need to synthesize a motion event, wait for a status,
           * and, if we get a good one, do a drop.
           */
          
          GdkEvent *current_event;
          GdkAtom selection;
          GdkWindow *dest_window;
          GdkDragProtocol proto;
          
          gtk_drag_proxy_begin (widget, info, time);
          info->proxy_drop_wait = TRUE;
          info->proxy_drop_time = time;
          
          current_event = gtk_get_current_event ();

          if (site->proxy_window)
            {
              dest_window = site->proxy_window;
              proto = site->proxy_protocol;
            }
          else
            {
              gdk_drag_find_window_for_screen (info->proxy_source->context,
                                               NULL,
                                               gdk_window_get_screen (current_event->dnd.window),
                                               current_event->dnd.x_root, 
                                               current_event->dnd.y_root,
                                               &dest_window, &proto);
            }

          gdk_drag_motion (info->proxy_source->context, 
                           dest_window, proto,
                           current_event->dnd.x_root, 
                           current_event->dnd.y_root, 
                           gdk_drag_context_get_suggested_action (context),
                           gdk_drag_context_get_actions (context),
                           time);

          if (!site->proxy_window && dest_window)
            g_object_unref (dest_window);

          selection = gdk_drag_get_selection (info->proxy_source->context);
          if (selection && 
              selection != gdk_drag_get_selection (info->context))
            gtk_drag_source_check_selection (info->proxy_source, selection, time);

          gdk_event_free (current_event);
        }

      return TRUE;
    }
  else
    {
      gboolean retval;

      if (site->flags & GTK_DEST_DEFAULT_DROP)
        {
          GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

          if (target == GDK_NONE)
            {
              gtk_drag_finish (context, FALSE, FALSE, time);
              return TRUE;
            }
          else 
            gtk_drag_get_data (widget, context, target, time);
        }

      g_signal_emit_by_name (widget, "drag-drop",
                             context, x, y, time, &retval);

      return (site->flags & GTK_DEST_DEFAULT_DROP) ? TRUE : retval;
    }
}

/***************
 * Source side *
 ***************/

static GtkIconHelper *
gtk_drag_source_site_get_icon_helper (GtkDragSourceSite *site)
{
  GtkIconHelper *helper;

  if (site)
    helper = g_object_ref (site->icon_helper);
  else
    helper = _gtk_icon_helper_new ();

  if (_gtk_icon_helper_get_is_empty (helper))
    _gtk_icon_helper_set_icon_name (helper, "text-x-generic", GTK_ICON_SIZE_DND);

  return helper;
}

/* Like gtk_drag_begin(), but also takes a GtkDragSourceSite,
 * so that we can set the icon from the source site information
 */
static GdkDragContext *
gtk_drag_begin_internal (GtkWidget         *widget,
                         GtkDragSourceSite *site,
                         GtkTargetList     *target_list,
                         GdkDragAction      actions,
                         gint               button,
                         const GdkEvent    *event,
                         int                x,
                         int                y)
{
  GtkDragSourceInfo *info;
  GList *targets = NULL;
  GList *tmp_list;
  guint32 time = GDK_CURRENT_TIME;
  GdkDragAction possible_actions, suggested_action;
  GdkDragContext *context;
  GtkWidget *ipc_widget;
  GdkCursor *cursor;
  GdkDevice *pointer, *keyboard;
  GdkWindow *ipc_window;

  pointer = keyboard = NULL;
  ipc_widget = gtk_drag_get_ipc_widget (widget);
  
  gtk_drag_get_event_actions (event, button, actions,
                              &suggested_action, &possible_actions);
  
  cursor = gtk_drag_get_cursor (widget,
                                gtk_widget_get_display (widget), 
                                suggested_action,
                                NULL);
  
  if (event)
    {
      time = gdk_event_get_time (event);
      if (time == GDK_CURRENT_TIME)
        time = gtk_get_current_event_time ();

      pointer = gdk_event_get_device (event);

      if (gdk_device_get_source (pointer) == GDK_SOURCE_KEYBOARD)
        {
          keyboard = pointer;
          pointer = gdk_device_get_associated_device (keyboard);
        }
      else
        keyboard = gdk_device_get_associated_device (pointer);
    }
  else
    {
      GdkDeviceManager *device_manager;

      device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
      pointer = gdk_device_manager_get_client_pointer (device_manager);
      keyboard = gdk_device_get_associated_device (pointer);
    }

  if (!pointer)
    return NULL;

  ipc_window = gtk_widget_get_window (ipc_widget);

  if (gdk_device_grab (pointer, ipc_window,
                       GDK_OWNERSHIP_APPLICATION, FALSE,
                       GDK_POINTER_MOTION_MASK |
                       GDK_BUTTON_RELEASE_MASK,
                       cursor, time) != GDK_GRAB_SUCCESS)
    {
      gtk_drag_release_ipc_widget (ipc_widget);
      return NULL;
    }

  if (keyboard)
    grab_dnd_keys (ipc_widget, keyboard, time);

  /* We use a GTK grab here to override any grabs that the widget
   * we are dragging from might have held
   */
  gtk_device_grab_add (ipc_widget, pointer, FALSE);

  tmp_list = g_list_last (target_list->list);
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;
      targets = g_list_prepend (targets, 
                                GINT_TO_POINTER (pair->target));
      tmp_list = tmp_list->prev;
    }

  source_widgets = g_slist_prepend (source_widgets, ipc_widget);

  context = gdk_drag_begin (ipc_window, targets);
  gdk_drag_context_set_device (context, pointer);
  g_list_free (targets);
  
  info = gtk_drag_get_source_info (context, TRUE);
  
  info->ipc_widget = ipc_widget;
  g_object_set_data (G_OBJECT (info->ipc_widget), I_("gtk-info"), info);

  info->widget = g_object_ref (widget);
  
  info->button = button;
  info->cursor = cursor;
  info->target_list = target_list;
  gtk_target_list_ref (target_list);

  info->possible_actions = actions;

  info->status = GTK_DRAG_STATUS_DRAG;
  info->last_event = NULL;
  info->selections = NULL;
  info->icon_window = NULL;
  info->destroy_icon = FALSE;

  /* Set cur_x, cur_y here so if the "drag-begin" signal shows
   * the drag icon, it will be in the right place
   */
  if (event)
    info->cur_screen = gdk_event_get_screen (event);
  else
    gdk_device_get_position (pointer, &info->cur_screen, NULL, NULL);

  if (x != -1 && y != -1)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
      gtk_widget_translate_coordinates (widget, toplevel,
                                        x, y, &x, &y);
      gdk_window_get_root_coords (gtk_widget_get_window (toplevel),
                                  x, y, &info->start_x, &info->start_y);
    }
  else if (event && event->type == GDK_MOTION_NOTIFY)
    {
      info->start_x = event->motion.x_root;
      info->start_y = event->motion.y_root;
    }
  else
    gdk_device_get_position (pointer, NULL, &info->start_x, &info->start_y);


  g_signal_emit_by_name (widget, "drag-begin", info->context);

  /* Ensure that we have an icon before we start the drag; the
   * application may have set one in ::drag_begin, or it may
   * not have set one.
   */
  if (!info->icon_window && !info->icon_helper)
    info->icon_helper = gtk_drag_source_site_get_icon_helper (site);

  /* We need to composite the icon into the cursor, if we are
   * not using an icon window.
   */
  if (info->icon_helper)
    {
      cursor = gtk_drag_get_cursor (widget,
                                    gtk_widget_get_display (widget), 
                                    suggested_action,
                                    info);
  
      if (cursor != info->cursor)
        {
          gdk_device_grab (pointer, gtk_widget_get_window (widget),
                           GDK_OWNERSHIP_APPLICATION, FALSE,
                           GDK_POINTER_MOTION_MASK |
                           GDK_BUTTON_RELEASE_MASK,
                           cursor, time);
          info->cursor = cursor;
        }
    }

  info->cur_x = info->start_x;
  info->cur_y = info->start_y;

  if (event && event->type == GDK_MOTION_NOTIFY)
    gtk_drag_motion_cb (info->ipc_widget, (GdkEventMotion *)event, info);
  else
    gtk_drag_update (info, info->cur_screen, info->cur_x, info->cur_y, event);

  g_signal_connect (info->ipc_widget, "grab-broken-event",
                    G_CALLBACK (gtk_drag_grab_broken_event_cb), info);
  g_signal_connect (info->ipc_widget, "grab-notify",
                    G_CALLBACK (gtk_drag_grab_notify_cb), info);
  g_signal_connect (info->ipc_widget, "button-release-event",
                    G_CALLBACK (gtk_drag_button_release_cb), info);
  g_signal_connect (info->ipc_widget, "motion-notify-event",
                    G_CALLBACK (gtk_drag_motion_cb), info);
  g_signal_connect (info->ipc_widget, "key-press-event",
                    G_CALLBACK (gtk_drag_key_cb), info);
  g_signal_connect (info->ipc_widget, "key-release-event",
                    G_CALLBACK (gtk_drag_key_cb), info);
  g_signal_connect (info->ipc_widget, "selection-get",
                    G_CALLBACK (gtk_drag_selection_get), info);

  info->have_grab = TRUE;
  info->grab_time = time;

  return info->context;
}

/**
 * gtk_drag_begin_with_coordinates: (method)
 * @widget: the source widget
 * @targets: The targets (data formats) in which the
 *    source can provide the data
 * @actions: A bitmask of the allowed drag actions for this drag
 * @button: The button the user clicked to start the drag
 * @event: The event that triggered the start of the drag
 * @x: The initial x coordinate to start dragging from, in the coordinate space
 *    of @widget. If -1 is passed, the coordinates are retrieved from @event or
 *    the current pointer position
 * @y: The initial y coordinate to start dragging from, in the coordinate space
 *    of @widget. If -1 is passed, the coordinates are retrieved from @event or
 *    the current pointer position
 *
 * Initiates a drag on the source side. The function only needs to be used
 * when the application is starting drags itself, and is not needed when
 * gtk_drag_source_set() is used.
 *
 * The @event is used to retrieve the timestamp that will be used internally to
 * grab the pointer.  If @event is %NULL, then %GDK_CURRENT_TIME will be used.
 * However, you should try to pass a real event in all cases, since that can be
 * used to get information about the drag.
 *
 * Generally there are three cases when you want to start a drag by hand by
 * calling this function:
 *
 * 1. During a #GtkWidget::button-press-event handler, if you want to start a drag
 * immediately when the user presses the mouse button.  Pass the @event
 * that you have in your #GtkWidget::button-press-event handler.
 *
 * 2. During a #GtkWidget::motion-notify-event handler, if you want to start a drag
 * when the mouse moves past a certain threshold distance after a button-press.
 * Pass the @event that you have in your #GtkWidget::motion-notify-event handler.
 *
 * 3. During a timeout handler, if you want to start a drag after the mouse
 * button is held down for some time.  Try to save the last event that you got
 * from the mouse, using gdk_event_copy(), and pass it to this function
 * (remember to free the event with gdk_event_free() when you are done).
 * If you can really not pass a real event, pass #NULL instead.
 *
 * Returns: (transfer none): the context for this drag
 *
 * Since: 3.10
 */
GdkDragContext *
gtk_drag_begin_with_coordinates (GtkWidget     *widget,
                                 GtkTargetList *targets,
                                 GdkDragAction  actions,
                                 gint           button,
                                 GdkEvent      *event,
                                 gint           x,
                                 gint           y)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_get_realized (widget), NULL);
  g_return_val_if_fail (targets != NULL, NULL);

  return gtk_drag_begin_internal (widget, NULL, targets,
                                  actions, button, event, x, y);
}

/**
 * gtk_drag_begin: (method)
 * @widget: the source widget
 * @targets: The targets (data formats) in which the
 *    source can provide the data
 * @actions: A bitmask of the allowed drag actions for this drag
 * @button: The button the user clicked to start the drag
 * @event: The event that triggered the start of the drag
 *
 * This function is equivalent to gtk_drag_begin_with_coordinates(),
 * passing -1, -1 as coordinates.
 *
 * Returns: (transfer none): the context for this drag
 *
 * Deprecated: 3.10: Use gtk_drag_begin_with_coordinates() instead
 */
GdkDragContext *
gtk_drag_begin (GtkWidget     *widget,
                GtkTargetList *targets,
                GdkDragAction  actions,
                gint           button,
                GdkEvent      *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_get_realized (widget), NULL);
  g_return_val_if_fail (targets != NULL, NULL);

  return gtk_drag_begin_internal (widget, NULL, targets,
                                  actions, button, event, -1, -1);
}

/**
 * gtk_drag_source_set: (method)
 * @widget: a #GtkWidget
 * @start_button_mask: the bitmask of buttons that can start the drag
 * @targets: (allow-none) (array length=n_targets): the table of targets
 *     that the drag will support, may be %NULL
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this widget
 *
 * Sets up a widget so that GTK+ will start a drag operation when the user
 * clicks and drags on the widget. The widget must have a window.
 */
void
gtk_drag_source_set (GtkWidget            *widget,
                     GdkModifierType       start_button_mask,
                     const GtkTargetEntry *targets,
                     gint                  n_targets,
                     GdkDragAction         actions)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  gtk_widget_add_events (widget,
                         gtk_widget_get_events (widget) |
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                         GDK_BUTTON_MOTION_MASK);

  if (site)
    {
      if (site->target_list)
        gtk_target_list_unref (site->target_list);
    }
  else
    {
      site = g_slice_new0 (GtkDragSourceSite);
      site->icon_helper = _gtk_icon_helper_new ();
      site->drag_gesture = gtk_gesture_drag_new (widget);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (site->drag_gesture),
                                                  GTK_PHASE_NONE);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (site->drag_gesture), 0);

      g_signal_connect (widget, "button-press-event",
                        G_CALLBACK (gtk_drag_source_event_cb),
                        site);
      g_signal_connect (widget, "button-release-event",
                        G_CALLBACK (gtk_drag_source_event_cb),
                        site);
      g_signal_connect (widget, "motion-notify-event",
                        G_CALLBACK (gtk_drag_source_event_cb),
                        site);
      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-site-data"), 
                              site, gtk_drag_source_site_destroy);
    }

  site->start_button_mask = start_button_mask;

  site->target_list = gtk_target_list_new (targets, n_targets);

  site->actions = actions;
}

/**
 * gtk_drag_source_unset: (method)
 * @widget: a #GtkWidget
 *
 * Undoes the effects of gtk_drag_source_set().
 */
void 
gtk_drag_source_unset (GtkWidget *widget)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  if (site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_source_event_cb,
                                            site);
      g_object_set_data (G_OBJECT (widget), I_("gtk-site-data"), NULL);
    }
}

/**
 * gtk_drag_source_get_target_list: (method)
 * @widget: a #GtkWidget
 *
 * Gets the list of targets this widget can provide for
 * drag-and-drop.
 *
 * Returns: (transfer none): the #GtkTargetList, or %NULL if none
 *
 * Since: 2.4
 */
GtkTargetList *
gtk_drag_source_get_target_list (GtkWidget *widget)
{
  GtkDragSourceSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  return site ? site->target_list : NULL;
}

/**
 * gtk_drag_source_set_target_list: (method)
 * @widget: a #GtkWidget that’s a drag source
 * @target_list: (allow-none): list of draggable targets, or %NULL for none
 *
 * Changes the target types that this widget offers for drag-and-drop.
 * The widget must first be made into a drag source with
 * gtk_drag_source_set().
 *
 * Since: 2.4
 */
void
gtk_drag_source_set_target_list (GtkWidget     *widget,
                                 GtkTargetList *target_list)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  if (site == NULL)
    {
      g_warning ("gtk_drag_source_set_target_list() requires the widget "
                 "to already be a drag source.");
      return;
    }

  if (target_list)
    gtk_target_list_ref (target_list);

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  site->target_list = target_list;
}

/**
 * gtk_drag_source_add_text_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the text targets supported by #GtkSelectionData to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_text_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_source_add_text_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_add_image_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the writable image targets supported by #GtkSelectionData to
 * the target list of the drag source. The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_image_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_source_add_image_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, TRUE);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the URI targets supported by #GtkSelectionData to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_uri_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_source_add_uri_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_set_icon_pixbuf: (method)
 * @widget: a #GtkWidget
 * @pixbuf: the #GdkPixbuf for the drag icon
 * 
 * Sets the icon that will be used for drags from a particular widget
 * from a #GdkPixbuf. GTK+ retains a reference for @pixbuf and will 
 * release it when it is no longer needed.
 */
void 
gtk_drag_source_set_icon_pixbuf (GtkWidget *widget,
                                 GdkPixbuf *pixbuf)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL); 
  g_object_ref (pixbuf);

  _gtk_icon_helper_set_pixbuf (site->icon_helper, pixbuf);
}

/**
 * gtk_drag_source_set_icon_stock: (method)
 * @widget: a #GtkWidget
 * @stock_id: the ID of the stock icon to use
 *
 * Sets the icon that will be used for drags from a particular source
 * to a stock icon.
 *
 * Deprecated: 3.10: Use gtk_drag_source_set_icon_name() instead.
 */
void
gtk_drag_source_set_icon_stock (GtkWidget   *widget,
                                const gchar *stock_id)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (stock_id != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  _gtk_icon_helper_set_stock_id (site->icon_helper, stock_id, GTK_ICON_SIZE_DND);
}

/**
 * gtk_drag_source_set_icon_name: (method)
 * @widget: a #GtkWidget
 * @icon_name: name of icon to use
 * 
 * Sets the icon that will be used for drags from a particular source
 * to a themed icon. See the docs for #GtkIconTheme for more details.
 *
 * Since: 2.8
 */
void 
gtk_drag_source_set_icon_name (GtkWidget   *widget,
                               const gchar *icon_name)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon_name != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  _gtk_icon_helper_set_icon_name (site->icon_helper, icon_name, GTK_ICON_SIZE_DND);
}

/**
 * gtk_drag_source_set_icon_gicon: (method)
 * @widget: a #GtkWidget
 * @icon: A #GIcon
 * 
 * Sets the icon that will be used for drags from a particular source
 * to @icon. See the docs for #GtkIconTheme for more details.
 *
 * Since: 3.2
 */
void
gtk_drag_source_set_icon_gicon (GtkWidget *widget,
                                GIcon     *icon)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon != NULL);
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  _gtk_icon_helper_set_gicon (site->icon_helper, icon, GTK_ICON_SIZE_DND);
}

static void
gtk_drag_get_icon (GtkDragSourceInfo  *info,
                   GtkWidget         **icon_window,
                   gint               *hot_x,
                   gint               *hot_y)
{
  if (get_can_change_screen (info->icon_window))
    gtk_window_set_screen (GTK_WINDOW (info->icon_window),
                           info->cur_screen);
      
  if (gtk_widget_get_screen (info->icon_window) != info->cur_screen)
    {
      if (!info->fallback_icon)
        {
          gint save_hot_x, save_hot_y;
          gboolean save_destroy_icon;
          GtkWidget *save_icon_window;
          GtkIconHelper *helper;
          
          /* HACK to get the appropriate icon
           */
          save_icon_window = info->icon_window;
          save_hot_x = info->hot_x;
          save_hot_y = info->hot_x;
          save_destroy_icon = info->destroy_icon;

          info->icon_window = NULL;

          helper = _gtk_icon_helper_new ();
          _gtk_icon_helper_set_icon_name (helper, "text-x-generic", GTK_ICON_SIZE_DND);
          set_icon_helper (info->context, helper, -2, -2, TRUE);
          info->fallback_icon = info->icon_window;
          
          info->icon_window = save_icon_window;
          info->hot_x = save_hot_x;
          info->hot_y = save_hot_y;
          info->destroy_icon = save_destroy_icon;

          g_object_unref (helper);
        }
      
      gtk_widget_hide (info->icon_window);
      
      *icon_window = info->fallback_icon;
      gtk_window_set_screen (GTK_WINDOW (*icon_window), info->cur_screen);
      
      *hot_x = -2;
      *hot_y = -2;
    }
  else
    {
      if (info->fallback_icon)
        gtk_widget_hide (info->fallback_icon);
      
      *icon_window = info->icon_window;
      *hot_x = info->hot_x;
      *hot_y = info->hot_y;
    }
}

static void
gtk_drag_update_icon (GtkDragSourceInfo *info)
{
  if (info->icon_window)
    {
      GtkWidget *icon_window;
      gint hot_x, hot_y;
  
      gtk_drag_get_icon (info, &icon_window, &hot_x, &hot_y);
      
      gtk_window_move (GTK_WINDOW (icon_window), 
                       info->cur_x - hot_x, 
                       info->cur_y - hot_y);

      if (gtk_widget_get_visible (icon_window))
        gdk_window_raise (gtk_widget_get_window (icon_window));
      else
        gtk_widget_show (icon_window);
    }
}

static void 
gtk_drag_set_icon_window (GdkDragContext *context,
                          GtkWidget      *widget,
                          gint            hot_x,
                          gint            hot_y,
                          gboolean        destroy_on_release)
{
  GtkDragSourceInfo *info;

  info = gtk_drag_get_source_info (context, FALSE);
  if (info == NULL)
    {
      if (destroy_on_release)
        gtk_widget_destroy (widget);
      return;
    }

  gtk_drag_remove_icon (info);

  if (widget)
    g_object_ref (widget);  
  
  info->icon_window = widget;
  info->hot_x = hot_x;
  info->hot_y = hot_y;
  info->destroy_icon = destroy_on_release;

  if (widget && info->icon_helper)
    g_clear_object (&info->icon_helper);

  gtk_drag_update_cursor (info);
  gtk_drag_update_icon (info);
}

/**
 * gtk_drag_set_icon_widget: (method)
 * @context: the context for a drag. (This must be called 
          with a  context for the source side of a drag)
 * @widget: a toplevel window to use as an icon
 * @hot_x: the X offset within @widget of the hotspot
 * @hot_y: the Y offset within @widget of the hotspot
 * 
 * Changes the icon for a widget to a given widget.
 * GTK+ will not destroy the icon, so if you don’t want
 * it to persist, you should connect to the “drag-end” 
 * signal and destroy it yourself.
 *
 * GTK+ will, however, change the opacity and position of
 * the window as part of the drag animation. If you want
 * to reuse the window, you have to restore these to
 * the values you need after each drag operation.
 */
void 
gtk_drag_set_icon_widget (GdkDragContext *context,
                          GtkWidget      *widget,
                          gint            hot_x,
                          gint            hot_y)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_drag_set_icon_window (context, widget, hot_x, hot_y, FALSE);
}

static void
icon_window_realize (GtkWidget     *window,
                     GtkIconHelper *helper)
{
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_t *cr;
  GdkPixbuf *pixbuf;

  pixbuf = _gtk_icon_helper_ensure_pixbuf (helper, gtk_widget_get_style_context (window));
  surface = gdk_window_create_similar_surface (gtk_widget_get_window (window),
                                               CAIRO_CONTENT_COLOR,
                                               gdk_pixbuf_get_width (pixbuf),
                                               gdk_pixbuf_get_height (pixbuf));

  cr = cairo_create (surface);
  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SATURATE);
  cairo_paint (cr);
  cairo_pop_group_to_source (cr);
  cairo_paint (cr);
  cairo_destroy (cr);

  pattern = cairo_pattern_create_for_surface (surface);
  gdk_window_set_background_pattern (gtk_widget_get_window (window), pattern);
  cairo_pattern_destroy (pattern);

  cairo_surface_destroy (surface);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    {
      cairo_region_t *region;

      surface = cairo_image_surface_create (CAIRO_FORMAT_A1,
                                            gdk_pixbuf_get_width (pixbuf),
                                            gdk_pixbuf_get_height (pixbuf));
      
      cr = cairo_create (surface);
      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      region = gdk_cairo_region_create_from_surface (surface);
      gtk_widget_shape_combine_region (window, region);
      cairo_region_destroy (region);

      cairo_surface_destroy (surface);
    }

  g_object_unref (pixbuf);
}

static void
set_icon_helper (GdkDragContext *context,
                 GtkIconHelper  *helper,
                 gint            hot_x,
                 gint            hot_y,
                 gboolean        force_window)
{
  GtkWidget *window;
  gint width, height;
  GdkScreen *screen;
  GdkDisplay *display;

  g_return_if_fail (context != NULL);
  g_return_if_fail (helper != NULL);

  screen = gdk_window_get_screen (gdk_drag_context_get_source_window (context));

  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DND);
  gtk_window_set_screen (GTK_WINDOW (window), screen);
  set_can_change_screen (window, TRUE);

  gtk_widget_set_events (window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_widget_set_app_paintable (window, TRUE);

  display = gdk_window_get_display (gdk_drag_context_get_source_window (context));
  _gtk_icon_helper_get_size (helper, 
                             gtk_widget_get_style_context (window),
                             &width, &height);

  if (!force_window &&
      gtk_drag_can_use_rgba_cursor (display, width + 2, height + 2))
    {
      GtkDragSourceInfo *info;

      gtk_widget_destroy (window);

      info = gtk_drag_get_source_info (context, FALSE);

      if (info->icon_helper)
        g_object_unref (info->icon_helper);
      info->icon_helper = g_object_ref (helper);

      gtk_drag_set_icon_window (context, NULL, hot_x, hot_y, TRUE);
    }
  else
    {
      gtk_widget_set_size_request (window, width, height);

      g_signal_connect_closure (window, "realize",
                                g_cclosure_new (G_CALLBACK (icon_window_realize),
                                                g_object_ref (helper),
                                                (GClosureNotify)g_object_unref),
                                FALSE);
                    
      gtk_drag_set_icon_window (context, window, hot_x, hot_y, TRUE);
   }
}

/**
 * gtk_drag_set_icon_pixbuf: (method)
 * @context: the context for a drag (This must be called 
 *            with a  context for the source side of a drag)
 * @pixbuf: the #GdkPixbuf to use as the drag icon
 * @hot_x: the X offset within @widget of the hotspot
 * @hot_y: the Y offset within @widget of the hotspot
 * 
 * Sets @pixbuf as the icon for a given drag.
 */
void 
gtk_drag_set_icon_pixbuf (GdkDragContext *context,
                          GdkPixbuf      *pixbuf,
                          gint            hot_x,
                          gint            hot_y)
{
  GtkIconHelper *icon;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  icon = _gtk_icon_helper_new ();
  _gtk_icon_helper_set_pixbuf (icon, pixbuf);
  set_icon_helper (context, icon, hot_x, hot_y, FALSE);

  g_object_unref (icon);
}

/**
 * gtk_drag_set_icon_stock: (method)
 * @context: the context for a drag (This must be called 
 *            with a  context for the source side of a drag)
 * @stock_id: the ID of the stock icon to use for the drag
 * @hot_x: the X offset within the icon of the hotspot
 * @hot_y: the Y offset within the icon of the hotspot
 * 
 * Sets the icon for a given drag from a stock ID.
 *
 * Deprecated: 3.10: Use gtk_drag_set_icon_name() instead.
 */
void 
gtk_drag_set_icon_stock (GdkDragContext *context,
                         const gchar    *stock_id,
                         gint            hot_x,
                         gint            hot_y)
{
  GtkIconHelper *icon;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (stock_id != NULL);

  icon = _gtk_icon_helper_new ();
  _gtk_icon_helper_set_stock_id (icon, stock_id, GTK_ICON_SIZE_DND);
  set_icon_helper (context, icon, hot_x, hot_y, FALSE);

  g_object_unref (icon);
}

/* XXX: This function is in gdk, too. Should it be in Cairo? */
static gboolean
_gtk_cairo_surface_extents (cairo_surface_t *surface,
                            GdkRectangle    *extents)
{
  double x1, x2, y1, y2;
  cairo_t *cr;

  g_return_val_if_fail (surface != NULL, FALSE);
  g_return_val_if_fail (extents != NULL, FALSE);

  cr = cairo_create (surface);
  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

  x1 = floor (x1);
  y1 = floor (y1);
  x2 = ceil (x2);
  y2 = ceil (y2);
  x2 -= x1;
  y2 -= y1;
  
  if (x1 < G_MININT || x1 > G_MAXINT ||
      y1 < G_MININT || y1 > G_MAXINT ||
      x2 > G_MAXINT || y2 > G_MAXINT)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return FALSE;
    }

  extents->x = x1;
  extents->y = y1;
  extents->width = x2;
  extents->height = y2;

  return TRUE;
}

/**
 * gtk_drag_set_icon_surface: (method)
 * @context: the context for a drag (This must be called
 *     with a context for the source side of a drag)
 * @surface: the surface to use as icon
 *
 * Sets @surface as the icon for a given drag. GTK+ retains
 * references for the arguments, and will release them when
 * they are no longer needed.
 *
 * To position the surface relative to the mouse, use
 * cairo_surface_set_device_offset() on @surface. The mouse
 * cursor will be positioned at the (0,0) coordinate of the
 * surface.
 */
void 
gtk_drag_set_icon_surface (GdkDragContext  *context,
                           cairo_surface_t *surface)
{
  GtkWidget *window;
  GdkScreen *screen;
  GdkRectangle extents;
  cairo_pattern_t *pattern;
  GdkVisual *rgba_visual;
  gboolean has_rgba;
      
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (surface != NULL);

  _gtk_cairo_surface_extents (surface, &extents);


  screen = gdk_window_get_screen (gdk_drag_context_get_source_window (context));
  rgba_visual = gdk_screen_get_rgba_visual (screen);

  window = gtk_window_new (GTK_WINDOW_POPUP);
  has_rgba =
    rgba_visual != NULL &&
    gdk_screen_is_composited (screen);
  if (has_rgba)
    gtk_widget_set_visual (GTK_WIDGET (window), rgba_visual);

  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DND);
  gtk_window_set_screen (GTK_WINDOW (window), screen);
  set_can_change_screen (window, TRUE);

  gtk_widget_set_events (window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_widget_set_app_paintable (window, TRUE);

  gtk_widget_set_size_request (window, extents.width, extents.height);
  gtk_widget_realize (window);

  if (cairo_surface_get_content (surface) != CAIRO_CONTENT_COLOR &&
      !has_rgba)
    {
      cairo_surface_t *saturated;
      cairo_region_t *region;
      cairo_t *cr;

      region = gdk_cairo_region_create_from_surface (surface);
      cairo_region_translate (region, -extents.x, -extents.y);

      gtk_widget_shape_combine_region (window, region);
      cairo_region_destroy (region);

      /* Need to saturate the colors, so it doesn't look like semi-transparent
       * pixels were painted on black. */
      saturated = gdk_window_create_similar_surface (gtk_widget_get_window (window),
                                                     CAIRO_CONTENT_COLOR,
                                                     extents.width,
                                                     extents.height);
      
      cr = cairo_create (saturated);
      cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
      cairo_set_source_surface (cr, surface, -extents.x, -extents.y);
      cairo_paint (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_SATURATE);
      cairo_paint (cr);
      cairo_pop_group_to_source (cr);
      cairo_paint (cr);
      cairo_destroy (cr);
    
      pattern = cairo_pattern_create_for_surface (saturated);

      cairo_surface_destroy (saturated);
    }
  else
    {
      cairo_matrix_t matrix;

      pattern = cairo_pattern_create_for_surface (surface);
      cairo_matrix_init_translate (&matrix, extents.x, extents.y);
      cairo_pattern_set_matrix (pattern, &matrix);
    }

  gdk_window_set_background_pattern (gtk_widget_get_window (window), pattern);
  cairo_pattern_destroy (pattern);

  gtk_drag_set_icon_window (context, window, extents.x, extents.y, TRUE);
}

/**
 * gtk_drag_set_icon_name: (method)
 * @context: the context for a drag (This must be called 
 *     with a context for the source side of a drag)
 * @icon_name: name of icon to use
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 * 
 * Sets the icon for a given drag from a named themed icon. See
 * the docs for #GtkIconTheme for more details. Note that the
 * size of the icon depends on the icon theme (the icon is
 * loaded at the symbolic size #GTK_ICON_SIZE_DND), thus 
 * @hot_x and @hot_y have to be used with care.
 *
 * Since: 2.8
 */
void 
gtk_drag_set_icon_name (GdkDragContext *context,
                        const gchar    *icon_name,
                        gint            hot_x,
                        gint            hot_y)
{
  GtkIconHelper *icon;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (icon_name != NULL);

  icon = _gtk_icon_helper_new ();
  _gtk_icon_helper_set_icon_name (icon, icon_name, GTK_ICON_SIZE_DND);
  set_icon_helper (context, icon, hot_x, hot_y, FALSE);

  g_object_unref (icon);
}

/**
 * gtk_drag_set_icon_gicon: (method)
 * @context: the context for a drag (This must be called 
 *     with a context for the source side of a drag)
 * @icon: a #GIcon
 * @hot_x: the X offset of the hotspot within the icon
 * @hot_y: the Y offset of the hotspot within the icon
 * 
 * Sets the icon for a given drag from the given @icon.
 * See the documentation for gtk_drag_set_icon_name()
 * for more details about using icons in drag and drop.
 *
 * Since: 3.2
 */
void 
gtk_drag_set_icon_gicon (GdkDragContext *context,
                         GIcon          *icon,
                         gint            hot_x,
                         gint            hot_y)
{
  GtkIconHelper *helper;

  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (icon != NULL);

  helper = _gtk_icon_helper_new ();
  _gtk_icon_helper_set_gicon (helper, icon, GTK_ICON_SIZE_DND);
  set_icon_helper (context, helper, hot_x, hot_y, FALSE);

  g_object_unref (helper);
}

/**
 * gtk_drag_set_icon_default: (method)
 * @context: the context for a drag (This must be called 
 *     with a  context for the source side of a drag)
 * 
 * Sets the icon for a particular drag to the default
 * icon.
 */
void 
gtk_drag_set_icon_default (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  gtk_drag_set_icon_name (context, "text-x-generic", -2, -2);
}

/*
 * _gtk_drag_source_handle_event:
 * @toplevel: Toplevel widget that received the event
 * @event: the event to handle
 *
 * Called from widget event handling code on Drag events
 * for drag sources.
 */
void
_gtk_drag_source_handle_event (GtkWidget *widget,
                               GdkEvent  *event)
{
  GtkDragSourceInfo *info;
  GdkDragContext *context;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (event != NULL);

  context = event->dnd.context;
  info = gtk_drag_get_source_info (context, FALSE);
  if (!info)
    return;

  switch (event->type)
    {
    case GDK_DRAG_STATUS:
      {
        GdkCursor *cursor;

        if (info->proxy_dest)
          {
            if (!event->dnd.send_event)
              {
                if (info->proxy_dest->proxy_drop_wait)
                  {
                    gboolean result = gdk_drag_context_get_selected_action (context) != 0;
                    
                    /* Aha - we can finally pass the DROP on... */
                    gdk_drop_reply (info->proxy_dest->context, result, info->proxy_dest->proxy_drop_time);
                    if (result)
                      gdk_drag_drop (info->context, info->proxy_dest->proxy_drop_time);
                    else
                      gtk_drag_finish (info->proxy_dest->context, FALSE, FALSE, info->proxy_dest->proxy_drop_time);
                  }
                else
                  {
                    gdk_drag_status (info->proxy_dest->context,
                                     gdk_drag_context_get_selected_action (event->dnd.context),
                                     event->dnd.time);
                  }
              }
          }
        else if (info->have_grab)
          {
            cursor = gtk_drag_get_cursor (widget, 
                                          gtk_widget_get_display (widget),
                                          gdk_drag_context_get_selected_action (event->dnd.context),
                                          info);
            if (info->cursor != cursor)
              {
                GdkDevice *pointer;

                pointer = gdk_drag_context_get_device (context);
                gdk_device_grab (pointer, gtk_widget_get_window (widget),
                                 GDK_OWNERSHIP_APPLICATION, FALSE,
                                 GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                 cursor, info->grab_time);
                info->cursor = cursor;
              }
            
            gtk_drag_add_update_idle (info);
          }
      }
      break;
      
    case GDK_DROP_FINISHED:
      gtk_drag_drop_finished (info, GTK_DRAG_RESULT_SUCCESS, event->dnd.time);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gtk_drag_source_check_selection (GtkDragSourceInfo *info, 
                                 GdkAtom            selection,
                                 guint32            time)
{
  GList *tmp_list;

  tmp_list = info->selections;
  while (tmp_list)
    {
      if (GDK_POINTER_TO_ATOM (tmp_list->data) == selection)
        return;
      tmp_list = tmp_list->next;
    }

  gtk_selection_owner_set_for_display (gtk_widget_get_display (info->widget),
                                       info->ipc_widget,
                                       selection,
                                       time);
  info->selections = g_list_prepend (info->selections,
                                     GUINT_TO_POINTER (selection));

  tmp_list = info->target_list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;

      gtk_selection_add_target (info->ipc_widget,
                                selection,
                                pair->target,
                                pair->info);
      tmp_list = tmp_list->next;
    }

  gtk_selection_add_target (info->ipc_widget,
                            selection,
                            gdk_atom_intern_static_string ("DELETE"),
                            TARGET_DELETE);
}


/* Clean up from the drag, and display snapback, if necessary. */
static void
gtk_drag_drop_finished (GtkDragSourceInfo *info,
                        GtkDragResult      result,
                        guint              time)
{
  gboolean success;

  success = (result == GTK_DRAG_RESULT_SUCCESS);
  gtk_drag_source_release_selections (info, time); 

  if (info->proxy_dest)
    {
      /* The time from the event isn't reliable for Xdnd drags */
      gtk_drag_finish (info->proxy_dest->context, success, FALSE, 
                       info->proxy_dest->proxy_drop_time);
      gtk_drag_source_info_destroy (info);
    }
  else
    {
      if (!success)
        g_signal_emit_by_name (info->widget, "drag-failed",
                               info->context, result, &success);

      if (success)
        {
          gtk_drag_source_info_destroy (info);
        }
      else
        {
          GtkDragAnim *anim = g_slice_new0 (GtkDragAnim);
          anim->info = info;
          anim->start_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (info->widget));

          info->cur_screen = gtk_widget_get_screen (info->widget);

          if (!info->icon_window)
            set_icon_helper (info->context, info->icon_helper, 
                             0, 0, TRUE);

          gtk_drag_update_icon (info);

          /* Mark the context as dead, so if the destination decides
           * to respond really late, we still are OK.
           */
          gtk_drag_clear_source_info (info->context);
          gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT, 17, gtk_drag_anim_timeout, anim, (GDestroyNotify) gtk_drag_anim_destroy);
        }
    }
}

static void
gtk_drag_source_release_selections (GtkDragSourceInfo *info,
                                    guint32            time)
{
  GdkDisplay *display = gtk_widget_get_display (info->widget);
  GList *tmp_list = info->selections;
  
  while (tmp_list)
    {
      GdkAtom selection = GDK_POINTER_TO_ATOM (tmp_list->data);
      if (gdk_selection_owner_get_for_display (display, selection) == gtk_widget_get_window (info->ipc_widget))
        gtk_selection_owner_set_for_display (display, NULL, selection, time);

      tmp_list = tmp_list->next;
    }

  g_list_free (info->selections);
  info->selections = NULL;
}

static void
gtk_drag_drop (GtkDragSourceInfo *info, 
               guint32            time)
{
  if (gdk_drag_context_get_protocol (info->context) == GDK_DRAG_PROTO_ROOTWIN)
    {
      GtkSelectionData selection_data;
      GList *tmp_list;
      /* GTK+ traditionally has used application/x-rootwin-drop, but the
       * XDND spec specifies x-rootwindow-drop.
       */
      GdkAtom target1 = gdk_atom_intern_static_string ("application/x-rootwindow-drop");
      GdkAtom target2 = gdk_atom_intern_static_string ("application/x-rootwin-drop");
      
      tmp_list = info->target_list->list;
      while (tmp_list)
        {
          GtkTargetPair *pair = tmp_list->data;
          
          if (pair->target == target1 || pair->target == target2)
            {
              selection_data.selection = GDK_NONE;
              selection_data.target = pair->target;
              selection_data.data = NULL;
              selection_data.length = -1;
              
              g_signal_emit_by_name (info->widget, "drag-data-get",
                                     info->context, &selection_data,
                                     pair->info,
                                     time);
              
              /* FIXME: Should we check for length >= 0 here? */
              gtk_drag_drop_finished (info, GTK_DRAG_RESULT_SUCCESS, time);
              return;
            }
          tmp_list = tmp_list->next;
        }
      gtk_drag_drop_finished (info, GTK_DRAG_RESULT_NO_TARGET, time);
    }
  else
    {
      if (info->icon_window)
        gtk_widget_hide (info->icon_window);
        
      gdk_drag_drop (info->context, time);
      info->drop_timeout = gdk_threads_add_timeout (DROP_ABORT_TIME,
                                          gtk_drag_abort_timeout,
                                          info);
      g_source_set_name_by_id (info->drop_timeout, "[gtk+] gtk_drag_abort_timeout");
    }
}

/*
 * Source side callbacks.
 */

static gboolean
gtk_drag_source_event_cb (GtkWidget *widget,
                          GdkEvent  *event,
                          gpointer   data)
{
  gdouble start_x, start_y, offset_x, offset_y;
  GtkDragSourceSite *site = data;

  gtk_event_controller_handle_event (GTK_EVENT_CONTROLLER (site->drag_gesture), event);

  if (gtk_gesture_is_recognized (site->drag_gesture))
    {
      gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (site->drag_gesture),
                                        &start_x, &start_y);
      gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (site->drag_gesture),
                                   &offset_x, &offset_y);

      if (gtk_drag_check_threshold (widget, start_x, start_y,
                                    start_x + offset_x, start_y + offset_y))
        {
          GdkEventSequence *sequence;
          const GdkEvent *event;
          guint button;

          sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (site->drag_gesture));
          event = gtk_gesture_get_last_event (site->drag_gesture, sequence);
          button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (site->drag_gesture));

          gtk_event_controller_reset (GTK_EVENT_CONTROLLER (site->drag_gesture));
          gtk_drag_begin_internal (widget, site, site->target_list,
                                   site->actions, button, event,
                                   start_x, start_y);
          return TRUE;
        }
    }

  return FALSE;
}

static void 
gtk_drag_source_site_destroy (gpointer data)
{
  GtkDragSourceSite *site = data;

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  g_clear_object (&site->icon_helper);
  g_clear_object (&site->drag_gesture);
  g_slice_free (GtkDragSourceSite, site);
}

static void
gtk_drag_selection_get (GtkWidget        *widget, 
                        GtkSelectionData *selection_data,
                        guint             sel_info,
                        guint32           time,
                        gpointer          data)
{
  GtkDragSourceInfo *info = data;
  static GdkAtom null_atom = GDK_NONE;
  guint target_info;

  if (!null_atom)
    null_atom = gdk_atom_intern_static_string ("NULL");

  switch (sel_info)
    {
    case TARGET_DELETE:
      g_signal_emit_by_name (info->widget,
                             "drag-data-delete", 
                             info->context);
      gtk_selection_data_set (selection_data, null_atom, 8, NULL, 0);
      break;
    default:
      if (info->proxy_dest)
        {
          /* This is sort of dangerous and needs to be thought
           * through better
           */
          info->proxy_dest->proxy_data = selection_data;
          gtk_drag_get_data (info->widget,
                             info->proxy_dest->context,
                             gtk_selection_data_get_target (selection_data),
                             time);
          gtk_main ();
          info->proxy_dest->proxy_data = NULL;
        }
      else
        {
          if (gtk_target_list_find (info->target_list, 
                                    gtk_selection_data_get_target (selection_data),
                                    &target_info))
            {
              g_signal_emit_by_name (info->widget, "drag-data-get",
                                     info->context,
                                     selection_data,
                                     target_info,
                                     time);
            }
        }
      break;
    }
}

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static void
gtk_drag_anim_destroy (GtkDragAnim *anim)
{
  gtk_drag_source_info_destroy (anim->info);
  g_slice_free (GtkDragAnim, anim);
}

static gboolean
gtk_drag_anim_timeout (gpointer data)
{
  GtkWidget *icon_window;
  GtkDragAnim *anim = data;
  GtkDragSourceInfo *info = anim->info;
  GtkWidget *widget = info->widget;
  GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (widget);
  gint64 current_time = gdk_frame_clock_get_frame_time (frame_clock);
  int hot_x, hot_y;
  double f = (current_time - anim->start_time) / (double) ANIM_TIME;
  double t;

  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  gtk_drag_get_icon (info, &icon_window, &hot_x, &hot_y);
  gtk_window_move (GTK_WINDOW (icon_window),
                   (info->cur_x + (info->start_x - info->cur_x) * t) - hot_x,
                   (info->cur_y + (info->start_y - info->cur_y) * t) - hot_y);
  gtk_widget_set_opacity (icon_window, (1.0 - f));

  return G_SOURCE_CONTINUE;
}

static void
gtk_drag_remove_icon (GtkDragSourceInfo *info)
{
  if (info->icon_window)
    {
      gtk_widget_hide (info->icon_window);
      if (info->destroy_icon)
        gtk_widget_destroy (info->icon_window);

      if (info->fallback_icon)
        {
          gtk_widget_destroy (info->fallback_icon);
          info->fallback_icon = NULL;
        }

      g_object_unref (info->icon_window);
      info->icon_window = NULL;
    }
}

static void
gtk_drag_source_info_destroy (GtkDragSourceInfo *info)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (drag_cursors); i++)
    {
      if (info->drag_cursors[i] != NULL)
        {
          g_object_unref (info->drag_cursors[i]);
          info->drag_cursors[i] = NULL;
        }
    }

  gtk_drag_remove_icon (info);
  g_clear_object (&info->icon_helper);

  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_grab_broken_event_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_grab_notify_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_button_release_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_motion_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_key_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_selection_get,
                                        info);

  if (!info->proxy_dest)
    g_signal_emit_by_name (info->widget, "drag-end", info->context);

  if (info->widget)
    g_object_unref (info->widget);

  gtk_selection_remove_all (info->ipc_widget);
  g_object_set_data (G_OBJECT (info->ipc_widget), I_("gtk-info"), NULL);
  source_widgets = g_slist_remove (source_widgets, info->ipc_widget);
  gtk_drag_release_ipc_widget (info->ipc_widget);

  gtk_target_list_unref (info->target_list);

  gtk_drag_clear_source_info (info->context);
  g_object_unref (info->context);

  if (info->drop_timeout)
    g_source_remove (info->drop_timeout);

  if (info->update_idle)
    g_source_remove (info->update_idle);

  g_free (info);
}

static gboolean
gtk_drag_update_idle (gpointer data)
{
  GtkDragSourceInfo *info = data;
  GdkWindow *dest_window;
  GdkDragProtocol protocol;
  GdkAtom selection;

  GdkDragAction action;
  GdkDragAction possible_actions;
  guint32 time;

  info->update_idle = 0;
    
  if (info->last_event)
    {
      time = gtk_drag_get_event_time (info->last_event);
      gtk_drag_get_event_actions (info->last_event,
                                  info->button, 
                                  info->possible_actions,
                                  &action, &possible_actions);
      gtk_drag_update_icon (info);
      gdk_drag_find_window_for_screen (info->context,
                                       info->icon_window ? gtk_widget_get_window (info->icon_window) : NULL,
                                       info->cur_screen, info->cur_x, info->cur_y,
                                       &dest_window, &protocol);
      
      if (!gdk_drag_motion (info->context, dest_window, protocol,
                            info->cur_x, info->cur_y, action, 
                            possible_actions,
                            time))
        {
          gdk_event_free ((GdkEvent *)info->last_event);
          info->last_event = NULL;
        }
  
      if (dest_window)
        g_object_unref (dest_window);
      
      selection = gdk_drag_get_selection (info->context);
      if (selection)
        gtk_drag_source_check_selection (info, selection, time);

    }

  return FALSE;
}

static void
gtk_drag_add_update_idle (GtkDragSourceInfo *info)
{
  /* We use an idle lower than GDK_PRIORITY_REDRAW so that exposes
   * from the last move can catch up before we move again.
   */
  if (!info->update_idle)
    {
      info->update_idle = gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW + 5,
                                                     gtk_drag_update_idle,
                                                     info,
                                                     NULL);
      g_source_set_name_by_id (info->update_idle, "[gtk+] gtk_drag_update_idle");
    }
}

/*
 * gtk_drag_update:
 * @info: DragSourceInfo for the drag
 * @screen: new screen
 * @x_root: new X position 
 * @y_root: new y position
 * @event: event received requiring update
 * 
 * Updates the status of the drag; called when the
 * cursor moves or the modifier changes
 */
static void
gtk_drag_update (GtkDragSourceInfo *info,
                 GdkScreen         *screen,
                 gint               x_root,
                 gint               y_root,
                 const GdkEvent    *event)
{
  info->cur_screen = screen;
  info->cur_x = x_root;
  info->cur_y = y_root;
  if (info->last_event)
    {
      gdk_event_free ((GdkEvent *)info->last_event);
      info->last_event = NULL;
    }
  if (event)
    info->last_event = gdk_event_copy ((GdkEvent *)event);

  gtk_drag_add_update_idle (info);
}

/* Called when the user finishes to drag, either by
 * releasing the mouse, or by pressing Esc.
 */
static void
gtk_drag_end (GtkDragSourceInfo *info,
              guint32            time)
{
  GtkWidget *source_widget = info->widget;
  GdkDevice *pointer, *keyboard;

  pointer = gdk_drag_context_get_device (info->context);
  keyboard = gdk_device_get_associated_device (pointer);

  /* Prevent ungrab before grab (see bug 623865) */
  if (info->grab_time == GDK_CURRENT_TIME)
    time = GDK_CURRENT_TIME;

  if (info->update_idle)
    {
      g_source_remove (info->update_idle);
      info->update_idle = 0;
    }
  
  if (info->last_event)
    {
      gdk_event_free (info->last_event);
      info->last_event = NULL;
    }
  
  info->have_grab = FALSE;
  
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_grab_broken_event_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_grab_notify_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_button_release_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_motion_cb,
                                        info);
  g_signal_handlers_disconnect_by_func (info->ipc_widget,
                                        gtk_drag_key_cb,
                                        info);

  gdk_device_ungrab (pointer, time);
  ungrab_dnd_keys (info->ipc_widget, keyboard, time);
  gtk_device_grab_remove (info->ipc_widget, pointer);

  if (gtk_widget_get_realized (source_widget))
    {
      GdkEvent *send_event;

      /* Send on a release pair to the original widget to convince it
       * to release its grab. We need to call gtk_propagate_event()
       * here, instead of gtk_widget_event() because widget like
       * GtkList may expect propagation.
       */

      send_event = gdk_event_new (GDK_BUTTON_RELEASE);
      send_event->button.window = g_object_ref (gdk_screen_get_root_window (gtk_widget_get_screen (source_widget)));
      send_event->button.send_event = TRUE;
      send_event->button.time = time;
      send_event->button.x = 0;
      send_event->button.y = 0;
      send_event->button.axes = NULL;
      send_event->button.state = 0;
      send_event->button.button = info->button;
      send_event->button.device = pointer;
      send_event->button.x_root = 0;
      send_event->button.y_root = 0;

      gtk_propagate_event (source_widget, send_event);
      gdk_event_free (send_event);
    }
}

/* Called on cancellation of a drag, either by the user
 * or programmatically.
 */
static void
gtk_drag_cancel (GtkDragSourceInfo *info,
                 GtkDragResult      result,
                 guint32            time)
{
  gtk_drag_end (info, time);
  gdk_drag_abort (info->context, time);
  gtk_drag_drop_finished (info, result, time);
}

/* “motion-notify-event” callback during drag. */
static gboolean
gtk_drag_motion_cb (GtkWidget      *widget,
                    GdkEventMotion *event,
                    gpointer        data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;
  GdkScreen *screen;
  gint x_root, y_root;

  if (event->is_hint)
    {
      gdk_device_get_position (event->device, &screen, &x_root, &y_root);
      event->x_root = x_root;
      event->y_root = y_root;
    }
  else
    screen = gdk_event_get_screen ((GdkEvent *)event);

  x_root = (gint)(event->x_root + 0.5);
  y_root = (gint)(event->y_root + 0.5);
  gtk_drag_update (info, screen, x_root, y_root, (GdkEvent *) event);

  return TRUE;
}

#define BIG_STEP 20
#define SMALL_STEP 1

/* “key-press/release-event” callback during drag */
static gboolean
gtk_drag_key_cb (GtkWidget   *widget, 
                 GdkEventKey *event, 
                 gpointer     data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;
  GdkModifierType state;
  GdkWindow *root_window;
  GdkDevice *pointer;
  gint dx, dy;

  dx = dy = 0;
  state = event->state & gtk_accelerator_get_default_mod_mask ();
  pointer = gdk_device_get_associated_device (gdk_event_get_device ((GdkEvent *) event));

  if (event->type == GDK_KEY_PRESS)
    {
      switch (event->keyval)
        {
        case GDK_KEY_Escape:
          gtk_drag_cancel (info, GTK_DRAG_RESULT_USER_CANCELLED, event->time);
          return TRUE;

        case GDK_KEY_space:
        case GDK_KEY_Return:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_KP_Space:
          if ((gdk_drag_context_get_selected_action (info->context) != 0) &&
              (gdk_drag_context_get_dest_window (info->context) != NULL))
            {
              gtk_drag_end (info, event->time);
              gtk_drag_drop (info, event->time);
            }
          else
            {
              gtk_drag_cancel (info, GTK_DRAG_RESULT_NO_TARGET, event->time);
            }

          return TRUE;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          dy = (state & GDK_MOD1_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          dy = (state & GDK_MOD1_MASK) ? BIG_STEP : SMALL_STEP;
          break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
          dx = (state & GDK_MOD1_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
          dx = (state & GDK_MOD1_MASK) ? BIG_STEP : SMALL_STEP;
          break;
        }
    }

  /* Now send a "motion" so that the modifier state is updated */

  /* The state is not yet updated in the event, so we need
   * to query it here. We could use XGetModifierMapping, but
   * that would be overkill.
   */
  root_window = gdk_screen_get_root_window (gtk_widget_get_screen (widget));
  gdk_window_get_device_position (root_window, pointer, NULL, NULL, &state);
  event->state = state;

  if (dx != 0 || dy != 0)
    {
      info->cur_x += dx;
      info->cur_y += dy;
      gdk_device_warp (pointer,
                       gtk_widget_get_screen (widget),
                       info->cur_x, info->cur_y);
    }

  gtk_drag_update (info, info->cur_screen, info->cur_x, info->cur_y, (GdkEvent *)event);

  return TRUE;
}

static gboolean
gtk_drag_grab_broken_event_cb (GtkWidget          *widget,
                               GdkEventGrabBroken *event,
                               gpointer            data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;

  /* Don't cancel if we break the implicit grab from the initial button_press.
   * Also, don't cancel if we re-grab on the widget or on our IPC window, for
   * example, when changing the drag cursor.
   */
  if (event->implicit
      || event->grab_window == gtk_widget_get_window (info->widget)
      || event->grab_window == gtk_widget_get_window (info->ipc_widget))
    return FALSE;

  gtk_drag_cancel (info, GTK_DRAG_RESULT_GRAB_BROKEN, gtk_get_current_event_time ());
  return TRUE;
}

static void
gtk_drag_grab_notify_cb (GtkWidget *widget,
                         gboolean   was_grabbed,
                         gpointer   data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;
  GdkDevice *pointer;

  pointer = gdk_drag_context_get_device (info->context);

  if (gtk_widget_device_is_shadowed (widget, pointer))
    {
      /* We have to block callbacks to avoid recursion here, because
         gtk_drag_cancel calls gtk_grab_remove (via gtk_drag_end) */
      g_signal_handlers_block_by_func (widget, gtk_drag_grab_notify_cb, data);
      gtk_drag_cancel (info, GTK_DRAG_RESULT_GRAB_BROKEN, gtk_get_current_event_time ());
      g_signal_handlers_unblock_by_func (widget, gtk_drag_grab_notify_cb, data);
    }
}

/* “button-release-event” callback during drag */
static gboolean
gtk_drag_button_release_cb (GtkWidget      *widget, 
                            GdkEventButton *event, 
                            gpointer        data)
{
  GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;

  if (event->button != info->button)
    return FALSE;

  if ((gdk_drag_context_get_selected_action (info->context) != 0) &&
      (gdk_drag_context_get_dest_window (info->context) != NULL))
    {
      gtk_drag_end (info, event->time);
      gtk_drag_drop (info, event->time);
    }
  else
    {
      gtk_drag_cancel (info, GTK_DRAG_RESULT_NO_TARGET, event->time);
    }

  return TRUE;
}

static gboolean
gtk_drag_abort_timeout (gpointer data)
{
  GtkDragSourceInfo *info = data;
  guint32 time = GDK_CURRENT_TIME;

  if (info->proxy_dest)
    time = info->proxy_dest->proxy_drop_time;

  info->drop_timeout = 0;
  gtk_drag_drop_finished (info, GTK_DRAG_RESULT_TIMEOUT_EXPIRED, time);
  
  return FALSE;
}

/**
 * gtk_drag_check_threshold: (method)
 * @widget: a #GtkWidget
 * @start_x: X coordinate of start of drag
 * @start_y: Y coordinate of start of drag
 * @current_x: current X coordinate
 * @current_y: current Y coordinate
 * 
 * Checks to see if a mouse drag starting at (@start_x, @start_y) and ending
 * at (@current_x, @current_y) has passed the GTK+ drag threshold, and thus
 * should trigger the beginning of a drag-and-drop operation.
 *
 * Returns: %TRUE if the drag threshold has been passed.
 */
gboolean
gtk_drag_check_threshold (GtkWidget *widget,
                          gint       start_x,
                          gint       start_y,
                          gint       current_x,
                          gint       current_y)
{
  gint drag_threshold;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-dnd-drag-threshold", &drag_threshold,
                NULL);
  
  return (ABS (current_x - start_x) > drag_threshold ||
          ABS (current_y - start_y) > drag_threshold);
}
