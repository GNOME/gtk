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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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


/**
 * SECTION:gtkenum
 * @Short_description: Public enumerated types used throughout GTK+
 * @Title: Standard Enumerations
 */


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
 *
 * Note that in horizontal context @GTK_ALIGN_START and @GTK_ALIGN_END
 * are interpreted relative to text direction.
 */
typedef enum
{
  GTK_ALIGN_FILL,
  GTK_ALIGN_START,
  GTK_ALIGN_END,
  GTK_ALIGN_CENTER
} GtkAlign;


/**
 * GtkArrowPlacement:
 * @GTK_ARROWS_BOTH: Place one arrow on each end of the menu.
 * @GTK_ARROWS_START: Place both arrows at the top of the menu.
 * @GTK_ARROWS_END: Place both arrows at the bottom of the menu.
 *
 * Used to specify the placement of scroll arrows in scrolling menus.
 */
typedef enum
{
  GTK_ARROWS_BOTH,
  GTK_ARROWS_START,
  GTK_ARROWS_END
} GtkArrowPlacement;

/**
 * GtkArrowType:
 * @GTK_ARROW_UP: Represents an upward pointing arrow.
 * @GTK_ARROW_DOWN: Represents a downward pointing arrow.
 * @GTK_ARROW_LEFT: Represents a left pointing arrow.
 * @GTK_ARROW_RIGHT: Represents a right pointing arrow.
 * @GTK_ARROW_NONE: No arrow. Since 2.10.
 *
 * Used to indicate the direction in which a #GtkArrow should point.
 */
typedef enum
{
  GTK_ARROW_UP,
  GTK_ARROW_DOWN,
  GTK_ARROW_LEFT,
  GTK_ARROW_RIGHT,
  GTK_ARROW_NONE
} GtkArrowType;

/**
 * GtkAttachOptions:
 * @GTK_EXPAND: the widget should expand to take up any extra space in its
 * container that has been allocated.
 * @GTK_SHRINK: the widget should shrink as and when possible.
 * @GTK_FILL: the widget should fill the space allocated to it.
 *
 * Denotes the expansion properties that a widget will have when it (or its
 * parent) is resized.
 */
typedef enum
{
  GTK_EXPAND = 1 << 0,
  GTK_SHRINK = 1 << 1,
  GTK_FILL   = 1 << 2
} GtkAttachOptions;

/**
 * GtkButtonBoxStyle:
 * @GTK_BUTTONBOX_DEFAULT_STYLE: Default packing.
 * @GTK_BUTTONBOX_SPREAD: Buttons are evenly spread across the box.
 * @GTK_BUTTONBOX_EDGE: Buttons are placed at the edges of the box.
 * @GTK_BUTTONBOX_START: Buttons are grouped towards the start of the box,
 *   (on the left for a HBox, or the top for a VBox).
 * @GTK_BUTTONBOX_END: Buttons are grouped towards the end of the box,
 *   (on the right for a HBox, or the bottom for a VBox).
 * @GTK_BUTTONBOX_CENTER: Buttons are centered in the box. Since 2.12.
 *
 * Used to dictate the style that a #GtkButtonBox uses to layout the buttons it
 * contains. (See also: #GtkVButtonBox and #GtkHButtonBox).
 */
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

/**
 * GtkExpanderStyle:
 * @GTK_EXPANDER_COLLAPSED: The style used for a collapsed subtree.
 * @GTK_EXPANDER_SEMI_COLLAPSED: Intermediate style used during animation.
 * @GTK_EXPANDER_SEMI_EXPANDED: Intermediate style used during animation.
 * @GTK_EXPANDER_EXPANDED: The style used for an expanded subtree.
 *
 * Used to specify the style of the expanders drawn by a #GtkTreeView.
 */
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

/**
 * GtkJustification:
 * @GTK_JUSTIFY_LEFT: The text is placed at the left edge of the label.
 * @GTK_JUSTIFY_RIGHT: The text is placed at the right edge of the label.
 * @GTK_JUSTIFY_CENTER: The text is placed in the center of the label.
 * @GTK_JUSTIFY_FILL: The text is placed is distributed across the label.
 *
 * Used for justifying the text inside a #GtkLabel widget. (See also
 * #GtkAlignment).
 */
