/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gdkprivate-win32.h"
#include "gdkdisplay-win32.h"

#undef STRICT
#include <pango/pangowin32.h>

struct _GdkWin32ALPNSink
{
  ITfActiveLanguageProfileNotifySink itf_alpn_sink;

  int ref_count;
  guint input_locale_is_ime : 1;
};

typedef struct _GdkWin32ALPNSink GdkWin32ALPNSink;

struct _GdkWin32InputLocaleItems
{
  /* Input locale items */
  HKL input_locale;

  /* COM objects to receive language or text service change notifcations in GdkWin32 */
  GdkWin32ALPNSink *notification_sink;
  ITfSource *itf_source;
  DWORD actlangchangenotify_id;

  /* keymap items */
  GdkKeymap *default_keymap;
  guint keymap_serial;
};

static guint
gdk_handle_hash (HANDLE *handle)
{
#ifdef _WIN64
  return ((guint *) handle)[0] ^ ((guint *) handle)[1];
#else
  return (guint) *handle;
#endif
}

static int
gdk_handle_equal (HANDLE *a,
		  HANDLE *b)
{
  return (*a == *b);
}

#define GDK_DISPLAY_HANDLE_HT(d) GDK_WIN32_DISPLAY(d)->display_surface_record->handle_ht

void
gdk_win32_display_handle_table_insert (GdkDisplay *display,
                                       HANDLE     *handle,
                                       gpointer    data)
{
  g_return_if_fail (handle != NULL);

  g_hash_table_insert (GDK_DISPLAY_HANDLE_HT (display), handle, data);
}

void
gdk_win32_display_handle_table_remove (GdkDisplay *display,
                                       HANDLE      handle)
{
  g_hash_table_remove (GDK_DISPLAY_HANDLE_HT (display), &handle);
}

gpointer
gdk_win32_display_handle_table_lookup_ (GdkDisplay *display,
                                        HWND        handle)
{
  gpointer data = NULL;
  if (display == NULL)
    display = gdk_display_get_default ();

  data = g_hash_table_lookup (GDK_DISPLAY_HANDLE_HT (display), &handle);

  return data;
}

gpointer
gdk_win32_handle_table_lookup (HWND handle)
{
  return gdk_win32_display_handle_table_lookup_ (NULL, handle);
}

/* set up input language or text service change notification for our GdkDisplay */

