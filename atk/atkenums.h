#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * AtkHyperlinkStateFlags:
 * @ATK_HYPERLINK_IS_INLINE: Link is inline
 *
 * Describes the type of link
 */ 
typedef enum 
{
  ATK_HYPERLINK_IS_INLINE = 1 << 0
} AtkHyperlinkStateFlags;

/**
 *AtkStateType:
 *@ATK_STATE_INVALID: Indicates an invalid state - probably an error condition.
 *@ATK_STATE_ACTIVE: Indicates a window is currently the active window, or an object is the active subelement within a container or table. ATK_STATE_ACTIVE should not be used for objects which have ATK_STATE_FOCUSABLE or ATK_STATE_SELECTABLE: Those objects should use ATK_STATE_FOCUSED and ATK_STATE_SELECTED respectively. ATK_STATE_ACTIVE is a means to indicate that an object which is not focusable and not selectable is the currently-active item within its parent container.
 *@ATK_STATE_ARMED: Indicates that the object is 'armed', i.e. will be activated by if a pointer button-release event occurs within its bounds.  Buttons often enter this state when a pointer click occurs within their bounds, as a precursor to activation. ATK_STATE_ARMED has been deprecated since ATK-2.16 and should not be used in newly-written code.
 *@ATK_STATE_BUSY:  Indicates the current object is busy, i.e. onscreen representation is in the process of changing, or the object is temporarily unavailable for interaction due to activity already in progress.  This state may be used by implementors of Document to indicate that content loading is underway.  It also may indicate other 'pending' conditions; clients may wish to interrogate this object when the ATK_STATE_BUSY flag is removed.
 *@ATK_STATE_CHECKED: Indicates this object is currently checked, for instance a checkbox is 'non-empty'.
 *@ATK_STATE_DEFUNCT: Indicates that this object no longer has a valid backing widget (for instance, if its peer object has been destroyed)
 *@ATK_STATE_EDITABLE: Indicates that this object can contain text, and that the
 * user can change the textual contents of this object by editing those contents
 * directly. For an object which is expected to be editable due to its type, but
 * which cannot be edited due to the application or platform preventing the user
 * from doing so, that object's #AtkStateSet should lack ATK_STATE_EDITABLE and
 * should contain ATK_STATE_READ_ONLY.
 *@ATK_STATE_ENABLED: 	Indicates that this object is enabled, i.e. that it currently reflects some application state. Objects that are "greyed out" may lack this state, and may lack the STATE_SENSITIVE if direct user interaction cannot cause them to acquire STATE_ENABLED. See also: ATK_STATE_SENSITIVE
 *@ATK_STATE_EXPANDABLE: Indicates this object allows progressive disclosure of its children
 *@ATK_STATE_EXPANDED: Indicates this object its expanded - see ATK_STATE_EXPANDABLE above
 *@ATK_STATE_FOCUSABLE: Indicates this object can accept keyboard focus, which means all events resulting from typing on the keyboard will normally be passed to it when it has focus
 *@ATK_STATE_FOCUSED: Indicates this object currently has the keyboard focus
 *@ATK_STATE_HORIZONTAL: Indicates the orientation of this object is horizontal; used, for instance, by objects of ATK_ROLE_SCROLL_BAR.  For objects where vertical/horizontal orientation is especially meaningful.
 *@ATK_STATE_ICONIFIED: Indicates this object is minimized and is represented only by an icon
 *@ATK_STATE_MODAL: Indicates something must be done with this object before the user can interact with an object in a different window
 *@ATK_STATE_MULTI_LINE: Indicates this (text) object can contain multiple lines of text
 *@ATK_STATE_MULTISELECTABLE: Indicates this object allows more than one of its children to be selected at the same time, or in the case of text objects, that the object supports non-contiguous text selections.
 *@ATK_STATE_OPAQUE: Indicates this object paints every pixel within its rectangular region.
 *@ATK_STATE_PRESSED: Indicates this object is currently pressed.
 *@ATK_STATE_RESIZABLE: Indicates the size of this object is not fixed
 *@ATK_STATE_SELECTABLE: Indicates this object is the child of an object that allows its children to be selected and that this child is one of those children that can be selected
 *@ATK_STATE_SELECTED: Indicates this object is the child of an object that allows its children to be selected and that this child is one of those children that has been selected
 *@ATK_STATE_SENSITIVE: Indicates this object is sensitive, e.g. to user interaction. 
 * STATE_SENSITIVE usually accompanies STATE_ENABLED for user-actionable controls,
 * but may be found in the absence of STATE_ENABLED if the current visible state of the 
 * control is "disconnected" from the application state.  In such cases, direct user interaction
 * can often result in the object gaining STATE_SENSITIVE, for instance if a user makes 
 * an explicit selection using an object whose current state is ambiguous or undefined.
 * @see STATE_ENABLED, STATE_INDETERMINATE.
 *@ATK_STATE_SHOWING: Indicates this object, the object's parent, the object's parent's parent, and so on, 
 * are all 'shown' to the end-user, i.e. subject to "exposure" if blocking or obscuring objects do not interpose
 * between this object and the top of the window stack.
 *@ATK_STATE_SINGLE_LINE: Indicates this (text) object can contain only a single line of text
 *@ATK_STATE_STALE: Indicates that the information returned for this object may no longer be
 * synchronized with the application state.  This is implied if the object has STATE_TRANSIENT,
 * and can also occur towards the end of the object peer's lifecycle. It can also be used to indicate that 
 * the index associated with this object has changed since the user accessed the object (in lieu of
 * "index-in-parent-changed" events).
 *@ATK_STATE_TRANSIENT: Indicates this object is transient, i.e. a snapshot which may not emit events when its
 * state changes.  Data from objects with ATK_STATE_TRANSIENT should not be cached, since there may be no
 * notification given when the cached data becomes obsolete.
 *@ATK_STATE_VERTICAL: Indicates the orientation of this object is vertical
 *@ATK_STATE_VISIBLE: Indicates this object is visible, e.g. has been explicitly marked for exposure to the user.
 * **note**: %ATK_STATE_VISIBLE is no guarantee that the object is actually unobscured on the screen, only
 * that it is 'potentially' visible, barring obstruction, being scrolled or clipped out of the 
 * field of view, or having an ancestor container that has not yet made visible.
 * A widget is potentially onscreen if it has both %ATK_STATE_VISIBLE and %ATK_STATE_SHOWING.
 * The absence of %ATK_STATE_VISIBLE and %ATK_STATE_SHOWING is semantically equivalent to saying
 * that an object is 'hidden'.  See also %ATK_STATE_TRUNCATED, which applies if an object with
 * %ATK_STATE_VISIBLE and %ATK_STATE_SHOWING set lies within a viewport which means that its
 * contents are clipped, e.g. a truncated spreadsheet cell or
 * an image within a scrolling viewport.  Mostly useful for screen-review and magnification
 * algorithms.
 *@ATK_STATE_MANAGES_DESCENDANTS: Indicates that "active-descendant-changed" event
 * is sent when children become 'active' (i.e. are selected or navigated to onscreen).
 * Used to prevent need to enumerate all children in very large containers, like tables.
 * The presence of STATE_MANAGES_DESCENDANTS is an indication to the client.
 * that the children should not, and need not, be enumerated by the client.
 * Objects implementing this state are expected to provide relevant state
 * notifications to listening clients, for instance notifications of visibility
 * changes and activation of their contained child objects, without the client 
 * having previously requested references to those children.
 *@ATK_STATE_INDETERMINATE: Indicates that the value, or some other quantifiable
 * property, of this AtkObject cannot be fully determined. In the case of a large
 * data set in which the total number of items in that set is unknown (e.g. 1 of
 * 999+), implementors should expose the currently-known set size (999) along
 * with this state. In the case of a check box, this state should be used to
 * indicate that the check box is a tri-state check box which is currently
 * neither checked nor unchecked.
 *@ATK_STATE_TRUNCATED: Indicates that an object is truncated, e.g. a text value in a speradsheet cell.
 *@ATK_STATE_REQUIRED: Indicates that explicit user interaction with an object is required by the user interface, e.g. a required field in a "web-form" interface.
 *@ATK_STATE_INVALID_ENTRY: Indicates that the object has encountered an error condition due to failure of input validation. For instance, a form control may acquire this state in response to invalid or malformed user input.
 *@ATK_STATE_SUPPORTS_AUTOCOMPLETION:  Indicates that the object in question implements some form of ¨typeahead¨ or 
 * pre-selection behavior whereby entering the first character of one or more sub-elements
 * causes those elements to scroll into view or become selected.  Subsequent character input
 * may narrow the selection further as long as one or more sub-elements match the string.
 * This state is normally only useful and encountered on objects that implement Selection.
 * In some cases the typeahead behavior may result in full or partial ¨completion¨ of 
 * the data in the input field, in which case these input events may trigger text-changed
 * events from the AtkText interface.  This state supplants @ATK_ROLE_AUTOCOMPLETE.
 *@ATK_STATE_SELECTABLE_TEXT:Indicates that the object in question supports text selection. It should only be exposed on objects which implement the Text interface, in order to distinguish this state from @ATK_STATE_SELECTABLE, which infers that the object in question is a selectable child of an object which implements Selection. While similar, text selection and subelement selection are distinct operations.
 *@ATK_STATE_DEFAULT: Indicates that the object is the "default" active component, i.e. the object which is activated by an end-user press of the "Enter" or "Return" key.  Typically a "close" or "submit" button.
 *@ATK_STATE_ANIMATED: Indicates that the object changes its appearance dynamically as an inherent part of its presentation.  This state may come and go if an object is only temporarily animated on the way to a 'final' onscreen presentation.
 * **note**: some applications, notably content viewers, may not be able to detect
 * all kinds of animated content.  Therefore the absence of this state should not
 * be taken as definitive evidence that the object's visual representation is
 * static; this state is advisory.
 *@ATK_STATE_VISITED: Indicates that the object (typically a hyperlink) has already been 'activated', and/or its backing data has already been downloaded, rendered, or otherwise "visited".
 *@ATK_STATE_CHECKABLE: Indicates this object has the potential to be
 *  checked, such as a checkbox or toggle-able table cell. @Since:
 *  ATK-2.12
 *@ATK_STATE_HAS_POPUP: Indicates that the object has a popup context
 * menu or sub-level menu which may or may not be showing. This means
 * that activation renders conditional content.  Note that ordinary
 * tooltips are not considered popups in this context. @Since: ATK-2.12
 *@ATK_STATE_HAS_TOOLTIP: Indicates this object has a tooltip. @Since: ATK-2.16
 *@ATK_STATE_READ_ONLY: Indicates that a widget which is ENABLED and SENSITIVE
 * has a value which can be read, but not modified, by the user. Note that this
 * state should only be applied to widget types whose value is normally directly
 * user modifiable, such as check boxes, radio buttons, spin buttons, text input
 * fields, and combo boxes, as a means to convey that the expected interaction
 * with that widget is not possible. When the expected interaction with a
 * widget does not include modification by the user, as is the case with
 * labels and containers, ATK_STATE_READ_ONLY should not be applied. See also
 * ATK_STATE_EDITABLE. @Since: ATK-2-16
 *@ATK_STATE_LAST_DEFINED: Not a valid state, used for finding end of enumeration
 *
 *The possible types of states of an object
 **/
