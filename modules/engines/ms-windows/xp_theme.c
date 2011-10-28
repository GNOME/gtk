/* MS-Windows Engine (aka GTK-Wimp)
 *
 * Copyright (C) 2003, 2004 Raymond Penners <raymond@dotsphinx.com>
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

#define _WIN32_WINNT 0x0501

#include "xp_theme.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifdef BUILDING_STANDALONE
#include "gdk/gdkwin32.h"
#else
#include "gdk/win32/gdkwin32.h"
#endif

#include "xp_theme_defs.h"

#ifndef TMT_CAPTIONFONT

/* These aren't in mingw's "w32api" headers, nor in the Platform SDK
 * headers.
 */
#define TMT_CAPTIONFONT   801
#define TMT_MENUFONT      803
#define TMT_STATUSFONT    804
#define TMT_MSGBOXFONT    805
#endif

#define GP_LINEHORZ       2
#define GP_LINEVERT       3
#define TP_SEPARATOR      5
#define TP_SEPARATORVERT  6

/* GLOBALS LINEHORZ states */
#define LHS_FLAT          1
#define LHS_RAISED        2
#define LHS_SUNKEN        3

/* GLOBAL LINEVERT states */
#define LVS_FLAT          1
#define LVS_RAISED        2
#define LVS_SUNKEN        3

/* TRACKBAR parts */
#define TKP_TRACK         1
#define TKP_TRACKVERT     2
#define TKP_THUMB         3
#define TKP_THUMBBOTTOM   4
#define TKP_THUMBTOP      5
#define TKP_THUMBVERT     6
#define TKP_THUMBLEFT     7
#define TKP_THUMBRIGHT    8
#define TKP_TICS          9
#define TKP_TICSVERT      10

#define TRS_NORMAL        1

#define MBI_NORMAL         1
#define MBI_HOT            2
#define MBI_PUSHED         3
#define MBI_DISABLED       4
#define MBI_DISABLEDHOT    5
#define MBI_DISABLEDPUSHED 6

#define MENU_POPUPGUTTER    13
#define MENU_POPUPITEM      14
#define MENU_POPUPSEPARATOR 15

static const LPCWSTR class_descriptors[] = {
  L"Scrollbar",			/* XP_THEME_CLASS_SCROLLBAR */
  L"Button",			/* XP_THEME_CLASS_BUTTON */
  L"Header",			/* XP_THEME_CLASS_HEADER */
  L"ComboBox",			/* XP_THEME_CLASS_COMBOBOX */
  L"Tab",			/* XP_THEME_CLASS_TAB */
  L"Edit",			/* XP_THEME_CLASS_EDIT */
  L"TreeView",			/* XP_THEME_CLASS_TREEVIEW */
  L"Spin",			/* XP_THEME_CLASS_SPIN */
  L"Progress",			/* XP_THEME_CLASS_PROGRESS */
  L"Tooltip",			/* XP_THEME_CLASS_TOOLTIP */
  L"Rebar",			/* XP_THEME_CLASS_REBAR */
  L"Toolbar",			/* XP_THEME_CLASS_TOOLBAR */
  L"Globals",			/* XP_THEME_CLASS_GLOBALS */
  L"Menu",			/* XP_THEME_CLASS_MENU */
  L"Window",			/* XP_THEME_CLASS_WINDOW */
  L"Status",			/* XP_THEME_CLASS_STATUS */
  L"Trackbar"			/* XP_THEME_CLASS_TRACKBAR */
};