typedef enum
{
  GTK_JUSTIFY_LEFT,
  GTK_JUSTIFY_RIGHT,
  GTK_JUSTIFY_CENTER,
  GTK_JUSTIFY_FILL
} GtkJustification;

/**
 * GtkMenuDirectionType:
 * @GTK_MENU_DIR_PARENT: To the parent menu shell
 * @GTK_MENU_DIR_CHILD: To the submenu, if any, associated with the item
 * @GTK_MENU_DIR_NEXT: To the next menu item
 * @GTK_MENU_DIR_PREV: To the previous menu item
 *
 * An enumeration representing directional movements within a menu.
 */
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

/**
 * GtkOrientation:
 * @GTK_ORIENTATION_HORIZONTAL: The widget is in horizontal orientation.
 * @GTK_ORIENTATION_VERTICAL: The widget is in vertical orientation.
 *
 * Represents the orientation of widgets which can be switched between horizontal
 * and vertical orientation on the fly, like #GtkToolbar.
 */
typedef enum
{
  GTK_ORIENTATION_HORIZONTAL,
  GTK_ORIENTATION_VERTICAL
} GtkOrientation;

/**
 * GtkCornerType:
 * @GTK_CORNER_TOP_LEFT: Place the scrollbars on the right and bottom of the
 *  widget (default behaviour).
 * @GTK_CORNER_BOTTOM_LEFT: Place the scrollbars on the top and right of the
 *  widget.
 * @GTK_CORNER_TOP_RIGHT: Place the scrollbars on the left and bottom of the
 *  widget.
 * @GTK_CORNER_BOTTOM_RIGHT: Place the scrollbars on the top and left of the
 *  widget.
 *
 * Specifies which corner a child widget should be placed in when packed into
 * a #GtkScrolledWindow. This is effectively the opposite of where the scroll
 * bars are placed.
 */
typedef enum
{
  GTK_CORNER_TOP_LEFT,
  GTK_CORNER_BOTTOM_LEFT,
  GTK_CORNER_TOP_RIGHT,
  GTK_CORNER_BOTTOM_RIGHT
} GtkCornerType;

/**
 * GtkPackType:
 * @GTK_PACK_START: The child is packed into the start of the box
 * @GTK_PACK_END: The child is packed into the end of the box
 *
 * Represents the packing location #GtkBox children. (See: #GtkVBox,
 * #GtkHBox, and #GtkButtonBox).
 */
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

/**
 * GtkPolicyType:
 * @GTK_POLICY_ALWAYS: The scrollbar is always visible.
 * @GTK_POLICY_AUTOMATIC: The scrollbar will appear and disappear as necessary. For example,
 *  when all of a #GtkCList can not be seen.
 * @GTK_POLICY_NEVER: The scrollbar will never appear.
 *
 * Determines when a scroll bar will be visible.
 */
typedef enum
{
  GTK_POLICY_ALWAYS,
  GTK_POLICY_AUTOMATIC,
  GTK_POLICY_NEVER
} GtkPolicyType;

/**
 * GtkPositionType:
 * @GTK_POS_LEFT: The feature is at the left edge.
 * @GTK_POS_RIGHT: The feature is at the right edge.
 * @GTK_POS_TOP: The feature is at the top edge.
 * @GTK_POS_BOTTOM: The feature is at the bottom edge.
 *
 * Describes which edge of a widget a certain feature is positioned at, e.g. the
 * tabs of a #GtkNotebook, the handle of a #GtkHandleBox or the label of a
 * #GtkScale.
 */
typedef enum
{
  GTK_POS_LEFT,
  GTK_POS_RIGHT,
  GTK_POS_TOP,
  GTK_POS_BOTTOM
} GtkPositionType;

/**
 * GtkReliefStyle:
 * @GTK_RELIEF_NORMAL: Draw a normal relief.
 * @GTK_RELIEF_HALF: A half relief.
 * @GTK_RELIEF_NONE: No relief.
 *
 * Indicated the relief to be drawn around a #GtkButton.
 */
typedef enum
{
  GTK_RELIEF_NORMAL,
  GTK_RELIEF_HALF,
  GTK_RELIEF_NONE
} GtkReliefStyle;