typedef enum
{
  ATK_STATE_INVALID,
  ATK_STATE_ACTIVE,
  ATK_STATE_ARMED,
  ATK_STATE_BUSY,
  ATK_STATE_CHECKED,
  ATK_STATE_DEFUNCT,
  ATK_STATE_EDITABLE,
  ATK_STATE_ENABLED,
  ATK_STATE_EXPANDABLE,
  ATK_STATE_EXPANDED,
  ATK_STATE_FOCUSABLE,
  ATK_STATE_FOCUSED,
  ATK_STATE_HORIZONTAL,
  ATK_STATE_ICONIFIED,
  ATK_STATE_MODAL,
  ATK_STATE_MULTI_LINE,
  ATK_STATE_MULTISELECTABLE,
  ATK_STATE_OPAQUE,
  ATK_STATE_PRESSED,
  ATK_STATE_RESIZABLE,
  ATK_STATE_SELECTABLE,
  ATK_STATE_SELECTED,
  ATK_STATE_SENSITIVE,
  ATK_STATE_SHOWING,
  ATK_STATE_SINGLE_LINE,
  ATK_STATE_STALE,
  ATK_STATE_TRANSIENT,
  ATK_STATE_VERTICAL,
  ATK_STATE_VISIBLE,
  ATK_STATE_MANAGES_DESCENDANTS,
  ATK_STATE_INDETERMINATE,
  ATK_STATE_TRUNCATED,
  ATK_STATE_REQUIRED,
  ATK_STATE_INVALID_ENTRY,
  ATK_STATE_SUPPORTS_AUTOCOMPLETION,
  ATK_STATE_SELECTABLE_TEXT,
  ATK_STATE_DEFAULT,
  ATK_STATE_ANIMATED,
  ATK_STATE_VISITED,
  ATK_STATE_CHECKABLE,
  ATK_STATE_HAS_POPUP,
  ATK_STATE_HAS_TOOLTIP,
  ATK_STATE_READ_ONLY,
  ATK_STATE_LAST_DEFINED
} AtkStateType;

/**
 * AtkScrollType:
 * @ATK_SCROLL_TOP_LEFT: Scroll the object vertically and horizontally to bring
 *   its top left corner to the top left corner of the window.
 * @ATK_SCROLL_BOTTOM_RIGHT: Scroll the object vertically and horizontally to
 *   bring its bottom right corner to the bottom right corner of the window.
 * @ATK_SCROLL_TOP_EDGE: Scroll the object vertically to bring its top edge to
 *   the top edge of the window.
 * @ATK_SCROLL_BOTTOM_EDGE: Scroll the object vertically to bring its bottom
 *   edge to the bottom edge of the window.
 * @ATK_SCROLL_LEFT_EDGE: Scroll the object vertically and horizontally to bring
 *   its left edge to the left edge of the window.
 * @ATK_SCROLL_RIGHT_EDGE: Scroll the object vertically and horizontally to
 *   bring its right edge to the right edge of the window.
 * @ATK_SCROLL_ANYWHERE: Scroll the object vertically and horizontally so that
 *   as much as possible of the object becomes visible. The exact placement is
 *   determined by the application.
 *
 * Specifies where an object should be placed on the screen when using scroll_to.
 */
typedef enum {
  ATK_SCROLL_TOP_LEFT,
  ATK_SCROLL_BOTTOM_RIGHT,
  ATK_SCROLL_TOP_EDGE,
  ATK_SCROLL_BOTTOM_EDGE,
  ATK_SCROLL_LEFT_EDGE,
  ATK_SCROLL_RIGHT_EDGE,
  ATK_SCROLL_ANYWHERE
} AtkScrollType;