static const short element_part_map[XP_THEME_ELEMENT__SIZEOF] = {
  BP_CHECKBOX,
  BP_CHECKBOX,
  BP_CHECKBOX,
  BP_PUSHBUTTON,
  HP_HEADERITEM,
  CP_DROPDOWNBUTTON,
  TABP_BODY,
  TABP_TABITEM,
  TABP_TABITEMLEFTEDGE,
  TABP_TABITEMRIGHTEDGE,
  TABP_TABITEMBOTHEDGE,
  TABP_PANE,
  SBP_THUMBBTNHORZ,
  SBP_THUMBBTNVERT,
  SBP_ARROWBTN,
  SBP_ARROWBTN,
  SBP_ARROWBTN,
  SBP_ARROWBTN,
  SBP_GRIPPERHORZ,
  SBP_GRIPPERVERT,
  SBP_LOWERTRACKHORZ,
  SBP_LOWERTRACKVERT,
  EP_EDITTEXT,
  BP_PUSHBUTTON,
  SPNP_UP,
  SPNP_DOWN,
  BP_RADIOBUTTON,
  BP_RADIOBUTTON,
  TVP_GLYPH,
  TVP_GLYPH,
  PP_CHUNK,
  PP_CHUNKVERT,
  PP_BAR,
  PP_BARVERT,
  TTP_STANDARD,
  0 /* RP_BAND */ ,
  RP_GRIPPER,
  RP_GRIPPERVERT,
  RP_CHEVRON,
  TP_BUTTON,
  MENU_POPUPITEM, /*MP_MENUITEM,*/
  MENU_POPUPSEPARATOR,  /*MP_SEPARATOR,*/
  SP_GRIPPER,
  SP_PANE,
  GP_LINEHORZ,
  GP_LINEVERT,
  TP_SEPARATOR,
  TP_SEPARATORVERT,
  TKP_TRACK,
  TKP_TRACKVERT,
  TKP_THUMB,
  TKP_THUMBVERT,
  TKP_TICS,
  TKP_TICSVERT
};

#define UXTHEME_DLL "uxtheme.dll"

static HINSTANCE uxtheme_dll = NULL;
static HTHEME open_themes[XP_THEME_CLASS__SIZEOF];
static gboolean use_xp_theme = FALSE;

typedef HRESULT (FAR PASCAL *GetThemeSysFontFunc)           (HTHEME hTheme, int iFontID, OUT LOGFONTW *plf);
typedef int (FAR PASCAL *GetThemeSysSizeFunc)               (HTHEME hTheme, int iSizeId);
typedef COLORREF (FAR PASCAL *GetThemeSysColorFunc)         (HTHEME hTheme,
							     int iColorID);
typedef HTHEME (FAR PASCAL *OpenThemeDataFunc)              (HWND hwnd,
							     LPCWSTR pszClassList);
typedef HRESULT (FAR PASCAL *CloseThemeDataFunc)            (HTHEME theme);
typedef HRESULT (FAR PASCAL *DrawThemeBackgroundFunc)       (HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
							     const RECT *pRect, const RECT *pClipRect);
typedef HRESULT (FAR PASCAL *EnableThemeDialogTextureFunc)  (HWND hwnd,
							     DWORD dwFlags);
typedef BOOL (FAR PASCAL *IsThemeActiveFunc)                (VOID);
typedef BOOL (FAR PASCAL *IsAppThemedFunc)                  (VOID);
typedef BOOL (FAR PASCAL *IsThemeBackgroundPartiallyTransparentFunc) (HTHEME hTheme,
								      int iPartId,
								      int iStateId);
typedef HRESULT (FAR PASCAL *DrawThemeParentBackgroundFunc) (HWND hwnd,
							     HDC hdc,
							     RECT *prc);
typedef HRESULT (FAR PASCAL *GetThemePartSizeFunc)          (HTHEME hTheme,
							     HDC hdc,
							     int iPartId,
							     int iStateId,
							     RECT *prc,
							     int eSize,
							     SIZE *psz);

