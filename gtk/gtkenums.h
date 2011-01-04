/* GTK - The GIMP Toolkit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_ENUMS_H__
#define __GTK_ENUMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GtkAlign:
 * @GTK_ALIGN_FILL: stretch to fill all space if possible, center if
 *     no meaningful way to stretch
 * @GTK_ALIGN_START: snap to left or top side, leaving space on right
 *     or bottom
 * @GTK_ALIGN_END: snap to right or bottom side, leaving space on left
 *     or top
 * @GTK_ALIGN_CENTER: center natural width of widget inside the
 *     allocation
 *
 * Controls how a widget deals with extra space in a single (x or y)
 * dimension.
 *
 * Alignment only matters if the widget receives a "too large" allocation,
 * for example if you packed the widget with the #GtkWidget:expand
 * flag inside a #GtkBox, then the widget might get extra space.  If
 * you have for example a 16x16 icon inside a 32x32 space, the icon
 * could be scaled and stretched, it could be centered, or it could be
 * positioned to one side of the space.
 */
typedef enum
{
  GTK_ALIGN_FILL,
  GTK_ALIGN_START,
  GTK_ALIGN_END,
  GTK_ALIGN_CENTER
} GtkAlign;

/* Arrow placement */
typedef enum
{
  GTK_ARROWS_BOTH,
  GTK_ARROWS_START,
  GTK_ARROWS_END
} GtkArrowPlacement;

/* Arrow types */
typedef enum
{
  GTK_ARROW_UP,
  GTK_ARROW_DOWN,
  GTK_ARROW_LEFT,
  GTK_ARROW_RIGHT,
  GTK_ARROW_NONE
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
  GTK_BUTTONBOX_SPREAD = 1,
  GTK_BUTTONBOX_EDGE,
  GTK_BUTTONBOX_START,
  GTK_BUTTONBOX_END,
  GTK_BUTTONBOX_CENTER
} GtkButtonBoxStyle;

typedef enum
{
  GTK_DELETE_CHARS,
  GTK_DELETE_WORD_ENDS,           /* delete only the portion of the word to the
                                   * left/right of cursor if we're in the middle
                                   * of a word */
  GTK_DELETE_WORDS,
  GTK_DELETE_DISPLAY_LINES,
  GTK_DELETE_DISPLAY_LINE_ENDS,
  GTK_DELETE_PARAGRAPH_ENDS,      /* like C-k in Emacs (or its reverse) */
  GTK_DELETE_PARAGRAPHS,          /* C-k in pico, kill whole line */
  GTK_DELETE_WHITESPACE           /* M-\ in Emacs */
} GtkDeleteType;

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

/* Expander styles */
typedef enum
{
  GTK_EXPANDER_COLLAPSED,
  GTK_EXPANDER_SEMI_COLLAPSED,
  GTK_EXPANDER_SEMI_EXPANDED,
  GTK_EXPANDER_EXPANDED
} GtkExpanderStyle;

/* Built-in stock icon sizes */
typedef enum
{
  GTK_ICON_SIZE_INVALID,
  GTK_ICON_SIZE_MENU,
  GTK_ICON_SIZE_SMALL_TOOLBAR,
  GTK_ICON_SIZE_LARGE_TOOLBAR,
  GTK_ICON_SIZE_BUTTON,
  GTK_ICON_SIZE_DND,
  GTK_ICON_SIZE_DIALOG
} GtkIconSize;

/**
 * GtkSensitivityType:
 * @GTK_SENSITIVITY_AUTO: The arrow is made insensitive if the
 *   thumb is at the end
 * @GTK_SENSITIVITY_ON: The arrow is always sensitive
 * @GTK_SENSITIVITY_OFF: The arrow is always insensitive
 *
 * Determines how GTK+ handles the sensitivity of stepper arrows
 * at the end of range widgets.
 */
typedef enum
{
  GTK_SENSITIVITY_AUTO,
  GTK_SENSITIVITY_ON,
  GTK_SENSITIVITY_OFF
} GtkSensitivityType;

/* Reading directions for text */
typedef enum
{
  GTK_TEXT_DIR_NONE,
  GTK_TEXT_DIR_LTR,
  GTK_TEXT_DIR_RTL
} GtkTextDirection;

