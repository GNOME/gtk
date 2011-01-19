/* MS-Windows Engine (aka GTK-Wimp)
 *
 * Copyright (C) 2003, 2004 Raymond Penners <raymond@dotsphinx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * Includes code adapted from redmond95 by Owen Taylor, and
 * gtk-nativewin by Evan Martin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Useful resources:
 *
 *  http://lxr.mozilla.org/seamonkey/source/widget/src/windows/nsNativeThemeWin.cpp
 *  http://lxr.mozilla.org/seamonkey/source/widget/src/windows/nsLookAndFeel.cpp
 *  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/userex/functions/drawthemebackground.asp
 *  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/pantdraw_4b3g.asp
 */

/* Include first, else we get redefinition warnings about STRICT */
#include "pango/pangowin32.h"

#include "msw_style.h"
#include "xp_theme.h"

#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "gtk/gtk.h"
#include "gtk/gtk.h"
#ifndef GTK_COMPILATION
#define GTK_COMPILATION
#endif
#include "gtk/gtkmenushellprivate.h"

#ifdef BUILDING_STANDALONE
#include "gdk/gdkwin32.h"
#else
#include "gdk/win32/gdkwin32.h"
#endif


/* Default values, not normally used
 */
static const GtkRequisition default_option_indicator_size = { 9, 8 };
static const GtkBorder default_option_indicator_spacing = { 7, 5, 2, 2 };

static GtkStyleClass *parent_class;
static HBRUSH g_dither_brush = NULL;

static HPEN g_light_pen = NULL;
static HPEN g_dark_pen = NULL;

typedef enum
{
  CHECK_AA,
  CHECK_BASE,
  CHECK_BLACK,
  CHECK_DARK,
  CHECK_LIGHT,
  CHECK_MID,
  CHECK_TEXT,
  CHECK_INCONSISTENT,
  RADIO_BASE,
  RADIO_BLACK,
  RADIO_DARK,
  RADIO_LIGHT,
  RADIO_MID,
  RADIO_TEXT
} Part;

#define PART_SIZE 13

static const unsigned char check_aa_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char check_base_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char check_black_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0xfe, 0x0f, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char check_dark_bits[] = {
  0xff, 0x1f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00
};
static const unsigned char check_light_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0xfe, 0x1f, 0x00, 0x00
};
static const unsigned char check_mid_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0xfc, 0x0f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char check_text_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x03, 0x00, 0x00, 0x88, 0x03, 0x00, 0x00,
  0xd8, 0x01, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00,
  0x70, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char check_inconsistent_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xf0, 0x03, 0x00, 0x00, 0xf0, 0x03, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char radio_base_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xf0, 0x01, 0x00, 0x00, 0xf8, 0x03, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xf8, 0x03, 0x00, 0x00,
  0xf0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char radio_black_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00,
  0x0c, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char radio_dark_bits[] = {
  0xf0, 0x01, 0x00, 0x00, 0x0c, 0x06, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char radio_light_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x0c, 0x06, 0x00, 0x00,
  0xf0, 0x01, 0x00, 0x00
};
static const unsigned char radio_mid_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
  0x0c, 0x06, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
static const unsigned char radio_text_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xe0, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00,
  0xf0, 0x01, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00,
  0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

static struct
{
  const unsigned char *bits;
  cairo_surface_t *bmap;
} parts[] = {
  { check_aa_bits, NULL           },
  { check_base_bits, NULL         },
  { check_black_bits, NULL        },
  { check_dark_bits, NULL         },
  { check_light_bits, NULL        },
  { check_mid_bits, NULL          },
  { check_text_bits, NULL         },
  { check_inconsistent_bits, NULL },
  { radio_base_bits, NULL         },
  { radio_black_bits, NULL        },
  { radio_dark_bits, NULL         },
  { radio_light_bits, NULL        },
  { radio_mid_bits, NULL          },
  { radio_text_bits, NULL         }
};

static void
_cairo_draw_line (cairo_t  *cr,
                  GdkColor *color,
                  gint      x1,
                  gint      y1,
                  gint      x2,
                  gint      y2)
{
  cairo_save (cr);

  gdk_cairo_set_source_color (cr, color);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1.0);

  cairo_move_to (cr, x1 + 0.5, y1 + 0.5);
  cairo_line_to (cr, x2 + 0.5, y2 + 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);
}

static void
_cairo_draw_rectangle (cairo_t *cr,
                       GdkColor *color,
                       gboolean filled,
                       gint x,
                       gint y,
                       gint width,
                       gint height)
{
  gdk_cairo_set_source_color (cr, color);

  if (filled)
    {
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }
  else
    {
      cairo_rectangle (cr, x + 0.5, y + 0.5, width, height);
      cairo_stroke (cr);
    }
}

static gboolean
get_system_font (XpThemeClass klazz, XpThemeFont type, LOGFONTW *out_lf)
{
  if (xp_theme_get_system_font (klazz, type, out_lf))
    {
      return TRUE;
    }
  else
    {
      /* Use wide char versions here, as the theming functions only support
       * wide chars versions of the structures. */
      NONCLIENTMETRICSW ncm;

      ncm.cbSize = sizeof (NONCLIENTMETRICSW);

      if (SystemParametersInfoW (SPI_GETNONCLIENTMETRICS,
				sizeof (NONCLIENTMETRICSW), &ncm, 0))
	{
	  if (type == XP_THEME_FONT_CAPTION)
	    *out_lf = ncm.lfCaptionFont;
	  else if (type == XP_THEME_FONT_MENU)
	    *out_lf = ncm.lfMenuFont;
	  else if (type == XP_THEME_FONT_STATUS)
	    *out_lf = ncm.lfStatusFont;
	  else
	    *out_lf = ncm.lfMessageFont;

	  return TRUE;
	}
    }

  return FALSE;
}

static char *
sys_font_to_pango_font (XpThemeClass klazz, XpThemeFont type, char *buf,
			size_t bufsiz)
{
  LOGFONTW lf;

  if (get_system_font (klazz, type, &lf))
    {
      PangoFontDescription *desc = NULL;
      int pt_size;
      const char *font;

      desc = pango_win32_font_description_from_logfontw (&lf);
      if (!desc)
	return NULL;

      font = pango_font_description_to_string (desc);
      pt_size = pango_font_description_get_size (desc);

      if (!(font && *font))
	{
	  pango_font_description_free (desc);
	  return NULL;
	}

      if (pt_size == 0)
	{
	  HDC hDC;
	  HWND hwnd;

	  hwnd = GetDesktopWindow ();
	  hDC = GetDC (hwnd);

	  if (hDC)
	    pt_size = -MulDiv (lf.lfHeight, 72, GetDeviceCaps (hDC, LOGPIXELSY));
	  else
	    pt_size = 10;

	  if (hDC)
	    ReleaseDC (hwnd, hDC);

	  g_snprintf (buf, bufsiz, "%s %d", font, pt_size);
	}
      else
	{
	  g_snprintf (buf, bufsiz, "%s", font);
	}

      if (desc)
	pango_font_description_free (desc);

      return buf;
    }

  return NULL;
}

/* missing from ms's header files */
#ifndef SPI_GETMENUSHOWDELAY
#define SPI_GETMENUSHOWDELAY 106
#endif

/* I don't know the proper XP theme class for things like
   HIGHLIGHTTEXT, so we'll just define it to be "BUTTON"
   for now */
#define XP_THEME_CLASS_TEXT XP_THEME_CLASS_BUTTON

#define WIN95_VERSION   0x400
#define WIN2K_VERSION   0x500
#define WINXP_VERSION   0x501
#define WIN2K3_VERSION  0x502
#define VISTA_VERSION   0x600

static gint32
get_windows_version ()
{
  static gint32 version = 0;
  static gboolean have_version = FALSE;

  if (!have_version)
    {
      OSVERSIONINFOEX osvi;
      have_version = TRUE;

      ZeroMemory (&osvi, sizeof (OSVERSIONINFOEX));
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);

      GetVersionEx((OSVERSIONINFO*) &osvi);

      version = (osvi.dwMajorVersion & 0xff) << 8 | (osvi.dwMinorVersion & 0xff);
    }

  return version;
}

static void
setup_menu_settings (GtkSettings *settings)
{
  int menu_delay;
  GObjectClass *klazz = G_OBJECT_GET_CLASS (G_OBJECT (settings));

  if (get_windows_version () > WIN95_VERSION)
    {
      if (SystemParametersInfo (SPI_GETMENUSHOWDELAY, 0, &menu_delay, 0))
	{
	  if (klazz)
	    {
	      if (g_object_class_find_property
		  (klazz, "gtk-menu-bar-popup-delay"))
		{
		  g_object_set (settings,
				"gtk-menu-bar-popup-delay", 0, NULL);
		}
	      if (g_object_class_find_property
		  (klazz, "gtk-menu-popup-delay"))
		{
		  g_object_set (settings,
				"gtk-menu-popup-delay", menu_delay, NULL);
		}
	      if (g_object_class_find_property
		  (klazz, "gtk-menu-popdown-delay"))
		{
		  g_object_set (settings,
				"gtk-menu-popdown-delay", menu_delay, NULL);
		}
	    }
	}
    }
}

void
msw_style_setup_system_settings (void)
{
  GtkSettings *settings;
  int cursor_blink_time;

  settings = gtk_settings_get_default ();
  if (!settings)
    return;

  cursor_blink_time = GetCaretBlinkTime ();
  g_object_set (settings, "gtk-cursor-blink", cursor_blink_time > 0, NULL);

  if (cursor_blink_time > 0)
    {
      g_object_set (settings, "gtk-cursor-blink-time",
		    2 * cursor_blink_time, NULL);
    }

  g_object_set (settings, "gtk-double-click-distance",
		GetSystemMetrics (SM_CXDOUBLECLK), NULL);
  g_object_set (settings, "gtk-double-click-time", GetDoubleClickTime (),
		NULL);
  g_object_set (settings, "gtk-dnd-drag-threshold",
		GetSystemMetrics (SM_CXDRAG), NULL);

  setup_menu_settings (settings);

  /*
     http://library.gnome.org/devel/gtk/stable/GtkSettings.html
     http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/systemparametersinfo.asp
     http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/getsystemmetrics.asp */
}

static void
setup_system_font (GtkStyle *style)
{
  char buf[256], *font;		/* It's okay, lfFaceName is smaller than 32
				   chars */

  if ((font = sys_font_to_pango_font (XP_THEME_CLASS_TEXT,
				      XP_THEME_FONT_MESSAGE,
				      buf, sizeof (buf))) != NULL)
    {
      if (style->font_desc)
	{
	  pango_font_description_free (style->font_desc);
	}

      style->font_desc = pango_font_description_from_string (font);
    }
}

static void
sys_color_to_gtk_color (XpThemeClass klazz, int id, GdkColor * pcolor)
{
  DWORD color;

  if (!xp_theme_get_system_color (klazz, id, &color))
    color = GetSysColor (id);

  pcolor->pixel = color;
  pcolor->red = (GetRValue (color) << 8) | GetRValue (color);
  pcolor->green = (GetGValue (color) << 8) | GetGValue (color);
  pcolor->blue = (GetBValue (color) << 8) | GetBValue (color);
}

static int
get_system_metric (XpThemeClass klazz, int id)
{
  int rval;

  if (!xp_theme_get_system_metric (klazz, id, &rval))
    rval = GetSystemMetrics (id);

  return rval;
}

