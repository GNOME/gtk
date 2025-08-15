
/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2001,2009 Hans Breuer
 * Copyright (C) 2007-2009 Cody Russell
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2020.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Cannot use TrackMouseEvent, as the stupid WM_MOUSELEAVE message
 * doesn’t tell us where the mouse has gone. Thus we cannot use it to
 * generate a correct GdkNotifyType. Pity, as using TrackMouseEvent
 * otherwise would make it possible to reliably generate
 * GDK_LEAVE_NOTIFY events, which would help get rid of those pesky
 * tooltips sometimes popping up in the wrong place.
 *
 * Update: a combination of TrackMouseEvent, GetCursorPos and
 * GetWindowPos can and is actually used to get rid of those
 * pesky tooltips. It should be possible to use this for the
 * whole ENTER/LEAVE NOTIFY handling but some platforms may
 * not have TrackMouseEvent at all (?) --hb
 */

#include "config.h"

#include "gdkprivate-win32.h"

#include <glib/gprintf.h>
#include <cairo-win32.h>

#include "gdk.h"
#include "gdkdevicemanager-win32.h"
#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"
#include "gdkglcontext-win32.h"
#include "gdkkeysyms.h"
#include "gdkmonitorprivate.h"
#include "gdkwin32.h"
#include "gdkdisplay-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-virtual.h"
#include "gdkdevice-wintab.h"
#include "gdkinput-dmanipulation.h"
#include "gdkinput-winpointer.h"
#include "gdkwin32dnd.h"
#include "gdkwin32dnd-private.h"
#include "gdkdisplay-win32.h"
//#include "gdkselection-win32.h"
#include "gdkdragprivate.h"
#include "gdkprivate.h"

#include <windowsx.h>

#ifdef G_WITH_CYGWIN
#include <fcntl.h>
#include <errno.h>
#endif

#include <objbase.h>
#include <imm.h>
#include <tchar.h>
#include <tpcshrd.h>

#include <stdint.h>

/* Undefined flags: */
#define SWP_NOCLIENTSIZE 0x0800
#define SWP_NOCLIENTMOVE 0x1000
#define SWP_STATECHANGED 0x8000
/*
 * Private function declarations
 */

#define SYNAPSIS_ICON_WINDOW_CLASS "SynTrackCursorWindowClass"

static gboolean gdk_event_translate (MSG        *msg,
				     int        *ret_valp);
static gboolean gdk_event_prepare  (GSource     *source,
				    int         *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

/* Private variable declarations
 */

typedef struct
{
  GSource source;

  GdkDisplay *display;
  GPollFD event_poll_fd;
} GdkWin32EventSource;

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

/* TODO: pending removal, pending gtkprintoperation-win32 rewrite */
static UINT got_gdk_events_message;
static HWND modal_win32_dialog = NULL;

#if 0
static HKL latin_locale = NULL;
#endif

static void
track_mouse_event (DWORD dwFlags,
		   HWND  hwnd)
{
  TRACKMOUSEEVENT tme;

  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = dwFlags;
  tme.hwndTrack = hwnd;
  tme.dwHoverTime = HOVER_DEFAULT; /* not used */

  if (!TrackMouseEvent (&tme))
    WIN32_API_FAILED ("TrackMouseEvent");
  else if (dwFlags == TME_LEAVE)
    GDK_NOTE (EVENTS, g_print(" (TrackMouseEvent %p)", hwnd));
  else if (dwFlags == TME_CANCEL)
    GDK_NOTE (EVENTS, g_print(" (cancel TrackMouseEvent %p)", hwnd));
}

gulong
_gdk_win32_get_next_tick (gulong suggested_tick)
{
  static gulong cur_tick = 0;

  if (suggested_tick == 0)
    suggested_tick = GetTickCount ();
  /* Ticks eventually wrap around.
   * This works as long as the interval between ticks is < 2147483648ms */
  if (suggested_tick <= cur_tick && ((cur_tick - suggested_tick) < 0x7FFFFFFF))
    return cur_tick;
  else
    return cur_tick = suggested_tick;
}

BOOL
_gdk_win32_get_cursor_pos (GdkDisplay *display,
                           LPPOINT     lpPoint)
{
  GdkDeviceManagerWin32 *manager = GDK_WIN32_DISPLAY (display)->device_manager;

  if (manager->pen_touch_input)
    {
      *lpPoint = manager->latest_pen_touch_position;
      return TRUE;
    }
  else
    return GetCursorPos (lpPoint);
}

static void
generate_focus_event (GdkDeviceManagerWin32 *device_manager,
                      GdkSurface            *surface,
                      gboolean               in)
{
  GdkDevice *device;
  GdkEvent *event;

  device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->core_keyboard;

  event = gdk_focus_event_new (surface, device, in);

  _gdk_win32_append_event (event);
}

static void
generate_grab_broken_event (GdkDeviceManagerWin32 *device_manager,
                            GdkSurface            *surface,
                            gboolean               keyboard,
                            GdkSurface            *grab_surface)
{
  GdkEvent *event;
  GdkDevice *device;

  if (keyboard)
    device = device_manager->core_keyboard;
  else
    device = device_manager->core_pointer;

  event = gdk_grab_broken_event_new (surface,
                                     device,
                                     grab_surface,
                                     FALSE);

  _gdk_win32_append_event (event);
}

static LRESULT
inner_hwnd_procedure (HWND   hwnd,
			UINT   message,
			WPARAM wparam,
			LPARAM lparam)
{
  MSG msg;
  DWORD pos;
  int ret_val = 0;

  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wparam;
  msg.lParam = lparam;
  msg.time = _gdk_win32_get_next_tick (0);
  pos = GetMessagePos ();
  msg.pt.x = GET_X_LPARAM (pos);
  msg.pt.y = GET_Y_LPARAM (pos);

  if (gdk_event_translate (&msg, &ret_val))
    {
      /* If gdk_event_translate() returns TRUE, we return ret_val from
       * the hwnd procedure.
       */
      if (modal_win32_dialog)
	PostMessageW (modal_win32_dialog, got_gdk_events_message,
		      (WPARAM) 1, 0);
      return ret_val;
    }
  else
    {
      /* Otherwise call DefWindowProcW(). */
      GDK_NOTE (EVENTS, g_print (" DefWindowProcW"));
      return DefWindowProcW (hwnd, message, wparam, lparam);
    }
}

LRESULT CALLBACK
_gdk_win32_surface_procedure (HWND   hwnd,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  LRESULT retval;
  GdkWin32Display *display = NULL;
  GdkSurface *surface = NULL;

  if (message != WM_CREATE && message != WM_NCCREATE)
    surface = GDK_SURFACE (GetWindowLongPtr (hwnd, GWLP_USERDATA));

  if (surface != NULL)
    display = GDK_WIN32_DISPLAY (gdk_surface_get_display (surface));
  else
    display = GDK_WIN32_DISPLAY (gdk_display_get_default ());

  GDK_NOTE (EVENTS, g_print ("%s%*s%s %p %#" G_GINTPTR_MODIFIER "x %#" G_GINTPTR_MODIFIER "x",
			     (display->event_record->debug_indent_surface_events > 0 ? "\n" : ""),
			     display->event_record->debug_indent_surface_events, "",
			     _gdk_win32_message_to_string (message), hwnd,
			     wparam, lparam));
  display->event_record->debug_indent_surface_events += 2;
  retval = inner_hwnd_procedure (hwnd, message, wparam, lparam);
  display->event_record->debug_indent_surface_events -= 2;

  GDK_NOTE (EVENTS, g_print (" => %" G_GINT64_FORMAT "%s", (gint64) retval, (display->event_record->debug_indent_surface_events == 0 ? "\n" : "")));

  return retval;
}

void
_gdk_events_init (GdkDisplay *display)
{
  GSource *source;
  GdkWin32EventSource *event_source;

#if 0
  int i, j, n;

  /* List of languages that use a latin keyboard. Somewhat sorted in
   * "order of least surprise", in case we have to load one of them if
   * the user only has arabic loaded, for instance.
   */
  static int latin_languages[] = {
    LANG_ENGLISH,
    LANG_SPANISH,
    LANG_PORTUGUESE,
    LANG_FRENCH,
    LANG_GERMAN,
    /* Rest in numeric order */
    LANG_CZECH,
    LANG_DANISH,
    LANG_FINNISH,
    LANG_HUNGARIAN,
    LANG_ICELANDIC,
    LANG_ITALIAN,
    LANG_DUTCH,
    LANG_NORWEGIAN,
    LANG_POLISH,
    LANG_ROMANIAN,
    LANG_SLOVAK,
    LANG_ALBANIAN,
    LANG_SWEDISH,
    LANG_TURKISH,
    LANG_INDONESIAN,
    LANG_SLOVENIAN,
    LANG_ESTONIAN,
    LANG_LATVIAN,
    LANG_LITHUANIAN,
    LANG_VIETNAMESE,
    LANG_AFRIKAANS,
    LANG_FAEROESE
#ifdef LANG_SWAHILI
   ,LANG_SWAHILI
#endif
  };
#endif

  got_gdk_events_message = RegisterWindowMessage (L"GDK_WIN32_GOT_EVENTS");

#if 0
  /* Check if we have some input locale identifier loaded that uses a
   * latin keyboard, to be able to get the virtual-key code for the
   * latin characters corresponding to ASCII control characters.
   */
  if ((n = GetKeyboardLayoutList (0, NULL)) == 0)
    WIN32_API_FAILED ("GetKeyboardLayoutList");
  else
    {
      HKL *hkl_list = g_new (HKL, n);
      if (GetKeyboardLayoutList (n, hkl_list) == 0)
	WIN32_API_FAILED ("GetKeyboardLayoutList");
      else
	{
	  for (i = 0; latin_locale == NULL && i < n; i++)
	    for (j = 0; j < G_N_ELEMENTS (latin_languages); j++)
	      if (PRIMARYLANGID (LOWORD (hkl_list[i])) == latin_languages[j])
		{
		  latin_locale = hkl_list [i];
		  break;
		}
	}
      g_free (hkl_list);
    }

  if (latin_locale == NULL)
    {
      /* Try to load a keyboard layout with latin characters then.
       */
      i = 0;
      while (latin_locale == NULL && i < G_N_ELEMENTS (latin_languages))
	{
	  char id[9];
	  g_sprintf (id, "%08x", MAKELANGID (latin_languages[i++], SUBLANG_DEFAULT));
	  latin_locale = LoadKeyboardLayout (id, KLF_NOTELLSHELL|KLF_SUBSTITUTE_OK);
	}
    }

  GDK_NOTE (EVENTS, g_print ("latin_locale = %08x\n", (guint) latin_locale));
#endif

  source = g_source_new (&event_funcs, sizeof (GdkWin32EventSource));
  g_source_set_static_name (source, "GDK Win32 event source");
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);

  event_source = (GdkWin32EventSource *)source;
  event_source->display = display;
#ifdef G_WITH_CYGWIN
  event_source->event_poll_fd.fd = open ("/dev/windows", O_RDONLY);
  if (event_source->event_poll_fd.fd == -1)
    g_error ("can't open \"/dev/windows\": %s", g_strerror (errno));
#else
  event_source->event_poll_fd.fd = G_WIN32_MSG_HANDLE;
#endif
  event_source->event_poll_fd.events = G_IO_IN;

  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
}

#if 0 /* Unused, but might be useful to re-introduce in some debugging output? */

