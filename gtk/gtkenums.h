/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
#ifndef __GTK_ENUMS_H__
#define __GTK_ENUMS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Widget states */
typedef enum
{
  GTK_STATE_NORMAL,
  GTK_STATE_ACTIVE,
  GTK_STATE_PRELIGHT,
  GTK_STATE_SELECTED,
  GTK_STATE_INSENSITIVE
} GtkStateType;

/* Window types */
typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_DIALOG,
  GTK_WINDOW_POPUP
} GtkWindowType;

/* Focus movement types */
typedef enum
{
  GTK_DIR_TAB_FORWARD,
  GTK_DIR_TAB_BACKWARD,
  GTK_DIR_UP,
  GTK_DIR_DOWN,
  GTK_DIR_LEFT,
  GTK_DIR_RIGHT
} GtkDirectionType;

/* Shadow types */
typedef enum
{
  GTK_SHADOW_NONE,
  GTK_SHADOW_IN,
  GTK_SHADOW_OUT,
  GTK_SHADOW_ETCHED_IN,
  GTK_SHADOW_ETCHED_OUT
} GtkShadowType;

/* Arrow types */
typedef enum
{
  GTK_ARROW_UP,
  GTK_ARROW_DOWN,
  GTK_ARROW_LEFT,
  GTK_ARROW_RIGHT
} GtkArrowType;

/* Packing types (for boxes) */
typedef enum
{
  GTK_PACK_START,
  GTK_PACK_END
} GtkPackType;

/* Scrollbar policy types (for scrolled windows) */
typedef enum
{
  GTK_POLICY_ALWAYS,
  GTK_POLICY_AUTOMATIC
} GtkPolicyType;

/* Data update types (for ranges) */
typedef enum
{
  GTK_UPDATE_CONTINUOUS,
  GTK_UPDATE_DISCONTINUOUS,
  GTK_UPDATE_DELAYED
} GtkUpdateType;

/* Attach options (for tables) */
typedef enum
{
  GTK_EXPAND = 1 << 0,
  GTK_SHRINK = 1 << 1,
  GTK_FILL   = 1 << 2
} GtkAttachOptions;

typedef enum
{
  GTK_RUN_FIRST      = 0x1,
  GTK_RUN_LAST       = 0x2,
  GTK_RUN_BOTH       = 0x3,
  GTK_RUN_MASK       = 0xF,
  GTK_RUN_NO_RECURSE = 0x10
} GtkSignalRunType;

typedef enum
{
  GTK_WIN_POS_NONE,
  GTK_WIN_POS_CENTER,
  GTK_WIN_POS_MOUSE
} GtkWindowPosition;

typedef enum
{
  GTK_DIRECTION_LEFT,
  GTK_DIRECTION_RIGHT
} GtkSubmenuDirection;

typedef enum
{
  GTK_TOP_BOTTOM,
  GTK_LEFT_RIGHT
} GtkSubmenuPlacement;

typedef enum
{
  GTK_MENU_FACTORY_MENU,
  GTK_MENU_FACTORY_MENU_BAR,
  GTK_MENU_FACTORY_OPTION_MENU
} GtkMenuFactoryType;

typedef enum
{
  GTK_PIXELS,
  GTK_INCHES,
  GTK_CENTIMETERS
} GtkMetricType;

typedef enum
{
  GTK_SCROLL_NONE,
  GTK_SCROLL_STEP_BACKWARD,
  GTK_SCROLL_STEP_FORWARD,
  GTK_SCROLL_PAGE_BACKWARD,
  GTK_SCROLL_PAGE_FORWARD,
  GTK_SCROLL_JUMP
} GtkScrollType;

typedef enum
{
  GTK_TROUGH_NONE,
  GTK_TROUGH_START,
  GTK_TROUGH_END,
  GTK_TROUGH_JUMP
} GtkTroughType;

typedef enum
{
  GTK_POS_LEFT,
  GTK_POS_RIGHT,
  GTK_POS_TOP,
  GTK_POS_BOTTOM
} GtkPositionType;

typedef enum
{
  GTK_PREVIEW_COLOR,
  GTK_PREVIEW_GRAYSCALE
} GtkPreviewType;

/* justification for label and maybe other widgets (text?) */
typedef enum
{
  GTK_JUSTIFY_LEFT,
  GTK_JUSTIFY_RIGHT,
  GTK_JUSTIFY_CENTER,
  GTK_JUSTIFY_FILL
} GtkJustification;

/* list selection modes */
typedef enum
{
  GTK_SELECTION_SINGLE,
  GTK_SELECTION_BROWSE,
  GTK_SELECTION_MULTIPLE,
  GTK_SELECTION_EXTENDED
} GtkSelectionMode;

/* Orientation for toolbars, etc. */
typedef enum
{
  GTK_ORIENTATION_HORIZONTAL,
  GTK_ORIENTATION_VERTICAL
} GtkOrientation;

/* Style for toolbars */
typedef enum
{
  GTK_TOOLBAR_ICONS,
  GTK_TOOLBAR_TEXT,
  GTK_TOOLBAR_BOTH
} GtkToolbarStyle;

/* Generic visibility flags */
typedef enum
{
  GTK_VISIBILITY_NONE,
  GTK_VISIBILITY_PARTIAL,
  GTK_VISIBILITY_FULL
} GtkVisibility;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENUMS_H__ */
