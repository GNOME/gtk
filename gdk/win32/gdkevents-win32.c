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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
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
#include "gdkdisplayprivate.h"
#include "gdkmonitorprivate.h"
#include "gdkwin32.h"
#include "gdkkeysyms.h"
#include "gdkdevicemanager-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-wintab.h"
#include "gdkwin32dnd.h"
#include "gdkwin32dnd-private.h"
#include "gdkdisplay-win32.h"
//#include "gdkselection-win32.h"
#include "gdkdragprivate.h"

#include <windowsx.h>

#ifdef G_WITH_CYGWIN
#include <fcntl.h>
#include <errno.h>
#endif

#include <objbase.h>

#include <imm.h>

#ifndef XBUTTON1
#define XBUTTON1 1
#define XBUTTON2 2
#endif

#ifndef VK_XBUTTON1
#define VK_XBUTTON1 5
#define VK_XBUTTON2 6
#endif

#ifndef MK_XBUTTON1
#define MK_XBUTTON1 32
#define MK_XBUTTON2 64
#endif

/* Undefined flags: */
#define SWP_NOCLIENTSIZE 0x0800
#define SWP_NOCLIENTMOVE 0x1000
#define SWP_STATECHANGED 0x8000
/*
 * Private function declarations
 */

#define SYNAPSIS_ICON_WINDOW_CLASS "SynTrackCursorWindowClass"

static gboolean gdk_event_translate (MSG        *msg,
				     gint       *ret_valp);
static gboolean gdk_event_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

/* Private variable declarations
 */

extern gint       _gdk_input_ignore_core;

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

/* Whenever we do an implicit grab (call SetCapture() after
 * a mouse button is held down), we ref the capturing surface
 * and keep that ref here. When mouse buttons are released,
 * we remove the implicit grab and synthesize a crossing
 * event from the grab surface to whatever surface is now
 * under cursor.
 */
static GdkSurface *implicit_grab_surface = NULL;

static GdkSurface *mouse_window = NULL;
static GdkSurface *mouse_window_ignored_leave = NULL;
static gint current_x, current_y;
static gint current_root_x, current_root_y;

static UINT got_gdk_events_message;
static HWND modal_win32_dialog = NULL;

#if 0
static HKL latin_locale = NULL;
#endif

static gboolean in_ime_composition = FALSE;
static UINT     modal_timer;
static UINT     sync_timer = 0;

static int debug_indent = 0;

static int both_shift_pressed[2]; /* to store keycodes for shift keys */

/* low-level keyboard hook handle */
static HHOOK keyboard_hook = NULL;
static UINT aerosnap_message;

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

static void
generate_focus_event (GdkDeviceManagerWin32 *device_manager,
                      GdkSurface        *window,
                      gboolean          in)
{
  GdkDevice *device;
  GdkDevice *source_device;
  GdkEvent *event;

  device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->core_keyboard;
  source_device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->system_keyboard;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->any.surface = window;
  event->focus_change.in = in;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source_device);

  _gdk_win32_append_event (event);
}

static void
generate_grab_broken_event (GdkDeviceManagerWin32 *device_manager,
                            GdkSurface        *window,
                            gboolean          keyboard,
                            GdkSurface        *grab_window)
{
  GdkEvent *event = gdk_event_new (GDK_GRAB_BROKEN);
  GdkDevice *device;
  GdkDevice *source_device;

  if (keyboard)
    {
      device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->core_keyboard;
      source_device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->system_keyboard;
    }
  else
    {
      device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->core_pointer;
      source_device = GDK_DEVICE_MANAGER_WIN32 (device_manager)->system_pointer;
    }

  event->any.surface = window;
  event->any.send_event = 0;
  event->grab_broken.keyboard = keyboard;
  event->grab_broken.implicit = FALSE;
  event->grab_broken.grab_surface = grab_window;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source_device);

  _gdk_win32_append_event (event);
}

static LRESULT
inner_window_procedure (HWND   hwnd,
			UINT   message,
			WPARAM wparam,
			LPARAM lparam)
{
  MSG msg;
  DWORD pos;
  gint ret_val = 0;

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
       * the window procedure.
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

  GDK_NOTE (EVENTS, g_print ("%s%*s%s %p %#x %#lx",
			     (debug_indent > 0 ? "\n" : ""),
			     debug_indent, "",
			     _gdk_win32_message_to_string (message), hwnd,
			     wparam, lparam));
  debug_indent += 2;
  retval = inner_window_procedure (hwnd, message, wparam, lparam);
  debug_indent -= 2;

  GDK_NOTE (EVENTS, g_print (" => %" G_GINT64_FORMAT "%s", (gint64) retval, (debug_indent == 0 ? "\n" : "")));

  return retval;
}

static LRESULT
low_level_keystroke_handler (WPARAM message,
                                       KBDLLHOOKSTRUCT *kbdhook,
                                       GdkSurface *window)
{
  GdkSurface *toplevel = window;
  static DWORD last_keydown = 0;

  if (message == WM_KEYDOWN &&
      !GDK_SURFACE_DESTROYED (toplevel) &&
      _gdk_win32_surface_lacks_wm_decorations (toplevel) && /* For CSD only */
      last_keydown != kbdhook->vkCode &&
      ((GetKeyState (VK_LWIN) & 0x8000) ||
      (GetKeyState (VK_RWIN) & 0x8000)))
	{
	  GdkWin32AeroSnapCombo combo = GDK_WIN32_AEROSNAP_COMBO_NOTHING;
	  gboolean lshiftdown = GetKeyState (VK_LSHIFT) & 0x8000;
          gboolean rshiftdown = GetKeyState (VK_RSHIFT) & 0x8000;
          gboolean oneshiftdown = (lshiftdown || rshiftdown) && !(lshiftdown && rshiftdown);
          gboolean maximized = gdk_surface_get_state (toplevel) & GDK_SURFACE_STATE_MAXIMIZED;

	  switch (kbdhook->vkCode)
	    {
	    case VK_UP:
	      combo = GDK_WIN32_AEROSNAP_COMBO_UP;
	      break;
	    case VK_DOWN:
	      combo = GDK_WIN32_AEROSNAP_COMBO_DOWN;
	      break;
	    case VK_LEFT:
	      combo = GDK_WIN32_AEROSNAP_COMBO_LEFT;
	      break;
	    case VK_RIGHT:
	      combo = GDK_WIN32_AEROSNAP_COMBO_RIGHT;
	      break;
	    }

	  if (oneshiftdown && combo != GDK_WIN32_AEROSNAP_COMBO_NOTHING)
	    combo += 4;

	  /* These are the only combos that Windows WM does handle for us */
	  if (combo == GDK_WIN32_AEROSNAP_COMBO_SHIFTLEFT ||
              combo == GDK_WIN32_AEROSNAP_COMBO_SHIFTRIGHT)
            combo = GDK_WIN32_AEROSNAP_COMBO_NOTHING;

          /* On Windows 10 the WM will handle this specific combo */
          if (combo == GDK_WIN32_AEROSNAP_COMBO_DOWN && maximized &&
              g_win32_check_windows_version (6, 4, 0, G_WIN32_OS_ANY))
            combo = GDK_WIN32_AEROSNAP_COMBO_NOTHING;

	  if (combo != GDK_WIN32_AEROSNAP_COMBO_NOTHING)
            PostMessage (GDK_SURFACE_HWND (toplevel), aerosnap_message, (WPARAM) combo, 0);
	}

  if (message == WM_KEYDOWN)
    last_keydown = kbdhook->vkCode;
  else if (message == WM_KEYUP && last_keydown == kbdhook->vkCode)
    last_keydown = 0;

  return 0;
}

static LRESULT CALLBACK
low_level_keyboard_proc (int    code,
                         WPARAM wParam,
                         LPARAM lParam)
{
  KBDLLHOOKSTRUCT *kbdhook;
  HWND kbd_focus_owner;
  GdkSurface *gdk_kbd_focus_owner;
  LRESULT chain;

  do
  {
    if (code < 0)
      break;

    kbd_focus_owner = GetFocus ();

    if (kbd_focus_owner == NULL)
      break;

    gdk_kbd_focus_owner = gdk_win32_handle_table_lookup (kbd_focus_owner);

    if (gdk_kbd_focus_owner == NULL)
      break;

    kbdhook = (KBDLLHOOKSTRUCT *) lParam;
    chain = low_level_keystroke_handler (wParam, kbdhook, gdk_kbd_focus_owner);

    if (chain != 0)
      return chain;
  } while (FALSE);

  return CallNextHookEx (0, code, wParam, lParam);
}

static void
set_up_low_level_keyboard_hook (void)
{
  HHOOK hook_handle;

  if (keyboard_hook != NULL)
    return;

  hook_handle = SetWindowsHookEx (WH_KEYBOARD_LL,
                                  (HOOKPROC) low_level_keyboard_proc,
                                  _gdk_dll_hinstance,
                                  0);

  if (hook_handle != NULL)
    keyboard_hook = hook_handle;
  else
    WIN32_API_FAILED ("SetWindowsHookEx");

  aerosnap_message = RegisterWindowMessage ("GDK_WIN32_AEROSNAP_MESSAGE");
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

  got_gdk_events_message = RegisterWindowMessage ("GDK_WIN32_GOT_EVENTS");

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
  g_source_set_name (source, "GDK Win32 event source");
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

  set_up_low_level_keyboard_hook ();
}

gboolean
_gdk_win32_display_has_pending (GdkDisplay *display)
{
  return (_gdk_event_queue_find_first (display) ||
	  (modal_win32_dialog == NULL &&
	   GetQueueStatus (QS_ALLINPUT) != 0));
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
  BIT (SUBSTRUCTURE);
  BIT (SCROLL);
#undef BIT

  return bfr;
}

#endif

static GdkSurface *
find_window_for_mouse_event (GdkSurface* reported_window,
			     MSG*       msg)
{
  POINT pt;
  GdkDisplay *display;
  GdkDeviceManagerWin32 *device_manager;
  GdkSurface *event_surface;
  HWND hwnd;
  RECT rect;
  GdkDeviceGrabInfo *grab;

  display = gdk_display_get_default ();
  device_manager = GDK_DEVICE_MANAGER_WIN32 (_gdk_device_manager);

  grab = _gdk_display_get_last_device_grab (display, device_manager->core_pointer);
  if (grab == NULL)
    return reported_window;

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
	    event_surface = gdk_win32_handle_table_lookup (hwnd);
	}
      if (event_surface == NULL)
	event_surface = grab->surface;
    }

  /* need to also adjust the coordinates to the new window */
  ScreenToClient (GDK_SURFACE_HWND (event_surface), &pt);

  /* ATTENTION: need to update client coords */
  msg->lParam = MAKELPARAM (pt.x, pt.y);

  return event_surface;
}

static void
build_key_event_state (GdkEvent *event,
		       BYTE     *key_state)
{
  GdkWin32Keymap *keymap;

  event->key.state = 0;

  if (key_state[VK_SHIFT] & 0x80)
    event->key.state |= GDK_SHIFT_MASK;

  if (key_state[VK_CAPITAL] & 0x01)
    event->key.state |= GDK_LOCK_MASK;

  if (key_state[VK_LBUTTON] & 0x80)
    event->key.state |= GDK_BUTTON1_MASK;
  if (key_state[VK_MBUTTON] & 0x80)
    event->key.state |= GDK_BUTTON2_MASK;
  if (key_state[VK_RBUTTON] & 0x80)
    event->key.state |= GDK_BUTTON3_MASK;
  if (key_state[VK_XBUTTON1] & 0x80)
    event->key.state |= GDK_BUTTON4_MASK;
  if (key_state[VK_XBUTTON2] & 0x80)
    event->key.state |= GDK_BUTTON5_MASK;

  keymap = GDK_WIN32_KEYMAP (_gdk_win32_display_get_keymap (_gdk_display));
  event->key.group = _gdk_win32_keymap_get_active_group (keymap);

  if (_gdk_win32_keymap_has_altgr (keymap) &&
      (key_state[VK_LCONTROL] & 0x80) &&
      (key_state[VK_RMENU] & 0x80))
    {
      event->key.state |= GDK_MOD2_MASK;
      if (key_state[VK_RCONTROL] & 0x80)
	event->key.state |= GDK_CONTROL_MASK;
      if (key_state[VK_LMENU] & 0x80)
	event->key.state |= GDK_MOD1_MASK;
    }
  else
    {
      if (key_state[VK_CONTROL] & 0x80)
	event->key.state |= GDK_CONTROL_MASK;
      if (key_state[VK_MENU] & 0x80)
	event->key.state |= GDK_MOD1_MASK;
    }
}