/* justification for label and maybe other widgets (text?) */
typedef enum
{
  GTK_JUSTIFY_LEFT,
  GTK_JUSTIFY_RIGHT,
  GTK_JUSTIFY_CENTER,
  GTK_JUSTIFY_FILL
} GtkJustification;

/* Menu keyboard movement types */
typedef enum
{
  GTK_MENU_DIR_PARENT,
  GTK_MENU_DIR_CHILD,
  GTK_MENU_DIR_NEXT,
  GTK_MENU_DIR_PREV
} GtkMenuDirectionType;

/**
 * GtkMessageType:
 * @GTK_MESSAGE_INFO: Informational message
 * @GTK_MESSAGE_WARNING: Nonfatal warning message
 * @GTK_MESSAGE_QUESTION: Question requiring a choice
 * @GTK_MESSAGE_ERROR: Fatal error message
 * @GTK_MESSAGE_OTHER: None of the above, doesn't get an icon
 *
 * The type of message being displayed in the dialog.
 */
typedef enum
{
  GTK_MESSAGE_INFO,
  GTK_MESSAGE_WARNING,
  GTK_MESSAGE_QUESTION,
  GTK_MESSAGE_ERROR,
  GTK_MESSAGE_OTHER
} GtkMessageType;

/**
 * GtkMovementStep:
 * @GTK_MOVEMENT_LOGICAL_POSITIONS: Move forward or back by graphemes
 * @GTK_MOVEMENT_VISUAL_POSITIONS:  Move left or right by graphemes
 * @GTK_MOVEMENT_WORDS:             Move forward or back by words
 * @GTK_MOVEMENT_DISPLAY_LINES:     Move up or down lines (wrapped lines)
 * @GTK_MOVEMENT_DISPLAY_LINE_ENDS: Move to either end of a line
 * @GTK_MOVEMENT_PARAGRAPHS:        Move up or down paragraphs (newline-ended lines)
 * @GTK_MOVEMENT_PARAGRAPH_ENDS:    Move to either end of a paragraph
 * @GTK_MOVEMENT_PAGES:             Move by pages
 * @GTK_MOVEMENT_BUFFER_ENDS:       Move to ends of the buffer
 * @GTK_MOVEMENT_HORIZONTAL_PAGES:  Move horizontally by pages
 */
typedef enum
{
  GTK_MOVEMENT_LOGICAL_POSITIONS,
  GTK_MOVEMENT_VISUAL_POSITIONS,
  GTK_MOVEMENT_WORDS,
  GTK_MOVEMENT_DISPLAY_LINES,
  GTK_MOVEMENT_DISPLAY_LINE_ENDS,
  GTK_MOVEMENT_PARAGRAPHS,
  GTK_MOVEMENT_PARAGRAPH_ENDS,
  GTK_MOVEMENT_PAGES,
  GTK_MOVEMENT_BUFFER_ENDS,
  GTK_MOVEMENT_HORIZONTAL_PAGES
} GtkMovementStep;

typedef enum
{
  GTK_SCROLL_STEPS,
  GTK_SCROLL_PAGES,
  GTK_SCROLL_ENDS,
  GTK_SCROLL_HORIZONTAL_STEPS,
  GTK_SCROLL_HORIZONTAL_PAGES,
  GTK_SCROLL_HORIZONTAL_ENDS
} GtkScrollStep;

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
  GTK_PATH_PRIO_THEME       = 10,
  GTK_PATH_PRIO_RC          = 12,
  GTK_PATH_PRIO_HIGHEST     = 15
} GtkPathPriorityType;
#define GTK_PATH_PRIO_MASK 0x0f

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

/* scrolling types */
typedef enum
{
  GTK_SCROLL_NONE,
  GTK_SCROLL_JUMP,
  GTK_SCROLL_STEP_BACKWARD,
  GTK_SCROLL_STEP_FORWARD,
  GTK_SCROLL_PAGE_BACKWARD,
  GTK_SCROLL_PAGE_FORWARD,
  GTK_SCROLL_STEP_UP,
  GTK_SCROLL_STEP_DOWN,
  GTK_SCROLL_PAGE_UP,
  GTK_SCROLL_PAGE_DOWN,
  GTK_SCROLL_STEP_LEFT,
  GTK_SCROLL_STEP_RIGHT,
  GTK_SCROLL_PAGE_LEFT,
  GTK_SCROLL_PAGE_RIGHT,
  GTK_SCROLL_START,
  GTK_SCROLL_END
} GtkScrollType;

