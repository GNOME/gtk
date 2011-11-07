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

#ifndef XP_THEME_H
#define XP_THEME_H

#include <windows.h>

#include "gtk/gtk.h"

typedef enum
{
  XP_THEME_CLASS_SCROLLBAR = 0,
  XP_THEME_CLASS_BUTTON,
  XP_THEME_CLASS_HEADER,
  XP_THEME_CLASS_COMBOBOX,
  XP_THEME_CLASS_TAB,
  XP_THEME_CLASS_EDIT,
  XP_THEME_CLASS_TREEVIEW,
  XP_THEME_CLASS_SPIN,
  XP_THEME_CLASS_PROGRESS,
  XP_THEME_CLASS_TOOLTIP,
  XP_THEME_CLASS_REBAR,
  XP_THEME_CLASS_TOOLBAR,
  XP_THEME_CLASS_GLOBALS,
  XP_THEME_CLASS_MENU,
  XP_THEME_CLASS_WINDOW,
  XP_THEME_CLASS_STATUS,
  XP_THEME_CLASS_TRACKBAR,
  XP_THEME_CLASS__SIZEOF
} XpThemeClass;

typedef enum
{
  XP_THEME_ELEMENT_PRESSED_CHECKBOX = 0,
  XP_THEME_ELEMENT_CHECKBOX,
  XP_THEME_ELEMENT_INCONSISTENT_CHECKBOX,
  XP_THEME_ELEMENT_BUTTON,
  XP_THEME_ELEMENT_LIST_HEADER,
  XP_THEME_ELEMENT_COMBOBUTTON,
  XP_THEME_ELEMENT_BODY,
  XP_THEME_ELEMENT_TAB_ITEM,
  XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE,
  XP_THEME_ELEMENT_TAB_ITEM_RIGHT_EDGE,
  XP_THEME_ELEMENT_TAB_ITEM_BOTH_EDGE,
  XP_THEME_ELEMENT_TAB_PANE,
  XP_THEME_ELEMENT_SCROLLBAR_H,
  XP_THEME_ELEMENT_SCROLLBAR_V,
  XP_THEME_ELEMENT_ARROW_UP,
  XP_THEME_ELEMENT_ARROW_DOWN,
  XP_THEME_ELEMENT_ARROW_LEFT,
  XP_THEME_ELEMENT_ARROW_RIGHT,
  XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H,
  XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V,
  XP_THEME_ELEMENT_TROUGH_H,
  XP_THEME_ELEMENT_TROUGH_V,
  XP_THEME_ELEMENT_EDIT_TEXT,
  XP_THEME_ELEMENT_DEFAULT_BUTTON,
  XP_THEME_ELEMENT_SPIN_BUTTON_UP,
  XP_THEME_ELEMENT_SPIN_BUTTON_DOWN,
  XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON,
  XP_THEME_ELEMENT_RADIO_BUTTON,
  XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED,
  XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED,
  XP_THEME_ELEMENT_PROGRESS_BAR_H,
  XP_THEME_ELEMENT_PROGRESS_BAR_V,
  XP_THEME_ELEMENT_PROGRESS_TROUGH_H,
  XP_THEME_ELEMENT_PROGRESS_TROUGH_V,
  XP_THEME_ELEMENT_TOOLTIP,
  XP_THEME_ELEMENT_REBAR,
  XP_THEME_ELEMENT_REBAR_GRIPPER_H,
  XP_THEME_ELEMENT_REBAR_GRIPPER_V,
  XP_THEME_ELEMENT_REBAR_CHEVRON,
  XP_THEME_ELEMENT_TOOLBAR_BUTTON,
  XP_THEME_ELEMENT_MENU_ITEM,
  XP_THEME_ELEMENT_MENU_SEPARATOR,
  XP_THEME_ELEMENT_STATUS_GRIPPER,
  XP_THEME_ELEMENT_STATUS_PANE,
  XP_THEME_ELEMENT_LINE_H,
  XP_THEME_ELEMENT_LINE_V,
  XP_THEME_ELEMENT_TOOLBAR_SEPARATOR_H,
  XP_THEME_ELEMENT_TOOLBAR_SEPARATOR_V,
  XP_THEME_ELEMENT_SCALE_TROUGH_H,
  XP_THEME_ELEMENT_SCALE_TROUGH_V,
  XP_THEME_ELEMENT_SCALE_SLIDER_H,
  XP_THEME_ELEMENT_SCALE_SLIDER_V,
  XP_THEME_ELEMENT_SCALE_TICS_H,
  XP_THEME_ELEMENT_SCALE_TICS_V,
  XP_THEME_ELEMENT__SIZEOF
} XpThemeElement;

typedef enum
{
  XP_THEME_FONT_CAPTION,
  XP_THEME_FONT_MENU,
  XP_THEME_FONT_STATUS,
  XP_THEME_FONT_MESSAGE
} XpThemeFont;

typedef struct
{
  GdkDrawable *drawable;
  GdkGC *gc;
  
  gint x_offset;
  gint y_offset;
  
  /*< private >*/
  gpointer data;
} XpDCInfo;

HDC get_window_dc (GtkStyle *style,
		   GdkWindow *window,
		   GtkStateType state_type,
		   XpDCInfo *dc_info_out,
		   gint x, gint y, gint width, gint height,
		   RECT *rect_out);
void release_window_dc (XpDCInfo *dc_info);

void xp_theme_init (void);
void xp_theme_reset (void);
void xp_theme_exit (void);
gboolean xp_theme_draw (GdkWindow *win, XpThemeElement element,
                        GtkStyle *style, int x, int y, int width,
                        int height, GtkStateType state_type,
                        GdkRectangle *area);
gboolean xp_theme_is_drawable (XpThemeElement element);
gboolean xp_theme_get_element_dimensions (XpThemeElement element,
                                          GtkStateType state_type,
                                          gint *cx, gint *cy);
gboolean xp_theme_get_system_font (XpThemeClass klazz, XpThemeFont fontId, OUT LOGFONTW *lf);
gboolean xp_theme_get_system_color (XpThemeClass klazz, int colorId, OUT DWORD *pColor);
gboolean xp_theme_get_system_metric (XpThemeClass klazz, int metricId, OUT int *pVal);

gboolean xp_theme_is_active (void);

#endif /* XP_THEME_H */