/**
 *AtkRelationType:
 *@ATK_RELATION_NULL: Not used, represens "no relationship" or an error condition.
 *@ATK_RELATION_CONTROLLED_BY: Indicates an object controlled by one or more target objects.
 *@ATK_RELATION_CONTROLLER_FOR: Indicates an object is an controller for one or more target objects.
 *@ATK_RELATION_LABEL_FOR: Indicates an object is a label for one or more target objects.
 *@ATK_RELATION_LABELLED_BY: Indicates an object is labelled by one or more target objects.
 *@ATK_RELATION_MEMBER_OF: Indicates an object is a member of a group of one or more target objects.
 *@ATK_RELATION_NODE_CHILD_OF: Indicates an object is a cell in a treetable which is displayed because a cell in the same column is expanded and identifies that cell.
 *@ATK_RELATION_FLOWS_TO: Indicates that the object has content that flows logically to another
 *  AtkObject in a sequential way, (for instance text-flow).
 *@ATK_RELATION_FLOWS_FROM: Indicates that the object has content that flows logically from
 *  another AtkObject in a sequential way, (for instance text-flow).
 *@ATK_RELATION_SUBWINDOW_OF: Indicates a subwindow attached to a component but otherwise has no connection in  the UI heirarchy to that component.
 *@ATK_RELATION_EMBEDS: Indicates that the object visually embeds 
 *  another object's content, i.e. this object's content flows around 
 *  another's content.
 *@ATK_RELATION_EMBEDDED_BY: Reciprocal of %ATK_RELATION_EMBEDS, indicates that
 *  this object's content is visualy embedded in another object.
 *@ATK_RELATION_POPUP_FOR: Indicates that an object is a popup for another object.
 *@ATK_RELATION_PARENT_WINDOW_OF: Indicates that an object is a parent window of another object.
 *@ATK_RELATION_DESCRIBED_BY: Reciprocal of %ATK_RELATION_DESCRIPTION_FOR. Indicates that one
 * or more target objects provide descriptive information about this object. This relation
 * type is most appropriate for information that is not essential as its presentation may
 * be user-configurable and/or limited to an on-demand mechanism such as an assistive
 * technology command. For brief, essential information such as can be found in a widget's
 * on-screen label, use %ATK_RELATION_LABELLED_BY. For an on-screen error message, use
 * %ATK_RELATION_ERROR_MESSAGE. For lengthy extended descriptive information contained in
 * an on-screen object, consider using %ATK_RELATION_DETAILS as assistive technologies may
 * provide a means for the user to navigate to objects containing detailed descriptions so
 * that their content can be more closely reviewed.
 *@ATK_RELATION_DESCRIPTION_FOR: Reciprocal of %ATK_RELATION_DESCRIBED_BY. Indicates that this
 * object provides descriptive information about the target object(s). See also
 * %ATK_RELATION_DETAILS_FOR and %ATK_RELATION_ERROR_FOR.
 *@ATK_RELATION_NODE_PARENT_OF: Indicates an object is a cell in a treetable and is expanded to display other cells in the same column.
 *@ATK_RELATION_DETAILS: Reciprocal of %ATK_RELATION_DETAILS_FOR. Indicates that this object
 * has a detailed or extended description, the contents of which can be found in the target
 * object(s). This relation type is most appropriate for information that is sufficiently
 * lengthy as to make navigation to the container of that information desirable. For less
 * verbose information suitable for announcement only, see %ATK_RELATION_DESCRIBED_BY. If
 * the detailed information describes an error condition, %ATK_RELATION_ERROR_FOR should be
 * used instead. @Since: ATK-2.26.
 *@ATK_RELATION_DETAILS_FOR: Reciprocal of %ATK_RELATION_DETAILS. Indicates that this object
 * provides a detailed or extended description about the target object(s). See also
 * %ATK_RELATION_DESCRIPTION_FOR and %ATK_RELATION_ERROR_FOR. @Since: ATK-2.26.
 *@ATK_RELATION_ERROR_MESSAGE: Reciprocal of %ATK_RELATION_ERROR_FOR. Indicates that this object
 * has one or more errors, the nature of which is described in the contents of the target
 * object(s). Objects that have this relation type should also contain %ATK_STATE_INVALID_ENTRY
 * in their #AtkStateSet. @Since: ATK-2.26.
 *@ATK_RELATION_ERROR_FOR: Reciprocal of %ATK_RELATION_ERROR_MESSAGE. Indicates that this object
 * contains an error message describing an invalid condition in the target object(s). @Since:
 * ATK_2.26.
 *@ATK_RELATION_LAST_DEFINED: Not used, this value indicates the end of the enumeration.
 * 
 *Describes the type of the relation
 **/
typedef enum
{
  ATK_RELATION_NULL = 0,
  ATK_RELATION_CONTROLLED_BY,
  ATK_RELATION_CONTROLLER_FOR,
  ATK_RELATION_LABEL_FOR,
  ATK_RELATION_LABELLED_BY,
  ATK_RELATION_MEMBER_OF,
  ATK_RELATION_NODE_CHILD_OF,
  ATK_RELATION_FLOWS_TO,
  ATK_RELATION_FLOWS_FROM,
  ATK_RELATION_SUBWINDOW_OF, 
  ATK_RELATION_EMBEDS, 
  ATK_RELATION_EMBEDDED_BY, 
  ATK_RELATION_POPUP_FOR, 
  ATK_RELATION_PARENT_WINDOW_OF, 
  ATK_RELATION_DESCRIBED_BY,
  ATK_RELATION_DESCRIPTION_FOR,
  ATK_RELATION_NODE_PARENT_OF,
  ATK_RELATION_DETAILS,
  ATK_RELATION_DETAILS_FOR,
  ATK_RELATION_ERROR_MESSAGE,
  ATK_RELATION_ERROR_FOR,
  ATK_RELATION_LAST_DEFINED
} AtkRelationType;

