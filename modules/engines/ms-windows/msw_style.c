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

#include "gdk/gdk.h"
#include "gtk/gtk.h"

#ifdef BUILDING_STANDALONE
#include "gdk/gdkwin32.h"
#else
#include "gdk/win32/gdkwin32.h"
#endif


#define DETAIL(xx)   ((detail) && (!strcmp(xx, detail)))


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
     http://developer.gnome.org/doc/API/2.0/gtk/GtkSettings.html
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

  /* enable coloring for text on buttons
   * TODO: use GetThemeMetric for the border and outside border */
  g_snprintf (buf, sizeof (buf),
              "style \"msw-button\" = \"msw-default\"\n"
              "{\n"
              "bg[NORMAL] = { %d, %d, %d }\n"
              "bg[PRELIGHT] = { %d, %d, %d }\n"
              "bg[INSENSITIVE] = { %d, %d, %d }\n"
              "fg[PRELIGHT] = { %d, %d, %d }\n"
              "GtkButton::default-border = { 0, 0, 0, 0 }\n"
              "GtkButton::default-outside-border = { 0, 0, 0, 0 }\n"
              "GtkButton::child-displacement-x = %d\n"
              "GtkButton::child-displacement-y = %d\n"
              "GtkWidget::focus-padding = %d\n"
              "}widget_class \"*Button*\" style \"msw-button\"\n",
              btn_face.red, btn_face.green, btn_face.blue,
              btn_face.red, btn_face.green, btn_face.blue,
              btn_face.red, btn_face.green, btn_face.blue,
              btn_fore.red, btn_fore.green, btn_fore.blue,
              xp_theme_is_active ()? 0 : 1,
              xp_theme_is_active ()? 0 : 1,
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
                          &style->fg[GTK_STATE_ACTIVE]);
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

static gboolean
sanitize_size (GdkWindow *window, gint *width, gint *height)
{
  gboolean set_bg = FALSE;

  if ((*width == -1) && (*height == -1))
    {
      set_bg = GDK_IS_WINDOW (window);
      gdk_drawable_get_size (window, width, height);
    }
  else if (*width == -1)
    {
      gdk_drawable_get_size (window, width, NULL);
    }
  else if (*height == -1)
    {
      gdk_drawable_get_size (window, NULL, height);
    }

  return set_bg;
}

static XpThemeElement
map_gtk_progress_bar_to_xp (GtkProgressBar *progress_bar, gboolean trough)
{
  XpThemeElement ret;

  switch (gtk_progress_bar_get_orientation (progress_bar))
    {
    case GTK_PROGRESS_LEFT_TO_RIGHT:
    case GTK_PROGRESS_RIGHT_TO_LEFT:
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

  for (tmp = w->parent; tmp; tmp = tmp->parent)
    {
      if (GTK_IS_COMBO_BOX (tmp))
	return TRUE;
    }

  return FALSE;
}

static void
draw_part (GdkDrawable *drawable,
           GdkColor *gc, GdkRectangle *area, gint x, gint y, Part part)
{
  cairo_t *cr = gdk_cairo_create (drawable);

  if (area)
    {
      gdk_cairo_rectangle (cr, area);
      cairo_clip (cr);
    }

  if (!parts[part].bmap)
    {
      parts[part].bmap = cairo_image_surface_create_for_data ((unsigned char *)parts[part].bits,
        					              CAIRO_FORMAT_A1,
        					              PART_SIZE, PART_SIZE, 4);
    }

  gdk_cairo_set_source_color (cr, gc);
  cairo_mask_surface (cr, parts[part].bmap, x, y);

  cairo_destroy(cr);
}

static void
draw_check (GtkStyle *style,
	    GdkWindow *window,
	    GtkStateType state,
	    GtkShadowType shadow,
	    GdkRectangle *area,
	    GtkWidget *widget,
	    const gchar *detail, gint x, gint y, gint width, gint height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (DETAIL("check"))	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
          draw_part (window, &style->black, area, x, y, CHECK_TEXT);
          draw_part (window, &style->dark[state], area, x, y, CHECK_AA);
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

      if (!xp_theme_draw (window, theme_elt,
			  style, x, y, width, height, state, area))
	{
	  if (DETAIL("cellcheck"))
	    state = GTK_STATE_NORMAL;

          draw_part (window, &style->black, area, x, y, CHECK_BLACK);
          draw_part (window, &style->dark[state], area, x, y, CHECK_DARK);
          draw_part (window, &style->mid[state], area, x, y, CHECK_MID);
          draw_part (window, &style->light[state], area, x, y, CHECK_LIGHT);
          draw_part (window, &style->base[state], area, x, y, CHECK_BASE);

	  if (shadow == GTK_SHADOW_IN)
	    {
              draw_part (window, &style->text[state], area, x,
			 y, CHECK_TEXT);
              draw_part (window, &style->text_aa[state], area,
			 x, y, CHECK_AA);
	    }
	  else if (shadow == GTK_SHADOW_ETCHED_IN)
	    {
              draw_part (window, &style->text[state], area, x, y,
			 CHECK_INCONSISTENT);
              draw_part (window, &style->text_aa[state], area, x, y,
			 CHECK_AA);
	    }
	}
    }
}

