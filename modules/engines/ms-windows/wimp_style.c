/* Wimp "Windows Impersonator" Engine
 *
 * Copyright (C) 2003 Raymond Penners <raymond@dotsphinx.com>
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

#include "wimp_style.h"

#include <windows.h>
#include <uxtheme.h>
#include <tmschema.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#include <stdio.h>

/* Default values, not normally used
 */
static GtkRequisition default_option_indicator_size = { 9, 8 };
static GtkBorder default_option_indicator_spacing = { 7, 5, 2, 2 };

static GtkStyleClass *parent_class;


typedef enum {
  CHECK_AA,
  CHECK_BASE,
  CHECK_BLACK,
  CHECK_DARK,
  CHECK_LIGHT,
  CHECK_MID,
  CHECK_TEXT,
  RADIO_BASE,
  RADIO_BLACK,
  RADIO_DARK,
  RADIO_LIGHT,
  RADIO_MID,
  RADIO_TEXT
} Part;

#define PART_SIZE 13

static char check_aa_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char check_base_bits[] = {
 0x00,0x00,0x00,0x00,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,
 0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0x00,0x00,0x00,0x00};
static char check_black_bits[] = {
 0x00,0x00,0xfe,0x0f,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,
 0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x00,0x00};
static char check_dark_bits[] = {
 0xff,0x1f,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
 0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00};
static char check_light_bits[] = {
 0x00,0x00,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
 0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0xfe,0x1f};
static char check_mid_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,
 0x08,0x00,0x08,0x00,0x08,0x00,0x08,0xfc,0x0f,0x00,0x00};
static char check_text_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x03,0x88,0x03,0xd8,0x01,0xf8,
 0x00,0x70,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char radio_base_bits[] = {
 0x00,0x00,0x00,0x00,0xf0,0x01,0xf8,0x03,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,
 0x07,0xfc,0x07,0xf8,0x03,0xf0,0x01,0x00,0x00,0x00,0x00};
static char radio_black_bits[] = {
 0x00,0x00,0xf0,0x01,0x0c,0x02,0x04,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,
 0x00,0x02,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char radio_dark_bits[] = {
 0xf0,0x01,0x0c,0x06,0x02,0x00,0x02,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
 0x00,0x01,0x00,0x02,0x00,0x02,0x00,0x00,0x00,0x00,0x00};
static char radio_light_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
 0x10,0x00,0x10,0x00,0x08,0x00,0x08,0x0c,0x06,0xf0,0x01};
static char radio_mid_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x04,0x00,0x08,0x00,0x08,0x00,0x08,0x00,
 0x08,0x00,0x08,0x00,0x04,0x0c,0x06,0xf0,0x01,0x00,0x00};
static char radio_text_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0xf0,0x01,0xf0,0x01,0xf0,
 0x01,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static struct {
  char      *bits;
  GdkBitmap *bmap;
} parts[] = {
  { check_aa_bits, NULL },
  { check_base_bits, NULL },
  { check_black_bits, NULL },
  { check_dark_bits, NULL },
  { check_light_bits, NULL },
  { check_mid_bits, NULL },
  { check_text_bits, NULL },
  { radio_base_bits, NULL },
  { radio_black_bits, NULL },
  { radio_dark_bits, NULL },
  { radio_light_bits, NULL },
  { radio_mid_bits, NULL },
  { radio_text_bits, NULL }
};

static HINSTANCE uxtheme_dll;
typedef HRESULT (FAR PASCAL *GetThemeSysFontFunc)
     (HTHEME hTheme, int iFontID, FAR LOGFONTW *plf);