/**
 *AtkTextAttribute:
 *@ATK_TEXT_ATTR_INVALID: Invalid attribute, like bad spelling or grammar.
 *@ATK_TEXT_ATTR_LEFT_MARGIN: The pixel width of the left margin
 *@ATK_TEXT_ATTR_RIGHT_MARGIN: The pixel width of the right margin
 *@ATK_TEXT_ATTR_INDENT: The number of pixels that the text is indented
 *@ATK_TEXT_ATTR_INVISIBLE: Either "true" or "false" indicating whether text is visible or not
 *@ATK_TEXT_ATTR_EDITABLE: Either "true" or "false" indicating whether text is editable or not
 *@ATK_TEXT_ATTR_PIXELS_ABOVE_LINES: Pixels of blank space to leave above each newline-terminated line. 
 *@ATK_TEXT_ATTR_PIXELS_BELOW_LINES: Pixels of blank space to leave below each newline-terminated line.
 *@ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP: Pixels of blank space to leave between wrapped lines inside the same newline-terminated line (paragraph).
 *@ATK_TEXT_ATTR_BG_FULL_HEIGHT: "true" or "false" whether to make the background color for each character the height of the highest font used on the current line, or the height of the font used for the current character.
 *@ATK_TEXT_ATTR_RISE: Number of pixels that the characters are risen above the baseline. See also ATK_TEXT_ATTR_TEXT_POSITION.
 *@ATK_TEXT_ATTR_UNDERLINE: "none", "single", "double", "low", or "error"
 *@ATK_TEXT_ATTR_STRIKETHROUGH: "true" or "false" whether the text is strikethrough 
 *@ATK_TEXT_ATTR_SIZE: The size of the characters in points. eg: 10
 *@ATK_TEXT_ATTR_SCALE: The scale of the characters. The value is a string representation of a double 
 *@ATK_TEXT_ATTR_WEIGHT: The weight of the characters.
 *@ATK_TEXT_ATTR_LANGUAGE: The language used
 *@ATK_TEXT_ATTR_FAMILY_NAME: The font family name
 *@ATK_TEXT_ATTR_BG_COLOR: The background color. The value is an RGB value of the format "%u,%u,%u"
 *@ATK_TEXT_ATTR_FG_COLOR:The foreground color. The value is an RGB value of the format "%u,%u,%u"
 *@ATK_TEXT_ATTR_BG_STIPPLE: "true" if a #GdkBitmap is set for stippling the background color.
 *@ATK_TEXT_ATTR_FG_STIPPLE: "true" if a #GdkBitmap is set for stippling the foreground color.
 *@ATK_TEXT_ATTR_WRAP_MODE: The wrap mode of the text, if any. Values are "none", "char", "word", or "word_char".
 *@ATK_TEXT_ATTR_DIRECTION: The direction of the text, if set. Values are "none", "ltr" or "rtl" 
 *@ATK_TEXT_ATTR_JUSTIFICATION: The justification of the text, if set. Values are "left", "right", "center" or "fill" 
 *@ATK_TEXT_ATTR_STRETCH: The stretch of the text, if set. Values are "ultra_condensed", "extra_condensed", "condensed", "semi_condensed", "normal", "semi_expanded", "expanded", "extra_expanded" or "ultra_expanded"
 *@ATK_TEXT_ATTR_VARIANT: The capitalization variant of the text, if set. Values are "normal" or "small_caps"
 *@ATK_TEXT_ATTR_STYLE: The slant style of the text, if set. Values are "normal", "oblique" or "italic"
 *@ATK_TEXT_ATTR_TEXT_POSITION: The vertical position with respect to the baseline. Values are "baseline", "super", or "sub". Note that a super or sub text attribute refers to position with respect to the baseline of the prior character.
 *@ATK_TEXT_ATTR_LAST_DEFINED: not a valid text attribute, used for finding end of enumeration
 *
 * Describes the text attributes supported
 **/
typedef enum
{
  ATK_TEXT_ATTR_INVALID = 0,
  ATK_TEXT_ATTR_LEFT_MARGIN,
  ATK_TEXT_ATTR_RIGHT_MARGIN,
  ATK_TEXT_ATTR_INDENT,
  ATK_TEXT_ATTR_INVISIBLE,
  ATK_TEXT_ATTR_EDITABLE,
  ATK_TEXT_ATTR_PIXELS_ABOVE_LINES,
  ATK_TEXT_ATTR_PIXELS_BELOW_LINES,
  ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP,
  ATK_TEXT_ATTR_BG_FULL_HEIGHT,
  ATK_TEXT_ATTR_RISE,
  ATK_TEXT_ATTR_UNDERLINE,
  ATK_TEXT_ATTR_STRIKETHROUGH,
  ATK_TEXT_ATTR_SIZE,
  ATK_TEXT_ATTR_SCALE,
  ATK_TEXT_ATTR_WEIGHT,
  ATK_TEXT_ATTR_LANGUAGE,
  ATK_TEXT_ATTR_FAMILY_NAME,
  ATK_TEXT_ATTR_BG_COLOR,
  ATK_TEXT_ATTR_FG_COLOR,
  ATK_TEXT_ATTR_BG_STIPPLE,
  ATK_TEXT_ATTR_FG_STIPPLE,
  ATK_TEXT_ATTR_WRAP_MODE,
  ATK_TEXT_ATTR_DIRECTION,
  ATK_TEXT_ATTR_JUSTIFICATION,
  ATK_TEXT_ATTR_STRETCH,
  ATK_TEXT_ATTR_VARIANT,
  ATK_TEXT_ATTR_STYLE,
  ATK_TEXT_ATTR_TEXT_POSITION,
  ATK_TEXT_ATTR_LAST_DEFINED
} AtkTextAttribute;

/**
 *AtkTextBoundary:
 *@ATK_TEXT_BOUNDARY_CHAR: Boundary is the boundary between characters
 * (including non-printing characters)
 *@ATK_TEXT_BOUNDARY_WORD_START: Boundary is the start (i.e. first character) of a word.
 *@ATK_TEXT_BOUNDARY_WORD_END: Boundary is the end (i.e. last
 * character) of a word.
 *@ATK_TEXT_BOUNDARY_SENTENCE_START: Boundary is the first character in a sentence.
 *@ATK_TEXT_BOUNDARY_SENTENCE_END: Boundary is the last (terminal)
 * character in a sentence; in languages which use "sentence stop"
 * punctuation such as English, the boundary is thus the '.', '?', or
 * similar terminal punctuation character.
 *@ATK_TEXT_BOUNDARY_LINE_START: Boundary is the initial character of the content or a
 * character immediately following a newline, linefeed, or return character.
 *@ATK_TEXT_BOUNDARY_LINE_END: Boundary is the linefeed, or return
 * character.
 *
 * Text boundary types used for specifying boundaries for regions of text.
 * This enumeration is deprecated since 2.9.4 and should not be used. Use
 * AtkTextGranularity with #atk_text_get_string_at_offset instead.
 **/
typedef enum {
  ATK_TEXT_BOUNDARY_CHAR,
  ATK_TEXT_BOUNDARY_WORD_START,
  ATK_TEXT_BOUNDARY_WORD_END,
  ATK_TEXT_BOUNDARY_SENTENCE_START,
  ATK_TEXT_BOUNDARY_SENTENCE_END,
  ATK_TEXT_BOUNDARY_LINE_START,
  ATK_TEXT_BOUNDARY_LINE_END
} AtkTextBoundary;

/**
 *AtkTextGranularity:
 *@ATK_TEXT_GRANULARITY_CHAR: Granularity is defined by the boundaries between characters
 * (including non-printing characters)
 *@ATK_TEXT_GRANULARITY_WORD: Granularity is defined by the boundaries of a word,
 * starting at the beginning of the current word and finishing at the beginning of
 * the following one, if present.
 *@ATK_TEXT_GRANULARITY_SENTENCE: Granularity is defined by the boundaries of a sentence,
 * starting at the beginning of the current sentence and finishing at the beginning of
 * the following one, if present.
 *@ATK_TEXT_GRANULARITY_LINE: Granularity is defined by the boundaries of a line,
 * starting at the beginning of the current line and finishing at the beginning of
 * the following one, if present.
 *@ATK_TEXT_GRANULARITY_PARAGRAPH: Granularity is defined by the boundaries of a paragraph,
 * starting at the beginning of the current paragraph and finishing at the beginning of
 * the following one, if present.
 *
 * Text granularity types used for specifying the granularity of the region of
 * text we are interested in.
 **/
typedef enum {
  ATK_TEXT_GRANULARITY_CHAR,
  ATK_TEXT_GRANULARITY_WORD,
  ATK_TEXT_GRANULARITY_SENTENCE,
  ATK_TEXT_GRANULARITY_LINE,
  ATK_TEXT_GRANULARITY_PARAGRAPH
} AtkTextGranularity;