static gint
build_pointer_event_state (MSG *msg)
{
  gint state;

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
    state |= GDK_MOD1_MASK;

  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  return state;
}

static void
build_wm_ime_composition_event (GdkEvent *event,
				MSG      *msg,
				wchar_t   wc,
				BYTE     *key_state)
{
  event->key.time = _gdk_win32_get_next_tick (msg->time);

  build_key_event_state (event, key_state);

  event->key.hardware_keycode = 0; /* FIXME: What should it be? */
  event->key.keyval = gdk_unicode_to_keyval (wc);
}

#ifdef G_ENABLE_DEBUG

static void
print_event_state (guint state)
{
#define CASE(bit) if (state & GDK_ ## bit ## _MASK) g_print (#bit " ");
  CASE (SHIFT);
  CASE (LOCK);
  CASE (CONTROL);
  CASE (MOD1);
  CASE (MOD2);
  CASE (MOD3);
  CASE (MOD4);
  CASE (MOD5);
  CASE (BUTTON1);
  CASE (BUTTON2);
  CASE (BUTTON3);
  CASE (BUTTON4);
  CASE (BUTTON5);
#undef CASE
}

void
_gdk_win32_print_event (const GdkEvent *event)
{
  gchar *kvname;

  g_print ("%s%*s===> ", (debug_indent > 0 ? "\n" : ""), debug_indent, "");
  switch (event->any.type)
    {
#define CASE(x) case x: g_print (#x); break;
    CASE (GDK_NOTHING);
    CASE (GDK_DELETE);
    CASE (GDK_DESTROY);
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
#undef CASE
    default: g_assert_not_reached ();
    }

  g_print (" %p @ %ums ",
           event->any.surface ? GDK_SURFACE_HWND (event->any.surface) : NULL,
           gdk_event_get_time (event));

  switch (event->any.type)
    {
    case GDK_MOTION_NOTIFY:
      g_print ("(%.4g,%.4g) (%.4g,%.4g)",
	       event->motion.x, event->motion.y,
	       event->motion.x_root, event->motion.y_root);
      print_event_state (event->motion.state);
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      g_print ("%d (%.4g,%.4g) (%.4g,%.4g) ",
	       event->button.button,
	       event->button.x, event->button.y,
	       event->button.x_root, event->button.y_root);
      print_event_state (event->button.state);
      break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      kvname = gdk_keyval_name (event->key.keyval);
      g_print ("%#.02x group:%d %s",
	       event->key.hardware_keycode, event->key.group,
	       (kvname ? kvname : "??"));
      print_event_state (event->key.state);
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      g_print ("%p (%.4g,%.4g) (%.4g,%.4g) %s %s%s",
	       event->crossing.child_surface == NULL ? NULL : GDK_SURFACE_HWND (event->crossing.child_surface),
	       event->crossing.x, event->crossing.y,
	       event->crossing.x_root, event->crossing.y_root,
	       (event->crossing.mode == GDK_CROSSING_NORMAL ? "NORMAL" :
		(event->crossing.mode == GDK_CROSSING_GRAB ? "GRAB" :
		 (event->crossing.mode == GDK_CROSSING_UNGRAB ? "UNGRAB" :
		  "???"))),
	       (event->crossing.detail == GDK_NOTIFY_ANCESTOR ? "ANCESTOR" :
		(event->crossing.detail == GDK_NOTIFY_VIRTUAL ? "VIRTUAL" :
		 (event->crossing.detail == GDK_NOTIFY_INFERIOR ? "INFERIOR" :
		  (event->crossing.detail == GDK_NOTIFY_NONLINEAR ? "NONLINEAR" :
		   (event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL ? "NONLINEAR_VIRTUAL" :
		    (event->crossing.detail == GDK_NOTIFY_UNKNOWN ? "UNKNOWN" :
		     "???")))))),
	       event->crossing.focus ? " FOCUS" : "");
      print_event_state (event->crossing.state);
      break;
    case GDK_FOCUS_CHANGE:
      g_print ("%s", (event->focus_change.in ? "IN" : "OUT"));
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      if (event->dnd.drop != NULL)
	g_print ("ctx:%p: %s",
		 event->dnd.drop,
		 _gdk_win32_drag_protocol_to_string (GDK_WIN32_DRAG (event->dnd.drop)->protocol));
      break;
    case GDK_SCROLL:
      g_print ("(%.4g,%.4g) (%.4g,%.4g) %s ",
	       event->scroll.x, event->scroll.y,
	       event->scroll.x_root, event->scroll.y_root,
	       (event->scroll.direction == GDK_SCROLL_UP ? "UP" :
		(event->scroll.direction == GDK_SCROLL_DOWN ? "DOWN" :
		 (event->scroll.direction == GDK_SCROLL_LEFT ? "LEFT" :
		  (event->scroll.direction == GDK_SCROLL_RIGHT ? "RIGHT" :
		   "???")))));
      print_event_state (event->scroll.state);
      break;
    case GDK_GRAB_BROKEN:
      g_print ("%s %s %p",
	       (event->grab_broken.keyboard ? "KEYBOARD" : "POINTER"),
	       (event->grab_broken.implicit ? "IMPLICIT" : "EXPLICIT"),
	       (event->grab_broken.grab_surface ? GDK_SURFACE_HWND (event->grab_broken.grab_surface) : 0));
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

#endif

static void
fixup_event (GdkEvent *event)
{
  if (event->any.surface)
    g_object_ref (event->any.surface);
  if (((event->any.type == GDK_ENTER_NOTIFY) ||
       (event->any.type == GDK_LEAVE_NOTIFY)) &&
      (event->crossing.child_surface != NULL))
    g_object_ref (event->crossing.child_surface);
  event->any.send_event = InSendMessage ();
}

void
_gdk_win32_append_event (GdkEvent *event)
{
  GdkDisplay *display;
  GList *link;

  display = gdk_display_get_default ();

  fixup_event (event);
#if 1
  link = _gdk_event_queue_append (display, event);
  GDK_NOTE (EVENTS, _gdk_win32_print_event (event));
  /* event morphing, the passed in may not be valid afterwards */
  _gdk_windowing_got_event (display, link, event, 0);
#else
  _gdk_event_queue_append (display, event);
  GDK_NOTE (EVENTS, _gdk_win32_print_event (event));
#endif
}

static GdkWin32MessageFilterReturn
apply_message_filters (GdkDisplay *display,
                       MSG        *msg,
                       gint       *ret_valp,
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
 * On Windows, transient windows will not have their own taskbar entries.
 * Because of this, we must hide and restore groups of transients in both
 * directions.  That is, all transient children must be hidden or restored
 * with this window, but if this window’s transient owner also has a
 * transient owner then this window’s transient owner must be hidden/restored
 * with this one.  And etc, up the chain until we hit an ancestor that has no
 * transient owner.
 *
 * It would be a good idea if applications don’t chain transient windows
 * together.  There’s a limit to how much evil GTK can try to shield you
 * from.
 */
static void
show_window_recurse (GdkSurface *window, gboolean hide_window)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
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
	      show_window_recurse (child, hide_window);

	      children = children->next;
	    }
	}

      if (GDK_SURFACE_IS_MAPPED (window))
	{
	  if (!hide_window)
	    {
	      if (gdk_surface_get_state (window) & GDK_SURFACE_STATE_ICONIFIED)
		{
		  if (gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED)
		    {
		      GtkShowWindow (window, SW_SHOWMAXIMIZED);
		    }
		  else
		    {
		      GtkShowWindow (window, SW_RESTORE);
		    }
		}
	    }
	  else
	    {
	      GtkShowWindow (window, SW_MINIMIZE);
	    }
	}

      impl->changing_state = FALSE;
    }
}

static void
do_show_window (GdkSurface *window, gboolean hide_window)
{
  GdkSurface *tmp_window = NULL;
  GdkWin32Surface *tmp_impl = GDK_WIN32_SURFACE (window);

  if (!tmp_impl->changing_state)
    {
      /* Find the top-level window in our transient chain. */
      while (tmp_impl->transient_owner != NULL)
	{
	  tmp_window = tmp_impl->transient_owner;
	  tmp_impl = GDK_WIN32_SURFACE (tmp_window);
	}

      /* If we couldn't find one, use the window provided. */
      if (tmp_window == NULL)
	{
	  tmp_window = window;
	}

      /* Recursively show/hide every window in the chain. */
      if (tmp_window != window)
	{
	  show_window_recurse (tmp_window, hide_window);
	}
    }
}

static void
send_crossing_event (GdkDisplay                 *display,
		     GdkSurface                  *window,
		     GdkEventType                type,
		     GdkCrossingMode             mode,
		     GdkNotifyType               notify_type,
		     GdkSurface                  *subwindow,
		     POINT                      *screen_pt,
		     GdkModifierType             mask,
		     guint32                     time_)
{
  GdkEvent *event;
  GdkDeviceGrabInfo *grab;
  GdkDeviceManagerWin32 *device_manager;
  POINT pt;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  device_manager = _gdk_device_manager;

  grab = _gdk_display_has_device_grab (display, device_manager->core_pointer, 0);

  if (grab != NULL &&
      !grab->owner_events &&
      mode != GDK_CROSSING_UNGRAB)
    {
      /* !owner_event => only report events wrt grab window, ignore rest */
      if ((GdkSurface *)window != grab->surface)
	return;
    }

  pt = *screen_pt;
  ScreenToClient (GDK_SURFACE_HWND (window), &pt);

  event = gdk_event_new (type);
  event->any.surface = window;
  event->crossing.child_surface = subwindow;
  event->crossing.time = _gdk_win32_get_next_tick (time_);
  event->crossing.x = pt.x / impl->surface_scale;
  event->crossing.y = pt.y / impl->surface_scale;
  event->crossing.x_root = (screen_pt->x + _gdk_offset_x) / impl->surface_scale;
  event->crossing.y_root = (screen_pt->y + _gdk_offset_y) / impl->surface_scale;
  event->crossing.mode = mode;
  event->crossing.detail = notify_type;
  event->crossing.mode = mode;
  event->crossing.detail = notify_type;
  event->crossing.focus = FALSE;
  event->crossing.state = mask;
  gdk_event_set_device (event, device_manager->core_pointer);
  gdk_event_set_source_device (event, device_manager->system_pointer);

  _gdk_win32_append_event (event);
}