typedef HTHEME (FAR PASCAL *OpenThemeDataFunc)
     (HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (FAR PASCAL *CloseThemeDataFunc)(HTHEME theme);
typedef HRESULT (FAR PASCAL *DrawThemeBackgroundFunc)
     (HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
      const RECT *pRect, const RECT *pClipRect);

static HTHEME
open_theme_data(HWND hwnd, LPCWSTR pszClassList)
{
  OpenThemeDataFunc proc = (OpenThemeDataFunc)
    GetProcAddress(uxtheme_dll, "OpenThemeData");
  return (*proc)(hwnd, pszClassList);
}

static HRESULT
draw_theme_background(HTHEME hTheme,
                      HDC hdc,
                      int iPartId,
                      int iStateId,
                      const RECT *pRect,
                      const RECT *pClipRect)
{
  DrawThemeBackgroundFunc proc = (DrawThemeBackgroundFunc)
    GetProcAddress(uxtheme_dll, "DrawThemeBackground");
  return (*proc)(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
}


static HRESULT
close_theme_data(HTHEME theme)
{
  CloseThemeDataFunc proc = (CloseThemeDataFunc) GetProcAddress(uxtheme_dll, "CloseThemeData");
  return (*proc)(theme);
}

static gboolean
get_system_font_xp(LOGFONTW *lf)
{
  GetThemeSysFontFunc proc = (GetThemeSysFontFunc) GetProcAddress(uxtheme_dll, "GetThemeSysFont");
  HRESULT hr;
  hr = (*proc)(NULL, TMT_MSGBOXFONT, lf);
  return hr == S_OK;
}

static gboolean
get_system_font(LOGFONT *lf)
{
  gboolean ok;
  if (NULL)//uxtheme_dll)
    {
      LOGFONTW lfw;
      ok = get_system_font_xp(&lfw);
      if (ok)
        {
          memcpy(lf, &lfw, sizeof(*lf));
          WideCharToMultiByte(CP_ACP, 0, lfw.lfFaceName, -1,
                              lf->lfFaceName, sizeof(lf->lfFaceName),
                              NULL, NULL);
        }
    }
  else
    {
      NONCLIENTMETRICS ncm;
      ncm.cbSize = sizeof(NONCLIENTMETRICS);
      ok = SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                sizeof(NONCLIENTMETRICS), &ncm, 0);
      if (ok)
        {
          *lf = ncm.lfMessageFont;
        }
//      HGDIOBJ font = GetStockObject(SYSTEM_FONT);
//      if (font)
//        {
//          if (GetObject( font, sizeof( LOGFONT ), lf ))
//            {
//              ok = TRUE;
//            }
//        }
    }
  return ok;
}


static void
setup_system_font(GtkStyle *style)
{
  LOGFONT lf;
  
  if (get_system_font(&lf))
    {
      char buf[64]; // It's okay, lfFaceName is smaller than 32 chars
      int pt_size;

      pt_size = -MulDiv(lf.lfHeight, 72,
                        GetDeviceCaps(GetDC(GetDesktopWindow()),
                                      LOGPIXELSY));
      sprintf(buf, "%s %d", lf.lfFaceName, pt_size);
      style->font_desc = pango_font_description_from_string(buf);
    }
}
  


static void
sys_color_to_gtk_color(int id, GdkColor *pcolor)
{
  DWORD color   = GetSysColor(id);
  pcolor->red   = GetRValue(color) << 8;
  pcolor->green = GetGValue(color) << 8;
  pcolor->blue  = GetBValue(color) << 8;
}

static void
setup_system_colors(GtkStyle *style)
{
  char buf[512];

  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_PRELIGHT]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_INSENSITIVE]);
  
  sys_color_to_gtk_color(COLOR_HIGHLIGHT,
                         &style->base[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHT,
                         &style->bg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHTTEXT,
                         &style->text[GTK_STATE_SELECTED]);

  sprintf(buf, "style \"wimp-menu-item\"\n"
          "{ bg[PRELIGHT] = { %d, %d, %d }\n"
          "  fg[PRELIGHT] = { %d, %d, %d }\n"
          "}\n"
          "class \"GtkMenuItem\" style \"wimp-menu-item\"\n"
          "widget_class \"*GtkAccelLabel*\" style \"wimp-menu-item\"\n",
          style->base[GTK_STATE_SELECTED].red,
          style->base[GTK_STATE_SELECTED].green,
          style->base[GTK_STATE_SELECTED].blue,
          style->text[GTK_STATE_SELECTED].red,
          style->text[GTK_STATE_SELECTED].green,
          style->text[GTK_STATE_SELECTED].blue);
  gtk_rc_parse_string(buf);

  sprintf(buf, "style \"wimp-option-menu\"\n"
          "{ GtkOptionMenu::indicator_width = 7\n"
          "GtkOptionMenu::indicator_left_spacing = 6\n"
          "GtkOptionMenu::indicator_right_spacing = 4\n"
          "bg[PRELIGHT] = { %d, %d, %d }\n"
          "fg[PRELIGHT] = { %d, %d, %d }\n"
          "}\nclass \"GtkOptionMenu\" style \"wimp-option-menu\"\n"
          "widget_class \"*GtkOptionMenu*GtkAccelLabel*\" style \"wimp-option-menu\"\n",
          style->bg[GTK_STATE_NORMAL].red,
          style->bg[GTK_STATE_NORMAL].green,
          style->bg[GTK_STATE_NORMAL].blue,
          style->text[GTK_STATE_NORMAL].red,
          style->text[GTK_STATE_NORMAL].green,
          style->text[GTK_STATE_NORMAL].blue);
  gtk_rc_parse_string(buf);
}


struct ThemeDrawInfo
{
  GdkDrawable *drawable;
  GdkGC *gc;
  HTHEME theme;
  RECT rect;
  HDC dc;
};


