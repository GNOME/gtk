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

#ifndef __GTK_ENUMS_H__
#define __GTK_ENUMS_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib-object.h>


/**
 * SECTION:gtkenums
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
 * @GTK_ALIGN_BASELINE: align the widget according to the baseline
 *
 * Controls how a widget deals with extra space in a single (x or y)
 * dimension.
 *
 * Alignment only matters if the widget receives a “too large” allocation,
 * for example if you packed the widget with the #GtkWidget:expand
 * flag inside a #GtkBox, then the widget might get extra space.  If
 * you have for example a 16x16 icon inside a 32x32 space, the icon
 * could be scaled and stretched, it could be centered, or it could be
 * positioned to one side of the space.
 *
 * Note that in horizontal context @GTK_ALIGN_START and @GTK_ALIGN_END
 * are interpreted relative to text direction.
 *
 * GTK_ALIGN_BASELINE support for it is optional for containers and widgets, and
 * it is only supported for vertical alignment.  When its not supported by
 * a child or a container it is treated as @GTK_ALIGN_FILL.
 */
typedef enum
{
  GTK_ALIGN_FILL,
  GTK_ALIGN_START,
  GTK_ALIGN_END,
  GTK_ALIGN_CENTER,
  GTK_ALIGN_BASELINE
} GtkAlign;

/**
 * GtkArrowType:
 * @GTK_ARROW_UP: Represents an upward pointing arrow.
 * @GTK_ARROW_DOWN: Represents a downward pointing arrow.
 * @GTK_ARROW_LEFT: Represents a left pointing arrow.
 * @GTK_ARROW_RIGHT: Represents a right pointing arrow.
 * @GTK_ARROW_NONE: No arrow.
 *
 * Used to indicate the direction in which an arrow should point.
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
 * GtkBaselinePosition:
 * @GTK_BASELINE_POSITION_TOP: Align the baseline at the top
 * @GTK_BASELINE_POSITION_CENTER: Center the baseline
 * @GTK_BASELINE_POSITION_BOTTOM: Align the baseline at the bottom
 *
 * Whenever a container has some form of natural row it may align
 * children in that row along a common typographical baseline. If
 * the amount of verical space in the row is taller than the total
 * requested height of the baseline-aligned children then it can use a
 * #GtkBaselinePosition to select where to put the baseline inside the
 * extra availible space.
 */
typedef enum
{
  GTK_BASELINE_POSITION_TOP,
  GTK_BASELINE_POSITION_CENTER,
  GTK_BASELINE_POSITION_BOTTOM
} GtkBaselinePosition;

/**
 * GtkDeleteType:
 * @GTK_DELETE_CHARS: Delete characters.
 * @GTK_DELETE_WORD_ENDS: Delete only the portion of the word to the
 *   left/right of cursor if we’re in the middle of a word.
 * @GTK_DELETE_WORDS: Delete words.
 * @GTK_DELETE_DISPLAY_LINES: Delete display-lines. Display-lines
 *   refers to the visible lines, with respect to to the current line
 *   breaks. As opposed to paragraphs, which are defined by line
 *   breaks in the input.
 * @GTK_DELETE_DISPLAY_LINE_ENDS: Delete only the portion of the
 *   display-line to the left/right of cursor.
 * @GTK_DELETE_PARAGRAPH_ENDS: Delete to the end of the
 *   paragraph. Like C-k in Emacs (or its reverse).
 * @GTK_DELETE_PARAGRAPHS: Delete entire line. Like C-k in pico.
 * @GTK_DELETE_WHITESPACE: Delete only whitespace. Like M-\ in Emacs.
 *
 * See also: #GtkEntry::delete-from-cursor.
 */
typedef enum
{
  GTK_DELETE_CHARS,
  GTK_DELETE_WORD_ENDS,
  GTK_DELETE_WORDS,
  GTK_DELETE_DISPLAY_LINES,
  GTK_DELETE_DISPLAY_LINE_ENDS,
  GTK_DELETE_PARAGRAPH_ENDS,
  GTK_DELETE_PARAGRAPHS,
  GTK_DELETE_WHITESPACE
} GtkDeleteType;