static GdkSurface *
find_common_ancestor (GdkSurface *win1,
		      GdkSurface *win2)
{
  GdkSurface *tmp;
  GList *path1 = NULL, *path2 = NULL;
  GList *list1, *list2;

  tmp = win1;
  while (tmp != NULL)
    {
      path1 = g_list_prepend (path1, tmp);
      tmp = tmp->parent;
    }

  tmp = win2;
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
synthesize_crossing_events (GdkDisplay                 *display,
			    GdkSurface                  *src,
			    GdkSurface                  *dest,
			    GdkCrossingMode             mode,
			    POINT                      *screen_pt,
			    GdkModifierType             mask,
			    guint32                     time_,
			    gboolean                    non_linear)
{
  GdkSurface *c;
  GdkSurface *win, *last, *next;
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
	  win = a->parent;
	  while (win != c && win != NULL)
	    {
	      send_crossing_event (display,
				   win, GDK_LEAVE_NOTIFY,
				   mode,
				   notify_type,
				   (GdkSurface *)last,
				   screen_pt,
				   mask, time_);

	      last = win;
	      win = win->parent;
	    }
	}
    }

  if (b) /* Might not be a dest, e.g. if we're moving out of the window */
    {
      /* Traverse down from c to b */
      if (c != b)
	{
	  path = NULL;
	  win = b->parent;
	  while (win != c && win != NULL)
	    {
	      path = g_list_prepend (path, win);
	      win = win->parent;
	    }

	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  list = path;
	  while (list)
	    {
	      win = (GdkSurface *)list->data;
	      list = list->next;
	      if (list)
		next = (GdkSurface *)list->data;
	      else
		next = b;

	      send_crossing_event (display,
				   win, GDK_ENTER_NOTIFY,
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
			   b, GDK_ENTER_NOTIFY,
			   mode,
			   notify_type,
			   NULL,
			   screen_pt,
			   mask, time_);
    }
}

/* Acquires actual client area size of the underlying native window.
 * Rectangle is in GDK screen coordinates (_gdk_offset_* is added).
 * Returns FALSE if configure events should be inhibited,
 * TRUE otherwise.
 */
gboolean
_gdk_win32_get_window_rect (GdkSurface *window,
                            RECT      *rect)
{
  RECT client_rect;
  POINT point;
  HWND hwnd;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  hwnd = GDK_SURFACE_HWND (window);

  GetClientRect (hwnd, &client_rect);
  point.x = client_rect.left; /* always 0 */
  point.y = client_rect.top;

  /* top level windows need screen coords */
  if (gdk_surface_get_parent (window) == NULL)
    {
      ClientToScreen (hwnd, &point);
      point.x += _gdk_offset_x * impl->surface_scale;
      point.y += _gdk_offset_y * impl->surface_scale;
    }

  rect->left = point.x;
  rect->top = point.y;
  rect->right = point.x + client_rect.right - client_rect.left;
  rect->bottom = point.y + client_rect.bottom - client_rect.top;

  return !impl->inhibit_configure;
}

void
_gdk_win32_do_emit_configure_event (GdkSurface *surface,
                                    RECT       rect)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  impl->unscaled_width = rect.right - rect.left;
  impl->unscaled_height = rect.bottom - rect.top;
  surface->width = (impl->unscaled_width + impl->surface_scale - 1) / impl->surface_scale;
  surface->height = (impl->unscaled_height + impl->surface_scale - 1) / impl->surface_scale;
  surface->x = rect.left / impl->surface_scale;
  surface->y = rect.top / impl->surface_scale;

  _gdk_surface_update_size (surface);

  g_signal_emit_by_name (surface, "size-changed", surface->width, surface->height);
}

void
_gdk_win32_emit_configure_event (GdkSurface *surface)
{
  RECT rect;

  if (!_gdk_win32_get_window_rect (surface, &rect))
    return;

  _gdk_win32_do_emit_configure_event (surface, rect);
}

cairo_region_t *
_gdk_win32_hrgn_to_region (HRGN  hrgn,
                           guint scale)
{
  RGNDATA *rgndata;
  RECT *rects;
  cairo_region_t *result;
  gint nbytes;
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
adjust_drag (LONG *drag,
	     LONG  curr,
	     gint  inc)
{
  if (*drag > curr)
    *drag = curr + ((*drag + inc/2 - curr) / inc) * inc;
  else
    *drag = curr - ((curr - *drag + inc/2) / inc) * inc;
}

static void
handle_wm_paint (MSG        *msg,
		 GdkSurface  *window)
{
  HRGN hrgn = CreateRectRgn (0, 0, 0, 0);
  HDC hdc;
  PAINTSTRUCT paintstruct;
  cairo_region_t *update_region;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

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
    gdk_surface_invalidate_region (window, update_region);
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

  while (_modal_operation_in_progress != GDK_WIN32_MODAL_OP_NONE &&
	 g_main_context_pending (NULL) &&
	 arbitrary_limit--)
    g_main_context_iteration (NULL, FALSE);
}

void
_gdk_win32_begin_modal_call (GdkWin32ModalOpKind kind)
{
  GdkWin32ModalOpKind was = _modal_operation_in_progress;
  g_assert (!(_modal_operation_in_progress & kind));

  _modal_operation_in_progress |= kind;

  if (was == GDK_WIN32_MODAL_OP_NONE)
    {
      modal_timer = SetTimer (NULL, 0, 10, modal_timer_proc);

      if (modal_timer == 0)
	WIN32_API_FAILED ("SetTimer");
    }
}

void
_gdk_win32_end_modal_call (GdkWin32ModalOpKind kind)
{
  g_assert (_modal_operation_in_progress & kind);

  _modal_operation_in_progress &= ~kind;

  if (_modal_operation_in_progress == GDK_WIN32_MODAL_OP_NONE &&
      modal_timer != 0)
    {
      API_CALL (KillTimer, (NULL, modal_timer));
      modal_timer = 0;
    }
}

static VOID CALLBACK
sync_timer_proc (HWND     hwnd,
		 UINT     msg,
		 UINT_PTR id,
		 DWORD    time)
{
  MSG message;
  if (PeekMessageW (&message, hwnd, WM_PAINT, WM_PAINT, PM_REMOVE))
    {
      return;
    }

  RedrawWindow (hwnd, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);

  KillTimer (hwnd, sync_timer);
}

static gboolean
handle_nchittest (HWND hwnd,
                  GdkSurface *window,
                  gint16 screen_x,
                  gint16 screen_y,
                  gint *ret_valp)
{
  RECT rect;
  GdkWin32Surface *impl;

  if (window == NULL || window->input_shape == NULL)
    return FALSE;

  /* If the window has decorations, DefWindowProc() will take
   * care of NCHITTEST.
   */
  if (!_gdk_win32_surface_lacks_wm_decorations (window))
    return FALSE;

  if (!GetWindowRect (hwnd, &rect))
    return FALSE;

  impl = GDK_WIN32_SURFACE (window);
  rect.left = screen_x - rect.left;
  rect.top = screen_y - rect.top;

  /* If it's inside the rect, return FALSE and let DefWindowProc() handle it */
  if (cairo_region_contains_point (window->input_shape,
                                   rect.left / impl->surface_scale,
                                   rect.top / impl->surface_scale))
    return FALSE;

  /* Otherwise override DefWindowProc() and tell WM that the point is not
   * within the window
   */
  *ret_valp = HTNOWHERE;
  return TRUE;
}

static void
handle_dpi_changed (GdkSurface *window,
                    MSG       *msg)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  GdkDisplay *display = gdk_display_get_default ();
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);
  RECT *rect = (RECT *)msg->lParam;
  guint old_scale = impl->surface_scale;

  /* MSDN for WM_DPICHANGED: dpi_x == dpi_y here, so LOWORD (msg->wParam) == HIWORD (msg->wParam) */
  guint dpi = LOWORD (msg->wParam);

  /* Don't bother if we use a fixed scale */
  if (win32_display->has_fixed_scale)
    return;

  impl->surface_scale = dpi / USER_DEFAULT_SCREEN_DPI;

  /* Don't bother if scales did not change in the end */
  if (old_scale == impl->surface_scale)
    return;

  if (!IsIconic (msg->hwnd) &&
      !GDK_SURFACE_DESTROYED (window))
    {
      GdkMonitor *monitor;

      monitor = gdk_display_get_monitor_at_surface (display, window);
      gdk_monitor_set_scale_factor (monitor, impl->surface_scale);

      if (impl->layered)
        {
          /* We only need to set the cairo surface device scale here ourselves for layered windows */
          if (impl->cache_surface != NULL)
            cairo_surface_set_device_scale (impl->cache_surface,
                                            impl->surface_scale,
                                            impl->surface_scale);
        }
    }

  _gdk_win32_adjust_client_rect (window, rect);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_surface_move_resize (window, window->x, window->y, window->width, window->height);
  else
    gdk_surface_resize (window, window->width, window->height);
}

static void
generate_button_event (GdkEventType      type,
                       gint              button,
                       GdkSurface        *window,
                       MSG              *msg)
{
  GdkEvent *event = gdk_event_new (type);
  GdkDeviceManagerWin32 *device_manager;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (_gdk_input_ignore_core > 0)
    return;

  device_manager = GDK_DEVICE_MANAGER_WIN32 (_gdk_device_manager);

  event->any.surface = window;
  event->button.time = _gdk_win32_get_next_tick (msg->time);
  event->button.x = current_x = (gint16) GET_X_LPARAM (msg->lParam) / impl->surface_scale;
  event->button.y = current_y = (gint16) GET_Y_LPARAM (msg->lParam) / impl->surface_scale;
  event->button.x_root = (msg->pt.x + _gdk_offset_x) / impl->surface_scale;
  event->button.y_root = (msg->pt.y + _gdk_offset_y) / impl->surface_scale;
  event->button.axes = NULL;
  event->button.state = build_pointer_event_state (msg);
  event->button.button = button;
  gdk_event_set_device (event, device_manager->core_pointer);
  gdk_event_set_source_device (event, device_manager->system_pointer);

  _gdk_win32_append_event (event);
}

/*
 * Used by the stacking functions to see if a window
 * should be always on top.
 * Restacking is only done if both windows are either ontop
 * or not ontop.
 */
static gboolean
should_window_be_always_on_top (GdkSurface *window)
{
  DWORD exstyle;

  if ((GDK_SURFACE_TYPE (window) == GDK_SURFACE_TEMP) ||
      (window->state & GDK_SURFACE_STATE_ABOVE))
    return TRUE;

  exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);

  if (exstyle & WS_EX_TOPMOST)
    return TRUE;

  return FALSE;
}