static gboolean
get_theme_draw_info(GtkStyle *style, GdkWindow *window,
                    int x, int y, int width, int height,
                    LPCWSTR klazz, struct ThemeDrawInfo *info)
{
  int xoff, yoff;

  if (! uxtheme_dll)
    return FALSE;

  info->theme = open_theme_data(NULL, klazz);
  if (! info->theme)
    return FALSE;
  
  gdk_window_get_internal_paint_info(window, &info->drawable, &xoff, &yoff);
  info->rect.left = x - xoff;
  info->rect.top = y - yoff;
  info->rect.right = info->rect.left + width;
  info->rect.bottom = info->rect.top + height;
  info->gc = style->dark_gc[GTK_STATE_NORMAL];
  info->dc = gdk_win32_hdc_get(info->drawable, info->gc, 0);

  return TRUE;
}

static void
free_theme_draw_info(struct ThemeDrawInfo *info)
{
  gdk_win32_hdc_release(info->drawable, info->gc, 0);
  close_theme_data(info->theme);
}

static int
get_check_button_state(GtkShadowType shadow, GtkStateType state)
{
  int ret;
  if (shadow == GTK_SHADOW_IN)
    {
    switch (state)
      {
      case GTK_STATE_NORMAL:
        ret = CBS_CHECKEDNORMAL;
        break;
      case GTK_STATE_ACTIVE:
        ret = CBS_CHECKEDPRESSED;
        break;
      case GTK_STATE_PRELIGHT:
      case GTK_STATE_SELECTED:
        ret = CBS_CHECKEDHOT;
        break;
      case GTK_STATE_INSENSITIVE:
        ret = CBS_CHECKEDDISABLED;
        break;
      }
    }
  else 
    {
    switch (state)
      {
      case GTK_STATE_NORMAL:
        ret = CBS_UNCHECKEDNORMAL;
        break;
      case GTK_STATE_ACTIVE:
        ret = CBS_UNCHECKEDPRESSED;
        break;
      case GTK_STATE_PRELIGHT:
      case GTK_STATE_SELECTED:
        ret = CBS_UNCHECKEDHOT;
        break;
      case GTK_STATE_INSENSITIVE:
        ret = CBS_UNCHECKEDDISABLED;
        break;
      }
    }
  return ret;
}

static int
get_scrollbar_trough_state(GtkStateType state_type)
{
  int state;
  switch (state_type)
    {
    case GTK_STATE_NORMAL:
      state = SCRBS_NORMAL;
      break;
    case GTK_STATE_ACTIVE:
      state = SCRBS_NORMAL;
      break;
    case GTK_STATE_PRELIGHT:
    case GTK_STATE_SELECTED:
      state = SCRBS_HOT;
      break;
    case GTK_STATE_INSENSITIVE:
      state = SCRBS_DISABLED;
      break;
    }
  return state;
}

static int
get_spin_state(int part, GtkStateType state_type)
{
  int state;
  if (part == SPNP_UP)
    {
    switch (state_type)
      {
      case GTK_STATE_NORMAL:
        state = UPS_NORMAL;
        break;
      case GTK_STATE_ACTIVE:
        state = UPS_PRESSED;
        break;
      case GTK_STATE_PRELIGHT:
      case GTK_STATE_SELECTED:
        state = UPS_HOT;
        break;
      case GTK_STATE_INSENSITIVE:
        state = UPS_DISABLED;
        break;
      }
    }
  else
    {
    switch (state_type)
      {
      case GTK_STATE_NORMAL:
        state = DNS_NORMAL;
        break;
      case GTK_STATE_ACTIVE:
        state = DNS_PRESSED;
        break;
      case GTK_STATE_PRELIGHT:
      case GTK_STATE_SELECTED:
        state = DNS_HOT;
        break;
      case GTK_STATE_INSENSITIVE:
        state = DNS_DISABLED;
        break;
      }
    }
  return state;
}

static int
get_scrollbar_arrow_button_state(GtkArrowType arrow_type,
                                 GtkStateType state_type)
{
  int state;
  if (arrow_type == GTK_ARROW_DOWN)
    {
      switch (state_type)
        {
        case GTK_STATE_NORMAL:
          state = ABS_DOWNNORMAL;
          break;
        case GTK_STATE_ACTIVE:
          state = ABS_DOWNPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
        case GTK_STATE_SELECTED:
          state = ABS_DOWNHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          state = ABS_DOWNDISABLED;
          break;
        }
    }
  else if (arrow_type == GTK_ARROW_UP)
    {
      switch (state_type)
        {
        case GTK_STATE_NORMAL:
          state = ABS_UPNORMAL;
          break;
        case GTK_STATE_ACTIVE:
          state = ABS_UPPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
        case GTK_STATE_SELECTED:
          state = ABS_UPHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          state = ABS_UPDISABLED;
          break;
        }
    }
  else if (arrow_type == GTK_ARROW_LEFT)
    {
      switch (state_type)
        {
        case GTK_STATE_NORMAL:
          state = ABS_LEFTNORMAL;
          break;
        case GTK_STATE_ACTIVE:
          state = ABS_LEFTPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
        case GTK_STATE_SELECTED:
          state = ABS_LEFTHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          state = ABS_LEFTDISABLED;
          break;
        }
    }
  else if (arrow_type == GTK_ARROW_RIGHT)
    {
      switch (state_type)
        {
        case GTK_STATE_NORMAL:
          state = ABS_RIGHTNORMAL;
          break;
        case GTK_STATE_ACTIVE:
          state = ABS_RIGHTPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
        case GTK_STATE_SELECTED:
          state = ABS_RIGHTHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          state = ABS_RIGHTDISABLED;
          break;
        }
    }
  return state;
}