static char *
event_mask_string (GdkEventMask mask)
{
  static char bfr[500];
  char *p = bfr;

  *p = '\0';
#define BIT(x) \
  if (mask & GDK_##x##_MASK) \
    p += g_sprintf (p, "%s" #x, (p > bfr ? " " : ""))
  BIT (EXPOSURE);
  BIT (POINTER_MOTION);
  BIT (POINTER_MOTION_HINT);
  BIT (BUTTON_MOTION);
  BIT (BUTTON1_MOTION);
  BIT (BUTTON2_MOTION);
  BIT (BUTTON3_MOTION);
  BIT (BUTTON_PRESS);
  BIT (BUTTON_RELEASE);
  BIT (KEY_PRESS);
  BIT (KEY_RELEASE);
  BIT (ENTER_NOTIFY);
  BIT (LEAVE_NOTIFY);
  BIT (FOCUS_CHANGE);
  BIT (STRUCTURE);
  BIT (PROPERTY_CHANGE);
  BIT (VISIBILITY_NOTIFY);
  BIT (PROXIMITY_IN);
  BIT (PROXIMITY_OUT);
  BIT (SCROLL);
#undef BIT

  return bfr;
}

#endif

static GdkSurface *
find_surface_for_mouse_event (GdkSurface *reported_surface,
			                  MSG        *msg)
{
  POINT pt;
  GdkDisplay *display;
  GdkDeviceManagerWin32 *device_manager;
  GdkSurface *event_surface;
  HWND hwnd;
  RECT rect;
  GdkDeviceGrabInfo *grab;

  display = gdk_display_get_default ();
  device_manager = GDK_WIN32_DISPLAY (display)->device_manager;

  grab = _gdk_display_get_last_device_grab (display, device_manager->core_pointer);
  if (grab == NULL)
    return reported_surface;

  pt = msg->pt;

  if (!grab->owner_events)
    event_surface = grab->surface;
  else
    {
      event_surface = NULL;
      hwnd = WindowFromPoint (pt);
      if (hwnd != NULL)
	{
	  POINT client_pt = pt;

	  ScreenToClient (hwnd, &client_pt);
	  GetClientRect (hwnd, &rect);
	  if (PtInRect (&rect, client_pt))
	    event_surface = gdk_win32_display_handle_table_lookup_ (display, hwnd);
	}
      if (event_surface == NULL)
	event_surface = grab->surface;
    }

  /* need to also adjust the coordinates to the new surface */
  ScreenToClient (GDK_SURFACE_HWND (event_surface), &pt);

  /* ATTENTION: need to update client coords */
  msg->lParam = MAKELPARAM (pt.x, pt.y);

  return event_surface;
}

static GdkModifierType
build_key_event_state (GdkDisplay *display,
                       BYTE       *key_state)
{
  GdkModifierType state;
  GdkWin32Keymap *keymap;
  keymap = GDK_WIN32_KEYMAP (gdk_display_get_keymap (display));

  state = _gdk_win32_keymap_get_mod_mask (keymap);

  if (key_state[VK_CAPITAL] & 0x01)
    state |= GDK_LOCK_MASK;

  if (key_state[VK_LBUTTON] & 0x80)
    state |= GDK_BUTTON1_MASK;
  if (key_state[VK_MBUTTON] & 0x80)
    state |= GDK_BUTTON2_MASK;
  if (key_state[VK_RBUTTON] & 0x80)
    state |= GDK_BUTTON3_MASK;
  if (key_state[VK_XBUTTON1] & 0x80)
    state |= GDK_BUTTON4_MASK;
  if (key_state[VK_XBUTTON2] & 0x80)
    state |= GDK_BUTTON5_MASK;

  return state;
}

static guint8
get_active_group (GdkDisplay *display)
{
  GdkWin32Keymap *keymap;

  keymap = GDK_WIN32_KEYMAP (gdk_display_get_keymap (display));

  return _gdk_win32_keymap_get_active_group (keymap);
}

static int
build_pointer_event_state (MSG *msg)
{
  int state;

  state = 0;

  if (msg->wParam & MK_CONTROL)
    state |= GDK_CONTROL_MASK;

  if ((msg->message != WM_LBUTTONDOWN &&
       (msg->wParam & MK_LBUTTON)) ||
      msg->message == WM_LBUTTONUP)
    state |= GDK_BUTTON1_MASK;

  if ((msg->message != WM_MBUTTONDOWN &&
       (msg->wParam & MK_MBUTTON)) ||
      msg->message == WM_MBUTTONUP)
    state |= GDK_BUTTON2_MASK;

  if ((msg->message != WM_RBUTTONDOWN &&
       (msg->wParam & MK_RBUTTON)) ||
      msg->message == WM_RBUTTONUP)
    state |= GDK_BUTTON3_MASK;

  if (((msg->message != WM_XBUTTONDOWN || HIWORD (msg->wParam) != XBUTTON1) &&
       (msg->wParam & MK_XBUTTON1)) ||
      (msg->message == WM_XBUTTONUP && HIWORD (msg->wParam) == XBUTTON1))
    state |= GDK_BUTTON4_MASK;

  if (((msg->message != WM_XBUTTONDOWN || HIWORD (msg->wParam) != XBUTTON2) &&
       (msg->wParam & MK_XBUTTON2)) ||
      (msg->message == WM_XBUTTONUP && HIWORD (msg->wParam) == XBUTTON2))
    state |= GDK_BUTTON5_MASK;

  if (msg->wParam & MK_SHIFT)
    state |= GDK_SHIFT_MASK;

  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_ALT_MASK;

  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  return state;
}

static void
print_event_state (guint state)
{
#define CASE(bit) if (state & GDK_ ## bit ## _MASK) g_print (#bit " ");
  CASE (SHIFT);
  CASE (LOCK);
  CASE (CONTROL);
  CASE (ALT);
  CASE (BUTTON1);
  CASE (BUTTON2);
  CASE (BUTTON3);
  CASE (BUTTON4);
  CASE (BUTTON5);
#undef CASE
}

void
_gdk_win32_print_event (GdkEvent *event)
{
  const char *kvname;
  double x, y;
  GdkCrossingMode mode;
  GdkNotifyType detail;
  GdkScrollDirection direction;
  GdkWin32Display *display = GDK_WIN32_DISPLAY (gdk_surface_get_display (gdk_event_get_surface (event)));
  int debug_indent = display->event_record->debug_indent_surface_events;

  g_print ("%s%*s===> ", (debug_indent > 0 ? "\n" : ""), debug_indent, "");
  switch (gdk_event_get_event_type (event))
    {
#define CASE(x) case x: g_print (#x); break;
    CASE (GDK_DELETE);
    CASE (GDK_MOTION_NOTIFY);
    CASE (GDK_BUTTON_PRESS);
    CASE (GDK_BUTTON_RELEASE);
    CASE (GDK_KEY_PRESS);
    CASE (GDK_KEY_RELEASE);
    CASE (GDK_ENTER_NOTIFY);
    CASE (GDK_LEAVE_NOTIFY);
    CASE (GDK_FOCUS_CHANGE);
    CASE (GDK_PROXIMITY_IN);
    CASE (GDK_PROXIMITY_OUT);
    CASE (GDK_DRAG_ENTER);
    CASE (GDK_DRAG_LEAVE);
    CASE (GDK_DRAG_MOTION);
    CASE (GDK_DROP_START);
    CASE (GDK_SCROLL);
    CASE (GDK_GRAB_BROKEN);
    CASE (GDK_TOUCH_BEGIN);
    CASE (GDK_TOUCH_UPDATE);
    CASE (GDK_TOUCH_END);
    CASE (GDK_TOUCH_CANCEL);
    CASE (GDK_TOUCHPAD_SWIPE);
    CASE (GDK_TOUCHPAD_PINCH);
    CASE (GDK_PAD_BUTTON_PRESS);
    CASE (GDK_PAD_BUTTON_RELEASE);
    CASE (GDK_PAD_RING);
    CASE (GDK_PAD_STRIP);
    CASE (GDK_PAD_GROUP_MODE);
    CASE (GDK_TOUCHPAD_HOLD);
#undef CASE
    default: g_assert_not_reached ();
    }

  g_print (" %p @ %ums ",
           GDK_SURFACE_HWND (gdk_event_get_surface (event)),
           gdk_event_get_time (event));

  switch (gdk_event_get_event_type (event))
    {
    case GDK_MOTION_NOTIFY:
      gdk_event_get_position (event, &x, &y);
      g_print ("(%.4g,%.4g) ", x, y);
      print_event_state (gdk_event_get_modifier_state (event));
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      gdk_event_get_position (event, &x, &y);
      g_print ("%d (%.4g,%.4g) ", gdk_button_event_get_button (event), x, y);
      print_event_state (gdk_event_get_modifier_state (event));
      break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      kvname = gdk_keyval_name (gdk_key_event_get_keyval (event));
      g_print ("%#.02x group:%d %s",
               gdk_key_event_get_keycode (event),
               gdk_key_event_get_layout (event),
	       (kvname ? kvname : "??"));
      print_event_state (gdk_event_get_modifier_state (event));
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      gdk_event_get_position (event, &x, &y);
      mode = gdk_crossing_event_get_mode (event);
      detail = gdk_crossing_event_get_detail (event);
      g_print ("(%.4g,%.4g) %s %s",
	       x, y,
	       (mode == GDK_CROSSING_NORMAL ? "NORMAL" :
		(mode == GDK_CROSSING_GRAB ? "GRAB" :
		 (mode == GDK_CROSSING_UNGRAB ? "UNGRAB" :
		  "???"))),
	       (detail == GDK_NOTIFY_ANCESTOR ? "ANCESTOR" :
		(detail == GDK_NOTIFY_VIRTUAL ? "VIRTUAL" :
		 (detail == GDK_NOTIFY_INFERIOR ? "INFERIOR" :
		  (detail == GDK_NOTIFY_NONLINEAR ? "NONLINEAR" :
		   (detail == GDK_NOTIFY_NONLINEAR_VIRTUAL ? "NONLINEAR_VIRTUAL" :
		    (detail == GDK_NOTIFY_UNKNOWN ? "UNKNOWN" :
		     "???")))))));
      print_event_state (gdk_event_get_modifier_state (event));
      break;
    case GDK_FOCUS_CHANGE:
      g_print ("%s", (gdk_focus_event_get_in (event) ? "IN" : "OUT"));
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      g_print ("DND");
      break;
    case GDK_SCROLL:
      direction = gdk_scroll_event_get_direction (event);
      g_print (" %s ",
	       (direction == GDK_SCROLL_UP ? "UP" :
		(direction == GDK_SCROLL_DOWN ? "DOWN" :
		 (direction == GDK_SCROLL_LEFT ? "LEFT" :
		  (direction == GDK_SCROLL_RIGHT ? "RIGHT" :
		   "???")))));
      print_event_state (gdk_event_get_modifier_state (event));
      break;
    case GDK_GRAB_BROKEN:
      g_print ("Grab broken");
    default:
      /* Nothing */
      break;
    }
  g_print ("%s", (debug_indent == 0 ? "\n" : ""));
}

static char *
decode_key_lparam (LPARAM lParam)
{
  static char buf[100];
  char *p = buf;

  if (HIWORD (lParam) & KF_UP)
    p += g_sprintf (p, "KF_UP ");
  if (HIWORD (lParam) & KF_REPEAT)
    p += g_sprintf (p, "KF_REPEAT ");
  if (HIWORD (lParam) & KF_ALTDOWN)
    p += g_sprintf (p, "KF_ALTDOWN ");
  if (HIWORD (lParam) & KF_EXTENDED)
    p += g_sprintf (p, "KF_EXTENDED ");
  p += g_sprintf (p, "sc:%d rep:%d", LOBYTE (HIWORD (lParam)), LOWORD (lParam));

  return buf;
}

static void
fixup_event (GdkEvent *event)
{
  if (event->surface)
    g_object_ref (event->surface);
}

void
_gdk_win32_append_event (GdkEvent *event)
{
  GdkDisplay *display;
  GList *link;
  gulong serial;

  display = gdk_display_get_default ();

  fixup_event (event);
#if 1
  link = _gdk_event_queue_append (display, event);
  GDK_NOTE (EVENTS, _gdk_win32_print_event (event));
  serial = _gdk_display_get_next_serial (display);
  /* event morphing, the passed in may not be valid afterwards */
  _gdk_windowing_got_event (display, link, event, serial);
#else
  _gdk_event_queue_append (display, event);
  GDK_NOTE (EVENTS, _gdk_win32_print_event (event));
#endif
}

static GdkWin32MessageFilterReturn
apply_message_filters (GdkDisplay *display,
                       MSG        *msg,
                       int        *ret_valp,
                       GList     **filters)
{
  GdkWin32MessageFilterReturn result = GDK_WIN32_MESSAGE_FILTER_CONTINUE;
  GList *tmp_list;

  tmp_list = *filters;
  while (tmp_list)
    {
      GdkWin32MessageFilter *filter = (GdkWin32MessageFilter *) tmp_list->data;
      GList *node;

      if (filter->removed)
        {
          tmp_list = tmp_list->next;
          continue;
        }

      filter->ref_count++;
      result = filter->function (GDK_WIN32_DISPLAY (display), msg, ret_valp, filter->data);

      /* get the next node after running the function since the
         function may add or remove a next node */
      node = tmp_list;
      tmp_list = tmp_list->next;

      filter->ref_count--;
      if (filter->ref_count == 0)
        {
          *filters = g_list_remove_link (*filters, node);
          g_list_free_1 (node);
          g_free (filter);
        }

      if (result != GDK_WIN32_MESSAGE_FILTER_CONTINUE)
	break;
    }

  return result;
}

/*
 * On Windows, transient surfaces will not have their own taskbar entries.
 * Because of this, we must hide and restore groups of transients in both
 * directions.  That is, all transient children must be hidden or restored
 * with this surface, but if this surface’s transient owner also has a
 * transient owner then this surface’s transient owner must be hidden/restored
 * with this one.  And etc, up the chain until we hit an ancestor that has no
 * transient owner.
 *
 * It would be a good idea if applications don’t chain transient surfaces
 * together.  There’s a limit to how much evil GTK can try to shield you
 * from.
 */
static void
show_surface_recurse (GdkSurface *surface, 
                      gboolean    hide_surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GSList *children = impl->transient_children;
  GdkSurface *child = NULL;

  if (!impl->changing_state)
    {
      impl->changing_state = TRUE;

      if (children != NULL)
	{
	  while (children != NULL)
	    {
	      child = children->data;
	      show_surface_recurse (child, hide_surface);

	      children = children->next;
	    }
	}

      if (GDK_SURFACE_IS_MAPPED (surface))
	{
	  if (!hide_surface)
	    {
	      if (gdk_toplevel_get_state (GDK_TOPLEVEL (surface)) & GDK_TOPLEVEL_STATE_MINIMIZED)
		{
		  if (gdk_toplevel_get_state (GDK_TOPLEVEL (surface)) & GDK_TOPLEVEL_STATE_MAXIMIZED)
		    {
		      GtkShowSurfaceHWND (surface, SW_SHOWMAXIMIZED);
		    }
		  else
		    {
		      GtkShowSurfaceHWND (surface, SW_RESTORE);
		    }
		}
	    }
	  else
	    {
	      GtkShowSurfaceHWND (surface, SW_MINIMIZE);
	    }
	}

      impl->changing_state = FALSE;
    }
}

static void
do_show_surface (GdkSurface *surface, 
                 gboolean    hide_surface)
{
  GdkSurface *tmp_surface = NULL;
  GdkWin32Surface *tmp_impl = GDK_WIN32_SURFACE (surface);

  if (!tmp_impl->changing_state)
    {
      /* Find the top-level surface in our transient chain. */
      while (tmp_impl->transient_owner != NULL)
	{
	  tmp_surface = tmp_impl->transient_owner;
	  tmp_impl = GDK_WIN32_SURFACE (tmp_surface);
	}

      /* If we couldn't find one, use the surface provided. */
      if (tmp_surface == NULL)
	{
	  tmp_surface = surface;
	}

      /* Recursively show/hide every surface in the chain. */
      if (tmp_surface != surface)
	{
	  show_surface_recurse (tmp_surface, hide_surface);
	}
    }
}

static void
send_crossing_event (GdkDisplay      *display,
                     GdkDevice       *physical_device,
                     GdkSurface      *surface,
                     GdkEventType     type,
                     GdkCrossingMode  mode,
                     GdkNotifyType    notify_type,
                     GdkSurface      *subsurface,
                     POINT           *screen_pt,
                     GdkModifierType  mask,
                     guint32          time_)
{
  GdkEvent *event;
  GdkDeviceGrabInfo *grab;
  POINT pt;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  grab = _gdk_display_has_device_grab (display, GDK_WIN32_DISPLAY (display)->device_manager->core_pointer, 0);

  if (grab != NULL &&
      !grab->owner_events &&
      mode != GDK_CROSSING_UNGRAB)
    {
      /* !owner_event => only report events wrt grab surface, ignore rest */
      if (surface != grab->surface)
	return;
    }

  pt = *screen_pt;
  ScreenToClient (GDK_SURFACE_HWND (surface), &pt);

  _gdk_device_virtual_set_active (GDK_WIN32_DISPLAY (display)->device_manager->core_pointer, physical_device);

  event = gdk_crossing_event_new (type,
                                  surface,
                                  GDK_WIN32_DISPLAY (display)->device_manager->core_pointer,
                                  time_,
                                  mask,
                                  pt.x / impl->surface_scale,
                                  pt.y / impl->surface_scale,
                                  mode,
                                  notify_type);

  _gdk_win32_append_event (event);
}

static GdkSurface *
find_common_ancestor (GdkSurface *s1,
		              GdkSurface *s2)
{
  GdkSurface *tmp;
  GList *path1 = NULL, *path2 = NULL;
  GList *list1, *list2;

  tmp = s1;
  while (tmp != NULL)
    {
      path1 = g_list_prepend (path1, tmp);
      tmp = tmp->parent;
    }

  tmp = s2;
  while (tmp != NULL)
    {
      path2 = g_list_prepend (path2, tmp);
      tmp = tmp->parent;
    }

  list1 = path1;
  list2 = path2;
  tmp = NULL;
  while (list1 && list2 && (list1->data == list2->data))
    {
      tmp = (GdkSurface *)list1->data;
      list1 = list1->next;
      list2 = list2->next;
    }
  g_list_free (path1);
  g_list_free (path2);

  return tmp;
}

void
synthesize_crossing_events (GdkDisplay      *display,
                            GdkDevice       *physical_device,
                            GdkSurface      *src,
                            GdkSurface      *dest,
                            GdkCrossingMode  mode,
                            POINT           *screen_pt,
                            GdkModifierType  mask,
                            guint32          time_,
                            gboolean         non_linear)
{
  GdkSurface *c;
  GdkSurface *s, *last, *next;
  GList *path, *list;
  GdkSurface *a;
  GdkSurface *b;
  GdkNotifyType notify_type;

  a = src;
  b = dest;
  if (a == b)
    return; /* No crossings generated between src and dest */

  c = find_common_ancestor (a, b);

  non_linear |= (c != a) && (c != b);

  if (a) /* There might not be a source (i.e. if no previous pointer_in_window) */
    {
      /* Traverse up from a to (excluding) c sending leave events */
      if (non_linear)
	notify_type = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
	notify_type = GDK_NOTIFY_INFERIOR;
      else
	notify_type = GDK_NOTIFY_ANCESTOR;
      send_crossing_event (display,
                           physical_device,
			   a, GDK_LEAVE_NOTIFY,
			   mode,
			   notify_type,
			   NULL,
			   screen_pt,
			   mask, time_);

      if (c != a)
	{
	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  last = a;
	  s = a->parent;
	  while (s != c && s != NULL)
	    {
	      send_crossing_event (display,
                                   physical_device,
				   s, GDK_LEAVE_NOTIFY,
				   mode,
				   notify_type,
				   (GdkSurface *)last,
				   screen_pt,
				   mask, time_);

	      last = s;
	      s = s->parent;
	    }
	}
    }

  if (b) /* Might not be a dest, e.g. if we're moving out of the window */
    {
      /* Traverse down from c to b */
      if (c != b)
	{
	  path = NULL;
	  s = b->parent;
	  while (s != c && s != NULL)
	    {
	      path = g_list_prepend (path, s);
	      s = s->parent;
	    }

	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  list = path;
	  while (list)
	    {
	      s = (GdkSurface *)list->data;
	      list = list->next;
	      if (list)
		next = (GdkSurface *)list->data;
	      else
		next = b;

	      send_crossing_event (display,
                                   physical_device,
				   s, GDK_ENTER_NOTIFY,
				   mode,
				   notify_type,
				   next,
				   screen_pt,
				   mask, time_);
	    }
	  g_list_free (path);
	}


      if (non_linear)
	notify_type = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
	notify_type = GDK_NOTIFY_ANCESTOR;
      else
	notify_type = GDK_NOTIFY_INFERIOR;

      send_crossing_event (display,
                           physical_device,
			   b, GDK_ENTER_NOTIFY,
			   mode,
			   notify_type,
			   NULL,
			   screen_pt,
			   mask, time_);
    }
}

static void
make_crossing_event (GdkDevice *physical_device,
                     GdkSurface *surface,
                     POINT *screen_pt,
                     guint32 time_)
{
  GdkDisplay *display = surface != NULL ? gdk_surface_get_display (surface) : gdk_display_get_default ();

  GdkSurface *mouse_surface = GDK_WIN32_DISPLAY (display)->event_record->mouse_surface;
  GDK_NOTE (EVENTS, g_print (" mouse_surface %p -> %p",
                             mouse_surface ? GDK_SURFACE_HWND (mouse_surface) : NULL,
                             surface ? GDK_SURFACE_HWND (surface) : NULL));
  synthesize_crossing_events (display,
                              physical_device,
                              mouse_surface, surface,
                              GDK_CROSSING_NORMAL,
                              screen_pt,
                              0, /* TODO: Set right mask */
                              time_,
                              FALSE);
  g_set_object (&GDK_WIN32_DISPLAY (display)->event_record->mouse_surface, surface);
}

/* Acquires actual client area size of the underlying native surface HWND.
 * Returns FALSE if configure events should be inhibited,
 * TRUE otherwise.
 */
gboolean
gdk_win32_get_surface_hwnd_rect (GdkSurface *surface,
                                 RECT       *rect)
{
  RECT client_rect;
  POINT point;
  HWND hwnd;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  hwnd = GDK_SURFACE_HWND (surface);

  GetClientRect (hwnd, &client_rect);
  point.x = client_rect.left; /* always 0 */
  point.y = client_rect.top;

  /* top level surfaces need screen coords */
  if (GDK_IS_TOPLEVEL (surface))
    ClientToScreen (hwnd, &point);

  rect->left = point.x;
  rect->top = point.y;
  rect->right = point.x + client_rect.right - client_rect.left;
  rect->bottom = point.y + client_rect.bottom - client_rect.top;

  return !impl->inhibit_configure;
}

cairo_region_t *
_gdk_win32_hrgn_to_region (HRGN  hrgn,
                           guint scale)
{
  RGNDATA *rgndata;
  RECT *rects;
  cairo_region_t *result;
  int nbytes;
  guint i;

  if ((nbytes = GetRegionData (hrgn, 0, NULL)) == 0)
    {
      WIN32_GDI_FAILED ("GetRegionData");
      return NULL;
    }

  rgndata = (RGNDATA *) g_malloc (nbytes);

  if (GetRegionData (hrgn, nbytes, rgndata) == 0)
    {
      WIN32_GDI_FAILED ("GetRegionData");
      g_free (rgndata);
      return NULL;
    }

  result = cairo_region_create ();
  rects = (RECT *) rgndata->Buffer;
  for (i = 0; i < rgndata->rdh.nCount; i++)
    {
      GdkRectangle r;

      r.x = rects[i].left;
      r.y = rects[i].top;
      r.width = (rects[i].right - r.x) / scale;
      r.height = (rects[i].bottom - r.y) / scale;

      cairo_region_union_rectangle (result, &r);
    }

  g_free (rgndata);

  return result;
}

static void
handle_wm_paint (MSG        *msg,
                 GdkSurface *surface)
{
  HRGN hrgn = CreateRectRgn (0, 0, 0, 0);
  HDC hdc;
  PAINTSTRUCT paintstruct;
  cairo_region_t *update_region;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  if (GetUpdateRgn (msg->hwnd, hrgn, FALSE) == ERROR)
    {
      WIN32_GDI_FAILED ("GetUpdateRgn");
      DeleteObject (hrgn);
      return;
    }

  hdc = BeginPaint (msg->hwnd, &paintstruct);

  GDK_NOTE (EVENTS, g_print (" %s %s dc %p",
			     _gdk_win32_rect_to_string (&paintstruct.rcPaint),
			     (paintstruct.fErase ? "erase" : ""),
			     hdc));

  EndPaint (msg->hwnd, &paintstruct);

  if ((paintstruct.rcPaint.right == paintstruct.rcPaint.left) ||
      (paintstruct.rcPaint.bottom == paintstruct.rcPaint.top))
    {
      GDK_NOTE (EVENTS, g_print (" (empty paintstruct, ignored)"));
      DeleteObject (hrgn);
      return;
    }

  update_region = _gdk_win32_hrgn_to_region (hrgn, impl->surface_scale);
  if (!cairo_region_is_empty (update_region))
    gdk_surface_invalidate_region (surface, update_region);
  cairo_region_destroy (update_region);

  DeleteObject (hrgn);
}

static VOID CALLBACK
modal_timer_proc (HWND     hwnd,
		  UINT     msg,
		  UINT_PTR id,
		  DWORD    time)
{
  int arbitrary_limit = 10;

  while (g_main_context_pending (NULL) && arbitrary_limit--)
    g_main_context_iteration (NULL, FALSE);
}

static void
_gdk_win32_begin_modal_call (GdkSurface          *surface,
                             GdkWin32ModalOpKind  kind)
{
  GdkWin32Display *display = GDK_WIN32_DISPLAY (gdk_surface_get_display (surface));
  GdkWin32ModalOpKind was = display->display_surface_record->modal_operation_in_progress;
  g_assert (!(was & kind));

  display->display_surface_record->modal_operation_in_progress |= kind;

  if (was == GDK_WIN32_MODAL_OP_NONE)
    {
      UINT modal_timer;
      modal_timer = SetTimer (NULL, (UINT_PTR)surface, 10, modal_timer_proc);

      if (modal_timer == 0)
	    WIN32_API_FAILED ("SetTimer");
      else
        display->display_surface_record->modal_timer = modal_timer;
    }
}

static void
_gdk_win32_end_modal_call (GdkSurface          *surface,
                           GdkWin32ModalOpKind  kind)
{
  GdkWin32Display *display = GDK_WIN32_DISPLAY (gdk_surface_get_display (surface));
  g_assert (display->display_surface_record->modal_operation_in_progress & kind);

  display->display_surface_record->modal_operation_in_progress &= ~kind;

  if (display->display_surface_record->modal_operation_in_progress == GDK_WIN32_MODAL_OP_NONE &&
      display->display_surface_record->modal_timer != 0)
    {
      API_CALL (KillTimer, (NULL, display->display_surface_record->modal_timer));
      display->display_surface_record->modal_timer = 0;
    }
}

static gboolean
handle_nchittest (HWND hwnd,
                  GdkSurface *surface,
                  gint16 screen_x,
                  gint16 screen_y,
                  int *ret_valp)
{
  GdkWin32Surface *impl;
  RECT client_rect;
  POINT client_pt;

  if (surface == NULL)
    return FALSE;

  /* If the surface has no particular input pass-through region,
   * then we can simply let DefWindowProc() handle the message */
  if (surface->input_region == NULL)
    return FALSE;

  if (!GetClientRect (hwnd, &client_rect))
    return FALSE;

  client_pt.x = screen_x;
  client_pt.y = screen_y;
  if (!ScreenToClient (hwnd, &client_pt))
    return FALSE;

  /* Check whether the point lies within the client area */
  if (!PtInRect (&client_rect, client_pt))
    return FALSE;

  impl = GDK_WIN32_SURFACE (surface);

  /* If the point lies inside the input region, return HTCLIENT,
   * otherwise return HTTRANSPARENT. */
  if (cairo_region_contains_point (surface->input_region,
                                   client_pt.x / impl->surface_scale,
                                   client_pt.y / impl->surface_scale))
    *ret_valp = HTCLIENT;
  else
    *ret_valp = HTTRANSPARENT;

  /* We handled the message, no need to call DefWindowProc() */
  return TRUE;
}

static void
handle_dpi_changed (GdkSurface *surface,
                    MSG        *msg)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  RECT *rect = (RECT *)msg->lParam;
  guint old_scale = impl->surface_scale;

  /* MSDN for WM_DPICHANGED: dpi_x == dpi_y here, so LOWORD (msg->wParam) == HIWORD (msg->wParam) */
  guint dpi = LOWORD (msg->wParam);

  impl->surface_scale = dpi / USER_DEFAULT_SCREEN_DPI;

  /* Don't bother if scales did not change in the end */
  if (old_scale == impl->surface_scale)
    return;

  if (!IsIconic (msg->hwnd) &&
      !GDK_SURFACE_DESTROYED (surface))
    {
      GdkMonitor *monitor;

      monitor = gdk_display_get_monitor_at_surface (display, surface);
      gdk_monitor_set_scale_factor (monitor, impl->surface_scale);
    }

  _gdk_win32_adjust_client_rect (surface, rect);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_move_resize (surface,
                                   surface->x, surface->y,
                                   surface->width, surface->height);
  else
    gdk_win32_surface_resize (surface, surface->width, surface->height);
}

static void
generate_button_event (GdkEventType  type,
                       int           button,
                       GdkSurface   *surface,
                       MSG          *msg)
{
  GdkEvent *event;
  GdkDeviceManagerWin32 *device_manager;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  double x, y;

  if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->pointer_device_items->input_ignore_core > 0)
    return;

  device_manager = GDK_DEVICE_MANAGER_WIN32 (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->device_manager);

  x = (double) GET_X_LPARAM (msg->lParam) / impl->surface_scale;
  y = (double) GET_Y_LPARAM (msg->lParam) / impl->surface_scale;

  _gdk_device_virtual_set_active (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->device_manager->core_pointer,
                                  GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->device_manager->system_pointer);

  event = gdk_button_event_new (type,
                                surface,
                                device_manager->core_pointer,
                                NULL,
                                _gdk_win32_get_next_tick (msg->time),
                                build_pointer_event_state (msg),
                                button,
                                x,
                                y,
                                NULL);

  _gdk_win32_append_event (event);
}

