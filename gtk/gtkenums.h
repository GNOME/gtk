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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_ENUMS_H__
#define __GTK_ENUMS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


typedef enum
{
  /* should the accelerator appear in
   * the widget's display?
   */
  G_NV (GTK_ACCEL_VISIBLE,		visible,	1 << 0),
  /* should the signal associated with
   * this accelerator be also visible?
   */
  G_NV (GTK_ACCEL_SIGNAL_VISIBLE,	signal-visible,	1 << 1),
  /* may the accelerator be removed
   * again?
   */
  G_NV (GTK_ACCEL_LOCKED,		locked,		1 << 2),
  G_NV (GTK_ACCEL_MASK,			mask,		0x07)
} G_FLAGS (GtkAccelFlags);


/* Arrow types */
typedef enum
{
  G_SV (GTK_ARROW_UP,	 up),
  G_SV (GTK_ARROW_DOWN,	 down),
  G_SV (GTK_ARROW_LEFT,	 left),
  G_SV (GTK_ARROW_RIGHT, right),
} G_ENUM (GtkArrowType);

/* Attach options (for tables) */
typedef enum
{
  G_NV (GTK_EXPAND,	expand,	1 << 0),
  G_NV (GTK_SHRINK,	shrink,	1 << 1),
  G_NV (GTK_FILL,	fill,	1 << 2)
} G_FLAGS (GtkAttachOptions);

/* button box styles */
typedef enum
{
  G_SV (GTK_BUTTONBOX_DEFAULT_STYLE,	default),
  G_SV (GTK_BUTTONBOX_SPREAD,		spread),
  G_SV (GTK_BUTTONBOX_EDGE,		edge),
  G_SV (GTK_BUTTONBOX_START,		start),
  G_SV (GTK_BUTTONBOX_END,		end),
} G_ENUM (GtkButtonBoxStyle);

/* curve types */
typedef enum
{
  G_SV (GTK_CURVE_TYPE_LINEAR,	linear)        /* linear interpolation */,
  G_SV (GTK_CURVE_TYPE_SPLINE,	spline)	       /* spline interpolation */,
  G_SV (GTK_CURVE_TYPE_FREE,	free)          /* free form curve */
} G_ENUM (GtkCurveType);

/* Focus movement types */
typedef enum
{
  G_SV (GTK_DIR_TAB_FORWARD,	tab-forward),
  G_SV (GTK_DIR_TAB_BACKWARD,	tab-backward),
  G_SV (GTK_DIR_UP,		up),
  G_SV (GTK_DIR_DOWN,		down),
  G_SV (GTK_DIR_LEFT,		left),
  G_SV (GTK_DIR_RIGHT,		right)
} G_ENUM (GtkDirectionType);

/* justification for label and maybe other widgets (text?) */
typedef enum
{
  G_SV (GTK_JUSTIFY_LEFT,	left),
  G_SV (GTK_JUSTIFY_RIGHT,	right),
  G_SV (GTK_JUSTIFY_CENTER,	center),
  G_SV (GTK_JUSTIFY_FILL,	fill),
} G_ENUM (GtkJustification);

/* GtkPatternSpec match types */
typedef enum
{
  G_SV (GTK_MATCH_ALL,		all)         /* "*A?A*" */,
  G_SV (GTK_MATCH_ALL_TAIL,	all-tail)    /* "*A?AA" */,
  G_SV (GTK_MATCH_HEAD,		head)        /* "AAAA*" */,
  G_SV (GTK_MATCH_TAIL,		tail)        /* "*AAAA" */,
  G_SV (GTK_MATCH_EXACT,	exact)       /* "AAAAA" */,
  G_SV (GTK_MATCH_LAST,		last)
} G_ENUM (GtkMatchType);

/* menu factory types (outdated) */
typedef enum
{
  G_SV (GTK_MENU_FACTORY_MENU,		menu),
  G_SV (GTK_MENU_FACTORY_MENU_BAR,	menu-bar),
  G_SV (GTK_MENU_FACTORY_OPTION_MENU,	option-menu)
} G_ENUM (GtkMenuFactoryType);

/* gtk metrics */
typedef enum
{
  G_SV (GTK_PIXELS,		pixels),
  G_SV (GTK_INCHES,		inches),
  G_SV (GTK_CENTIMETERS,	centimeters)
} G_ENUM (GtkMetricType);

/* Orientation for toolbars, etc. */
typedef enum
{
  G_SV (GTK_ORIENTATION_HORIZONTAL,	horizontal),
  G_SV (GTK_ORIENTATION_VERTICAL,	vertical)
} G_ENUM (GtkOrientation);

/* Packing types (for boxes) */
typedef enum
{
  G_SV (GTK_PACK_START,		start),
  G_SV (GTK_PACK_END,		end)
} G_ENUM (GtkPackType);

/* priorities for path lookups */
typedef enum
{
  G_NV (GTK_PATH_PRIO_LOWEST,		lowest,		0),
  G_NV (GTK_PATH_PRIO_GTK,		gtk,		4),
  G_NV (GTK_PATH_PRIO_APPLICATION,	application,	8),
  G_NV (GTK_PATH_PRIO_RC,		rc,		12),
  G_NV (GTK_PATH_PRIO_HIGHEST,		highest,	15),
  G_NV (GTK_PATH_PRIO_MASK,		mask,		0x0f)
} G_ENUM (GtkPathPriorityType);

/* widget path types */
typedef enum
{
  G_SV (GTK_PATH_WIDGET,	widget),
  G_SV (GTK_PATH_WIDGET_CLASS,	widget-class),
  G_SV (GTK_PATH_CLASS,		class)
} G_ENUM (GtkPathType);