/**
 *AtkTextClipType:
 *@ATK_TEXT_CLIP_NONE: No clipping to be done
 *@ATK_TEXT_CLIP_MIN: Text clipped by min coordinate is omitted
 *@ATK_TEXT_CLIP_MAX: Text clipped by max coordinate is omitted
 *@ATK_TEXT_CLIP_BOTH: Only text fully within mix/max bound is retained
 *
 *Describes the type of clipping required.
 **/
typedef enum {
    ATK_TEXT_CLIP_NONE,
    ATK_TEXT_CLIP_MIN,
    ATK_TEXT_CLIP_MAX,
    ATK_TEXT_CLIP_BOTH
} AtkTextClipType;

/**
 *AtkKeyEventType:
 *@ATK_KEY_EVENT_PRESS: specifies a key press event
 *@ATK_KEY_EVENT_RELEASE: specifies a key release event
 *@ATK_KEY_EVENT_LAST_DEFINED: Not a valid value; specifies end of enumeration
 *
 *Specifies the type of a keyboard evemt.
 **/
typedef enum
{
  ATK_KEY_EVENT_PRESS,
  ATK_KEY_EVENT_RELEASE,
  ATK_KEY_EVENT_LAST_DEFINED
} AtkKeyEventType;

/**
 *AtkCoordType:
 *@ATK_XY_SCREEN: specifies xy coordinates relative to the screen
 *@ATK_XY_WINDOW: specifies xy coordinates relative to the widget's
 * top-level window
 *@ATK_XY_PARENT: specifies xy coordinates relative to the widget's
 * immediate parent. Since: 2.30
 *
 *Specifies how xy coordinates are to be interpreted. Used by functions such
 *as atk_component_get_position() and atk_text_get_character_extents() 
 **/
typedef enum {
  ATK_XY_SCREEN,
  ATK_XY_WINDOW,
  ATK_XY_PARENT
}AtkCoordType;