/* list selection modes */
typedef enum
{
  GTK_SELECTION_NONE,                             /* Nothing can be selected */
  GTK_SELECTION_SINGLE,
  GTK_SELECTION_BROWSE,
  GTK_SELECTION_MULTIPLE
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

/**
 * GtkStateType:
 * @GTK_STATE_NORMAL: State during normal operation.
 * @GTK_STATE_ACTIVE: State of a currently active widget, such as a depressed button.
 * @GTK_STATE_PRELIGHT: State indicating that the mouse pointer is over
 *                      the widget and the widget will respond to mouse clicks.
 * @GTK_STATE_SELECTED: State of a selected item, such the selected row in a list.
 * @GTK_STATE_INSENSITIVE: State indicating that the widget is
 *                         unresponsive to user actions.
 * @GTK_STATE_INCONSISTENT: The widget is inconsistent, such as checkbuttons
 *                          or radiobuttons that aren't either set to %TRUE nor %FALSE,
 *                          or buttons requiring the user attention.
 * @GTK_STATE_FOCUSED: The widget has the keyboard focus.
 *
 * This type indicates the current state of a widget; the state determines how
 * the widget is drawn. The #GtkStateType enumeration is also used to
 * identify different colors in a #GtkStyle for drawing, so states can be
 * used for subparts of a widget as well as entire widgets.
 */
typedef enum
{
  GTK_STATE_NORMAL,
  GTK_STATE_ACTIVE,
  GTK_STATE_PRELIGHT,
  GTK_STATE_SELECTED,
  GTK_STATE_INSENSITIVE,
  GTK_STATE_INCONSISTENT,
  GTK_STATE_FOCUSED
} GtkStateType;

/* Style for toolbars */
typedef enum
{
  GTK_TOOLBAR_ICONS,
  GTK_TOOLBAR_TEXT,
  GTK_TOOLBAR_BOTH,
  GTK_TOOLBAR_BOTH_HORIZ
} GtkToolbarStyle;

/* Window position types */
typedef enum
{
  GTK_WIN_POS_NONE,
  GTK_WIN_POS_CENTER,
  GTK_WIN_POS_MOUSE,
  GTK_WIN_POS_CENTER_ALWAYS,
  GTK_WIN_POS_CENTER_ON_PARENT
} GtkWindowPosition;

/* Window types */
typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_POPUP
} GtkWindowType;

/* Text wrap */
typedef enum
{
  GTK_WRAP_NONE,
  GTK_WRAP_CHAR,
  GTK_WRAP_WORD,
  GTK_WRAP_WORD_CHAR
} GtkWrapMode;

/* How to sort */
typedef enum
{
  GTK_SORT_ASCENDING,
  GTK_SORT_DESCENDING
} GtkSortType;

/* Style for gtk input method preedit/status */
typedef enum
{
  GTK_IM_PREEDIT_NOTHING,
  GTK_IM_PREEDIT_CALLBACK,
  GTK_IM_PREEDIT_NONE
} GtkIMPreeditStyle;

typedef enum
{
  GTK_IM_STATUS_NOTHING,
  GTK_IM_STATUS_CALLBACK,
  GTK_IM_STATUS_NONE
} GtkIMStatusStyle;

typedef enum
{
  GTK_PACK_DIRECTION_LTR,
  GTK_PACK_DIRECTION_RTL,
  GTK_PACK_DIRECTION_TTB,
  GTK_PACK_DIRECTION_BTT
} GtkPackDirection;

typedef enum
{
  GTK_PRINT_PAGES_ALL,
  GTK_PRINT_PAGES_CURRENT,
  GTK_PRINT_PAGES_RANGES,
  GTK_PRINT_PAGES_SELECTION
} GtkPrintPages;

typedef enum
{
  GTK_PAGE_SET_ALL,
  GTK_PAGE_SET_EVEN,
  GTK_PAGE_SET_ODD
} GtkPageSet;