/* Scrollbar policy types (for scrolled windows) */
typedef enum
{
  G_SV (GTK_POLICY_ALWAYS,	always),
  G_SV (GTK_POLICY_AUTOMATIC,	automatic)
} G_ENUM (GtkPolicyType);

/* gtk position */
typedef enum
{
  G_SV (GTK_POS_LEFT,	left),
  G_SV (GTK_POS_RIGHT,	right),
  G_SV (GTK_POS_TOP,	top),
  G_SV (GTK_POS_BOTTOM,	bottom)
} G_ENUM (GtkPositionType);

/* GtkPreview types */
typedef enum
{
  G_SV (GTK_PREVIEW_COLOR,	color),
  G_SV (GTK_PREVIEW_GRAYSCALE,	grayscale),
  G_NV (GTK_PREVIEW_GREYSCALE,	greyscale,	GTK_PREVIEW_GRAYSCALE)
} G_ENUM (GtkPreviewType);

/* Style for buttons */
typedef enum
{
  G_SV (GTK_RELIEF_NORMAL,	normal),
  G_SV (GTK_RELIEF_NONE,	none)
} G_ENUM (GtkReliefStyle);

/* scrolling types */
typedef enum
{
  G_SV (GTK_SCROLL_NONE,		none),
  G_SV (GTK_SCROLL_STEP_BACKWARD,	step-backward),
  G_SV (GTK_SCROLL_STEP_FORWARD,	step-forward),
  G_SV (GTK_SCROLL_PAGE_BACKWARD,	page-backward),
  G_SV (GTK_SCROLL_PAGE_FORWARD,	page-forward),
  G_SV (GTK_SCROLL_JUMP,		jump)
} G_ENUM (GtkScrollType);

/* list selection modes */
typedef enum
{
  G_SV (GTK_SELECTION_SINGLE,	single),
  G_SV (GTK_SELECTION_BROWSE,	browse),
  G_SV (GTK_SELECTION_MULTIPLE,	multiple),
  G_SV (GTK_SELECTION_EXTENDED,	extended)
} G_ENUM (GtkSelectionMode);

/* Shadow types */
typedef enum
{
  G_SV (GTK_SHADOW_NONE,	none),
  G_SV (GTK_SHADOW_IN,		in),
  G_SV (GTK_SHADOW_OUT,		out),
  G_SV (GTK_SHADOW_ETCHED_IN,	etched-in),
  G_SV (GTK_SHADOW_ETCHED_OUT,	etched-out)
} G_ENUM (GtkShadowType);

/* signal run types */
typedef enum
{
  G_NV (GTK_RUN_FIRST,		first,		0x1),
  G_NV (GTK_RUN_LAST,		last,		0x2),
  G_NV (GTK_RUN_BOTH,		both,		0x3),
  G_NV (GTK_RUN_MASK,		mask,		0xF),
  G_NV (GTK_RUN_NO_RECURSE,	no-recurse,	0x10),
  G_NV (GTK_RUN_ACTION,		action,		0x20)
} G_FLAGS (GtkSignalRunType);

/* Widget states */
typedef enum
{
  G_SV (GTK_STATE_NORMAL,	normal),
  G_SV (GTK_STATE_ACTIVE,	active),
  G_SV (GTK_STATE_PRELIGHT,	prelight),
  G_SV (GTK_STATE_SELECTED,	selected),
  G_SV (GTK_STATE_INSENSITIVE,	insensitive)
} G_ENUM (GtkStateType);

/* directions for submenus */
typedef enum
{
  G_SV (GTK_DIRECTION_LEFT,	left),
  G_SV (GTK_DIRECTION_RIGHT,	right)
} G_ENUM (GtkSubmenuDirection);

/* placement of submenus */
typedef enum
{
  G_SV (GTK_TOP_BOTTOM,		top-bottom),
  G_SV (GTK_LEFT_RIGHT,		left-right)
} G_ENUM (GtkSubmenuPlacement);

/* Style for toolbars */
typedef enum
{
  G_SV (GTK_TOOLBAR_ICONS,	icons),
  G_SV (GTK_TOOLBAR_TEXT,	text),
  G_SV (GTK_TOOLBAR_BOTH,	both)
} G_ENUM (GtkToolbarStyle);

/* trough types for GtkRange */
typedef enum
{
  G_SV (GTK_TROUGH_NONE,	none),
  G_SV (GTK_TROUGH_START,	start),
  G_SV (GTK_TROUGH_END,		end),
  G_SV (GTK_TROUGH_JUMP,	jump)
} G_ENUM (GtkTroughType);

/* Data update types (for ranges) */
typedef enum
{
  G_SV (GTK_UPDATE_CONTINUOUS,		continuous),
  G_SV (GTK_UPDATE_DISCONTINUOUS,	discontinuous),
  G_SV (GTK_UPDATE_DELAYED,		delayed)
} G_ENUM (GtkUpdateType);

/* Generic visibility flags */
typedef enum
{
  G_SV (GTK_VISIBILITY_NONE,		none),
  G_SV (GTK_VISIBILITY_PARTIAL,		partial),
  G_SV (GTK_VISIBILITY_FULL,		full)
} G_ENUM (GtkVisibility);

/* window position types */
typedef enum
{
  G_SV (GTK_WIN_POS_NONE,	none),
  G_SV (GTK_WIN_POS_CENTER,	center),
  G_SV (GTK_WIN_POS_MOUSE,	mouse)
} G_ENUM (GtkWindowPosition);

/* Window types */
typedef enum
{
  G_SV (GTK_WINDOW_TOPLEVEL,	toplevel),
  G_SV (GTK_WINDOW_DIALOG,	dialog),
  G_SV (GTK_WINDOW_POPUP,	popup)
} G_ENUM (GtkWindowType);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENUMS_H__ */