static gboolean
handle_wm_sysmenu (GdkSurface *surface,
                   MSG        *msg,
                   int *ret_valp)
{
  GdkWin32Surface *impl;
  LONG_PTR style, tmp_style;
  LONG_PTR additional_styles;

  impl = GDK_WIN32_SURFACE (surface);

  style = GetWindowLongPtr (msg->hwnd, GWL_STYLE);

  additional_styles = 0;

  if (!(style & WS_SYSMENU))
    additional_styles |= WS_SYSMENU;

  if (!(style & WS_MAXIMIZEBOX))
    additional_styles |= WS_MAXIMIZEBOX;

  if (!(style & WS_MINIMIZEBOX))
    additional_styles |= WS_MINIMIZEBOX;

  if (!(style & WS_SIZEBOX))
    additional_styles |= WS_SIZEBOX;

  if (!(style & WS_DLGFRAME))
    additional_styles |= WS_DLGFRAME;

  if (!(style & WS_BORDER))
    additional_styles |= WS_BORDER;

  if (additional_styles == 0)
    /* The caller will eventually pass this to DefWindowProc (),
     * only without the style dance, which isn't needed, as it turns out.
     */
    return FALSE;

  /* Note: This code will enable resizing, maximizing and minimizing surfaces
   * via window menu even if these are non-CSD windows that were explicitly
   * forbidden from doing this by removing the appropriate styles,
   * or if these are CSD windows that were explicitly forbidden from doing
   * this by removing appropriate decorations from the headerbar and/or
   * changing hints or properties.
   *
   * If doing this for non-CSD windows is not desired,
   * do a _gdk_win32_surface_lacks_wm_decorations() check and return FALSE
   * if it doesn't pass.
   *
   * If doing this for CSD windows with disabled decorations is not desired,
   * tough luck - GDK can't know which CSD decorations are enabled, and which
   * are not.
   *
   * If doing this for CSD windows with particular hints is not desired,
   * check surface hints here and return FALSE (DefWindowProc() will return
   * FALSE later) or set *ret_valp to 0 and return TRUE.
   */
  tmp_style = style | additional_styles;
  GDK_NOTE (EVENTS, g_print (" Handling WM_SYSMENU: style 0x%" G_GINTPTR_MODIFIER "x -> 0x%" G_GINTPTR_MODIFIER "x\n", style, tmp_style));
  impl->have_temp_styles = TRUE;
  impl->temp_styles = additional_styles;
  SetWindowLongPtr (msg->hwnd, GWL_STYLE, tmp_style);

  *ret_valp = DefWindowProc (msg->hwnd, msg->message, msg->wParam, msg->lParam);

  tmp_style = GetWindowLongPtr (msg->hwnd, GWL_STYLE);
  style = tmp_style & ~additional_styles;

  GDK_NOTE (EVENTS, g_print (" Handling WM_SYSMENU: style 0x%" G_GINTPTR_MODIFIER "x <- 0x%" G_GINTPTR_MODIFIER "x\n", style, tmp_style));
  SetWindowLongPtr (msg->hwnd, GWL_STYLE, style);
  impl->have_temp_styles = FALSE;

  return TRUE;
}

