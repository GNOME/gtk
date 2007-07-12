/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2007 Cody Russell
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* Cannot use TrackMouseEvent, as the stupid WM_MOUSELEAVE message
 * doesn't tell us where the mouse has gone. Thus we cannot use it to
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

#include <config.h>

#include <glib/gprintf.h>

#if defined (__GNUC__) && defined (HAVE_DIMM_H)
/* The w32api imm.h clashes a bit with the IE5.5 dimm.h */
# define IMEMENUITEMINFOA hidden_IMEMENUITEMINFOA
# define IMEMENUITEMINFOW hidden_IMEMENUITEMINFOW
#endif

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"
#include "gdkkeysyms.h"

#include <windowsx.h>

#ifdef G_WITH_CYGWIN
#include <fcntl.h>
#include <errno.h>
#endif

#include <objbase.h>

#include <imm.h>

#if defined (__GNUC__) && defined (HAVE_DIMM_H)
# undef IMEMENUITEMINFOA
# undef IMEMENUITEMINFOW
#endif

#ifdef HAVE_DIMM_H
#include <dimm.h>
#endif

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

/* 
 * Private function declarations
 */

static gboolean gdk_event_translate (MSG        *msg,
				     gint       *ret_valp);
static void     handle_wm_paint     (MSG        *msg,
				     GdkWindow  *window,
				     gboolean    return_exposes,
				     GdkEvent  **event);

static gboolean gdk_event_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

static void append_event (GdkEvent *event);

/* Private variable declarations
 */

static GdkWindow *p_grab_window = NULL; /* Window that currently holds
					 * the pointer grab
					 */

static GdkWindow *p_grab_confine_to = NULL;

static GdkWindow *k_grab_window = NULL; /* Window the holds the
					 * keyboard grab
					 */

static GList *client_filters;	/* Filters for client messages */

static gboolean p_grab_automatic;
static GdkEventMask p_grab_mask;
static gboolean p_grab_owner_events, k_grab_owner_events;
static HCURSOR p_grab_cursor;

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

GPollFD event_poll_fd;

static GdkWindow *current_window = NULL;
static gint current_x, current_y;
static gint current_root_x, current_root_y;
static UINT msh_mousewheel;
static UINT client_message;

static UINT got_gdk_events_message;
static HWND modal_win32_dialog = NULL;

#ifdef HAVE_DIMM_H
static IActiveIMMApp *active_imm_app = NULL;
static IActiveIMMMessagePumpOwner *active_imm_msgpump_owner = NULL;
#endif

#if 0
static HKL latin_locale = NULL;
#endif

static gboolean in_ime_composition = FALSE;
static UINT     resize_timer;

static int debug_indent = 0;

static void
assign_object (gpointer lhsp,
	       gpointer rhs)
{
  if (*(gpointer *)lhsp != rhs)
    {
      if (*(gpointer *)lhsp != NULL)
	g_object_unref (*(gpointer *)lhsp);
      *(gpointer *)lhsp = rhs;
      if (rhs != NULL)
	g_object_ref (rhs);
    }
}

static void
track_mouse_event (DWORD dwFlags,
		   HWND  hwnd)
{
  typedef BOOL (WINAPI *PFN_TrackMouseEvent) (LPTRACKMOUSEEVENT);
  static PFN_TrackMouseEvent p_TrackMouseEvent = NULL;
  static gboolean once = FALSE;

  if (!once)
    {
      HMODULE user32;
      HINSTANCE commctrl32;

      user32 = GetModuleHandle ("user32.dll");
      if ((p_TrackMouseEvent = (PFN_TrackMouseEvent)GetProcAddress (user32, "TrackMouseEvent")) == NULL)
        {
          if ((commctrl32 = LoadLibrary ("commctrl32.dll")) != NULL)
	    p_TrackMouseEvent = (PFN_TrackMouseEvent)
	      GetProcAddress (commctrl32, "_TrackMouseEvent");
        }
      once = TRUE;
    }

  if (p_TrackMouseEvent)
    {
      TRACKMOUSEEVENT tme;
      tme.cbSize = sizeof(TRACKMOUSEEVENT);
      tme.dwFlags = dwFlags;
      tme.hwndTrack = hwnd;
      tme.dwHoverTime = HOVER_DEFAULT; /* not used */

      if (!p_TrackMouseEvent (&tme))
        WIN32_API_FAILED ("TrackMouseEvent");
      else if (dwFlags == TME_LEAVE)
        GDK_NOTE (EVENTS, g_print(" (TrackMouseEvent %p)", hwnd));
      else if (dwFlags == TME_CANCEL)
	GDK_NOTE (EVENTS, g_print(" (cancel TrackMouseEvent %p)", hwnd));
    }
}

gulong
_gdk_win32_get_next_tick (gulong suggested_tick)
{
  static gulong cur_tick = 0;

  if (suggested_tick == 0)
    suggested_tick = GetTickCount ();
  if (suggested_tick <= cur_tick)
    return cur_tick;
  else
    return cur_tick = suggested_tick;
}

static void
generate_focus_event (GdkWindow *window,
		      gboolean   in)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = window;
  event->focus_change.in = in;

  append_event (event);
}

static void
generate_grab_broken_event (GdkWindow *window,
			    gboolean   keyboard,
			    GdkWindow *grab_window)
{
  GdkEvent *event = gdk_event_new (GDK_GRAB_BROKEN);

  event->grab_broken.window = window;
  event->grab_broken.send_event = 0;
  event->grab_broken.keyboard = keyboard;
  event->grab_broken.implicit = FALSE;
  event->grab_broken.grab_window = grab_window;
	  
  append_event (event);
}

static LRESULT 
inner_window_procedure (HWND   hwnd,
			UINT   message,
			WPARAM wparam,
			LPARAM lparam)
{
  MSG msg;
  DWORD pos;
#ifdef HAVE_DIMM_H
  LRESULT lres;
#endif
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
	PostMessage (modal_win32_dialog, got_gdk_events_message,
		     (WPARAM) 1, 0);
      return ret_val;
    }
  else
    {
      /* Otherwise call DefWindowProc(). */
      GDK_NOTE (EVENTS, g_print (" DefWindowProc"));
#ifndef HAVE_DIMM_H
      return DefWindowProc (hwnd, message, wparam, lparam);
#else
      if (active_imm_app == NULL ||
	  (*active_imm_app->lpVtbl->OnDefWindowProc) (active_imm_app, hwnd, message, wparam, lparam, &lres) == S_FALSE)
	return DefWindowProc (hwnd, message, wparam, lparam);
      else
	return lres;
#endif
    }
}

LRESULT CALLBACK
_gdk_win32_window_procedure (HWND   hwnd,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  LRESULT retval;

  GDK_NOTE (EVENTS, g_print ("%s%*s%s %p",
			     (debug_indent > 0 ? "\n" : ""),
			     debug_indent, "", 
			     _gdk_win32_message_to_string (message), hwnd));
  debug_indent += 2;
  retval = inner_window_procedure (hwnd, message, wparam, lparam);
  debug_indent -= 2;

  GDK_NOTE (EVENTS, g_print (" => %ld%s",
			     retval, (debug_indent == 0 ? "\n" : "")));

  return retval;
}

void 
_gdk_events_init (void)
{
  GSource *source;
#ifdef HAVE_DIMM_H
  HRESULT hres;
#endif

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

  /* This is the string MSH_MOUSEWHEEL from zmouse.h,
   * http://www.microsoft.com/mouse/intellimouse/sdk/zmouse.h
   * This message is used by mouse drivers than cannot generate WM_MOUSEWHEEL
   * or on Win95.
   */
  msh_mousewheel = RegisterWindowMessage ("MSWHEEL_ROLLMSG");

  client_message = RegisterWindowMessage ("GDK_WIN32_CLIENT_MESSAGE");
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

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);

#ifdef G_WITH_CYGWIN
  event_poll_fd.fd = open ("/dev/windows", O_RDONLY);
  if (event_poll_fd.fd == -1)
    g_error ("can't open \"/dev/windows\": %s", g_strerror (errno));
#else
  event_poll_fd.fd = G_WIN32_MSG_HANDLE;
#endif
  event_poll_fd.events = G_IO_IN;
  
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

#ifdef HAVE_DIMM_H
  hres = CoCreateInstance (&CLSID_CActiveIMM,
			   NULL,
			   CLSCTX_ALL,
			   &IID_IActiveIMMApp,
			   (LPVOID *) &active_imm_app);
  
  if (hres == S_OK)
    {
      GDK_NOTE (EVENTS, g_print ("IActiveIMMApp created %p\n",
				 active_imm_app));
      (*active_imm_app->lpVtbl->Activate) (active_imm_app, TRUE);
      
      hres = (*active_imm_app->lpVtbl->QueryInterface) (active_imm_app, &IID_IActiveIMMMessagePumpOwner, (void **) &active_imm_msgpump_owner);
      GDK_NOTE (EVENTS, g_print ("IActiveIMMMessagePumpOwner created %p\n",
				 active_imm_msgpump_owner));
      (active_imm_msgpump_owner->lpVtbl->Start) (active_imm_msgpump_owner);
    }
#endif
}

gboolean
gdk_events_pending (void)
{
  MSG msg;
  return (_gdk_event_queue_find_first (_gdk_display) ||
	  (modal_win32_dialog == NULL &&
	   PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)));
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  MSG msg;
  GdkEvent *event = NULL;

  g_return_val_if_fail (window != NULL, NULL);
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose\n"));

  if (PeekMessage (&msg, GDK_WINDOW_HWND (window), WM_PAINT, WM_PAINT, PM_REMOVE))
    {
      handle_wm_paint (&msg, window, TRUE, &event);
      if (event != NULL)
	{
	  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose: got it!\n"));
	  return event;
	}
    }
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose: nope\n"));
  return NULL;	
}

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