typedef enum
{
  GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM, /*< nick=lrtb >*/
  GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP, /*< nick=lrbt >*/
  GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM, /*< nick=rltb >*/
  GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP, /*< nick=rlbt >*/
  GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT, /*< nick=tblr >*/
  GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT, /*< nick=tbrl >*/
  GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT, /*< nick=btlr >*/
  GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT  /*< nick=btrl >*/
} GtkNumberUpLayout;

typedef enum
{
  GTK_PAGE_ORIENTATION_PORTRAIT,
  GTK_PAGE_ORIENTATION_LANDSCAPE,
  GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT,
  GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE
} GtkPageOrientation;

typedef enum
{
  GTK_PRINT_QUALITY_LOW,
  GTK_PRINT_QUALITY_NORMAL,
  GTK_PRINT_QUALITY_HIGH,
  GTK_PRINT_QUALITY_DRAFT
} GtkPrintQuality;

typedef enum
{
  GTK_PRINT_DUPLEX_SIMPLEX,
  GTK_PRINT_DUPLEX_HORIZONTAL,
  GTK_PRINT_DUPLEX_VERTICAL
} GtkPrintDuplex;


typedef enum
{
  GTK_UNIT_PIXEL,
  GTK_UNIT_POINTS,
  GTK_UNIT_INCH,
  GTK_UNIT_MM
} GtkUnit;

/**
 * GtkTreeViewGridLines:
 * @GTK_TREE_VIEW_GRID_LINES_NONE: No grid lines.
 * @GTK_TREE_VIEW_GRID_LINES_HORIZONTAL: Horizontal grid lines.
 * @GTK_TREE_VIEW_GRID_LINES_VERTICAL: Vertical grid lines.
 * @GTK_TREE_VIEW_GRID_LINES_BOTH: Horizontal and vertical grid lines.
 *
 * Used to indicate which grid lines to draw in a tree view.
 */
typedef enum
{
  GTK_TREE_VIEW_GRID_LINES_NONE,
  GTK_TREE_VIEW_GRID_LINES_HORIZONTAL,
  GTK_TREE_VIEW_GRID_LINES_VERTICAL,
  GTK_TREE_VIEW_GRID_LINES_BOTH
} GtkTreeViewGridLines;

typedef enum
{
  GTK_DRAG_RESULT_SUCCESS,
  GTK_DRAG_RESULT_NO_TARGET,
  GTK_DRAG_RESULT_USER_CANCELLED,
  GTK_DRAG_RESULT_TIMEOUT_EXPIRED,
  GTK_DRAG_RESULT_GRAB_BROKEN,
  GTK_DRAG_RESULT_ERROR
} GtkDragResult;

/**
 * GtkSizeRequestMode:
 * @GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH: Prefer height-for-width geometry management
 * @GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT: Prefer width-for-height geometry management
 * 
 * Specifies a preference for height-for-width or
 * width-for-height geometry management.
 */
typedef enum
{
  GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH = 0,
  GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT
} GtkSizeRequestMode;

/**
 * GtkScrollablePolicy:
 * @GTK_SCROLL_MINIMUM: Scrollable adjustments are based on the minimum size
 * @GTK_SCROLL_NATURAL: Scrollable adjustments are based on the natural size
 *
 * Defines the policy to be used in a scrollable widget when updating
 * the scrolled window adjustments in a given orientation.
 */
typedef enum
{
  GTK_SCROLL_MINIMUM = 0,
  GTK_SCROLL_NATURAL
} GtkScrollablePolicy;

/**
 * GtkStateFlags:
 * @GTK_STATE_FLAG_NORMAL: State during normal operation.
 * @GTK_STATE_FLAG_ACTIVE: Widget is active.
 * @GTK_STATE_FLAG_PRELIGHT: Widget has a mouse pointer over it.
 * @GTK_STATE_FLAG_SELECTED: Widget is selected.
 * @GTK_STATE_FLAG_INSENSITIVE: Widget is insensitive.
 * @GTK_STATE_FLAG_INCONSISTENT: Widget is inconsistent.
 * @GTK_STATE_FLAG_FOCUSED: Widget has the keyboard focus.
 *
 * Describes a widget state.
 */
