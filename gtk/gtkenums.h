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
#pragma }
#endif /* __cplusplus */

typedef enum
{
  /* should the accelerator appear in
   * the widget's display?
   */
  GTK_ACCEL_VISIBLE        = 1 << 0,
  /* should the signal associated with
   * this accelerator be also visible?
   */
  GTK_ACCEL_SIGNAL_VISIBLE = 1 << 1,
  /* may the accelerator be removed
   * again?
   */
  GTK_ACCEL_LOCKED         = 1 << 2,
  GTK_ACCEL_MASK           = 0x07
} GtkAccelFlags;

/* Arrow types */
typedef enum
{
  GTK_ARROW_UP,
  GTK_ARROW_DOWN,
  GTK_ARROW_LEFT,
  GTK_ARROW_RIGHT
} GtkArrowType;

/* Attach options (for tables) */
typedef enum
{
  GTK_EXPAND = 1 << 0,
  GTK_SHRINK = 1 << 1,
  GTK_FILL   = 1 << 2
} GtkAttachOptions;

/* Button box styles */
typedef enum 
{
  GTK_BUTTONBOX_DEFAULT_STYLE,
  GTK_BUTTONBOX_SPREAD,
  GTK_BUTTONBOX_EDGE,
  GTK_BUTTONBOX_START,
  GTK_BUTTONBOX_END
} GtkButtonBoxStyle;

/* Curve types */
typedef enum
{
  GTK_CURVE_TYPE_LINEAR,       /* linear interpolation */
  GTK_CURVE_TYPE_SPLINE,       /* spline interpolation */
  GTK_CURVE_TYPE_FREE          /* free form curve */
} GtkCurveType;
 
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

/* justification for label and maybe other widgets (text?) */
typedef enum
{
  GTK_JUSTIFY_LEFT,
  GTK_JUSTIFY_RIGHT,
  GTK_JUSTIFY_CENTER,
  GTK_JUSTIFY_FILL
} GtkJustification;

/* GtkPatternSpec match types */
typedef enum
{
  GTK_MATCH_ALL,       /* "*A?A*" */
  GTK_MATCH_ALL_TAIL,  /* "*A?AA" */
  GTK_MATCH_HEAD,      /* "AAAA*" */
  GTK_MATCH_TAIL,      /* "*AAAA" */
  GTK_MATCH_EXACT,     /* "AAAAA" */
  GTK_MATCH_LAST
} GtkMatchType;

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

/* Orientation for toolbars, etc. */
typedef enum
{
  GTK_ORIENTATION_HORIZONTAL,
  GTK_ORIENTATION_VERTICAL
} GtkOrientation;

/* Packing types (for boxes) */
typedef enum
{
  GTK_PACK_START,
  GTK_PACK_END
} GtkPackType;

/* priorities for path lookups */
typedef enum
{
  GTK_PATH_PRIO_LOWEST      = 0,
  GTK_PATH_PRIO_GTK	    = 4,
  GTK_PATH_PRIO_APPLICATION = 8,
  GTK_PATH_PRIO_RC          = 12,
  GTK_PATH_PRIO_HIGHEST     = 15,
  GTK_PATH_PRIO_MASK        = 0x0f
} GtkPathPriorityType;

/* widget path types */
typedef enum
{
  GTK_PATH_WIDGET,
  GTK_PATH_WIDGET_CLASS,
  GTK_PATH_CLASS
} GtkPathType;

/* Scrollbar policy types (for scrolled windows) */
typedef enum
{
  GTK_POLICY_ALWAYS,
  GTK_POLICY_AUTOMATIC
} GtkPolicyType;

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

/* Style for buttons */
typedef enum
{
  GTK_RELIEF_NORMAL,
  GTK_RELIEF_NONE
} GtkReliefStyle;

/* signal run types */
typedef enum			/*< flags >*/
{
  GTK_RUN_FIRST      = 0x1,
  GTK_RUN_LAST       = 0x2,
  GTK_RUN_BOTH       = 0x3,
  GTK_RUN_MASK       = 0xF,
  GTK_RUN_NO_RECURSE = 0x10,
  GTK_RUN_ACTION  = 0x20
} GtkSignalRunType;

/* scrolling types */
typedef enum
{
  GTK_SCROLL_NONE,
  GTK_SCROLL_STEP_BACKWARD,
  GTK_SCROLL_STEP_FORWARD,
  GTK_SCROLL_PAGE_BACKWARD,
  GTK_SCROLL_PAGE_FORWARD,
  GTK_SCROLL_JUMP
} GtkScrollType;

/* list selection modes */
typedef enum
{
  GTK_SELECTION_SINGLE,
  GTK_SELECTION_BROWSE,
  GTK_SELECTION_MULTIPLE,
  GTK_SELECTION_EXTENDED
} GtkSelectionMode;

/* Shadow types */
typedef enum
{
  GTK_SHADOW_NONE,
  GTK_SHADOW_IN,
  GTK_SHADOW_OUT,
  GTK_SHADOW_ETCHED_IN,
  GTK_SHADOW_ETCHED_OUT
} GtkShadowType;

/* Widget states */
typedef enum
{
  GTK_STATE_NORMAL,
  GTK_STATE_ACTIVE,
  GTK_STATE_PRELIGHT,
  GTK_STATE_SELECTED,
  GTK_STATE_INSENSITIVE
} GtkStateType;

/* Directions for submenus */
typedef enum
{
  GTK_DIRECTION_LEFT,
  GTK_DIRECTION_RIGHT
} GtkSubmenuDirection;

/* Placement of submenus */
typedef enum
{
  GTK_TOP_BOTTOM,
  GTK_LEFT_RIGHT
} GtkSubmenuPlacement;

/* Style for toolbars */
typedef enum
{
  GTK_TOOLBAR_ICONS,
  GTK_TOOLBAR_TEXT,
  GTK_TOOLBAR_BOTH
} GtkToolbarStyle;

/* Trough types for GtkRange */
typedef enum
{
  GTK_TROUGH_NONE,
  GTK_TROUGH_START,
  GTK_TROUGH_END,
  GTK_TROUGH_JUMP
} GtkTroughType;

/* Data update types (for ranges) */
typedef enum
{
  GTK_UPDATE_CONTINUOUS,
  GTK_UPDATE_DISCONTINUOUS,
  GTK_UPDATE_DELAYED
} GtkUpdateType;

/* Generic visibility flags */
typedef enum
{
  GTK_VISIBILITY_NONE,
  GTK_VISIBILITY_PARTIAL,
  GTK_VISIBILITY_FULL
} GtkVisibility;

/* Window position types */
typedef enum
{
  GTK_WIN_POS_NONE,
  GTK_WIN_POS_CENTER,
  GTK_WIN_POS_MOUSE
} GtkWindowPosition;

/* Window types */
typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_DIALOG,
  GTK_WINDOW_POPUP
} GtkWindowType;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENUMS_H__ */