GdkGrabStatus
gdk_pointer_grab (GdkWindow    *window,
		  gboolean	owner_events,
		  GdkEventMask	event_mask,
		  GdkWindow    *confine_to,
		  GdkCursor    *cursor,
		  guint32	time)
{
  HCURSOR hcursor;
  GdkCursorPrivate *cursor_private;
  gint return_val = GDK_GRAB_SUCCESS;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);
  
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!cursor)
    hcursor = NULL;
  else if ((hcursor = CopyCursor (cursor_private->hcursor)) == NULL)
    WIN32_API_FAILED ("CopyCursor");

  return_val = _gdk_input_grab_pointer (window,
					owner_events,
					event_mask,
					confine_to,
					time);

  if (return_val == GDK_GRAB_SUCCESS)
    {
      if (!GDK_WINDOW_DESTROYED (window))
	{
	  GDK_NOTE (EVENTS, g_print ("%sgdk_pointer_grab: %p %s %p %s%s",
				     (debug_indent > 0 ? "\n" : ""),
				     GDK_WINDOW_HWND (window),
				     (owner_events ? "TRUE" : "FALSE"),
				     hcursor,
				     event_mask_string (event_mask),
				     (debug_indent == 0 ? "\n" : "")));

	  p_grab_mask = event_mask;
	  p_grab_owner_events = owner_events;
	  p_grab_automatic = FALSE;
	  
	  SetCapture (GDK_WINDOW_HWND (window));
	  return_val = GDK_GRAB_SUCCESS;
	}
      else
	return_val = GDK_GRAB_ALREADY_GRABBED;
    }
  
  if (return_val == GDK_GRAB_SUCCESS)
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl);

      if (p_grab_window != NULL && p_grab_window != window)
	generate_grab_broken_event (p_grab_window, FALSE, window);
      
      assign_object (&p_grab_window, window);

      if (p_grab_cursor != NULL)
	{
	  if (GetCursor () == p_grab_cursor)
	    SetCursor (NULL);
	  DestroyCursor (p_grab_cursor);
	}

      p_grab_cursor = hcursor;

      if (p_grab_cursor != NULL)
	SetCursor (p_grab_cursor);
      else if (impl->hcursor != NULL)
	SetCursor (impl->hcursor);
      else
	SetCursor (LoadCursor (NULL, IDC_ARROW));

      if (confine_to != NULL)
	{
	  gint x, y, width, height;
	  RECT rect;

	  gdk_window_get_origin (confine_to, &x, &y);
	  gdk_drawable_get_size (confine_to, &width, &height);

	  x -= _gdk_offset_x;
	  y -= _gdk_offset_y;

	  rect.left = x;
	  rect.top = y;
	  rect.right = x + width;
	  rect.bottom = y + height;
	  API_CALL (ClipCursor, (&rect));
	  p_grab_confine_to = confine_to;
	}

      /* FIXME: Generate GDK_CROSSING_GRAB events */
    }
  
  return return_val;
}

void
gdk_display_pointer_ungrab (GdkDisplay *display,
                            guint32     time)
{
  g_return_if_fail (display == _gdk_display);

  GDK_NOTE (EVENTS, g_print ("%sgdk_display_pointer_ungrab%s",
			     (debug_indent > 0 ? "\n" : ""),
			     (debug_indent == 0 ? "\n" : "")));

  _gdk_input_ungrab_pointer (time);

  if (GetCapture () != NULL)
    ReleaseCapture ();

  /* FIXME: Generate GDK_CROSSING_UNGRAB events */

  assign_object (&p_grab_window, NULL);
  if (p_grab_cursor != NULL)
    {
      if (GetCursor () == p_grab_cursor)
	SetCursor (NULL);
      DestroyCursor (p_grab_cursor);
      p_grab_cursor = NULL;
    }

  if (p_grab_confine_to != NULL)
    {
      API_CALL (ClipCursor, (NULL));
      p_grab_confine_to = NULL;
    }
}

static GdkWindow *
find_real_window_for_grabbed_mouse_event (GdkWindow* reported_window,
					  MSG*       msg)
{
  HWND hwnd;
  POINTS points;
  POINT pt;
  GdkWindow* other_window = NULL;

  points = MAKEPOINTS (msg->lParam);
  pt.x = points.x;
  pt.y = points.y;
  ClientToScreen (msg->hwnd, &pt);

  hwnd = WindowFromPoint (pt);

  if (hwnd != NULL)
    {
      RECT rect;

      GetClientRect (hwnd, &rect);
      ScreenToClient (hwnd, &pt);
      if (!PtInRect (&rect, pt))
	return _gdk_root;

      other_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) hwnd);
    }

  if (other_window == NULL)
    return _gdk_root;

  return other_window;
}

static GdkWindow* 
find_window_for_mouse_event (GdkWindow* reported_window,
			     MSG*       msg)
{
  if (p_grab_window == NULL || !p_grab_owner_events)
    return reported_window;
  else
    return find_real_window_for_grabbed_mouse_event (reported_window, msg);
}

gboolean
gdk_display_pointer_is_grabbed (GdkDisplay *display)
{
  g_return_val_if_fail (display == _gdk_display, FALSE);
  GDK_NOTE (EVENTS, g_print ("gdk_pointer_is_grabbed: %s\n",
			     p_grab_window != NULL ? "TRUE" : "FALSE"));
  return p_grab_window != NULL;
}

gboolean
gdk_pointer_grab_info_libgtk_only (GdkDisplay *display,
				   GdkWindow **grab_window,
				   gboolean   *owner_events)
{
  g_return_val_if_fail (display == _gdk_display, FALSE);

  if (p_grab_window != NULL)
    {
      if (grab_window)
        *grab_window = p_grab_window;
      if (owner_events)
        *owner_events = p_grab_owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *window,
		   gboolean   owner_events,
		   guint32    time)
{
  GdkWindow *real_focus_window, *grab_focus_window;

  gint return_val;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_grab %p\n",
			     GDK_WINDOW_HWND (window)));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      k_grab_owner_events = owner_events != 0;
      return_val = GDK_GRAB_SUCCESS;
    }
  else
    return_val = GDK_GRAB_ALREADY_GRABBED;

  if (return_val == GDK_GRAB_SUCCESS)
    {
      if (k_grab_window != NULL && k_grab_window != window)
	generate_grab_broken_event (k_grab_window, TRUE, window);

      assign_object (&k_grab_window, window);

      if (!k_grab_owner_events)
	{
	  real_focus_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) GetFocus ());
	  if (real_focus_window)
	    real_focus_window = gdk_window_get_toplevel (real_focus_window);
	  grab_focus_window = gdk_window_get_toplevel (k_grab_window);
	  if (real_focus_window != grab_focus_window)
	    {
	      /* Generate events for focus change from the window that really
	       * has focus to the grabber.
	       */
	      if (real_focus_window && !GDK_WINDOW_DESTROYED (real_focus_window)
		  && (((GdkWindowObject *) real_focus_window)->event_mask
		      & GDK_FOCUS_CHANGE_MASK))
		generate_focus_event (real_focus_window, FALSE);

	      if (((GdkWindowObject *) grab_focus_window)->event_mask 
		  & GDK_FOCUS_CHANGE_MASK)
		generate_focus_event (grab_focus_window, TRUE);
	    }
	}
    }
  
  return return_val;
}

void
gdk_display_keyboard_ungrab (GdkDisplay *display,
                             guint32 time)
{
  GdkWindow *real_focus_window, *grab_focus_window;

  g_return_if_fail (display == _gdk_display);

  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_ungrab\n"));

  if (k_grab_window && !k_grab_owner_events)
    {
      real_focus_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) GetFocus ());
      if (real_focus_window)
	real_focus_window = gdk_window_get_toplevel (real_focus_window);
      if (!GDK_WINDOW_DESTROYED (k_grab_window))
	grab_focus_window = gdk_window_get_toplevel (k_grab_window);
      else
	grab_focus_window = NULL;
      if (real_focus_window != grab_focus_window)
	{
	  /* Generate events for focus change from grabber to the window that
	   * really has focus. Important for example if a new window is created
	   * while focus is grabbed.
	   */
	  if (grab_focus_window
	      && (((GdkWindowObject *) grab_focus_window)->event_mask
		  & GDK_FOCUS_CHANGE_MASK))
	    generate_focus_event (grab_focus_window, FALSE);

	  if (real_focus_window && !GDK_WINDOW_DESTROYED (real_focus_window)
	      && (((GdkWindowObject *) real_focus_window)->event_mask
		  & GDK_FOCUS_CHANGE_MASK))
	    generate_focus_event (real_focus_window, TRUE);
	}
    }

  assign_object (&k_grab_window, NULL);
}

gboolean
gdk_keyboard_grab_info_libgtk_only (GdkDisplay *display,
				    GdkWindow **grab_window,
				    gboolean   *owner_events)
{
  g_return_val_if_fail (display == _gdk_display, FALSE);

  if (k_grab_window)
    {
      if (grab_window)
        *grab_window = k_grab_window;
      if (owner_events)
        *owner_events = k_grab_owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

void 
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
  /* XXX */
  gdk_add_client_message_filter (message_type, func, data);
}

void 
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  GdkClientFilter *filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  
  client_filters = g_list_append (client_filters, filter);
}