/**
 * GtkResizeMode:
 * @GTK_RESIZE_PARENT: Pass resize request to the parent
 * @GTK_RESIZE_QUEUE: Queue resizes on this widget
 * @GTK_RESIZE_IMMEDIATE: Resize immediately. Deprecated.
 */
typedef enum
{
  GTK_RESIZE_PARENT,
  GTK_RESIZE_QUEUE,
  GTK_RESIZE_IMMEDIATE
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

/**
 * GtkSelectionMode:
 * @GTK_SELECTION_NONE: No selection is possible.
 * @GTK_SELECTION_SINGLE: Zero or one element may be selected.
 * @GTK_SELECTION_BROWSE: Exactly one element is selected.
 *     In some circumstances, such as initially or during a search
 *     operation, it's possible for no element to be selected with
 *     %GTK_SELECTION_BROWSE. What is really enforced is that the user
 *     can't deselect a currently selected element except by selecting
 *     another element.
 * @GTK_SELECTION_MULTIPLE: Any number of elements may be selected.
 *      The Ctrl key may be used to enlarge the selection, and Shift
 *      key to select between the focus and the child pointed to.
 *      Some widgets may also allow Click-drag to select a range of elements.
 * @GTK_SELECTION_EXTENDED: Deprecated, behaves identical to %GTK_SELECTION_MULTIPLE.
 *
 * Used to control what selections users are allowed to make.
 */
typedef enum
{
  GTK_SELECTION_NONE,
  GTK_SELECTION_SINGLE,
  GTK_SELECTION_BROWSE,
  GTK_SELECTION_MULTIPLE
} GtkSelectionMode;

/**
 * GtkShadowType:
 * @GTK_SHADOW_NONE: No outline.
 * @GTK_SHADOW_IN: The outline is bevelled inwards.
 * @GTK_SHADOW_OUT: The outline is bevelled outwards like a button.
 * @GTK_SHADOW_ETCHED_IN: The outline has a sunken 3d appearance.
 * @GTK_SHADOW_ETCHED_OUT: The outline has a raised 3d appearance.
 *
 * Used to change the appearance of an outline typically provided by a #GtkFrame.
 */
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

/**
 * GtkToolbarStyle:
 * @GTK_TOOLBAR_ICONS: Buttons display only icons in the toolbar.
 * @GTK_TOOLBAR_TEXT: Buttons display only text labels in the toolbar.
 * @GTK_TOOLBAR_BOTH: Buttons display text and icons in the toolbar.
 * @GTK_TOOLBAR_BOTH_HORIZ: Buttons display icons and text alongside each
 *  other, rather than vertically stacked
 *
 * Used to customize the appearance of a #GtkToolbar. Note that
 * setting the toolbar style overrides the user's preferences
 * for the default toolbar style.  Note that if the button has only
 * a label set and GTK_TOOLBAR_ICONS is used, the label will be
 * visible, and vice versa.
 */
typedef enum
{
  GTK_TOOLBAR_ICONS,
  GTK_TOOLBAR_TEXT,
  GTK_TOOLBAR_BOTH,
  GTK_TOOLBAR_BOTH_HORIZ
} GtkToolbarStyle;

/**
 * GtkWindowPosition:
 * @GTK_WIN_POS_NONE: No influence is made on placement.
 * @GTK_WIN_POS_CENTER: Windows should be placed in the center of the screen.
 * @GTK_WIN_POS_MOUSE: Windows should be placed at the current mouse position.
 * @GTK_WIN_POS_CENTER_ALWAYS: Keep window centered as it changes size, etc.
 * @GTK_WIN_POS_CENTER_ON_PARENT: Center the window on its transient
 *  parent (see gtk_window_set_transient_for()).
 *
 * Window placement can be influenced using this enumeration. Note that
 * using #GTK_WIN_POS_CENTER_ALWAYS is almost always a bad idea.
 * It won't necessarily work well with all window managers or on all windowing systems.
 */
typedef enum
{
  GTK_WIN_POS_NONE,
  GTK_WIN_POS_CENTER,
  GTK_WIN_POS_MOUSE,
  GTK_WIN_POS_CENTER_ALWAYS,
  GTK_WIN_POS_CENTER_ON_PARENT
} GtkWindowPosition;

/**
 * GtkWindowType:
 * @GTK_WINDOW_TOPLEVEL: A regular window, such as a dialog.
 * @GTK_WINDOW_POPUP: A special window such as a tooltip.
 *
 * A #GtkWindow can be one of these types. Most things you'd consider a
 * "window" should have type #GTK_WINDOW_TOPLEVEL; windows with this type
 * are managed by the window manager and have a frame by default (call
 * gtk_window_set_decorated() to toggle the frame).  Windows with type
 * #GTK_WINDOW_POPUP are ignored by the window manager; window manager
 * keybindings won't work on them, the window manager won't decorate the
 * window with a frame, many GTK+ features that rely on the window
 * manager will not work (e.g. resize grips and
 * maximization/minimization). #GTK_WINDOW_POPUP is used to implement
 * widgets such as #GtkMenu or tooltips that you normally don't think of
 * as windows per se. Nearly all windows should be #GTK_WINDOW_TOPLEVEL.
 * In particular, do not use #GTK_WINDOW_POPUP just to turn off
 * the window borders; use gtk_window_set_decorated() for that.
 */
typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_POPUP
} GtkWindowType;