static void
setup_msw_rc_style (void)
{
  char buf[1024], font_buf[256], *font_ptr;
  char menu_bar_prelight_str[128];

  GdkColor menu_color;
  GdkColor menu_text_color;
  GdkColor tooltip_back;
  GdkColor tooltip_fore;
  GdkColor btn_fore;
  GdkColor btn_face;
  GdkColor progress_back;

  GdkColor fg_prelight;
  GdkColor bg_prelight;
  GdkColor base_prelight;
  GdkColor text_prelight;

  /* Prelight */
  sys_color_to_gtk_color (get_windows_version () >= VISTA_VERSION ? XP_THEME_CLASS_MENU : XP_THEME_CLASS_TEXT,
			  get_windows_version () >= VISTA_VERSION ? COLOR_MENUTEXT : COLOR_HIGHLIGHTTEXT,
			  &fg_prelight);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHT, &bg_prelight);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHT,
			  &base_prelight);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHTTEXT,
			  &text_prelight);

  sys_color_to_gtk_color (XP_THEME_CLASS_MENU, COLOR_MENUTEXT,
			  &menu_text_color);
  sys_color_to_gtk_color (XP_THEME_CLASS_MENU, COLOR_MENU, &menu_color);

  /* tooltips */
  sys_color_to_gtk_color (XP_THEME_CLASS_TOOLTIP, COLOR_INFOTEXT,
			  &tooltip_fore);
  sys_color_to_gtk_color (XP_THEME_CLASS_TOOLTIP, COLOR_INFOBK,
			  &tooltip_back);

  /* text on push buttons. TODO: button shadows, backgrounds, and
     highlights */
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNTEXT, &btn_fore);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE, &btn_face);

  /* progress bar background color */
  sys_color_to_gtk_color (XP_THEME_CLASS_PROGRESS, COLOR_HIGHLIGHT,
			  &progress_back);

  /* Enable coloring for menus. */
  font_ptr =
    sys_font_to_pango_font (XP_THEME_CLASS_MENU, XP_THEME_FONT_MENU,
			    font_buf, sizeof (font_buf));
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-menu\" = \"msw-default\"\n" "{\n"
	      "GtkMenuItem::toggle-spacing = 8\n"
	      "fg[PRELIGHT] = { %d, %d, %d }\n"
	      "bg[PRELIGHT] = { %d, %d, %d }\n"
	      "text[PRELIGHT] = { %d, %d, %d }\n"
	      "base[PRELIGHT] = { %d, %d, %d }\n"
	      "fg[NORMAL] = { %d, %d, %d }\n"
	      "bg[NORMAL] = { %d, %d, %d }\n" "%s = \"%s\"\n"
	      "}widget_class \"*MenuItem*\" style \"msw-menu\"\n"
	      "widget_class \"*GtkMenu\" style \"msw-menu\"\n"
	      "widget_class \"*GtkMenuShell*\" style \"msw-menu\"\n",
	      fg_prelight.red, fg_prelight.green, fg_prelight.blue,
	      bg_prelight.red, bg_prelight.green, bg_prelight.blue,
	      text_prelight.red, text_prelight.green, text_prelight.blue,
	      base_prelight.red, base_prelight.green, base_prelight.blue,
	      menu_text_color.red, menu_text_color.green,
	      menu_text_color.blue, menu_color.red, menu_color.green,
	      menu_color.blue, (font_ptr ? "font_name" : "#"),
	      (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string (buf);

  if (xp_theme_is_active ())
    {
      *menu_bar_prelight_str = '\0';
    }
  else
    {
      g_snprintf (menu_bar_prelight_str, sizeof (menu_bar_prelight_str),
		  "fg[PRELIGHT] = { %d, %d, %d }\n",
		  menu_text_color.red, menu_text_color.green,
		  menu_text_color.blue);
    }

  /* Enable coloring for menu bars. */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-menu-bar\" = \"msw-menu\"\n"
	      "{\n"
	      "bg[NORMAL] = { %d, %d, %d }\n"
	      "%s" "GtkMenuBar::shadow-type = %d\n"
	      /*
	         FIXME: This should be enabled once gtk+ support
	         GtkMenuBar::prelight-item style property.
	       */
	      /* "GtkMenuBar::prelight-item = 1\n" */
	      "}widget_class \"*MenuBar*\" style \"msw-menu-bar\"\n",
	      btn_face.red, btn_face.green, btn_face.blue,
	      menu_bar_prelight_str, xp_theme_is_active ()? 0 : 2);
  gtk_rc_parse_string (buf);

  g_snprintf (buf, sizeof (buf),
	      "style \"msw-toolbar\" = \"msw-default\"\n"
	      "{\n"
	      "GtkHandleBox::shadow-type = %s\n"
	      "GtkToolbar::shadow-type = %s\n"
	      "}widget_class \"*HandleBox*\" style \"msw-toolbar\"\n",
	      "etched-in", "etched-in");
  gtk_rc_parse_string (buf);

  /* enable tooltip fonts */
  font_ptr = sys_font_to_pango_font (XP_THEME_CLASS_STATUS, XP_THEME_FONT_STATUS,
				     font_buf, sizeof (font_buf));
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-tooltips-caption\" = \"msw-default\"\n"
	      "{fg[NORMAL] = { %d, %d, %d }\n" "%s = \"%s\"\n"
	      "}widget \"gtk-tooltips.GtkLabel\" style \"msw-tooltips-caption\"\n"
	      "widget \"gtk-tooltip.GtkLabel\" style \"msw-tooltips-caption\"\n",
	      tooltip_fore.red, tooltip_fore.green, tooltip_fore.blue,
	      (font_ptr ? "font_name" : "#"),
	      (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string (buf);

  g_snprintf (buf, sizeof (buf),
	      "style \"msw-tooltips\" = \"msw-default\"\n"
	      "{bg[NORMAL] = { %d, %d, %d }\n"
	      "}widget \"gtk-tooltips*\" style \"msw-tooltips\"\n"
	      "widget \"gtk-tooltip*\" style \"msw-tooltips\"\n",
	      tooltip_back.red, tooltip_back.green, tooltip_back.blue);
  gtk_rc_parse_string (buf);

  /* enable font theming for status bars */
  font_ptr = sys_font_to_pango_font (XP_THEME_CLASS_STATUS, XP_THEME_FONT_STATUS,
				     font_buf, sizeof (font_buf));
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-status\" = \"msw-default\"\n" "{%s = \"%s\"\n"
	      "bg[NORMAL] = { %d, %d, %d }\n"
	      "}widget_class \"*Status*\" style \"msw-status\"\n",
	      (font_ptr ? "font_name" : "#"),
	      (font_ptr ? font_ptr : " font name should go here"),
	      btn_face.red, btn_face.green, btn_face.blue);
  gtk_rc_parse_string (buf);

  /* enable coloring for text on buttons TODO: use GetThemeMetric for the
     border and outside border */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-button\" = \"msw-default\"\n"
	      "{\n"
	      "bg[NORMAL] = { %d, %d, %d }\n"
	      "bg[PRELIGHT] = { %d, %d, %d }\n"
	      "bg[INSENSITIVE] = { %d, %d, %d }\n"
	      "fg[PRELIGHT] = { %d, %d, %d }\n"
	      "GtkButton::default-border = { 0, 0, 0, 0 }\n"
	      "GtkButton::default-outside-border = { 0, 0, 0, 0 }\n"
	      "GtkButton::child-displacement-x = 1\n"
	      "GtkButton::child-displacement-y = 1\n"
	      "GtkButton::focus-padding = %d\n"
	      "}widget_class \"*Button*\" style \"msw-button\"\n",
	      btn_face.red, btn_face.green, btn_face.blue,
	      btn_face.red, btn_face.green, btn_face.blue,
	      btn_face.red, btn_face.green, btn_face.blue,
	      btn_fore.red, btn_fore.green, btn_fore.blue,
	      xp_theme_is_active ()? 1 : 2);
  gtk_rc_parse_string (buf);

  /* enable coloring for progress bars */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-progress\" = \"msw-default\"\n"
	      "{bg[PRELIGHT] = { %d, %d, %d }\n"
	      "bg[NORMAL] = { %d, %d, %d }\n"
	      "}widget_class \"*Progress*\" style \"msw-progress\"\n",
	      progress_back.red,
	      progress_back.green,
	      progress_back.blue,
	      btn_face.red, btn_face.green, btn_face.blue);
  gtk_rc_parse_string (buf);

  /* scrollbar thumb width and height */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-vscrollbar\" = \"msw-default\"\n"
	      "{GtkRange::slider-width = %d\n"
	      "GtkRange::stepper-size = %d\n"
	      "GtkRange::stepper-spacing = 0\n"
	      "GtkRange::trough_border = 0\n"
	      "GtkScale::slider-length = %d\n"
	      "GtkScrollbar::min-slider-length = 8\n"
	      "}widget_class \"*VScrollbar*\" style \"msw-vscrollbar\"\n"
	      "widget_class \"*VScale*\" style \"msw-vscrollbar\"\n",
	      GetSystemMetrics (SM_CYVTHUMB),
	      get_system_metric (XP_THEME_CLASS_SCROLLBAR, SM_CXVSCROLL), 11);
  gtk_rc_parse_string (buf);

  g_snprintf (buf, sizeof (buf),
	      "style \"msw-hscrollbar\" = \"msw-default\"\n"
	      "{GtkRange::slider-width = %d\n"
	      "GtkRange::stepper-size = %d\n"
	      "GtkRange::stepper-spacing = 0\n"
	      "GtkRange::trough_border = 0\n"
	      "GtkScale::slider-length = %d\n"
	      "GtkScrollbar::min-slider-length = 8\n"
	      "}widget_class \"*HScrollbar*\" style \"msw-hscrollbar\"\n"
	      "widget_class \"*HScale*\" style \"msw-hscrollbar\"\n",
	      GetSystemMetrics (SM_CXHTHUMB),
	      get_system_metric (XP_THEME_CLASS_SCROLLBAR, SM_CYHSCROLL), 11);
  gtk_rc_parse_string (buf);

  gtk_rc_parse_string ("style \"msw-scrolled-window\" = \"msw-default\"\n"
		       "{GtkScrolledWindow::scrollbars-within-bevel = 1}\n"
		       "class \"GtkScrolledWindow\" style \"msw-scrolled-window\"\n");

  /* radio/check button sizes */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-checkbutton\" = \"msw-button\"\n"
	      "{GtkCheckButton::indicator-size = 13\n"
	      "}widget_class \"*CheckButton*\" style \"msw-checkbutton\"\n"
	      "widget_class \"*RadioButton*\" style \"msw-checkbutton\"\n");
  gtk_rc_parse_string (buf);

  /* size of combo box toggle button */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-combobox-button\" = \"msw-default\"\n"
	      "{\n"
	      "xthickness = 0\n"
	      "ythickness = 0\n"
	      "GtkButton::default-border = { 0, 0, 0, 0 }\n"
	      "GtkButton::default-outside-border = { 0, 0, 0, 0 }\n"
	      "GtkButton::child-displacement-x = 0\n"
	      "GtkButton::child-displacement-y = 0\n"
	      "GtkWidget::focus-padding = 0\n"
	      "GtkWidget::focus-line-width = 0\n"
	      "}\n"
	      "widget_class \"*ComboBox*ToggleButton*\" style \"msw-combobox-button\"\n");
  gtk_rc_parse_string (buf);

  g_snprintf (buf, sizeof (buf),
	      "style \"msw-combobox\" = \"msw-default\"\n"
	      "{\n"
	      "GtkComboBox::shadow-type = in\n"
	      "xthickness = %d\n"
	      "ythickness = %d\n"
	      "}\n"
	      "class \"GtkComboBox\" style \"msw-combobox\"\n",
        xp_theme_is_active()? 1 : GetSystemMetrics (SM_CXEDGE),
        xp_theme_is_active()? 1 : GetSystemMetrics (SM_CYEDGE));
  gtk_rc_parse_string (buf);

  /* size of tree view header */
  g_snprintf (buf, sizeof (buf),
	      "style \"msw-header-button\" = \"msw-default\"\n"
	      "{\n"
	      "xthickness = 0\n"
	      "ythickness = 0\n"
	      "GtkWidget::draw-border = {0, 0, 0, 0}\n"
        "GtkButton::default-border = { 0, 0, 0, 0 }\n"
	      "GtkButton::default-outside-border = { 0, 0, 0, 0 }\n"
	      "GtkButton::child-displacement-x = 0\n"
	      "GtkButton::child-displacement-y = 0\n"
	      "GtkWidget::focus-padding = 0\n"
	      "GtkWidget::focus-line-width = 0\n"
	      "}\n"
	      "widget_class \"*TreeView*Button*\" style \"msw-header-button\"\n");
  gtk_rc_parse_string (buf);

  /* FIXME: This should be enabled once gtk+ support GtkNotebok::prelight-tab */
  /* enable prelight tab of GtkNotebook */
  /*
     g_snprintf (buf, sizeof (buf),
     "style \"msw-notebook\" = \"msw-default\"\n"
     "{GtkNotebook::prelight-tab=1\n"
     "}widget_class \"*Notebook*\" style \"msw-notebook\"\n");
     gtk_rc_parse_string (buf);
   */

  /* FIXME: This should be enabled once gtk+ support GtkTreeView::full-row-focus */
  /*
     g_snprintf (buf, sizeof (buf),
     "style \"msw-treeview\" = \"msw-default\"\n"
     "{GtkTreeView::full-row-focus=0\n"
     "}widget_class \"*TreeView*\" style \"msw-treeview\"\n");
     gtk_rc_parse_string (buf);
   */
}