static void
build_key_event_state (GdkEvent *event,
		       BYTE     *key_state)
{
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

  /* Win9x doesn't distinguish between left and right Control and Alt
   * in the keyboard state as returned by GetKeyboardState(), so we
   * have to punt, and accept either Control + either Alt to be AltGr.
   *
   * Alternatively, we could have some state saved when the Control
   * and Alt keys messages come in, as the KF_EXTENDED bit in lParam
   * does indicate correctly whether it is the right Control or Alt
   * key. But that would be a bit messy.
   */
  if (!G_WIN32_IS_NT_BASED () &&
      _gdk_keyboard_has_altgr &&
      key_state[VK_CONTROL] & 0x80 &&
      key_state[VK_MENU] & 0x80)
    key_state[VK_LCONTROL] = key_state[VK_RMENU] = 0x80;

  if (_gdk_keyboard_has_altgr &&
      (key_state[VK_LCONTROL] & 0x80) &&
      (key_state[VK_RMENU] & 0x80))
    {
      event->key.group = 1;
      event->key.state |= GDK_MOD2_MASK;
      if (key_state[VK_RCONTROL] & 0x80)
	event->key.state |= GDK_CONTROL_MASK;
      if (key_state[VK_LMENU] & 0x80)
	event->key.state |= GDK_MOD1_MASK;
    }
  else
    {
      event->key.group = 0;
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
  event->key.string = NULL;
  event->key.length = 0;
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

static void
print_event (GdkEvent *event)
{
  gchar *escaped, *kvname;

  g_print ("%s%*s===> ", (debug_indent > 0 ? "\n" : ""), debug_indent, "");
  switch (event->any.type)
    {
#define CASE(x) case x: g_print (#x); break;
    CASE (GDK_NOTHING);
    CASE (GDK_DELETE);
    CASE (GDK_DESTROY);
    CASE (GDK_EXPOSE);
    CASE (GDK_MOTION_NOTIFY);
    CASE (GDK_BUTTON_PRESS);
    CASE (GDK_2BUTTON_PRESS);
    CASE (GDK_3BUTTON_PRESS);
    CASE (GDK_BUTTON_RELEASE);
    CASE (GDK_KEY_PRESS);
    CASE (GDK_KEY_RELEASE);
    CASE (GDK_ENTER_NOTIFY);
    CASE (GDK_LEAVE_NOTIFY);
    CASE (GDK_FOCUS_CHANGE);
    CASE (GDK_CONFIGURE);
    CASE (GDK_MAP);
    CASE (GDK_UNMAP);
    CASE (GDK_PROPERTY_NOTIFY);
    CASE (GDK_SELECTION_CLEAR);
    CASE (GDK_SELECTION_REQUEST);
    CASE (GDK_SELECTION_NOTIFY);
    CASE (GDK_PROXIMITY_IN);
    CASE (GDK_PROXIMITY_OUT);
    CASE (GDK_DRAG_ENTER);
    CASE (GDK_DRAG_LEAVE);
    CASE (GDK_DRAG_MOTION);
    CASE (GDK_DRAG_STATUS);
    CASE (GDK_DROP_START);
    CASE (GDK_DROP_FINISHED);
    CASE (GDK_CLIENT_EVENT);
    CASE (GDK_VISIBILITY_NOTIFY);
    CASE (GDK_NO_EXPOSE);
    CASE (GDK_SCROLL);
    CASE (GDK_WINDOW_STATE);
    CASE (GDK_SETTING);
    CASE (GDK_OWNER_CHANGE);
    CASE (GDK_GRAB_BROKEN);
#undef CASE
    default: g_assert_not_reached ();
    }

  g_print (" %p ", GDK_WINDOW_HWND (event->any.window));

  switch (event->any.type)
    {
    case GDK_EXPOSE:
      g_print ("%s %d",
	       _gdk_win32_gdkrectangle_to_string (&event->expose.area),
	       event->expose.count);
      break;
    case GDK_MOTION_NOTIFY:
      g_print ("(%.4g,%.4g) (%.4g,%.4g) %s",
	       event->motion.x, event->motion.y,
	       event->motion.x_root, event->motion.y_root,
	       event->motion.is_hint ? "HINT " : "");
      print_event_state (event->motion.state);
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      g_print ("%d (%.4g,%.4g) (%.4g,%.4g) ",
	       event->button.button,
	       event->button.x, event->button.y,
	       event->button.x_root, event->button.y_root);
      print_event_state (event->button.state);
      break;
    case GDK_KEY_PRESS: 
    case GDK_KEY_RELEASE:
      if (event->key.length == 0)
	escaped = g_strdup ("");
      else
	escaped = g_strescape (event->key.string, NULL);
      kvname = gdk_keyval_name (event->key.keyval);
      g_print ("%#.02x group:%d %s %d:\"%s\" ",
	       event->key.hardware_keycode, event->key.group,
	       (kvname ? kvname : "??"),
	       event->key.length,
	       escaped);
      g_free (escaped);
      print_event_state (event->key.state);
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      g_print ("%p (%.4g,%.4g) (%.4g,%.4g) %s %s%s",
	       event->crossing.subwindow == NULL ? NULL : GDK_WINDOW_HWND (event->crossing.subwindow),
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
    case GDK_CONFIGURE:
      g_print ("x:%d y:%d w:%d h:%d",
	       event->configure.x, event->configure.y,
	       event->configure.width, event->configure.height);
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
    case GDK_WINDOW_STATE:
      g_print ("%s: %s",
	       _gdk_win32_window_state_to_string (event->window_state.changed_mask),
	       _gdk_win32_window_state_to_string (event->window_state.new_window_state));
    case GDK_SETTING:
      g_print ("%s: %s",
	       (event->setting.action == GDK_SETTING_ACTION_NEW ? "NEW" :
		(event->setting.action == GDK_SETTING_ACTION_CHANGED ? "CHANGED" :
		 (event->setting.action == GDK_SETTING_ACTION_DELETED ? "DELETED" :
		  "???"))),
	       (event->setting.name ? event->setting.name : "NULL"));
    case GDK_GRAB_BROKEN:
      g_print ("%s %s %p",
	       (event->grab_broken.keyboard ? "KEYBOARD" : "POINTER"),
	       (event->grab_broken.implicit ? "IMPLICIT" : "EXPLICIT"),
	       (event->grab_broken.grab_window ? GDK_WINDOW_HWND (event->grab_broken.grab_window) : 0));
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
  if (event->any.window)
    g_object_ref (event->any.window);
  if (((event->any.type == GDK_ENTER_NOTIFY) ||
       (event->any.type == GDK_LEAVE_NOTIFY)) &&
      (event->crossing.subwindow != NULL))
    g_object_ref (event->crossing.subwindow);
  event->any.send_event = InSendMessage (); 
}

static void
append_event (GdkEvent *event)
{
  fixup_event (event);
  _gdk_event_queue_append (_gdk_display, event);
  GDK_NOTE (EVENTS, print_event (event));
}

static void
fill_key_event_string (GdkEvent *event)
{
  gunichar c;
  gchar buf[256];

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  
  c = 0;
  if (event->key.keyval != GDK_VoidSymbol)
    c = gdk_keyval_to_unicode (event->key.keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;
      
      /* Apply the control key - Taken from Xlib
       */
      if (event->key.state & GDK_CONTROL_MASK)
	{
	  if ((c >= '@' && c < '\177') || c == ' ')
	    c &= 0x1F;
	  else if (c == '2')
	    {
	      event->key.string = g_memdup ("\0\0", 2);
	      event->key.length = 1;
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
	  
      event->key.string = g_locale_from_utf8 (buf, len,
					      NULL, &bytes_written,
					      NULL);
      if (event->key.string)
	event->key.length = bytes_written;
    }
  else if (event->key.keyval == GDK_Escape)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\033");
    }
  else if (event->key.keyval == GDK_Return ||
	   event->key.keyval == GDK_KP_Enter)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\r");
    }
  
  if (!event->key.string)
    {
      event->key.length = 0;
      event->key.string = g_strdup ("");
    }
}

static GdkFilterReturn
apply_filters (GdkWindow  *window,
	       MSG        *msg,
	       GList      *filters)
{
  GdkFilterReturn result = GDK_FILTER_CONTINUE;
  GdkEvent *event;
  GList *node;
  GList *tmp_list;

  event = gdk_event_new (GDK_NOTHING);
  if (window != NULL)
    event->any.window = g_object_ref (window);
  ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

  /* I think GdkFilterFunc semantics require the passed-in event
   * to already be in the queue. The filter func can generate
   * more events and append them after it if it likes.
   */
  node = _gdk_event_queue_append (_gdk_display, event);
  
  tmp_list = filters;
  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter *) tmp_list->data;
      
      tmp_list = tmp_list->next;
      result = filter->function (msg, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	break;
    }

  if (result == GDK_FILTER_CONTINUE || result == GDK_FILTER_REMOVE)
    {
      _gdk_event_queue_remove_link (_gdk_display, node);
      g_list_free_1 (node);
      gdk_event_free (event);
    }
  else /* GDK_FILTER_TRANSLATE */
    {
      ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
      fixup_event (event);
      GDK_NOTE (EVENTS, print_event (event));
    }
  return result;
}

static gboolean
gdk_window_is_ancestor (GdkWindow *ancestor,
			GdkWindow *window)
{
  if (ancestor == NULL || window == NULL)
    return FALSE;

  return (gdk_window_get_parent (window) == ancestor ||
	  gdk_window_is_ancestor (ancestor, gdk_window_get_parent (window)));
}

static void
synthesize_enter_or_leave_event (GdkWindow    	*window,
				 MSG          	*msg,
				 GdkEventType 	 type,
				 GdkCrossingMode mode,
				 GdkNotifyType detail,
				 gint         	 x,
				 gint         	 y)
{
  GdkEvent *event;
  gint xoffset, yoffset;
  
  event = gdk_event_new (type);
  event->crossing.window = window;
  event->crossing.subwindow = NULL;
  event->crossing.time = _gdk_win32_get_next_tick (msg->time);
  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
  event->crossing.x = x + xoffset;
  event->crossing.y = y + yoffset;
  event->crossing.x_root = msg->pt.x + _gdk_offset_x;
  event->crossing.y_root = msg->pt.y + _gdk_offset_y;
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.focus = TRUE; /* FIXME: Set correctly */
  event->crossing.state = 0;	/* FIXME: Set correctly */
  
  append_event (event);
  
  if (type == GDK_ENTER_NOTIFY &&
      ((GdkWindowObject *) window)->extension_events != 0)
    _gdk_input_enter_event (window);
}

static void
synthesize_leave_event (GdkWindow      *window,
			MSG            *msg,
			GdkCrossingMode mode,
			GdkNotifyType   detail)
{
  POINT pt;

  if (p_grab_window != NULL && !p_grab_owner_events && !(p_grab_mask & GDK_LEAVE_NOTIFY_MASK))
    return;

  if (!(((GdkWindowObject *) window)->event_mask & GDK_LEAVE_NOTIFY_MASK))
    return;

  /* Leave events are at (current_x,current_y) in current_window */

  if (current_window != window)
    {
      pt.x = current_x;
      pt.y = current_y;
      ClientToScreen (GDK_WINDOW_HWND (current_window), &pt);
      ScreenToClient (GDK_WINDOW_HWND (window), &pt);
      synthesize_enter_or_leave_event (window, msg, GDK_LEAVE_NOTIFY, mode, detail, pt.x, pt.y);
    }
  else
    synthesize_enter_or_leave_event (window, msg, GDK_LEAVE_NOTIFY, mode, detail, current_x, current_y);

  /* This would only make sense if the WM_MOUSEMOVE messages would come
   * before the respective WM_MOUSELEAVE message, which apparently they
   * do not.
  track_mouse_event (TME_CANCEL, msg->hwnd);
   */
}
  
static void
synthesize_enter_event (GdkWindow      *window,
			MSG            *msg,
			GdkCrossingMode mode,
			GdkNotifyType   detail)
{
  POINT pt;

  if (p_grab_window != NULL && !p_grab_owner_events && !(p_grab_mask & GDK_ENTER_NOTIFY_MASK))
    return;

  if (!(((GdkWindowObject *) window)->event_mask & GDK_ENTER_NOTIFY_MASK))
    return;

  /* Enter events are at GET_X_LPARAM (msg->lParam), GET_Y_LPARAM
   * (msg->lParam) in msg->hwnd
   */

  pt.x = GET_X_LPARAM (msg->lParam);
  pt.y = GET_Y_LPARAM (msg->lParam);
  if (msg->hwnd != GDK_WINDOW_HWND (window))
    {
      ClientToScreen (msg->hwnd, &pt);
      ScreenToClient (GDK_WINDOW_HWND (window), &pt);
    }
  synthesize_enter_or_leave_event (window, msg, GDK_ENTER_NOTIFY, mode, detail, pt.x, pt.y);

  track_mouse_event (TME_LEAVE, GDK_WINDOW_HWND (window));
}
  
static void
synthesize_enter_events (GdkWindow      *from,
			 GdkWindow      *to,
			 MSG            *msg,
			 GdkCrossingMode mode,
			 GdkNotifyType   detail)
{
  GdkWindow *prev = gdk_window_get_parent (to);
  
  if (prev != from)
    synthesize_enter_events (from, prev, msg, mode, detail);
  synthesize_enter_event (to, msg, mode, detail);
}
			 
static void
synthesize_leave_events (GdkWindow    	*from,
			 GdkWindow    	*to,
			 MSG          	*msg,
			 GdkCrossingMode mode,
			 GdkNotifyType	 detail)
{
  GdkWindow *next = gdk_window_get_parent (from);
  
  synthesize_leave_event (from, msg, mode, detail);
  if (next != to)
    synthesize_leave_events (next, to, msg, mode, detail);
}
			 
static void
synthesize_crossing_events (GdkWindow      *window,
			    GdkCrossingMode mode,
			    MSG            *msg)
{
  GdkWindow *intermediate, *tem, *common_ancestor;

  if (gdk_window_is_ancestor (current_window, window))
    {
      /* Pointer has moved to an inferior window. */
      synthesize_leave_event (current_window, msg, mode, GDK_NOTIFY_INFERIOR);
      
      /* If there are intermediate windows, generate ENTER_NOTIFY
       * events for them
       */
      intermediate = gdk_window_get_parent (window);
      if (intermediate != current_window)
	{
	  synthesize_enter_events (current_window, intermediate, msg, mode, GDK_NOTIFY_VIRTUAL);
	}
      
      synthesize_enter_event (window, msg, mode, GDK_NOTIFY_ANCESTOR);
    }
  else if (gdk_window_is_ancestor (window, current_window))
    {
      /* Pointer has moved to an ancestor window. */
      synthesize_leave_event (current_window, msg, mode, GDK_NOTIFY_ANCESTOR);
      
      /* If there are intermediate windows, generate LEAVE_NOTIFY
       * events for them
       */
      intermediate = gdk_window_get_parent (current_window);
      if (intermediate != window)
	{
	  synthesize_leave_events (intermediate, window, msg, mode, GDK_NOTIFY_VIRTUAL);
	}

      synthesize_enter_event (window, msg, mode, GDK_NOTIFY_INFERIOR);
    }
  else if (current_window)
    {
      /* Find least common ancestor of current_window and window */
      tem = current_window;
      do {
	common_ancestor = gdk_window_get_parent (tem);
	tem = common_ancestor;
      } while (common_ancestor &&
	       !gdk_window_is_ancestor (common_ancestor, window));
      if (common_ancestor)
	{
	  synthesize_leave_event (current_window, msg, mode, GDK_NOTIFY_NONLINEAR);
	  intermediate = gdk_window_get_parent (current_window);
	  if (intermediate != common_ancestor)
	    {
	      synthesize_leave_events (intermediate, common_ancestor,
				       msg, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  intermediate = gdk_window_get_parent (window);
	  if (intermediate != common_ancestor)
	    {
	      synthesize_enter_events (common_ancestor, intermediate,
				       msg, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  synthesize_enter_event (window, msg, mode, GDK_NOTIFY_NONLINEAR);
	}
    }
  else
    {
      /* Dunno where we are coming from */
      synthesize_enter_event (window, msg, mode, GDK_NOTIFY_UNKNOWN);
    }

  assign_object (&current_window, window);
}

static void
synthesize_expose_events (GdkWindow *window)
{
  RECT r;
  HDC hdc;
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (((GdkWindowObject *) window)->impl);
  GList *list = gdk_window_get_children (window);
  GList *head = list;
  GdkEvent *event;
  int k;
  
  while (list)
    {
      synthesize_expose_events ((GdkWindow *) list->data);
      list = list->next;
    }

  g_list_free (head);

  if (((GdkWindowObject *) window)->input_only)
    ;
  else if (!(hdc = GetDC (impl->handle)))
    WIN32_GDI_FAILED ("GetDC");
  else
    {
      if ((k = GetClipBox (hdc, &r)) == ERROR)
	WIN32_GDI_FAILED ("GetClipBox");
      else if (k != NULLREGION)
	{
	  event = gdk_event_new (GDK_EXPOSE);
	  event->expose.window = window;
	  event->expose.area.x = r.left;
	  event->expose.area.y = r.top;
	  event->expose.area.width = r.right - r.left;
	  event->expose.area.height = r.bottom - r.top;
	  event->expose.region = gdk_region_rectangle (&(event->expose.area));
	  event->expose.count = 0;
  
	  append_event (event);
	}
      GDI_CALL (ReleaseDC, (impl->handle, hdc));
    }
}

static void
update_colors (GdkWindow *window,
	       gboolean   top)
{
  HDC hdc;
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (((GdkWindowObject *) window)->impl);
  GList *list = gdk_window_get_children (window);
  GList *head = list;

  GDK_NOTE (COLORMAP, (top ? g_print ("update_colors:") : (void) 0));

  while (list)
    {
      update_colors ((GdkWindow *) list->data, FALSE);
      list = list->next;
    }
  g_list_free (head);

  if (((GdkWindowObject *) window)->input_only ||
      impl->colormap == NULL)
    return;

  if (!(hdc = GetDC (impl->handle)))
    WIN32_GDI_FAILED ("GetDC");
  else
    {
      GdkColormapPrivateWin32 *cmapp = GDK_WIN32_COLORMAP_DATA (impl->colormap);
      HPALETTE holdpal;
      gint k;
      
      if ((holdpal = SelectPalette (hdc, cmapp->hpal, TRUE)) == NULL)
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else
	{
	  GDK_NOTE (COLORMAP,
		    (k > 0 ?
		     g_print (" %p pal=%p: realized %d colors\n"
			      "update_colors:",
			      impl->handle, cmapp->hpal, k) :
		     (void) 0,
		     g_print (" %p", impl->handle)));
	  GDI_CALL (UpdateColors, (hdc));
	  SelectPalette (hdc, holdpal, TRUE);
	  RealizePalette (hdc);
	}
      GDI_CALL (ReleaseDC, (impl->handle, hdc));
    }
  GDK_NOTE (COLORMAP, (top ? g_print ("\n") : (void) 0));
}

static void
translate_mouse_coords (GdkWindow *window1,
			GdkWindow *window2,
			MSG       *msg)
{
  POINT pt;

  pt.x = GET_X_LPARAM (msg->lParam);
  pt.y = GET_Y_LPARAM (msg->lParam);
  ClientToScreen (GDK_WINDOW_HWND (window1), &pt);
  ScreenToClient (GDK_WINDOW_HWND (window2), &pt);
  msg->lParam = MAKELPARAM (pt.x, pt.y);
}

/* The check_extended flag controls whether to check if the windows want
 * events from extended input devices and if the message should be skipped
 * because an extended input device is active */
static gboolean
propagate (GdkWindow  **window,
	   MSG         *msg,
	   GdkWindow   *grab_window,
	   gboolean     grab_owner_events,
	   gint	        grab_mask,
	   gboolean   (*doesnt_want_it) (gint mask,
					 MSG *msg),
	   gboolean    	check_extended)
{
  gboolean in_propagation = FALSE;

  if (grab_window != NULL && !grab_owner_events)
    {
      /* Event source is grabbed with owner_events FALSE */

      /* See if the event should be ignored because an extended input device
       * is used */
      if (check_extended &&
	  ((GdkWindowObject *) grab_window)->extension_events != 0 &&
	  _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print (" (ignored for grabber)"));
	  return FALSE;
	}
      if ((*doesnt_want_it) (grab_mask, msg))
	{
	  GDK_NOTE (EVENTS, g_print (" (grabber doesn't want it)"));
	  return FALSE;
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print (" (to grabber)"));
	  assign_object (window, grab_window);
	  return TRUE;
	}
    }
  while (TRUE)
    {
      /* See if the event should be ignored because an extended input device
       * is used */
      if (check_extended &&
	  ((GdkWindowObject *) *window)->extension_events != 0 &&
	  _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print (" (ignored)"));
	  return FALSE;
	}
      if ((*doesnt_want_it) (((GdkWindowObject *) *window)->event_mask, msg))
	{
	  /* Owner doesn't want it, propagate to parent. */
	  GdkWindow *parent = gdk_window_get_parent (*window);
	  if (parent == _gdk_root || parent == NULL)
	    {
	      /* No parent; check if grabbed */
	      if (grab_window != NULL)
		{
		  /* Event source is grabbed with owner_events TRUE */

		  /* See if the event should be ignored because an extended
		   * input device is used */
		  if (check_extended &&
		      ((GdkWindowObject *) grab_window)->extension_events != 0 &&
		      _gdk_input_ignore_core)
		    {
		      GDK_NOTE (EVENTS, g_print (" (ignored for grabber)"));
		      return FALSE;
		    }
		  if ((*doesnt_want_it) (grab_mask, msg))
		    {
		      /* Grabber doesn't want it either */
		      GDK_NOTE (EVENTS, g_print (" (grabber doesn't want it)"));
		      return FALSE;
		    }
		  else
		    {
		      /* Grabbed! */
		      GDK_NOTE (EVENTS, g_print (" (to grabber)"));
		      assign_object (window, grab_window);
		      return TRUE;
		    }
		}
	      else
		{
		  GDK_NOTE (EVENTS, g_print (" (undelivered)"));
		  return FALSE;
		}
	    }
	  else
	    {
	      assign_object (window, parent);
	      in_propagation = TRUE;
	      /* The only branch where we actually continue the loop */
	    }
	}
      else
	return TRUE;
    }
}

static gboolean
doesnt_want_key (gint mask,
		 MSG *msg)
{
  return (((msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) &&
	   !(mask & GDK_KEY_RELEASE_MASK)) ||
	  ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
	   !(mask & GDK_KEY_PRESS_MASK)));
}

static gboolean
doesnt_want_char (gint mask,
		  MSG *msg)
{
  return !(mask & (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK));
}

static gboolean
doesnt_want_button_press (gint mask,
			  MSG *msg)
{
  return !(mask & GDK_BUTTON_PRESS_MASK);
}

static gboolean
doesnt_want_button_release (gint mask,
			    MSG *msg)
{
  return !(mask & GDK_BUTTON_RELEASE_MASK);
}

static gboolean
doesnt_want_button_motion (gint mask,
			   MSG *msg)
{
  return !((mask & GDK_POINTER_MOTION_MASK) ||
	   ((msg->wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON)) && (mask & GDK_BUTTON_MOTION_MASK)) ||
	   ((msg->wParam & MK_LBUTTON) && (mask & GDK_BUTTON1_MOTION_MASK)) ||
	   ((msg->wParam & MK_MBUTTON) && (mask & GDK_BUTTON2_MOTION_MASK)) ||
	   ((msg->wParam & MK_RBUTTON) && (mask & GDK_BUTTON3_MOTION_MASK)));
}

static gboolean
doesnt_want_scroll (gint mask,
		    MSG *msg)
{
  /* As there are no separate scroll events in X11, button press
   * events are used, so higher level code might be selecting for
   * either GDK_BUTTON_PRESS_MASK or GDK_SCROLL_MASK when it wants GDK
   * scroll events. Make sure this works in the Win32 backend, too.
   */
  return !(mask & (GDK_SCROLL_MASK|GDK_BUTTON_PRESS_MASK));
}

static void
handle_configure_event (MSG       *msg,
			GdkWindow *window)
{
  RECT client_rect;
  POINT point;

  GetClientRect (msg->hwnd, &client_rect);
  point.x = client_rect.left; /* always 0 */
  point.y = client_rect.top;
  /* top level windows need screen coords */
  if (gdk_window_get_parent (window) == _gdk_root)
    {
      ClientToScreen (msg->hwnd, &point);
      point.x += _gdk_offset_x;
      point.y += _gdk_offset_y;
    }

  GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->width = client_rect.right - client_rect.left;
  GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->height = client_rect.bottom - client_rect.top;
  
  ((GdkWindowObject *) window)->x = point.x;
  ((GdkWindowObject *) window)->y = point.y;
  
  if (((GdkWindowObject *) window)->event_mask & GDK_STRUCTURE_MASK)
    {
      GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

      event->configure.window = window;

      event->configure.width = client_rect.right - client_rect.left;
      event->configure.height = client_rect.bottom - client_rect.top;
      
      event->configure.x = point.x;
      event->configure.y = point.y;

      append_event (event);
    }
}

GdkRegion *
_gdk_win32_hrgn_to_region (HRGN hrgn)
{
  RGNDATA *rgndata;
  RECT *rects;
  GdkRegion *result;
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

  result = gdk_region_new ();
  rects = (RECT *) rgndata->Buffer;
  for (i = 0; i < rgndata->rdh.nCount; i++)
    {
      GdkRectangle r;

      r.x = rects[i].left;
      r.y = rects[i].top;
      r.width = rects[i].right - r.x;
      r.height = rects[i].bottom - r.y;

      gdk_region_union_with_rect (result, &r);
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
		 GdkWindow  *window,
		 gboolean    return_exposes,
		 GdkEvent  **event)
{
  HRGN hrgn = CreateRectRgn (0, 0, 0, 0);
  HDC hdc;
  PAINTSTRUCT paintstruct;
  GdkRegion *update_region;
  gint xoffset, yoffset;

  if (GetUpdateRgn (msg->hwnd, hrgn, FALSE) == ERROR)
    {
      WIN32_GDI_FAILED ("GetUpdateRgn");
      return;
    }

  hdc = BeginPaint (msg->hwnd, &paintstruct);

  GDK_NOTE (EVENTS, g_print (" %s %s dc %p%s",
			     _gdk_win32_rect_to_string (&paintstruct.rcPaint),
			     (paintstruct.fErase ? "erase" : ""),
			     hdc,
			     (return_exposes ? " return_exposes" : "")));

  EndPaint (msg->hwnd, &paintstruct);

  /* HB: don't generate GDK_EXPOSE events for InputOnly
   * windows -> backing store now works!
   */
  if (((GdkWindowObject *) window)->input_only)
    {
      DeleteObject (hrgn);
      return;
    }

  if (!(((GdkWindowObject *) window)->event_mask & GDK_EXPOSURE_MASK))
    {
      GDK_NOTE (EVENTS, g_print (" (ignored)"));
      DeleteObject (hrgn);
      return;
    }

#if 0 /* we need to process exposes even with GDK_NO_BG
   * Otherwise The GIMP canvas update is broken ....
   */
  if (((GdkWindowObject *) window)->bg_pixmap == GDK_NO_BG)
    break;
#endif

  if ((paintstruct.rcPaint.right == paintstruct.rcPaint.left) ||
      (paintstruct.rcPaint.bottom == paintstruct.rcPaint.top))
    {
      GDK_NOTE (EVENTS, g_print (" (empty paintstruct, ignored)"));
      DeleteObject (hrgn);
      return;
    }

  if (return_exposes)
    {
      if (!GDK_WINDOW_DESTROYED (window))
	{
	  GList *list = _gdk_display->queued_events;

	  *event = gdk_event_new (GDK_EXPOSE);
	  (*event)->expose.window = window;
	  (*event)->expose.area.x = paintstruct.rcPaint.left;
	  (*event)->expose.area.y = paintstruct.rcPaint.top;
	  (*event)->expose.area.width = paintstruct.rcPaint.right - paintstruct.rcPaint.left;
	  (*event)->expose.area.height = paintstruct.rcPaint.bottom - paintstruct.rcPaint.top;
	  (*event)->expose.region = _gdk_win32_hrgn_to_region (hrgn);
	  (*event)->expose.count = 0;

	  while (list != NULL)
	    {
	      GdkEventPrivate *evp = list->data;

	      if (evp->event.any.type == GDK_EXPOSE &&
		  evp->event.any.window == window &&
		  !(evp->flags & GDK_EVENT_PENDING))
		evp->event.expose.count++;

	      list = list->next;
	    }
	}

      return;
    }

  update_region = _gdk_win32_hrgn_to_region (hrgn);

  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
  gdk_region_offset (update_region, xoffset, yoffset);
  
  _gdk_window_process_expose (window, update_region);
  gdk_region_destroy (update_region);

  DeleteObject (hrgn);
}

static void
handle_stuff_while_moving_or_resizing (void)
{
  int arbitrary_limit = 1;
  while (g_main_context_pending (NULL) && arbitrary_limit--)
    g_main_context_iteration (NULL, FALSE);
}

static VOID CALLBACK
resize_timer_proc (HWND     hwnd,
		   UINT     msg,
		   UINT     id,
		   DWORD    time)
{
  if (_sizemove_in_progress)
    handle_stuff_while_moving_or_resizing ();
}

static void
handle_display_change (void)
{
  _gdk_monitor_init ();
  _gdk_root_window_size_init ();
  g_signal_emit_by_name (_gdk_screen, "size_changed");
}

static void
generate_button_event (GdkEventType type,
		       gint         button,
		       GdkWindow   *window,
		       GdkWindow   *orig_window,
		       MSG         *msg)
{
  GdkEvent *event = gdk_event_new (type);
  gint xoffset, yoffset;

  event->button.window = window;
  event->button.time = _gdk_win32_get_next_tick (msg->time);
  if (window != orig_window)
    translate_mouse_coords (orig_window, window, msg);
  event->button.x = current_x = (gint16) GET_X_LPARAM (msg->lParam);
  event->button.y = current_y = (gint16) GET_Y_LPARAM (msg->lParam);
  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
  event->button.x += xoffset;
  event->button.y += yoffset;
  event->button.x_root = msg->pt.x + _gdk_offset_x;
  event->button.y_root = msg->pt.y + _gdk_offset_y;
  event->button.axes = NULL;
  event->button.state = build_pointer_event_state (msg);
  event->button.button = button;
  event->button.device = _gdk_display->core_pointer;

  append_event (event);

  if (type == GDK_BUTTON_PRESS)
    _gdk_event_button_generate (_gdk_display, event);
}

static gboolean
gdk_event_translate (MSG  *msg,
		     gint *ret_valp)
{
  RECT rect, *drag, orig_drag;
  POINT point;
  MINMAXINFO *mmi;
  HWND hwnd;
  HCURSOR hcursor;
  BYTE key_state[256];
  HIMC himc;

  GdkEvent *event;

  wchar_t wbuf[100];
  gint ccount;

  GdkWindow *window = NULL;
  GdkWindowImplWin32 *impl;

  GdkWindow *orig_window, *new_window;
  gint xoffset, yoffset;

  static gint update_colors_counter = 0;
  gint button;
  GdkAtom target;

  gchar buf[256];
  gboolean return_val = FALSE;

  int i;

  if (_gdk_default_filters)
    {
      /* Apply global filters */

      GdkFilterReturn result = apply_filters (NULL, msg, _gdk_default_filters);
      
      /* If result is GDK_FILTER_CONTINUE, we continue as if nothing
       * happened. If it is GDK_FILTER_REMOVE or GDK_FILTER_TRANSLATE,
       * we return TRUE, and DefWindowProc() will not be called.
       */
      if (result == GDK_FILTER_REMOVE || result == GDK_FILTER_TRANSLATE)
	return TRUE;
    }

  window = gdk_win32_handle_table_lookup ((GdkNativeWindow) msg->hwnd);
  orig_window = window;

  if (window == NULL)
    {
      /* XXX Handle WM_QUIT here ? */
      if (msg->message == WM_QUIT)
	{
	  GDK_NOTE (EVENTS, g_print (" %d", msg->wParam));
	  exit (msg->wParam);
	}
      else if (msg->message == WM_MOVE ||
	       msg->message == WM_SIZE)
	{
	  /* It's quite normal to get these messages before we have
	   * had time to register the window in our lookup table, or
	   * when the window is being destroyed and we already have
	   * removed it. Repost the same message to our queue so that
	   * we will get it later when we are prepared.
	   */
	  GDK_NOTE (EVENTS, g_print (" (posted)"));
	
	  PostMessage (msg->hwnd, msg->message,
		       msg->wParam, msg->lParam);
	}
      else if (msg->message == WM_CREATE)
	{
	  window = (UNALIGNED GdkWindow*) (((LPCREATESTRUCT) msg->lParam)->lpCreateParams);
	  GDK_WINDOW_HWND (window) = msg->hwnd;
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print (" (no GdkWindow)"));
	}
      return FALSE;
    }
  
  g_object_ref (window);

  /* window's refcount has now been increased, so code below should
   * not just return from this function, but instead goto done (or
   * break out of the big switch). To protect against forgetting this,
   * #define return to a syntax error...
   */
#define return GOTO_DONE_INSTEAD
  
  if (!GDK_WINDOW_DESTROYED (window) && ((GdkWindowObject *) window)->filters)
    {
      /* Apply per-window filters */

      GdkFilterReturn result = apply_filters (window, msg, ((GdkWindowObject *) window)->filters);

      if (result == GDK_FILTER_REMOVE || result == GDK_FILTER_TRANSLATE)
	{
	  return_val = TRUE;
	  goto done;
	}
    }

  if (msg->message == msh_mousewheel)
    {
      GDK_NOTE (EVENTS, g_print (" (MSH_MOUSEWHEEL)"));
      
      /* MSH_MOUSEWHEEL is delivered to the foreground window.  Work
       * around that. Also, the position is in screen coordinates, not
       * client coordinates as with the button messages.
       */
      point.x = GET_X_LPARAM (msg->lParam);
      point.y = GET_Y_LPARAM (msg->lParam);
      if ((hwnd = WindowFromPoint (point)) == NULL)
	goto done;

      msg->hwnd = hwnd;
      if ((new_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) msg->hwnd)) == NULL)
	goto done;

      assign_object (&window, new_window);

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_scroll, TRUE))
	goto done;

      if (GDK_WINDOW_DESTROYED (window))
	goto done;

      ScreenToClient (msg->hwnd, &point);

      event = gdk_event_new (GDK_SCROLL);
      event->scroll.window = window;
      event->scroll.direction = ((int) msg->wParam > 0) ?
	GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      event->scroll.time = _gdk_win32_get_next_tick (msg->time);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->scroll.x = (gint16) point.x + xoffset;
      event->scroll.y = (gint16) point.y + yoffset;
      event->scroll.x_root = (gint16) GET_X_LPARAM (msg->lParam) + _gdk_offset_x;
      event->scroll.y_root = (gint16) GET_Y_LPARAM (msg->lParam) + _gdk_offset_y;
      event->scroll.state = 0;	/* No state information with MSH_MOUSEWHEEL */
      event->scroll.device = _gdk_display->core_pointer;

      append_event (event);

      return_val = TRUE;
      goto done;
    }
  else if (msg->message == client_message)
    {
      GList *tmp_list;
      GdkFilterReturn result = GDK_FILTER_CONTINUE;

      GDK_NOTE (EVENTS, g_print (" client_message"));

      tmp_list = client_filters;
      while (tmp_list)
	{
	  GdkClientFilter *filter = tmp_list->data;

	  tmp_list = tmp_list->next;

	  if (filter->type == GDK_POINTER_TO_ATOM (msg->wParam))
	    {
	      GList *filter_list = g_list_append (NULL, filter);
	      
	      GDK_NOTE (EVENTS, g_print (" (match)"));

	      result = apply_filters (window, msg, filter_list);

	      g_list_free (filter_list);

	      if (result != GDK_FILTER_CONTINUE)
		break;
	    }
	}

      if (result == GDK_FILTER_REMOVE || result == GDK_FILTER_TRANSLATE)
	{
	  return_val = TRUE;
	  goto done;
	}
      else
	{
	  /* Send unknown client messages on to Gtk for it to use */

	  event = gdk_event_new (GDK_CLIENT_EVENT);
	  event->client.window = window;
	  event->client.message_type = GDK_POINTER_TO_ATOM (msg->wParam);
	  event->client.data_format = 32;
	  event->client.data.l[0] = msg->lParam;
	  for (i = 1; i < 5; i++)
	    event->client.data.l[i] = 0;
	  
	  append_event (event);
	  
	  return_val = TRUE;
	  goto done;
	}
    }

  switch (msg->message)
    {
    case WM_INPUTLANGCHANGE:
      _gdk_input_locale = (HKL) msg->lParam;
      _gdk_input_locale_is_ime = ImmIsIME (_gdk_input_locale);
      GetLocaleInfo (MAKELCID (LOWORD (_gdk_input_locale), SORT_DEFAULT),
		     LOCALE_IDEFAULTANSICODEPAGE,
		     buf, sizeof (buf));
      _gdk_input_codepage = atoi (buf);
      _gdk_keymap_serial++;
      GDK_NOTE (EVENTS,
		g_print (" cs:%lu hkl:%lx%s cp:%d",
			 (gulong) msg->wParam,
			 msg->lParam, _gdk_input_locale_is_ime ? " (IME)" : "",
			 _gdk_input_codepage));
      break;

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
      GDK_NOTE (EVENTS,
		g_print (" %s ch:%.02x %s",
			 _gdk_win32_key_to_string (msg->lParam),
			 msg->wParam,
			 decode_key_lparam (msg->lParam)));

      /* If posted without us having keyboard focus, ignore */
      if ((msg->wParam != VK_F10 && msg->wParam != VK_MENU) &&
	  !(HIWORD (msg->lParam) & KF_ALTDOWN))
	break;

      /* Let the system handle Alt-Tab, Alt-Space and Alt-F4 unless
       * the keyboard is grabbed.
       */
      if (k_grab_window == NULL &&
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
			 msg->wParam,
			 decode_key_lparam (msg->lParam)));

    keyup_or_down:

      /* Ignore key messages intended for the IME */
      if (msg->wParam == VK_PROCESSKEY ||
	  in_ime_composition)
	break;

      if (!propagate (&window, msg,
		      k_grab_window, k_grab_owner_events, GDK_ALL_EVENTS_MASK,
		      doesnt_want_key, FALSE))
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      event = gdk_event_new ((msg->message == WM_KEYDOWN ||
			      msg->message == WM_SYSKEYDOWN) ?
			     GDK_KEY_PRESS : GDK_KEY_RELEASE);
      event->key.window = window;
      event->key.time = _gdk_win32_get_next_tick (msg->time);
      event->key.keyval = GDK_VoidSymbol;
      event->key.string = NULL;
      event->key.length = 0;
      event->key.hardware_keycode = msg->wParam;
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
	       LOBYTE (HIWORD (msg->lParam)) == _scancode_rshift)
	event->key.hardware_keycode = VK_RSHIFT;

      API_CALL (GetKeyboardState, (key_state));

      /* g_print ("ctrl:%02x lctrl:%02x rctrl:%02x alt:%02x lalt:%02x ralt:%02x\n", key_state[VK_CONTROL], key_state[VK_LCONTROL], key_state[VK_RCONTROL], key_state[VK_MENU], key_state[VK_LMENU], key_state[VK_RMENU]); */
      
      build_key_event_state (event, key_state);

      gdk_keymap_translate_keyboard_state (NULL,
					   event->key.hardware_keycode,
					   event->key.state,
					   event->key.group,
					   &event->key.keyval,
					   NULL, NULL, NULL);

      fill_key_event_string (event);

      /* Reset MOD1_MASK if it is the Alt key itself */
      if (msg->wParam == VK_MENU)
	event->key.state &= ~GDK_MOD1_MASK;

      append_event (event);

      return_val = TRUE;
      break;

    case WM_SYSCHAR:
      if (msg->wParam != VK_SPACE)
	{
	  /* To prevent beeps, don't let DefWindowProc() be called */
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
      GDK_NOTE (EVENTS, g_print (" %#lx", msg->lParam));

      if (!(msg->lParam & GCS_RESULTSTR))
	break;

      if (!propagate (&window, msg,
		      k_grab_window, k_grab_owner_events, GDK_ALL_EVENTS_MASK,
		      doesnt_want_char, FALSE))
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      himc = ImmGetContext (msg->hwnd);
      ccount = ImmGetCompositionStringW (himc, GCS_RESULTSTR,
					 wbuf, sizeof (wbuf));
      ImmReleaseContext (msg->hwnd, himc);

      ccount /= 2;

      API_CALL (GetKeyboardState, (key_state));

      for (i = 0; i < ccount; i++)
	{
	  if (((GdkWindowObject *) window)->event_mask & GDK_KEY_PRESS_MASK)
	    {
	      /* Build a key press event */
	      event = gdk_event_new (GDK_KEY_PRESS);
	      event->key.window = window;
	      build_wm_ime_composition_event (event, msg, wbuf[i], key_state);

	      append_event (event);
	    }
	  
	  if (((GdkWindowObject *) window)->event_mask & GDK_KEY_RELEASE_MASK)
	    {
	      /* Build a key release event.  */
	      event = gdk_event_new (GDK_KEY_RELEASE);
	      event->key.window = window;
	      build_wm_ime_composition_event (event, msg, wbuf[i], key_state);

	      append_event (event);
	    }
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

      assign_object (&window, find_window_for_mouse_event (window, msg));

      if (p_grab_window != NULL)
	{
	  GdkWindow *real_window = find_real_window_for_grabbed_mouse_event (window, msg);

	  if (real_window != current_window)
	    synthesize_crossing_events (real_window, GDK_CROSSING_NORMAL, msg);
	}
      else
	{
	  if (window != current_window)
	    synthesize_crossing_events (window, GDK_CROSSING_NORMAL, msg);
	}

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_press, TRUE))
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      /* Emulate X11's automatic active grab */
      if (!p_grab_window)
	{
	  /* No explicit active grab, let's start one automatically */
	  GDK_NOTE (EVENTS, g_print (" (automatic grab)"));
	  gdk_pointer_grab (window,
			    FALSE,
			    ((GdkWindowObject *) window)->event_mask,
			    NULL, NULL, 0);
	  p_grab_automatic = TRUE;
	}

      generate_button_event (GDK_BUTTON_PRESS, button,
			     window, orig_window, msg);

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

      assign_object (&window, find_window_for_mouse_event (window, msg));

      if (p_grab_window != NULL)
	{
	  GdkWindow *real_window = find_real_window_for_grabbed_mouse_event (window, msg);

	  if (real_window != current_window)
	    synthesize_crossing_events (real_window, GDK_CROSSING_NORMAL, msg);
	}
      else
	{
	  if (window != current_window)
	    synthesize_crossing_events (window, GDK_CROSSING_NORMAL, msg);
	}