gboolean
_gdk_win32_surface_fill_min_max_info (GdkSurface *surface,
                                      MINMAXINFO *mmi)
{
  GdkWin32Surface *impl;
  RECT rect;

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  impl = GDK_WIN32_SURFACE (surface);

  if (impl->hint_flags & GDK_HINT_MIN_SIZE)
    {
      rect.left = rect.top = 0;
      rect.right = impl->hints.min_width * impl->surface_scale;
      rect.bottom = impl->hints.min_height * impl->surface_scale;

      _gdk_win32_adjust_client_rect (surface, &rect);

      mmi->ptMinTrackSize.x = rect.right - rect.left;
      mmi->ptMinTrackSize.y = rect.bottom - rect.top;
    }

  if (impl->hint_flags & GDK_HINT_MAX_SIZE)
    {
      int maxw, maxh;

      rect.left = rect.top = 0;
      rect.right = impl->hints.max_width * impl->surface_scale;
      rect.bottom = impl->hints.max_height * impl->surface_scale;

      _gdk_win32_adjust_client_rect (surface, &rect);

      /* at least on win9x we have the 16 bit trouble */
      maxw = rect.right - rect.left;
      maxh = rect.bottom - rect.top;
      mmi->ptMaxTrackSize.x = maxw > 0 && maxw < G_MAXSHORT ? maxw : G_MAXSHORT;
      mmi->ptMaxTrackSize.y = maxh > 0 && maxh < G_MAXSHORT ? maxh : G_MAXSHORT;
    }
  else
    {
      /* According to "How does the window manager adjust ptMaxSize and
       * ptMaxPosition for multiple monitors?" article
       * https://blogs.msdn.microsoft.com/oldnewthing/20150501-00/?p=44964
       * if ptMaxSize >= primary_monitor_size, then it will be adjusted by
       * WM to account for the monitor size differences if the window gets
       * maximized on a non-primary monitor, by simply adding the size
       * difference (i.e. if non-primary monitor is larger by 100px, then
       * window will be made larger exactly by 100px).
       * If ptMaxSize < primary_monitor_size at least in one direction,
       * nothing is adjusted.
       * Therefore, if primary monitor is smaller than the actual monitor,
       * then it is not possible to give window a size that is larger than
       * the primary monitor and smaller than the non-primary monitor,
       * because WM will always enlarge the window.
       * Therefore, it is impossible to account for taskbar size.
       * So we don't try at all. Instead we just remember that we're trying
       * to maximize the window, catch WM_WINDOWPOSCHANGING and
       * adjust the size then.
       */
      HMONITOR nearest_monitor;
      MONITORINFO nearest_info;

      nearest_monitor = MonitorFromWindow (GDK_SURFACE_HWND (surface), MONITOR_DEFAULTTONEAREST);
      nearest_info.cbSize = sizeof (nearest_info);

      if (GetMonitorInfo (nearest_monitor, &nearest_info))
        {
          /* MSDN says that we must specify maximized window
           * size as if it was located on the primary monitor.
           * However, we still need to account for a taskbar
           * that might or might not be on the nearest monitor where
           * window will actually end up.
           * "0" here is the top-left corner of the primary monitor.
           */
          /* An investigation into bug 765161 turned up a weird Windows WM behaviour
           * where it would interpret "0:0" as "top-left of the workea" (accounting for a taskbar
           * possibly being along the left/top edge of the screen) when window has styles
           * (i.e. not CSD), and interpret the same "0:0" as "top-left of the screen" (not
           * accounting for a taskbar) when window has no styles (i.e. a CSD window).
           * This doesn't seem to be documented anywhere.
           * The following code uses a simple CSD/non-CSD test, but it could be that
           * this behaviour hinges on just one particular window style.
           * Finding exactly which style that could be is not very useful for GTK, however.
           */
          mmi->ptMaxPosition.x = 0;
          mmi->ptMaxPosition.y = 0;

          if (_gdk_win32_surface_lacks_wm_decorations (surface))
            {
              mmi->ptMaxPosition.x += (nearest_info.rcWork.left - nearest_info.rcMonitor.left);
              mmi->ptMaxPosition.y += (nearest_info.rcWork.top - nearest_info.rcMonitor.top);
            }

          mmi->ptMaxSize.x = nearest_info.rcWork.right - nearest_info.rcWork.left;
          mmi->ptMaxSize.y = nearest_info.rcWork.bottom - nearest_info.rcWork.top;
        }

      mmi->ptMaxTrackSize.x = GetSystemMetrics (SM_CXVIRTUALSCREEN) + (impl->shadow.left + impl->shadow.right) * impl->surface_scale;
      mmi->ptMaxTrackSize.y = GetSystemMetrics (SM_CYVIRTUALSCREEN) + (impl->shadow.left + impl->shadow.right) * impl->surface_scale;
    }

  return TRUE;
}

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
			     GDK_BUTTON2_MASK | \
			     GDK_BUTTON3_MASK | \
			     GDK_BUTTON4_MASK | \
			     GDK_BUTTON5_MASK)