/**
 *AtkRole:
 *@ATK_ROLE_INVALID: Invalid role
 *@ATK_ROLE_ACCEL_LABEL: A label which represents an accelerator
 *@ATK_ROLE_ALERT: An object which is an alert to the user. Assistive Technologies typically respond to ATK_ROLE_ALERT by reading the entire onscreen contents of containers advertising this role.  Should be used for warning dialogs, etc.
 *@ATK_ROLE_ANIMATION: An object which is an animated image
 *@ATK_ROLE_ARROW: An arrow in one of the four cardinal directions
 *@ATK_ROLE_CALENDAR:  An object that displays a calendar and allows the user to select a date
 *@ATK_ROLE_CANVAS: An object that can be drawn into and is used to trap events
 *@ATK_ROLE_CHECK_BOX: A choice that can be checked or unchecked and provides a separate indicator for the current state
 *@ATK_ROLE_CHECK_MENU_ITEM: A menu item with a check box
 *@ATK_ROLE_COLOR_CHOOSER: A specialized dialog that lets the user choose a color
 *@ATK_ROLE_COLUMN_HEADER: The header for a column of data
 *@ATK_ROLE_COMBO_BOX: A collapsible list of choices the user can select from
 *@ATK_ROLE_DATE_EDITOR: An object whose purpose is to allow a user to edit a date
 *@ATK_ROLE_DESKTOP_ICON: An inconifed internal frame within a DESKTOP_PANE
 *@ATK_ROLE_DESKTOP_FRAME: A pane that supports internal frames and iconified versions of those internal frames
 *@ATK_ROLE_DIAL: An object whose purpose is to allow a user to set a value
 *@ATK_ROLE_DIALOG: A top level window with title bar and a border
 *@ATK_ROLE_DIRECTORY_PANE: A pane that allows the user to navigate through and select the contents of a directory
 *@ATK_ROLE_DRAWING_AREA: An object used for drawing custom user interface elements
 *@ATK_ROLE_FILE_CHOOSER: A specialized dialog that lets the user choose a file
 *@ATK_ROLE_FILLER: A object that fills up space in a user interface
 *@ATK_ROLE_FONT_CHOOSER: A specialized dialog that lets the user choose a font
 *@ATK_ROLE_FRAME: A top level window with a title bar, border, menubar, etc.
 *@ATK_ROLE_GLASS_PANE: A pane that is guaranteed to be painted on top of all panes beneath it
 *@ATK_ROLE_HTML_CONTAINER: A document container for HTML, whose children represent the document content
 *@ATK_ROLE_ICON: A small fixed size picture, typically used to decorate components
 *@ATK_ROLE_IMAGE: An object whose primary purpose is to display an image
 *@ATK_ROLE_INTERNAL_FRAME: A frame-like object that is clipped by a desktop pane
 *@ATK_ROLE_LABEL: An object used to present an icon or short string in an interface
 *@ATK_ROLE_LAYERED_PANE: A specialized pane that allows its children to be drawn in layers, providing a form of stacking order
 *@ATK_ROLE_LIST: An object that presents a list of objects to the user and allows the user to select one or more of them 
 *@ATK_ROLE_LIST_ITEM: An object that represents an element of a list 
 *@ATK_ROLE_MENU: An object usually found inside a menu bar that contains a list of actions the user can choose from
 *@ATK_ROLE_MENU_BAR: An object usually drawn at the top of the primary dialog box of an application that contains a list of menus the user can choose from 
 *@ATK_ROLE_MENU_ITEM: An object usually contained in a menu that presents an action the user can choose
 *@ATK_ROLE_OPTION_PANE: A specialized pane whose primary use is inside a DIALOG
 *@ATK_ROLE_PAGE_TAB: An object that is a child of a page tab list
 *@ATK_ROLE_PAGE_TAB_LIST: An object that presents a series of panels (or page tabs), one at a time, through some mechanism provided by the object 
 *@ATK_ROLE_PANEL: A generic container that is often used to group objects
 *@ATK_ROLE_PASSWORD_TEXT: A text object uses for passwords, or other places where the text content is not shown visibly to the user
 *@ATK_ROLE_POPUP_MENU: A temporary window that is usually used to offer the user a list of choices, and then hides when the user selects one of those choices
 *@ATK_ROLE_PROGRESS_BAR: An object used to indicate how much of a task has been completed
 *@ATK_ROLE_PUSH_BUTTON: An object the user can manipulate to tell the application to do something
 *@ATK_ROLE_RADIO_BUTTON: A specialized check box that will cause other radio buttons in the same group to become unchecked when this one is checked
 *@ATK_ROLE_RADIO_MENU_ITEM: A check menu item which belongs to a group. At each instant exactly one of the radio menu items from a group is selected
 *@ATK_ROLE_ROOT_PANE: A specialized pane that has a glass pane and a layered pane as its children
 *@ATK_ROLE_ROW_HEADER: The header for a row of data
 *@ATK_ROLE_SCROLL_BAR: An object usually used to allow a user to incrementally view a large amount of data.
 *@ATK_ROLE_SCROLL_PANE: An object that allows a user to incrementally view a large amount of information
 *@ATK_ROLE_SEPARATOR: An object usually contained in a menu to provide a visible and logical separation of the contents in a menu
 *@ATK_ROLE_SLIDER: An object that allows the user to select from a bounded range
 *@ATK_ROLE_SPLIT_PANE: A specialized panel that presents two other panels at the same time
 *@ATK_ROLE_SPIN_BUTTON: An object used to get an integer or floating point number from the user
 *@ATK_ROLE_STATUSBAR: An object which reports messages of minor importance to the user
 *@ATK_ROLE_TABLE: An object used to represent information in terms of rows and columns
 *@ATK_ROLE_TABLE_CELL: A cell in a table
 *@ATK_ROLE_TABLE_COLUMN_HEADER: The header for a column of a table
 *@ATK_ROLE_TABLE_ROW_HEADER: The header for a row of a table
 *@ATK_ROLE_TEAR_OFF_MENU_ITEM: A menu item used to tear off and reattach its menu
 *@ATK_ROLE_TERMINAL: An object that represents an accessible terminal.  (Since: 0.6)
 *@ATK_ROLE_TEXT: An interactive widget that supports multiple lines of text and
 * optionally accepts user input, but whose purpose is not to solicit user input.
 * Thus ATK_ROLE_TEXT is appropriate for the text view in a plain text editor
 * but inappropriate for an input field in a dialog box or web form. For widgets
 * whose purpose is to solicit input from the user, see ATK_ROLE_ENTRY and
 * ATK_ROLE_PASSWORD_TEXT. For generic objects which display a brief amount of
 * textual information, see ATK_ROLE_STATIC.
 *@ATK_ROLE_TOGGLE_BUTTON: A specialized push button that can be checked or unchecked, but does not provide a separate indicator for the current state
 *@ATK_ROLE_TOOL_BAR: A bar or palette usually composed of push buttons or toggle buttons
 *@ATK_ROLE_TOOL_TIP: An object that provides information about another object
 *@ATK_ROLE_TREE: An object used to represent hierarchical information to the user
 *@ATK_ROLE_TREE_TABLE: An object capable of expanding and collapsing rows as well as showing multiple columns of data.   (Since: 0.7)
 *@ATK_ROLE_UNKNOWN: The object contains some Accessible information, but its role is not known
 *@ATK_ROLE_VIEWPORT: An object usually used in a scroll pane
 *@ATK_ROLE_WINDOW: A top level window with no title or border.
 *@ATK_ROLE_HEADER: An object that serves as a document header. (Since: 1.1.1)
 *@ATK_ROLE_FOOTER: An object that serves as a document footer.  (Since: 1.1.1)
 *@ATK_ROLE_PARAGRAPH: An object which is contains a paragraph of text content.   (Since: 1.1.1)
 *@ATK_ROLE_RULER: An object which describes margins and tab stops, etc. for text objects which it controls (should have CONTROLLER_FOR relation to such).   (Since: 1.1.1)
 *@ATK_ROLE_APPLICATION: The object is an application object, which may contain @ATK_ROLE_FRAME objects or other types of accessibles.  The root accessible of any application's ATK hierarchy should have ATK_ROLE_APPLICATION.   (Since: 1.1.4)
 *@ATK_ROLE_AUTOCOMPLETE: The object is a dialog or list containing items for insertion into an entry widget, for instance a list of words for completion of a text entry.   (Since: 1.3)
 *@ATK_ROLE_EDITBAR: The object is an editable text object in a toolbar.  (Since: 1.5)
 *@ATK_ROLE_EMBEDDED: The object is an embedded container within a document or panel.  This role is a grouping "hint" indicating that the contained objects share a context.  (Since: 1.7.2)
 *@ATK_ROLE_ENTRY: The object is a component whose textual content may be entered or modified by the user, provided @ATK_STATE_EDITABLE is present.   (Since: 1.11)
 *@ATK_ROLE_CHART: The object is a graphical depiction of quantitative data. It may contain multiple subelements whose attributes and/or description may be queried to obtain both the quantitative data and information about how the data is being presented. The LABELLED_BY relation is particularly important in interpreting objects of this type, as is the accessible-description property.  (Since: 1.11)
 *@ATK_ROLE_CAPTION: The object contains descriptive information, usually textual, about another user interface element such as a table, chart, or image.  (Since: 1.11)
 *@ATK_ROLE_DOCUMENT_FRAME: The object is a visual frame or container which contains a view of document content. Document frames may occur within another Document instance, in which case the second document may be said to be embedded in the containing instance. HTML frames are often ROLE_DOCUMENT_FRAME. Either this object, or a singleton descendant, should implement the Document interface.  (Since: 1.11)
 *@ATK_ROLE_HEADING: The object serves as a heading for content which follows it in a document. The 'heading level' of the heading, if availabe, may be obtained by querying the object's attributes.
 *@ATK_ROLE_PAGE: The object is a containing instance which encapsulates a page of information. @ATK_ROLE_PAGE is used in documents and content which support a paginated navigation model.  (Since: 1.11)
 *@ATK_ROLE_SECTION: The object is a containing instance of document content which constitutes a particular 'logical' section of the document. The type of content within a section, and the nature of the section division itself, may be obtained by querying the object's attributes. Sections may be nested. (Since: 1.11)
 *@ATK_ROLE_REDUNDANT_OBJECT: The object is redundant with another object in the hierarchy, and is exposed for purely technical reasons.  Objects of this role should normally be ignored by clients. (Since: 1.11)
 *@ATK_ROLE_FORM: The object is a container for form controls, for instance as part of a 
 * web form or user-input form within a document.  This role is primarily a tag/convenience for 
 * clients when navigating complex documents, it is not expected that ordinary GUI containers will 
 * always have ATK_ROLE_FORM. (Since: 1.12.0)
 *@ATK_ROLE_LINK: The object is a hypertext anchor, i.e. a "link" in a
 * hypertext document.  Such objects are distinct from 'inline'
 * content which may also use the Hypertext/Hyperlink interfaces
 * to indicate the range/location within a text object where
 * an inline or embedded object lies.  (Since: 1.12.1)
 *@ATK_ROLE_INPUT_METHOD_WINDOW: The object is a window or similar viewport 
 * which is used to allow composition or input of a 'complex character',
 * in other words it is an "input method window." (Since: 1.12.1)
 *@ATK_ROLE_TABLE_ROW: A row in a table.  (Since: 2.1.0)
 *@ATK_ROLE_TREE_ITEM: An object that represents an element of a tree.  (Since: 2.1.0)
 *@ATK_ROLE_DOCUMENT_SPREADSHEET: A document frame which contains a spreadsheet.  (Since: 2.1.0)
 *@ATK_ROLE_DOCUMENT_PRESENTATION: A document frame which contains a presentation or slide content.  (Since: 2.1.0)
 *@ATK_ROLE_DOCUMENT_TEXT: A document frame which contains textual content, such as found in a word processing application.  (Since: 2.1.0)
 *@ATK_ROLE_DOCUMENT_WEB: A document frame which contains HTML or other markup suitable for display in a web browser.  (Since: 2.1.0)
 *@ATK_ROLE_DOCUMENT_EMAIL: A document frame which contains email content to be displayed or composed either in plain text or HTML.  (Since: 2.1.0)
 *@ATK_ROLE_COMMENT: An object found within a document and designed to present a comment, note, or other annotation. In some cases, this object might not be visible until activated.  (Since: 2.1.0)
 *@ATK_ROLE_LIST_BOX: A non-collapsible list of choices the user can select from. (Since: 2.1.0)
 *@ATK_ROLE_GROUPING: A group of related widgets. This group typically has a label. (Since: 2.1.0)
 *@ATK_ROLE_IMAGE_MAP: An image map object. Usually a graphic with multiple hotspots, where each hotspot can be activated resulting in the loading of another document or section of a document. (Since: 2.1.0)
 *@ATK_ROLE_NOTIFICATION: A transitory object designed to present a message to the user, typically at the desktop level rather than inside a particular application.  (Since: 2.1.0)
 *@ATK_ROLE_INFO_BAR: An object designed to present a message to the user within an existing window. (Since: 2.1.0)
 *@ATK_ROLE_LEVEL_BAR: A bar that serves as a level indicator to, for instance, show the strength of a password or the state of a battery.  (Since: 2.7.3)
 *@ATK_ROLE_TITLE_BAR: A bar that serves as the title of a window or a
 * dialog. (Since: 2.12)
 *@ATK_ROLE_BLOCK_QUOTE: An object which contains a text section
 * that is quoted from another source. (Since: 2.12)
 *@ATK_ROLE_AUDIO: An object which represents an audio element. (Since: 2.12)
 *@ATK_ROLE_VIDEO: An object which represents a video element. (Since: 2.12)
 *@ATK_ROLE_DEFINITION: A definition of a term or concept. (Since: 2.12)
 *@ATK_ROLE_ARTICLE: A section of a page that consists of a
 * composition that forms an independent part of a document, page, or
 * site. Examples: A blog entry, a news story, a forum post. (Since: 2.12)
 *@ATK_ROLE_LANDMARK: A region of a web page intended as a
 * navigational landmark. This is designed to allow Assistive
 * Technologies to provide quick navigation among key regions within a
 * document. (Since: 2.12)
 *@ATK_ROLE_LOG: A text widget or container holding log content, such
 * as chat history and error logs. In this role there is a
 * relationship between the arrival of new items in the log and the
 * reading order. The log contains a meaningful sequence and new
 * information is added only to the end of the log, not at arbitrary
 * points. (Since: 2.12)
 *@ATK_ROLE_MARQUEE: A container where non-essential information
 * changes frequently. Common usages of marquee include stock tickers
 * and ad banners. The primary difference between a marquee and a log
 * is that logs usually have a meaningful order or sequence of
 * important content changes. (Since: 2.12)
 *@ATK_ROLE_MATH: A text widget or container that holds a mathematical
 * expression. (Since: 2.12)
 *@ATK_ROLE_RATING: A widget whose purpose is to display a rating,
 * such as the number of stars associated with a song in a media
 * player. Objects of this role should also implement
 * AtkValue. (Since: 2.12)
 *@ATK_ROLE_TIMER: An object containing a numerical counter which
 * indicates an amount of elapsed time from a start point, or the time
 * remaining until an end point. (Since: 2.12)
 *@ATK_ROLE_DESCRIPTION_LIST: An object that represents a list of
 * term-value groups. A term-value group represents a individual
 * description and consist of one or more names
 * (ATK_ROLE_DESCRIPTION_TERM) followed by one or more values
 * (ATK_ROLE_DESCRIPTION_VALUE). For each list, there should not be
 * more than one group with the same term name. (Since: 2.12)
 *@ATK_ROLE_DESCRIPTION_TERM: An object that represents a term or phrase
 * with a corresponding definition. (Since: 2.12)
 *@ATK_ROLE_DESCRIPTION_VALUE: An object that represents the
 * description, definition or value of a term. (Since: 2.12)
 *@ATK_ROLE_STATIC: A generic non-container object whose purpose is to display a
 * brief amount of information to the user and whose role is known by the
 * implementor but lacks semantic value for the user. Examples in which
 * %ATK_ROLE_STATIC is appropriate include the message displayed in a message box
 * and an image used as an alternative means to display text. %ATK_ROLE_STATIC
 * should not be applied to widgets which are traditionally interactive, objects
 * which display a significant amount of content, or any object which has an
 * accessible relation pointing to another object. Implementors should expose the
 * displayed information through the accessible name of the object. If doing so seems
 * inappropriate, it may indicate that a different role should be used. For
 * labels which describe another widget, see %ATK_ROLE_LABEL. For text views, see
 * %ATK_ROLE_TEXT. For generic containers, see %ATK_ROLE_PANEL. For objects whose
 * role is not known by the implementor, see %ATK_ROLE_UNKNOWN. (Since: 2.16)
 *@ATK_ROLE_MATH_FRACTION: An object that represents a mathematical fraction.
 * (Since: 2.16)
 *@ATK_ROLE_MATH_ROOT: An object that represents a mathematical expression
 * displayed with a radical. (Since: 2.16)
 *@ATK_ROLE_SUBSCRIPT: An object that contains text that is displayed as a
 * subscript. (Since: 2.16)
 *@ATK_ROLE_SUPERSCRIPT: An object that contains text that is displayed as a
 * superscript. (Since: 2.16)
 *@ATK_ROLE_FOOTNOTE: An object that contains the text of a footnote. (Since: 2.26)
 *@ATK_ROLE_CONTENT_DELETION: Content previously deleted or proposed to be
 * deleted, e.g. in revision history or a content view providing suggestions
 * from reviewers. (Since: 2.34)
 *@ATK_ROLE_CONTENT_INSERTION: Content previously inserted or proposed to be
 * inserted, e.g. in revision history or a content view providing suggestions
 * from reviewers. (Since: 2.34)
 *@ATK_ROLE_MARK: A run of content that is marked or highlighted, such as for
 * reference purposes, or to call it out as having a special purpose. If the
 * marked content has an associated section in the document elaborating on the
 * reason for the mark, then %ATK_RELATION_DETAILS should be used on the mark
 * to point to that associated section. In addition, the reciprocal relation
 * %ATK_RELATION_DETAILS_FOR should be used on the associated content section
 * to point back to the mark. (Since: 2.36)
 *@ATK_ROLE_SUGGESTION: A container for content that is called out as a proposed
 * change from the current version of the document, such as by a reviewer of the
 * content. This role should include either %ATK_ROLE_CONTENT_DELETION and/or
 * %ATK_ROLE_CONTENT_INSERTION children, in any order, to indicate what the
 * actual change is. (Since: 2.36)
 *@ATK_ROLE_LAST_DEFINED: not a valid role, used for finding end of the enumeration
 *
 * Describes the role of an object
 *
 * These are the built-in enumerated roles that UI components can have
 * in ATK.  Other roles may be added at runtime, so an AtkRole >=
 * %ATK_ROLE_LAST_DEFINED is not necessarily an error.
 */