static void
restack_children (GdkSurface *window)
{
  GList *popup;

  for (popup = window->children; popup; popup = popup->next)
    {
      GdkSurface *child = GDK_SURFACE (popup->data);
      /* Windows doesn't have a function to put window A *above* window B.
       * Instead we put window A immediately *below* window B,
       * then put window B immediately below window A.
       * SWP_NOSENDCHANGING prevents our own handler from triggering.
       */
      SetWindowPos (GDK_SURFACE_HWND (child), GDK_SURFACE_HWND (window), 0, 0, 0, 0,
                    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
      g_print ("Put popup %p (%p) above %p (%p)\n", child, GDK_SURFACE_HWND (child), window, GDK_SURFACE_HWND (window));
      SetWindowPos (GDK_SURFACE_HWND (window), GDK_SURFACE_HWND (child), 0, 0, 0, 0,
                    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
    }
}

static void
ensure_stacking_on_unminimize (MSG *msg)
{
  HWND rover;
  HWND lowest_transient = NULL;
  GdkSurface *msg_window;
  gboolean window_ontop = FALSE;

  msg_window = gdk_win32_handle_table_lookup (msg->hwnd);

  if (msg_window)
    window_ontop = should_window_be_always_on_top (msg_window);

  for (rover = GetNextWindow (msg->hwnd, GW_HWNDNEXT);
       rover;
       rover = GetNextWindow (rover, GW_HWNDNEXT))
    {
      GdkSurface *rover_gdkw = gdk_win32_handle_table_lookup (rover);
      GdkWin32Surface *rover_impl;
      gboolean rover_ontop;

      /* Checking window group not implemented yet */
      if (rover_gdkw == NULL)
        continue;

      rover_ontop = should_window_be_always_on_top (rover_gdkw);
      rover_impl = GDK_WIN32_SURFACE (rover_gdkw);

      if (GDK_SURFACE_IS_MAPPED (rover_gdkw) &&
          (rover_impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY ||
           rover_impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
           rover_impl->transient_owner != NULL) &&
           ((window_ontop && rover_ontop) || (!window_ontop && !rover_ontop)))
        {
          lowest_transient = rover;
        }
    }

  if (lowest_transient != NULL)
    {
      GDK_NOTE (EVENTS,
		g_print (" restacking %p below %p",
			 msg->hwnd, lowest_transient));
      SetWindowPos (msg->hwnd, lowest_transient, 0, 0, 0, 0,
		    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
    }
}

static gboolean
ensure_stacking_on_window_pos_changing (MSG       *msg,
					GdkSurface *window)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  WINDOWPOS *windowpos = (WINDOWPOS *) msg->lParam;
  HWND rover;
  gboolean restacking;
  gboolean window_ontop;

  if (GetActiveWindow () != msg->hwnd ||
      impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY ||
      impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
      impl->transient_owner != NULL)
    return FALSE;

  /* Make sure the window stays behind any transient-type windows
   * of the same window group.
   *
   * If the window is not active and being activated, we let
   * Windows bring it to the top and rely on the WM_ACTIVATEAPP
   * handling to bring any utility windows on top of it.
   */

  window_ontop = should_window_be_always_on_top (window);

  for (rover = windowpos->hwndInsertAfter, restacking = FALSE;
       rover;
       rover = GetNextWindow (rover, GW_HWNDNEXT))
    {
      GdkSurface *rover_gdkw = gdk_win32_handle_table_lookup (rover);
      GdkWin32Surface *rover_impl;
      gboolean rover_ontop;

      /* Checking window group not implemented yet */

      if (rover_gdkw == NULL)
	continue;

      rover_ontop = should_window_be_always_on_top (rover_gdkw);
      rover_impl = GDK_WIN32_SURFACE (rover_gdkw);

      if (GDK_SURFACE_IS_MAPPED (rover_gdkw) &&
          (rover_impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY ||
           rover_impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
           rover_impl->transient_owner != NULL) &&
          ((window_ontop && rover_ontop) || (!window_ontop && !rover_ontop)))
        {
          restacking = TRUE;
          windowpos->hwndInsertAfter = rover;
        }
    }

  if (restacking)
    {
      GDK_NOTE (EVENTS,
		g_print (" letting Windows restack %p above %p",
			 msg->hwnd, windowpos->hwndInsertAfter));
      return TRUE;
    }

  return FALSE;
}

static void
ensure_stacking_on_activate_app (MSG       *msg,
				 GdkSurface *window)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  HWND rover;
  gboolean window_ontop;

  if (impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY ||
      impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
      impl->transient_owner != NULL)
    {
      GdkSurface *child = window;
      GdkSurface *owner = impl->transient_owner;

      SetWindowPos (msg->hwnd, HWND_TOP, 0, 0, 0, 0,
		    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
      /* Bing the whole hierarchy of transients back up */
      while (owner != NULL)
        {
          SetWindowPos (GDK_SURFACE_HWND (owner), GDK_SURFACE_HWND (child), 0, 0, 0, 0,
                        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
          restack_children (owner);
          child = owner;
          owner = GDK_WIN32_SURFACE (owner)->transient_owner;
        }
      return;
    }

  if (!IsWindowVisible (msg->hwnd) ||
      msg->hwnd != GetActiveWindow ())
    return;


  /* This window is not a transient-type window and it is the
   * activated window. Make sure this window is as visible as
   * possible, just below the lowest transient-type window of this
   * app.
   */

  window_ontop = should_window_be_always_on_top (window);

  for (rover = GetNextWindow (msg->hwnd, GW_HWNDPREV);
       rover;
       rover = GetNextWindow (rover, GW_HWNDPREV))
    {
      GdkSurface *rover_gdkw = gdk_win32_handle_table_lookup (rover);
      GdkWin32Surface *rover_impl;
      gboolean rover_ontop;

      /* Checking window group not implemented yet */
      if (rover_gdkw == NULL)
        continue;

      rover_ontop = should_window_be_always_on_top (rover_gdkw);
      rover_impl = GDK_WIN32_SURFACE (rover_gdkw);

      if (GDK_SURFACE_IS_MAPPED (rover_gdkw) &&
          (rover_impl->type_hint == GDK_SURFACE_TYPE_HINT_UTILITY ||
           rover_impl->type_hint == GDK_SURFACE_TYPE_HINT_DIALOG ||
           rover_impl->transient_owner != NULL) &&
          ((window_ontop && rover_ontop) || (!window_ontop && !rover_ontop)))
        {
	  GDK_NOTE (EVENTS,
		    g_print (" restacking %p below %p",
			     msg->hwnd, rover));
	  SetWindowPos (msg->hwnd, rover, 0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
          break;
	}
    }
}

static gboolean
handle_wm_sysmenu (GdkSurface *window, MSG *msg, gint *ret_valp)
{
  GdkWin32Surface *impl;
  LONG_PTR style, tmp_style;
  LONG_PTR additional_styles;

  impl = GDK_WIN32_SURFACE (window);

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

  /* Note: This code will enable resizing, maximizing and minimizing windows
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
   * check window hints here and return FALSE (DefWindowProc() will return
   * FALSE later) or set *ret_valp to 0 and return TRUE.
   */
  tmp_style = style | additional_styles;
  GDK_NOTE (EVENTS, g_print (" Handling WM_SYSMENU: style 0x%lx -> 0x%lx\n", style, tmp_style));
  impl->have_temp_styles = TRUE;
  impl->temp_styles = additional_styles;
  SetWindowLongPtr (msg->hwnd, GWL_STYLE, tmp_style);

  *ret_valp = DefWindowProc (msg->hwnd, msg->message, msg->wParam, msg->lParam);

  tmp_style = GetWindowLongPtr (msg->hwnd, GWL_STYLE);
  style = tmp_style & ~additional_styles;

  GDK_NOTE (EVENTS, g_print (" Handling WM_SYSMENU: style 0x%lx <- 0x%lx\n", style, tmp_style));
  SetWindowLongPtr (msg->hwnd, GWL_STYLE, style);
  impl->have_temp_styles = FALSE;

  return TRUE;
}

gboolean
_gdk_win32_surface_fill_min_max_info (GdkSurface  *window,
                                     MINMAXINFO *mmi)
{
  GdkWin32Surface *impl;
  RECT rect;

  if (GDK_SURFACE_DESTROYED (window))
    return FALSE;

  impl = GDK_WIN32_SURFACE (window);

  if (impl->hint_flags & GDK_HINT_MIN_SIZE)
    {
      rect.left = rect.top = 0;
      rect.right = impl->hints.min_width * impl->surface_scale;
      rect.bottom = impl->hints.min_height * impl->surface_scale;

      _gdk_win32_adjust_client_rect (window, &rect);

      mmi->ptMinTrackSize.x = rect.right - rect.left;
      mmi->ptMinTrackSize.y = rect.bottom - rect.top;
    }

  if (impl->hint_flags & GDK_HINT_MAX_SIZE)
    {
      int maxw, maxh;

      rect.left = rect.top = 0;
      rect.right = impl->hints.max_width * impl->surface_scale;
      rect.bottom = impl->hints.max_height * impl->surface_scale;

      _gdk_win32_adjust_client_rect (window, &rect);

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

      nearest_monitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
      nearest_info.cbSize = sizeof (nearest_info);

      if (GetMonitorInfoA (nearest_monitor, &nearest_info))
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

          if (_gdk_win32_surface_lacks_wm_decorations (window))
            {
              mmi->ptMaxPosition.x += (nearest_info.rcWork.left - nearest_info.rcMonitor.left);
              mmi->ptMaxPosition.y += (nearest_info.rcWork.top - nearest_info.rcMonitor.top);
            }

          mmi->ptMaxSize.x = nearest_info.rcWork.right - nearest_info.rcWork.left;
          mmi->ptMaxSize.y = nearest_info.rcWork.bottom - nearest_info.rcWork.top;
        }

      mmi->ptMaxTrackSize.x = GetSystemMetrics (SM_CXVIRTUALSCREEN) + impl->margins_x * impl->surface_scale;
      mmi->ptMaxTrackSize.y = GetSystemMetrics (SM_CYVIRTUALSCREEN) + impl->margins_y * impl->surface_scale;
    }

  return TRUE;
}

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
			     GDK_BUTTON2_MASK | \
			     GDK_BUTTON3_MASK | \
			     GDK_BUTTON4_MASK | \
			     GDK_BUTTON5_MASK)

static gboolean
gdk_event_translate (MSG  *msg,
		     gint *ret_valp)
{
  RECT rect, *drag, orig_drag;
  POINT point;
  MINMAXINFO *mmi;
  HWND hwnd;
  BYTE key_state[256];
  HIMC himc;
  WINDOWPOS *windowpos;
  gboolean ignore_leave;

  GdkEvent *event;

  wchar_t wbuf[100];
  gint ccount;

  GdkDisplay *display;
  GdkSurface *window = NULL;
  GdkWin32Surface *impl;
  GdkWin32Display *win32_display;

  GdkSurface *new_window;

  GdkDeviceManagerWin32 *device_manager_win32;

  GdkDeviceGrabInfo *keyboard_grab = NULL;
  GdkDeviceGrabInfo *pointer_grab = NULL;
  GdkSurface *grab_window = NULL;

  gint button;

  gchar buf[256];
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

  window = gdk_win32_handle_table_lookup (msg->hwnd);

  if (window == NULL)
    {
      /* XXX Handle WM_QUIT here ? */
      if (msg->message == WM_QUIT)
	{
	  GDK_NOTE (EVENTS, g_print (" %d", (int) msg->wParam));
	  exit (msg->wParam);
	}
      else if (msg->message == WM_CREATE)
	{
	  window = (UNALIGNED GdkSurface*) (((LPCREATESTRUCTW) msg->lParam)->lpCreateParams);
	  GDK_SURFACE_HWND (window) = msg->hwnd;
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print (" (no GdkSurface)"));
	}
      return FALSE;
    }

  device_manager_win32 = GDK_DEVICE_MANAGER_WIN32 (_gdk_device_manager);

  keyboard_grab = _gdk_display_get_last_device_grab (display,
                                                     device_manager_win32->core_keyboard);
  pointer_grab = _gdk_display_get_last_device_grab (display,
                                                    device_manager_win32->core_pointer);

  g_object_ref (window);

  /* window's refcount has now been increased, so code below should
   * not just return from this function, but instead goto done (or
   * break out of the big switch). To protect against forgetting this,
   * #define return to a syntax error...
   */
#define return GOTO_DONE_INSTEAD

  if (msg->message == aerosnap_message)
    _gdk_win32_surface_handle_aerosnap (window,
                                       (GdkWin32AeroSnapCombo) msg->wParam);

  switch (msg->message)
    {
    case WM_INPUTLANGCHANGE:
      _gdk_input_locale = (HKL) msg->lParam;
      _gdk_win32_keymap_set_active_layout (GDK_WIN32_KEYMAP (_gdk_win32_display_get_keymap (_gdk_display)), _gdk_input_locale);
      GetLocaleInfo (MAKELCID (LOWORD (_gdk_input_locale), SORT_DEFAULT),
		     LOCALE_IDEFAULTANSICODEPAGE,
		     buf, sizeof (buf));
      _gdk_input_codepage = atoi (buf);
      _gdk_keymap_serial++;
      GDK_NOTE (EVENTS,
		g_print (" cs:%lu hkl:%p%s cp:%d",
			 (gulong) msg->wParam,
			 (gpointer) msg->lParam, _gdk_input_locale_is_ime ? " (IME)" : "",
			 _gdk_input_codepage));
      gdk_display_setting_changed (display, "gtk-im-module");

      /* Generate a dummy key event to "nudge" IMContext */
      event = gdk_event_new (GDK_KEY_PRESS);
      event->any.surface = window;
      event->key.time = _gdk_win32_get_next_tick (msg->time);
      event->key.keyval = GDK_KEY_VoidSymbol;
      event->key.hardware_keycode = 0;
      event->key.group = 0;
      gdk_event_set_scancode (event, 0);
      gdk_event_set_device (event, device_manager_win32->core_keyboard);
      gdk_event_set_source_device (event, device_manager_win32->system_keyboard);
      event->key.is_modifier = FALSE;
      event->key.state = 0;
      _gdk_win32_append_event (event);
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

      /* Ignore key messages intended for the IME */
      if (msg->wParam == VK_PROCESSKEY ||
	  in_ime_composition)
	break;

      /* Ignore autorepeats on modifiers */
      if (msg->message == WM_KEYDOWN &&
          (msg->wParam == VK_MENU ||
           msg->wParam == VK_CONTROL ||
           msg->wParam == VK_SHIFT) &&
           ((HIWORD(msg->lParam) & KF_REPEAT) >= 1))
        break;

      if (GDK_SURFACE_DESTROYED (window))
	break;

      impl = GDK_WIN32_SURFACE (window);

      API_CALL (GetKeyboardState, (key_state));

      ccount = 0;

      if (msg->wParam == VK_PACKET)
	{
	  ccount = ToUnicode (VK_PACKET, HIWORD (msg->lParam), key_state, wbuf, 1, 0);
	  if (ccount == 1)
	    {
	      if (wbuf[0] >= 0xD800 && wbuf[0] < 0xDC00)
	        {
		  if (msg->message == WM_KEYDOWN)
		    impl->leading_surrogate_keydown = wbuf[0];
		  else
		    impl->leading_surrogate_keyup = wbuf[0];

		  /* don't emit an event */
		  return_val = TRUE;
		  break;
	        }
	      else
		{
		  /* wait until an event is created */;
		}
	    }
	}

      event = gdk_event_new ((msg->message == WM_KEYDOWN ||
			      msg->message == WM_SYSKEYDOWN) ?
			     GDK_KEY_PRESS : GDK_KEY_RELEASE);
      event->any.surface = window;
      event->key.time = _gdk_win32_get_next_tick (msg->time);
      event->key.keyval = GDK_KEY_VoidSymbol;
      event->key.hardware_keycode = msg->wParam;
      /* save original scancode */
      gdk_event_set_scancode (event, msg->lParam >> 16);
      gdk_event_set_device (event, device_manager_win32->core_keyboard);
      gdk_event_set_source_device (event, device_manager_win32->system_keyboard);
      if (HIWORD (msg->lParam) & KF_EXTENDED)
	{
	  switch (msg->wParam)
	    {
	    case VK_CONTROL:
	      event->key.hardware_keycode = VK_RCONTROL;
	      break;
	    case VK_SHIFT:	/* Actually, KF_EXTENDED is not set
				 * for the right shift key.
				 */
	      event->key.hardware_keycode = VK_RSHIFT;
	      break;
	    case VK_MENU:
	      event->key.hardware_keycode = VK_RMENU;
	      break;
	    }
	}
      else if (msg->wParam == VK_SHIFT &&
	       LOBYTE (HIWORD (msg->lParam)) == _gdk_win32_keymap_get_rshift_scancode (GDK_WIN32_KEYMAP (_gdk_win32_display_get_keymap (_gdk_display))))
	event->key.hardware_keycode = VK_RSHIFT;

      event->key.is_modifier = (msg->wParam == VK_CONTROL ||
                                msg->wParam == VK_SHIFT ||
                                msg->wParam == VK_MENU);
      /* g_print ("ctrl:%02x lctrl:%02x rctrl:%02x alt:%02x lalt:%02x ralt:%02x\n", key_state[VK_CONTROL], key_state[VK_LCONTROL], key_state[VK_RCONTROL], key_state[VK_MENU], key_state[VK_LMENU], key_state[VK_RMENU]); */

      build_key_event_state (event, key_state);

      if (msg->wParam == VK_PACKET && ccount == 1)
	{
	  if (wbuf[0] >= 0xD800 && wbuf[0] < 0xDC00)
	    {
	      g_assert_not_reached ();
	    }
	  else if (wbuf[0] >= 0xDC00 && wbuf[0] < 0xE000)
	    {
	      wchar_t leading;

              if (msg->message == WM_KEYDOWN)
		leading = impl->leading_surrogate_keydown;
	      else
		leading = impl->leading_surrogate_keyup;

	      event->key.keyval = gdk_unicode_to_keyval ((leading - 0xD800) * 0x400 + wbuf[0] - 0xDC00 + 0x10000);
	    }
	  else
	    {
	      event->key.keyval = gdk_unicode_to_keyval (wbuf[0]);
	    }
	}
      else
	{
	  gdk_keymap_translate_keyboard_state (_gdk_win32_display_get_keymap (display),
					       event->key.hardware_keycode,
					       event->key.state,
					       event->key.group,
					       &event->key.keyval,
					       NULL, NULL, NULL);
	}

      if (msg->message == WM_KEYDOWN)
	impl->leading_surrogate_keydown = 0;
      else
	impl->leading_surrogate_keyup = 0;

  /* Only one release key event is fired when both shift keys are pressed together
     and then released. In order to send the missing event, press events for shift
     keys are recorded and sent together when the release event occurs.
     Other modifiers (e.g. ctrl, alt) don't have this problem. */
  if (msg->message == WM_KEYDOWN && msg->wParam == VK_SHIFT)
    {
      int pressed_shift = msg->lParam & 0xffffff; /* mask shift modifier */
      if (both_shift_pressed[0] == 0)
        both_shift_pressed[0] = pressed_shift;
      else if (both_shift_pressed[0] != pressed_shift)
        both_shift_pressed[1] = pressed_shift;
    }

  if (msg->message == WM_KEYUP && msg->wParam == VK_SHIFT)
    {
      if (both_shift_pressed[0] != 0 && both_shift_pressed[1] != 0)
        {
          gint tmp_retval;
          MSG fake_release = *msg;
          int pressed_shift = msg->lParam & 0xffffff;

          if (both_shift_pressed[0] == pressed_shift)
            fake_release.lParam = both_shift_pressed[1];
          else
            fake_release.lParam = both_shift_pressed[0];

          both_shift_pressed[0] = both_shift_pressed[1] = 0;
          gdk_event_translate (&fake_release, &tmp_retval);
        }
      both_shift_pressed[0] = both_shift_pressed[1] = 0;
    }

      /* Reset MOD1_MASK if it is the Alt key itself */
      if (msg->wParam == VK_MENU)
	event->key.state &= ~GDK_MOD1_MASK;

      _gdk_win32_append_event (event);

      return_val = TRUE;
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
      in_ime_composition = TRUE;
      break;

    case WM_IME_ENDCOMPOSITION:
      in_ime_composition = FALSE;
      break;

    case WM_IME_COMPOSITION:
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

      if (GDK_SURFACE_DESTROYED (window))
	break;

      himc = ImmGetContext (msg->hwnd);
      ccount = ImmGetCompositionStringW (himc, GCS_RESULTSTR,
					 wbuf, sizeof (wbuf));
      ImmReleaseContext (msg->hwnd, himc);

      ccount /= 2;

      API_CALL (GetKeyboardState, (key_state));

      for (i = 0; i < ccount; i++)
	{
          /* Build a key press event */
          event = gdk_event_new (GDK_KEY_PRESS);
          event->any.surface = window;
          gdk_event_set_device (event, device_manager_win32->core_keyboard);
          gdk_event_set_source_device (event, device_manager_win32->system_keyboard);
          build_wm_ime_composition_event (event, msg, wbuf[i], key_state);

          _gdk_win32_append_event (event);

          /* Build a key release event.  */
          event = gdk_event_new (GDK_KEY_RELEASE);
          event->any.surface = window;
          gdk_event_set_device (event, device_manager_win32->core_keyboard);
          gdk_event_set_source_device (event, device_manager_win32->system_keyboard);
          build_wm_ime_composition_event (event, msg, wbuf[i], key_state);

          _gdk_win32_append_event (event);
	}
      return_val = TRUE;
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

      g_set_object (&window, find_window_for_mouse_event (window, msg));
      /* TODO_CSW?: there used to some synthesize and propagate */
      if (GDK_SURFACE_DESTROYED (window))
	break;

      if (pointer_grab == NULL)
	{
	  SetCapture (GDK_SURFACE_HWND (window));
	  g_set_object (&implicit_grab_surface, g_object_ref (window));
	}
      else
	g_set_object (&implicit_grab_surface, NULL);

      generate_button_event (GDK_BUTTON_PRESS, button,
			     window, msg);

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
      GDK_NOTE (EVENTS,
		g_print (" (%d,%d)",
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      g_set_object (&window, find_window_for_mouse_event (window, msg));

      if (pointer_grab == NULL && implicit_grab_surface != NULL)
	{
	  gint state = build_pointer_event_state (msg);

	  /* We keep the implicit grab until no buttons at all are held down */
	  if ((state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (button - 1))) == 0)
	    {
	      ReleaseCapture ();

	      new_window = NULL;
	      hwnd = WindowFromPoint (msg->pt);
	      if (hwnd != NULL)
		{
		  POINT client_pt = msg->pt;

		  ScreenToClient (hwnd, &client_pt);
		  GetClientRect (hwnd, &rect);
		  if (PtInRect (&rect, client_pt))
		    new_window = gdk_win32_handle_table_lookup (hwnd);
		}

	      synthesize_crossing_events (display,
					  implicit_grab_surface, new_window,
					  GDK_CROSSING_UNGRAB,
					  &msg->pt,
					  0, /* TODO: Set right mask */
					  msg->time,
					  FALSE);
	      g_set_object (&implicit_grab_surface, NULL);
	      g_set_object (&mouse_window, new_window);
	      mouse_window_ignored_leave = NULL;
	    }
	}
      else
	g_set_object (&implicit_grab_surface, NULL);

      generate_button_event (GDK_BUTTON_RELEASE, button,
			     window, msg);

      impl = GDK_WIN32_SURFACE (window);

      /* End a drag op when the same button that started it is released */
      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE &&
          impl->drag_move_resize_context.button == button)
        gdk_win32_surface_end_move_resize_drag (window);

      return_val = TRUE;
      break;

    case WM_MOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print (" %p (%d,%d)",
			 (gpointer) msg->wParam,
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      new_window = window;

      if (pointer_grab != NULL)
	{
	  POINT pt;
	  pt = msg->pt;

	  new_window = NULL;
	  hwnd = WindowFromPoint (pt);
	  if (hwnd != NULL)
	    {
	      POINT client_pt = pt;

	      ScreenToClient (hwnd, &client_pt);
	      GetClientRect (hwnd, &rect);
	      if (PtInRect (&rect, client_pt))
		new_window = gdk_win32_handle_table_lookup (hwnd);
	    }

	  if (!pointer_grab->owner_events &&
	      new_window != NULL &&
	      new_window != pointer_grab->surface)
	    new_window = NULL;
	}

      if (mouse_window != new_window)
	{
	  GDK_NOTE (EVENTS, g_print (" mouse_sinwod %p -> %p",
				     mouse_window ? GDK_SURFACE_HWND (mouse_window) : NULL,
				     new_window ? GDK_SURFACE_HWND (new_window) : NULL));
	  synthesize_crossing_events (display,
				      mouse_window, new_window,
				      GDK_CROSSING_NORMAL,
				      &msg->pt,
				      0, /* TODO: Set right mask */
				      msg->time,
				      FALSE);
	  g_set_object (&mouse_window, new_window);
	  mouse_window_ignored_leave = NULL;
	  if (new_window != NULL)
	    track_mouse_event (TME_LEAVE, GDK_SURFACE_HWND (new_window));
	}
      else if (new_window != NULL &&
	       new_window == mouse_window_ignored_leave)
	{
	  /* If we ignored a leave event for this window and we're now getting
	     input again we need to re-arm the mouse tracking, as that was
	     cancelled by the mouseleave. */
	  mouse_window_ignored_leave = NULL;
	  track_mouse_event (TME_LEAVE, GDK_SURFACE_HWND (new_window));
	}

      g_set_object (&window, find_window_for_mouse_event (window, msg));
      impl = GDK_WIN32_SURFACE (window);

      /* If we haven't moved, don't create any GDK event. Windows
       * sends WM_MOUSEMOVE messages after a new window is shows under
       * the mouse, even if the mouse hasn't moved. This disturbs gtk.
       */
      if ((msg->pt.x + _gdk_offset_x) / impl->surface_scale == current_root_x &&
	  (msg->pt.y + _gdk_offset_y) / impl->surface_scale == current_root_y)
	break;

      current_root_x = (msg->pt.x + _gdk_offset_x) / impl->surface_scale;
      current_root_y = (msg->pt.y + _gdk_offset_y) / impl->surface_scale;


      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
        {
          gdk_win32_surface_do_move_resize_drag (window, current_root_x, current_root_y);
        }
      else if (_gdk_input_ignore_core == 0)
	{
	  event = gdk_event_new (GDK_MOTION_NOTIFY);
	  event->any.surface = window;
	  event->motion.time = _gdk_win32_get_next_tick (msg->time);
	  event->motion.x = current_x = (gint16) GET_X_LPARAM (msg->lParam) / impl->surface_scale;
	  event->motion.y = current_y = (gint16) GET_Y_LPARAM (msg->lParam) / impl->surface_scale;
	  event->motion.x_root = current_root_x;
	  event->motion.y_root = current_root_y;
	  event->motion.axes = NULL;
	  event->motion.state = build_pointer_event_state (msg);
	  gdk_event_set_device (event, device_manager_win32->core_pointer);
	  gdk_event_set_source_device (event, device_manager_win32->system_pointer);

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

      new_window = NULL;
      hwnd = WindowFromPoint (msg->pt);
      ignore_leave = FALSE;
      if (hwnd != NULL)
	{
	  char classname[64];

	  POINT client_pt = msg->pt;

	  /* The synapitics trackpad drivers have this irritating
	     feature where it pops up a window right under the pointer
	     when you scroll. We ignore the leave and enter events for
	     this window */
	  if (GetClassNameA (hwnd, classname, sizeof(classname)) &&
	      strcmp (classname, SYNAPSIS_ICON_WINDOW_CLASS) == 0)
	    ignore_leave = TRUE;

	  ScreenToClient (hwnd, &client_pt);
	  GetClientRect (hwnd, &rect);
	  if (PtInRect (&rect, client_pt))
	    new_window = gdk_win32_handle_table_lookup (hwnd);
	}

      if (!ignore_leave)
	synthesize_crossing_events (display,
				    mouse_window, new_window,
				    GDK_CROSSING_NORMAL,
				    &msg->pt,
				    0, /* TODO: Set right mask */
				    msg->time,
				    FALSE);
      g_set_object (&mouse_window, new_window);
      mouse_window_ignored_leave = ignore_leave ? new_window : NULL;


      return_val = TRUE;
      break;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      GDK_NOTE (EVENTS, g_print (" %d", (short) HIWORD (msg->wParam)));

      /* WM_MOUSEWHEEL is delivered to the focus window. Work around
       * that. Also, the position is in screen coordinates, not client
       * coordinates as with the button messages. I love the
       * consistency of Windows.
       */
      point.x = GET_X_LPARAM (msg->lParam);
      point.y = GET_Y_LPARAM (msg->lParam);

      if ((hwnd = WindowFromPoint (point)) == NULL)
	break;

      {
	char classname[64];

	/* The synapitics trackpad drivers have this irritating
	   feature where it pops up a window right under the pointer
	   when you scroll. We backtrack and to the toplevel and
	   find the innermost child instead. */
	if (GetClassNameA (hwnd, classname, sizeof(classname)) &&
	    strcmp (classname, SYNAPSIS_ICON_WINDOW_CLASS) == 0)
	  {
	    HWND hwndc;

	    /* Find our toplevel window */
	    hwnd = GetAncestor (msg->hwnd, GA_ROOT);

	    /* Walk back up to the outermost child at the desired point */
	    do {
	      ScreenToClient (hwnd, &point);
	      hwndc = ChildWindowFromPoint (hwnd, point);
	      ClientToScreen (hwnd, &point);
	    } while (hwndc != hwnd && (hwnd = hwndc, 1));
	  }
      }

      msg->hwnd = hwnd;
      if ((new_window = gdk_win32_handle_table_lookup (msg->hwnd)) == NULL)
	break;

      if (new_window != window)
	{
	  g_set_object (&window, new_window);
	}

      impl = GDK_WIN32_SURFACE (window);
      ScreenToClient (msg->hwnd, &point);

      event = gdk_event_new (GDK_SCROLL);
      event->any.surface = window;
      event->scroll.direction = GDK_SCROLL_SMOOTH;

      if (msg->message == WM_MOUSEWHEEL)
        {
          event->scroll.delta_y = (gdouble) GET_WHEEL_DELTA_WPARAM (msg->wParam) / (gdouble) WHEEL_DELTA;
        }
      else if (msg->message == WM_MOUSEHWHEEL)
        {
          event->scroll.delta_x = (gdouble) GET_WHEEL_DELTA_WPARAM (msg->wParam) / (gdouble) WHEEL_DELTA;
        }
      /* Positive delta scrolls up, not down,
         see API documentation for WM_MOUSEWHEEL message.
       */
      event->scroll.delta_y *= -1.0;
      event->scroll.time = _gdk_win32_get_next_tick (msg->time);
      event->scroll.x = (gint16) point.x / impl->surface_scale;
      event->scroll.y = (gint16) point.y / impl->surface_scale;
      event->scroll.x_root = ((gint16) GET_X_LPARAM (msg->lParam) + _gdk_offset_x) / impl->surface_scale;
      event->scroll.y_root = ((gint16) GET_Y_LPARAM (msg->lParam) + _gdk_offset_y) / impl->surface_scale;
      event->scroll.state = build_pointer_event_state (msg);
      gdk_event_set_device (event, device_manager_win32->core_pointer);
      gdk_event_set_source_device (event, device_manager_win32->system_pointer);
      gdk_event_set_pointer_emulated (event, FALSE);

      _gdk_win32_append_event (gdk_event_copy (event));

      /* Append the discrete version too */
      if (msg->message == WM_MOUSEWHEEL)
	event->scroll.direction = (((short) HIWORD (msg->wParam)) > 0) ?
	  GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      else if (msg->message == WM_MOUSEHWHEEL)
	event->scroll.direction = (((short) HIWORD (msg->wParam)) > 0) ?
	  GDK_SCROLL_RIGHT : GDK_SCROLL_LEFT;
      event->scroll.delta_x = 0;
      event->scroll.delta_y = 0;
      gdk_event_set_pointer_emulated (event, TRUE);

      _gdk_win32_append_event (event);

      return_val = TRUE;
      break;

    case WM_HSCROLL:
      /* Just print more debugging information, don't actually handle it. */
      GDK_NOTE (EVENTS,
		(g_print (" %s",
			  (LOWORD (msg->wParam) == SB_ENDSCROLL ? "ENDSCROLL" :
			   (LOWORD (msg->wParam) == SB_LEFT ? "LEFT" :
			    (LOWORD (msg->wParam) == SB_RIGHT ? "RIGHT" :
			     (LOWORD (msg->wParam) == SB_LINELEFT ? "LINELEFT" :
			      (LOWORD (msg->wParam) == SB_LINERIGHT ? "LINERIGHT" :
			       (LOWORD (msg->wParam) == SB_PAGELEFT ? "PAGELEFT" :
				(LOWORD (msg->wParam) == SB_PAGERIGHT ? "PAGERIGHT" :
				 (LOWORD (msg->wParam) == SB_THUMBPOSITION ? "THUMBPOSITION" :
				  (LOWORD (msg->wParam) == SB_THUMBTRACK ? "THUMBTRACK" :
				   "???")))))))))),
		 (LOWORD (msg->wParam) == SB_THUMBPOSITION ||
		  LOWORD (msg->wParam) == SB_THUMBTRACK) ?
		 (g_print (" %d", HIWORD (msg->wParam)), 0) : 0));
      break;

    case WM_VSCROLL:
      /* Just print more debugging information, don't actually handle it. */
      GDK_NOTE (EVENTS,
		(g_print (" %s",
			  (LOWORD (msg->wParam) == SB_ENDSCROLL ? "ENDSCROLL" :
			   (LOWORD (msg->wParam) == SB_BOTTOM ? "BOTTOM" :
			    (LOWORD (msg->wParam) == SB_TOP ? "TOP" :
			     (LOWORD (msg->wParam) == SB_LINEDOWN ? "LINDOWN" :
			      (LOWORD (msg->wParam) == SB_LINEUP ? "LINEUP" :
			       (LOWORD (msg->wParam) == SB_PAGEDOWN ? "PAGEDOWN" :
				(LOWORD (msg->wParam) == SB_PAGEUP ? "PAGEUP" :
				 (LOWORD (msg->wParam) == SB_THUMBPOSITION ? "THUMBPOSITION" :
				  (LOWORD (msg->wParam) == SB_THUMBTRACK ? "THUMBTRACK" :
				   "???")))))))))),
		 (LOWORD (msg->wParam) == SB_THUMBPOSITION ||
		  LOWORD (msg->wParam) == SB_THUMBTRACK) ?
		 (g_print (" %d", HIWORD (msg->wParam)), 0) : 0));
      break;

     case WM_MOUSEACTIVATE:
       {
	 if (gdk_surface_get_surface_type (window) == GDK_SURFACE_TEMP
	     || !window->accept_focus)
	   {
	     *ret_valp = MA_NOACTIVATE;
	     return_val = TRUE;
	   }

	 if (_gdk_modal_blocked (window))
	   {
	     *ret_valp = MA_NOACTIVATEANDEAT;
	     return_val = TRUE;
	   }
       }

       break;

    case WM_KILLFOCUS:
      if (keyboard_grab != NULL &&
	  !GDK_SURFACE_DESTROYED (keyboard_grab->surface) &&
	  (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_DND) == 0)
	{
	  generate_grab_broken_event (_gdk_device_manager, keyboard_grab->surface, TRUE, NULL);
	}
      G_GNUC_FALLTHROUGH;

    case WM_SETFOCUS:
      if (keyboard_grab != NULL &&
	  !keyboard_grab->owner_events)
	break;

      if (GDK_SURFACE_DESTROYED (window))
	break;

      generate_focus_event (_gdk_device_manager, window, (msg->message == WM_SETFOCUS));
      return_val = TRUE;
      break;

    case WM_ERASEBKGND:
      GDK_NOTE (EVENTS, g_print (" %p", (HANDLE) msg->wParam));

      if (GDK_SURFACE_DESTROYED (window))
	break;

      return_val = TRUE;
      *ret_valp = 1;
      break;

    case WM_SYNCPAINT:
      sync_timer = SetTimer (GDK_SURFACE_HWND (window),
			     1,
			     200, sync_timer_proc);
      break;

    case WM_PAINT:
      handle_wm_paint (msg, window);
      break;

    case WM_SETCURSOR:
      GDK_NOTE (EVENTS, g_print (" %#x %#x",
				 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (pointer_grab != NULL)
        grab_window = pointer_grab->surface;

      if (grab_window == NULL && LOWORD (msg->lParam) != HTCLIENT)
	break;

      return_val = FALSE;

      if (grab_window != NULL &&
          !GDK_SURFACE_DESTROYED (grab_window))
        {
          win32_display = GDK_WIN32_DISPLAY (gdk_surface_get_display (grab_window));

          if (win32_display->grab_cursor != NULL)
            {
              GDK_NOTE (EVENTS, g_print (" (grab SetCursor(%p)", gdk_win32_hcursor_get_handle (win32_display->grab_cursor)));
              SetCursor (gdk_win32_hcursor_get_handle (win32_display->grab_cursor));
              return_val = TRUE;
              *ret_valp = TRUE;
            }
        }

      if (!return_val &&
          !GDK_SURFACE_DESTROYED (window) &&
          GDK_WIN32_SURFACE (window)->cursor != NULL)
        {
          win32_display = GDK_WIN32_DISPLAY (gdk_surface_get_display (window));
          GDK_NOTE (EVENTS, g_print (" (window SetCursor(%p)", gdk_win32_hcursor_get_handle (GDK_WIN32_SURFACE (window)->cursor)));
          SetCursor (gdk_win32_hcursor_get_handle (GDK_WIN32_SURFACE (window)->cursor));
          return_val = TRUE;
          *ret_valp = TRUE;
        }

      break;

    case WM_SYSMENU:
      return_val = handle_wm_sysmenu (window, msg, ret_valp);
      break;

    case WM_INITMENU:
      impl = GDK_WIN32_SURFACE (window);

      if (impl->have_temp_styles)
        {
          LONG_PTR window_style;

          window_style = GetWindowLongPtr (GDK_SURFACE_HWND (window),
                                           GWL_STYLE);
          /* Handling WM_SYSMENU added extra styles to this window,
           * remove them now.
           */
          window_style &= ~impl->temp_styles;
          SetWindowLongPtr (GDK_SURFACE_HWND (window),
                            GWL_STYLE,
                            window_style);
        }

      break;

    case WM_SYSCOMMAND:
      switch (msg->wParam)
	{
	case SC_MINIMIZE:
	case SC_RESTORE:
	  do_show_window (window, msg->wParam == SC_MINIMIZE ? TRUE : FALSE);
	  break;
        case SC_MAXIMIZE:
          impl = GDK_WIN32_SURFACE (window);
          impl->maximizing = TRUE;
	  break;
	}

      break;

    case WM_ENTERSIZEMOVE:
      _modal_move_resize_window = msg->hwnd;
      _gdk_win32_begin_modal_call (GDK_WIN32_MODAL_OP_SIZEMOVE_MASK);
      break;

    case WM_EXITSIZEMOVE:
      if (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
	{
	  _modal_move_resize_window = NULL;
	  _gdk_win32_end_modal_call (GDK_WIN32_MODAL_OP_SIZEMOVE_MASK);
	}
      break;

    case WM_ENTERMENULOOP:
      _gdk_win32_begin_modal_call (GDK_WIN32_MODAL_OP_MENU);
      break;

    case WM_EXITMENULOOP:
      if (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_MENU)
	_gdk_win32_end_modal_call (GDK_WIN32_MODAL_OP_MENU);
      break;

      break;

    /*
     * Handle WM_CANCELMODE and do nothing in response to it when DnD is
     * active. Otherwise pass it to DefWindowProc, which will call ReleaseCapture()
     * on our behalf.
     * This prevents us from losing mouse capture when alt-tabbing during DnD
     * (this includes the feature of Windows Explorer where dragging stuff over
     * a window button in the taskbar causes that window to receive focus, i.e.
     * keyboardless alt-tabbing).
     */
    case WM_CANCELMODE:
      if (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_DND)
        {
          return_val = TRUE;
          *ret_valp = 0;
        }
      break;

    case WM_CAPTURECHANGED:
      /* Sometimes we don't get WM_EXITSIZEMOVE, for instance when you
	 select move/size in the menu and then click somewhere without
	 moving/resizing. We work around this using WM_CAPTURECHANGED. */
      if (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
	{
	  _modal_move_resize_window = NULL;
	  _gdk_win32_end_modal_call (GDK_WIN32_MODAL_OP_SIZEMOVE_MASK);
	}

      impl = GDK_WIN32_SURFACE (window);

      if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
        gdk_win32_surface_end_move_resize_drag (window);
      break;

    case WM_WINDOWPOSCHANGING:
      GDK_NOTE (EVENTS, (windowpos = (WINDOWPOS *) msg->lParam,
			 g_print (" %s %s %dx%d@%+d%+d now below %p",
				  _gdk_win32_surface_pos_bits_to_string (windowpos->flags),
				  (windowpos->hwndInsertAfter == HWND_BOTTOM ? "BOTTOM" :
				   (windowpos->hwndInsertAfter == HWND_NOTOPMOST ? "NOTOPMOST" :
				    (windowpos->hwndInsertAfter == HWND_TOP ? "TOP" :
				     (windowpos->hwndInsertAfter == HWND_TOPMOST ? "TOPMOST" :
				      (sprintf (buf, "%p", windowpos->hwndInsertAfter),
				       buf))))),
				  windowpos->cx, windowpos->cy, windowpos->x, windowpos->y,
				  GetNextWindow (msg->hwnd, GW_HWNDPREV))));

      if (GDK_SURFACE_IS_MAPPED (window))
        {
	  return_val = ensure_stacking_on_window_pos_changing (msg, window);

          impl = GDK_WIN32_SURFACE (window);

          if (impl->maximizing)
            {
              MINMAXINFO our_mmi;

              if (_gdk_win32_surface_fill_min_max_info (window, &our_mmi))
                {
                  windowpos = (WINDOWPOS *) msg->lParam;
                  windowpos->cx = our_mmi.ptMaxSize.x;
                  windowpos->cy = our_mmi.ptMaxSize.y;
                }

              impl->maximizing = FALSE;
            }
        }

      break;

    case WM_WINDOWPOSCHANGED:
      windowpos = (WINDOWPOS *) msg->lParam;
      GDK_NOTE (EVENTS, g_print (" %s %s %dx%d@%+d%+d",
				 _gdk_win32_surface_pos_bits_to_string (windowpos->flags),
				 (windowpos->hwndInsertAfter == HWND_BOTTOM ? "BOTTOM" :
				  (windowpos->hwndInsertAfter == HWND_NOTOPMOST ? "NOTOPMOST" :
				   (windowpos->hwndInsertAfter == HWND_TOP ? "TOP" :
				    (windowpos->hwndInsertAfter == HWND_TOPMOST ? "TOPMOST" :
				     (sprintf (buf, "%p", windowpos->hwndInsertAfter),
				      buf))))),
				 windowpos->cx, windowpos->cy, windowpos->x, windowpos->y));

      /* Break grabs on unmap or minimize */
      if (windowpos->flags & SWP_HIDEWINDOW ||
	  ((windowpos->flags & SWP_STATECHANGED) && IsIconic (msg->hwnd)))
      {
        GdkDevice *device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

        if ((pointer_grab != NULL && pointer_grab->surface == window) ||
            (keyboard_grab != NULL && keyboard_grab->surface == window))
          gdk_device_ungrab (device, msg -> time);
    }

      /* Update window state */
      if (windowpos->flags & (SWP_STATECHANGED | SWP_SHOWWINDOW | SWP_HIDEWINDOW))
	{
	  GdkSurfaceState set_bits, unset_bits, old_state, new_state;

	  old_state = window->state;

	  set_bits = 0;
	  unset_bits = 0;

	  if (IsWindowVisible (msg->hwnd))
	    unset_bits |= GDK_SURFACE_STATE_WITHDRAWN;
	  else
	    set_bits |= GDK_SURFACE_STATE_WITHDRAWN;

	  if (IsIconic (msg->hwnd))
	    set_bits |= GDK_SURFACE_STATE_ICONIFIED;
	  else
	    unset_bits |= GDK_SURFACE_STATE_ICONIFIED;

	  if (IsZoomed (msg->hwnd))
	    set_bits |= GDK_SURFACE_STATE_MAXIMIZED;
	  else
	    unset_bits |= GDK_SURFACE_STATE_MAXIMIZED;

	  gdk_synthesize_surface_state (window, unset_bits, set_bits);

	  new_state = window->state;

	  /* Whenever one window changes iconified state we need to also
	   * change the iconified state in all transient related windows,
	   * as windows doesn't give icons for transient childrens.
	   */
	  if ((old_state & GDK_SURFACE_STATE_ICONIFIED) !=
	      (new_state & GDK_SURFACE_STATE_ICONIFIED))
	    do_show_window (window, (new_state & GDK_SURFACE_STATE_ICONIFIED));


	  /* When un-minimizing, make sure we're stacked under any
	     transient-type windows. */
	  if (!(old_state & GDK_SURFACE_STATE_ICONIFIED) &&
	      (new_state & GDK_SURFACE_STATE_ICONIFIED))
            {
	      ensure_stacking_on_unminimize (msg);
	      restack_children (window);

	    }
	}

      /* Show, New size or position => configure event */
      if (!(windowpos->flags & SWP_NOCLIENTMOVE) ||
	  !(windowpos->flags & SWP_NOCLIENTSIZE) ||
	  (windowpos->flags & SWP_SHOWWINDOW))
	{
	  if (!IsIconic (msg->hwnd) &&
	      !GDK_SURFACE_DESTROYED (window))
	    _gdk_win32_emit_configure_event (window);
	}

      if ((windowpos->flags & SWP_HIDEWINDOW) &&
	  !GDK_SURFACE_DESTROYED (window))
	{
	  /* Make transient parent the forground window when window unmaps */
	  impl = GDK_WIN32_SURFACE (window);

	  if (impl->transient_owner &&
	      GetForegroundWindow () == GDK_SURFACE_HWND (window))
	    SetForegroundWindow (GDK_SURFACE_HWND (impl->transient_owner));
	}

      if (!(windowpos->flags & SWP_NOCLIENTSIZE))
	{
	  if (window->resize_count > 1)
	    window->resize_count -= 1;
	}

      /* Call modal timer immediate so that we repaint faster after a resize. */
      if (_modal_operation_in_progress & GDK_WIN32_MODAL_OP_SIZEMOVE_MASK)
	modal_timer_proc (0,0,0,0);

      /* Claim as handled, so that WM_SIZE and WM_MOVE are avoided */
      return_val = TRUE;
      *ret_valp = 0;
      break;

    case WM_SIZING:
      GetWindowRect (GDK_SURFACE_HWND (window), &rect);
      drag = (RECT *) msg->lParam;
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
				 _gdk_win32_rect_to_string (drag)));

      impl = GDK_WIN32_SURFACE (window);
      orig_drag = *drag;
      if (impl->hint_flags & GDK_HINT_RESIZE_INC)
	{
	  GDK_NOTE (EVENTS, g_print (" (RESIZE_INC)"));
	  if (impl->hint_flags & GDK_HINT_BASE_SIZE)
	    {
	      /* Resize in increments relative to the base size */
	      rect.left = rect.top = 0;
	      rect.right = impl->hints.base_width * impl->surface_scale;
	      rect.bottom = impl->hints.base_height * impl->surface_scale;
	      _gdk_win32_adjust_client_rect (window, &rect);
	      point.x = rect.left;
	      point.y = rect.top;
	      ClientToScreen (GDK_SURFACE_HWND (window), &point);
	      rect.left = point.x;
	      rect.top = point.y;
	      point.x = rect.right;
	      point.y = rect.bottom;
	      ClientToScreen (GDK_SURFACE_HWND (window), &point);
	      rect.right = point.x;
	      rect.bottom = point.y;

	      GDK_NOTE (EVENTS, g_print (" (also BASE_SIZE, using %s)",
					 _gdk_win32_rect_to_string (&rect)));
	    }

	  switch (msg->wParam)
	    {
	    case WMSZ_BOTTOM:
	      if (drag->bottom == rect.bottom)
		break;
        adjust_drag (&drag->bottom, rect.bottom, impl->hints.height_inc * impl->surface_scale);
	      break;

	    case WMSZ_BOTTOMLEFT:
	      if (drag->bottom == rect.bottom && drag->left == rect.left)
		break;
	      adjust_drag (&drag->bottom, rect.bottom, impl->hints.height_inc * impl->surface_scale);
	      adjust_drag (&drag->left, rect.left, impl->hints.width_inc * impl->surface_scale);
	      break;

	    case WMSZ_LEFT:
	      if (drag->left == rect.left)
		break;
	      adjust_drag (&drag->left, rect.left, impl->hints.width_inc * impl->surface_scale);
	      break;

	    case WMSZ_TOPLEFT:
	      if (drag->top == rect.top && drag->left == rect.left)
		break;
	      adjust_drag (&drag->top, rect.top, impl->hints.height_inc * impl->surface_scale);
	      adjust_drag (&drag->left, rect.left, impl->hints.width_inc * impl->surface_scale);
	      break;

	    case WMSZ_TOP:
	      if (drag->top == rect.top)
		break;
	      adjust_drag (&drag->top, rect.top, impl->hints.height_inc * impl->surface_scale);
	      break;

	    case WMSZ_TOPRIGHT:
	      if (drag->top == rect.top && drag->right == rect.right)
		break;
	      adjust_drag (&drag->top, rect.top, impl->hints.height_inc * impl->surface_scale);
	      adjust_drag (&drag->right, rect.right, impl->hints.width_inc * impl->surface_scale);
	      break;

	    case WMSZ_RIGHT:
	      if (drag->right == rect.right)
		break;
	      adjust_drag (&drag->right, rect.right, impl->hints.width_inc * impl->surface_scale);
	      break;

	    case WMSZ_BOTTOMRIGHT:
	      if (drag->bottom == rect.bottom && drag->right == rect.right)
		break;
	      adjust_drag (&drag->bottom, rect.bottom, impl->hints.height_inc * impl->surface_scale);
	      adjust_drag (&drag->right, rect.right, impl->hints.width_inc * impl->surface_scale);
	      break;
	    }

	  if (drag->bottom != orig_drag.bottom || drag->left != orig_drag.left ||
	      drag->top != orig_drag.top || drag->right != orig_drag.right)
	    {
	      *ret_valp = TRUE;
	      return_val = TRUE;
	      GDK_NOTE (EVENTS, g_print (" (handled RESIZE_INC: %s)",
					 _gdk_win32_rect_to_string (drag)));
	    }
	}

      /* WM_GETMINMAXINFO handles min_size and max_size hints? */

      if (impl->hint_flags & GDK_HINT_ASPECT)
	{
	  RECT decorated_rect;
	  RECT undecorated_drag;
	  int decoration_width, decoration_height;
	  gdouble drag_aspect;
	  int drag_width, drag_height, new_width, new_height;

	  GetClientRect (GDK_SURFACE_HWND (window), &rect);
	  decorated_rect = rect;
	  _gdk_win32_adjust_client_rect (window, &decorated_rect);

	  /* Set undecorated_drag to the client area being dragged
	   * out, in screen coordinates.
	   */
	  undecorated_drag = *drag;
	  undecorated_drag.left -= decorated_rect.left - rect.left;
	  undecorated_drag.right -= decorated_rect.right - rect.right;
	  undecorated_drag.top -= decorated_rect.top - rect.top;
	  undecorated_drag.bottom -= decorated_rect.bottom - rect.bottom;

	  decoration_width = (decorated_rect.right - decorated_rect.left) - (rect.right - rect.left);
	  decoration_height = (decorated_rect.bottom - decorated_rect.top) - (rect.bottom - rect.top);

	  drag_width = undecorated_drag.right - undecorated_drag.left;
	  drag_height = undecorated_drag.bottom - undecorated_drag.top;

	  drag_aspect = (gdouble) drag_width / drag_height;

	  GDK_NOTE (EVENTS, g_print (" (ASPECT:%g--%g curr: %g)",
				     impl->hints.min_aspect, impl->hints.max_aspect, drag_aspect));

	  if (drag_aspect < impl->hints.min_aspect)
	    {
	      /* Aspect is getting too narrow */
	      switch (msg->wParam)
		{
		case WMSZ_BOTTOM:
		case WMSZ_TOP:
		  /* User drags top or bottom edge outward. Keep height, increase width. */
		  new_width = impl->hints.min_aspect * drag_height;
		  drag->left -= (new_width - drag_width) / 2;
		  drag->right = drag->left + new_width + decoration_width;
		  break;
		case WMSZ_BOTTOMLEFT:
		case WMSZ_BOTTOMRIGHT:
		  /* User drags bottom-left or bottom-right corner down. Adjust height. */
		  new_height = drag_width / impl->hints.min_aspect;
		  drag->bottom = drag->top + new_height + decoration_height;
		  break;
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
		  /* User drags left or right edge inward. Decrease height */
		  new_height = drag_width / impl->hints.min_aspect;
		  drag->top += (drag_height - new_height) / 2;
		  drag->bottom = drag->top + new_height + decoration_height;
		  break;
		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
		  /* User drags top-left or top-right corner up. Adjust height. */
		  new_height = drag_width / impl->hints.min_aspect;
		  drag->top = drag->bottom - new_height - decoration_height;
		}
	    }
	  else if (drag_aspect > impl->hints.max_aspect)
	    {
	      /* Aspect is getting too wide */
	      switch (msg->wParam)
		{
		case WMSZ_BOTTOM:
		case WMSZ_TOP:
		  /* User drags top or bottom edge inward. Decrease width. */
		  new_width = impl->hints.max_aspect * drag_height;
		  drag->left += (drag_width - new_width) / 2;
		  drag->right = drag->left + new_width + decoration_width;
		  break;
		case WMSZ_BOTTOMLEFT:
		case WMSZ_TOPLEFT:
		  /* User drags bottom-left or top-left corner left. Adjust width. */
		  new_width = impl->hints.max_aspect * drag_height;
		  drag->left = drag->right - new_width - decoration_width;
		  break;
		case WMSZ_BOTTOMRIGHT:
		case WMSZ_TOPRIGHT:
		  /* User drags bottom-right or top-right corner right. Adjust width. */
		  new_width = impl->hints.max_aspect * drag_height;
		  drag->right = drag->left + new_width + decoration_width;
		  break;
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
		  /* User drags left or right edge outward. Increase height. */
		  new_height = drag_width / impl->hints.max_aspect;
		  drag->top -= (new_height - drag_height) / 2;
		  drag->bottom = drag->top + new_height + decoration_height;
		  break;
		}
	    }

	  *ret_valp = TRUE;
	  return_val = TRUE;
	  GDK_NOTE (EVENTS, g_print (" (handled ASPECT: %s)",
				     _gdk_win32_rect_to_string (drag)));
	}
      break;

    case WM_GETMINMAXINFO:
      mmi = (MINMAXINFO*) msg->lParam;

      GDK_NOTE (EVENTS, g_print (" (mintrack:%ldx%ld maxtrack:%ldx%ld "
				 "maxpos:%+ld%+ld maxsize:%ldx%ld)",
				 mmi->ptMinTrackSize.x, mmi->ptMinTrackSize.y,
				 mmi->ptMaxTrackSize.x, mmi->ptMaxTrackSize.y,
				 mmi->ptMaxPosition.x, mmi->ptMaxPosition.y,
				 mmi->ptMaxSize.x, mmi->ptMaxSize.y));

      if (_gdk_win32_surface_fill_min_max_info (window, mmi))
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
      if (GDK_SURFACE_DESTROYED (window))
	break;

      event = gdk_event_new (GDK_DELETE);
      event->any.surface = window;

      _gdk_win32_append_event (event);

      impl = GDK_WIN32_SURFACE (window);

      if (impl->transient_owner && GetForegroundWindow() == GDK_SURFACE_HWND (window))
	{
	  SetForegroundWindow (GDK_SURFACE_HWND (impl->transient_owner));
	}

      return_val = TRUE;
      break;

    case WM_DPICHANGED:
      handle_dpi_changed (window, msg);
      return_val = FALSE;
      *ret_valp = 0;
      break;

    case WM_NCDESTROY:
      if ((pointer_grab != NULL && pointer_grab->surface == window) ||
          (keyboard_grab && keyboard_grab->surface == window))
      {
        GdkDevice *device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
        gdk_device_ungrab (device, msg -> time);
      }

      if ((window != NULL) && (msg->hwnd != GetDesktopWindow ()))
	gdk_surface_destroy_notify (window);

      if (window == NULL || GDK_SURFACE_DESTROYED (window))
	break;

      event = gdk_event_new (GDK_DESTROY);
      event->any.surface = window;

      _gdk_win32_append_event (event);

      return_val = TRUE;
      break;

    case WM_DWMCOMPOSITIONCHANGED:
      gdk_win32_display_check_composited (GDK_WIN32_DISPLAY (display));
      _gdk_win32_surface_enable_transparency (window);
      break;

    case WM_ACTIVATE:
      GDK_NOTE (EVENTS, g_print (" %s%s %p",
				 (LOWORD (msg->wParam) == WA_ACTIVE ? "ACTIVE" :
				  (LOWORD (msg->wParam) == WA_CLICKACTIVE ? "CLICKACTIVE" :
				   (LOWORD (msg->wParam) == WA_INACTIVE ? "INACTIVE" : "???"))),
				 HIWORD (msg->wParam) ? " minimized" : "",
				 (HWND) msg->lParam));
      if (window->surface_type == GDK_SURFACE_POPUP ||
          window->surface_type == GDK_SURFACE_TEMP)
        {
          /* Popups cannot be activated or de-activated - 
           * they only support keyboard focus, which GTK
           * will handle for us.
           */
          *ret_valp = 0;
          return_val = TRUE;
          break;
        }
      /* We handle mouse clicks for modally-blocked windows under WM_MOUSEACTIVATE,
       * but we still need to deal with alt-tab, or with SetActiveWindow() type
       * situations.
       */
      if (_gdk_modal_blocked (window) && LOWORD (msg->wParam) == WA_ACTIVE)
	{
	  GdkSurface *modal_current = _gdk_modal_current ();
	  SetActiveWindow (GDK_SURFACE_HWND (modal_current));
	  *ret_valp = 0;
	  return_val = TRUE;
	  break;
	}

      if (LOWORD (msg->wParam) == WA_INACTIVE && msg->lParam != 0)
        {
          GdkSurface *other_surface = gdk_win32_handle_table_lookup ((HWND) msg->lParam);
          if (other_surface != NULL &&
              (other_surface->surface_type == GDK_SURFACE_POPUP ||
               other_surface->surface_type == GDK_SURFACE_TEMP))
            {
              /* We're being deactivated in favour of some popup or temp window.
               * Since only toplevels can have visual focus, pretend that
               * nothing happened.
               */
              *ret_valp = 0;
              return_val = TRUE;
              break;
            }
        }

      if (LOWORD (msg->wParam) == WA_INACTIVE)
	gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_FOCUSED, 0);
      else
	gdk_synthesize_surface_state (window, 0, GDK_SURFACE_STATE_FOCUSED);

      /* Bring any tablet contexts to the top of the overlap order when
       * one of our windows is activated.
       * NOTE: It doesn't seem to work well if it is done in WM_ACTIVATEAPP
       * instead
       */
      if (LOWORD(msg->wParam) != WA_INACTIVE)
	_gdk_input_set_tablet_active ();
      break;

    case WM_ACTIVATEAPP:
      GDK_NOTE (EVENTS, g_print (" %s thread: %" G_GINT64_FORMAT,
				 msg->wParam ? "YES" : "NO",
				 (gint64) msg->lParam));
      if (msg->wParam && GDK_SURFACE_IS_MAPPED (window))
	{
	  ensure_stacking_on_activate_app (msg, window);
	  restack_children (window);
	}
      break;
    case WM_NCHITTEST:
      /* TODO: pass all messages to DwmDefWindowProc() first! */
      return_val = handle_nchittest (msg->hwnd, window,
                                     GET_X_LPARAM (msg->lParam),
                                     GET_Y_LPARAM (msg->lParam), ret_valp);
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

      event = gdk_event_new (GDK_NOTHING);
      event->any.surface = window;
      g_object_ref (window);

      if (gdk_input_other_event (display, event, msg, window))
	_gdk_win32_append_event (event);
      else
	g_object_unref (event);

      break;
    }

done:

  if (window)
    g_object_unref (window);

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
		   gint    *timeout)
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

      g_object_unref (event);
    }

  return TRUE;
}

void
gdk_win32_set_modal_dialog_libgtk_only (HWND window)
{
  modal_win32_dialog = window;
}