static void
draw_expander (GtkStyle        *style,
               GdkWindow       *window,
               GtkStateType     state,
               GdkRectangle    *area,
               GtkWidget       *widget,
               const gchar     *detail,
               gint             x,
               gint             y,
               GtkExpanderStyle expander_style)
{
  cairo_t *cr = gdk_cairo_create (window);

  gint expander_size;
  gint expander_semi_size;
  XpThemeElement xp_expander;
  GtkOrientation orientation;

  gtk_widget_style_get (widget, "expander_size", &expander_size, NULL);

  if (DETAIL("tool-palette-header"))
    {
      /* Expanders are usually drawn as little triangles and unfortunately
       * do not support rotated drawing modes. So a hack is applied (see
       * gtk_tool_item_group_header_expose_event_cb for details) when
       * drawing a GtkToolItemGroup's header for horizontal GtkToolShells,
       * forcing the triangle to point in the right direction. Except we
       * don't draw expanders as triangles on Windows. Usually, expanders
       * are represented as "+" and "-". It sucks for "+" to become "-" and
       * the inverse when we don't want to, so reverse the hack here. */

      orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (widget));

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
          expander_style = GTK_EXPANDER_EXPANDED - expander_style;
    }

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED;
      break;

    case GTK_EXPANDER_EXPANDED:
    case GTK_EXPANDER_SEMI_EXPANDED:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED;
      break;

    default:
      g_assert_not_reached ();
    }

  if ((expander_size % 2) == 0)
    expander_size--;

  if (expander_size > 2)
    expander_size -= 2;

  if (area)
    {
      gdk_cairo_rectangle (cr, area);
      cairo_clip (cr);
      gdk_cairo_set_source_color (cr, &style->fg[state]);
    }

  expander_semi_size = expander_size / 2;
  x -= expander_semi_size;
  y -= expander_semi_size;

  if (!xp_theme_draw (window, xp_expander, style,
		      x, y, expander_size, expander_size, state, area))
    {
      HDC dc;
      RECT rect;
      HPEN pen;
      HGDIOBJ old_pen;
      XpDCInfo dc_info;

      dc = get_window_dc (style, window, state, &dc_info, x, y, expander_size,
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

  cairo_destroy(cr);
}

static void
draw_option (GtkStyle *style,
	     GdkWindow *window,
	     GtkStateType state,
	     GtkShadowType shadow,
	     GdkRectangle *area,
	     GtkWidget *widget,
	     const gchar *detail, gint x, gint y, gint width, gint height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (DETAIL("option"))	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
          draw_part (window, &style->fg[state], area, x, y, RADIO_TEXT);
	}
    }
  else
    {
      if (xp_theme_draw (window, shadow == GTK_SHADOW_IN
			 ? XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON
			 : XP_THEME_ELEMENT_RADIO_BUTTON,
			 style, x, y, width, height, state, area))
	{
	}
      else
	{
	  if (DETAIL("cellradio"))
	    state = GTK_STATE_NORMAL;

          draw_part (window, &style->black, area, x, y, RADIO_BLACK);
          draw_part (window, &style->dark[state], area, x, y, RADIO_DARK);
          draw_part (window, &style->mid[state], area, x, y, RADIO_MID);
          draw_part (window, &style->light[state], area, x, y, RADIO_LIGHT);
          draw_part (window, &style->base[state], area, x, y, RADIO_BASE);

	  if (shadow == GTK_SHADOW_IN)
            draw_part (window, &style->text[state], area, x, y, RADIO_TEXT);
	}
    }
}

static void
draw_varrow (GdkWindow *window,
             GdkColor *gc,
	     GtkShadowType shadow_type,
	     GdkRectangle *area,
	     GtkArrowType arrow_type, gint x, gint y, gint width, gint height)
{
  gint steps, extra;
  gint y_start, y_increment;
  gint i;
  cairo_t *cr;
  
  cr = gdk_cairo_create (window);

  if (area)
    {
       gdk_cairo_rectangle (cr, area);
       cairo_clip (cr);
    }

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

  gdk_cairo_set_source_color (cr, gc);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1.0);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

  cairo_move_to (cr, x + 0.5, y_start + extra * y_increment + 0.5);
  cairo_line_to (cr, x + width - 1 + 0.5, y_start + extra * y_increment + 0.5);
  cairo_line_to (cr, x + (height - 1 - extra) + 0.5, y_start + (height - 1) * y_increment + 0.5);
  cairo_close_path (cr);
  cairo_stroke_preserve (cr);
  cairo_fill (cr);

  cairo_destroy(cr);
}