static int
get_expander_state(GtkExpanderStyle expander_style, GtkStateType gtk_state)
{
  int state = GLPS_OPENED;

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      state = GLPS_CLOSED;
    }
  return state;
}

static int
get_part_state(const char *detail, GtkStateType gtk_state)
{
  int state = 0;

  if (!strcmp(detail, "button"))
    {
      state = PBS_NORMAL;
      switch (gtk_state)
        {
        case GTK_STATE_NORMAL:
        case GTK_STATE_SELECTED:
          state = PBS_NORMAL;
          break;
        case GTK_STATE_ACTIVE:
          state = PBS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          state = PBS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          state = PBS_DISABLED;
          break;
        }
    }
  else if (! strcmp(detail, "buttondefault"))
    {
      state = PBS_DEFAULTED;
    }
  else if (! strcmp(detail, "tab"))
    {
      state = TIS_NORMAL;
      switch (gtk_state)
        {
        case GTK_STATE_NORMAL:
          state = TIS_SELECTED;
          break;
        case GTK_STATE_ACTIVE:
          state = TIS_NORMAL;
          break;
        case GTK_STATE_PRELIGHT:
          state = TIS_NORMAL;
          break;
        case GTK_STATE_SELECTED:
          state = TIS_NORMAL;
          break;
        case GTK_STATE_INSENSITIVE:
          state = TIS_DISABLED;
          break;
        }
    }
  else if (! strcmp(detail, "slider"))
    {
      state = TIS_NORMAL;
      switch (gtk_state)
        {
        case GTK_STATE_NORMAL:
          state = SCRBS_NORMAL;
          break;
        case GTK_STATE_ACTIVE:
          state = SCRBS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
        case GTK_STATE_SELECTED:
          state = SCRBS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          state = SCRBS_DISABLED;
          break;
        }
    }
  return state;
}

static gboolean 
sanitize_size (GdkWindow *window,
	       gint      *width,
	       gint      *height)
{
  gboolean set_bg = FALSE;

  if ((*width == -1) && (*height == -1))
    {
      set_bg = GDK_IS_WINDOW (window);
      gdk_window_get_size (window, width, height);
    }
  else if (*width == -1)
    gdk_window_get_size (window, width, NULL);
  else if (*height == -1)
    gdk_window_get_size (window, NULL, height);

  return set_bg;
}

static void
draw_part (GdkDrawable  *drawable,
	   GdkGC        *gc,
	   GdkRectangle *area,
	   gint          x,
	   gint          y,
	   Part          part)
{
  if (area)
    gdk_gc_set_clip_rectangle (gc, area);
  
  if (!parts[part].bmap)
      parts[part].bmap = gdk_bitmap_create_from_data (drawable,
						      parts[part].bits,
						      PART_SIZE, PART_SIZE);

  gdk_gc_set_ts_origin (gc, x, y);
  gdk_gc_set_stipple (gc, parts[part].bmap);
  gdk_gc_set_fill (gc, GDK_STIPPLED);

  gdk_draw_rectangle (drawable, gc, TRUE, x, y, PART_SIZE, PART_SIZE);

  gdk_gc_set_fill (gc, GDK_SOLID);

  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
draw_check(GtkStyle      *style,
	   GdkWindow     *window,
	   GtkStateType   state,
	   GtkShadowType  shadow,
	   GdkRectangle  *area,
	   GtkWidget     *widget,
	   const gchar   *detail,
	   gint           x,
	   gint           y,
	   gint           width,
	   gint           height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;
      
  if (detail && strcmp (detail, "check") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
	  draw_part (window, style->black_gc, area, x, y, CHECK_TEXT);
	  draw_part (window, style->dark_gc[state], area, x, y, CHECK_AA);
	}
    }
  else
    {
      struct ThemeDrawInfo info;
      if (get_theme_draw_info(style, window, x, y, width,
                              height, L"Button", &info))
        {
          int pstate = get_check_button_state(shadow, state);
          draw_theme_background(info.theme, info.dc, BP_CHECKBOX,
                                pstate, &info.rect, NULL);
          free_theme_draw_info(&info);
        }
      else
        {
          draw_part (window, style->black_gc, area, x, y, CHECK_BLACK);
          draw_part (window, style->dark_gc[state], area, x, y, CHECK_DARK);
          draw_part (window, style->mid_gc[state], area, x, y, CHECK_MID);
          draw_part (window, style->light_gc[state], area, x, y, CHECK_LIGHT);
          draw_part (window, style->base_gc[state], area, x, y, CHECK_BASE);
      
          if (shadow == GTK_SHADOW_IN)
            {
              draw_part (window, style->text_gc[state], area, x, y, CHECK_TEXT);
              draw_part (window, style->text_aa_gc[state], area, x, y, CHECK_AA);
            }
        }
    }
}