typedef enum
{
  GTK_STATE_FLAG_NORMAL       = 0,
  GTK_STATE_FLAG_ACTIVE       = 1 << 0,
  GTK_STATE_FLAG_PRELIGHT     = 1 << 1,
  GTK_STATE_FLAG_SELECTED     = 1 << 2,
  GTK_STATE_FLAG_INSENSITIVE  = 1 << 3,
  GTK_STATE_FLAG_INCONSISTENT = 1 << 4,
  GTK_STATE_FLAG_FOCUSED      = 1 << 5
} GtkStateFlags;

/**
 * GtkRegionFlags:
 * @GTK_REGION_EVEN: Region has an even number within a set.
 * @GTK_REGION_ODD: Region has an odd number within a set.
 * @GTK_REGION_FIRST: Region is the first one within a set.
 * @GTK_REGION_LAST: Region is the last one within a set.
 * @GTK_REGION_SORTED: Region is part of a sorted area.
 *
 * Describes a region within a widget.
 */
typedef enum {
  GTK_REGION_EVEN    = 1 << 0,
  GTK_REGION_ODD     = 1 << 1,
  GTK_REGION_FIRST   = 1 << 2,
  GTK_REGION_LAST    = 1 << 3,
  GTK_REGION_SORTED  = 1 << 5
} GtkRegionFlags;

/**
 * GtkJunctionSides:
 * @GTK_JUNCTION_NONE: No junctions.
 * @GTK_JUNCTION_CORNER_TOPLEFT: Element connects on the top-left corner.
 * @GTK_JUNCTION_CORNER_TOPRIGHT: Element connects on the top-right corner.
 * @GTK_JUNCTION_CORNER_BOTTOMLEFT: Element connects on the bottom-left corner.
 * @GTK_JUNCTION_CORNER_BOTTOMRIGHT: Element connects on the bottom-right corner.
 * @GTK_JUNCTION_TOP: Element connects on the top side.
 * @GTK_JUNCTION_BOTTOM: Element connects on the bottom side.
 * @GTK_JUNCTION_LEFT: Element connects on the left side.
 * @GTK_JUNCTION_RIGHT: Element connects on the right side.
 *
 * Describes how a rendered element connects to adjacent elements.
 */
typedef enum {
  GTK_JUNCTION_NONE   = 0,
  GTK_JUNCTION_CORNER_TOPLEFT = 1 << 0,
  GTK_JUNCTION_CORNER_TOPRIGHT = 1 << 1,
  GTK_JUNCTION_CORNER_BOTTOMLEFT = 1 << 2,
  GTK_JUNCTION_CORNER_BOTTOMRIGHT = 1 << 3,
  GTK_JUNCTION_TOP    = (GTK_JUNCTION_CORNER_TOPLEFT | GTK_JUNCTION_CORNER_TOPRIGHT),
  GTK_JUNCTION_BOTTOM = (GTK_JUNCTION_CORNER_BOTTOMLEFT | GTK_JUNCTION_CORNER_BOTTOMRIGHT),
  GTK_JUNCTION_LEFT   = (GTK_JUNCTION_CORNER_TOPLEFT | GTK_JUNCTION_CORNER_BOTTOMLEFT),
  GTK_JUNCTION_RIGHT  = (GTK_JUNCTION_CORNER_TOPRIGHT | GTK_JUNCTION_CORNER_BOTTOMRIGHT)
} GtkJunctionSides;

/**
 * GtkBorderStyle:
 * @GTK_BORDER_STYLE_NONE: No visible border
 * @GTK_BORDER_STYLE_SOLID: A solid border
 * @GTK_BORDER_STYLE_INSET: An inset border
 * @GTK_BORDER_STYLE_OUTSET: An outset border
 *
 * Describes how the border of a UI element should be rendered.
 */
typedef enum {
  GTK_BORDER_STYLE_NONE,
  GTK_BORDER_STYLE_SOLID,
  GTK_BORDER_STYLE_INSET,
  GTK_BORDER_STYLE_OUTSET
} GtkBorderStyle;

G_END_DECLS


#endif /* __GTK_ENUMS_H__ */