typedef enum
{
  ATK_ROLE_INVALID = 0,
  ATK_ROLE_ACCEL_LABEL,      /*<nick=accelerator-label>*/
  ATK_ROLE_ALERT,
  ATK_ROLE_ANIMATION,
  ATK_ROLE_ARROW,
  ATK_ROLE_CALENDAR,
  ATK_ROLE_CANVAS,
  ATK_ROLE_CHECK_BOX,
  ATK_ROLE_CHECK_MENU_ITEM,
  ATK_ROLE_COLOR_CHOOSER,
  ATK_ROLE_COLUMN_HEADER,
  ATK_ROLE_COMBO_BOX,
  ATK_ROLE_DATE_EDITOR,
  ATK_ROLE_DESKTOP_ICON,
  ATK_ROLE_DESKTOP_FRAME,
  ATK_ROLE_DIAL,
  ATK_ROLE_DIALOG,
  ATK_ROLE_DIRECTORY_PANE,
  ATK_ROLE_DRAWING_AREA,
  ATK_ROLE_FILE_CHOOSER,
  ATK_ROLE_FILLER,
  ATK_ROLE_FONT_CHOOSER,
  ATK_ROLE_FRAME,
  ATK_ROLE_GLASS_PANE,
  ATK_ROLE_HTML_CONTAINER,
  ATK_ROLE_ICON,
  ATK_ROLE_IMAGE,
  ATK_ROLE_INTERNAL_FRAME,
  ATK_ROLE_LABEL,
  ATK_ROLE_LAYERED_PANE,
  ATK_ROLE_LIST,
  ATK_ROLE_LIST_ITEM,
  ATK_ROLE_MENU,
  ATK_ROLE_MENU_BAR,
  ATK_ROLE_MENU_ITEM,
  ATK_ROLE_OPTION_PANE,
  ATK_ROLE_PAGE_TAB,
  ATK_ROLE_PAGE_TAB_LIST,
  ATK_ROLE_PANEL,
  ATK_ROLE_PASSWORD_TEXT,
  ATK_ROLE_POPUP_MENU,
  ATK_ROLE_PROGRESS_BAR,
  ATK_ROLE_PUSH_BUTTON,
  ATK_ROLE_RADIO_BUTTON,
  ATK_ROLE_RADIO_MENU_ITEM,
  ATK_ROLE_ROOT_PANE,
  ATK_ROLE_ROW_HEADER,
  ATK_ROLE_SCROLL_BAR,
  ATK_ROLE_SCROLL_PANE,
  ATK_ROLE_SEPARATOR,
  ATK_ROLE_SLIDER,
  ATK_ROLE_SPLIT_PANE,
  ATK_ROLE_SPIN_BUTTON,
  ATK_ROLE_STATUSBAR,
  ATK_ROLE_TABLE,
  ATK_ROLE_TABLE_CELL,
  ATK_ROLE_TABLE_COLUMN_HEADER,
  ATK_ROLE_TABLE_ROW_HEADER,
  ATK_ROLE_TEAR_OFF_MENU_ITEM,
  ATK_ROLE_TERMINAL,
  ATK_ROLE_TEXT,
  ATK_ROLE_TOGGLE_BUTTON,
  ATK_ROLE_TOOL_BAR,
  ATK_ROLE_TOOL_TIP,
  ATK_ROLE_TREE,
  ATK_ROLE_TREE_TABLE,
  ATK_ROLE_UNKNOWN,
  ATK_ROLE_VIEWPORT,
  ATK_ROLE_WINDOW,
  ATK_ROLE_HEADER,
  ATK_ROLE_FOOTER,
  ATK_ROLE_PARAGRAPH,
  ATK_ROLE_RULER,
  ATK_ROLE_APPLICATION,
  ATK_ROLE_AUTOCOMPLETE,
  ATK_ROLE_EDITBAR,          /*<nick=edit-bar>*/
  ATK_ROLE_EMBEDDED,
  ATK_ROLE_ENTRY,
  ATK_ROLE_CHART,
  ATK_ROLE_CAPTION,
  ATK_ROLE_DOCUMENT_FRAME,
  ATK_ROLE_HEADING,
  ATK_ROLE_PAGE,
  ATK_ROLE_SECTION,
  ATK_ROLE_REDUNDANT_OBJECT,
  ATK_ROLE_FORM,
  ATK_ROLE_LINK,
  ATK_ROLE_INPUT_METHOD_WINDOW,
  ATK_ROLE_TABLE_ROW,
  ATK_ROLE_TREE_ITEM,
  ATK_ROLE_DOCUMENT_SPREADSHEET,
  ATK_ROLE_DOCUMENT_PRESENTATION,
  ATK_ROLE_DOCUMENT_TEXT,
  ATK_ROLE_DOCUMENT_WEB,
  ATK_ROLE_DOCUMENT_EMAIL,
  ATK_ROLE_COMMENT,
  ATK_ROLE_LIST_BOX,
  ATK_ROLE_GROUPING,
  ATK_ROLE_IMAGE_MAP,
  ATK_ROLE_NOTIFICATION,
  ATK_ROLE_INFO_BAR,
  ATK_ROLE_LEVEL_BAR,
  ATK_ROLE_TITLE_BAR,
  ATK_ROLE_BLOCK_QUOTE,
  ATK_ROLE_AUDIO,
  ATK_ROLE_VIDEO,
  ATK_ROLE_DEFINITION,
  ATK_ROLE_ARTICLE,
  ATK_ROLE_LANDMARK,
  ATK_ROLE_LOG,
  ATK_ROLE_MARQUEE,
  ATK_ROLE_MATH,
  ATK_ROLE_RATING,
  ATK_ROLE_TIMER,
  ATK_ROLE_DESCRIPTION_LIST,
  ATK_ROLE_DESCRIPTION_TERM,
  ATK_ROLE_DESCRIPTION_VALUE,
  ATK_ROLE_STATIC,
  ATK_ROLE_MATH_FRACTION,
  ATK_ROLE_MATH_ROOT,
  ATK_ROLE_SUBSCRIPT,
  ATK_ROLE_SUPERSCRIPT,
  ATK_ROLE_FOOTNOTE,
  ATK_ROLE_CONTENT_DELETION,
  ATK_ROLE_CONTENT_INSERTION,
  ATK_ROLE_MARK,
  ATK_ROLE_SUGGESTION,
  ATK_ROLE_LAST_DEFINED
} AtkRole;