static void
draw_expander(GtkStyle      *style,
	      GdkWindow     *window,
	      GtkStateType   state,
	      GdkRectangle  *area,
	      GtkWidget     *widget,
	      const gchar   *detail,
	      gint           x,
	      gint           y,
	      GtkExpanderStyle expander_style)
{
  gint expander_size;
  gint expander_semi_size;
  GdkColor color;
  GdkGCValues values;
  gboolean success;
  struct ThemeDrawInfo info;

  gtk_widget_style_get (widget, "expander_size", &expander_size, NULL);

  if (get_theme_draw_info(style, window, x, y - expander_size / 2,
			  expander_size, expander_size, L"TreeView", &info))
    {
      int pstate = get_expander_state (expander_style, state);

      draw_theme_background(info.theme, info.dc, TVP_GLYPH,
                            pstate, &info.rect, NULL);
      free_theme_draw_info(&info);
      return;
    }

  if (expander_size > 2)
    expander_size -= 2;

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state], area);

  expander_semi_size = expander_size / 2;
  x -= expander_semi_size;
  y -= expander_semi_size;

  gdk_gc_get_values (style->fg_gc[state], &values);

  /* RGB values to emulate Windows Classic style */
  color.red = color.green = color.blue = 128 << 8;

  success = gdk_colormap_alloc_color
    (gtk_widget_get_default_colormap (), &color, FALSE, TRUE);

  if (success)
    gdk_gc_set_foreground (style->fg_gc[state], &color);

  gdk_draw_rectangle
    (window, style->fg_gc[state], FALSE, x, y, expander_size, expander_size);

  if (success)
    gdk_gc_set_foreground (style->fg_gc[state], &values.foreground);
  
  gdk_draw_line
    (window, style->fg_gc[state], x + 2, y + expander_semi_size,
     x + expander_size - 2, y + expander_semi_size);

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      gdk_draw_line
	(window, style->fg_gc[state], x + expander_semi_size, y + 2,
	 x + expander_semi_size, y + expander_size - 2);
      break;
    }

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state], NULL);
}

static void
draw_option(GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state,
	    GtkShadowType  shadow,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;
      
  if (detail && strcmp (detail, "option") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	draw_part (window, style->fg_gc[state], area, x, y, RADIO_TEXT);
    }
  else
    {
      struct ThemeDrawInfo info;
      if (get_theme_draw_info(style, window, x, y, width,
                              height, L"Button", &info))
        {
          int pstate = get_check_button_state(shadow, state);
          draw_theme_background(info.theme, info.dc, BP_RADIOBUTTON,
                                pstate, &info.rect, NULL);
          free_theme_draw_info(&info);
        }
      else
	{
	  draw_part (window, style->black_gc, area, x, y, RADIO_BLACK);
	  draw_part (window, style->dark_gc[state], area, x, y, RADIO_DARK);
	  draw_part (window, style->mid_gc[state], area, x, y, RADIO_MID);
	  draw_part (window, style->light_gc[state], area, x, y, RADIO_LIGHT);
	  draw_part (window, style->base_gc[state], area, x, y, RADIO_BASE);
      
	  if (shadow == GTK_SHADOW_IN)
	    draw_part (window, style->text_gc[state], area, x, y, RADIO_TEXT);
	}
    }
}