/* Focus movement types */
/**
 * GtkDirectionType:
 * @GTK_DIR_TAB_FORWARD: Move forward.
 * @GTK_DIR_TAB_BACKWARD: Move backward.
 * @GTK_DIR_UP: Move up.
 * @GTK_DIR_DOWN: Move down.
 * @GTK_DIR_LEFT: Move left.
 * @GTK_DIR_RIGHT: Move right.
 *
 * Focus movement types.
 */
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
 * GtkIconSize:
 * @GTK_ICON_SIZE_INHERIT: Keep the size of the parent element
 * @GTK_ICON_SIZE_NORMAL: Size similar to text size
 * @GTK_ICON_SIZE_LARGE: Large size, for example in an icon view
 *
 * Built-in icon sizes.
 *
 * Icon sizes default to being inherited. Where they cannot be
 * inherited, text size is the default.
 *
 * All widgets which use GtkIconSize set the normal-icons or large-icons
 * style classes correspondingly, and let themes determine the actual size
 * to be used with the -gtk-icon-size CSS property.
 */
typedef enum
{
  GTK_ICON_SIZE_INHERIT,
  GTK_ICON_SIZE_NORMAL,
  GTK_ICON_SIZE_LARGE
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
/**
 * GtkTextDirection:
 * @GTK_TEXT_DIR_NONE: No direction.
 * @GTK_TEXT_DIR_LTR: Left to right text direction.
 * @GTK_TEXT_DIR_RTL: Right to left text direction.
 *
 * Reading directions for text.
 */
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
 * @GTK_MESSAGE_WARNING: Non-fatal warning message
 * @GTK_MESSAGE_QUESTION: Question requiring a choice
 * @GTK_MESSAGE_ERROR: Fatal error message
 * @GTK_MESSAGE_OTHER: None of the above
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

/**
 * GtkScrollStep:
 * @GTK_SCROLL_STEPS: Scroll in steps.
 * @GTK_SCROLL_PAGES: Scroll by pages.
 * @GTK_SCROLL_ENDS: Scroll to ends.
 * @GTK_SCROLL_HORIZONTAL_STEPS: Scroll in horizontal steps.
 * @GTK_SCROLL_HORIZONTAL_PAGES: Scroll by horizontal pages.
 * @GTK_SCROLL_HORIZONTAL_ENDS: Scroll to the horizontal ends.
 */
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
 * @GTK_ORIENTATION_HORIZONTAL: The element is in horizontal orientation.
 * @GTK_ORIENTATION_VERTICAL: The element is in vertical orientation.
 *
 * Represents the orientation of widgets and other objects which can be switched
 * between horizontal and vertical orientation on the fly, like #GtkToolbar or
 * #GtkGesturePan.
 */
typedef enum
{
  GTK_ORIENTATION_HORIZONTAL,
  GTK_ORIENTATION_VERTICAL
} GtkOrientation;

/**
 * GtkOverflow:
 * @GTK_OVERFLOW_VISIBLE: No change is applied. Content is drawn at the specified
 *     position.
 * @GTK_OVERFLOW_HIDDEN: Content is clipped to the bounds of the area. Content
 *     outside the area is not drawn and cannot be interacted with.
 *
 * Defines how content overflowing a given area should be handled, such as
 * with gtk_widget_set_overflow(). This property is modeled after the CSS overflow
 * property, but implements it only partially.
 */
typedef enum
{
  GTK_OVERFLOW_VISIBLE,
  GTK_OVERFLOW_HIDDEN
} GtkOverflow;

/**
 * GtkPackType:
 * @GTK_PACK_START: The child is packed into the start of the box
 * @GTK_PACK_END: The child is packed into the end of the box
 *
 * Represents the packing location #GtkBox children
 */
typedef enum
{
  GTK_PACK_START,
  GTK_PACK_END
} GtkPackType;

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
 * @GTK_RELIEF_NONE: No relief.
 *
 * Indicated the relief to be drawn around a #GtkButton.
 */
typedef enum
{
  GTK_RELIEF_NORMAL,
  GTK_RELIEF_NONE
} GtkReliefStyle;

/**
 * GtkScrollType:
 * @GTK_SCROLL_NONE: No scrolling.
 * @GTK_SCROLL_JUMP: Jump to new location.
 * @GTK_SCROLL_STEP_BACKWARD: Step backward.
 * @GTK_SCROLL_STEP_FORWARD: Step forward.
 * @GTK_SCROLL_PAGE_BACKWARD: Page backward.
 * @GTK_SCROLL_PAGE_FORWARD: Page forward.
 * @GTK_SCROLL_STEP_UP: Step up.
 * @GTK_SCROLL_STEP_DOWN: Step down.
 * @GTK_SCROLL_PAGE_UP: Page up.
 * @GTK_SCROLL_PAGE_DOWN: Page down.
 * @GTK_SCROLL_STEP_LEFT: Step to the left.
 * @GTK_SCROLL_STEP_RIGHT: Step to the right.
 * @GTK_SCROLL_PAGE_LEFT: Page to the left.
 * @GTK_SCROLL_PAGE_RIGHT: Page to the right.
 * @GTK_SCROLL_START: Scroll to start.
 * @GTK_SCROLL_END: Scroll to end.
 *
 * Scrolling types.
 */
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
 *     operation, it’s possible for no element to be selected with
 *     %GTK_SELECTION_BROWSE. What is really enforced is that the user
 *     can’t deselect a currently selected element except by selecting
 *     another element.
 * @GTK_SELECTION_MULTIPLE: Any number of elements may be selected.
 *      The Ctrl key may be used to enlarge the selection, and Shift
 *      key to select between the focus and the child pointed to.
 *      Some widgets may also allow Click-drag to select a range of elements.
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
 *
 * Note that many themes do not differentiate the appearance of the
 * various shadow types: Either there is no visible shadow (@GTK_SHADOW_NONE),
 * or there is (any other value).
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
 * GtkToolbarStyle:
 * @GTK_TOOLBAR_ICONS: Buttons display only icons in the toolbar.
 * @GTK_TOOLBAR_TEXT: Buttons display only text labels in the toolbar.
 * @GTK_TOOLBAR_BOTH: Buttons display text and icons in the toolbar.
 * @GTK_TOOLBAR_BOTH_HORIZ: Buttons display icons and text alongside each
 *  other, rather than vertically stacked
 *
 * Used to customize the appearance of a #GtkToolbar. Note that
 * setting the toolbar style overrides the user’s preferences
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

/**
 * GtkPrintPages:
 * @GTK_PRINT_PAGES_ALL: All pages.
 * @GTK_PRINT_PAGES_CURRENT: Current page.
 * @GTK_PRINT_PAGES_RANGES: Range of pages.
 * @GTK_PRINT_PAGES_SELECTION: Selected pages.
 *
 * See also gtk_print_job_set_pages()
 */
typedef enum
{
  GTK_PRINT_PAGES_ALL,
  GTK_PRINT_PAGES_CURRENT,
  GTK_PRINT_PAGES_RANGES,
  GTK_PRINT_PAGES_SELECTION
} GtkPrintPages;

/**
 * GtkPageSet:
 * @GTK_PAGE_SET_ALL: All pages.
 * @GTK_PAGE_SET_EVEN: Even pages.
 * @GTK_PAGE_SET_ODD: Odd pages.
 *
 * See also gtk_print_job_set_page_set().
 */
typedef enum
{
  GTK_PAGE_SET_ALL,
  GTK_PAGE_SET_EVEN,
  GTK_PAGE_SET_ODD
} GtkPageSet;

/**
 * GtkNumberUpLayout:
 * @GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM: ![](layout-lrtb.png)
 * @GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP: ![](layout-lrbt.png)
 * @GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM: ![](layout-rltb.png)
 * @GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP: ![](layout-rlbt.png)
 * @GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT: ![](layout-tblr.png)
 * @GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT: ![](layout-tbrl.png)
 * @GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT: ![](layout-btlr.png)
 * @GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT: ![](layout-btrl.png)
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

/**
 * GtkPageOrientation:
 * @GTK_PAGE_ORIENTATION_PORTRAIT: Portrait mode.
 * @GTK_PAGE_ORIENTATION_LANDSCAPE: Landscape mode.
 * @GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT: Reverse portrait mode.
 * @GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE: Reverse landscape mode.
 *
 * See also gtk_print_settings_set_orientation().
 */
typedef enum
{
  GTK_PAGE_ORIENTATION_PORTRAIT,
  GTK_PAGE_ORIENTATION_LANDSCAPE,
  GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT,
  GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE
} GtkPageOrientation;

/**
 * GtkPrintQuality:
 * @GTK_PRINT_QUALITY_LOW: Low quality.
 * @GTK_PRINT_QUALITY_NORMAL: Normal quality.
 * @GTK_PRINT_QUALITY_HIGH: High quality.
 * @GTK_PRINT_QUALITY_DRAFT: Draft quality.
 *
 * See also gtk_print_settings_set_quality().
 */
typedef enum
{
  GTK_PRINT_QUALITY_LOW,
  GTK_PRINT_QUALITY_NORMAL,
  GTK_PRINT_QUALITY_HIGH,
  GTK_PRINT_QUALITY_DRAFT
} GtkPrintQuality;

/**
 * GtkPrintDuplex:
 * @GTK_PRINT_DUPLEX_SIMPLEX: No duplex.
 * @GTK_PRINT_DUPLEX_HORIZONTAL: Horizontal duplex.
 * @GTK_PRINT_DUPLEX_VERTICAL: Vertical duplex.
 *
 * See also gtk_print_settings_set_duplex().
 */
typedef enum
{
  GTK_PRINT_DUPLEX_SIMPLEX,
  GTK_PRINT_DUPLEX_HORIZONTAL,
  GTK_PRINT_DUPLEX_VERTICAL
} GtkPrintDuplex;


/**
 * GtkUnit:
 * @GTK_UNIT_NONE: No units.
 * @GTK_UNIT_POINTS: Dimensions in points.
 * @GTK_UNIT_INCH: Dimensions in inches.
 * @GTK_UNIT_MM: Dimensions in millimeters
 *
 * See also gtk_print_settings_set_paper_width().
 */
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
 * GtkSizeGroupMode:
 * @GTK_SIZE_GROUP_NONE: group has no effect
 * @GTK_SIZE_GROUP_HORIZONTAL: group affects horizontal requisition
 * @GTK_SIZE_GROUP_VERTICAL: group affects vertical requisition
 * @GTK_SIZE_GROUP_BOTH: group affects both horizontal and vertical requisition
 *
 * The mode of the size group determines the directions in which the size
 * group affects the requested sizes of its component widgets.
 **/
typedef enum {
  GTK_SIZE_GROUP_NONE,
  GTK_SIZE_GROUP_HORIZONTAL,
  GTK_SIZE_GROUP_VERTICAL,
  GTK_SIZE_GROUP_BOTH
} GtkSizeGroupMode;

/**
 * GtkSizeRequestMode:
 * @GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH: Prefer height-for-width geometry management
 * @GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT: Prefer width-for-height geometry management
 * @GTK_SIZE_REQUEST_CONSTANT_SIZE: Don’t trade height-for-width or width-for-height
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
 * @GTK_STATE_FLAG_NORMAL: State during normal operation
 * @GTK_STATE_FLAG_ACTIVE: Widget is active
 * @GTK_STATE_FLAG_PRELIGHT: Widget has a mouse pointer over it
 * @GTK_STATE_FLAG_SELECTED: Widget is selected
 * @GTK_STATE_FLAG_INSENSITIVE: Widget is insensitive
 * @GTK_STATE_FLAG_INCONSISTENT: Widget is inconsistent
 * @GTK_STATE_FLAG_FOCUSED: Widget has the keyboard focus
 * @GTK_STATE_FLAG_BACKDROP: Widget is in a background toplevel window
 * @GTK_STATE_FLAG_DIR_LTR: Widget is in left-to-right text direction
 * @GTK_STATE_FLAG_DIR_RTL: Widget is in right-to-left text direction
 * @GTK_STATE_FLAG_LINK: Widget is a link
 * @GTK_STATE_FLAG_VISITED: The location the widget points to has already been visited
 * @GTK_STATE_FLAG_CHECKED: Widget is checked
 * @GTK_STATE_FLAG_DROP_ACTIVE: Widget is highlighted as a drop target for DND
 * @GTK_STATE_FLAG_FOCUS_VISIBLE: Widget has the visible focus
 *
 * Describes a widget state. Widget states are used to match the widget
 * against CSS pseudo-classes. Note that GTK extends the regular CSS
 * classes and sometimes uses different names.
 */
typedef enum
{
  GTK_STATE_FLAG_NORMAL        = 0,
  GTK_STATE_FLAG_ACTIVE        = 1 << 0,
  GTK_STATE_FLAG_PRELIGHT      = 1 << 1,
  GTK_STATE_FLAG_SELECTED      = 1 << 2,
  GTK_STATE_FLAG_INSENSITIVE   = 1 << 3,
  GTK_STATE_FLAG_INCONSISTENT  = 1 << 4,
  GTK_STATE_FLAG_FOCUSED       = 1 << 5,
  GTK_STATE_FLAG_BACKDROP      = 1 << 6,
  GTK_STATE_FLAG_DIR_LTR       = 1 << 7,
  GTK_STATE_FLAG_DIR_RTL       = 1 << 8,
  GTK_STATE_FLAG_LINK          = 1 << 9,
  GTK_STATE_FLAG_VISITED       = 1 << 10,
  GTK_STATE_FLAG_CHECKED       = 1 << 11,
  GTK_STATE_FLAG_DROP_ACTIVE   = 1 << 12,
  GTK_STATE_FLAG_FOCUS_VISIBLE = 1 << 13
} GtkStateFlags;

/**
 * GtkBorderStyle:
 * @GTK_BORDER_STYLE_NONE: No visible border
 * @GTK_BORDER_STYLE_SOLID: A single line segment
 * @GTK_BORDER_STYLE_INSET: Looks as if the content is sunken into the canvas
 * @GTK_BORDER_STYLE_OUTSET: Looks as if the content is coming out of the canvas
 * @GTK_BORDER_STYLE_HIDDEN: Same as @GTK_BORDER_STYLE_NONE
 * @GTK_BORDER_STYLE_DOTTED: A series of round dots
 * @GTK_BORDER_STYLE_DASHED: A series of square-ended dashes
 * @GTK_BORDER_STYLE_DOUBLE: Two parallel lines with some space between them
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
 * minus) and “e” or “E” as in 3.14E+000.
 *
 * This enumeration may be extended in the future; input methods should
 * interpret unknown values as “free form”.
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
 * @GTK_INPUT_HINT_VERTICAL_WRITING: The text is vertical
 * @GTK_INPUT_HINT_EMOJI: Suggest offering Emoji support
 * @GTK_INPUT_HINT_NO_EMOJI: Suggest not offering Emoji support
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
  GTK_INPUT_HINT_INHIBIT_OSK         = 1 << 7,
  GTK_INPUT_HINT_VERTICAL_WRITING    = 1 << 8,
  GTK_INPUT_HINT_EMOJI               = 1 << 9,
  GTK_INPUT_HINT_NO_EMOJI            = 1 << 10
} GtkInputHints;

/**
 * GtkPropagationPhase:
 * @GTK_PHASE_NONE: Events are not delivered automatically. Those can be
 *   manually fed through gtk_event_controller_handle_event(). This should
 *   only be used when full control about when, or whether the controller
 *   handles the event is needed.
 * @GTK_PHASE_CAPTURE: Events are delivered in the capture phase. The
 *   capture phase happens before the bubble phase, runs from the toplevel down
 *   to the event widget. This option should only be used on containers that
 *   might possibly handle events before their children do.
 * @GTK_PHASE_BUBBLE: Events are delivered in the bubble phase. The bubble
 *   phase happens after the capture phase, and before the default handlers
 *   are run. This phase runs from the event widget, up to the toplevel.
 * @GTK_PHASE_TARGET: Events are delivered in the default widget event handlers,
 *   note that widget implementations must chain up on button, motion, touch and
 *   grab broken handlers for controllers in this phase to be run.
 *
 * Describes the stage at which events are fed into a #GtkEventController.
 */
typedef enum
{
  GTK_PHASE_NONE,
  GTK_PHASE_CAPTURE,
  GTK_PHASE_BUBBLE,
  GTK_PHASE_TARGET
} GtkPropagationPhase;

/**
 * GtkPropagationLimit:
 * @GTK_LIMIT_NONE: Events are handled regardless of what their
 *   target is.
 * @GTK_LIMIT_SAME_NATIVE: Events are only handled if their target
 *   is in the same #GtkNative as the event controllers widget. Note
 *   that some event types have two targets (origin and destination).
 *
 * Describes limits of a #GtkEventController for handling events
 * targeting other widgets.
 */
typedef enum
{
  GTK_LIMIT_NONE,
  GTK_LIMIT_SAME_NATIVE
} GtkPropagationLimit;

/**
 * GtkEventSequenceState:
 * @GTK_EVENT_SEQUENCE_NONE: The sequence is handled, but not grabbed.
 * @GTK_EVENT_SEQUENCE_CLAIMED: The sequence is handled and grabbed.
 * @GTK_EVENT_SEQUENCE_DENIED: The sequence is denied.
 *
 * Describes the state of a #GdkEventSequence in a #GtkGesture.
 */
typedef enum
{
  GTK_EVENT_SEQUENCE_NONE,
  GTK_EVENT_SEQUENCE_CLAIMED,
  GTK_EVENT_SEQUENCE_DENIED
} GtkEventSequenceState;

/**
 * GtkPanDirection:
 * @GTK_PAN_DIRECTION_LEFT: panned towards the left
 * @GTK_PAN_DIRECTION_RIGHT: panned towards the right
 * @GTK_PAN_DIRECTION_UP: panned upwards
 * @GTK_PAN_DIRECTION_DOWN: panned downwards
 *
 * Describes the panning direction of a #GtkGesturePan
 */
typedef enum
{
  GTK_PAN_DIRECTION_LEFT,
  GTK_PAN_DIRECTION_RIGHT,
  GTK_PAN_DIRECTION_UP,
  GTK_PAN_DIRECTION_DOWN
} GtkPanDirection;

/**
 * GtkPopoverConstraint:
 * @GTK_POPOVER_CONSTRAINT_NONE: Don't constrain the popover position
 *   beyond what is imposed by the implementation
 * @GTK_POPOVER_CONSTRAINT_WINDOW: Constrain the popover to the boundaries
 *   of the window that it is attached to
 *
 * Describes constraints to positioning of popovers. More values
 * may be added to this enumeration in the future.
 */
typedef enum
{
  GTK_POPOVER_CONSTRAINT_NONE,
  GTK_POPOVER_CONSTRAINT_WINDOW
} GtkPopoverConstraint;


typedef enum {
  GTK_PLACES_OPEN_NORMAL     = 1 << 0,
  GTK_PLACES_OPEN_NEW_TAB    = 1 << 1,
  GTK_PLACES_OPEN_NEW_WINDOW = 1 << 2
} GtkPlacesOpenFlags;

/**
 * GtkPickFlags:
 * @GTK_PICK_DEFAULT: The default behavior, include widgets that are receiving events
 * @GTK_PICK_INSENSITIVE: Include widgets that are insensitive
 * @GTK_PICK_NON_TARGETABLE: Include widgets that are marked as non-targetable. See #GtkWidget::can-target 
 * 
 * Flags that influence the behavior of gtk_widget_pick()
 */
typedef enum {
  GTK_PICK_DEFAULT        = 0,
  GTK_PICK_INSENSITIVE    = 1 << 0,
  GTK_PICK_NON_TARGETABLE = 1 << 1
} GtkPickFlags;

/**
 * GtkConstraintRelation:
 * @GTK_CONSTRAINT_RELATION_EQ: Equal
 * @GTK_CONSTRAINT_RELATION_LE: Less than, or equal
 * @GTK_CONSTRAINT_RELATION_GE: Greater than, or equal
 *
 * The relation between two terms of a constraint.
 */
typedef enum {
  GTK_CONSTRAINT_RELATION_LE = -1,
  GTK_CONSTRAINT_RELATION_EQ = 0,
  GTK_CONSTRAINT_RELATION_GE = 1
} GtkConstraintRelation;

/**
 * GtkConstraintStrength:
 * @GTK_CONSTRAINT_STRENGTH_REQUIRED: The constraint is required towards solving the layout
 * @GTK_CONSTRAINT_STRENGTH_STRONG: A strong constraint
 * @GTK_CONSTRAINT_STRENGTH_MEDIUM: A medium constraint
 * @GTK_CONSTRAINT_STRENGTH_WEAK: A weak constraint
 *
 * The strength of a constraint, expressed as a symbolic constant.
 *
 * The strength of a #GtkConstraint can be expressed with any positive
 * integer; the values of this enumeration can be used for readability.
 */
typedef enum {
  GTK_CONSTRAINT_STRENGTH_REQUIRED = 1001001000,
  GTK_CONSTRAINT_STRENGTH_STRONG   = 1000000000,
  GTK_CONSTRAINT_STRENGTH_MEDIUM   = 1000,
  GTK_CONSTRAINT_STRENGTH_WEAK     = 1
} GtkConstraintStrength;

/**
 * GtkConstraintAttribute:
 * @GTK_CONSTRAINT_ATTRIBUTE_NONE: No attribute, used for constant
 *   relations
 * @GTK_CONSTRAINT_ATTRIBUTE_LEFT: The left edge of a widget, regardless of
 *   text direction
 * @GTK_CONSTRAINT_ATTRIBUTE_RIGHT: The right edge of a widget, regardless
 *   of text direction
 * @GTK_CONSTRAINT_ATTRIBUTE_TOP: The top edge of a widget
 * @GTK_CONSTRAINT_ATTRIBUTE_BOTTOM: The bottom edge of a widget
 * @GTK_CONSTRAINT_ATTRIBUTE_START: The leading edge of a widget, depending
 *   on text direction; equivalent to %GTK_CONSTRAINT_ATTRIBUTE_LEFT for LTR
 *   languages, and %GTK_CONSTRAINT_ATTRIBUTE_RIGHT for RTL ones
 * @GTK_CONSTRAINT_ATTRIBUTE_END: The trailing edge of a widget, depending
 *   on text direction; equivalent to %GTK_CONSTRAINT_ATTRIBUTE_RIGHT for LTR
 *   languages, and %GTK_CONSTRAINT_ATTRIBUTE_LEFT for RTL ones
 * @GTK_CONSTRAINT_ATTRIBUTE_WIDTH: The width of a widget
 * @GTK_CONSTRAINT_ATTRIBUTE_HEIGHT: The height of a widget
 * @GTK_CONSTRAINT_ATTRIBUTE_CENTER_X: The center of a widget, on the
 *   horizontal axis
 * @GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y: The center of a widget, on the
 *   vertical axis
 * @GTK_CONSTRAINT_ATTRIBUTE_BASELINE: The baseline of a widget
 *
 * The widget attributes that can be used when creating a #GtkConstraint.
 */
typedef enum {
  GTK_CONSTRAINT_ATTRIBUTE_NONE,
  GTK_CONSTRAINT_ATTRIBUTE_LEFT,
  GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
  GTK_CONSTRAINT_ATTRIBUTE_TOP,
  GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
  GTK_CONSTRAINT_ATTRIBUTE_START,
  GTK_CONSTRAINT_ATTRIBUTE_END,
  GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
  GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
  GTK_CONSTRAINT_ATTRIBUTE_CENTER_X,
  GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y,
  GTK_CONSTRAINT_ATTRIBUTE_BASELINE
} GtkConstraintAttribute;

/**
 * GtkConstraintVflParserError:
 * @GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL: Invalid or unknown symbol
 * @GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_ATTRIBUTE: Invalid or unknown attribute
 * @GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_VIEW: Invalid or unknown view
 * @GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_METRIC: Invalid or unknown metric
 * @GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_PRIORITY: Invalid or unknown priority
 * @GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_RELATION: Invalid or unknown relation
 *
 * Domain for VFL parsing errors.
 */
typedef enum {
  GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL,
  GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_ATTRIBUTE,
  GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_VIEW,
  GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_METRIC,
  GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_PRIORITY,
  GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_RELATION
} GtkConstraintVflParserError;

#endif /* __GTK_ENUMS_H__ */