static GetThemeSysFontFunc get_theme_sys_font_func = NULL;
static GetThemeSysColorFunc get_theme_sys_color_func = NULL;
static GetThemeSysSizeFunc get_theme_sys_metric_func = NULL;
static OpenThemeDataFunc open_theme_data_func = NULL;
static CloseThemeDataFunc close_theme_data_func = NULL;
static DrawThemeBackgroundFunc draw_theme_background_func = NULL;
static EnableThemeDialogTextureFunc enable_theme_dialog_texture_func = NULL;
static IsThemeActiveFunc is_theme_active_func = NULL;
static IsAppThemedFunc is_app_themed_func = NULL;
static IsThemeBackgroundPartiallyTransparentFunc is_theme_partially_transparent_func = NULL;
static DrawThemeParentBackgroundFunc draw_theme_parent_background_func = NULL;
static GetThemePartSizeFunc get_theme_part_size_func = NULL;

static void
xp_theme_close_open_handles (void)
{
  int i;

  for (i = 0; i < XP_THEME_CLASS__SIZEOF; i++)
    {
      if (open_themes[i])
	{
	  close_theme_data_func (open_themes[i]);
	  open_themes[i] = NULL;
	}
    }
}

void
xp_theme_init (void)
{
  char *buf;
  char dummy;
  int n, k;

  if (uxtheme_dll)
    return;

  memset (open_themes, 0, sizeof (open_themes));

  n = GetSystemDirectory (&dummy, 0);

  if (n <= 0)
    return;

  buf = g_malloc (n + 1 + strlen (UXTHEME_DLL));
  k = GetSystemDirectory (buf, n);
  
  if (k == 0 || k > n)
    {
      g_free (buf);
      return;
    }

  if (!G_IS_DIR_SEPARATOR (buf[strlen (buf) -1]))
    strcat (buf, G_DIR_SEPARATOR_S);
  strcat (buf, UXTHEME_DLL);

  uxtheme_dll = LoadLibrary (buf);
  g_free (buf);

  if (!uxtheme_dll)
    return;

  is_app_themed_func = (IsAppThemedFunc) GetProcAddress (uxtheme_dll, "IsAppThemed");

  if (is_app_themed_func)
    {
      is_theme_active_func = (IsThemeActiveFunc) GetProcAddress (uxtheme_dll, "IsThemeActive");
      open_theme_data_func = (OpenThemeDataFunc) GetProcAddress (uxtheme_dll, "OpenThemeData");
      close_theme_data_func = (CloseThemeDataFunc) GetProcAddress (uxtheme_dll, "CloseThemeData");
      draw_theme_background_func = (DrawThemeBackgroundFunc) GetProcAddress (uxtheme_dll, "DrawThemeBackground");
      enable_theme_dialog_texture_func = (EnableThemeDialogTextureFunc) GetProcAddress (uxtheme_dll, "EnableThemeDialogTexture");
      get_theme_sys_font_func = (GetThemeSysFontFunc) GetProcAddress (uxtheme_dll, "GetThemeSysFont");
      get_theme_sys_color_func = (GetThemeSysColorFunc) GetProcAddress (uxtheme_dll, "GetThemeSysColor");
      get_theme_sys_metric_func = (GetThemeSysSizeFunc) GetProcAddress (uxtheme_dll, "GetThemeSysSize");
      is_theme_partially_transparent_func = (IsThemeBackgroundPartiallyTransparentFunc) GetProcAddress (uxtheme_dll, "IsThemeBackgroundPartiallyTransparent");
      draw_theme_parent_background_func = (DrawThemeParentBackgroundFunc) GetProcAddress (uxtheme_dll, "DrawThemeParentBackground");
      get_theme_part_size_func = (GetThemePartSizeFunc) GetProcAddress (uxtheme_dll, "GetThemePartSize");
    }

  if (is_app_themed_func && is_theme_active_func)
    {
      use_xp_theme = (is_app_themed_func () && is_theme_active_func ());
    }
  else
    {
      use_xp_theme = FALSE;
    }
}

void
xp_theme_reset (void)
{
  xp_theme_close_open_handles ();

  if (is_app_themed_func && is_theme_active_func)
    {
      use_xp_theme = (is_app_themed_func () && is_theme_active_func ());
    }
  else
    {
      use_xp_theme = FALSE;
    }
}