static void
draw_varrow (GdkWindow     *window,
	     GdkGC         *gc,
	     GtkShadowType  shadow_type,
	     GdkRectangle  *area,
	     GtkArrowType   arrow_type,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint y_start, y_increment;
  gint i;

  if (area)
    gdk_gc_set_clip_rectangle (gc, area);
  
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

#if 0
  for (i = 0; i < extra; i++)
    {
      gdk_draw_line (window, gc,
		     x,              y_start + i * y_increment,
		     x + width - 1,  y_start + i * y_increment);
    }
#endif
  for (i = extra; i < height; i++)
    {
      gdk_draw_line (window, gc,
		     x + (i - extra),              y_start + i * y_increment,
		     x + width - (i - extra) - 1,  y_start + i * y_increment);
    }
  

  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
draw_harrow (GdkWindow     *window,
	     GdkGC         *gc,
	     GtkShadowType  shadow_type,
	     GdkRectangle  *area,
	     GtkArrowType   arrow_type,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint x_start, x_increment;
  gint i;

  if (area)
    gdk_gc_set_clip_rectangle (gc, area);
  
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

#if 0
  for (i = 0; i < extra; i++)
    {
      gdk_draw_line (window, gc,
		     x_start + i * x_increment, y,
		     x_start + i * x_increment, y + height - 1);
    }
#endif
  for (i = extra; i < width; i++)
    {
      gdk_draw_line (window, gc,
		     x_start + i * x_increment, y + (i - extra),
		     x_start + i * x_increment, y + height - (i - extra) - 1);
    }
  

  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

/* This function makes up for some brokeness in gtkrange.c
 * where we never get the full arrow of the stepper button
 * and the type of button in a single drawing function.
 *
 * It doesn't work correctly when the scrollbar is squished
 * to the point we don't have room for full-sized steppers.
 */
static void
reverse_engineer_stepper_box (GtkWidget    *range,
			      GtkArrowType  arrow_type,
			      gint         *x,
			      gint         *y,
			      gint         *width,
			      gint         *height)
{
  gint slider_width = 14, stepper_size = 14;
  gint box_width;
  gint box_height;
  
  if (range)
    {
      gtk_widget_style_get (range,
			    "slider_width", &slider_width,
			    "stepper_size", &stepper_size,
			    NULL);
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


static void
draw_arrow (GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state,
	    GtkShadowType  shadow,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    GtkArrowType   arrow_type,
	    gboolean       fill,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  sanitize_size (window, &width, &height);
  
  if (detail && strcmp (detail, "spinbutton") == 0)
    {
      struct ThemeDrawInfo info;
      
      if (get_theme_draw_info(style, window, x, y, width,
                              height, L"Spin", &info))
        {
          // already drawn in draw_box()
          free_theme_draw_info(&info);
          return;
        }
      else
        {
          x += (width - 7) / 2;

          if (arrow_type == GTK_ARROW_UP)
            y += (height - 4) / 2;
          else
            y += (1 + height - 4) / 2;
          draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 7, 4);
        }
    }
  else if (detail && (!strcmp (detail, "vscrollbar")
                      || !strcmp (detail, "hscrollbar")))
    {
      gint box_x = x;
      gint box_y = y;
      gint box_width = width;
      gint box_height = height;
      struct ThemeDrawInfo info;

      reverse_engineer_stepper_box (widget, arrow_type,
				    &box_x, &box_y, &box_width, &box_height);

      if (get_theme_draw_info(style, window, box_x, box_y,
                              box_width, box_height,
                              L"Scrollbar", &info))
        {
          int pstate = get_scrollbar_arrow_button_state(arrow_type,
                                                       state);
          draw_theme_background(info.theme, info.dc, SBP_ARROWBTN,
                                pstate, &info.rect, NULL);
          free_theme_draw_info(&info);
        }
      else if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
        {
          x += (width - 7) / 2;
          y += (height - 5) / 2;

          draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 7, 5);
        }
      else
        {
          y += (height - 7) / 2;
          x += (width - 5) / 2;

          draw_harrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 5, 7);
        }
    }
  else
    {
      if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
	{
	  x += (width - 7) / 2;
	  y += (height - 5) / 2;
	  
	  draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
		       x, y, 7, 5);
	}
      else
	{
	  x += (width - 5) / 2;
	  y += (height - 7) / 2;
	  
	  draw_harrow (window, style->fg_gc[state], shadow, area, arrow_type,
		       x, y, 5, 7);
	}
    }
}

static void
option_menu_get_props (GtkWidget      *widget,
		       GtkRequisition *indicator_size,
		       GtkBorder      *indicator_spacing)
{
  GtkRequisition *tmp_size = NULL;
  GtkBorder *tmp_spacing = NULL;
  
  if (widget)
    gtk_widget_style_get (widget, 
			  "indicator_size", &tmp_size,
			  "indicator_spacing", &tmp_spacing,
			  NULL);

  if (tmp_size)
    {
      *indicator_size = *tmp_size;
      g_free (tmp_size);
    }
  else
    *indicator_size = default_option_indicator_size;

  if (tmp_spacing)
    {
      *indicator_spacing = *tmp_spacing;
      g_free (tmp_spacing);
    }
  else
    *indicator_spacing = default_option_indicator_spacing;
}