/**
 * GtkWrapMode:
 * @GTK_WRAP_NONE: do not wrap lines; just make the text area wider
 * @GTK_WRAP_CHAR: wrap text, breaking lines anywhere the cursor can
 *     appear (between characters, usually - if you want to be technical,
 *     between graphemes, see pango_get_log_attrs())
 * @GTK_WRAP_WORD: wrap text, breaking lines in between words
 * @GTK_WRAP_WORD_CHAR: wrap text, breaking lines in between words, or if
 *     that is not enough, also between graphemes
 *
 * Describes a type of line wrapping.
 */
typedef enum
{
  GTK_WRAP_NONE,
  GTK_WRAP_CHAR,
  GTK_WRAP_WORD,
  GTK_WRAP_WORD_CHAR
} GtkWrapMode;

/**
 * GtkSortType:
 * @GTK_SORT_ASCENDING: Sorting is in ascending order.
 * @GTK_SORT_DESCENDING: Sorting is in descending order.
 *
 * Determines the direction of a sort.
 */
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

/**
 * GtkPackDirection:
 * @GTK_PACK_DIRECTION_LTR: Widgets are packed left-to-right
 * @GTK_PACK_DIRECTION_RTL: Widgets are packed right-to-left
 * @GTK_PACK_DIRECTION_TTB: Widgets are packed top-to-bottom
 * @GTK_PACK_DIRECTION_BTT: Widgets are packed bottom-to-top
 *
 * Determines how widgets should be packed insided menubars
 * and menuitems contained in menubars.
 */
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

/**
 * GtkNumberUpLayout:
 * @GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM: <inlinegraphic valign="middle" fileref="layout-lrtb.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP: <inlinegraphic valign="middle" fileref="layout-lrbt.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM: <inlinegraphic valign="middle" fileref="layout-rltb.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP: <inlinegraphic valign="middle" fileref="layout-rlbt.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT: <inlinegraphic valign="middle" fileref="layout-tblr.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT: <inlinegraphic valign="middle" fileref="layout-tbrl.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT: <inlinegraphic valign="middle" fileref="layout-btlr.png" format="PNG"></inlinegraphic>
 * @GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT: <inlinegraphic valign="middle" fileref="layout-btrl.png" format="PNG"></inlinegraphic>
 *
 * Used to determine the layout of pages on a sheet when printing
 * multiple pages per sheet.
 */
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
  GTK_UNIT_NONE,
  GTK_UNIT_POINTS,
  GTK_UNIT_INCH,
  GTK_UNIT_MM
} GtkUnit;

#define GTK_UNIT_PIXEL GTK_UNIT_NONE

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

/**
 * GtkDragResult:
 * @GTK_DRAG_RESULT_SUCCESS: The drag operation was successful.
 * @GTK_DRAG_RESULT_NO_TARGET: No suitable drag target.
 * @GTK_DRAG_RESULT_USER_CANCELLED: The user cancelled the drag operation.
 * @GTK_DRAG_RESULT_TIMEOUT_EXPIRED: The drag operation timed out.
 * @GTK_DRAG_RESULT_GRAB_BROKEN: The pointer or keyboard grab used
 *  for the drag operation was broken.
 * @GTK_DRAG_RESULT_ERROR: The drag operation failed due to some
 *  unspecified error.
 *
 * Gives an indication why a drag operation failed.
 * The value can by obtained by connecting to the
 * #GtkWidget::drag-failed signal.
 */
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
 * @GTK_SIZE_REQUEST_CONSTANT_SIZE: Dont trade height-for-width or width-for-height
 * 
 * Specifies a preference for height-for-width or
 * width-for-height geometry management.
 */