void
xp_theme_exit (void)
{
  if (!uxtheme_dll)
    return;

  xp_theme_close_open_handles ();

  FreeLibrary (uxtheme_dll);
  uxtheme_dll = NULL;
  use_xp_theme = FALSE;

  is_app_themed_func = NULL;
  is_theme_active_func = NULL;
  open_theme_data_func = NULL;
  close_theme_data_func = NULL;
  draw_theme_background_func = NULL;
  enable_theme_dialog_texture_func = NULL;
  get_theme_sys_font_func = NULL;
  get_theme_sys_color_func = NULL;
  get_theme_sys_metric_func = NULL;
  is_theme_partially_transparent_func = NULL;
  draw_theme_parent_background_func = NULL;
  get_theme_part_size_func = NULL;
}

static HTHEME
xp_theme_get_handle_by_class (XpThemeClass klazz)
{
  if (!open_themes[klazz] && open_theme_data_func)
    {
      open_themes[klazz] = open_theme_data_func (NULL, class_descriptors[klazz]);
    }

  return open_themes[klazz];
}

static HTHEME
xp_theme_get_handle_by_element (XpThemeElement element)
{
  HTHEME ret = NULL;
  XpThemeClass klazz = XP_THEME_CLASS__SIZEOF;

  switch (element)
    {
    case XP_THEME_ELEMENT_TOOLTIP:
      klazz = XP_THEME_CLASS_TOOLTIP;
      break;

    case XP_THEME_ELEMENT_REBAR:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_H:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_V:
    case XP_THEME_ELEMENT_REBAR_CHEVRON:
      klazz = XP_THEME_CLASS_REBAR;
      break;

    case XP_THEME_ELEMENT_SCALE_TROUGH_H:
    case XP_THEME_ELEMENT_SCALE_TROUGH_V:
    case XP_THEME_ELEMENT_SCALE_SLIDER_H:
    case XP_THEME_ELEMENT_SCALE_SLIDER_V:
    case XP_THEME_ELEMENT_SCALE_TICS_H:
    case XP_THEME_ELEMENT_SCALE_TICS_V:
      klazz = XP_THEME_CLASS_TRACKBAR;
      break;

    case XP_THEME_ELEMENT_STATUS_GRIPPER:
    case XP_THEME_ELEMENT_STATUS_PANE:
      klazz = XP_THEME_CLASS_STATUS;
      break;

    case XP_THEME_ELEMENT_TOOLBAR_BUTTON:
    case XP_THEME_ELEMENT_TOOLBAR_SEPARATOR_H:
    case XP_THEME_ELEMENT_TOOLBAR_SEPARATOR_V:
      klazz = XP_THEME_CLASS_TOOLBAR;
      break;

    case XP_THEME_ELEMENT_MENU_ITEM:
    case XP_THEME_ELEMENT_MENU_SEPARATOR:
      klazz = XP_THEME_CLASS_MENU;
      break;

    case XP_THEME_ELEMENT_PRESSED_CHECKBOX:
    case XP_THEME_ELEMENT_INCONSISTENT_CHECKBOX:
    case XP_THEME_ELEMENT_CHECKBOX:
    case XP_THEME_ELEMENT_BUTTON:
    case XP_THEME_ELEMENT_DEFAULT_BUTTON:
    case XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON:
    case XP_THEME_ELEMENT_RADIO_BUTTON:
      klazz = XP_THEME_CLASS_BUTTON;
      break;

    case XP_THEME_ELEMENT_LIST_HEADER:
      klazz = XP_THEME_CLASS_HEADER;
      break;

    case XP_THEME_ELEMENT_COMBOBUTTON:
      klazz = XP_THEME_CLASS_COMBOBOX;
      break;

    case XP_THEME_ELEMENT_BODY:
    case XP_THEME_ELEMENT_TAB_ITEM:
    case XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE:
    case XP_THEME_ELEMENT_TAB_ITEM_RIGHT_EDGE:
    case XP_THEME_ELEMENT_TAB_ITEM_BOTH_EDGE:
    case XP_THEME_ELEMENT_TAB_PANE:
      klazz = XP_THEME_CLASS_TAB;
      break;

    case XP_THEME_ELEMENT_SCROLLBAR_V:
    case XP_THEME_ELEMENT_SCROLLBAR_H:
    case XP_THEME_ELEMENT_ARROW_UP:
    case XP_THEME_ELEMENT_ARROW_DOWN:
    case XP_THEME_ELEMENT_ARROW_LEFT:
    case XP_THEME_ELEMENT_ARROW_RIGHT:
    case XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V:
    case XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H:
    case XP_THEME_ELEMENT_TROUGH_V:
    case XP_THEME_ELEMENT_TROUGH_H:
      klazz = XP_THEME_CLASS_SCROLLBAR;
      break;

    case XP_THEME_ELEMENT_EDIT_TEXT:
      klazz = XP_THEME_CLASS_EDIT;
      break;

    case XP_THEME_ELEMENT_SPIN_BUTTON_UP:
    case XP_THEME_ELEMENT_SPIN_BUTTON_DOWN:
      klazz = XP_THEME_CLASS_SPIN;
      break;

    case XP_THEME_ELEMENT_PROGRESS_BAR_H:
    case XP_THEME_ELEMENT_PROGRESS_BAR_V:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_H:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_V:
      klazz = XP_THEME_CLASS_PROGRESS;
      break;

    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED:
    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED:
      klazz = XP_THEME_CLASS_TREEVIEW;
      break;

    case XP_THEME_ELEMENT_LINE_H:
    case XP_THEME_ELEMENT_LINE_V:
      klazz = XP_THEME_CLASS_GLOBALS;
      break;

    default:
      break;
    }

  if (klazz != XP_THEME_CLASS__SIZEOF)
    {
      ret = xp_theme_get_handle_by_class (klazz);
    }

  return ret;
}