/**
 *AtkLayer:
 *@ATK_LAYER_INVALID: The object does not have a layer
 *@ATK_LAYER_BACKGROUND: This layer is reserved for the desktop background
 *@ATK_LAYER_CANVAS: This layer is used for Canvas components
 *@ATK_LAYER_WIDGET: This layer is normally used for components
 *@ATK_LAYER_MDI: This layer is used for layered components
 *@ATK_LAYER_POPUP: This layer is used for popup components, such as menus
 *@ATK_LAYER_OVERLAY: This layer is reserved for future use.
 *@ATK_LAYER_WINDOW: This layer is used for toplevel windows.
 *
 * Describes the layer of a component
 *
 * These enumerated "layer values" are used when determining which UI
 * rendering layer a component is drawn into, which can help in making
 * determinations of when components occlude one another.
 **/
typedef enum
{
  ATK_LAYER_INVALID,
  ATK_LAYER_BACKGROUND,
  ATK_LAYER_CANVAS,
  ATK_LAYER_WIDGET,
  ATK_LAYER_MDI,
  ATK_LAYER_POPUP,
  ATK_LAYER_OVERLAY,
  ATK_LAYER_WINDOW
} AtkLayer;

/**
 * AtkValueType:
 *
 * Default types for a given value. Those are defined in order to
 * easily get localized strings to describe a given value or a given
 * subrange, using atk_value_type_get_localized_name().
 *
 */
typedef enum
{
  ATK_VALUE_VERY_WEAK,
  ATK_VALUE_WEAK,
  ATK_VALUE_ACCEPTABLE,
  ATK_VALUE_STRONG,
  ATK_VALUE_VERY_STRONG,
  ATK_VALUE_VERY_LOW,
  ATK_VALUE_LOW,
  ATK_VALUE_MEDIUM,
  ATK_VALUE_HIGH,
  ATK_VALUE_VERY_HIGH,
  ATK_VALUE_VERY_BAD,
  ATK_VALUE_BAD,
  ATK_VALUE_GOOD,
  ATK_VALUE_VERY_GOOD,
  ATK_VALUE_BEST,
  ATK_VALUE_LAST_DEFINED
}AtkValueType;

G_END_DECLS