static gboolean
gdk_event_translate (MSG *msg,
		     int *ret_valp)
{
  RECT rect;
  POINT point;
  MINMAXINFO *mmi;
  HWND hwnd;
  HIMC himc;
  WINDOWPOS *hwndpos;
  gboolean ignore_leave;

  GdkEvent *event;

  GdkDisplay *display;
  GdkSurface *surface = NULL;
  GdkWin32Surface *impl;
  GdkWin32Display *win32_display;

  GdkSurface *new_surface;

  GdkDeviceGrabInfo *keyboard_grab = NULL;
  GdkDeviceGrabInfo *pointer_grab = NULL;
  GdkSurface *grab_surface = NULL;

  crossing_cb_t crossing_cb = NULL;

  int button;

  gboolean return_val = FALSE;

  int i;

  display = gdk_display_get_default ();
  win32_display = GDK_WIN32_DISPLAY (display);

  if (win32_display->filters)
    {
      /* Apply display filters */
      GdkWin32MessageFilterReturn result = apply_message_filters (display, msg, ret_valp, &win32_display->filters);

      if (result == GDK_WIN32_MESSAGE_FILTER_REMOVE)
	return TRUE;
    }

  surface = gdk_win32_display_handle_table_lookup_ (display, msg->hwnd);

  if (surface == NULL)
    {
      /* XXX Handle WM_QUIT here ? */
      if (msg->message == WM_QUIT)
	      {
	        GDK_NOTE (EVENTS, g_print (" %d", (int) msg->wParam));
	        exit (msg->wParam);
	      }
      else if (msg->message == WM_CREATE)
	      {
	        surface = (GdkSurface*) (((LPCREATESTRUCTW) msg->lParam)->lpCreateParams);
      	  GDK_SURFACE_HWND (surface) = msg->hwnd;
	      }
      else
	      {
	        GDK_NOTE (EVENTS, g_print (" (no GdkSurface)"));
	      }
      return FALSE;
    }

  keyboard_grab = _gdk_display_get_last_device_grab (display,
                                                     win32_display->device_manager->core_keyboard);
  pointer_grab = _gdk_display_get_last_device_grab (display,
                                                    win32_display->device_manager->core_pointer);

  g_object_ref (surface);

  /* surface's refcount has now been increased, so code below should
   * not just return from this function, but instead goto done (or
   * break out of the big switch). To protect against forgetting this,
   * #define return to a syntax error...
   */
#define return GOTO_DONE_INSTEAD

  switch (msg->message)
    {
    case WM_INPUTLANGCHANGE:
      {
        GdkWin32Keymap *win32_keymap;
        GdkTranslatedKey translated;
        HKL input_locale;

        win32_keymap = GDK_WIN32_KEYMAP (gdk_display_get_keymap (display));

        input_locale = (HKL) msg->lParam;
        gdk_win32_display_set_input_locale (win32_display, input_locale);
        _gdk_win32_keymap_set_active_layout (win32_keymap, input_locale);
        gdk_win32_display_increment_keymap_serial (win32_display);
        GDK_NOTE (EVENTS,
                  g_print (" cs:%lu hkl:%p%s",
                           (gulong) msg->wParam,
                           (gpointer) msg->lParam,
                           gdk_win32_display_input_locale_is_ime (win32_display) ? " (IME)" : ""));
        gdk_display_setting_changed (display, "gtk-im-module");

        /* Generate a dummy key event to "nudge" IMContext */
        translated.keyval = GDK_KEY_VoidSymbol;
        translated.consumed = 0;
        translated.layout = 0;
        translated.level = 0;
        event = gdk_key_event_new (GDK_KEY_PRESS,
                                   surface,
                                   win32_display->device_manager->core_keyboard,
                                   _gdk_win32_get_next_tick (msg->time),
                                   0,
                                   0,
                                   FALSE,
                                   &translated,
                                   &translated,
                                   NULL);
        _gdk_win32_append_event (event);
      }
      break;

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
      GDK_NOTE (EVENTS,
		g_print (" %s ch:%.02x %s",
			 _gdk_win32_key_to_string (msg->lParam),
			 (int) msg->wParam,
			 decode_key_lparam (msg->lParam)));

      /* If posted without us having keyboard focus, ignore */
      if ((msg->wParam != VK_F10 && msg->wParam != VK_MENU) &&
	  !(HIWORD (msg->lParam) & KF_ALTDOWN))
	break;

      /* Let the system handle Alt-Tab, Alt-Space and Alt-F4 unless
       * the keyboard is grabbed.
       */
      if (!keyboard_grab &&
	  (msg->wParam == VK_TAB ||
	   msg->wParam == VK_SPACE ||
	   msg->wParam == VK_F4))
	break;

      /* Jump to code in common with WM_KEYUP and WM_KEYDOWN */
      goto keyup_or_down;

    case WM_KEYUP:
    case WM_KEYDOWN:
      GDK_NOTE (EVENTS,
		g_print (" %s ch:%.02x %s",
			 _gdk_win32_key_to_string (msg->lParam),
			 (int) msg->wParam,
			 decode_key_lparam (msg->lParam)));

    keyup_or_down:
      {
        GdkWin32Keymap *win32_keymap;
        GdkModifierType state;
        guint keyval;
        guint16 keycode;
        guint8 group;
        gboolean is_modifier;
        GdkTranslatedKey translated;
        GdkTranslatedKey no_lock;
        BYTE key_state[256];
        GArray *translation;
        MSG msg2;
        int level = 0;
        int effective_group = 0;
        GdkModifierType consumed = 0;
        char *composed = NULL;

        /* Ignore key messages intended for the IME */
        if (msg->wParam == VK_PROCESSKEY || win32_display->event_record->in_ime_composition)
          break;

        /* Ignore autorepeats on modifiers */
        if (msg->message == WM_KEYDOWN &&
            (msg->wParam == VK_MENU ||
             msg->wParam == VK_CONTROL ||
             msg->wParam == VK_SHIFT) &&
             ((HIWORD(msg->lParam) & KF_REPEAT) >= 1))
          break;

        if (GDK_SURFACE_DESTROYED (surface))
          break;

        win32_keymap = GDK_WIN32_KEYMAP (gdk_display_get_keymap (display));
        impl = GDK_WIN32_SURFACE (surface);

        API_CALL (GetKeyboardState, (key_state));

        keyval = GDK_KEY_VoidSymbol;
        keycode = msg->wParam;

        /* Get the WinAPI translation of the WM_KEY messages to characters.

           The WM_CHAR messages are generated by a previous call to TranslateMessage() and always
           follow directly after the corresponding WM_KEY* messages.
           There could be 0 or more WM_CHAR messages following (for example dead keys don't generate
           WM_CHAR messages - they generate WM_DEAD_CHAR instead, but we are not interested in those
           messages). */

        translation = g_array_sized_new (FALSE, FALSE, sizeof (gunichar2), 2);
        while (PeekMessageW (&msg2, msg->hwnd, 0, 0, 0) && (msg2.message == WM_CHAR || msg2.message == WM_SYSCHAR))
          {
            /* The character is encoded in WPARAM as UTF-16. */
            gunichar2 c = msg2.wParam;

            /* Append character to translation string. */
            g_array_append_val (translation, c);

            /* Remove message from queue */
            GetMessageW (&msg2, msg->hwnd, 0, 0);
          }

        if (translation->len > 0)
          composed = g_utf16_to_utf8 ((gunichar2*)translation->data,
                                      translation->len, NULL, NULL, NULL);

        g_array_unref (translation);
        translation = NULL;

        /* Ignore control sequences like Backspace */
        if (composed && g_unichar_iscntrl (g_utf8_get_char (composed)))
          g_clear_pointer (&composed, g_free);

        if (HIWORD (msg->lParam) & KF_EXTENDED)
          {
            switch (msg->wParam)
              {
              case VK_CONTROL:
                keycode = VK_RCONTROL;
                break;
              case VK_SHIFT:      /* Actually, KF_EXTENDED is not set
                                   * for the right shift key.
                                   */
                keycode = VK_RSHIFT;
                break;
              case VK_MENU:
                keycode = VK_RMENU;
                break;
              }
          }
        else if (msg->wParam == VK_SHIFT &&
                 LOBYTE (HIWORD (msg->lParam)) == _gdk_win32_keymap_get_rshift_scancode (win32_keymap))
          keycode = VK_RSHIFT;

        is_modifier = (msg->wParam == VK_CONTROL ||
                       msg->wParam == VK_SHIFT ||
                       msg->wParam == VK_MENU);

        state = build_key_event_state (display, key_state);
        group = get_active_group (display);

        gdk_keymap_translate_keyboard_state ((GdkKeymap*) win32_keymap, keycode, state, group,
                                             &keyval, &effective_group, &level, &consumed);
        translated.keyval = keyval;
        translated.consumed = consumed;
        translated.layout = effective_group;
        translated.level = level;

        gdk_keymap_translate_keyboard_state ((GdkKeymap*) win32_keymap, keycode,
                                             state & ~GDK_LOCK_MASK, group, &keyval,
                                             &effective_group, &level, &consumed);
        no_lock.keyval = keyval;
        no_lock.consumed = consumed;
        no_lock.layout = effective_group;
        no_lock.level = level;

        /* Only one release key event is fired when both shift keys are pressed together
           and then released. In order to send the missing event, press events for shift
           keys are recorded and sent together when the release event occurs.
           Other modifiers (e.g. ctrl, alt) don't have this problem. */
        if (msg->message == WM_KEYDOWN && msg->wParam == VK_SHIFT)
          {
            int pressed_shift = msg->lParam & 0xffffff; /* mask shift modifier */
            if (win32_display->event_record->both_shift_pressed[0] == 0)
              win32_display->event_record->both_shift_pressed[0] = pressed_shift;
            else if (win32_display->event_record->both_shift_pressed[0] != pressed_shift)
              win32_display->event_record->both_shift_pressed[1] = pressed_shift;
          }

        if (msg->message == WM_KEYUP && msg->wParam == VK_SHIFT)
          {
            if (win32_display->event_record->both_shift_pressed[0] != 0 &&
                win32_display->event_record->both_shift_pressed[1] != 0)
              {
                int tmp_retval;
                MSG fake_release = *msg;
                int pressed_shift = msg->lParam & 0xffffff;

                if (win32_display->event_record->both_shift_pressed[0] == pressed_shift)
                  fake_release.lParam = win32_display->event_record->both_shift_pressed[1];
                else
                  fake_release.lParam = win32_display->event_record->both_shift_pressed[0];

                win32_display->event_record->both_shift_pressed[0] = win32_display->event_record->both_shift_pressed[1] = 0;
                gdk_event_translate (&fake_release, &tmp_retval);
              }
            win32_display->event_record->both_shift_pressed[0] = win32_display->event_record->both_shift_pressed[1] = 0;
          }

        /* Reset ALT_MASK if it is the Alt key itself */
        if (msg->wParam == VK_MENU)
          state &= ~GDK_ALT_MASK;

        event = gdk_key_event_new ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN)
                                     ? GDK_KEY_PRESS
                                     : GDK_KEY_RELEASE,
                                   surface,
                                   win32_display->device_manager->core_keyboard,
                                   _gdk_win32_get_next_tick (msg->time),
                                   keycode,
                                   state,
                                   is_modifier,
                                   &translated,
                                   &no_lock,
                                   composed);

        _gdk_win32_append_event (event);

        g_free (composed);
        return_val = TRUE;
      }
      break;

    case WM_SYSCHAR:
      if (msg->wParam != VK_SPACE)
	{
	  /* To prevent beeps, don't let DefWindowProcW() be called */
	  return_val = TRUE;
	  goto done;
	}
      break;

    case WM_IME_STARTCOMPOSITION:
      win32_display->event_record->in_ime_composition = TRUE;
      break;

    case WM_IME_ENDCOMPOSITION:
      win32_display->event_record->in_ime_composition = FALSE;
      break;

    case WM_IME_COMPOSITION:
      {
        BYTE key_state[256];
        wchar_t wbuf[100];
        int ccount = 0;

        /* On Win2k WM_IME_CHAR doesn't work correctly for non-Unicode
         * applications. Thus, handle WM_IME_COMPOSITION with
         * GCS_RESULTSTR instead, fetch the Unicode chars from the IME
         * with ImmGetCompositionStringW().
         *
         * See for instance
         * http://groups.google.com/groups?selm=natX5.57%24g77.19788%40nntp2.onemain.com
         * and
         * http://groups.google.com/groups?selm=u2XfrXw5BHA.1628%40tkmsftngp02
         * for comments by other people that seems to have the same
         * experience. WM_IME_CHAR just gives question marks, apparently
         * because of going through some conversion to the current code
         * page.
         *
         * WM_IME_CHAR might work on NT4 or Win9x with ActiveIMM, but
         * use WM_IME_COMPOSITION there, too, to simplify the code.
         */
        GDK_NOTE (EVENTS, g_print (" %#lx", (long) msg->lParam));

        if (!(msg->lParam & GCS_RESULTSTR))
          break;

        if (GDK_SURFACE_DESTROYED (surface))
          break;

        himc = ImmGetContext (msg->hwnd);
        ccount = ImmGetCompositionStringW (himc, GCS_RESULTSTR,
                                           wbuf, sizeof (wbuf));
        ImmReleaseContext (msg->hwnd, himc);

        ccount /= 2;

        API_CALL (GetKeyboardState, (key_state));

        for (i = 0; i < ccount; i++)
          {
            GdkTranslatedKey translated;

            /* Build a key press event */
            translated.keyval = gdk_unicode_to_keyval (wbuf[i]);
            translated.consumed = 0;
            translated.layout = get_active_group (display);
            translated.level = 0;
            event = gdk_key_event_new (GDK_KEY_PRESS,
                                       surface,
                                       win32_display->device_manager->core_keyboard,
                                       _gdk_win32_get_next_tick (msg->time),
                                       0,
                                       build_key_event_state (display, key_state),
                                       FALSE,
                                       &translated,
                                       &translated,
                                       NULL);

            _gdk_win32_append_event (event);

            /* Build a key release event.  */
            event = gdk_key_event_new (GDK_KEY_RELEASE,
                                       surface,
                                       win32_display->device_manager->core_keyboard,
                                       _gdk_win32_get_next_tick (msg->time),
                                       0,
                                       build_key_event_state (display, key_state),
                                       FALSE,
                                       &translated,
                                       &translated,
                                       NULL);

            _gdk_win32_append_event (event);
          }

        return_val = TRUE;
      }
      break;

    case WM_LBUTTONDOWN:
      button = 1;
      goto buttondown0;

    case WM_MBUTTONDOWN:
      button = 2;
      goto buttondown0;

    case WM_RBUTTONDOWN:
      button = 3;
      goto buttondown0;

    case WM_XBUTTONDOWN:
      if (HIWORD (msg->wParam) == XBUTTON1)
	button = 4;
      else
	button = 5;

    buttondown0:
      GDK_NOTE (EVENTS,
		g_print (" (%d,%d)",
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      win32_display->device_manager->pen_touch_input = FALSE;

      g_set_object (&surface, find_surface_for_mouse_event (surface, msg));
      /* TODO_CSW?: there used to some synthesize and propagate */
      if (GDK_SURFACE_DESTROYED (surface))
	break;

      if (pointer_grab == NULL)
	{
	  SetCapture (GDK_SURFACE_HWND (surface));
	}

      generate_button_event (GDK_BUTTON_PRESS, button,
			     surface, msg);

      *ret_valp = (msg->message == WM_XBUTTONDOWN ? TRUE : 0);
      return_val = TRUE;
      break;

    case WM_LBUTTONUP:
      button = 1;
      goto buttonup0;

    case WM_MBUTTONUP:
      button = 2;
      goto buttonup0;

    case WM_RBUTTONUP:
      button = 3;
      goto buttonup0;

    case WM_XBUTTONUP:
      if (HIWORD (msg->wParam) == XBUTTON1)
	button = 4;
      else
	button = 5;

    buttonup0:
    {
      gboolean release_implicit_grab = FALSE;
      GdkSurface *prev_surface = NULL;

      GDK_NOTE (EVENTS,
		g_print (" (%d,%d)",
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      win32_display->device_manager->pen_touch_input = FALSE;

      g_set_object (&surface, find_surface_for_mouse_event (surface, msg));

      if (pointer_grab != NULL && pointer_grab->implicit)
        {
          int state = build_pointer_event_state (msg);

          /* We keep the implicit grab until no buttons at all are held down */
          if ((state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (button - 1))) == 0)
            {
              release_implicit_grab = TRUE;
              prev_surface = pointer_grab->surface;
            }
        }

      generate_button_event (GDK_BUTTON_RELEASE, button, surface, msg);

      impl = GDK_WIN32_SURFACE (surface);

      /* End a drag op when the same button that started it is released */
      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE &&
          impl->drag_move_resize_context.button == button)
        gdk_win32_surface_end_move_resize_drag (surface);

      if (release_implicit_grab)
        {
          ReleaseCapture ();

          new_surface = NULL;
          hwnd = WindowFromPoint (msg->pt);
          if (hwnd != NULL)
            {
              POINT client_pt = msg->pt;

              ScreenToClient (hwnd, &client_pt);
              GetClientRect (hwnd, &rect);
              if (PtInRect (&rect, client_pt))
                new_surface = gdk_win32_display_handle_table_lookup_ (display, hwnd);
            }

          synthesize_crossing_events (display,
                                      win32_display->device_manager->system_pointer,
                                      prev_surface, new_surface,
                                      GDK_CROSSING_UNGRAB,
                                      &msg->pt,
                                      0, /* TODO: Set right mask */
                                      _gdk_win32_get_next_tick (msg->time),
                                     FALSE);
          g_set_object (&win32_display->event_record->mouse_surface, new_surface);
          win32_display->event_record->mouse_surface_ignored_leave = NULL;
        }

      *ret_valp = (msg->message == WM_XBUTTONUP ? TRUE : 0);
      return_val = TRUE;
      break;
    }

    case WM_MOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print (" %p (%d,%d)",
			 (gpointer) msg->wParam,
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      /* Even if we handle WM_POINTER messages, synthetic WM_MOUSEMOVE messages
       * are still sent occasionally by the OS, e.g. when a surface is hidden
       * or shown. Discard spurious WM_MOUSEMOVE messages while handling pen or
       * touch input
       *
       * See the article
       * "Why do I get spurious WM_MOUSEMOVE messages?" by Raymond Chen:
       * https://devblogs.microsoft.com/oldnewthing/20031001-00/?p=42343
       *
       */
      if (win32_display->tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINPOINTER &&
          ((msg->time - win32_display->device_manager->last_digitizer_time) < 200 ||
           (win32_display->device_manager->last_digitizer_time - msg->time) < 200 ))
        break;

      win32_display->device_manager->pen_touch_input = FALSE;

      g_set_object (&surface, find_surface_for_mouse_event (surface, msg));

      if (win32_display->event_record->mouse_surface != surface)
        {
          GdkSurface *mouse_surface = win32_display->event_record->mouse_surface;

          GDK_NOTE (EVENTS, g_print (" mouse_surface %p -> %p",
                    mouse_surface ? GDK_SURFACE_HWND (mouse_surface) : NULL,
                    surface ? GDK_SURFACE_HWND (surface) : NULL));
          synthesize_crossing_events (display,
                                      win32_display->device_manager->system_pointer,
                                      mouse_surface, surface,
                                      GDK_CROSSING_NORMAL,
                                     &msg->pt,
                                      0, /* TODO: Set right mask */
                                      _gdk_win32_get_next_tick (msg->time),
                                      FALSE);
          g_set_object (&win32_display->event_record->mouse_surface, surface);
          win32_display->event_record->mouse_surface_ignored_leave = NULL;
          if (surface != NULL)
            track_mouse_event (TME_LEAVE, GDK_SURFACE_HWND (surface));
        }
      else if (surface != NULL && surface == win32_display->event_record->mouse_surface_ignored_leave)
        {
           /* If we ignored a leave event for this surface and we're now getting
              input again we need to re-arm the mouse tracking, as that was
              cancelled by the mouseleave. */
          win32_display->event_record->mouse_surface_ignored_leave = NULL;
          track_mouse_event (TME_LEAVE, GDK_SURFACE_HWND (surface));
        }

      impl = GDK_WIN32_SURFACE (surface);

      /* If we haven't moved, don't create any GDK event. Windows
       * sends WM_MOUSEMOVE messages after a new surface is shown under
       * the mouse, even if the mouse hasn't moved. This disturbs gtk.
       */
      if (msg->pt.x == win32_display->event_record->current_root_x &&
          msg->pt.y == win32_display->event_record->current_root_y)
        break;

      win32_display->event_record->current_root_x = msg->pt.x;
      win32_display->event_record->current_root_y = msg->pt.y;

      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
        gdk_win32_surface_do_move_resize_drag (surface, msg->pt.x, msg->pt.y);
      else if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->pointer_device_items->input_ignore_core == 0)
	{
          double x = (double) GET_X_LPARAM (msg->lParam) / impl->surface_scale;
          double y = (double) GET_Y_LPARAM (msg->lParam) / impl->surface_scale;

          _gdk_device_virtual_set_active (win32_display->device_manager->core_pointer,
                                          win32_display->device_manager->system_pointer);

	  event = gdk_motion_event_new (surface,
	                                win32_display->device_manager->core_pointer,
                                        NULL,
                                        _gdk_win32_get_next_tick (msg->time),
	                                build_pointer_event_state (msg),
                                        x,
                                        y,
                                        NULL);

	  _gdk_win32_append_event (event);
	}

      return_val = TRUE;
      break;

    case WM_NCMOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print (" (%d,%d)",
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));
      break;

    case WM_MOUSELEAVE:
      GDK_NOTE (EVENTS, g_print (" %d (%ld,%ld)",
				 HIWORD (msg->wParam), msg->pt.x, msg->pt.y));

      win32_display->device_manager->pen_touch_input = FALSE;

      new_surface = NULL;
      hwnd = WindowFromPoint (msg->pt);
      ignore_leave = FALSE;
      if (hwnd != NULL)
	{
	  char classname[64];

	  POINT client_pt = msg->pt;

	  /* The synapitics trackpad drivers have this irritating
	     feature where it pops up a surface right under the pointer
	     when you scroll. We ignore the leave and enter events for
	     this surface */
	  if (GetClassNameA (hwnd, classname, sizeof(classname)) &&
	      strcmp (classname, SYNAPSIS_ICON_WINDOW_CLASS) == 0)
	    ignore_leave = TRUE;

	  ScreenToClient (hwnd, &client_pt);
	  GetClientRect (hwnd, &rect);
	  if (PtInRect (&rect, client_pt))
	    new_surface = gdk_win32_display_handle_table_lookup_ (display, hwnd);
	}

      if (!ignore_leave)
        synthesize_crossing_events (display,
                                    win32_display->device_manager->system_pointer,
                                    win32_display->event_record->mouse_surface, new_surface,
                                    GDK_CROSSING_NORMAL,
                                   &msg->pt,
                                    0, /* TODO: Set right mask */
                                    _gdk_win32_get_next_tick (msg->time),
                                    FALSE);
      g_set_object (&win32_display->event_record->mouse_surface, new_surface);
      win32_display->event_record->mouse_surface_ignored_leave = ignore_leave ? new_surface : NULL;


      return_val = TRUE;
      break;

    case WM_POINTERDOWN:
      if (win32_display->tablet_input_api != GDK_WIN32_TABLET_INPUT_API_WINPOINTER ||
          gdk_winpointer_should_forward_message (win32_display->device_manager, msg))
        {
          return_val = FALSE;
          break;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
        {
          win32_display->event_record->current_root_x = win32_display->device_manager->latest_pen_touch_position.x = GET_X_LPARAM (msg->lParam);
          win32_display->event_record->current_root_y = win32_display->device_manager->latest_pen_touch_position.y = GET_Y_LPARAM (msg->lParam);
          win32_display->device_manager->pen_touch_input = TRUE;
          win32_display->device_manager->last_digitizer_time = msg->time;
        }

      if (pointer_grab != NULL &&
          !pointer_grab->implicit &&
          !pointer_grab->owner_events)
        g_set_object (&surface, pointer_grab->surface);

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam) && win32_display->event_record->mouse_surface != surface)
        crossing_cb = make_crossing_event;

      gdk_winpointer_input_events (surface, crossing_cb, msg);

      *ret_valp = 0;
      return_val = TRUE;
      break;

    case WM_POINTERUP:
      if (win32_display->tablet_input_api != GDK_WIN32_TABLET_INPUT_API_WINPOINTER ||
          gdk_winpointer_should_forward_message (win32_display->device_manager, msg))
        {
          return_val = FALSE;
          break;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
        {
          win32_display->event_record->current_root_x = win32_display->device_manager->latest_pen_touch_position.x = GET_X_LPARAM (msg->lParam);
          win32_display->event_record->current_root_y = win32_display->device_manager->latest_pen_touch_position.y = GET_Y_LPARAM (msg->lParam);
          win32_display->device_manager->pen_touch_input = TRUE;
          win32_display->device_manager->last_digitizer_time = msg->time;
        }

      if (pointer_grab != NULL &&
          !pointer_grab->implicit &&
          !pointer_grab->owner_events)
        g_set_object (&surface, pointer_grab->surface);

      gdk_winpointer_input_events (surface, NULL, msg);

      impl = GDK_WIN32_SURFACE (surface);
      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
        {
          gdk_win32_surface_end_move_resize_drag (surface);
        }

      *ret_valp = 0;
      return_val = TRUE;
      break;

    case WM_POINTERUPDATE:
      if (win32_display->tablet_input_api != GDK_WIN32_TABLET_INPUT_API_WINPOINTER ||
          gdk_winpointer_should_forward_message (win32_display->device_manager, msg))
        {
          return_val = FALSE;
          break;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
        {
          win32_display->event_record->current_root_x = win32_display->device_manager->latest_pen_touch_position.x = GET_X_LPARAM (msg->lParam);
          win32_display->event_record->current_root_y = win32_display->device_manager->latest_pen_touch_position.y = GET_Y_LPARAM (msg->lParam);
          win32_display->device_manager->pen_touch_input = TRUE;
          win32_display->device_manager->last_digitizer_time = msg->time;
        }

      if (pointer_grab != NULL &&
          !pointer_grab->implicit &&
          !pointer_grab->owner_events)
        g_set_object (&surface, pointer_grab->surface);

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam) && win32_display->event_record->mouse_surface != surface)
        crossing_cb = make_crossing_event;

      impl = GDK_WIN32_SURFACE (surface);

      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
        {
          gdk_win32_surface_do_move_resize_drag (surface,
                                                 win32_display->event_record->current_root_x,
                                                 win32_display->event_record->current_root_y);
        }
      else
        {
          gdk_winpointer_input_events (surface, crossing_cb, msg);
        }

      *ret_valp = 0;
      return_val = TRUE;
      break;

    case WM_NCPOINTERUPDATE:
      if (win32_display->tablet_input_api != GDK_WIN32_TABLET_INPUT_API_WINPOINTER ||
          gdk_winpointer_should_forward_message (win32_display->device_manager, msg))
        {
          return_val = FALSE;
          break;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
        {
          win32_display->event_record->current_root_x = win32_display->device_manager->latest_pen_touch_position.x = GET_X_LPARAM (msg->lParam);
          win32_display->event_record->current_root_y = win32_display->device_manager->latest_pen_touch_position.y = GET_Y_LPARAM (msg->lParam);
          win32_display->device_manager->pen_touch_input = TRUE;
          win32_display->device_manager->last_digitizer_time = msg->time;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam) &&
          !IS_POINTER_INCONTACT_WPARAM (msg->wParam) &&
          win32_display->event_record->mouse_surface != NULL)
        {
          GdkDevice *event_device = NULL;
          guint32 event_time = 0;

          if (gdk_winpointer_get_message_info (msg, &event_device, win32_display, &event_time))
            {
              make_crossing_event(event_device,
                                  NULL,
                                  &win32_display->device_manager->latest_pen_touch_position,
                                  event_time);
            }
        }

      return_val = FALSE; /* forward to DefWindowProc */
      break;

    case WM_POINTERENTER:
      if (win32_display->tablet_input_api != GDK_WIN32_TABLET_INPUT_API_WINPOINTER ||
          gdk_winpointer_should_forward_message (win32_display->device_manager, msg))
        {
          return_val = FALSE;
          break;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
        {
          win32_display->event_record->current_root_x = win32_display->device_manager->latest_pen_touch_position.x = GET_X_LPARAM (msg->lParam);
          win32_display->event_record->current_root_y = win32_display->device_manager->latest_pen_touch_position.y = GET_Y_LPARAM (msg->lParam);
          win32_display->device_manager->pen_touch_input = TRUE;
          win32_display->device_manager->last_digitizer_time = msg->time;
        }

      if (pointer_grab != NULL &&
          !pointer_grab->implicit &&
          !pointer_grab->owner_events)
        g_set_object (&surface, pointer_grab->surface);

      if (IS_POINTER_NEW_WPARAM (msg->wParam))
        {
          gdk_winpointer_input_events (surface, NULL, msg);
        }

      *ret_valp = 0;
      return_val = TRUE;
      break;

    case WM_POINTERLEAVE:
      if (win32_display->tablet_input_api != GDK_WIN32_TABLET_INPUT_API_WINPOINTER ||
          gdk_winpointer_should_forward_message (win32_display->device_manager, msg))
        {
          return_val = FALSE;
          break;
        }

      if (IS_POINTER_PRIMARY_WPARAM (msg->wParam))
        {
          win32_display->event_record->current_root_x = win32_display->device_manager->latest_pen_touch_position.x = GET_X_LPARAM (msg->lParam);
          win32_display->event_record->current_root_y = win32_display->device_manager->latest_pen_touch_position.y = GET_Y_LPARAM (msg->lParam);
          win32_display->device_manager->pen_touch_input = TRUE;
          win32_display->device_manager->last_digitizer_time = msg->time;
        }

      if (!IS_POINTER_INRANGE_WPARAM (msg->wParam))
        {
          gdk_winpointer_input_events (surface, NULL, msg);
        }
      else if (IS_POINTER_PRIMARY_WPARAM (msg->wParam) && win32_display->event_record->mouse_surface != NULL)
        {
          GdkDevice *event_device = NULL;
          guint32 event_time = 0;

          if (gdk_winpointer_get_message_info (msg, &event_device, win32_display, &event_time))
            {
              make_crossing_event(event_device,
                                  NULL,
                                  &win32_display->device_manager->latest_pen_touch_position,
                                  event_time);
            }
        }

      gdk_winpointer_interaction_ended (win32_display->device_manager, msg);

      *ret_valp = 0;
      return_val = TRUE;
      break;

    case DM_POINTERHITTEST:
      gdk_dmanipulation_maybe_add_contact (surface, msg);

      *ret_valp = 0;
      return_val = TRUE;
      break;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    {
      int16_t scroll_x = 0;
      int16_t scroll_y = 0;
      GdkScrollDirection direction;

      char classname[64];

      GDK_NOTE (EVENTS, g_print (" %d", (short) HIWORD (msg->wParam)));

      /* On versions of Windows before Windows 10, the WM_MOUSEWHEEL
       * is delivered to the surface that has keyboard focus, not the
       * surface under the pointer. Work around that.
       * Also, the position is in screen coordinates, not client
       * coordinates as with the button messages. */
      point.x = GET_X_LPARAM (msg->lParam);
      point.y = GET_Y_LPARAM (msg->lParam);

      hwnd = WindowFromPoint (point);
      if (!hwnd)
        break;

      /* The synapitics trackpad drivers have this irritating
         feature where it pops up a surface right under the pointer
         when you scroll. We backtrack and to the toplevel and
         find the innermost child instead. */
      if (GetClassNameA (hwnd, classname, sizeof(classname)) &&
          strcmp (classname, SYNAPSIS_ICON_WINDOW_CLASS) == 0)
        {
          HWND hwndc;

          /* Find our toplevel surface */
          hwnd = GetAncestor (msg->hwnd, GA_ROOT);

          /* Walk back up to the outermost child at the desired point */
          do {
            ScreenToClient (hwnd, &point);
            hwndc = ChildWindowFromPoint (hwnd, point);
            ClientToScreen (hwnd, &point);
          } while (hwndc != hwnd && (hwnd = hwndc, 1));
        }

      msg->hwnd = hwnd;

      g_set_object (&surface, gdk_win32_display_handle_table_lookup_ (display, hwnd));
      if (!surface)
        break;

      if (msg->message == WM_MOUSEWHEEL)
        scroll_y = GET_WHEEL_DELTA_WPARAM (msg->wParam);
      else if (msg->message == WM_MOUSEHWHEEL)
        scroll_x = GET_WHEEL_DELTA_WPARAM (msg->wParam);

      _gdk_device_virtual_set_active (win32_display->device_manager->core_pointer,
                                      win32_display->device_manager->system_pointer);

      direction = 0;
      if (msg->message == WM_MOUSEWHEEL)
        direction = (((short) HIWORD (msg->wParam)) > 0)
                      ? GDK_SCROLL_UP
                      : GDK_SCROLL_DOWN;
      else if (msg->message == WM_MOUSEHWHEEL)
        direction = (((short) HIWORD (msg->wParam)) > 0)
                      ? GDK_SCROLL_RIGHT
                      : GDK_SCROLL_LEFT;

      event = gdk_scroll_event_new_value120 (surface,
                                             win32_display->device_manager->core_pointer,
                                             NULL,
                                             _gdk_win32_get_next_tick (msg->time),
                                             build_pointer_event_state (msg),
                                             direction,
                                             (double) scroll_x,
                                             (double) -scroll_y,
                                             GDK_SCROLL_RELATIVE_DIRECTION_UNKNOWN);

      _gdk_win32_append_event (event);

      *ret_valp = 0;
      return_val = TRUE;
    }
    break;

    case WM_MOUSEACTIVATE:
      if (GDK_IS_DRAG_SURFACE (surface) ||
          _gdk_modal_blocked (surface))
        {
          /* Focus the modal surface */
          GdkSurface *modal_surface = _gdk_modal_current ();
          if (modal_surface != NULL)
            SetFocus (GDK_SURFACE_HWND (modal_surface));
          *ret_valp = MA_NOACTIVATE;
          return_val = TRUE;
        }

      break;

    case WM_POINTERACTIVATE:
      if (GDK_IS_DRAG_SURFACE (surface) ||
          _gdk_modal_blocked (surface))
        {
          /* Focus the modal surface */
          GdkSurface *modal_surface = _gdk_modal_current ();
          if (modal_surface != NULL)
            SetFocus (GDK_SURFACE_HWND (modal_surface));
          *ret_valp = PA_NOACTIVATE;
          return_val = TRUE;
        }

      break;

    case WM_KILLFOCUS:
      if (keyboard_grab != NULL &&
	  !GDK_SURFACE_DESTROYED (keyboard_grab->surface) &&
	  (win32_display->display_surface_record->modal_operation_in_progress & GDK_WIN32_MODAL_OP_DND) == 0)
	{
	  generate_grab_broken_event (win32_display->device_manager, keyboard_grab->surface, TRUE, NULL);
	}
      G_GNUC_FALLTHROUGH;

    case WM_SETFOCUS:
      if (keyboard_grab != NULL &&
	  !keyboard_grab->owner_events)
	break;

      if (GDK_SURFACE_DESTROYED (surface))
	break;

      generate_focus_event (win32_display->device_manager, surface, (msg->message == WM_SETFOCUS));
      return_val = TRUE;
      break;

    case WM_ERASEBKGND:
      GDK_NOTE (EVENTS, g_print (" %p", (HANDLE) msg->wParam));

      if (GDK_SURFACE_DESTROYED (surface))
	break;

      return_val = TRUE;
      *ret_valp = 1;
      break;

    case WM_PAINT:
      handle_wm_paint (msg, surface);
      break;

    case WM_SETCURSOR:
      GDK_NOTE (EVENTS, g_print (" %#x %#x",
				 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (pointer_grab != NULL)
        grab_surface = pointer_grab->surface;

      if (grab_surface == NULL && LOWORD (msg->lParam) != HTCLIENT)
	break;

      return_val = FALSE;

      if (grab_surface != NULL &&
          !GDK_SURFACE_DESTROYED (grab_surface))
        {
          win32_display = GDK_WIN32_DISPLAY (gdk_surface_get_display (grab_surface));

          if (win32_display->grab_cursor != NULL)
            {
              GDK_NOTE (EVENTS, g_print (" (grab SetCursor(%p)", gdk_win32_hcursor_get_handle (win32_display->grab_cursor)));
              SetCursor (gdk_win32_hcursor_get_handle (win32_display->grab_cursor));
              return_val = TRUE;
              *ret_valp = TRUE;
            }
        }

      if (!return_val &&
          !GDK_SURFACE_DESTROYED (surface) &&
          GDK_WIN32_SURFACE (surface)->cursor != NULL)
        {
          win32_display = GDK_WIN32_DISPLAY (gdk_surface_get_display (surface));
          GDK_NOTE (EVENTS, g_print (" (surface SetCursor(%p)", gdk_win32_hcursor_get_handle (GDK_WIN32_SURFACE (surface)->cursor)));
          SetCursor (gdk_win32_hcursor_get_handle (GDK_WIN32_SURFACE (surface)->cursor));
          return_val = TRUE;
          *ret_valp = TRUE;
        }

      break;

    case WM_SYSMENU:
      return_val = handle_wm_sysmenu (surface, msg, ret_valp);
      break;

    case WM_INITMENU:
      impl = GDK_WIN32_SURFACE (surface);

      if (impl->have_temp_styles)
        {
          LONG_PTR hwnd_style;

          hwnd_style = GetWindowLongPtr (GDK_SURFACE_HWND (surface),
                                           GWL_STYLE);
          /* Handling WM_SYSMENU added extra styles to this surface,
           * remove them now.
           */
          hwnd_style &= ~impl->temp_styles;
          SetWindowLongPtr (GDK_SURFACE_HWND (surface),
                            GWL_STYLE,
                            hwnd_style);
        }

      break;

    case WM_SYSCOMMAND:
      /* From: https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand?redirectedfrom=MSDN
       * To obtain the correct result when testing the value of wParam,
       * an application must combine the value 0xFFF0 with the wParam value by using the bitwise AND operator. */
      switch (msg->wParam & 0xFFF0)
        {
        case SC_MINIMIZE:
        case SC_RESTORE:
          do_show_surface (surface, msg->wParam == SC_MINIMIZE ? TRUE : FALSE);
          break;

        case SC_MAXIMIZE:
          impl = GDK_WIN32_SURFACE (surface);
          impl->maximizing = TRUE;
          break;
        }

      break;

    case WM_ENTERSIZEMOVE:
      GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_move_resize_hwnd = msg->hwnd;
      _gdk_win32_begin_modal_call (surface, GDK_WIN32_MODAL_OP_SIZEMOVE_MASK);
      break;

    case WM_EXITSIZEMOVE:
      if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_operation_in_progress &
          GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
	{
	  GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_move_resize_hwnd = NULL;
	  _gdk_win32_end_modal_call (surface, GDK_WIN32_MODAL_OP_SIZEMOVE_MASK);
	}
      break;

    case WM_ENTERMENULOOP:
      _gdk_win32_begin_modal_call (surface, GDK_WIN32_MODAL_OP_MENU);
      break;

    case WM_EXITMENULOOP:
      if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_operation_in_progress &
          GDK_WIN32_MODAL_OP_MENU)
	_gdk_win32_end_modal_call (surface, GDK_WIN32_MODAL_OP_MENU);
      break;

      break;

    /*
     * Handle WM_CANCELMODE and do nothing in response to it when DnD is
     * active. Otherwise pass it to DefWindowProc, which will call ReleaseCapture()
     * on our behalf.
     * This prevents us from losing mouse capture when alt-tabbing during DnD
     * (this includes the feature of Windows Explorer where dragging stuff over
     * a window button in the taskbar causes that surface to receive focus, i.e.
     * keyboardless alt-tabbing).
     */
    case WM_CANCELMODE:
      if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_operation_in_progress &
          GDK_WIN32_MODAL_OP_DND)
        {
          return_val = TRUE;
          *ret_valp = 0;
        }
      break;

    case WM_CAPTURECHANGED:
      /* Sometimes we don't get WM_EXITSIZEMOVE, for instance when you
	 select move/size in the menu and then click somewhere without
	 moving/resizing. We work around this using WM_CAPTURECHANGED. */
      if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_operation_in_progress & GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
	{
	  GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_move_resize_hwnd = NULL;
	  _gdk_win32_end_modal_call (surface, GDK_WIN32_MODAL_OP_SIZEMOVE_MASK);
	}


      impl = GDK_WIN32_SURFACE (surface);
      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
        gdk_win32_surface_end_move_resize_drag (surface);
      break;

    case WM_WINDOWPOSCHANGING:
      {
        char buf[256];
        GDK_NOTE (EVENTS, (hwndpos = (WINDOWPOS *) msg->lParam,
                           g_print (" %s %s %dx%d@%+d%+d now below %p",
                                    _gdk_win32_surface_pos_bits_to_string (hwndpos->flags),
                                    (hwndpos->hwndInsertAfter == HWND_BOTTOM ? "BOTTOM" :
                                     (hwndpos->hwndInsertAfter == HWND_NOTOPMOST ? "NOTOPMOST" :
                                      (hwndpos->hwndInsertAfter == HWND_TOP ? "TOP" :
                                       (hwndpos->hwndInsertAfter == HWND_TOPMOST ? "TOPMOST" :
                                        (sprintf (buf, "%p", hwndpos->hwndInsertAfter),
                                         buf))))),
                                    hwndpos->cx, hwndpos->cy, hwndpos->x, hwndpos->y,
                                    GetNextWindow (msg->hwnd, GW_HWNDPREV))));
      }

      if (GDK_SURFACE_IS_MAPPED (surface))
        {

          impl = GDK_WIN32_SURFACE (surface);

          if (impl->maximizing)
            {
              MINMAXINFO our_mmi;

              if (_gdk_win32_surface_fill_min_max_info (surface, &our_mmi))
                {
                  hwndpos = (WINDOWPOS *) msg->lParam;
                  hwndpos->cx = our_mmi.ptMaxSize.x;
                  hwndpos->cy = our_mmi.ptMaxSize.y;

                  if (!_gdk_win32_surface_lacks_wm_decorations (surface) &&
                      !(hwndpos->flags & SWP_NOCLIENTSIZE) &&
                      surface->width == impl->next_layout.configured_width &&
                      surface->height == impl->next_layout.configured_height)
                    {
                      impl->inhibit_configure = TRUE;
                    }
                }

              impl->maximizing = FALSE;
            }
        }

      break;

    case WM_WINDOWPOSCHANGED:
      hwndpos = (WINDOWPOS *) msg->lParam;

      {
        char buf[256];
        GDK_NOTE (EVENTS, g_print (" %s %s %dx%d@%+d%+d",
                                   _gdk_win32_surface_pos_bits_to_string (hwndpos->flags),
                                   (hwndpos->hwndInsertAfter == HWND_BOTTOM ? "BOTTOM" :
                                    (hwndpos->hwndInsertAfter == HWND_NOTOPMOST ? "NOTOPMOST" :
                                     (hwndpos->hwndInsertAfter == HWND_TOP ? "TOP" :
                                      (hwndpos->hwndInsertAfter == HWND_TOPMOST ? "TOPMOST" :
                                       (sprintf (buf, "%p", hwndpos->hwndInsertAfter),
                                        buf))))),
                                   hwndpos->cx, hwndpos->cy, hwndpos->x, hwndpos->y));
      }

      impl = GDK_WIN32_SURFACE (surface);

      /* Break grabs on unmap or minimize */
      if (hwndpos->flags & SWP_HIDEWINDOW ||
	  ((hwndpos->flags & SWP_STATECHANGED) && IsIconic (msg->hwnd)))
      {
        GdkDevice *device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

        if ((pointer_grab != NULL && pointer_grab->surface == surface) ||
            (keyboard_grab != NULL && keyboard_grab->surface == surface))
          gdk_device_ungrab (device, msg -> time);
    }

      /* Update surface HWND state */
      if (hwndpos->flags & (SWP_STATECHANGED | SWP_SHOWWINDOW | SWP_HIDEWINDOW))
	{
	  GdkToplevelState set_bits, unset_bits, old_state, new_state;

	  old_state = surface->state;

	  set_bits = 0;
	  unset_bits = 0;

	  if (IsIconic (msg->hwnd))
	    set_bits |= (GDK_TOPLEVEL_STATE_MINIMIZED |
                         GDK_TOPLEVEL_STATE_SUSPENDED);
	  else
	    unset_bits |= (GDK_TOPLEVEL_STATE_MINIMIZED |
                           GDK_TOPLEVEL_STATE_SUSPENDED);

	  if (IsZoomed (msg->hwnd))
	    set_bits |= GDK_TOPLEVEL_STATE_MAXIMIZED;
	  else
	    unset_bits |= GDK_TOPLEVEL_STATE_MAXIMIZED;

      /*
       * If we are minimizing, pause all surface layout computations, and re-start the
       * computation once we are coming out of a minimized state
       */
      if (!(old_state & GDK_TOPLEVEL_STATE_MINIMIZED) && set_bits & GDK_TOPLEVEL_STATE_MINIMIZED)
        gdk_surface_freeze_updates (surface);

      if (old_state & GDK_TOPLEVEL_STATE_MINIMIZED && unset_bits & GDK_TOPLEVEL_STATE_MINIMIZED)
        gdk_surface_thaw_updates (surface);

	  gdk_surface_set_is_mapped (surface, !!IsWindowVisible (msg->hwnd));
	  gdk_synthesize_surface_state (surface, unset_bits, set_bits);

	  new_state = surface->state;

	  /* Whenever one surface changes iconified state we need to also
	   * change the iconified state in all transient related surfaces,
	   * as surfaces doesn't give icons for transient children.
	   */
	  if ((old_state & GDK_TOPLEVEL_STATE_MINIMIZED) !=
	      (new_state & GDK_TOPLEVEL_STATE_MINIMIZED))
	    do_show_surface (surface, (new_state & GDK_TOPLEVEL_STATE_MINIMIZED));
	}

      /* Show, New size or position => configure event */
      if (!(hwndpos->flags & SWP_NOCLIENTMOVE) ||
          !(hwndpos->flags & SWP_NOCLIENTSIZE) ||
          (hwndpos->flags & SWP_SHOWWINDOW))
	      {
          if (!IsIconic (msg->hwnd) && !GDK_SURFACE_DESTROYED (surface))
            {
              if (!_gdk_win32_surface_lacks_wm_decorations (surface) &&
                  !(hwndpos->flags & SWP_NOCLIENTSIZE) &&
                  surface->width == impl->next_layout.configured_width &&
                  surface->height == impl->next_layout.configured_height)
                {
                  impl->inhibit_configure = TRUE;
                }

              gdk_surface_request_layout (surface);
            }
        }

      /* Call modal timer immediate so that we repaint faster after a resize. */
      if (GDK_WIN32_DISPLAY (gdk_surface_get_display (surface))->display_surface_record->modal_operation_in_progress & GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
        {
          modal_timer_proc (msg->hwnd,
                            msg->message,
                            (UINT_PTR)surface,
                            msg->time);
        }

      /* Claim as handled, so that WM_SIZE and WM_MOVE are avoided */
      return_val = TRUE;
      *ret_valp = 0;
      break;

    case WM_SIZING:
      GetWindowRect (GDK_SURFACE_HWND (surface), &rect);
      GDK_NOTE (EVENTS, g_print (" %s curr:%s drag:%s",
				 (msg->wParam == WMSZ_BOTTOM ? "BOTTOM" :
				  (msg->wParam == WMSZ_BOTTOMLEFT ? "BOTTOMLEFT" :
				   (msg->wParam == WMSZ_LEFT ? "LEFT" :
				    (msg->wParam == WMSZ_TOPLEFT ? "TOPLEFT" :
				     (msg->wParam == WMSZ_TOP ? "TOP" :
				      (msg->wParam == WMSZ_TOPRIGHT ? "TOPRIGHT" :
				       (msg->wParam == WMSZ_RIGHT ? "RIGHT" :

					(msg->wParam == WMSZ_BOTTOMRIGHT ? "BOTTOMRIGHT" :
					 "???")))))))),
				 _gdk_win32_rect_to_string (&rect),
				 _gdk_win32_rect_to_string ((RECT *) msg->lParam)));

      impl = GDK_WIN32_SURFACE (surface);

      break;

    case WM_GETMINMAXINFO:
      mmi = (MINMAXINFO*) msg->lParam;

      GDK_NOTE (EVENTS, g_print (" (mintrack:%ldx%ld maxtrack:%ldx%ld "
				 "maxpos:%+ld%+ld maxsize:%ldx%ld)",
				 mmi->ptMinTrackSize.x, mmi->ptMinTrackSize.y,
				 mmi->ptMaxTrackSize.x, mmi->ptMaxTrackSize.y,
				 mmi->ptMaxPosition.x, mmi->ptMaxPosition.y,
				 mmi->ptMaxSize.x, mmi->ptMaxSize.y));

      if (_gdk_win32_surface_fill_min_max_info (surface, mmi))
        {
          /* Don't call DefWindowProcW() */
          GDK_NOTE (EVENTS,
                    g_print (" (handled, mintrack:%ldx%ld maxtrack:%ldx%ld "
                             "maxpos:%+ld%+ld maxsize:%ldx%ld)",
                             mmi->ptMinTrackSize.x, mmi->ptMinTrackSize.y,
                             mmi->ptMaxTrackSize.x, mmi->ptMaxTrackSize.y,
                             mmi->ptMaxPosition.x, mmi->ptMaxPosition.y,
                             mmi->ptMaxSize.x, mmi->ptMaxSize.y));

          return_val = TRUE;
        }

      break;

    case WM_CLOSE:
      if (GDK_SURFACE_DESTROYED (surface))
	break;

      event = gdk_delete_event_new (surface);

      _gdk_win32_append_event (event);

      impl = GDK_WIN32_SURFACE (surface);

      if (impl->transient_owner && GetForegroundWindow() == GDK_SURFACE_HWND (surface))
	{
	  SetForegroundWindow (GDK_SURFACE_HWND (impl->transient_owner));
	}

      return_val = TRUE;
      break;

    case WM_DPICHANGED:
      handle_dpi_changed (surface, msg);
      return_val = FALSE;
      *ret_valp = 0;
      break;

    case WM_DESTROY:
      if (win32_display->tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINPOINTER)
        gdk_winpointer_finalize_surface (surface);

      gdk_dmanipulation_finalize_surface (surface);

      return_val = FALSE;
      break;

    case WM_NCDESTROY:
      if ((pointer_grab != NULL && pointer_grab->surface == surface) ||
          (keyboard_grab && keyboard_grab->surface == surface))
      {
        GdkDevice *device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
        gdk_device_ungrab (device, msg -> time);
      }

      if ((surface != NULL) && (msg->hwnd != GetDesktopWindow ()))
	gdk_surface_destroy_notify (surface);

      if (surface == NULL || GDK_SURFACE_DESTROYED (surface))
	break;

      event = gdk_delete_event_new (surface);

      _gdk_win32_append_event (event);

      return_val = TRUE;
      break;

    case WM_DWMCOMPOSITIONCHANGED:
      gdk_win32_surface_enable_transparency (surface);
      break;

    case WM_ACTIVATE:
      GDK_NOTE (EVENTS, g_print (" %s%s %p",
				 (LOWORD (msg->wParam) == WA_ACTIVE ? "ACTIVE" :
				  (LOWORD (msg->wParam) == WA_CLICKACTIVE ? "CLICKACTIVE" :
				   (LOWORD (msg->wParam) == WA_INACTIVE ? "INACTIVE" : "???"))),
				 HIWORD (msg->wParam) ? " minimized" : "",
				 (HWND) msg->lParam));
      if (GDK_IS_POPUP (surface) || GDK_IS_DRAG_SURFACE (surface))
        {
          /* Popups cannot be activated or de-activated - 
           * they only support keyboard focus, which GTK
           * will handle for us.
           */
          *ret_valp = 0;
          return_val = TRUE;
          break;
        }
      /* We handle mouse clicks for modally-blocked surfaces under WM_MOUSEACTIVATE,
       * but we still need to deal with alt-tab, or with SetActiveWindow() type
       * situations.
       */
      if (_gdk_modal_blocked (surface) && LOWORD (msg->wParam) == WA_ACTIVE)
	{
	  GdkSurface *modal_current = _gdk_modal_current ();
	  SetActiveWindow (GDK_SURFACE_HWND (modal_current));
	  *ret_valp = 0;
	  return_val = TRUE;
	  break;
	}

      if (LOWORD (msg->wParam) == WA_INACTIVE)
        {
          if (msg->lParam != 0)
            {
               GdkSurface *other_surface = gdk_win32_display_handle_table_lookup_ (display, (HWND) msg->lParam);
               if (other_surface != NULL &&
                   (GDK_IS_POPUP (other_surface) || GDK_IS_DRAG_SURFACE (other_surface)))
                {
                  /* We're being deactivated in favour of some popup or temp surface.
                   * Since only toplevels can have visual focus, pretend that
                   * nothing happened.
                   */
                  *ret_valp = 0;
                  return_val = TRUE;
                  break;
                }
            }

          gdk_synthesize_surface_state (surface, GDK_TOPLEVEL_STATE_FOCUSED, 0);
        }
      else
        {
          gdk_synthesize_surface_state (surface, 0, GDK_TOPLEVEL_STATE_FOCUSED);

          if (win32_display->tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINTAB)
            {
              /* Bring any tablet contexts to the top of the overlap order when
               * one of our surfaces is activated.
               * NOTE: It doesn't seem to work well if it is done in WM_ACTIVATEAPP
               * instead
               */
              _gdk_wintab_set_tablet_active (win32_display->device_manager);
           }
        }

      break;

    case WM_ACTIVATEAPP:
      GDK_NOTE (EVENTS, g_print (" %s thread: %" G_GINT64_FORMAT,
				 msg->wParam ? "YES" : "NO",
				 (gint64) msg->lParam));
      break;
    case WM_NCHITTEST:
      /* TODO: pass all messages to DwmDefWindowProc() first! */
      return_val = handle_nchittest (msg->hwnd, surface,
                                     GET_X_LPARAM (msg->lParam),
                                     GET_Y_LPARAM (msg->lParam), ret_valp);
      break;

    case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
      *ret_valp = TABLET_DISABLE_PRESSANDHOLD |
                  TABLET_DISABLE_PENTAPFEEDBACK |
                  TABLET_DISABLE_PENBARRELFEEDBACK |
                  TABLET_DISABLE_FLICKS |
                  TABLET_DISABLE_FLICKFALLBACKKEYS;
      return_val = TRUE;
      break;


      /* Handle WINTAB events here, as we know that the device manager will
       * use the fixed WT_DEFBASE as lcMsgBase, and we thus can use the
       * constants as case labels.
       */
    case WT_PACKET:
      GDK_NOTE (EVENTS, g_print (" %d %p",
				 (int) msg->wParam, (gpointer) msg->lParam));
      goto wintab;

    case WT_CSRCHANGE:
      GDK_NOTE (EVENTS, g_print (" %d %p",
				 (int) msg->wParam, (gpointer) msg->lParam));
      goto wintab;

    case WT_PROXIMITY:
      GDK_NOTE (EVENTS, g_print (" %p %d %d",
				 (gpointer) msg->wParam,
				 LOWORD (msg->lParam),
				 HIWORD (msg->lParam)));
      /* Fall through */
    wintab:

      if (win32_display->tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINTAB)
        {
          event = gdk_wintab_make_event (display, msg, surface);
          if (event)
            {
              _gdk_win32_append_event (event);
               gdk_event_unref (event);
            }
        }

      break;
    }

done:

  if (surface)
    g_object_unref (surface);

#undef return
  return return_val;
}