static int
xp_theme_map_gtk_state (XpThemeElement element, GtkStateType state)
{
  int ret = 0;

  switch (element)
    {
    case XP_THEME_ELEMENT_TOOLTIP:
      ret = TTSS_NORMAL;
      break;

    case XP_THEME_ELEMENT_REBAR:
      ret = 0;
      break;

    case XP_THEME_ELEMENT_REBAR_GRIPPER_H:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_V:
      ret = 0;
      break;

    case XP_THEME_ELEMENT_STATUS_GRIPPER:
    case XP_THEME_ELEMENT_STATUS_PANE:
      ret = 1;
      break;

    case XP_THEME_ELEMENT_REBAR_CHEVRON:
      switch (state)
	{
	case GTK_STATE_PRELIGHT:
	  ret = CHEVS_HOT;
	  break;

	case GTK_STATE_SELECTED:
	case GTK_STATE_ACTIVE:
	  ret = CHEVS_PRESSED;
	  break;

	default:
	  ret = CHEVS_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_TOOLBAR_SEPARATOR_H:
    case XP_THEME_ELEMENT_TOOLBAR_SEPARATOR_V:
      ret = TS_NORMAL;
      break;

    case XP_THEME_ELEMENT_TOOLBAR_BUTTON:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = TS_PRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = TS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = TS_DISABLED;
	  break;

	default:
	  ret = TS_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_TAB_PANE:
      ret = 1;
      break;

    case XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE:
    case XP_THEME_ELEMENT_TAB_ITEM_RIGHT_EDGE:
    case XP_THEME_ELEMENT_TAB_ITEM_BOTH_EDGE:
    case XP_THEME_ELEMENT_TAB_ITEM:
      switch (state)
	{
	case GTK_STATE_PRELIGHT:
	  ret = TIS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = TIS_DISABLED;
	  break;

	case GTK_STATE_SELECTED:
	case GTK_STATE_ACTIVE:
	  ret = TIS_NORMAL;
	  break;

	default:
	  ret = TIS_SELECTED;
	}
      break;

    case XP_THEME_ELEMENT_EDIT_TEXT:
      switch (state)
	{
	case GTK_STATE_PRELIGHT:
	  ret = ETS_FOCUSED;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = ETS_READONLY;
	  break;

	case GTK_STATE_SELECTED:
	case GTK_STATE_ACTIVE:
	default:
	  ret = ETS_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_TROUGH_H:
    case XP_THEME_ELEMENT_TROUGH_V:
      ret = SCRBS_NORMAL;
      break;

    case XP_THEME_ELEMENT_SCROLLBAR_H:
    case XP_THEME_ELEMENT_SCROLLBAR_V:
      switch (state)
	{
	case GTK_STATE_SELECTED:
	case GTK_STATE_ACTIVE:
	  ret = SCRBS_PRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = SCRBS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = SCRBS_DISABLED;
	  break;

	default:
	  ret = SCRBS_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_ARROW_DOWN:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = ABS_DOWNPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = ABS_DOWNHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = ABS_DOWNDISABLED;
	  break;

	default:
	  ret = ABS_DOWNNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_ARROW_UP:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = ABS_UPPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = ABS_UPHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = ABS_UPDISABLED;
	  break;

	default:
	  ret = ABS_UPNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_ARROW_LEFT:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = ABS_LEFTPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = ABS_LEFTHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = ABS_LEFTDISABLED;
	  break;

	default:
	  ret = ABS_LEFTNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_ARROW_RIGHT:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = ABS_RIGHTPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = ABS_RIGHTHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = ABS_RIGHTDISABLED;
	  break;

	default:
	  ret = ABS_RIGHTNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_CHECKBOX:
    case XP_THEME_ELEMENT_RADIO_BUTTON:
      switch (state)
	{
	case GTK_STATE_SELECTED:
	  ret = CBS_UNCHECKEDPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = CBS_UNCHECKEDHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = CBS_UNCHECKEDDISABLED;
	  break;

	default:
	  ret = CBS_UNCHECKEDNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_INCONSISTENT_CHECKBOX:
      switch (state)
	{
	case GTK_STATE_SELECTED:
	  ret = CBS_MIXEDPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = CBS_MIXEDHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = CBS_MIXEDDISABLED;
	  break;

	default:
	  ret = CBS_MIXEDNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_PRESSED_CHECKBOX:
    case XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON:
      switch (state)
	{
	case GTK_STATE_SELECTED:
	  ret = CBS_CHECKEDPRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = CBS_CHECKEDHOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = CBS_CHECKEDDISABLED;
	  break;

	default:
	  ret = CBS_CHECKEDNORMAL;
	}
      break;

    case XP_THEME_ELEMENT_DEFAULT_BUTTON:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = PBS_PRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = PBS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = PBS_DISABLED;
	  break;

	default:
	  ret = PBS_DEFAULTED;
	}
      break;

    case XP_THEME_ELEMENT_SPIN_BUTTON_DOWN:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = DNS_PRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = DNS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = DNS_DISABLED;
	  break;

	default:
	  ret = DNS_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_SPIN_BUTTON_UP:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = UPS_PRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = UPS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = UPS_DISABLED;
	  break;

	default:
	  ret = UPS_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED:
      ret = GLPS_OPENED;
      break;

    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED:
      ret = GLPS_CLOSED;
      break;

    case XP_THEME_ELEMENT_PROGRESS_BAR_H:
    case XP_THEME_ELEMENT_PROGRESS_BAR_V:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_H:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_V:
      ret = 1;
      break;

    case XP_THEME_ELEMENT_MENU_SEPARATOR:
      ret = TS_NORMAL;
      break;

    case XP_THEME_ELEMENT_MENU_ITEM:
      switch (state)
	{
	case GTK_STATE_SELECTED:
	  ret = MS_SELECTED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = MBI_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = MBI_DISABLED;
	  break;

	default:
	  ret = MBI_NORMAL;
	}
      break;

    case XP_THEME_ELEMENT_LINE_H:
      switch (state)
	{
	  /* LHS_FLAT, LHS_RAISED, LHS_SUNKEN */
	  ret = LHS_RAISED;
	  break;
	}
      break;

    case XP_THEME_ELEMENT_LINE_V:
      switch (state)
	{
	  /* LVS_FLAT, LVS_RAISED, LVS_SUNKEN */
	  ret = LVS_RAISED;
	  break;
	}
      break;

    case XP_THEME_ELEMENT_SCALE_TROUGH_H:
    case XP_THEME_ELEMENT_SCALE_TROUGH_V:
      ret = TRS_NORMAL;
      break;

    default:
      switch (state)
	{
	case GTK_STATE_ACTIVE:
	  ret = PBS_PRESSED;
	  break;

	case GTK_STATE_PRELIGHT:
	  ret = PBS_HOT;
	  break;

	case GTK_STATE_INSENSITIVE:
	  ret = PBS_DISABLED;
	  break;

	default:
	  ret = PBS_NORMAL;
	}
    }

  return ret;
}

HDC
get_window_dc (GtkStyle *style,
	       GdkWindow *window,
	       GtkStateType state_type,
	       XpDCInfo *dc_info_out,
	       gint x, gint y, gint width, gint height,
	       RECT *rect_out)
{
  GdkDrawable *drawable = NULL;
  GdkGC *gc = style->dark_gc[state_type];
  gint x_offset, y_offset;
  
  dc_info_out->data = NULL;
  
  drawable = gdk_win32_begin_direct_draw_libgtk_only (window,
						      gc, &dc_info_out->data,
						      &x_offset, &y_offset);
  if (!drawable)
    return NULL;

  rect_out->left = x - x_offset;
  rect_out->top = y - y_offset;
  rect_out->right = rect_out->left + width;
  rect_out->bottom = rect_out->top + height;
  
  dc_info_out->drawable = drawable;
  dc_info_out->gc = gc;
  dc_info_out->x_offset = x_offset;
  dc_info_out->y_offset = y_offset;
  
  return gdk_win32_hdc_get (drawable, gc, 0);
}

void
release_window_dc (XpDCInfo *dc_info)
{
  gdk_win32_hdc_release (dc_info->drawable, dc_info->gc, 0);

  gdk_win32_end_direct_draw_libgtk_only (dc_info->data);
}

gboolean
xp_theme_draw (GdkWindow *win, XpThemeElement element, GtkStyle *style,
	       int x, int y, int width, int height,
	       GtkStateType state_type, GdkRectangle *area)
{
  HTHEME theme;
  RECT rect, clip, *pClip;
  HDC dc;
  XpDCInfo dc_info;
  int part_state;
  HWND hwnd;

  if (!xp_theme_is_drawable (element))
    return FALSE;

  theme = xp_theme_get_handle_by_element (element);
  if (!theme)
    return FALSE;

  /* FIXME: Recheck its function */
  hwnd = gdk_win32_window_get_impl_hwnd (win);
  if (hwnd != NULL)
    enable_theme_dialog_texture_func (hwnd, ETDT_ENABLETAB);

  dc = get_window_dc (style, win, state_type, &dc_info,
		      x, y, width, height,
		      &rect);
  if (!dc)
    return FALSE;

  if (area)
    {
      clip.left = area->x - dc_info.x_offset;
      clip.top = area->y - dc_info.y_offset;
      clip.right = clip.left + area->width;
      clip.bottom = clip.top + area->height;

      pClip = &clip;
    }
  else
    {
      pClip = NULL;
    }

  part_state = xp_theme_map_gtk_state (element, state_type);

  /* Support transparency */
  if (is_theme_partially_transparent_func (theme, element_part_map[element], part_state))
    draw_theme_parent_background_func (hwnd, dc, pClip);

  draw_theme_background_func (theme, dc, element_part_map[element],
			      part_state, &rect, pClip);

  release_window_dc (&dc_info);

  return TRUE;
}

gboolean
xp_theme_is_active (void)
{
  return use_xp_theme;
}

gboolean
xp_theme_is_drawable (XpThemeElement element)
{
  if (xp_theme_is_active ())
    return (xp_theme_get_handle_by_element (element) != NULL);

  return FALSE;
}

gboolean
xp_theme_get_element_dimensions (XpThemeElement element,
				 GtkStateType state_type,
				 gint *cx, gint *cy)
{
  HTHEME theme;
  SIZE part_size;
  int part_state;

  if (!xp_theme_is_active ())
    return FALSE;

  theme = xp_theme_get_handle_by_element (element);
  if (!theme)
    return FALSE;

  part_state = xp_theme_map_gtk_state (element, state_type);

  get_theme_part_size_func (theme,
			    NULL,
			    element_part_map[element],
			    part_state,
			    NULL,
			    TS_MIN,
			    &part_size);

  *cx = part_size.cx;
  *cy = part_size.cy;

  if (element == XP_THEME_ELEMENT_MENU_ITEM ||
      element == XP_THEME_ELEMENT_MENU_SEPARATOR)
  {
    SIZE gutter_size;

    get_theme_part_size_func (theme,
			      NULL,
			      MENU_POPUPGUTTER,
			      0,
			      NULL,
			      TS_MIN,
			      &gutter_size);

	*cx += gutter_size.cx * 2;
	*cy += gutter_size.cy * 2;
  }

  return TRUE;
}

gboolean
xp_theme_get_system_font (XpThemeClass klazz, XpThemeFont fontId,
			  OUT LOGFONTW *lf)
{
  if (xp_theme_is_active () && get_theme_sys_font_func != NULL)
    {
      HTHEME theme = xp_theme_get_handle_by_class (klazz);
      int themeFont;

      if (!theme)
	return FALSE;

      switch (fontId)
	{
	case XP_THEME_FONT_CAPTION:
	  themeFont = TMT_CAPTIONFONT;
	  break;

	case XP_THEME_FONT_MENU:
	  themeFont = TMT_MENUFONT;
	  break;

	case XP_THEME_FONT_STATUS:
	  themeFont = TMT_STATUSFONT;
	  break;

	case XP_THEME_FONT_MESSAGE:
	default:
	  themeFont = TMT_MSGBOXFONT;
	  break;
	}

      /* if theme is NULL, it will just return the GetSystemFont()
         value */
      return ((*get_theme_sys_font_func) (theme, themeFont, lf) == S_OK);
    }

  return FALSE;
}

gboolean
xp_theme_get_system_color (XpThemeClass klazz, int colorId,
			   OUT DWORD *pColor)
{
  if (xp_theme_is_active () && get_theme_sys_color_func != NULL)
    {
      HTHEME theme = xp_theme_get_handle_by_class (klazz);

      /* if theme is NULL, it will just return the GetSystemColor()
         value */
      *pColor = (*get_theme_sys_color_func) (theme, colorId);
      return TRUE;
    }

  return FALSE;
}

gboolean
xp_theme_get_system_metric (XpThemeClass klazz, int metricId, OUT int *pVal)
{
  if (xp_theme_is_active () && get_theme_sys_metric_func != NULL)
    {
      HTHEME theme = xp_theme_get_handle_by_class (klazz);

      /* if theme is NULL, it will just return the GetSystemMetrics()
         value */
      *pVal = (*get_theme_sys_metric_func) (theme, metricId);
      return TRUE;
    }

  return FALSE;
}