static void
draw_harrow (GdkWindow *window,
             GdkColor *gc,
	     GtkShadowType shadow_type,
	     GdkRectangle *area,
	     GtkArrowType arrow_type, gint x, gint y, gint width, gint height)
{
  gint steps, extra;
  gint x_start, x_increment;
  gint i;
  cairo_t *cr;
  
  cr = gdk_cairo_create (window);

  if (area)
    {
       gdk_cairo_rectangle (cr, area);
       cairo_clip (cr);
    }

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

  gdk_cairo_set_source_color (cr, gc);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1.0);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

  cairo_move_to (cr, x_start + extra * x_increment + 0.5, y + 0.5);
  cairo_line_to (cr, x_start + extra * x_increment + 0.5, y + height - 1 + 0.5);
  cairo_line_to (cr, x_start + (width - 1) * x_increment + 0.5, y + height - (width - 1 - extra) - 1 + 0.5);
  cairo_close_path (cr);
  cairo_stroke_preserve (cr);
  cairo_fill (cr);

  cairo_destroy(cr);
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
	    GdkWindow *window,
	    GtkStateType state,
	    GtkShadowType shadow,
	    GdkRectangle *area,
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

  sanitize_size (window, &width, &height);

  if (GTK_IS_ARROW (widget) && is_combo_box_child (widget) && xp_theme_is_active ())
    return;

  if (DETAIL("spinbutton"))
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

      draw_varrow (window, &style->fg[state], shadow, area,
		   arrow_type, x, y, width, height);

      return;
    }
  else if (DETAIL("vscrollbar") || DETAIL("hscrollbar"))
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

      if (gtk_range_get_adjustment(&scrollbar->range)->page_size >=
          (gtk_range_get_adjustment(&scrollbar->range)->upper -
           gtk_range_get_adjustment(&scrollbar->range)->lower))
	{
	  is_disabled = TRUE;
	}

      if (xp_theme_draw (window, to_xp_arrow (arrow_type), style, box_x, box_y,
			 box_width, box_height, state, area))
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
	      sanitize_size (window, &width, &height);

	      dc = get_window_dc (style, window, state, &dc_info,
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
	      (window, XP_THEME_ELEMENT_REBAR_CHEVRON, style, x, y,
	       width, height, state, area))
	    {
	      return;
	    }
	}
      /* probably a gtk combo box on a toolbar */
      else if (0		/* widget->parent && GTK_IS_BUTTON
				   (widget->parent) */ )
	{
	  if (xp_theme_draw
	      (window, XP_THEME_ELEMENT_COMBOBUTTON, style, x - 3,
	       widget->allocation.y + 1, width + 5,
	       widget->allocation.height - 4, state, area))
	    {
	      return;
	    }
	}

      if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
	{
	  x += (width - 7) / 2;
	  y += (height - 5) / 2;

          draw_varrow (window, &style->fg[state], shadow, area,
		       arrow_type, x, y, 7, 5);
	}
      else
	{
	  x += (width - 5) / 2;
	  y += (height - 7) / 2;

          draw_harrow (window, &style->fg[state], shadow, area,
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
	wid = wid->parent;
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
	wid = wid->parent;
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
draw_menu_item (GdkWindow *window, GtkWidget *widget, GtkStyle *style,
		gint x, gint y, gint width, gint height,
		GtkStateType state_type, GdkRectangle *area)
{
  GtkWidget *parent;
  GtkMenuShell *bar;
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  if (xp_theme_is_active ())
    {
      return (xp_theme_draw (window, XP_THEME_ELEMENT_MENU_ITEM, style,
                             x, y, width, height, state_type, area));
    }

  if ((parent = gtk_widget_get_parent (widget))
      && GTK_IS_MENU_BAR (parent) && !xp_theme_is_active ())
    {
      bar = GTK_MENU_SHELL (parent);

      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

      if (state_type == GTK_STATE_PRELIGHT)
	{
	  draw_3d_border (dc, &rect, bar->active);
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
draw_tool_button (GdkWindow *window, GtkWidget *widget, GtkStyle *style,
		  gint x, gint y, gint width, gint height,
		  GtkStateType state_type, GdkRectangle *area)
{
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;
  gboolean is_toggled = FALSE;

  if (xp_theme_is_active ())
    {
      return (xp_theme_draw (window, XP_THEME_ELEMENT_TOOLBAR_BUTTON, style,
			     x, y, width, height, state_type, area));
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

  dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);
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
      if (is_toggled && !is_menu_tool_button_child (widget->parent))
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
draw_push_button (GdkWindow *window, GtkWidget *widget, GtkStyle *style,
		  gint x, gint y, gint width, gint height,
		  GtkStateType state_type, gboolean is_default)
{
  HDC dc;
  RECT rect;
  XpDCInfo dc_info;

  dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

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
	  GdkWindow *window,
	  GtkStateType state_type,
	  GtkShadowType shadow_type,
	  GdkRectangle *area,
	  GtkWidget *widget,
	  const gchar *detail, gint x, gint y, gint width, gint height)
{
  if (is_combo_box_child (widget) && DETAIL("button"))
    {
      RECT rect;
      XpDCInfo dc_info;
      DWORD border;
      HDC dc;
      int cx;

      border = (GTK_TOGGLE_BUTTON (widget)->active ? DFCS_PUSHED | DFCS_FLAT : 0);

      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);
      DrawFrameControl (dc, &rect, DFC_SCROLL, DFCS_SCROLLDOWN | border);
      release_window_dc (&dc_info);

      if (xp_theme_is_active ()
	  && xp_theme_draw (window, XP_THEME_ELEMENT_COMBOBUTTON, style, x, y,
			    width, height, state_type, area))
	{
      cx = GetSystemMetrics(SM_CXVSCROLL);
      x += width - cx;
      width = cx;


      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width - cx, height, &rect);
      FillRect (dc, &rect, GetSysColorBrush (COLOR_WINDOW));
      release_window_dc (&dc_info);
      return;
	}
    }

  if (DETAIL("button") || DETAIL("buttondefault"))
    {
      if (GTK_IS_TREE_VIEW (widget->parent) || GTK_IS_CLIST (widget->parent))
      {
        if (xp_theme_draw
	      (window, XP_THEME_ELEMENT_LIST_HEADER, style, x, y,
	       width, height, state_type, area))
	    {
	      return;
	    }
	  else
	    {
	      HDC dc;
	      RECT rect;
	      XpDCInfo dc_info;
	      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

	      DrawFrameControl (dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH |
				(state_type ==
				 GTK_STATE_ACTIVE ? (DFCS_PUSHED | DFCS_FLAT)
				 : 0));
	      release_window_dc (&dc_info);
	    }
	}
      else if (is_toolbar_child (widget->parent)
	       || (!GTK_IS_BUTTON (widget) ||
		   (GTK_RELIEF_NONE == gtk_button_get_relief (GTK_BUTTON (widget)))))
	{
	  if (draw_tool_button (window, widget, style, x, y,
				width, height, state_type, area))
	    {
	      return;
	    }
	}
      else
	{
	  gboolean is_default = gtk_widget_has_default (widget);
	  if (xp_theme_draw
	      (window,
	       is_default ? XP_THEME_ELEMENT_DEFAULT_BUTTON :
	       XP_THEME_ELEMENT_BUTTON, style, x, y, width, height,
	       state_type, area))
	    {
	      return;
	    }

	  draw_push_button (window, widget, style,
			    x, y, width, height, state_type, is_default);

	  return;
	}

      return;
    }
  else if (DETAIL("spinbutton"))
    {
      if (xp_theme_is_drawable (XP_THEME_ELEMENT_SPIN_BUTTON_UP))
	{
	  return;
	}
    }
  else if (DETAIL("spinbutton_up") || DETAIL("spinbutton_down"))
    {
      if (!xp_theme_draw (window,
			  DETAIL("spinbutton_up")
			  ? XP_THEME_ELEMENT_SPIN_BUTTON_UP
			  : XP_THEME_ELEMENT_SPIN_BUTTON_DOWN,
			  style, x, y, width, height, state_type, area))
	{
	  RECT rect;
	  XpDCInfo dc_info;
	  HDC dc;

	  dc = get_window_dc (style, window, state_type, &dc_info,
			      x, y, width, height, &rect);
	  DrawEdge (dc, &rect,
		    state_type ==
		    GTK_STATE_ACTIVE ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
	  release_window_dc (&dc_info);
	}
      return;
    }
  else if (DETAIL("slider"))
    {
      if (GTK_IS_SCROLLBAR (widget))
	{
	  GtkScrollbar *scrollbar = GTK_SCROLLBAR (widget);
	  GtkOrientation orientation;
	  gboolean is_vertical;

          orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));

          if (orientation == GTK_ORIENTATION_VERTICAL)
            is_vertical = TRUE;
          else
            is_vertical = FALSE;

	  if (xp_theme_draw (window,
			     is_vertical
			     ? XP_THEME_ELEMENT_SCROLLBAR_V
			     : XP_THEME_ELEMENT_SCROLLBAR_H,
			     style, x, y, width, height, state_type, area))
	    {
	      XpThemeElement gripper =
		(is_vertical ? XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V :
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

	      xp_theme_draw (window, gripper, style, x, y,
			     width, height, state_type, area);
	      return;
	    }
	  else
	    {
              if (gtk_range_get_adjustment(&scrollbar->range)->page_size >=
        	  (gtk_range_get_adjustment(&scrollbar->range)->upper -
        	   gtk_range_get_adjustment(&scrollbar->range)->lower))
		{
		  return;
		}
	    }
	}
    }
  else if (DETAIL("bar"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
	{
	  GtkProgressBar *progress_bar = GTK_PROGRESS_BAR (widget);
	  XpThemeElement xp_progress_bar =
	    map_gtk_progress_bar_to_xp (progress_bar, FALSE);

	  if (xp_theme_draw (window, xp_progress_bar, style, x, y,
			     width, height, state_type, area))
	    {
	      return;
	    }

	  shadow_type = GTK_SHADOW_NONE;
	}
    }
  else if (DETAIL("menuitem"))
    {
      shadow_type = GTK_SHADOW_NONE;
      if (draw_menu_item (window, widget, style,
			  x, y, width, height, state_type, area))
	{
	  return;
	}
    }
  else if (DETAIL("trough"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
	{
	  GtkProgressBar *progress_bar = GTK_PROGRESS_BAR (widget);
	  XpThemeElement xp_progress_bar =
	    map_gtk_progress_bar_to_xp (progress_bar, TRUE);
	  if (xp_theme_draw
	      (window, xp_progress_bar, style, x, y, width, height,
	       state_type, area))
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
          GtkOrientation orientation;
	  gboolean is_vertical;

          orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));

          if (orientation == GTK_ORIENTATION_VERTICAL)
            is_vertical = TRUE;
          else
            is_vertical = FALSE;

	  if (xp_theme_draw (window,
			     is_vertical
			     ? XP_THEME_ELEMENT_TROUGH_V
			     : XP_THEME_ELEMENT_TROUGH_H,
			     style, x, y, width, height, state_type, area))
	    {
	      return;
	    }
	  else
	    {
	      HDC dc;
	      RECT rect;
	      XpDCInfo dc_info;

	      sanitize_size (window, &width, &height);
	      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

	      SetTextColor (dc, GetSysColor (COLOR_3DHILIGHT));
	      SetBkColor (dc, GetSysColor (COLOR_BTNFACE));
	      FillRect (dc, &rect, get_dither_brush ());

	      release_window_dc (&dc_info);

	      return;
	    }
	}
      else if (widget && GTK_IS_SCALE (widget))
	{
          GtkOrientation orientation;

          orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));

	  if (!xp_theme_is_active ())
	    {
	      parent_class->draw_box (style, window, state_type,
				      GTK_SHADOW_NONE, area,
				      widget, detail, x, y, width, height);
	    }

	  if (orientation == GTK_ORIENTATION_VERTICAL)
	    {
	      if (xp_theme_draw
		  (window, XP_THEME_ELEMENT_SCALE_TROUGH_V,
		   style, (2 * x + width) / 2, y, 2, height,
		   state_type, area))
		{
		  return;
		}

	      parent_class->draw_box (style, window, state_type,
				      GTK_SHADOW_ETCHED_IN,
				      area, NULL, NULL,
				      (2 * x + width) / 2, y, 1, height);
	    }
	  else
	    {
	      if (xp_theme_draw
		  (window, XP_THEME_ELEMENT_SCALE_TROUGH_H,
		   style, x, (2 * y + height) / 2, width, 2,
		   state_type, area))
		{
		  return;
		}

	      parent_class->draw_box (style, window, state_type,
				      GTK_SHADOW_ETCHED_IN,
				      area, NULL, NULL, x,
				      (2 * y + height) / 2, width, 1);
	    }

	  return;
	}
    }
  else if (DETAIL("optionmenu"))
    {
      if (xp_theme_draw (window, XP_THEME_ELEMENT_EDIT_TEXT,
			 style, x, y, width, height, state_type, area))
	{
	  return;
	}
    }
  else if (DETAIL("vscrollbar") || DETAIL("hscrollbar"))
    {
      return;
    }
  else if (DETAIL("handlebox_bin") || DETAIL("toolbar") || DETAIL("menubar"))
    {
      sanitize_size (window, &width, &height);
      if (xp_theme_draw (window, XP_THEME_ELEMENT_REBAR,
			 style, x, y, width, height, state_type, area))
	{
	  return;
	}
    }
  else if (DETAIL("handlebox"))	/* grip */
    {
      if (!xp_theme_is_active ())
	{
	  return;
	}
    }
  else if (DETAIL("notebook") && GTK_IS_NOTEBOOK (widget))
    {
      if (xp_theme_draw (window, XP_THEME_ELEMENT_TAB_PANE, style,
			 x, y, width, height, state_type, area))
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
	      (window, XP_THEME_ELEMENT_TOOLTIP, style, x, y, width,
	       height, state_type, area))
	    {
	      return;
	    }
	  else
	    {
	      HBRUSH brush;
	      RECT rect;
	      XpDCInfo dc_info;
	      HDC hdc;

	      hdc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

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

  parent_class->draw_box (style, window, state_type, shadow_type, area,
			  widget, detail, x, y, width, height);

  if (DETAIL("optionmenu"))
    {
      GtkRequisition indicator_size;
      GtkBorder indicator_spacing;
      gint vline_x;

      option_menu_get_props (widget, &indicator_size, &indicator_spacing);

      sanitize_size (window, &width, &height);

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

	  parent_class->draw_vline (style, window, state_type, area, widget,
				    detail,
				    y + style->ythickness + 1,
				    y + height - style->ythickness - 3, vline_x);
	}
    }
}

static void
draw_tab (GtkStyle *style,
	  GdkWindow *window,
	  GtkStateType state,
	  GtkShadowType shadow,
	  GdkRectangle *area,
	  GtkWidget *widget,
	  const gchar *detail, gint x, gint y, gint width, gint height)
{
  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;

  gint arrow_height;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if (DETAIL("optionmenutab"))
    {
      if (xp_theme_draw (window, XP_THEME_ELEMENT_COMBOBUTTON,
			 style, x - 5, widget->allocation.y + 1,
			 width + 10, widget->allocation.height - 2,
			 state, area))
	{
	  return;
	}
    }

  option_menu_get_props (widget, &indicator_size, &indicator_spacing);

  x += (width - indicator_size.width) / 2;
  arrow_height = (indicator_size.width + 1) / 2;

  y += (height - arrow_height) / 2;

  draw_varrow (window, &style->black, shadow, area, GTK_ARROW_DOWN,
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

static void
get_notebook_tab_position (GtkNotebook *notebook,
                           gboolean *start,
                           gboolean *end)
{
  gboolean found_start = FALSE, found_end = FALSE;
  gint i, n_pages;

  /* default value */
  *start = TRUE;
  *end = FALSE;

  n_pages = gtk_notebook_get_n_pages (notebook);
  for (i = 0; i < n_pages; i++)
    {
      GtkWidget *tab_child;
      GtkWidget *tab_label;
      gboolean expand;
      GtkPackType pack_type;
      gboolean is_selected;

      tab_child = gtk_notebook_get_nth_page (notebook, i);
      is_selected = gtk_notebook_get_current_page (notebook) == i;

      /* Skip invisible tabs */
      tab_label = gtk_notebook_get_tab_label (notebook, tab_child);
      if (!tab_label || !GTK_WIDGET_VISIBLE (tab_label))
        continue;

      /* Mimics what the notebook does internally. */
      if (tab_label && !gtk_widget_get_child_visible (tab_label))
        {
          /* One child is hidden because scroll arrows are present.
           * So both corners are rounded. */
          *start = FALSE;
          *end = FALSE;
          return;
        }

      gtk_notebook_query_tab_label_packing (notebook, tab_child, &expand,
                                            NULL, /* don't need fill */
                                            &pack_type);

      if (pack_type == GTK_PACK_START)
        {
          if (!found_start)
            {
              /* This is the first tab with PACK_START pack type */
              found_start = TRUE;

              if (is_selected)
                {
                  /* first PACK_START item is selected: set start to TRUE */
                  *start = TRUE;

                  if (expand && !found_end)
                    {
                      /* tentatively set end to TRUE: will be invalidated if we
                       * find other items */
                      *end = TRUE;
                    }
                }
              else
                {
                  *start = FALSE;
                }
            }
          else if (!found_end && !is_selected)
            {
              /* an unselected item exists, and no item with PACK_END pack type */
              *end = FALSE;
            }
        }

      if (pack_type == GTK_PACK_END)
        {
          if (!found_end)
            {
              /* This is the first tab with PACK_END pack type */
              found_end = TRUE;

              if (is_selected)
                {
                  /* first PACK_END item is selected: set end to TRUE */
                  *end = TRUE;

                  if (expand && !found_start)
                    {
                      /* tentatively set start to TRUE: will be invalidated if
                       * we find other items */
                      *start = TRUE;
                    }
                }
              else
                {
                *end = FALSE;
                }
            }
          else if (!found_start && !is_selected)
            {
              *start = FALSE;
            }
        }
    }
}

static gboolean
draw_themed_tab_button (GtkStyle *style,
			GdkWindow *window,
			GtkStateType state_type,
			GtkNotebook *notebook,
			gint x,
			gint y,
			gint width,
			gint height,
			gint gap_side)
{
  GdkPixmap *pixmap = NULL;
  GdkRectangle draw_rect, clip_rect;
  cairo_t *cr;
  gboolean start, stop;
  XpThemeElement element;
  gint d_w, d_h;

  get_notebook_tab_position (notebook, &start, &stop);

  if (state_type == GTK_STATE_NORMAL)
    {
      if (start && stop)
        {
          /* Both edges of the notebook are covered by the item */
          element = XP_THEME_ELEMENT_TAB_ITEM_BOTH_EDGE;
        }
      else if (start)
        {
          /* The start edge is covered by the item */
          element = XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE;
        }
      else if (stop)
        {
          /* the stop edge is reached by the item */
          element = XP_THEME_ELEMENT_TAB_ITEM_RIGHT_EDGE;
        }
      else
        {
          /* no edge should be aligned with the tab */
          element = XP_THEME_ELEMENT_TAB_ITEM;
        }
    }
  else
    {
      /* Ideally, we should do the same here. Unfortunately, we don't have ways
       * to determine what tab widget is actually being drawn here, so we can't
       * determine its position relative to the borders */
      element = XP_THEME_ELEMENT_TAB_ITEM;
    }

  draw_rect.x = x;
  draw_rect.y = y;
  draw_rect.width = width;
  draw_rect.height = height;

  /* Perform adjustments required to have the theme perfectly aligned */
  if (state_type == GTK_STATE_ACTIVE)
    {
      switch (gap_side)
        {
        case GTK_POS_TOP:
          draw_rect.x += 2;
          draw_rect.width -= 2;
          draw_rect.height -= 1;
          break;
        case GTK_POS_BOTTOM:
          draw_rect.x += 2;
          draw_rect.width -= 2;
          draw_rect.y += 1;
          draw_rect.height -= 1;
          break;
        case GTK_POS_LEFT:
          draw_rect.y += 2;
          draw_rect.height -= 2;
          draw_rect.width -= 1;
          break;
        case GTK_POS_RIGHT:
          draw_rect.y += 2;
          draw_rect.height -= 2;
          draw_rect.x += 1;
          draw_rect.width -= 1;
          break;
        }
    }
  else
    {
      switch (gap_side)
        {
        case GTK_POS_TOP:
          draw_rect.height += 1;
          draw_rect.width += 2;
          break;
        case GTK_POS_BOTTOM:
          draw_rect.y -= 1;
          draw_rect.height += 1;
          draw_rect.width += 2;
          break;
        case GTK_POS_LEFT:
          draw_rect.width += 1;
          draw_rect.height += 2;
          break;
        case GTK_POS_RIGHT:
          draw_rect.x -= 1;
          draw_rect.width += 1;
          draw_rect.height += 2;
          break;
        }
    }

  clip_rect = draw_rect;

  /* Take care of obvious case where the clipping is an empty region */
  if (clip_rect.width <= 0 || clip_rect.height <= 0)
    return TRUE;

  /* Simple case: tabs on top are just drawn as is */
  if (gap_side == GTK_POS_TOP)
    {
       return xp_theme_draw (window, element, style,
	                     draw_rect.x, draw_rect.y,
	                     draw_rect.width, draw_rect.height,
	                     state_type, &clip_rect);
    }

  /* For other cases, we need to print the tab on a pixmap, and then rotate
   * it according to the gap side */
  if (gap_side == GTK_POS_LEFT || gap_side == GTK_POS_RIGHT)
    {
      /* pixmap will have width/height inverted as we'll rotate +- PI / 2 */
      d_w = draw_rect.height;
      d_h = draw_rect.width;
    }
  else
    {
      d_w = draw_rect.width;
      d_h = draw_rect.height;
    }

  pixmap = gdk_pixmap_new (window, d_w, d_h, -1);

  /* First copy the previously saved window background */
  cr = gdk_cairo_create (pixmap);

  /* pixmaps unfortunately don't handle the alpha channel. We then
   * paint it first in white, hoping the actual background is clear */
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);
  cairo_destroy (cr);

  if (!xp_theme_draw (pixmap, element, style, 0, 0, d_w, d_h, state_type, 0))
    {
      g_object_unref (pixmap);
      return FALSE;
    }

  /* Now we have the pixmap, we need to flip/rotate it according to its
   * final position. We'll do it using cairo on the dest window */
  cr = gdk_cairo_create (window);
  cairo_rectangle (cr, clip_rect.x, clip_rect.y,
                   clip_rect.width, clip_rect.height);
  cairo_clip (cr);
  cairo_translate(cr, draw_rect.x + draw_rect.width * 0.5,
                  draw_rect.y + draw_rect.height * 0.5);

  if (gap_side == GTK_POS_LEFT || gap_side == GTK_POS_RIGHT) {
    cairo_rotate (cr, G_PI/2.0);
  }

  if (gap_side == GTK_POS_LEFT || gap_side == GTK_POS_BOTTOM) {
    cairo_scale (cr, 1, -1);
  }

  cairo_translate(cr, -d_w * 0.5, -d_h * 0.5);
  gdk_cairo_set_source_pixmap (cr, pixmap, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return TRUE;
}

static gboolean
draw_tab_button (GtkStyle *style,
		 GdkWindow *window,
		 GtkStateType state_type,
		 GtkShadowType shadow_type,
		 GdkRectangle *area,
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
	  cairo_t *cr;

      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);
	  cr = gdk_cairo_create (window);

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
      if (area)
        {
           gdk_cairo_rectangle (cr, area);
           cairo_clip (cr);
           gdk_cairo_set_source_color (cr, &style->dark[state_type]);
        }

      DrawTab (dc, rect, aPosition,
	       state_type != GTK_STATE_PRELIGHT,
	       (gap_side != GTK_POS_LEFT), (gap_side != GTK_POS_RIGHT));

      cairo_destroy (cr);

      release_window_dc (&dc_info);
      return TRUE;
    }

  return FALSE;
}

static void
draw_extension (GtkStyle *style,
		GdkWindow *window,
		GtkStateType state_type,
		GtkShadowType shadow_type,
		GdkRectangle *area,
		GtkWidget *widget,
		const gchar *detail,
		gint x, gint y,
		gint width, gint height, GtkPositionType gap_side)
{
  if (widget && GTK_IS_NOTEBOOK (widget) && DETAIL("tab"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK (widget);

      /* draw_themed_tab_button and draw_tab_button expect to work with tab
       * position, instead of simply taking the "side of the gap" (gap_side).
       * The gap side, simply said, is the side of the tab that touches the notebook
       * frame and is always the exact opposite of the tab position... */
      int tab_pos = gtk_notebook_get_tab_pos (notebook);

      if (!draw_themed_tab_button (style, window, state_type,
				   notebook, x, y,
				   width, height, tab_pos))
	{
	  if (!draw_tab_button (style, window, state_type,
				shadow_type, area, widget,
				detail, x, y, width, height, tab_pos))
	    {
	      /* GtkStyle expects the usual gap_side */
	      parent_class->draw_extension (style, window, state_type,
					    shadow_type, area, widget, detail,
					    x, y, width, height,
					    gap_side);
	    }
	}
    }
}

static void
draw_box_gap (GtkStyle *style,
              GdkWindow *window,
              GtkStateType state_type,
	      GtkShadowType shadow_type,
	      GdkRectangle *area,
	      GtkWidget *widget,
	      const gchar *detail,
	      gint x,
	      gint y,
	      gint width,
	      gint height,
	      GtkPositionType gap_side,
	      gint gap_x,
	      gint gap_width)
{
  if (GTK_IS_NOTEBOOK (widget) && DETAIL("notebook"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK (widget);

      int side = gtk_notebook_get_tab_pos (notebook);
      int x2 = x, y2 = y;
      int w2 = width + style->xthickness, h2 = height + style->ythickness;

      switch (side)
        {
        case GTK_POS_TOP:
          y2 -= 1;
          break;
        case GTK_POS_BOTTOM:
          break;
        case GTK_POS_LEFT:
          x2 -= 1;
          break;
        case GTK_POS_RIGHT:
          w2 += 1;
          break;
        }

      if (xp_theme_draw (window, XP_THEME_ELEMENT_TAB_PANE, style,
			 x2, y2, w2, h2, state_type, area))
	{
	  return;
	}
    }

  parent_class->draw_box_gap (style, window, state_type, shadow_type,
			      area, widget, detail, x, y, width, height,
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
draw_flat_box (GtkStyle *style, GdkWindow *window,
	       GtkStateType state_type, GtkShadowType shadow_type,
	       GdkRectangle *area, GtkWidget *widget,
	       const gchar *detail, gint x, gint y, gint width, gint height)
{
  if (detail)
    {
      if (state_type == GTK_STATE_SELECTED &&
	  (!strncmp ("cell_even", detail, 9) || !strncmp ("cell_odd", detail, 8)))
	{
          GdkColor *gc = gtk_widget_has_focus (widget) ? &style->base[state_type] : &style->base[GTK_STATE_ACTIVE];
          cairo_t *cr = gdk_cairo_create (window);

          _cairo_draw_rectangle (cr, gc, TRUE, x, y, width, height);

		  cairo_destroy (cr);

	  return;
	}
      else if (DETAIL("checkbutton"))
	{
	  if (state_type == GTK_STATE_PRELIGHT)
	    {
	      return;
	    }
	}
    }

  parent_class->draw_flat_box (style, window, state_type, shadow_type,
			       area, widget, detail, x, y, width, height);
}

static gboolean
draw_menu_border (GdkWindow *win, GtkStyle *style,
		  gint x, gint y, gint width, gint height)
{
  RECT rect;
  XpDCInfo dc_info;
  HDC dc;

  dc = get_window_dc (style, win, GTK_STATE_NORMAL, &dc_info, x, y, width, height, &rect);

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
	     GdkWindow *window,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GdkRectangle *area,
	     GtkWidget *widget,
	     const gchar *detail, gint x, gint y, gint width, gint height)
{
  gboolean is_handlebox;
  gboolean is_toolbar;

  if (DETAIL("frame"))
    {

      HDC dc;
      RECT rect;
      XpDCInfo dc_info;



      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);
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
  if (DETAIL("entry") || DETAIL("combobox"))
    {
      if (shadow_type != GTK_SHADOW_IN)
	return;

      if (!xp_theme_draw (window, XP_THEME_ELEMENT_EDIT_TEXT, style,
			  x, y, width, height, state_type, area))
	{
	  HDC dc;
	  RECT rect;
	  XpDCInfo dc_info;

	  dc = get_window_dc (style, window, state_type, &dc_info,
			      x, y, width, height, &rect);

	  DrawEdge (dc, &rect, EDGE_SUNKEN, BF_RECT);
	  release_window_dc (&dc_info);
	}

      return;
    }

  if (DETAIL("scrolled_window") &&
      xp_theme_draw (window, XP_THEME_ELEMENT_EDIT_TEXT, style,
		     x, y, width, height, state_type, area))
    {
      return;
    }

  if (DETAIL("spinbutton"))
    return;

  if (DETAIL("menu"))
    {
      if (draw_menu_border (window, style, x, y, width, height))
	{
	  return;
	}
    }

  if (DETAIL("handlebox"))
    return;

  is_handlebox = (DETAIL("handlebox_bin"));
  is_toolbar = (DETAIL("toolbar") || DETAIL("menubar"));

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

	  sanitize_size (window, &width, &height);

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

	  dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

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

  if (DETAIL("statusbar"))
    {
      return;
    }

  parent_class->draw_shadow (style, window, state_type, shadow_type, area,
			     widget, detail, x, y, width, height);
}

static void
draw_hline (GtkStyle *style,
	    GdkWindow *window,
	    GtkStateType state_type,
	    GdkRectangle *area,
	    GtkWidget *widget,
	    const gchar *detail, gint x1, gint x2, gint y)
{
  cairo_t *cr;
  
  cr = gdk_cairo_create (window);

  if (xp_theme_is_active () && DETAIL("menuitem"))
    {
      gint cx, cy;
      gint new_y, new_height;
      gint y_offset;

      xp_theme_get_element_dimensions (XP_THEME_ELEMENT_MENU_SEPARATOR,
				       state_type,
				       &cx, &cy);

      /* Center the separator */
      y_offset = (area->height / 2) - (cy / 2);
      new_y = y_offset >= 0 ? area->y + y_offset : area->y;
      new_height = cy;

      if (xp_theme_draw
	  (window, XP_THEME_ELEMENT_MENU_SEPARATOR, style, x1, new_y, x2, new_height,
	   state_type, area))
	{
	  return;
	}
      else
	{
	  if (area)
	    {
              gdk_cairo_rectangle (cr, area);
              cairo_clip (cr);
	    }

          _cairo_draw_line (cr, &style->dark[state_type], x1, y, x2, y);

	}
    }
  else
    {
      if (style->ythickness == 2)
	{
	  if (area)
	    {
              gdk_cairo_rectangle (cr, area);
              cairo_clip (cr);
	    }

          _cairo_draw_line (cr, &style->dark[state_type], x1, y, x2, y);
	  ++y;
          _cairo_draw_line (cr, &style->light[state_type], x1, y, x2, y);

	}
      else
	{
	  parent_class->draw_hline (style, window, state_type, area, widget,
				    detail, x1, x2, y);
	}
    }
  cairo_destroy (cr);
}

static void
draw_vline (GtkStyle *style,
	    GdkWindow *window,
	    GtkStateType state_type,
	    GdkRectangle *area,
	    GtkWidget *widget,
	    const gchar *detail, gint y1, gint y2, gint x)
{
  cairo_t *cr;
  
  cr = gdk_cairo_create (window);

  if (style->xthickness == 2)
    {
      if (area)
	{
              gdk_cairo_rectangle (cr, area);
              cairo_clip (cr);
	}

      _cairo_draw_line (cr, &style->dark[state_type], x, y1, x, y2);
      ++x;
      _cairo_draw_line (cr, &style->light[state_type], x, y1, x, y2);

    }
  else
    {
      parent_class->draw_vline (style, window, state_type, area, widget,
				detail, y1, y2, x);
    }

  cairo_destroy (cr);
}

static void
draw_slider (GtkStyle *style,
	     GdkWindow *window,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GdkRectangle *area,
	     GtkWidget *widget,
	     const gchar *detail,
	     gint x,
	     gint y, gint width, gint height, GtkOrientation orientation)
{
  if (GTK_IS_SCALE (widget) &&
      xp_theme_draw (window, ((orientation == GTK_ORIENTATION_VERTICAL) ?
			      XP_THEME_ELEMENT_SCALE_SLIDER_V :
			      XP_THEME_ELEMENT_SCALE_SLIDER_H), style, x, y, width,
		     height, state_type, area))
    {
      return;
    }

  parent_class->draw_slider (style, window, state_type, shadow_type, area,
			     widget, detail, x, y, width, height,
			     orientation);
}

static void
draw_resize_grip (GtkStyle *style,
		  GdkWindow *window,
		  GtkStateType state_type,
		  GdkRectangle *area,
		  GtkWidget *widget,
		  const gchar *detail,
		  GdkWindowEdge edge, gint x, gint y, gint width, gint height)
{
  cairo_t *cr;
  
  cr = gdk_cairo_create (window);
  
  if (DETAIL("statusbar"))
    {
      if (xp_theme_draw
	  (window, XP_THEME_ELEMENT_STATUS_GRIPPER, style, x, y, width,
	   height, state_type, area))
	{
          cairo_destroy (cr);
	  return;
	}
      else
	{
	  RECT rect;
	  XpDCInfo dc_info;
	  HDC dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

	  if (area)
            {
              gdk_cairo_rectangle (cr, area);
              cairo_clip (cr);
              gdk_cairo_set_source_color (cr, &style->dark[state_type]);
            }

	  DrawFrameControl (dc, &rect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
	  release_window_dc (&dc_info);

          cairo_destroy (cr);
	  return;
	}
    }

  parent_class->draw_resize_grip (style, window, state_type, area,
				  widget, detail, edge, x, y, width, height);
}

static void
draw_handle (GtkStyle      *style,
             GdkWindow     *window,
             GtkStateType   state_type,
             GtkShadowType  shadow_type,
             GdkRectangle  *area,
             GtkWidget     *widget,
             const gchar   *detail,
             gint           x,
             gint           y,
             gint           width,
             gint           height,
             GtkOrientation orientation)
{
  if (is_toolbar_child (widget))
    {
      HDC dc;
      RECT rect;
      XpDCInfo dc_info;
      XpThemeElement hndl;

      sanitize_size (window, &width, &height);

      if (GTK_IS_HANDLE_BOX (widget))
	{
	  GtkPositionType pos;
	  pos = gtk_handle_box_get_handle_position (GTK_HANDLE_BOX (widget));

	  if (pos == GTK_POS_TOP || pos == GTK_POS_BOTTOM)
	      orientation = GTK_ORIENTATION_HORIZONTAL;
	  else
	      orientation = GTK_ORIENTATION_VERTICAL;
	}

      if (orientation == GTK_ORIENTATION_VERTICAL)
	hndl = XP_THEME_ELEMENT_REBAR_GRIPPER_V;
      else
	hndl = XP_THEME_ELEMENT_REBAR_GRIPPER_H;

      if (xp_theme_draw (window, hndl, style, x, y, width, height, state_type, area))
	return;

      dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);

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
}

static void
draw_focus (GtkStyle *style,
	    GdkWindow *window,
	    GtkStateType state_type,
	    GdkRectangle *area,
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
  if (GTK_IS_TREE_VIEW (widget->parent)	/* list view bheader */
      || GTK_IS_CLIST (widget->parent))
    {
      return;
    }

  dc = get_window_dc (style, window, state_type, &dc_info, x, y, width, height, &rect);
  DrawFocusRect (dc, &rect);
  release_window_dc (&dc_info);
/*
    parent_class->draw_focus (style, window, state_type,
						     area, widget, detail, x, y, width, height);
*/
}

static void
draw_layout (GtkStyle *style,
	     GdkWindow *window,
	     GtkStateType state_type,
	     gboolean use_text,
	     GdkRectangle *area,
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
  if (xp_theme_is_active () && DETAIL("label"))
    {
      if (widget->parent != NULL)
	{
	  if (GTK_IS_NOTEBOOK (widget->parent))
	    {
	      int side;
	      notebook = GTK_NOTEBOOK (widget->parent);
	      side = gtk_notebook_get_tab_pos (notebook);

	      if (side == GTK_POS_TOP || side == GTK_POS_BOTTOM)
		{
		  x += 2;
		}
	    }
	}
    }

  parent_class->draw_layout (style, window, state_type,
			     use_text, area, widget, detail, x, y, layout);
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