void
_gdk_win32_display_queue_events (GdkDisplay *display)
{
  MSG msg;

  if (modal_win32_dialog != NULL)
    return;

  while (PeekMessageW (&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessageW (&msg);
    }
}

static gboolean
gdk_event_prepare (GSource *source,
		   int     *timeout)
{
  GdkWin32EventSource *event_source = (GdkWin32EventSource *)source;
  gboolean retval;

  *timeout = -1;

  if (event_source->display->event_pause_count > 0)
    retval =_gdk_event_queue_find_first (event_source->display) != NULL;
  else
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
              (modal_win32_dialog == NULL &&
               GetQueueStatus (QS_ALLINPUT) != 0));

  return retval;
}

static gboolean
gdk_event_check (GSource *source)
{
  GdkWin32EventSource *event_source = (GdkWin32EventSource *)source;
  gboolean retval;

  if (event_source->display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (event_source->display) != NULL;
  else if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
              (modal_win32_dialog == NULL &&
               GetQueueStatus (QS_ALLINPUT) != 0));
  else
    retval = FALSE;

  return retval;
}

static gboolean
gdk_event_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)
{
  GdkWin32EventSource *event_source = (GdkWin32EventSource *)source;
  GdkEvent *event;

  _gdk_win32_display_queue_events (event_source->display);
  event = _gdk_event_unqueue (event_source->display);

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_unref (event);
    }

  return TRUE;
}

void
gdk_win32_set_modal_dialog_libgtk_only (HWND hwnd)
{
  modal_win32_dialog = hwnd;
}