static void 
draw_box (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state_type,
	  GtkShadowType  shadow_type,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
  if (detail &&
      (!strcmp (detail, "button") ||
       !strcmp (detail, "buttondefault")))
    {
      struct ThemeDrawInfo info;

      if (get_theme_draw_info(style, window, x, y, width, height,
                              L"Button", &info))
        {
          int win_state = get_part_state(detail, state_type);
          draw_theme_background(info.theme, info.dc, BP_PUSHBUTTON,
                                win_state, &info.rect, NULL);
          free_theme_draw_info(&info);
          return;
        }
    }
  else if (detail && !strcmp (detail, "spinbutton"))
    {
      struct ThemeDrawInfo info;

      if (get_theme_draw_info(style, window, x, y, width, height,
                              L"Spin", &info))
        {
          // Skip. We draw the box when the arrow is drawn.
          free_theme_draw_info(&info);
          return;
        }
    }
  else if (detail && (!strcmp (detail, "spinbutton_up")
                      || !strcmp (detail, "spinbutton_down")))
    {
      struct ThemeDrawInfo info;

      if (get_theme_draw_info(style, window, x, y, width, height,
                              L"Spin", &info))
        {
          int part;
          int pstate;

          part = 
            ! strcmp (detail, "spinbutton_up")
            ? SPNP_UP : SPNP_DOWN;
          pstate = get_spin_state(part, state_type);
          draw_theme_background(info.theme, info.dc, part,
                                pstate, &info.rect, NULL);
          free_theme_draw_info(&info);
          return;
        }
    }

  else if (detail && !strcmp (detail, "slider"))
    {
      if (GTK_IS_SCROLLBAR(widget))
        {
          GtkScrollbar * scrollbar = GTK_SCROLLBAR(widget);
        
          struct ThemeDrawInfo info;

          if (get_theme_draw_info(style, window, x, y, width, height,
                                  L"Scrollbar", &info))
            {
              int part, state, grip;
              if (GTK_IS_VSCROLLBAR(widget))
                {
                  part = SBP_THUMBBTNHORZ;
                  grip = SBP_GRIPPERVERT;
                }
              else
                {
                  part = SBP_THUMBBTNVERT;
                  grip = SBP_GRIPPERHORZ;
                }
              state = get_part_state(detail, state_type);
              draw_theme_background(info.theme, info.dc, part,
                                    state, &info.rect, NULL);
              draw_theme_background(info.theme, info.dc, grip,
                                    0, &info.rect, NULL);
              free_theme_draw_info(&info);
              return;
            }
        }
    }

  if (detail && strcmp (detail, "menuitem") == 0) 
    shadow_type = GTK_SHADOW_NONE;
  
  if (detail && !strcmp (detail, "trough"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
        {
          // Blank in classic Windows 
        }
      else
        {
          struct ThemeDrawInfo info;

          if (GTK_IS_RANGE(widget)
              && get_theme_draw_info(style, window, x, y, width, height,
                                     L"Scrollbar", &info))
            {
              int part, pstate;
              if (GTK_IS_VSCROLLBAR(widget))
                {
                  part = SBP_LOWERTRACKVERT;
                }
              else
                {
                  part = SBP_LOWERTRACKHORZ;
                }
              pstate = get_scrollbar_trough_state(state_type);
              draw_theme_background(info.theme, info.dc,
                                    part, pstate,
                                    &info.rect, NULL);
              free_theme_draw_info(&info);
              return;
            }
          else
            {
              GdkGCValues gc_values;
              GdkGC *gc;
              GdkPixmap *pixmap;

              sanitize_size (window, &width, &height);
	  
              pixmap = gdk_pixmap_new (window, 2, 2, -1);

              gdk_draw_point (pixmap, style->bg_gc[GTK_STATE_NORMAL], 0, 0);
              gdk_draw_point (pixmap, style->bg_gc[GTK_STATE_NORMAL], 1, 1);
              gdk_draw_point (pixmap, style->light_gc[GTK_STATE_NORMAL], 1, 0);
              gdk_draw_point (pixmap, style->light_gc[GTK_STATE_NORMAL], 0, 1);

              gc_values.fill = GDK_TILED;
              gc_values.tile = pixmap;
              gc_values.ts_x_origin = x;
              gc_values.ts_y_origin = y;
              gc = gdk_gc_new_with_values (window, &gc_values,
                                           GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN | GDK_GC_FILL | GDK_GC_TILE);

              if (area)
                gdk_gc_set_clip_rectangle (gc, area);
      
              gdk_draw_rectangle (window, gc, TRUE, x, y, width, height);

              gdk_gc_unref (gc);
              gdk_pixmap_unref (pixmap);
      
              return;
            }
        }
    }

  parent_class->draw_box (style, window, state_type, shadow_type, area,
			  widget, detail, x, y, width, height);

  if (detail && strcmp (detail, "optionmenu") == 0)
    {
      GtkRequisition indicator_size;
      GtkBorder indicator_spacing;
      gint vline_x;

      option_menu_get_props (widget, &indicator_size, &indicator_spacing);

      sanitize_size (window, &width, &height);
  
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	vline_x = x + indicator_size.width + indicator_spacing.left + indicator_spacing.right;
      else 
	vline_x = x + width - (indicator_size.width + indicator_spacing.left + indicator_spacing.right) - style->xthickness;

      parent_class->draw_vline (style, window, state_type, area, widget,
				detail,
				y + style->ythickness + 1,
				y + height - style->ythickness - 3,
				vline_x);
    }
}

static void
draw_tab (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state,
	  GtkShadowType  shadow,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;
  
  gint arrow_height;
  
  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if (widget)
    gtk_widget_style_get (widget, "indicator_size", &indicator_size, NULL);

#if 0
  if (detail && strcmp (detail, "optionmenutab") == 0)
    {
      struct ThemeDrawInfo info;
      if (get_theme_draw_info(style, window, x, y, width, height,
                              L"ComboBox", &info))
        {
          int partid = CP_DROPDOWNBUTTON;
          int pstate = CBXS_NORMAL;
          draw_theme_background(info.theme, info.dc, partid,
                                pstate, &info.rect, NULL);
          free_theme_draw_info(&info);
          return;
        }
    }
#endif
  option_menu_get_props (widget, &indicator_size, &indicator_spacing);

  x += (width - indicator_size.width) / 2;
  arrow_height = (indicator_size.width + 1) / 2;
  
  y += (height - arrow_height) / 2;

  draw_varrow (window, style->black_gc, shadow, area, GTK_ARROW_DOWN,
	       x, y, indicator_size.width, arrow_height);
}

  
static void
draw_extension(GtkStyle *style,
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
               GtkPositionType gap_side)
{
  if (detail && !strcmp(detail, "tab"))
    {
      struct ThemeDrawInfo info;
      GtkNotebook *notebook = GTK_NOTEBOOK(widget);

      /* FIXME: pos != TOP to be implemented */
      if (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP
          && get_theme_draw_info(style, window, x, y, width, height,
                                 L"Tab", &info))
        {
          int partid = TABP_TABITEM;
          int win_state = get_part_state(detail, state_type);

          if (state_type == GTK_STATE_NORMAL)
            {
              /* FIXME: where does the magic number 2 come from? */
              info.rect.bottom+=2; 
              if (gtk_notebook_get_current_page(notebook) == 0)
                {
                  partid = TABP_TABITEMLEFTEDGE;
                }
            }

          draw_theme_background(info.theme, info.dc, partid,
                                win_state, &info.rect, NULL);
          free_theme_draw_info(&info);
          return;
        }
    }
  parent_class->draw_extension 
    (style, window, state_type, shadow_type, area, widget, detail,
     	 x, y, width, height, gap_side);
}

static void
draw_box_gap (GtkStyle *style, GdkWindow *window, GtkStateType state_type,
              GtkShadowType shadow_type, GdkRectangle *area,
              GtkWidget *widget, const gchar *detail, gint x,
              gint y, gint width, gint height, GtkPositionType gap_side,
              gint gap_x, gint gap_width)
{
  if (detail && !strcmp(detail, "notebook"))
    {
      struct ThemeDrawInfo info;
      GtkNotebook *notebook = GTK_NOTEBOOK(widget);

      /* FIXME: pos != TOP to be implemented */
      if (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP
          && get_theme_draw_info(style, window, x, y, width, height,
                                 L"Tab", &info))
        {
          draw_theme_background(info.theme, info.dc, TABP_PANE,
                                0, &info.rect, NULL);
          free_theme_draw_info(&info);
          return;
        }
    }
  parent_class->draw_box_gap(style, window, state_type, shadow_type,
                             area, widget, detail, x, y, width, height,
                             gap_side, gap_x, gap_width);
}

static void
draw_flat_box (GtkStyle *style, GdkWindow *window,
               GtkStateType state_type, GtkShadowType shadow_type,
               GdkRectangle *area, GtkWidget *widget,
               const gchar *detail, gint x, gint y,
               gint width, gint height)
{
  if (detail && ! strcmp (detail, "checkbutton"))
    {
      if (state_type == GTK_STATE_PRELIGHT)
        {
          return;
        }
    }

  //      gtk_style_apply_default_background (style, window,
  //				  widget && !GTK_WIDGET_NO_WINDOW (widget),
  //				  state_type, area, x, y, width, height);
  
  parent_class->draw_flat_box(style, window, state_type, shadow_type,
                              area, widget, detail, x, y, width, height);
}

static void
wimp_style_init_from_rc (GtkStyle * style, GtkRcStyle * rc_style)
{
  setup_system_font (style);
  setup_system_colors (style);
  parent_class->init_from_rc(style, rc_style);
}

static void
wimp_style_init (WimpStyle * style)
{
  uxtheme_dll = LoadLibrary("uxtheme.dll");
}

static void
wimp_style_class_init (WimpStyleClass *klass)
{
  GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  style_class->init_from_rc    = wimp_style_init_from_rc;
  style_class->draw_arrow      = draw_arrow;
  style_class->draw_box        = draw_box;
  style_class->draw_check      = draw_check;
  style_class->draw_option     = draw_option;
  style_class->draw_tab        = draw_tab;
  style_class->draw_flat_box   = draw_flat_box;
  style_class->draw_expander   = draw_expander;
  style_class->draw_extension = draw_extension;
  style_class->draw_box_gap = draw_box_gap;
}

GType wimp_type_style = 0;

void
wimp_style_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (WimpStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) wimp_style_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (WimpStyle),
    0,              /* n_preallocs */
    (GInstanceInitFunc) wimp_style_init,
  };
  
  wimp_type_style = g_type_module_register_type (module,
						   GTK_TYPE_STYLE,
						   "WimpStyle",
						   &object_info, 0);
}