static void
setup_system_styles (GtkStyle *style)
{
  int i;

  /* Default background */
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE,
			  &style->bg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHT,
			  &style->bg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE,
			  &style->bg[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE,
			  &style->bg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE,
			  &style->bg[GTK_STATE_PRELIGHT]);

  /* Default base */
  sys_color_to_gtk_color (XP_THEME_CLASS_WINDOW, COLOR_WINDOW,
			  &style->base[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHT,
			  &style->base[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE,
			  &style->base[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNFACE,
			  &style->base[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_WINDOW, COLOR_WINDOW,
			  &style->base[GTK_STATE_PRELIGHT]);

  /* Default text */
  sys_color_to_gtk_color (XP_THEME_CLASS_WINDOW, COLOR_WINDOWTEXT,
			  &style->text[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHTTEXT,
			  &style->text[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_GRAYTEXT,
			  &style->text[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNTEXT,
			  &style->text[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_WINDOW, COLOR_WINDOWTEXT,
			  &style->text[GTK_STATE_PRELIGHT]);

  /* Default foreground */
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNTEXT,
			  &style->fg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHTTEXT,
			  &style->fg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color (XP_THEME_CLASS_TEXT, COLOR_GRAYTEXT,
			  &style->fg[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_BTNTEXT,
        		  &style->bg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color (XP_THEME_CLASS_WINDOW, COLOR_WINDOWTEXT,
			  &style->fg[GTK_STATE_PRELIGHT]);

  for (i = 0; i < 5; i++)
    {
      sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_3DSHADOW,
			      &style->dark[i]);
      sys_color_to_gtk_color (XP_THEME_CLASS_BUTTON, COLOR_3DHILIGHT,
			      &style->light[i]);

      style->mid[i].red = (style->light[i].red + style->dark[i].red) / 2;
      style->mid[i].green =
	(style->light[i].green + style->dark[i].green) / 2;
      style->mid[i].blue = (style->light[i].blue + style->dark[i].blue) / 2;

      style->text_aa[i].red = (style->text[i].red + style->base[i].red) / 2;
      style->text_aa[i].green =
	(style->text[i].green + style->base[i].green) / 2;
      style->text_aa[i].blue =
	(style->text[i].blue + style->base[i].blue) / 2;
    }
}

static XpThemeElement
map_gtk_progress_bar_to_xp (GtkProgressBar *progress_bar, gboolean trough)
{
  XpThemeElement ret;

  switch (gtk_orientable_get_orientation (GTK_ORIENTABLE (progress_bar)))
    {
    case GTK_ORIENTATION_HORIZONTAL:
      ret = trough
	? XP_THEME_ELEMENT_PROGRESS_TROUGH_H
	: XP_THEME_ELEMENT_PROGRESS_BAR_H;
      break;

    default:
      ret = trough
	? XP_THEME_ELEMENT_PROGRESS_TROUGH_V
	: XP_THEME_ELEMENT_PROGRESS_BAR_V;
      break;
    }

  return ret;
}

static gboolean
is_combo_box_child (GtkWidget *w)
{
  GtkWidget *tmp;

  if (w == NULL)
    return FALSE;

  for (tmp = gtk_widget_get_parent (w); tmp; tmp = gtk_widget_get_parent (tmp))
    {
      if (GTK_IS_COMBO_BOX (tmp))
	return TRUE;
    }

  return FALSE;
}

/* This function is not needed anymore */
/* static gboolean
combo_box_draw_arrow (GtkStyle *style,
		      cairo_t *cr,
		      GtkStateType state,
		      GtkWidget *widget)
{
  if (xp_theme_is_active ())
    return TRUE;

  if (widget && GTK_IS_TOGGLE_BUTTON (widget->parent))
    {
      DWORD border;
      RECT rect;
      HDC dc;
      XpDCInfo dc_info;

      dc = get_window_dc (style, cr, state, &dc_info, area->x, area->y, area->width,
			  area->height, &rect);
      border = (GTK_TOGGLE_BUTTON (gtk_widget_get_parent (widget))->
		active ? DFCS_PUSHED | DFCS_FLAT : 0);

      InflateRect (&rect, 1, 1);
      DrawFrameControl (dc, &rect, DFC_SCROLL, DFCS_SCROLLDOWN | border);

      release_window_dc (&dc_info);

      return TRUE;
    }

  return FALSE;
}*/

static void
draw_part (cairo_t *cr,
           GdkColor *gc, gint x, gint y, Part part)
{
  if (!parts[part].bmap)
    {
      parts[part].bmap = cairo_image_surface_create_for_data ((unsigned char *)parts[part].bits,
        					              CAIRO_FORMAT_A1,
        					              PART_SIZE, PART_SIZE, 4);
    }

  gdk_cairo_set_source_color (cr, gc);
  cairo_mask_surface (cr, parts[part].bmap, x, y);
}

static void
draw_check (GtkStyle *style,
	    cairo_t *cr,
	    GtkStateType state,
	    GtkShadowType shadow,
	    GtkWidget *widget,
	    const gchar *detail, gint x, gint y, gint width, gint height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (detail && strcmp (detail, "check") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
          draw_part (cr, &style->black, x, y, CHECK_TEXT);
          draw_part (cr, &style->dark[state], x, y, CHECK_AA);
	}
    }
  else
    {
      XpThemeElement theme_elt = XP_THEME_ELEMENT_CHECKBOX;
      switch (shadow)
	{
	case GTK_SHADOW_ETCHED_IN:
	  theme_elt = XP_THEME_ELEMENT_INCONSISTENT_CHECKBOX;
	  break;

	case GTK_SHADOW_IN:
	  theme_elt = XP_THEME_ELEMENT_PRESSED_CHECKBOX;
	  break;

	default:
	  break;
	}

      if (!xp_theme_draw (cr, theme_elt,
			  style, x, y, width, height, state))
	{
	  if (detail && !strcmp (detail, "cellcheck"))
	    state = GTK_STATE_NORMAL;

          draw_part (cr, &style->black, x, y, CHECK_BLACK);
          draw_part (cr, &style->dark[state], x, y, CHECK_DARK);
          draw_part (cr, &style->mid[state], x, y, CHECK_MID);
          draw_part (cr, &style->light[state], x, y, CHECK_LIGHT);
          draw_part (cr, &style->base[state], x, y, CHECK_BASE);

	  if (shadow == GTK_SHADOW_IN)
	    {
              draw_part (cr, &style->text[state], x, y, CHECK_TEXT);
              draw_part (cr, &style->text_aa[state], x, y, CHECK_AA);
	    }
	  else if (shadow == GTK_SHADOW_ETCHED_IN)
	    {
              draw_part (cr, &style->text[state], x, y, CHECK_INCONSISTENT);
              draw_part (cr, &style->text_aa[state], x, y, CHECK_AA);
	    }
	}
    }
}

static void
draw_expander (GtkStyle *style,
	       cairo_t *cr,
	       GtkStateType state,
	       GtkWidget *widget,
	       const gchar *detail,
	       gint x, gint y, GtkExpanderStyle expander_style)
{
  gint expander_size;
  gint expander_semi_size;
  XpThemeElement xp_expander;

  gtk_widget_style_get (widget, "expander_size", &expander_size, NULL);

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED;
      break;

    default:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED;
      break;
    }

  if ((expander_size % 2) == 0)
    expander_size--;

  if (expander_size > 2)
    expander_size -= 2;

/* FIXME: wtf?
      gdk_cairo_set_source_color (cr, &style->fg[state]);
 */

  expander_semi_size = expander_size / 2;
  x -= expander_semi_size;
  y -= expander_semi_size;

  if (!xp_theme_draw (cr, xp_expander, style,
		      x, y, expander_size, expander_size, state))
    {
      HDC dc;
      RECT rect;
      HPEN pen;
      HGDIOBJ old_pen;
      XpDCInfo dc_info;

      dc = get_window_dc (style, cr, state, &dc_info, x, y, expander_size,
			  expander_size, &rect);
      FrameRect (dc, &rect, GetSysColorBrush (COLOR_GRAYTEXT));
      InflateRect (&rect, -1, -1);
      FillRect (dc, &rect,
		GetSysColorBrush (state ==
				  GTK_STATE_INSENSITIVE ? COLOR_BTNFACE :
				  COLOR_WINDOW));

      InflateRect (&rect, -1, -1);

      pen = CreatePen (PS_SOLID, 1, GetSysColor (COLOR_WINDOWTEXT));
      old_pen = SelectObject (dc, pen);

      MoveToEx (dc, rect.left, rect.top - 2 + expander_semi_size, NULL);
      LineTo (dc, rect.right, rect.top - 2 + expander_semi_size);

      if (expander_style == GTK_EXPANDER_COLLAPSED ||
	  expander_style == GTK_EXPANDER_SEMI_COLLAPSED)
	{
	  MoveToEx (dc, rect.left - 2 + expander_semi_size, rect.top, NULL);
	  LineTo (dc, rect.left - 2 + expander_semi_size, rect.bottom);
	}

      SelectObject (dc, old_pen);
      DeleteObject (pen);
      release_window_dc (&dc_info);
    }
}

static void
draw_option (GtkStyle *style,
	     cairo_t *cr,
	     GtkStateType state,
	     GtkShadowType shadow,
	     GtkWidget *widget,
	     const gchar *detail, gint x, gint y, gint width, gint height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (detail && strcmp (detail, "option") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
          draw_part (cr, &style->fg[state], x, y, RADIO_TEXT);
	}
    }
  else
    {
      if (xp_theme_draw (cr, shadow == GTK_SHADOW_IN
			 ? XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON
			 : XP_THEME_ELEMENT_RADIO_BUTTON,
			 style, x, y, width, height, state))
	{
	}
      else
	{
	  if (detail && !strcmp (detail, "cellradio"))
	    state = GTK_STATE_NORMAL;

          draw_part (cr, &style->black, x, y, RADIO_BLACK);
          draw_part (cr, &style->dark[state], x, y, RADIO_DARK);
          draw_part (cr, &style->mid[state], x, y, RADIO_MID);
          draw_part (cr, &style->light[state], x, y, RADIO_LIGHT);
          draw_part (cr, &style->base[state], x, y, RADIO_BASE);

	  if (shadow == GTK_SHADOW_IN)
            draw_part (cr, &style->text[state], x, y, RADIO_TEXT);
	}
    }
}

static void
draw_varrow (cairo_t *cr,
             GdkColor *gc,
	     GtkShadowType shadow_type,
	     GtkArrowType arrow_type, gint x, gint y, gint width, gint height)
{
  gint steps, extra;
  gint y_start, y_increment;
  gint i;

  width = width + width % 2 - 1;	/* Force odd */
  steps = 1 + width / 2;
  extra = height - steps;

  if (arrow_type == GTK_ARROW_DOWN)
    {
      y_start = y;
      y_increment = 1;
    }
  else
    {
      y_start = y + height - 1;
      y_increment = -1;
    }

  for (i = extra; i < height; i++)
    {
      _cairo_draw_line (cr, gc,
		     x + (i - extra), y_start + i * y_increment,
		     x + width - (i - extra) - 1, y_start + i * y_increment);
    }
}

static void
draw_harrow (cairo_t *cr,
             GdkColor *gc,
	     GtkShadowType shadow_type,
	     GtkArrowType arrow_type, gint x, gint y, gint width, gint height)
{
  gint steps, extra;
  gint x_start, x_increment;
  gint i;

  height = height + height % 2 - 1;	/* Force odd */
  steps = 1 + height / 2;
  extra = width - steps;

  if (arrow_type == GTK_ARROW_RIGHT)
    {
      x_start = x;
      x_increment = 1;
    }
  else
    {
      x_start = x + width - 1;
      x_increment = -1;
    }

  for (i = extra; i < width; i++)
    {
      _cairo_draw_line (cr, gc,
		     x_start + i * x_increment, y + (i - extra),
		     x_start + i * x_increment, y + height - (i - extra) - 1);
    }
}

/* This function makes up for some brokeness in gtkrange.c
 * where we never get the full arrow of the stepper button
 * and the type of button in a single drawing function.
 *
 * It doesn't work correctly when the scrollbar is squished
 * to the point we don't have room for full-sized steppers.
 */
static void
reverse_engineer_stepper_box (GtkWidget *range,
			      GtkArrowType arrow_type,
			      gint *x, gint *y, gint *width, gint *height)
{
  gint slider_width = 14, stepper_size = 14;
  gint box_width;
  gint box_height;

  if (range)
    {
      gtk_widget_style_get (range,
			    "slider_width", &slider_width,
			    "stepper_size", &stepper_size, NULL);
    }

  if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
    {
      box_width = slider_width;
      box_height = stepper_size;
    }
  else
    {
      box_width = stepper_size;
      box_height = slider_width;
    }

  *x = *x - (box_width - *width) / 2;
  *y = *y - (box_height - *height) / 2;
  *width = box_width;
  *height = box_height;
}

static XpThemeElement
to_xp_arrow (GtkArrowType arrow_type)
{
  XpThemeElement xp_arrow;

  switch (arrow_type)
    {
    case GTK_ARROW_UP:
      xp_arrow = XP_THEME_ELEMENT_ARROW_UP;
      break;

    case GTK_ARROW_DOWN:
      xp_arrow = XP_THEME_ELEMENT_ARROW_DOWN;
      break;

    case GTK_ARROW_LEFT:
      xp_arrow = XP_THEME_ELEMENT_ARROW_LEFT;
      break;

    default:
      xp_arrow = XP_THEME_ELEMENT_ARROW_RIGHT;
      break;
    }

  return xp_arrow;
}

static void
draw_arrow (GtkStyle *style,
	    cairo_t *cr,
	    GtkStateType state,
	    GtkShadowType shadow,
	    GtkWidget *widget,
	    const gchar *detail,
	    GtkArrowType arrow_type,
	    gboolean fill, gint x, gint y, gint width, gint height)
{
  const gchar *name;
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  name = gtk_widget_get_name (widget);

  if (GTK_IS_ARROW (widget) && is_combo_box_child (widget) && xp_theme_is_active ())
    return;

  if (detail && strcmp (detail, "spinbutton") == 0)
    {
      if (xp_theme_is_drawable (XP_THEME_ELEMENT_SPIN_BUTTON_UP))
	{
	  return;
	}

      width -= 2;
      --height;
      if (arrow_type == GTK_ARROW_DOWN)
	++y;
      ++x;

      if (state == GTK_STATE_ACTIVE)
	{
	  ++x;
	  ++y;
	}

      draw_varrow (cr, &style->fg[state], shadow,
		   arrow_type, x, y, width, height);

      return;
    }
  else if (detail && (!strcmp (detail, "vscrollbar")
		      || !strcmp (detail, "hscrollbar")))
    {
      gboolean is_disabled = FALSE;
      UINT btn_type = 0;
      GtkScrollbar *scrollbar = GTK_SCROLLBAR (widget);

      gint box_x = x;
      gint box_y = y;
      gint box_width = width;
      gint box_height = height;

      reverse_engineer_stepper_box (widget, arrow_type,
				    &box_x, &box_y, &box_width, &box_height);

      if (gtk_adjustment_get_page_size(gtk_range_get_adjustment(&scrollbar->range)) >=
          (gtk_adjustment_get_upper(gtk_range_get_adjustment(&scrollbar->range)) -
           gtk_adjustment_get_lower(gtk_range_get_adjustment(&scrollbar->range))))
	{
	  is_disabled = TRUE;
	}

      if (xp_theme_draw (cr, to_xp_arrow (arrow_type), style, box_x, box_y,
			 box_width, box_height, state))
	{
	}
      else
	{
	  switch (arrow_type)
	    {
	    case GTK_ARROW_UP:
	      btn_type = DFCS_SCROLLUP;
	      break;

	    case GTK_ARROW_DOWN:
	      btn_type = DFCS_SCROLLDOWN;
	      break;

	    case GTK_ARROW_LEFT:
	      btn_type = DFCS_SCROLLLEFT;
	      break;

	    case GTK_ARROW_RIGHT:
	      btn_type = DFCS_SCROLLRIGHT;
	      break;

	    case GTK_ARROW_NONE:
	      break;
	    }

	  if (state == GTK_STATE_INSENSITIVE)
	    {
	      btn_type |= DFCS_INACTIVE;
	    }

	  if (widget)
	    {
	      dc = get_window_dc (style, cr, state, &dc_info,
				  box_x, box_y, box_width, box_height, &rect);
	      DrawFrameControl (dc, &rect, DFC_SCROLL,
				btn_type | (shadow ==
					    GTK_SHADOW_IN ? (DFCS_PUSHED |
							     DFCS_FLAT) : 0));
	      release_window_dc (&dc_info);
	    }
	}
    }
  else
    {
      /* draw the toolbar chevrons - waiting for GTK 2.4 */
      if (name && !strcmp (name, "gtk-toolbar-arrow"))
	{
	  if (xp_theme_draw
	      (cr, XP_THEME_ELEMENT_REBAR_CHEVRON, style, x, y,
	       width, height, state))
	    {
	      return;
	    }
	}
      /* probably a gtk combo box on a toolbar */
      else if (0		/* gtk_widget_get_parent (widget) && GTK_IS_BUTTON
				   (gtk_widget_get_parent (widget)) */ )
	{
	  GtkAllocation allocation;

	  gtk_widget_get_allocation (widget, &allocation);
	  if (xp_theme_draw
	      (cr, XP_THEME_ELEMENT_COMBOBUTTON, style, x - 3,
	       allocation.y + 1, width + 5,
	       allocation.height - 4, state))
	    {
	      return;
	    }
	}

      if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
	{
	  x += (width - 7) / 2;
	  y += (height - 5) / 2;

          draw_varrow (cr, &style->fg[state], shadow,
		       arrow_type, x, y, 7, 5);
	}
      else
	{
	  x += (width - 5) / 2;
	  y += (height - 7) / 2;

          draw_harrow (cr, &style->fg[state], shadow,
		       arrow_type, x, y, 5, 7);
	}
    }
}

static void
option_menu_get_props (GtkWidget *widget,
		       GtkRequisition *indicator_size,
		       GtkBorder *indicator_spacing)
{
  GtkRequisition *tmp_size = NULL;
  GtkBorder *tmp_spacing = NULL;

  if (widget)
    gtk_widget_style_get (widget,
			  "indicator_size", &tmp_size,
			  "indicator_spacing", &tmp_spacing, NULL);

  if (tmp_size)
    {
      *indicator_size = *tmp_size;
      gtk_requisition_free (tmp_size);
    }
  else
    {
      *indicator_size = default_option_indicator_size;
    }

  if (tmp_spacing)
    {
      *indicator_spacing = *tmp_spacing;
      gtk_border_free (tmp_spacing);
    }
  else
    {
      *indicator_spacing = default_option_indicator_spacing;
    }
}

static gboolean
is_toolbar_child (GtkWidget *wid)
{
  while (wid)
    {
      if (GTK_IS_TOOLBAR (wid) || GTK_IS_HANDLE_BOX (wid))
	return TRUE;
      else
	wid = gtk_widget_get_parent (wid);
    }

  return FALSE;
}

static gboolean
is_menu_tool_button_child (GtkWidget *wid)
{
  while (wid)
    {
      if (GTK_IS_MENU_TOOL_BUTTON (wid))
	return TRUE;
      else
	wid = gtk_widget_get_parent (wid);
    }
  return FALSE;
}

static HPEN
get_light_pen ()
{
  if (!g_light_pen)
    {
      g_light_pen = CreatePen (PS_SOLID | PS_INSIDEFRAME, 1,
			       GetSysColor (COLOR_BTNHIGHLIGHT));
    }

  return g_light_pen;
}

static HPEN
get_dark_pen ()
{
  if (!g_dark_pen)
    {
      g_dark_pen = CreatePen (PS_SOLID | PS_INSIDEFRAME, 1,
			      GetSysColor (COLOR_BTNSHADOW));
    }

  return g_dark_pen;
}

static void
draw_3d_border (HDC hdc, RECT *rc, gboolean sunken)
{
  HPEN pen1, pen2;
  HGDIOBJ old_pen;

  if (sunken)
    {
      pen1 = get_dark_pen ();
      pen2 = get_light_pen ();
    }
  else
    {
      pen1 = get_light_pen ();
      pen2 = get_dark_pen ();
    }

  MoveToEx (hdc, rc->left, rc->bottom - 1, NULL);

  old_pen = SelectObject (hdc, pen1);
  LineTo (hdc, rc->left, rc->top);
  LineTo (hdc, rc->right - 1, rc->top);
  SelectObject (hdc, old_pen);

  old_pen = SelectObject (hdc, pen2);
  LineTo (hdc, rc->right - 1, rc->bottom - 1);
  LineTo (hdc, rc->left, rc->bottom - 1);
  SelectObject (hdc, old_pen);
}

static gboolean
draw_menu_item (cairo_t *cr, GtkWidget *widget, GtkStyle *style,
		gint x, gint y, gint width, gint height,
		GtkStateType state_type)
{
  GtkWidget *parent;
  GtkMenuShell *bar;
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  if (xp_theme_is_active ())
    {
      return (xp_theme_draw (cr, XP_THEME_ELEMENT_MENU_ITEM, style,
                             x, y, width, height, state_type));
    }

  if ((parent = gtk_widget_get_parent (widget))
      && GTK_IS_MENU_BAR (parent) && !xp_theme_is_active ())
    {
      bar = GTK_MENU_SHELL (parent);

      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

      if (state_type == GTK_STATE_PRELIGHT)
	{
        draw_3d_border (dc, &rect, bar->priv->active);
	}

      release_window_dc (&dc_info);

      return TRUE;
    }

  return FALSE;
}

static HBRUSH
get_dither_brush (void)
{
  WORD pattern[8];
  HBITMAP pattern_bmp;
  int i;

  if (g_dither_brush)
    return g_dither_brush;

  for (i = 0; i < 8; i++)
    {
      pattern[i] = (WORD) (0x5555 << (i & 1));
    }

  pattern_bmp = CreateBitmap (8, 8, 1, 1, &pattern);

  if (pattern_bmp)
    {
      g_dither_brush = CreatePatternBrush (pattern_bmp);
      DeleteObject (pattern_bmp);
    }

  return g_dither_brush;
}

static gboolean
draw_tool_button (cairo_t *cr, GtkWidget *widget, GtkStyle *style,
		  gint x, gint y, gint width, gint height,
		  GtkStateType state_type)
{
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;
  gboolean is_toggled = FALSE;

  if (xp_theme_is_active ())
    {
      return (xp_theme_draw (cr, XP_THEME_ELEMENT_TOOLBAR_BUTTON, style,
			     x, y, width, height, state_type));
    }

  if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
	  is_toggled = TRUE;
	}
    }

  if (state_type != GTK_STATE_PRELIGHT
      && state_type != GTK_STATE_ACTIVE && !is_toggled)
    {
      return FALSE;
    }

  dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);
  if (state_type == GTK_STATE_PRELIGHT)
    {
      if (is_toggled)
	{
	  FillRect (dc, &rect, GetSysColorBrush (COLOR_BTNFACE));
	}

      draw_3d_border (dc, &rect, is_toggled);
    }
  else if (state_type == GTK_STATE_ACTIVE)
    {
      if (is_toggled && !is_menu_tool_button_child (gtk_widget_get_parent (widget)))
	{
	  SetTextColor (dc, GetSysColor (COLOR_3DHILIGHT));
	  SetBkColor (dc, GetSysColor (COLOR_BTNFACE));
	  FillRect (dc, &rect, get_dither_brush ());
	}

      draw_3d_border (dc, &rect, TRUE);
    }

  release_window_dc (&dc_info);

  return TRUE;
}

static void
draw_push_button (cairo_t *cr, GtkWidget *widget, GtkStyle *style,
		  gint x, gint y, gint width, gint height,
		  GtkStateType state_type, gboolean is_default)
{
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

  if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      if (state_type == GTK_STATE_PRELIGHT &&
	  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
	  state_type = GTK_STATE_ACTIVE;
	}
    }

  if (state_type == GTK_STATE_ACTIVE)
    {
      if (GTK_IS_TOGGLE_BUTTON (widget))
	{
	  DrawEdge (dc, &rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
	  SetTextColor (dc, GetSysColor (COLOR_3DHILIGHT));
	  SetBkColor (dc, GetSysColor (COLOR_BTNFACE));
	  FillRect (dc, &rect, get_dither_brush ());
	}
      else
	{
	  FrameRect (dc, &rect, GetSysColorBrush (COLOR_WINDOWFRAME));
	  InflateRect (&rect, -1, -1);
	  FrameRect (dc, &rect, GetSysColorBrush (COLOR_BTNSHADOW));
	  InflateRect (&rect, -1, -1);
	  FillRect (dc, &rect, GetSysColorBrush (COLOR_BTNFACE));
	}
    }
  else
    {
      if (is_default || gtk_widget_has_focus (widget))
	{
	  FrameRect (dc, &rect, GetSysColorBrush (COLOR_WINDOWFRAME));
	  InflateRect (&rect, -1, -1);
	}

      DrawFrameControl (dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH);
    }

  release_window_dc (&dc_info);
}

static void
draw_box (GtkStyle *style,
	  cairo_t *cr,
	  GtkStateType state_type,
	  GtkShadowType shadow_type,
	  GtkWidget *widget,
	  const gchar *detail, gint x, gint y, gint width, gint height)
{
  if (is_combo_box_child (widget) && detail && !strcmp (detail, "button"))
    {
      RECT rect;
      XpDCInfo dc_info;
      DWORD border;
      HDC dc;
      int cx;

      border = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? DFCS_PUSHED | DFCS_FLAT : 0);

      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);
      DrawFrameControl (dc, &rect, DFC_SCROLL, DFCS_SCROLLDOWN | border);
      release_window_dc (&dc_info);

      if (xp_theme_is_active ()
	  && xp_theme_draw (cr, XP_THEME_ELEMENT_COMBOBUTTON, style, x, y,
			    width, height, state_type))
	{
      cx = GetSystemMetrics(SM_CXVSCROLL);
      x += width - cx;
      width = cx;


      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width - cx, height, &rect);
      FillRect (dc, &rect, GetSysColorBrush (COLOR_WINDOW));
      release_window_dc (&dc_info);
      return;
	}
    }

  if (detail &&
      (!strcmp (detail, "button") || !strcmp (detail, "buttondefault")))
    {
      if (GTK_IS_TREE_VIEW (gtk_widget_get_parent (widget)))
      {
        if (xp_theme_draw
	      (cr, XP_THEME_ELEMENT_LIST_HEADER, style, x, y,
	       width, height, state_type))
	    {
	      return;
	    }
	  else
	    {
	      HDC dc;
	      RECT rect;
	      XpDCInfo dc_info;
	      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

	      DrawFrameControl (dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH |
				(state_type ==
				 GTK_STATE_ACTIVE ? (DFCS_PUSHED | DFCS_FLAT)
				 : 0));
	      release_window_dc (&dc_info);
	    }
	}
      else if (is_toolbar_child (gtk_widget_get_parent (widget))
	       || (!GTK_IS_BUTTON (widget) ||
		   (GTK_RELIEF_NONE == gtk_button_get_relief (GTK_BUTTON (widget)))))
	{
	  if (draw_tool_button (cr, widget, style, x, y,
				width, height, state_type))
	    {
	      return;
	    }
	}
      else
	{
	  gboolean is_default = gtk_widget_has_default (widget);
	  if (xp_theme_draw
	      (cr,
	       is_default ? XP_THEME_ELEMENT_DEFAULT_BUTTON :
	       XP_THEME_ELEMENT_BUTTON, style, x, y, width, height,
	       state_type))
	    {
	      return;
	    }

	  draw_push_button (cr, widget, style,
			    x, y, width, height, state_type, is_default);

	  return;
	}

      return;
    }
  else if (detail && !strcmp (detail, "spinbutton"))
    {
      if (xp_theme_is_drawable (XP_THEME_ELEMENT_SPIN_BUTTON_UP))
	{
	  return;
	}
    }
  else if (detail && (!strcmp (detail, "spinbutton_up")
		      || !strcmp (detail, "spinbutton_down")))
    {
      if (!xp_theme_draw (cr,
			  (!strcmp (detail, "spinbutton_up"))
			  ? XP_THEME_ELEMENT_SPIN_BUTTON_UP
			  : XP_THEME_ELEMENT_SPIN_BUTTON_DOWN,
			  style, x, y, width, height, state_type))
	{
	  RECT rect;
	  XpDCInfo dc_info;
	  HDC dc;

	  dc = get_window_dc (style, cr, state_type, &dc_info,
			      x, y, width, height, &rect);
	  DrawEdge (dc, &rect,
		    state_type ==
		    GTK_STATE_ACTIVE ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
	  release_window_dc (&dc_info);
	}
      return;
    }
  else if (detail && !strcmp (detail, "slider"))
    {
      if (GTK_IS_SCROLLBAR (widget))
	{
	  GtkScrollbar *scrollbar = GTK_SCROLLBAR (widget);
	  gboolean is_v = GTK_IS_VSCROLLBAR (widget);

	  if (xp_theme_draw (cr,
			     is_v
			     ? XP_THEME_ELEMENT_SCROLLBAR_V
			     : XP_THEME_ELEMENT_SCROLLBAR_H,
			     style, x, y, width, height, state_type))
	    {
	      XpThemeElement gripper =
		(is_v ? XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V :
		 XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H);

	      /* Do not display grippers on tiny scroll bars,
	         the limit imposed is rather arbitrary, perhaps
	         we can fetch the gripper geometry from
	         somewhere and use that... */
	      if ((gripper ==
		   XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H
		   && width < 16)
		  || (gripper ==
		      XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V && height < 16))
		{
		  return;
		}

	      xp_theme_draw (cr, gripper, style, x, y,
			     width, height, state_type);
	      return;
	    }
	  else
	    {
              if (gtk_adjustment_get_page_size(gtk_range_get_adjustment(&scrollbar->range)) >=
        	  (gtk_adjustment_get_page_size(gtk_range_get_adjustment(&scrollbar->range)) -
        	   gtk_adjustment_get_page_size(gtk_range_get_adjustment(&scrollbar->range))))
		{
		  return;
		}
	    }
	}
    }
  else if (detail && !strcmp (detail, "bar"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
	{
	  GtkProgressBar *progress_bar = GTK_PROGRESS_BAR (widget);
	  XpThemeElement xp_progress_bar =
	    map_gtk_progress_bar_to_xp (progress_bar, FALSE);

	  if (xp_theme_draw (cr, xp_progress_bar, style, x, y,
			     width, height, state_type))
	    {
	      return;
	    }

	  shadow_type = GTK_SHADOW_NONE;
	}
    }
  else if (detail && strcmp (detail, "menuitem") == 0)
    {
      shadow_type = GTK_SHADOW_NONE;
      if (draw_menu_item (cr, widget, style,
			  x, y, width, height, state_type))
	{
	  return;
	}
    }
  else if (detail && !strcmp (detail, "trough"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
	{
	  GtkProgressBar *progress_bar = GTK_PROGRESS_BAR (widget);
	  XpThemeElement xp_progress_bar =
	    map_gtk_progress_bar_to_xp (progress_bar, TRUE);
	  if (xp_theme_draw
	      (cr, xp_progress_bar, style, x, y, width, height,
	       state_type))
	    {
	      return;
	    }
	  else
	    {
	      /* Blank in classic Windows */
	    }
	}
      else if (widget && GTK_IS_SCROLLBAR (widget))
	{
	  gboolean is_vertical = GTK_IS_VSCROLLBAR (widget);

	  if (xp_theme_draw (cr,
			     is_vertical
			     ? XP_THEME_ELEMENT_TROUGH_V
			     : XP_THEME_ELEMENT_TROUGH_H,
			     style, x, y, width, height, state_type))
	    {
	      return;
	    }
	  else
	    {
	      HDC dc;
	      RECT rect;
	      XpDCInfo dc_info;

	      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

	      SetTextColor (dc, GetSysColor (COLOR_3DHILIGHT));
	      SetBkColor (dc, GetSysColor (COLOR_BTNFACE));
	      FillRect (dc, &rect, get_dither_brush ());

	      release_window_dc (&dc_info);

	      return;
	    }
	}
      else if (widget && GTK_IS_SCALE (widget))
	{
	  gboolean is_vertical = GTK_IS_VSCALE (widget);

	  if (!xp_theme_is_active ())
	    {
	      parent_class->draw_box (style, cr, state_type,
				      GTK_SHADOW_NONE,
				      widget, detail, x, y, width, height);
	    }

	  if (is_vertical)
	    {
	      if (xp_theme_draw
		  (cr, XP_THEME_ELEMENT_SCALE_TROUGH_V,
		   style, (2 * x + width) / 2, y, 2, height,
		   state_type))
		{
		  return;
		}

	      parent_class->draw_box (style, cr, state_type,
				      GTK_SHADOW_ETCHED_IN,
				      NULL, NULL,
				      (2 * x + width) / 2, y, 1, height);
	    }
	  else
	    {
	      if (xp_theme_draw
		  (cr, XP_THEME_ELEMENT_SCALE_TROUGH_H,
		   style, x, (2 * y + height) / 2, width, 2,
		   state_type))
		{
		  return;
		}

	      parent_class->draw_box (style, cr, state_type,
				      GTK_SHADOW_ETCHED_IN,
				      NULL, NULL, x,
				      (2 * y + height) / 2, width, 1);
	    }

	  return;
	}
    }
  else if (detail && strcmp (detail, "optionmenu") == 0)
    {
      if (xp_theme_draw (cr, XP_THEME_ELEMENT_EDIT_TEXT,
			 style, x, y, width, height, state_type))
	{
	  return;
	}
    }
  else if (detail
	   && (strcmp (detail, "vscrollbar") == 0
	       || strcmp (detail, "hscrollbar") == 0))
    {
      return;
    }
  else if (detail
	   && (strcmp (detail, "handlebox_bin") == 0
	       || strcmp (detail, "toolbar") == 0
	       || strcmp (detail, "menubar") == 0))
    {
      if (xp_theme_draw (cr, XP_THEME_ELEMENT_REBAR,
			 style, x, y, width, height, state_type))
	{
	  return;
	}
    }
  else if (detail && (!strcmp (detail, "handlebox")))	/* grip */
    {
      if (!xp_theme_is_active ())
	{
	  return;
	}
    }
  else if (detail && !strcmp (detail, "notebook") && GTK_IS_NOTEBOOK (widget))
    {
      if (xp_theme_draw (cr, XP_THEME_ELEMENT_TAB_PANE, style,
			 x, y, width, height, state_type))
	{
	  return;
	}
    }

  else
    {
      const gchar *name = gtk_widget_get_name (widget);

      if (name && !strcmp (name, "gtk-tooltips"))
	{
	  if (xp_theme_draw
	      (cr, XP_THEME_ELEMENT_TOOLTIP, style, x, y, width,
	       height, state_type))
	    {
	      return;
	    }
	  else
	    {
	      HBRUSH brush;
	      RECT rect;
	      XpDCInfo dc_info;
	      HDC hdc;

	      hdc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

	      brush = GetSysColorBrush (COLOR_3DDKSHADOW);

	      if (brush)
		{
		  FrameRect (hdc, &rect, brush);
		}

	      InflateRect (&rect, -1, -1);
	      FillRect (hdc, &rect, (HBRUSH) (COLOR_INFOBK + 1));

	      release_window_dc (&dc_info);

	      return;
	    }
	}
    }

  parent_class->draw_box (style, cr, state_type, shadow_type,
			  widget, detail, x, y, width, height);

  if (detail && strcmp (detail, "optionmenu") == 0)
    {
      GtkRequisition indicator_size;
      GtkBorder indicator_spacing;
      gint vline_x;

      option_menu_get_props (widget, &indicator_size, &indicator_spacing);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	{
	  vline_x =
	    x + indicator_size.width + indicator_spacing.left +
	    indicator_spacing.right;
	}
      else
	{
	  vline_x = x + width - (indicator_size.width +
				 indicator_spacing.left +
				 indicator_spacing.right) - style->xthickness;

	  parent_class->draw_vline (style, cr, state_type, widget,
				    detail,
				    y + style->ythickness + 1,
				    y + height - style->ythickness - 3, vline_x);
	}
    }
}

static void
draw_tab (GtkStyle *style,
	  cairo_t *cr,
	  GtkStateType state,
	  GtkShadowType shadow,
	  GtkWidget *widget,
	  const gchar *detail, gint x, gint y, gint width, gint height)
{
  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;

  gint arrow_height;

  g_return_if_fail (style != NULL);
  g_return_if_fail (cr != NULL);

  if (detail && !strcmp (detail, "optionmenutab"))
    {
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      if (xp_theme_draw (cr, XP_THEME_ELEMENT_COMBOBUTTON,
			 style, x - 5, allocation.y + 1,
			 width + 10, allocation.height - 2,
			 state))
	{
	  return;
	}
    }

  option_menu_get_props (widget, &indicator_size, &indicator_spacing);

  x += (width - indicator_size.width) / 2;
  arrow_height = (indicator_size.width + 1) / 2;

  y += (height - arrow_height) / 2;

  draw_varrow (cr, &style->black, shadow, GTK_ARROW_DOWN,
	       x, y, indicator_size.width, arrow_height);
}

/* Draw classic Windows tab - thanks Mozilla!
  (no system API for this, but DrawEdge can draw all the parts of a tab) */
static void
DrawTab (HDC hdc, const RECT R, gint32 aPosition, gboolean aSelected,
	 gboolean aDrawLeft, gboolean aDrawRight)
{
  gint32 leftFlag, topFlag, rightFlag, lightFlag, shadeFlag;
  RECT topRect, sideRect, bottomRect, lightRect, shadeRect;
  gint32 selectedOffset, lOffset, rOffset;

  selectedOffset = aSelected ? 1 : 0;
  lOffset = aDrawLeft ? 2 : 0;
  rOffset = aDrawRight ? 2 : 0;

  /* Get info for tab orientation/position (Left, Top, Right, Bottom) */
  switch (aPosition)
    {
    case BF_LEFT:
      leftFlag = BF_TOP;
      topFlag = BF_LEFT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      SetRect (&topRect, R.left, R.top + lOffset, R.right,
	       R.bottom - rOffset);
      SetRect (&sideRect, R.left + 2, R.top, R.right - 2 + selectedOffset,
	       R.bottom);
      SetRect (&bottomRect, R.right - 2, R.top, R.right, R.bottom);
      SetRect (&lightRect, R.left, R.top, R.left + 3, R.top + 3);
      SetRect (&shadeRect, R.left + 1, R.bottom - 2, R.left + 2,
	       R.bottom - 1);
      break;

    case BF_TOP:
      leftFlag = BF_LEFT;
      topFlag = BF_TOP;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      SetRect (&topRect, R.left + lOffset, R.top, R.right - rOffset,
	       R.bottom);
      SetRect (&sideRect, R.left, R.top + 2, R.right,
	       R.bottom - 1 + selectedOffset);
      SetRect (&bottomRect, R.left, R.bottom - 1, R.right, R.bottom);
      SetRect (&lightRect, R.left, R.top, R.left + 3, R.top + 3);
      SetRect (&shadeRect, R.right - 2, R.top + 1, R.right - 1, R.top + 2);
      break;

    case BF_RIGHT:
      leftFlag = BF_TOP;
      topFlag = BF_RIGHT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      SetRect (&topRect, R.left, R.top + lOffset, R.right,
	       R.bottom - rOffset);
      SetRect (&sideRect, R.left + 2 - selectedOffset, R.top, R.right - 2,
	       R.bottom);
      SetRect (&bottomRect, R.left, R.top, R.left + 2, R.bottom);
      SetRect (&lightRect, R.right - 3, R.top, R.right - 1, R.top + 2);
      SetRect (&shadeRect, R.right - 2, R.bottom - 3, R.right, R.bottom - 1);
      break;

    case BF_BOTTOM:
      leftFlag = BF_LEFT;
      topFlag = BF_BOTTOM;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      SetRect (&topRect, R.left + lOffset, R.top, R.right - rOffset,
	       R.bottom);
      SetRect (&sideRect, R.left, R.top + 2 - selectedOffset, R.right,
	       R.bottom - 2);
      SetRect (&bottomRect, R.left, R.top, R.right, R.top + 2);
      SetRect (&lightRect, R.left, R.bottom - 3, R.left + 2, R.bottom - 1);
      SetRect (&shadeRect, R.right - 2, R.bottom - 3, R.right, R.bottom - 1);
      break;

    default:
      g_return_if_reached ();
    }

  /* Background */
  FillRect (hdc, &R, (HBRUSH) (COLOR_3DFACE + 1));

  /* Tab "Top" */
  DrawEdge (hdc, &topRect, EDGE_RAISED, BF_SOFT | topFlag);

  /* Tab "Bottom" */
  if (!aSelected)
    DrawEdge (hdc, &bottomRect, EDGE_RAISED, BF_SOFT | topFlag);

  /* Tab "Sides" */
  if (!aDrawLeft)
    leftFlag = 0;
  if (!aDrawRight)
    rightFlag = 0;

  DrawEdge (hdc, &sideRect, EDGE_RAISED, BF_SOFT | leftFlag | rightFlag);

  /* Tab Diagonal Corners */
  if (aDrawLeft)
    DrawEdge (hdc, &lightRect, EDGE_RAISED, BF_SOFT | lightFlag);

  if (aDrawRight)
    DrawEdge (hdc, &shadeRect, EDGE_RAISED, BF_SOFT | shadeFlag);
}

static gboolean
draw_themed_tab_button (GtkStyle *style,
			cairo_t *cr,
			GtkStateType state_type,
			GtkNotebook *notebook,
			gint x, gint y,
			gint width, gint height, gint gap_side)
{
/* FIXME: poop
  GdkPixmap *pixmap = NULL;
 */
  gint border_width =
    gtk_container_get_border_width (GTK_CONTAINER (notebook));
  GtkWidget *widget = GTK_WIDGET (notebook);
  GdkRectangle draw_rect;
  GdkPixbufRotation rotation = GDK_PIXBUF_ROTATE_NONE;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  if (gap_side == GTK_POS_TOP)
    {
      int widget_right;

      if (state_type == GTK_STATE_NORMAL)
	{
	  draw_rect.x = x;
	  draw_rect.y = y;
	  draw_rect.width = width + 2;
	  draw_rect.height = height;

/* FIXME: wtf?
	  clip_rect.height--;
 */
	}
      else
	{
	  draw_rect.x = x + 2;
	  draw_rect.y = y;
	  draw_rect.width = width - 2;
	  draw_rect.height = height - 2;
	}

      /* If we are currently drawing the right-most tab, and if that tab is the selected tab... */
      widget_right = allocation.x + allocation.width - border_width - 2;

      if (draw_rect.x + draw_rect.width >= widget_right)
	{
	  draw_rect.width = widget_right - draw_rect.x;
	}
    }
  if (gap_side == GTK_POS_BOTTOM)
    {
      int widget_right;

      if (state_type == GTK_STATE_NORMAL)
	{
	  draw_rect.x = x;
	  draw_rect.y = y;
	  draw_rect.width = width + 2;
	  draw_rect.height = height;
	}
      else
	{
	  draw_rect.x = x + 2;
	  draw_rect.y = y + 2;
	  draw_rect.width = width - 2;
	  draw_rect.height = height - 2;
	}

      /* If we are currently drawing the right-most tab, and if that tab is the selected tab... */
      widget_right = allocation.x + allocation.width - border_width - 2;

      if (draw_rect.x + draw_rect.width >= widget_right)
	{
	  draw_rect.width = widget_right - draw_rect.x;
	}

      rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
    }
  else if (gap_side == GTK_POS_LEFT)
    {
      int widget_bottom;

      if (state_type == GTK_STATE_NORMAL)
	{
	  draw_rect.x = x;
	  draw_rect.y = y;
	  draw_rect.width = width;
	  draw_rect.height = height + 2;

/* FIXME: wtf?
	  clip_rect.width--;
 */
	}
      else
	{
	  draw_rect.x = x;
	  draw_rect.y = y + 2;
	  draw_rect.width = width - 2;
	  draw_rect.height = height - 2;
	}

      /* If we are currently drawing the bottom-most tab, and if that tab is the selected tab... */
      widget_bottom = allocation.x + allocation.height - border_width - 2;

      if (draw_rect.y + draw_rect.height >= widget_bottom)
	{
	  draw_rect.height = widget_bottom - draw_rect.y;
	}

      rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
    }
  else if (gap_side == GTK_POS_RIGHT)
    {
      int widget_bottom;

      if (state_type == GTK_STATE_NORMAL)
	{
	  draw_rect.x = x + 1;
	  draw_rect.y = y;
	  draw_rect.width = width;
	  draw_rect.height = height + 2;

/* FIXME: wtf?
	  clip_rect.width--;
 */
	}
      else
	{
	  draw_rect.x = x + 2;
	  draw_rect.y = y + 2;
	  draw_rect.width = width - 2;
	  draw_rect.height = height - 2;
	}

      /* If we are currently drawing the bottom-most tab, and if that tab is the selected tab... */
      widget_bottom = allocation.x + allocation.height - border_width - 2;

      if (draw_rect.y + draw_rect.height >= widget_bottom)
	{
	  draw_rect.height = widget_bottom - draw_rect.y;
	}

      rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
    }

  if (gap_side == GTK_POS_TOP)
    {
      if (!xp_theme_draw (cr, XP_THEME_ELEMENT_TAB_ITEM, style,
			  draw_rect.x, draw_rect.y,
			  draw_rect.width, draw_rect.height,
			  state_type))
	{
	  return FALSE;
	}
    }
  else
    {
/* FIXME: poop */
#if 0
      GdkPixbuf *pixbuf;
      GdkPixbuf *rotated;

      if (gap_side == GTK_POS_LEFT || gap_side == GTK_POS_RIGHT)
	{
	  pixmap = gdk_pixmap_new (cr, draw_rect.height, draw_rect.width, -1);

	  if (!xp_theme_draw (pixmap, XP_THEME_ELEMENT_TAB_ITEM, style,
/* FIXME: wtf? */
			      draw_rect.y - draw_rect.y, draw_rect.x - draw_rect.x,
			      draw_rect.height, draw_rect.width, state_type, 0))
	    {
	      g_object_unref (pixmap);
	      return FALSE;
	    }

	  pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, NULL, 0, 0, 0, 0,
						 draw_rect.height, draw_rect.width);
	  g_object_unref (pixmap);
	}
      else
	{
	  pixmap = gdk_pixmap_new (cr, draw_rect.width, draw_rect.height, -1);

	  if (!xp_theme_draw (pixmap, XP_THEME_ELEMENT_TAB_ITEM, style,
/* FIXME: wtf? */
			      draw_rect.x - draw_rect.x, draw_rect.y - draw_rect.y,
			      draw_rect.width, draw_rect.height, state_type, 0))
	    {
	      g_object_unref (pixmap);
	      return FALSE;
	    }

	  pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, NULL, 0, 0, 0, 0,
						 draw_rect.width, draw_rect.height);
	  g_object_unref (pixmap);
	}

      rotated = gdk_pixbuf_rotate_simple (pixbuf, rotation);
      g_object_unref (pixbuf);
      pixbuf = rotated;

      // XXX - This is really hacky and evil.  When we're drawing the left-most tab
      //       while it is active on a bottom-oriented notebook, there is one white
      //       pixel at the top.  There may be a better solution than this if someone
      //       has time to discover it.
      if (gap_side == GTK_POS_BOTTOM && state_type == GTK_STATE_NORMAL
	  && x == allocation.x)
	{
	  int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	  int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	  int psub = 0;

	  guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
	  guchar *p = pixels + rowstride;

	  for (psub = 0; psub < n_channels; psub++)
	    {
	      pixels[psub] = p[psub];
	    }
	}

      gdk_cairo_set_source_pixbuf (cr, pixbuf, clip_rect.x, clip_rect.y);
      cairo_paint (cr);
      g_object_unref (pixbuf);
#endif
    }

  return TRUE;
}

static gboolean
draw_tab_button (GtkStyle *style,
		 cairo_t *cr,
		 GtkStateType state_type,
		 GtkShadowType shadow_type,
		 GtkWidget *widget,
		 const gchar *detail,
		 gint x, gint y, gint width, gint height, gint gap_side)
{
  if (gap_side == GTK_POS_TOP || gap_side == GTK_POS_BOTTOM)
    {
      /* experimental tab-drawing code from mozilla */
      RECT rect;
      XpDCInfo dc_info;
      HDC dc;
      gint32 aPosition;

      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

      if (gap_side == GTK_POS_TOP)
	aPosition = BF_TOP;
      else if (gap_side == GTK_POS_BOTTOM)
	aPosition = BF_BOTTOM;
      else if (gap_side == GTK_POS_LEFT)
	aPosition = BF_LEFT;
      else
	aPosition = BF_RIGHT;

      if (state_type == GTK_STATE_PRELIGHT)
	state_type = GTK_STATE_NORMAL;

/* FIXME: wtf? 
           gdk_cairo_set_source_color (cr, &style->dark[state_type]);
 */

      DrawTab (dc, rect, aPosition,
	       state_type != GTK_STATE_PRELIGHT,
	       (gap_side != GTK_POS_LEFT), (gap_side != GTK_POS_RIGHT));

      release_window_dc (&dc_info);

      return TRUE;
    }

  return FALSE;
}

static void
draw_extension (GtkStyle *style,
		cairo_t *cr,
		GtkStateType state_type,
		GtkShadowType shadow_type,
		GtkWidget *widget,
		const gchar *detail,
		gint x, gint y,
		gint width, gint height, GtkPositionType gap_side)
{
  if (widget && GTK_IS_NOTEBOOK (widget) && detail && !strcmp (detail, "tab"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK (widget);

      /* Why this differs from gap_side, I have no idea.. */
      int real_gap_side = gtk_notebook_get_tab_pos (notebook);

      if (!draw_themed_tab_button (style, cr, state_type,
				   GTK_NOTEBOOK (widget), x, y,
				   width, height, real_gap_side))
	{
	  if (!draw_tab_button (style, cr, state_type,
				shadow_type, widget,
				detail, x, y, width, height, real_gap_side))
	    {
	      parent_class->draw_extension (style, cr, state_type,
					    shadow_type, widget, detail,
					    x, y, width, height,
					    real_gap_side);
	    }
	}
    }
}

static void
draw_box_gap (GtkStyle *style, cairo_t *cr, GtkStateType state_type,
	      GtkShadowType shadow_type,
	      GtkWidget *widget, const gchar *detail, gint x,
	      gint y, gint width, gint height, GtkPositionType gap_side,
	      gint gap_x, gint gap_width)
{
  if (GTK_IS_NOTEBOOK (widget) && detail && !strcmp (detail, "notebook"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK (widget);
      int side = gtk_notebook_get_tab_pos (notebook);
      int x2 = x, y2 = y, w2 = width, h2 = height;

      if (side == GTK_POS_TOP)
	{
	  x2 = x;
	  y2 = y - gtk_notebook_get_tab_vborder (notebook);
	  w2 = width;
	  h2 = height + gtk_notebook_get_tab_vborder (notebook) * 2;
	}
      else if (side == GTK_POS_BOTTOM)
	{
	  x2 = x;
	  y2 = y;
	  w2 = width;
	  h2 = height + gtk_notebook_get_tab_vborder (notebook) * 2;
	}
      else if (side == GTK_POS_LEFT)
	{
	  x2 = x - gtk_notebook_get_tab_hborder (notebook);
	  y2 = y;
	  w2 = width + gtk_notebook_get_tab_hborder (notebook);
	  h2 = height;
	}
      else if (side == GTK_POS_RIGHT)
	{
	  x2 = x;
	  y2 = y;
	  w2 = width + gtk_notebook_get_tab_hborder (notebook) * 2;
	  h2 = height;
	}

      if (xp_theme_draw (cr, XP_THEME_ELEMENT_TAB_PANE, style,
			 x2, y2, w2, h2, state_type))
	{
	  return;
	}
    }

  parent_class->draw_box_gap (style, cr, state_type, shadow_type,
			      widget, detail, x, y, width, height,
			      gap_side, gap_x, gap_width);
}

static gboolean
is_popup_window_child (GtkWidget *widget)
{
  GtkWidget *top;
  GtkWindowType type = -1;

  top = gtk_widget_get_toplevel (widget);

  if (top && GTK_IS_WINDOW (top))
    {
      g_object_get (top, "type", &type, NULL);

      if (type == GTK_WINDOW_POPUP)
	{			/* Hack for combo boxes */
	  return TRUE;
	}
    }

  return FALSE;
}

static void
draw_flat_box (GtkStyle *style, cairo_t *cr,
	       GtkStateType state_type, GtkShadowType shadow_type,
	       GtkWidget *widget,
	       const gchar *detail, gint x, gint y, gint width, gint height)
{
  if (detail)
    {
      if (state_type == GTK_STATE_SELECTED &&
	  (!strncmp ("cell_even", detail, 9) || !strncmp ("cell_odd", detail, 8)))
	{
          GdkColor *gc = gtk_widget_has_focus (widget) ? &style->base[state_type] : &style->base[GTK_STATE_ACTIVE];

          _cairo_draw_rectangle (cr, gc, TRUE, x, y, width, height);

	  return;
	}
      else if (!strcmp (detail, "checkbutton"))
	{
	  if (state_type == GTK_STATE_PRELIGHT)
	    {
	      return;
	    }
	}
    }

  parent_class->draw_flat_box (style, cr, state_type, shadow_type,
			       widget, detail, x, y, width, height);
}

static gboolean
draw_menu_border (cairo_t *cr, GtkStyle *style,
		  gint x, gint y, gint width, gint height)
{
  RECT rect;
  XpDCInfo dc_info;
  HDC dc;

  dc = get_window_dc (style, cr, GTK_STATE_NORMAL, &dc_info, x, y, width, height, &rect);

  if (!dc)
    return FALSE;

  if (xp_theme_is_active ())
    {
      FrameRect (dc, &rect, GetSysColorBrush (COLOR_3DSHADOW));
    }
  else
    {
      DrawEdge (dc, &rect, EDGE_RAISED, BF_RECT);
    }

  release_window_dc (&dc_info);

  return TRUE;
}

static void
draw_shadow (GtkStyle *style,
	     cairo_t *cr,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GtkWidget *widget,
	     const gchar *detail, gint x, gint y, gint width, gint height)
{
  gboolean is_handlebox;
  gboolean is_toolbar;

  if (detail && !strcmp (detail, "frame"))
    {

      HDC dc;
      RECT rect;
      XpDCInfo dc_info;



      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);
      if (is_combo_box_child (widget))
        {
          FillRect (dc, &rect, GetSysColorBrush (COLOR_WINDOW));
        }
      else if (is_popup_window_child (widget))
	{
	  FrameRect (dc, &rect, GetSysColorBrush (COLOR_WINDOWFRAME));
	}
      else
	{
	  switch (shadow_type)
	    {
	    case GTK_SHADOW_IN:
	      draw_3d_border (dc, &rect, TRUE);
	      break;

	    case GTK_SHADOW_OUT:
	      draw_3d_border (dc, &rect, FALSE);
	      break;

	    case GTK_SHADOW_ETCHED_IN:
	      draw_3d_border (dc, &rect, TRUE);
	      InflateRect (&rect, -1, -1);
	      draw_3d_border (dc, &rect, FALSE);
	      break;

	    case GTK_SHADOW_ETCHED_OUT:
	      draw_3d_border (dc, &rect, FALSE);
	      InflateRect (&rect, -1, -1);
	      draw_3d_border (dc, &rect, TRUE);
	      break;

	    case GTK_SHADOW_NONE:
	      break;
	    }
	}

      release_window_dc (&dc_info);

      return;
    }
  if (detail && (!strcmp (detail, "entry") || !strcmp (detail, "combobox")))
    {
      if (shadow_type != GTK_SHADOW_IN)
	return;

      if (!xp_theme_draw (cr, XP_THEME_ELEMENT_EDIT_TEXT, style,
			  x, y, width, height, state_type))
	{
	  HDC dc;
	  RECT rect;
	  XpDCInfo dc_info;

	  dc = get_window_dc (style, cr, state_type, &dc_info,
			      x, y, width, height, &rect);

	  DrawEdge (dc, &rect, EDGE_SUNKEN, BF_RECT);
	  release_window_dc (&dc_info);
	}

      return;
    }

  if (detail && !strcmp (detail, "scrolled_window") &&
      xp_theme_draw (cr, XP_THEME_ELEMENT_EDIT_TEXT, style,
		     x, y, width, height, state_type))
    {
      return;
    }

  if (detail && !strcmp (detail, "spinbutton"))
    return;

  if (detail && !strcmp (detail, "menu"))
    {
      if (draw_menu_border (cr, style, x, y, width, height))
	{
	  return;
	}
    }

  if (detail && !strcmp (detail, "handlebox"))
    return;

  is_handlebox = (detail && !strcmp (detail, "handlebox_bin"));
  is_toolbar = (detail
		&& (!strcmp (detail, "toolbar")
		    || !strcmp (detail, "menubar")));

  if (is_toolbar || is_handlebox)
    {
      if (shadow_type == GTK_SHADOW_NONE)
	{
	  return;
	}

      if (widget)
	{
	  HDC dc;
	  RECT rect;
	  XpDCInfo dc_info;
	  HGDIOBJ old_pen = NULL;
	  GtkPositionType pos;

	  if (is_handlebox)
	    {
	      pos = gtk_handle_box_get_handle_position (GTK_HANDLE_BOX (widget));
	      /*
	         If the handle box is at left side,
	         we shouldn't draw its right border.
	         The same holds true for top, right, and bottom.
	       */
	      switch (pos)
		{
		case GTK_POS_LEFT:
		  pos = GTK_POS_RIGHT;
		  break;

		case GTK_POS_RIGHT:
		  pos = GTK_POS_LEFT;
		  break;

		case GTK_POS_TOP:
		  pos = GTK_POS_BOTTOM;
		  break;

		case GTK_POS_BOTTOM:
		  pos = GTK_POS_TOP;
		  break;
		}
	    }
	  else
	    {
	      GtkWidget *parent = gtk_widget_get_parent (widget);

	      /* Dirty hack for toolbars contained in handle boxes */
	      if (GTK_IS_HANDLE_BOX (parent))
		{
		  pos = gtk_handle_box_get_handle_position (GTK_HANDLE_BOX (parent));
		}
	      else
		{
		  /*
		     Dirty hack:
		     Make pos != all legal enum vaules of GtkPositionType.
		     So every border will be draw.
		   */
		  pos = (GtkPositionType) - 1;
		}
	    }

	  dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

	  if (pos != GTK_POS_LEFT)
	    {
	      old_pen = SelectObject (dc, get_light_pen ());
	      MoveToEx (dc, rect.left, rect.top, NULL);
	      LineTo (dc, rect.left, rect.bottom);
	    }
	  if (pos != GTK_POS_TOP)
	    {
	      old_pen = SelectObject (dc, get_light_pen ());
	      MoveToEx (dc, rect.left, rect.top, NULL);
	      LineTo (dc, rect.right, rect.top);
	    }
	  if (pos != GTK_POS_RIGHT)
	    {
	      old_pen = SelectObject (dc, get_dark_pen ());
	      MoveToEx (dc, rect.right - 1, rect.top, NULL);
	      LineTo (dc, rect.right - 1, rect.bottom);
	    }
	  if (pos != GTK_POS_BOTTOM)
	    {
	      old_pen = SelectObject (dc, get_dark_pen ());
	      MoveToEx (dc, rect.left, rect.bottom - 1, NULL);
	      LineTo (dc, rect.right, rect.bottom - 1);
	    }
	  if (old_pen)
	    SelectObject (dc, old_pen);
	  release_window_dc (&dc_info);
	}

      return;
    }

  if (detail && !strcmp (detail, "statusbar"))
    {
      return;
    }

  parent_class->draw_shadow (style, cr, state_type, shadow_type,
			     widget, detail, x, y, width, height);
}

static void
draw_hline (GtkStyle *style,
	    cairo_t *cr,
	    GtkStateType state_type,
	    GtkWidget *widget,
	    const gchar *detail, gint x1, gint x2, gint y)
{
  if (xp_theme_is_active () && detail && !strcmp (detail, "menuitem"))
    {
      gint cx, cy;
      gint new_y, new_height;
      gint y_offset;

      xp_theme_get_element_dimensions (XP_THEME_ELEMENT_MENU_SEPARATOR,
				       state_type,
				       &cx, &cy);

      /* Center the separator */
      y_offset = (cy / 2) - 1;
      new_y = (y - y_offset) >= 0 ? y - y_offset : y;
      new_height = cy;

      if (xp_theme_draw
	  (cr, XP_THEME_ELEMENT_MENU_SEPARATOR, style, x1, new_y, x2, new_height,
	   state_type))
	{
	  return;
	}
      else
	{
          _cairo_draw_line (cr, &style->dark[state_type], x1, y, x2, y);

	}
    }
  else
    {
      if (style->ythickness == 2)
	{
          _cairo_draw_line (cr, &style->dark[state_type], x1, y, x2, y);
	  ++y;
          _cairo_draw_line (cr, &style->light[state_type], x1, y, x2, y);

	}
      else
	{
	  parent_class->draw_hline (style, cr, state_type, widget,
				    detail, x1, x2, y);
	}
    }
}

static void
draw_vline (GtkStyle *style,
	    cairo_t *cr,
	    GtkStateType state_type,
	    GtkWidget *widget,
	    const gchar *detail, gint y1, gint y2, gint x)
{
  if (style->xthickness == 2)
    {
      _cairo_draw_line (cr, &style->dark[state_type], x, y1, x, y2);
      ++x;
      _cairo_draw_line (cr, &style->light[state_type], x, y1, x, y2);

    }
  else
    {
      parent_class->draw_vline (style, cr, state_type, widget,
				detail, y1, y2, x);
    }
}

static void
draw_slider (GtkStyle *style,
	     cairo_t *cr,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GtkWidget *widget,
	     const gchar *detail,
	     gint x,
	     gint y, gint width, gint height, GtkOrientation orientation)
{
  if (GTK_IS_SCALE (widget) &&
      xp_theme_draw (cr, ((orientation == GTK_ORIENTATION_VERTICAL) ?
			      XP_THEME_ELEMENT_SCALE_SLIDER_V :
			      XP_THEME_ELEMENT_SCALE_SLIDER_H), style, x, y, width,
		     height, state_type))
    {
      return;
    }

  parent_class->draw_slider (style, cr, state_type, shadow_type,
			     widget, detail, x, y, width, height,
			     orientation);
}

static void
draw_resize_grip (GtkStyle *style,
		  cairo_t *cr,
		  GtkStateType state_type,
		  GtkWidget *widget,
		  const gchar *detail,
		  GdkWindowEdge edge, gint x, gint y, gint width, gint height)
{
  if (detail && !strcmp (detail, "statusbar"))
    {
      if (xp_theme_draw
	  (cr, XP_THEME_ELEMENT_STATUS_GRIPPER, style, x, y, width,
	   height, state_type))
	{
	  return;
	}
      else
	{
	  RECT rect;
	  XpDCInfo dc_info;
	  HDC dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

/* FIXME: wtf?
              gdk_cairo_set_source_color (cr, &style->dark[state_type]);
 */

	  DrawFrameControl (dc, &rect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
	  release_window_dc (&dc_info);

	  return;
	}
    }

  parent_class->draw_resize_grip (style, cr, state_type,
				  widget, detail, edge, x, y, width, height);
}

static void
draw_handle (GtkStyle *style,
	     cairo_t *cr,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GtkWidget *widget,
	     const gchar *detail,
	     gint x,
	     gint y, gint width, gint height, GtkOrientation orientation)
{
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  if (is_toolbar_child (widget))
    {
      XpThemeElement hndl;

      if (GTK_IS_HANDLE_BOX (widget))
	{
	  GtkPositionType pos;
	  pos = gtk_handle_box_get_handle_position (GTK_HANDLE_BOX (widget));

	  if (pos == GTK_POS_TOP || pos == GTK_POS_BOTTOM)
	    {
	      orientation = GTK_ORIENTATION_HORIZONTAL;
	    }
	  else
	    {
	      orientation = GTK_ORIENTATION_VERTICAL;
	    }
	}

      if (orientation == GTK_ORIENTATION_VERTICAL)
	hndl = XP_THEME_ELEMENT_REBAR_GRIPPER_V;
      else
	hndl = XP_THEME_ELEMENT_REBAR_GRIPPER_H;

      if (xp_theme_draw (cr, hndl, style, x, y, width, height,
			 state_type))
	{
	  return;
	}

      dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);

      if (orientation == GTK_ORIENTATION_VERTICAL)
	{
	  rect.left += 3;
	  rect.right = rect.left + 3;
	  rect.bottom -= 3;
	  rect.top += 3;
	}
      else
	{
	  rect.top += 3;
	  rect.bottom = rect.top + 3;
	  rect.right -= 3;
	  rect.left += 3;
	}

      draw_3d_border (dc, &rect, FALSE);
      release_window_dc (&dc_info);
      return;
    }

  if (!GTK_IS_PANED (widget))
    {
      gint xthick, ythick;
      GdkColor *light, *dark, *shadow;
      GdkRectangle dest;

      gtk_paint_box (style, cr, state_type, shadow_type,
		     widget, detail, x, y, width, height);

      light = &style->light[state_type];
      dark = &style->dark[state_type];
      shadow = &style->mid[state_type];

      xthick = style->xthickness;
      ythick = style->ythickness;

      dest.x = x + xthick;
      dest.y = y + ythick;
      dest.width = width - (xthick * 2);
      dest.height = height - (ythick * 2);

      if (dest.width < dest.height)
	dest.x += 2;
      else
	dest.y += 2;

      if (dest.width < dest.height)
	{
          _cairo_draw_line (cr, light, dest.x, dest.y, dest.x,
			 dest.height);
          _cairo_draw_line (cr, dark, dest.x + (dest.width / 2),
			 dest.y, dest.x + (dest.width / 2), dest.height);
          _cairo_draw_line (cr, shadow, dest.x + dest.width,
			 dest.y, dest.x + dest.width, dest.height);
	}
      else
	{
          _cairo_draw_line (cr, light, dest.x, dest.y,
			 dest.x + dest.width, dest.y);
          _cairo_draw_line (cr, dark, dest.x,
			 dest.y + (dest.height / 2),
			 dest.x + dest.width, dest.y + (dest.height / 2));
          _cairo_draw_line (cr, shadow, dest.x,
			 dest.y + dest.height, dest.x + dest.width,
			 dest.y + dest.height);
	}
    }
}

static void
draw_focus (GtkStyle *style,
	    cairo_t *cr,
	    GtkStateType state_type,
	    GtkWidget *widget,
	    const gchar *detail, gint x, gint y, gint width, gint height)
{
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  if (!gtk_widget_get_can_focus (widget))
    {
      return;
    }

  if (is_combo_box_child (widget)
      && (GTK_IS_ARROW (widget) || GTK_IS_BUTTON (widget)))
    {
      return;
    }
  if (GTK_IS_TREE_VIEW (gtk_widget_get_parent (widget))	/* list view bheader */)
    {
      return;
    }

  dc = get_window_dc (style, cr, state_type, &dc_info, x, y, width, height, &rect);
  DrawFocusRect (dc, &rect);
  release_window_dc (&dc_info);
/*
    parent_class->draw_focus (style, cr, state_type,
						     widget, detail, x, y, width, height);
*/
}

static void
draw_layout (GtkStyle *style,
	     cairo_t *cr,
	     GtkStateType state_type,
	     gboolean use_text,
	     GtkWidget *widget,
	     const gchar *detail,
	     gint old_x, gint old_y, PangoLayout *layout)
{
  GtkNotebook *notebook = NULL;
  gint x = old_x;
  gint y = old_y;

  /* In the XP theme, labels don't appear correctly centered inside
   * notebook tabs, so we give them a gentle nudge two pixels to the
   * right.  A little hackish, but what are 'ya gonna do?  -- Cody
   */
  if (xp_theme_is_active () && detail && !strcmp (detail, "label"))
    {
      if (gtk_widget_get_parent (widget) != NULL)
	{
	  if (GTK_IS_NOTEBOOK (gtk_widget_get_parent (widget)))
	    {
	      int side;
	      notebook = GTK_NOTEBOOK (gtk_widget_get_parent (widget));
	      side = gtk_notebook_get_tab_pos (notebook);

	      if (side == GTK_POS_TOP || side == GTK_POS_BOTTOM)
		{
		  x += 2;
		}
	    }
	}
    }

  parent_class->draw_layout (style, cr, state_type,
			     use_text, widget, detail, x, y, layout);
}

static void
msw_style_init_from_rc (GtkStyle *style, GtkRcStyle *rc_style)
{
  setup_system_font (style);
  setup_menu_settings (gtk_settings_get_default ());
  setup_system_styles (style);
  parent_class->init_from_rc (style, rc_style);
}

static void
msw_style_class_init (MswStyleClass *klass)
{
  GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  style_class->init_from_rc = msw_style_init_from_rc;
  style_class->draw_arrow = draw_arrow;
  style_class->draw_box = draw_box;
  style_class->draw_check = draw_check;
  style_class->draw_option = draw_option;
  style_class->draw_tab = draw_tab;
  style_class->draw_flat_box = draw_flat_box;
  style_class->draw_expander = draw_expander;
  style_class->draw_extension = draw_extension;
  style_class->draw_box_gap = draw_box_gap;
  style_class->draw_shadow = draw_shadow;
  style_class->draw_hline = draw_hline;
  style_class->draw_vline = draw_vline;
  style_class->draw_handle = draw_handle;
  style_class->draw_resize_grip = draw_resize_grip;
  style_class->draw_slider = draw_slider;
  style_class->draw_focus = draw_focus;
  style_class->draw_layout = draw_layout;
}

GType msw_type_style = 0;

void
msw_style_register_type (GTypeModule *module)
{
  const GTypeInfo object_info = {
    sizeof (MswStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) msw_style_class_init,
    NULL,			/* class_finalize */
    NULL,			/* class_data */
    sizeof (MswStyle),
    0,				/* n_preallocs */
    (GInstanceInitFunc) NULL,
  };

  msw_type_style = g_type_module_register_type (module,
						GTK_TYPE_STYLE,
						"MswStyle", &object_info, 0);
}

void
msw_style_init (void)
{
  xp_theme_init ();
  msw_style_setup_system_settings ();
  setup_msw_rc_style ();

  if (g_light_pen)
    {
      DeleteObject (g_light_pen);
      g_light_pen = NULL;
    }

  if (g_dark_pen)
    {
      DeleteObject (g_dark_pen);
      g_dark_pen = NULL;
    }
}

void
msw_style_finalize (void)
{
  if (g_dither_brush)
    {
      DeleteObject (g_dither_brush);
    }

  if (g_light_pen)
    {
      DeleteObject (g_light_pen);
    }

  if (g_dark_pen)
    {
      DeleteObject (g_dark_pen);
    }
}