#if 0
      if (((GdkWindowObject *) window)->extension_events != 0 &&
	  _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print (" (ignored)"));
	  break;
	}
#endif

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_release, TRUE))
	{
	}
      else if (!GDK_WINDOW_DESTROYED (window))
	{
	  generate_button_event (GDK_BUTTON_RELEASE, button,
				 window, orig_window, msg);
	}

      if (p_grab_window != NULL &&
	  p_grab_automatic &&
	  (msg->wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0)
	{
	  /* Terminate automatic grab */
	  gdk_pointer_ungrab (0);
	}

      return_val = TRUE;
      break;

    case WM_MOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print (" %#x (%d,%d)",
			 msg->wParam,
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      /* If we haven't moved, don't create any GDK event. Windows
       * sends WM_MOUSEMOVE messages after a new window is shows under
       * the mouse, even if the mouse hasn't moved. This disturbs gtk.
       */
      if (msg->pt.x + _gdk_offset_x == current_root_x &&
	  msg->pt.y + _gdk_offset_y == current_root_y)
	break;

      current_root_x = msg->pt.x + _gdk_offset_x;
      current_root_y = msg->pt.y + _gdk_offset_y;

      assign_object (&window, find_window_for_mouse_event (window, msg));

      if (p_grab_window != NULL)
	{
	  GdkWindow *real_window = find_real_window_for_grabbed_mouse_event (window, msg);

	  if (real_window != current_window)
	    {
	      if (p_grab_owner_events)
		{
		  synthesize_crossing_events (real_window, GDK_CROSSING_NORMAL, msg);
		}
	      else if (current_window == p_grab_window)
		{
		  synthesize_leave_event (p_grab_window, msg, GDK_CROSSING_NORMAL, GDK_NOTIFY_ANCESTOR);
		  assign_object (&current_window, _gdk_root);
		}
	      else if (real_window == p_grab_window)
		{
		  synthesize_enter_event (p_grab_window, msg, GDK_CROSSING_NORMAL, GDK_NOTIFY_ANCESTOR);
		  assign_object (&current_window, p_grab_window);
		}
	    }
	}
      else
	{
	  if (window != current_window)
	    synthesize_crossing_events (window, GDK_CROSSING_NORMAL, msg);
	}

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_motion, TRUE))
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      if (window != orig_window)
	translate_mouse_coords (orig_window, window, msg);

      event = gdk_event_new (GDK_MOTION_NOTIFY);
      event->motion.window = window;
      event->motion.time = _gdk_win32_get_next_tick (msg->time);
      event->motion.x = current_x = (gint16) GET_X_LPARAM (msg->lParam);
      event->motion.y = current_y = (gint16) GET_Y_LPARAM (msg->lParam);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->motion.x += xoffset;
      event->motion.y += yoffset;
      event->motion.x_root = current_root_x;
      event->motion.y_root = current_root_y;
      event->motion.axes = NULL;
      event->motion.state = build_pointer_event_state (msg);
      event->motion.is_hint = FALSE;
      event->motion.device = _gdk_display->core_pointer;

      append_event (event);

      return_val = TRUE;
      break;

    case WM_NCMOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print (" (%d,%d)",
			 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));
      if (current_window != NULL &&
	  (((GdkWindowObject *) current_window)->event_mask & GDK_LEAVE_NOTIFY_MASK))
	{
	  synthesize_crossing_events (_gdk_root, GDK_CROSSING_NORMAL, msg);
	}

      break;

    case WM_MOUSELEAVE:
      GDK_NOTE (EVENTS, g_print (" %d (%ld,%ld)",
				 HIWORD (msg->wParam), msg->pt.x, msg->pt.y));

      if (!gdk_win32_handle_table_lookup ((GdkNativeWindow) WindowFromPoint (msg->pt)))
	{
	  GdkNotifyType detail;

	  if (GDK_WINDOW_TYPE (current_window) != GDK_WINDOW_CHILD)
	    detail = GDK_NOTIFY_ANCESTOR;
	  else
	    detail = GDK_NOTIFY_UNKNOWN;

	  /* we are only interested if we don't know the new window */
	  synthesize_enter_or_leave_event (current_window, msg, 
					   GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL, detail,
					   current_x, current_y);
	  assign_object (&current_window, _gdk_root);
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print (" (ignored)"));
	}
      return_val = TRUE;
      break;

    case WM_MOUSEWHEEL:
      GDK_NOTE (EVENTS, g_print (" %d", HIWORD (msg->wParam)));

      /* WM_MOUSEWHEEL is delivered to the focus window. Work around
       * that. Also, the position is in screen coordinates, not client
       * coordinates as with the button messages. I love the
       * consistency of Windows.
       */
      point.x = GET_X_LPARAM (msg->lParam);
      point.y = GET_Y_LPARAM (msg->lParam);

      if ((hwnd = WindowFromPoint (point)) == NULL)
	break;

      msg->hwnd = hwnd;
      if ((new_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) msg->hwnd)) == NULL)
	break;

      if (new_window != window)
	{
	  assign_object (&window, new_window);
	}

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_scroll, TRUE))
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      ScreenToClient (msg->hwnd, &point);

      event = gdk_event_new (GDK_SCROLL);
      event->scroll.window = window;
      event->scroll.direction = (((short) HIWORD (msg->wParam)) > 0) ?
	GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      event->scroll.time = _gdk_win32_get_next_tick (msg->time);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->scroll.x = (gint16) point.x + xoffset;
      event->scroll.y = (gint16) point.y + yoffset;
      event->scroll.x_root = (gint16) GET_X_LPARAM (msg->lParam) + _gdk_offset_x;
      event->scroll.y_root = (gint16) GET_Y_LPARAM (msg->lParam) + _gdk_offset_y;
      event->scroll.state = build_pointer_event_state (msg);
      event->scroll.device = _gdk_display->core_pointer;

      append_event (event);
      
      return_val = TRUE;
      break;

    case WM_QUERYNEWPALETTE:
      if (gdk_visual_get_system ()->type == GDK_VISUAL_PSEUDO_COLOR)
	{
	  synthesize_expose_events (window);
	  update_colors_counter = 0;
	}
      return_val = TRUE;
      break;

    case WM_PALETTECHANGED:
      GDK_NOTE (EVENTS_OR_COLORMAP, g_print (" %p", (HWND) msg->wParam));
      if (gdk_visual_get_system ()->type != GDK_VISUAL_PSEUDO_COLOR)
	break;

      return_val = TRUE;

      if (msg->hwnd == (HWND) msg->wParam)
	break;

      if (++update_colors_counter == 5)
	{
	  synthesize_expose_events (window);
	  update_colors_counter = 0;
	  break;
	}
      
      update_colors (window, TRUE);
      break;

     case WM_MOUSEACTIVATE:
       if (gdk_window_get_window_type (window) == GDK_WINDOW_TEMP 
	   || !((GdkWindowObject *)window)->accept_focus)
	 {
	   *ret_valp = MA_NOACTIVATE;
	   return_val = TRUE;
	 }
       break;

    case WM_KILLFOCUS:
      if (p_grab_window != NULL && !GDK_WINDOW_DESTROYED (p_grab_window))
	generate_grab_broken_event (p_grab_window, FALSE, NULL);

      if (k_grab_window != NULL && !GDK_WINDOW_DESTROYED (k_grab_window) 
	  && k_grab_window != p_grab_window)
	generate_grab_broken_event (k_grab_window, TRUE, NULL);

      /* fallthrough */
    case WM_SETFOCUS:
      if (k_grab_window != NULL && !k_grab_owner_events)
	break;

      if (!(((GdkWindowObject *) window)->event_mask & GDK_FOCUS_CHANGE_MASK))
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      generate_focus_event (window, (msg->message == WM_SETFOCUS));
      return_val = TRUE;
      break;

    case WM_ERASEBKGND:
      GDK_NOTE (EVENTS, g_print (" %p", (HANDLE) msg->wParam));
      
      if (GDK_WINDOW_DESTROYED (window))
	break;

      return_val = TRUE;
      *ret_valp = 1;
      break;

    case WM_PAINT:
      handle_wm_paint (msg, window, FALSE, NULL);
      break;

    case WM_SETCURSOR:
      GDK_NOTE (EVENTS, g_print (" %#x %#x",
				 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (p_grab_window == NULL && LOWORD (msg->lParam) != HTCLIENT)
	break;

      if (p_grab_window != NULL && p_grab_cursor != NULL)
	hcursor = p_grab_cursor;
      else if (!GDK_WINDOW_DESTROYED (window))
	hcursor = GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->hcursor;
      else
	hcursor = NULL;

      if (hcursor != NULL)
	{
	  GDK_NOTE (EVENTS, g_print (" (SetCursor(%p)", hcursor));
	  SetCursor (hcursor);
	  return_val = TRUE;
	  *ret_valp = TRUE;
	}
      break;

    case WM_SHOWWINDOW:
      GDK_NOTE (EVENTS, g_print (" %d", msg->wParam));

      if (!(((GdkWindowObject *) window)->event_mask & GDK_STRUCTURE_MASK))
	break;

      if (msg->lParam == SW_OTHERUNZOOM ||
	  msg->lParam == SW_OTHERZOOM)
	break;

      if (GDK_WINDOW_DESTROYED (window))
	break;

      event = gdk_event_new (msg->wParam ? GDK_MAP : GDK_UNMAP);
      event->any.window = window;

      append_event (event);

      if (event->any.type == GDK_UNMAP)
	{
	  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

	  if (impl->transient_owner && GetForegroundWindow () == GDK_WINDOW_HWND (window))
	    {
	      SetForegroundWindow (GDK_WINDOW_HWND (impl->transient_owner));
	    }
	}

      if (event->any.type == GDK_UNMAP &&
	  p_grab_window == window)
	gdk_pointer_ungrab (msg->time);

      if (event->any.type == GDK_UNMAP &&
	  k_grab_window == window)
	gdk_keyboard_ungrab (msg->time);

      return_val = TRUE;
      break;

    case WM_SIZE:
      GDK_NOTE (EVENTS,
		g_print (" %s %dx%d",
			 (msg->wParam == SIZE_MAXHIDE ? "MAXHIDE" :
			  (msg->wParam == SIZE_MAXIMIZED ? "MAXIMIZED" :
			   (msg->wParam == SIZE_MAXSHOW ? "MAXSHOW" :
			    (msg->wParam == SIZE_MINIMIZED ? "MINIMIZED" :
			     (msg->wParam == SIZE_RESTORED ? "RESTORED" : "?"))))),
			 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (msg->wParam == SIZE_MINIMIZED)
	{
	  /* Don't generate any GDK event. This is *not* an UNMAP. */

	  if (p_grab_window == window)
	    gdk_pointer_ungrab (msg->time);

	  if (k_grab_window == window)
	    gdk_keyboard_ungrab (msg->time);

	  gdk_synthesize_window_state (window,
				       GDK_WINDOW_STATE_WITHDRAWN,
				       GDK_WINDOW_STATE_ICONIFIED);
	}
      else if ((msg->wParam == SIZE_RESTORED ||
		msg->wParam == SIZE_MAXIMIZED) &&
	       GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD)
	{
	  GdkWindowState withdrawn_bit =
	    IsWindowVisible (msg->hwnd) ? GDK_WINDOW_STATE_WITHDRAWN : 0;

	  if (!GDK_WINDOW_DESTROYED (window))
	    handle_configure_event (msg, window);
	  
	  if (msg->wParam == SIZE_RESTORED)
	    gdk_synthesize_window_state (window,
					 GDK_WINDOW_STATE_ICONIFIED |
					 GDK_WINDOW_STATE_MAXIMIZED |
					 withdrawn_bit,
					 0);
	  else if (msg->wParam == SIZE_MAXIMIZED)
	    gdk_synthesize_window_state (window,
					 GDK_WINDOW_STATE_ICONIFIED |
					 withdrawn_bit,
					 GDK_WINDOW_STATE_MAXIMIZED);

	  if (((GdkWindowObject *) window)->resize_count > 1)
	    ((GdkWindowObject *) window)->resize_count -= 1;
	  
	  if (((GdkWindowObject *) window)->extension_events != 0)
	    _gdk_input_configure_event (window);

	  return_val = TRUE;
	}
      break;

    case WM_ENTERSIZEMOVE:
      _sizemove_in_progress = TRUE;
      resize_timer = SetTimer (NULL, 0, 20, resize_timer_proc);
      break;

    case WM_EXITSIZEMOVE:
      _sizemove_in_progress = FALSE;
      KillTimer (NULL, resize_timer);
      break;

    case WM_WINDOWPOSCHANGED :
      /* Once we've entered the moving or sizing modal loop, we won't
       * return to the main loop until we're done sizing or moving.
       */
      if (_sizemove_in_progress &&
	 GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&
	 !GDK_WINDOW_DESTROYED (window))
	{
	  if (((GdkWindowObject *) window)->event_mask & GDK_STRUCTURE_MASK)
	    {
	      if (((GdkWindowObject *) window)->resize_count > 1)
		((GdkWindowObject *) window)->resize_count -= 1;
	      
	      handle_configure_event (msg, window);
	      g_main_context_iteration (NULL, FALSE);
	      
	      /* Dispatch main loop - to realize resizes... */
	      handle_stuff_while_moving_or_resizing ();
	      
	      /* Claim as handled, so that WM_SIZE and WM_MOVE are avoided */
	      return_val = TRUE;
	      *ret_valp = 1;
	    }
	}
      break;

    case WM_SIZING:
      GetWindowRect (GDK_WINDOW_HWND (window), &rect);
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

      impl = GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl);
      orig_drag = *drag;
      if (impl->hint_flags & GDK_HINT_RESIZE_INC)
	{
	  GDK_NOTE (EVENTS, g_print (" (RESIZE_INC)"));
	  if (impl->hint_flags & GDK_HINT_BASE_SIZE)
	    {
	      /* Resize in increments relative to the base size */
	      rect.left = rect.top = 0;
	      rect.right = impl->hints.base_width;
	      rect.bottom = impl->hints.base_height;
	      _gdk_win32_adjust_client_rect (window, &rect);
	      point.x = rect.left;
	      point.y = rect.top;
	      ClientToScreen (GDK_WINDOW_HWND (window), &point);
	      rect.left = point.x;
	      rect.top = point.y;
	      point.x = rect.right;
	      point.y = rect.bottom;
	      ClientToScreen (GDK_WINDOW_HWND (window), &point);
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
	      adjust_drag (&drag->bottom, rect.bottom, impl->hints.height_inc);
	      break;

	    case WMSZ_BOTTOMLEFT:
	      if (drag->bottom == rect.bottom && drag->left == rect.left)
		break;
	      adjust_drag (&drag->bottom, rect.bottom, impl->hints.height_inc);
	      adjust_drag (&drag->left, rect.left, impl->hints.width_inc);
	      break;

	    case WMSZ_LEFT:
	      if (drag->left == rect.left)
		break;
	      adjust_drag (&drag->left, rect.left, impl->hints.width_inc);
	      break;

	    case WMSZ_TOPLEFT:
	      if (drag->top == rect.top && drag->left == rect.left)
		break;
	      adjust_drag (&drag->top, rect.top, impl->hints.height_inc);
	      adjust_drag (&drag->left, rect.left, impl->hints.width_inc);
	      break;

	    case WMSZ_TOP:
	      if (drag->top == rect.top)
		break;
	      adjust_drag (&drag->top, rect.top, impl->hints.height_inc);
	      break;

	    case WMSZ_TOPRIGHT:
	      if (drag->top == rect.top && drag->right == rect.right)
		break;
	      adjust_drag (&drag->top, rect.top, impl->hints.height_inc);
	      adjust_drag (&drag->right, rect.right, impl->hints.width_inc);
	      break;

	    case WMSZ_RIGHT:
	      if (drag->right == rect.right)
		break;
	      adjust_drag (&drag->right, rect.right, impl->hints.width_inc);
	      break;

	    case WMSZ_BOTTOMRIGHT:
	      if (drag->bottom == rect.bottom && drag->right == rect.right)
		break;
	      adjust_drag (&drag->bottom, rect.bottom, impl->hints.height_inc);
	      adjust_drag (&drag->right, rect.right, impl->hints.width_inc);
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

	  GetClientRect (GDK_WINDOW_HWND (window), &rect);
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
      if (GDK_WINDOW_DESTROYED (window))
	break;

      impl = GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl);
      mmi = (MINMAXINFO*) msg->lParam;
      GDK_NOTE (EVENTS, g_print (" (mintrack:%ldx%ld maxtrack:%ldx%ld "
				 "maxpos:%+ld%+ld maxsize:%ldx%ld)",
				 mmi->ptMinTrackSize.x, mmi->ptMinTrackSize.y,
				 mmi->ptMaxTrackSize.x, mmi->ptMaxTrackSize.y,
				 mmi->ptMaxPosition.x, mmi->ptMaxPosition.y,
				 mmi->ptMaxSize.x, mmi->ptMaxSize.y));

      if (impl->hint_flags & GDK_HINT_MIN_SIZE)
	{
	  rect.left = rect.top = 0;
	  rect.right = impl->hints.min_width;
	  rect.bottom = impl->hints.min_height;

	  _gdk_win32_adjust_client_rect (window, &rect);

	  mmi->ptMinTrackSize.x = rect.right - rect.left;
	  mmi->ptMinTrackSize.y = rect.bottom - rect.top;
	}

      if (impl->hint_flags & GDK_HINT_MAX_SIZE)
	{
	  int maxw, maxh;

	  rect.left = rect.top = 0;
	  rect.right = impl->hints.max_width;
	  rect.bottom = impl->hints.max_height;

	  _gdk_win32_adjust_client_rect (window, &rect);

	  /* at least on win9x we have the 16 bit trouble */
	  maxw = rect.right - rect.left;
	  maxh = rect.bottom - rect.top;
	  mmi->ptMaxTrackSize.x = maxw > 0 && maxw < G_MAXSHORT ? maxw : G_MAXSHORT;
	  mmi->ptMaxTrackSize.y = maxh > 0 && maxh < G_MAXSHORT ? maxh : G_MAXSHORT;
	}

      if (impl->hint_flags & (GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE))
	{
	  /* Don't call DefWindowProc() */
	  GDK_NOTE (EVENTS, g_print (" (handled, mintrack:%ldx%ld maxtrack:%ldx%ld "
				     "maxpos:%+ld%+ld maxsize:%ldx%ld)",
				     mmi->ptMinTrackSize.x, mmi->ptMinTrackSize.y,
				     mmi->ptMaxTrackSize.x, mmi->ptMaxTrackSize.y,
				     mmi->ptMaxPosition.x, mmi->ptMaxPosition.y,
				     mmi->ptMaxSize.x, mmi->ptMaxSize.y));
	  return_val = TRUE;
	}
      break;

    case WM_MOVE:
      GDK_NOTE (EVENTS, g_print (" (%d,%d)",
				 GET_X_LPARAM (msg->lParam), GET_Y_LPARAM (msg->lParam)));

      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&
	  !IsIconic (msg->hwnd))
	{
	  if (!GDK_WINDOW_DESTROYED (window))
	    handle_configure_event (msg, window);

	  return_val = TRUE;
	}
      break;

    case WM_CLOSE:
      if (GDK_WINDOW_DESTROYED (window))
	break;

      event = gdk_event_new (GDK_DELETE);
      event->any.window = window;

      append_event (event);

      impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

      if (impl->transient_owner && GetForegroundWindow() == GDK_WINDOW_HWND (window))
	{
	  SetForegroundWindow (GDK_WINDOW_HWND (impl->transient_owner));
	}

      return_val = TRUE;
      break;

    case WM_DESTROY:
      if (window == current_window)
	assign_object (&current_window, _gdk_root);

      if (p_grab_window == window)
	gdk_pointer_ungrab (msg->time);

      if (k_grab_window == window)
	gdk_keyboard_ungrab (msg->time);

      if ((window != NULL) && (msg->hwnd != GetDesktopWindow ()))
	gdk_window_destroy_notify (window);

      if (window == NULL || GDK_WINDOW_DESTROYED (window))
	break;

      event = gdk_event_new (GDK_DESTROY);
      event->any.window = window;

      append_event (event);

      return_val = TRUE;
      break;

    case WM_DISPLAYCHANGE:
      handle_display_change ();
      break;
      
    case WM_DESTROYCLIPBOARD:
      if (!_ignore_destroy_clipboard)
	{
	  event = gdk_event_new (GDK_SELECTION_CLEAR);
	  event->selection.window = window;
	  event->selection.selection = GDK_SELECTION_CLIPBOARD;
	  event->selection.time = _gdk_win32_get_next_tick (msg->time);
          append_event (event);
	}
      else
	return_val = TRUE;
      break;

    case WM_RENDERFORMAT:
      GDK_NOTE (EVENTS, g_print (" %s", _gdk_win32_cf_to_string (msg->wParam)));

      if (!(target = g_hash_table_lookup (_format_atom_table, GINT_TO_POINTER (msg->wParam))))
	{
	  GDK_NOTE (EVENTS, g_print (" (target not found)"));
	  return_val = TRUE;
	  break;
	}

      /* We need to render to clipboard immediately, don't call
       * append_event()
       */
      if (_gdk_event_func)
	{
	  event = gdk_event_new (GDK_SELECTION_REQUEST);
	  event->selection.window = window;
	  event->selection.send_event = FALSE;
	  event->selection.selection = GDK_SELECTION_CLIPBOARD;
	  event->selection.target = target;
	  event->selection.property = _gdk_selection_property;
	  event->selection.requestor = (guint32) msg->hwnd;
	  event->selection.time = msg->time;

	  fixup_event (event);
	  GDK_NOTE (EVENTS, g_print (" (calling gdk_event_func)"));
	  GDK_NOTE (EVENTS, print_event (event));
	  (*_gdk_event_func) (event, _gdk_event_data);
	  gdk_event_free (event);

	  /* Now the clipboard owner should have rendered */
	  if (!_delayed_rendering_data)
	    GDK_NOTE (EVENTS, g_print (" (no _delayed_rendering_data?)"));
	  else
	    {
	      if (msg->wParam == CF_DIB)
		{
		  _delayed_rendering_data =
		    _gdk_win32_selection_convert_to_dib (_delayed_rendering_data,
							 target);
		  if (!_delayed_rendering_data)
		    {
		      g_warning ("Cannot convert to DIB from delayed rendered image");
		      break;
		    }
		}
	      /* The requestor is holding the clipboard, no
	       * OpenClipboard() is required/possible
	       */
	      API_CALL (SetClipboardData, (msg->wParam, _delayed_rendering_data));
	      _delayed_rendering_data = NULL;
	    }
	}
      break;

#ifdef HAVE_WINTAB
    case WM_ACTIVATE:
      /* Bring any tablet contexts to the top of the overlap order when
       * one of our windows is activated.
       * NOTE: It doesn't seem to work well if it is done in WM_ACTIVATEAPP
       * instead
       */
      if (LOWORD(msg->wParam) != WA_INACTIVE)
	_gdk_input_set_tablet_active ();
      break;
      
      /* Handle WINTAB events here, as we know that gdkinput.c will
       * use the fixed WT_DEFBASE as lcMsgBase, and we thus can use the
       * constants as case labels.
       */
    case WT_PACKET:
      GDK_NOTE (EVENTS, g_print (" %d %#lx",
				 msg->wParam, msg->lParam));
      goto wintab;
      
    case WT_CSRCHANGE:
      GDK_NOTE (EVENTS, g_print (" %d %#lx",
				 msg->wParam, msg->lParam));
      goto wintab;
      
    case WT_PROXIMITY:
      GDK_NOTE (EVENTS, g_print (" %#x %d %d",
				 msg->wParam,
				 LOWORD (msg->lParam),
				 HIWORD (msg->lParam)));
      /* Fall through */
    wintab:

      event = gdk_event_new (GDK_NOTHING);
      event->any.window = window;
      g_object_ref (window);
      if (_gdk_input_other_event (event, msg, window))
	append_event (event);
      else
	gdk_event_free (event);
      break;
#endif
    }

done:

  if (window)
    g_object_unref (window);
  
#undef return
  return return_val;
}

void
_gdk_events_queue (GdkDisplay *display)
{
  MSG msg;

  if (modal_win32_dialog != NULL)
    return;
  
  while (!_gdk_event_queue_find_first (display) &&
	 PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
}

static gboolean
gdk_event_prepare (GSource *source,
		   gint    *timeout)
{
  MSG msg;
  gboolean retval;

  GDK_THREADS_ENTER ();

  *timeout = -1;

  retval = (_gdk_event_queue_find_first (_gdk_display) != NULL ||
	    (modal_win32_dialog == NULL &&
	     PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)));

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_check (GSource *source)
{
  MSG msg;
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (_gdk_display) != NULL ||
	      (modal_win32_dialog == NULL &&
	       PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)));
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)
{
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  _gdk_events_queue (_gdk_display);
  event = _gdk_event_unqueue (_gdk_display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

void
gdk_win32_set_modal_dialog_libgtk_only (HWND window)
{
  modal_win32_dialog = window;
}

static void
check_for_too_much_data (GdkEvent *event)
{
  if (event->client.data.l[1] ||
      event->client.data.l[2] ||
      event->client.data.l[3] ||
      event->client.data.l[4])
    g_warning ("Only four bytes of data are passed in client messages on Win32\n");
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay     *display,
                                           GdkEvent       *event, 
                                           GdkNativeWindow winid)
{
  check_for_too_much_data (event);

  return PostMessage ((HWND) winid, client_message,
		      (WPARAM) event->client.message_type,
		      event->client.data.l[0]);
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen, 
				     GdkEvent  *event)
{
  check_for_too_much_data (event);

  PostMessage (HWND_BROADCAST, client_message,
	       (WPARAM) event->client.message_type,
	       event->client.data.l[0]);
}

void
gdk_flush (void)
{
#if 0
  MSG msg;

  /* Process all messages currently available */
  while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
#endif

  GdiFlush ();
}

void
gdk_display_sync (GdkDisplay * display)
{
  MSG msg;

  g_return_if_fail (display == _gdk_display);

  /* Process all messages currently available */
  while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    DispatchMessage (&msg);
}

void
gdk_display_flush (GdkDisplay * display)
{
  g_return_if_fail (display == _gdk_display);

  /* Nothing */
}

gboolean
gdk_net_wm_supports (GdkAtom property)
{
  return FALSE;
}