typedef enum
{
  GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH = 0,
  GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT,
  GTK_SIZE_REQUEST_CONSTANT_SIZE
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
 * @GTK_STATE_FLAG_BACKDROP: Widget is in a background toplevel window.
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
  GTK_STATE_FLAG_FOCUSED      = 1 << 5,
  GTK_STATE_FLAG_BACKDROP     = 1 << 6
} GtkStateFlags;

/**
 * GtkRegionFlags:
 * @GTK_REGION_EVEN: Region has an even number within a set.
 * @GTK_REGION_ODD: Region has an odd number within a set.
 * @GTK_REGION_FIRST: Region is the first one within a set.
 * @GTK_REGION_LAST: Region is the last one within a set.
 * @GTK_REGION_ONLY: Region is the only one within a set.
 * @GTK_REGION_SORTED: Region is part of a sorted area.
 *
 * Describes a region within a widget.
 */
typedef enum {
  GTK_REGION_EVEN    = 1 << 0,
  GTK_REGION_ODD     = 1 << 1,
  GTK_REGION_FIRST   = 1 << 2,
  GTK_REGION_LAST    = 1 << 3,
  GTK_REGION_ONLY    = 1 << 4,
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
 * @GTK_BORDER_STYLE_SOLID: A single line segment
 * @GTK_BORDER_STYLE_INSET: Looks as if the content is sunken into the canvas
 * @GTK_BORDER_STYLE_OUTSET: Looks as if the content is coming out of the canvas
 * @GTK_BORDER_STYLE_HIDDEN: Same as @GTK_BORDER_STYLE_NONE
 * @GTK_BORDER_STYLE_DOTTED: A series of round dots
 * @GTK_BORDER_STYLE_DASHED: A series of square-ended dashes
 * @GTK_BORDER_STYLE_DOUBLE: Two parrallel lines with some space between them
 * @GTK_BORDER_STYLE_GROOVE: Looks as if it were carved in the canvas
 * @GTK_BORDER_STYLE_RIDGE: Looks as if it were coming out of the canvas
 *
 * Describes how the border of a UI element should be rendered.
 */
typedef enum {
  GTK_BORDER_STYLE_NONE,
  GTK_BORDER_STYLE_SOLID,
  GTK_BORDER_STYLE_INSET,
  GTK_BORDER_STYLE_OUTSET,
  GTK_BORDER_STYLE_HIDDEN,
  GTK_BORDER_STYLE_DOTTED,
  GTK_BORDER_STYLE_DASHED,
  GTK_BORDER_STYLE_DOUBLE,
  GTK_BORDER_STYLE_GROOVE,
  GTK_BORDER_STYLE_RIDGE
} GtkBorderStyle;

/**
 * GtkLevelBarMode:
 * @GTK_LEVEL_BAR_MODE_CONTINUOUS: the bar has a continuous mode
 * @GTK_LEVEL_BAR_MODE_DISCRETE: the bar has a discrete mode
 *
 * Describes how #GtkLevelBar contents should be rendered.
 * Note that this enumeration could be extended with additional modes
 * in the future.
 *
 * Since: 3.6
 */
typedef enum {
  GTK_LEVEL_BAR_MODE_CONTINUOUS,
  GTK_LEVEL_BAR_MODE_DISCRETE
} GtkLevelBarMode;

G_END_DECLS

/**
 * GtkInputPurpose:
 * @GTK_INPUT_PURPOSE_FREE_FORM: Allow any character
 * @GTK_INPUT_PURPOSE_ALPHA: Allow only alphabetic characters
 * @GTK_INPUT_PURPOSE_DIGITS: Allow only digits
 * @GTK_INPUT_PURPOSE_NUMBER: Edited field expects numbers
 * @GTK_INPUT_PURPOSE_PHONE: Edited field expects phone number
 * @GTK_INPUT_PURPOSE_URL: Edited field expects URL
 * @GTK_INPUT_PURPOSE_EMAIL: Edited field expects email address
 * @GTK_INPUT_PURPOSE_NAME: Edited field expects the name of a person
 * @GTK_INPUT_PURPOSE_PASSWORD: Like @GTK_INPUT_PURPOSE_FREE_FORM, but characters are hidden
 * @GTK_INPUT_PURPOSE_PIN: Like @GTK_INPUT_PURPOSE_DIGITS, but characters are hidden
 *
 * Describes primary purpose of the input widget. This information is
 * useful for on-screen keyboards and similar input methods to decide
 * which keys should be presented to the user.
 *
 * Note that the purpose is not meant to impose a totally strict rule
 * about allowed characters, and does not replace input validation.
 * It is fine for an on-screen keyboard to let the user override the
 * character set restriction that is expressed by the purpose. The
 * application is expected to validate the entry contents, even if
 * it specified a purpose.
 *
 * The difference between @GTK_INPUT_PURPOSE_DIGITS and
 * @GTK_INPUT_PURPOSE_NUMBER is that the former accepts only digits
 * while the latter also some punctuation (like commas or points, plus,
 * minus) and 'e' or 'E' as in 3.14E+000.
 *
 * This enumeration may be extended in the future; input methods should
 * interpret unknown values as 'free form'.
 *
 * Since: 3.6
 */
typedef enum
{
  GTK_INPUT_PURPOSE_FREE_FORM,
  GTK_INPUT_PURPOSE_ALPHA,
  GTK_INPUT_PURPOSE_DIGITS,
  GTK_INPUT_PURPOSE_NUMBER,
  GTK_INPUT_PURPOSE_PHONE,
  GTK_INPUT_PURPOSE_URL,
  GTK_INPUT_PURPOSE_EMAIL,
  GTK_INPUT_PURPOSE_NAME,
  GTK_INPUT_PURPOSE_PASSWORD,
  GTK_INPUT_PURPOSE_PIN
} GtkInputPurpose;

/**
 * GtkInputHints:
 * @GTK_INPUT_HINT_NONE: No special behaviour suggested
 * @GTK_INPUT_HINT_SPELLCHECK: Suggest checking for typos
 * @GTK_INPUT_HINT_NO_SPELLCHECK: Suggest not checking for typos
 * @GTK_INPUT_HINT_WORD_COMPLETION: Suggest word completion
 * @GTK_INPUT_HINT_LOWERCASE: Suggest to convert all text to lowercase
 * @GTK_INPUT_HINT_UPPERCASE_CHARS: Suggest to capitalize all text
 * @GTK_INPUT_HINT_UPPERCASE_WORDS: Suggest to capitalize the first
 *     character of each word
 * @GTK_INPUT_HINT_UPPERCASE_SENTENCES: Suggest to capitalize the
 *     first word of each sentence
 * @GTK_INPUT_HINT_INHIBIT_OSK: Suggest to not show an onscreen keyboard
 *     (e.g for a calculator that already has all the keys).
 *
 * Describes hints that might be taken into account by input methods
 * or applications. Note that input methods may already tailor their
 * behaviour according to the #GtkInputPurpose of the entry.
 *
 * Some common sense is expected when using these flags - mixing
 * @GTK_INPUT_HINT_LOWERCASE with any of the uppercase hints makes no sense.
 *
 * This enumeration may be extended in the future; input methods should
 * ignore unknown values.
 *
 * Since: 3.6
 */
typedef enum
{
  GTK_INPUT_HINT_NONE                = 0,
  GTK_INPUT_HINT_SPELLCHECK          = 1 << 0,
  GTK_INPUT_HINT_NO_SPELLCHECK       = 1 << 1,
  GTK_INPUT_HINT_WORD_COMPLETION     = 1 << 2,
  GTK_INPUT_HINT_LOWERCASE           = 1 << 3,
  GTK_INPUT_HINT_UPPERCASE_CHARS     = 1 << 4,
  GTK_INPUT_HINT_UPPERCASE_WORDS     = 1 << 5,
  GTK_INPUT_HINT_UPPERCASE_SENTENCES = 1 << 6,
  GTK_INPUT_HINT_INHIBIT_OSK         = 1 << 7
} GtkInputHints;

#endif /* __GTK_ENUMS_H__ */