/* COM implementation for receiving notifications when input language or text service has changed in GDK */
static ULONG STDMETHODCALLTYPE
alpn_sink_addref (ITfActiveLanguageProfileNotifySink *This)
{
  GdkWin32ALPNSink *alpn_sink = (GdkWin32ALPNSink *) This;
  int ref_count = ++alpn_sink->ref_count;

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
alpn_sink_queryinterface (ITfActiveLanguageProfileNotifySink *This,
                          REFIID                              riid,
                          LPVOID                             *ppvObject)
{
  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      ITfActiveLanguageProfileNotifySink_AddRef (This);
      *ppvObject = This;
      return S_OK;
    }

  if (IsEqualGUID (riid, &IID_ITfActiveLanguageProfileNotifySink))
    {
      ITfActiveLanguageProfileNotifySink_AddRef (This);
      *ppvObject = This;
      return S_OK;
    }

  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
alpn_sink_release (ITfActiveLanguageProfileNotifySink *This)
{
  GdkWin32ALPNSink *alpn_sink = (GdkWin32ALPNSink *) This;
  int ref_count = --alpn_sink->ref_count;

  if (ref_count == 0)
    {
      g_free (This);
    }

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
alpn_sink_on_activated (ITfActiveLanguageProfileNotifySink *This,
                        REFCLSID                            clsid,
                        REFGUID                             guidProfile,
                        BOOL                                fActivated)
{
  GdkWin32ALPNSink *alpn_sink = (GdkWin32ALPNSink *) This;

  alpn_sink->input_locale_is_ime = fActivated;
  return S_OK;
}

static const ITfActiveLanguageProfileNotifySinkVtbl alpn_sink_vtbl = {
  alpn_sink_queryinterface,
  alpn_sink_addref,
  alpn_sink_release,
  alpn_sink_on_activated,
};

void
gdk_win32_display_lang_notification_init (GdkWin32Display *display)
{
  HRESULT hr;
  ITfThreadMgr *itf_threadmgr;

  if (display->input_locale_items == NULL)
    display->input_locale_items = g_new0 (GdkWin32InputLocaleItems, 1);

  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  if (display->input_locale_items->notification_sink != NULL)
    return;

  hr = CoCreateInstance (&CLSID_TF_ThreadMgr,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         &IID_ITfThreadMgr,
                         (LPVOID *) &itf_threadmgr);

  if (!SUCCEEDED (hr))
    return;

  hr = ITfThreadMgr_QueryInterface (itf_threadmgr, &IID_ITfSource, (VOID **) &display->input_locale_items->itf_source);
  ITfThreadMgr_Release (itf_threadmgr);

  if (!SUCCEEDED (hr))
    return;

  display->input_locale_items->notification_sink = g_new0 (GdkWin32ALPNSink, 1);
  display->input_locale_items->notification_sink->itf_alpn_sink.lpVtbl = &alpn_sink_vtbl;
  display->input_locale_items->notification_sink->ref_count = 0;
  ITfActiveLanguageProfileNotifySink_AddRef (&display->input_locale_items->notification_sink->itf_alpn_sink);

  hr = ITfSource_AdviseSink (display->input_locale_items->itf_source,
                             &IID_ITfActiveLanguageProfileNotifySink,
                             (IUnknown *) display->input_locale_items->notification_sink,
                             &display->input_locale_items->actlangchangenotify_id);

  if (!SUCCEEDED (hr))
    {
      ITfActiveLanguageProfileNotifySink_Release (&display->input_locale_items->notification_sink->itf_alpn_sink);
      display->input_locale_items->notification_sink = NULL;
      ITfSource_Release (display->input_locale_items->itf_source);
      display->input_locale_items->itf_source = NULL;
    }
}

void
gdk_win32_display_lang_notification_exit (GdkWin32Display *display)
{
  if (display->input_locale_items->notification_sink != NULL && display->input_locale_items->itf_source != NULL)
    {
      ITfSource_UnadviseSink (display->input_locale_items->itf_source, display->input_locale_items->actlangchangenotify_id);
      ITfSource_Release (display->input_locale_items->itf_source);
      ITfActiveLanguageProfileNotifySink_Release (&display->input_locale_items->notification_sink->itf_alpn_sink);
    }

  CoUninitialize ();

  g_clear_pointer (&display->input_locale_items->default_keymap, g_object_unref);
  g_free (display->input_locale_items->notification_sink);
  g_free (display->input_locale_items);
}

void
gdk_win32_display_set_input_locale (GdkWin32Display *display,
                                    HKL              input_locale)
{
  g_return_if_fail (GDK_IS_WIN32_DISPLAY (display));
  display->input_locale_items->input_locale = input_locale;
}

gboolean
gdk_win32_display_input_locale_is_ime (GdkWin32Display *display)
{
  g_return_val_if_fail (GDK_IS_WIN32_DISPLAY (display), FALSE);

  return display->input_locale_items->notification_sink->input_locale_is_ime;
}

GdkKeymap *
gdk_win32_display_get_default_keymap (GdkWin32Display *display)
{
  g_return_val_if_fail (GDK_IS_WIN32_DISPLAY (display), NULL);

  if (display->input_locale_items->default_keymap == NULL)
    {
      display->input_locale_items->default_keymap =
        g_object_new (GDK_TYPE_WIN32_KEYMAP,
                      "display", display,
                      NULL);
    }

  return display->input_locale_items->default_keymap;
}

void
gdk_win32_display_increment_keymap_serial (GdkWin32Display *display)
{
  g_return_if_fail (GDK_IS_WIN32_DISPLAY (display));

  display->input_locale_items->keymap_serial++;
}

guint
gdk_win32_display_get_keymap_serial (GdkWin32Display *display)
{
  g_return_val_if_fail (GDK_IS_WIN32_DISPLAY (display), 0);

  return display->input_locale_items->keymap_serial;
}

static char *
_get_system_font_name (HDC hdc)
{
  NONCLIENTMETRICSW ncm;
  PangoFontDescription *font_desc;
  char *result, *font_desc_string;
  int logpixelsy;
  int font_size;

  ncm.cbSize = sizeof(NONCLIENTMETRICSW);
  if (!SystemParametersInfoW (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
    return NULL;

  logpixelsy = GetDeviceCaps (hdc, LOGPIXELSY);
  font_desc = pango_win32_font_description_from_logfontw (&ncm.lfMessageFont);
  font_desc_string = pango_font_description_to_string (font_desc);
  pango_font_description_free (font_desc);

  /* https://docs.microsoft.com/en-us/windows/desktop/api/wingdi/ns-wingdi-taglogfonta */
  font_size = -MulDiv (ncm.lfMessageFont.lfHeight, 72, logpixelsy);
  result = g_strdup_printf ("%s %d", font_desc_string, font_size);
  g_free (font_desc_string);

  return result;
}

gboolean
gdk_win32_display_get_setting (GdkDisplay  *display,
                               const char *name,
                               GValue      *value)
{
  if (gdk_display_get_debug_flags (display) & GDK_DEBUG_DEFAULT_SETTINGS)
    return FALSE;

  if (strcmp ("gtk-alternative-button-order", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-sort-arrows", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-cursor-blink", name) == 0)
    {
      gboolean blinks = (GetCaretBlinkTime () != INFINITE);
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %s\n", name, blinks ? "TRUE" : "FALSE"));
      g_value_set_boolean (value, blinks);
      return TRUE;
    }
  else if (strcmp ("gtk-cursor-theme-size", name) == 0)
    {
      int cursor_size = GetSystemMetrics (SM_CXCURSOR);
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, cursor_size));
      g_value_set_int (value, cursor_size);
      return TRUE;
    }
  else if (strcmp ("gtk-dnd-drag-threshold", name) == 0)
    {
      int i = MAX(GetSystemMetrics (SM_CXDRAG), GetSystemMetrics (SM_CYDRAG));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-distance", name) == 0)
    {
      int i = MAX(GetSystemMetrics (SM_CXDOUBLECLK), GetSystemMetrics (SM_CYDOUBLECLK));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-time", name) == 0)
    {
      int i = GetDoubleClickTime ();
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-font-name", name) == 0)
    {
      char *font_name = NULL;
      HDC hdc = NULL;

      if ((hdc = GetDC (HWND_DESKTOP)) != NULL)
        {
          font_name = _get_system_font_name (hdc);
          ReleaseDC (HWND_DESKTOP, hdc);
          hdc = NULL;
        }

      if (font_name)
        {
          GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, font_name));
          g_value_take_string (value, font_name);
          return TRUE;
        }
      else
        {
          g_warning ("gdk_win32_get_setting: Detecting the system font failed");
          return FALSE;
        }
    }
  else if (strcmp ("gtk-hint-font-metrics", name) == 0)
    {
      gboolean hint_font_metrics = TRUE;
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name,
                             hint_font_metrics ? "TRUE" : "FALSE"));
      g_value_set_boolean (value, hint_font_metrics);
      return TRUE;
    }
  else if (strcmp ("gtk-im-module", name) == 0)
    {
      const char *im_module = GDK_WIN32_DISPLAY (display)->input_locale_items->notification_sink->input_locale_is_ime ? "ime" : "";
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, im_module));
      g_value_set_static_string (value, im_module);
      return TRUE;
    }
  else if (strcmp ("gtk-overlay-scrolling", name) == 0)
    {
      DWORD val = 0;
      DWORD sz = sizeof (val);
      LSTATUS ret = 0;

      ret = RegGetValueW (HKEY_CURRENT_USER, L"Control Panel\\Accessibility", L"DynamicScrollbars", RRF_RT_DWORD, NULL, &val, &sz);
      if (ret == ERROR_SUCCESS)
        {
          gboolean overlay_scrolling = val != 0;
          GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name,
                                 overlay_scrolling ? "TRUE" : "FALSE"));
          g_value_set_boolean (value, overlay_scrolling);
          return TRUE;
        }
    }
  else if (strcmp ("gtk-shell-shows-desktop", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-split-cursor", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : FALSE\n", name));
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else if (strcmp ("gtk-theme-name", name) == 0)
    {
      HIGHCONTRASTW hc;
      memset (&hc, 0, sizeof (hc));
      hc.cbSize = sizeof (hc);
      if (API_CALL (SystemParametersInfoW, (SPI_GETHIGHCONTRAST, sizeof (hc), &hc, 0)))
        {
          if (hc.dwFlags & HCF_HIGHCONTRASTON)
            {
              const char *theme_name = "Default-hc";

              GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %s\n", name, theme_name));
              g_value_set_string (value, theme_name);
              return TRUE;
            }
        }
    }
  else if (strcmp ("gtk-xft-antialias", name) == 0)
    {
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : 1\n", name));
      g_value_set_int (value, 1);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-dpi", name) == 0)
    {
      GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);

      if (display_win32->dpi_aware_type == PROCESS_SYSTEM_DPI_AWARE &&
          !display_win32->has_fixed_scale)
        {
          HDC hdc = GetDC (NULL);

          if (hdc != NULL)
            {
              int dpi = GetDeviceCaps (GetDC (NULL), LOGPIXELSX);
              ReleaseDC (NULL, hdc);

              if (dpi >= 96)
                {
                  int xft_dpi = 1024 * dpi / display_win32->surface_scale;
                  GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %d\n", name, xft_dpi));
                  g_value_set_int (value, xft_dpi);
                  return TRUE;
                }
            }
        }
    }
  else if (strcmp ("gtk-xft-hinting", name) == 0)
    {
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : 1\n", name));
      g_value_set_int (value, 1);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-hintstyle", name) == 0)
    {
      g_value_set_static_string (value, "hintfull");
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }
  else if (strcmp ("gtk-xft-rgba", name) == 0)
    {
      unsigned int orientation = 0;
      if (SystemParametersInfoW (SPI_GETFONTSMOOTHINGORIENTATION, 0, &orientation, 0))
        {
          if (orientation == FE_FONTSMOOTHINGORIENTATIONRGB)
            g_value_set_static_string (value, "rgb");
          else if (orientation == FE_FONTSMOOTHINGORIENTATIONBGR)
            g_value_set_static_string (value, "bgr");
          else
            g_value_set_static_string (value, "none");
        }
      else
        g_value_set_static_string (value, "none");

      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }

  return FALSE;
}

