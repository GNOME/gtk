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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_ENUMS_H__
#define __GTK_ENUMS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

/* Menu keyboard movement types */
typedef enum
{
  GTK_MENU_DIR_PARENT,
  GTK_MENU_DIR_CHILD,
  GTK_MENU_DIR_NEXT,
  GTK_MENU_DIR_PREV
} GtkMenuDirectionType;

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

/* Placement type for scrolled window */
typedef enum
{
  GTK_CORNER_TOP_LEFT,
  GTK_CORNER_BOTTOM_LEFT,
  GTK_CORNER_TOP_RIGHT,
  GTK_CORNER_BOTTOM_RIGHT
} GtkCornerType;

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
  GTK_POLICY_AUTOMATIC,
  GTK_POLICY_NEVER
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
  GTK_RELIEF_HALF,
  GTK_RELIEF_NONE
} GtkReliefStyle;

/* Resize type */
typedef enum
{
  GTK_RESIZE_PARENT,		/* Pass resize request to the parent */
  GTK_RESIZE_QUEUE,		/* Queue resizes on this widget */
  GTK_RESIZE_IMMEDIATE		/* Perform the resizes now */
} GtkResizeMode;

/* signal run types */
typedef enum			/*< flags >*/
{
  GTK_RUN_FIRST      = 1 << 0,
  GTK_RUN_LAST       = 1 << 1,
  GTK_RUN_BOTH       = (GTK_RUN_FIRST | GTK_RUN_LAST),
  GTK_RUN_NO_RECURSE = 1 << 2,
  GTK_RUN_ACTION     = 1 << 3,
  GTK_RUN_NO_HOOKS   = 1 << 4
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
  GTK_WIN_POS_MOUSE,
  GTK_WIN_POS_CENTER_ALWAYS
} GtkWindowPosition;

/* Window types */
typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_DIALOG,
  GTK_WINDOW_POPUP
} GtkWindowType;

/* How to sort */
typedef enum
{
  GTK_SORT_ASCENDING,
  GTK_SORT_DESCENDING
} GtkSortType;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENUMS_H__ */
