/* GDK - The GIMP Drawing Kit
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
#ifndef __GDK_TYPES_H__
#define __GDK_TYPES_H__


/* GDK uses "glib". (And so does GTK).
 */
#include <glib.h>


#define GDK_NONE	     0L
#define GDK_CURRENT_TIME     0L
#define GDK_PARENT_RELATIVE  1L

/* special deviceid for core pointer events */
#define GDK_CORE_POINTER 0xfedc


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


/* Type definitions for the basic structures.
 */

typedef gulong			      GdkAtom;
typedef struct _GdkColor	      GdkColor;
typedef struct _GdkColormap	      GdkColormap;
typedef struct _GdkVisual	      GdkVisual;
typedef struct _GdkWindowAttr	      GdkWindowAttr;
typedef struct _GdkWindow	      GdkWindow;
typedef struct _GdkWindow	      GdkPixmap;
typedef struct _GdkWindow	      GdkBitmap;
typedef struct _GdkWindow	      GdkDrawable;
typedef struct _GdkImage	      GdkImage;
typedef struct _GdkGCValues	      GdkGCValues;
typedef struct _GdkGC		      GdkGC;
typedef struct _GdkPoint	      GdkPoint;
typedef struct _GdkRectangle	      GdkRectangle;
typedef struct _GdkSegment	      GdkSegment;
typedef struct _GdkFont		      GdkFont;
typedef struct _GdkCursor	      GdkCursor;
typedef struct _GdkColorContextDither GdkColorContextDither;
typedef struct _GdkColorContext	      GdkColorContext;

typedef struct _GdkEventAny	    GdkEventAny;
typedef struct _GdkEventExpose	    GdkEventExpose;
typedef struct _GdkEventNoExpose    GdkEventNoExpose;
typedef struct _GdkEventVisibility  GdkEventVisibility;
typedef struct _GdkEventMotion	    GdkEventMotion;
typedef struct _GdkEventButton	    GdkEventButton;
typedef struct _GdkEventKey	    GdkEventKey;
typedef struct _GdkEventFocus	    GdkEventFocus;
typedef struct _GdkEventCrossing    GdkEventCrossing;
typedef struct _GdkEventConfigure   GdkEventConfigure;
typedef struct _GdkEventProperty    GdkEventProperty;
typedef struct _GdkEventSelection   GdkEventSelection;
typedef struct _GdkEventProximity   GdkEventProximity;
typedef struct _GdkEventOther	    GdkEventOther;
typedef struct _GdkEventDragBegin   GdkEventDragBegin;
typedef struct _GdkEventDragRequest GdkEventDragRequest;
typedef struct _GdkEventDropEnter   GdkEventDropEnter;
typedef struct _GdkEventDropDataAvailable GdkEventDropDataAvailable;
typedef struct _GdkEventDropLeave   GdkEventDropLeave;
typedef struct _GdkEventClient	    GdkEventClient;
typedef union  _GdkEvent	    GdkEvent;
typedef struct _GdkDeviceKey	    GdkDeviceKey;
typedef struct _GdkDeviceInfo	    GdkDeviceInfo;
typedef struct _GdkTimeCoord	    GdkTimeCoord;
typedef struct _GdkRegion	    GdkRegion;
typedef gint (*GdkEventFunc) (GdkEvent *event,
			      gpointer	data);

typedef void*			  GdkIC;
typedef void*			  GdkIM;


/* Types of windows.
 *   Root: There is only 1 root window and it is initialized
 *	   at startup. Creating a window of type GDK_WINDOW_ROOT
 *	   is an error.
 *   Toplevel: Windows which interact with the window manager.
 *   Child: Windows which are children of some other type of window.
 *	    (Any other type of window). Most windows are child windows.
 *   Dialog: A special kind of toplevel window which interacts with
 *	     the window manager slightly differently than a regular
 *	     toplevel window. Dialog windows should be used for any
 *	     transient window.
 *   Pixmap: Pixmaps are really just another kind of window which
 *	     doesn't actually appear on the screen. It can't have
 *	     children, either and is really just a convenience so
 *	     that the drawing functions can work on both windows
 *	     and pixmaps transparently. (ie. You shouldn't pass a
 *	     pixmap to any procedure which accepts a window with the
 *	     exception of the drawing functions).
 *   Foreign: A window that actually belongs to another application
 */
typedef enum
{
  G_SV (GDK_WINDOW_ROOT,	root),
  G_SV (GDK_WINDOW_TOPLEVEL,	toplevel),
  G_SV (GDK_WINDOW_CHILD,	child),
  G_SV (GDK_WINDOW_DIALOG,	dialog),
  G_SV (GDK_WINDOW_TEMP,	temp),
  G_SV (GDK_WINDOW_PIXMAP,	pixmap),
  G_SV (GDK_WINDOW_FOREIGN,	foreign)
} G_ENUM (GdkWindowType);

/* Classes of windows.
 *   InputOutput: Almost every window should be of this type. Such windows
 *		  receive events and are also displayed on screen.
 *   InputOnly: Used only in special circumstances when events need to be
 *		stolen from another window or windows. Input only windows
 *		have no visible output, so they are handy for placing over
 *		top of a group of windows in order to grab the events (or
 *		filter the events) from those windows.
 */
typedef enum
{
  G_SV (GDK_INPUT_OUTPUT,	input-output),
  G_SV (GDK_INPUT_ONLY,		input-only)
} G_ENUM (GdkWindowClass);

/* Types of images.
 *   Normal: Normal X image type. These are slow as they involve passing
 *	     the entire image through the X connection each time a draw
 *	     request is required.
 *   Shared: Shared memory X image type. These are fast as the X server
 *	     and the program actually use the same piece of memory. They
 *	     should be used with care though as there is the possibility
 *	     for both the X server and the program to be reading/writing
 *	     the image simultaneously and producing undesired results.
 */
typedef enum
{
  G_SV (GDK_IMAGE_NORMAL,	normal),
  G_SV (GDK_IMAGE_SHARED,	shared),
  G_SV (GDK_IMAGE_FASTEST,	fastest)
} G_ENUM (GdkImageType);

/* Types of visuals.
 *   StaticGray:
 *   Grayscale:
 *   StaticColor:
 *   PseudoColor:
 *   TrueColor:
 *   DirectColor:
 */
typedef enum
{
  G_SV (GDK_VISUAL_STATIC_GRAY,		static-gray),
  G_SV (GDK_VISUAL_GRAYSCALE,		grayscale),
  G_SV (GDK_VISUAL_STATIC_COLOR,	static-color),
  G_SV (GDK_VISUAL_PSEUDO_COLOR,	pseudo-color),
  G_SV (GDK_VISUAL_TRUE_COLOR,		true-color),
  G_SV (GDK_VISUAL_DIRECT_COLOR,	direct-color)
} G_ENUM (GdkVisualType);

/* Types of font.
 *   GDK_FONT_FONT: the font is an XFontStruct.
 *   GDK_FONT_FONTSET: the font is an XFontSet used for I18N.
 */
typedef enum
{
  G_SV (GDK_FONT_FONT,		font),
  G_SV (GDK_FONT_FONTSET,	fontset)
} G_ENUM (GdkFontType);

/* Window attribute mask values.
 *   GDK_WA_TITLE: The "title" field is valid.
 *   GDK_WA_X: The "x" field is valid.
 *   GDK_WA_Y: The "y" field is valid.
 *   GDK_WA_CURSOR: The "cursor" field is valid.
 *   GDK_WA_COLORMAP: The "colormap" field is valid.
 *   GDK_WA_VISUAL: The "visual" field is valid.
 */
typedef enum
{
  G_NV (GDK_WA_TITLE,		title,		1 << 1),
  G_NV (GDK_WA_X,		x,		1 << 2),
  G_NV (GDK_WA_Y,		y,		1 << 3),
  G_NV (GDK_WA_CURSOR,		cursor,		1 << 4),
  G_NV (GDK_WA_COLORMAP,	colormap,	1 << 5),
  G_NV (GDK_WA_VISUAL,		visual,		1 << 6),
  G_NV (GDK_WA_WMCLASS,		wmclass,	1 << 7),
  G_NV (GDK_WA_NOREDIR,		noredir,	1 << 8)
} G_FLAGS (GdkWindowAttributesType);

/* Size restriction enumeration.
 */
typedef enum
{
  G_NV (GDK_HINT_POS,		pos,		1 << 0),
  G_NV (GDK_HINT_MIN_SIZE,	min-size,	1 << 1),
  G_NV (GDK_HINT_MAX_SIZE,	max-size,	1 << 2)
} G_FLAGS (GdkWindowHints);

/* GC function types.
 *   Copy: Overwrites destination pixels with the source pixels.
 *   Invert: Inverts the destination pixels.
 *   Xor: Xor's the destination pixels with the source pixels.
 */
typedef enum
{
  G_SV (GDK_COPY,	copy),
  G_SV (GDK_INVERT,	invert),
  G_SV (GDK_XOR,	xor)
} G_ENUM (GdkFunction);

/* GC fill types.
 *  Solid:
 *  Tiled:
 *  Stippled:
 *  OpaqueStippled:
 */
typedef enum
{
  G_SV (GDK_SOLID,		solid),
  G_SV (GDK_TILED,		tiled),
  G_SV (GDK_STIPPLED,		stippled),
  G_SV (GDK_OPAQUE_STIPPLED,	opaque-stippled)
} G_ENUM (GdkFill);

/* GC fill rule for polygons
 *  EvenOddRule
 *  WindingRule
 */
typedef enum
{
  G_SV (GDK_EVEN_ODD_RULE,	even-odd-rule),
  G_SV (GDK_WINDING_RULE,	winding-rule)
} G_ENUM (GdkFillRule);

/* GC line styles
 *  Solid:
 *  OnOffDash:
 *  DoubleDash:
 */
typedef enum
{
  G_SV (GDK_LINE_SOLID,		solid),
  G_SV (GDK_LINE_ON_OFF_DASH,	on-off-dash),
  G_SV (GDK_LINE_DOUBLE_DASH,	double-dash)
} G_ENUM (GdkLineStyle);

/* GC cap styles
 *  CapNotLast:
 *  CapButt:
 *  CapRound:
 *  CapProjecting:
 */
typedef enum
{
  G_SV (GDK_CAP_NOT_LAST,	not-last),
  G_SV (GDK_CAP_BUTT,		butt),
  G_SV (GDK_CAP_ROUND,		round),
  G_SV (GDK_CAP_PROJECTING,	projecting)
} G_ENUM (GdkCapStyle);

/* GC join styles
 *  JoinMiter:
 *  JoinRound:
 *  JoinBevel:
 */
typedef enum
{
  G_SV (GDK_JOIN_MITER,	miter),
  G_SV (GDK_JOIN_ROUND,	round),
  G_SV (GDK_JOIN_BEVEL,	bevel)
} G_ENUM (GdkJoinStyle);

/* Cursor types.
 */
typedef enum
{
#include <gdk/gdkcursors.h>
  GDK_LAST_CURSOR,
  GDK_CURSOR_IS_PIXMAP = -1
} GdkCursorType;

typedef enum {
  G_SV (GDK_FILTER_CONTINUE,	continue),  /* Event not handled,
					     * continue processesing */
  G_SV (GDK_FILTER_TRANSLATE,	translate), /* Translated event stored */
  G_SV (GDK_FILTER_REMOVE,	remove)	    /* Terminate processing,
					     * removing event */
} G_ENUM (GdkFilterReturn);

typedef enum {
  G_SV (GDK_VISIBILITY_UNOBSCURED,	unobscured),
  G_SV (GDK_VISIBILITY_PARTIAL,		partial),
  G_SV (GDK_VISIBILITY_FULLY_OBSCURED,	fully-obscured)
} G_ENUM (GdkVisibilityState);

/* Event types.
 *   Nothing: No event occurred.
 *   Delete: A window delete event was sent by the window manager.
 *	     The specified window should be deleted.
 *   Destroy: A window has been destroyed.
 *   Expose: Part of a window has been uncovered.
 *   NoExpose: Same as expose, but no expose event was generated.
 *   VisibilityNotify: A window has become fully/partially/not obscured.
 *   MotionNotify: The mouse has moved.
 *   ButtonPress: A mouse button was pressed.
 *   ButtonRelease: A mouse button was release.
 *   KeyPress: A key was pressed.
 *   KeyRelease: A key was released.
 *   EnterNotify: A window was entered.
 *   LeaveNotify: A window was exited.
 *   FocusChange: The focus window has changed. (The focus window gets
 *		  keyboard events).
 *   Resize: A window has been resized.
 *   Map: A window has been mapped. (It is now visible on the screen).
 *   Unmap: A window has been unmapped. (It is no longer visible on
 *	    the screen).
 */
typedef enum
{
  G_NV (GDK_NOTHING,		NOTHING,	-1),
  G_NV (GDK_DELETE,		delete,		 0),
  G_NV (GDK_DESTROY,		destroy,	 1),
  G_NV (GDK_EXPOSE,		expose,		 2),
  G_NV (GDK_MOTION_NOTIFY,	motion-notify,	 3),
  G_NV (GDK_BUTTON_PRESS,	button-press,	 4),
  G_NV (GDK_2BUTTON_PRESS,	2button-press,	 5),
  G_NV (GDK_3BUTTON_PRESS,	3button-press,	 6),
  G_NV (GDK_BUTTON_RELEASE,	button-release,	 7),
  G_NV (GDK_KEY_PRESS,		key-press,	 8),
  G_NV (GDK_KEY_RELEASE,	key-release,	 9),
  G_NV (GDK_ENTER_NOTIFY,	enter-notify,	 10),
  G_NV (GDK_LEAVE_NOTIFY,	leave-notify,	 11),
  G_NV (GDK_FOCUS_CHANGE,	focus-change,	 12),
  G_NV (GDK_CONFIGURE,		configure,	 13),
  G_NV (GDK_MAP,		map,		 14),
  G_NV (GDK_UNMAP,		unmap,		 15),
  G_NV (GDK_PROPERTY_NOTIFY,	property-notify, 16),
  G_NV (GDK_SELECTION_CLEAR,	selection-clear, 17),
  G_NV (GDK_SELECTION_REQUEST,	selection-request,18),
  G_NV (GDK_SELECTION_NOTIFY,	selection-notify,19),
  G_NV (GDK_PROXIMITY_IN,	proximity-in,	 20),
  G_NV (GDK_PROXIMITY_OUT,	proximity-out,	 21),
  G_NV (GDK_DRAG_BEGIN,		drag-begin,	 22),
  G_NV (GDK_DRAG_REQUEST,	drag-request,	 23),
  G_NV (GDK_DROP_ENTER,		drop-enter,	 24),
  G_NV (GDK_DROP_LEAVE,		drop-leave,	 25),
  G_NV (GDK_DROP_DATA_AVAIL,	drop-data-avail, 26),
  G_NV (GDK_CLIENT_EVENT,	client-event,	 27),
  G_NV (GDK_VISIBILITY_NOTIFY,	visibility-notify, 28),
  G_NV (GDK_NO_EXPOSE,		no-expose,	 29),
  G_NV (GDK_OTHER_EVENT,	other-event,	 9999)	/* Deprecated, use
							 * filters instead */
} G_ENUM (GdkEventType);

/* Event masks. (Used to select what types of events a window
 *  will receive).
 */
typedef enum
{
  G_NV (GDK_EXPOSURE_MASK,		exposure-mask,		1 << 1),
  G_NV (GDK_POINTER_MOTION_MASK,	pointer-motion-mask,	1 << 2),
  G_NV (GDK_POINTER_MOTION_HINT_MASK,	pointer-motion-hint-mask, 1 << 3),
  G_NV (GDK_BUTTON_MOTION_MASK,		button-motion-mask,	1 << 4),
  G_NV (GDK_BUTTON1_MOTION_MASK,	button1-motion-mask,	1 << 5),
  G_NV (GDK_BUTTON2_MOTION_MASK,	button2-motion-mask,	1 << 6),
  G_NV (GDK_BUTTON3_MOTION_MASK,	button3-motion-mask,	1 << 7),
  G_NV (GDK_BUTTON_PRESS_MASK,		button-press-mask,	1 << 8),
  G_NV (GDK_BUTTON_RELEASE_MASK,	button-release-mask,	1 << 9),
  G_NV (GDK_KEY_PRESS_MASK,		key-press-mask,		1 << 10),
  G_NV (GDK_KEY_RELEASE_MASK,		key-release-mask,	1 << 11),
  G_NV (GDK_ENTER_NOTIFY_MASK,		enter-notify-mask,	1 << 12),
  G_NV (GDK_LEAVE_NOTIFY_MASK,		leave-notify-mask,	1 << 13),
  G_NV (GDK_FOCUS_CHANGE_MASK,		focus-change-mask,	1 << 14),
  G_NV (GDK_STRUCTURE_MASK,		structure-mask,		1 << 15),
  G_NV (GDK_PROPERTY_CHANGE_MASK,	property-change-mask,	1 << 16),
  G_NV (GDK_VISIBILITY_NOTIFY_MASK,	visibility-notify-mask,	1 << 17),
  G_NV (GDK_PROXIMITY_IN_MASK,		proximity-in-mask,	1 << 18),
  G_NV (GDK_PROXIMITY_OUT_MASK,		proximity-out-mask,	1 << 19),
  G_NV (GDK_SUBSTRUCTURE_MASK,		substructure-mask,	1 << 20),
  G_NV (GDK_ALL_EVENTS_MASK,		all-events-mask,	0x0FFFFF)
} G_FLAGS (GdkEventMask);

/* Types of enter/leave notifications.
 *   Ancestor:
 *   Virtual:
 *   Inferior:
 *   Nonlinear:
 *   NonlinearVirtual:
 *   Unknown: An unknown type of enter/leave event occurred.
 */
typedef enum
{
  G_NV (GDK_NOTIFY_ANCESTOR,		ancestor,		0),
  G_NV (GDK_NOTIFY_VIRTUAL,		virtual,		1),
  G_NV (GDK_NOTIFY_INFERIOR,		inferior,		2),
  G_NV (GDK_NOTIFY_NONLINEAR,		nonlinear,		3),
  G_NV (GDK_NOTIFY_NONLINEAR_VIRTUAL,	nonlinear-virtual,	4),
  G_NV (GDK_NOTIFY_UNKNOWN,		unknown,		5)
} G_ENUM (GdkNotifyType);

/* Enter/leave event modes.
 *   NotifyNormal
 *   NotifyGrab
 *   NotifyUngrab
 */
typedef enum
{
  G_SV (GDK_CROSSING_NORMAL,	crossing-normal),
  G_SV (GDK_CROSSING_GRAB,	crossing-grab),
  G_SV (GDK_CROSSING_UNGRAB,	crossing-ungrab)
} G_ENUM (GdkCrossingMode);

/* Types of modifiers.
 */
typedef enum
{
  G_NV (GDK_SHIFT_MASK,		shift-mask,	1 << 0),
  G_NV (GDK_LOCK_MASK,		lock-mask,	1 << 1),
  G_NV (GDK_CONTROL_MASK,	control-mask,	1 << 2),
  G_NV (GDK_MOD1_MASK,		mod1-mask,	1 << 3),
  G_NV (GDK_MOD2_MASK,		mod2-mask,	1 << 4),
  G_NV (GDK_MOD3_MASK,		mod3-mask,	1 << 5),
  G_NV (GDK_MOD4_MASK,		mod4-mask,	1 << 6),
  G_NV (GDK_MOD5_MASK,		mod5-mask,	1 << 7),
  G_NV (GDK_BUTTON1_MASK,	button1-mask,	1 << 8),
  G_NV (GDK_BUTTON2_MASK,	button2-mask,	1 << 9),
  G_NV (GDK_BUTTON3_MASK,	button3-mask,	1 << 10),
  G_NV (GDK_BUTTON4_MASK,	button4-mask,	1 << 11),
  G_NV (GDK_BUTTON5_MASK,	button5-mask,	1 << 12),
  G_NV (GDK_MODIFIER_MASK,	modifier-mask,	0x1fff)
} G_FLAGS (GdkModifierType);

typedef enum
{
  G_NV (GDK_CLIP_BY_CHILDREN,	clip-by-children,	0),
  G_NV (GDK_INCLUDE_INFERIORS,	include-inferiors,	1)
} G_ENUM (GdkSubwindowMode);

typedef enum
{
  G_NV (GDK_INPUT_READ,		read,		1 << 0),
  G_NV (GDK_INPUT_WRITE,	write,		1 << 1),
  G_NV (GDK_INPUT_EXCEPTION,	exception,	1 << 2)
} G_FLAGS (GdkInputCondition);

typedef enum
{
  G_NV (GDK_OK,			ok,		 0),
  G_NV (GDK_ERROR,		error,		-1),
  G_NV (GDK_ERROR_PARAM,	error-param,	-2),
  G_NV (GDK_ERROR_FILE,		error-file,	-3),
  G_NV (GDK_ERROR_MEM,		error-mem,	-4)
} G_ENUM (GdkStatus);

typedef enum
{
  G_SV (GDK_LSB_FIRST,	lsb-first),
  G_SV (GDK_MSB_FIRST,	msb-first)
} G_ENUM (GdkByteOrder);

typedef enum
{
  G_NV (GDK_GC_FOREGROUND,	foreground,	1 << 0),
  G_NV (GDK_GC_BACKGROUND,	background,	1 << 1),
  G_NV (GDK_GC_FONT,		font,		1 << 2),
  G_NV (GDK_GC_FUNCTION,	function,	1 << 3),
  G_NV (GDK_GC_FILL,		fill,		1 << 4),
  G_NV (GDK_GC_TILE,		tile,		1 << 5),
  G_NV (GDK_GC_STIPPLE,		stipple,	1 << 6),
  G_NV (GDK_GC_CLIP_MASK,	clip-mask,	1 << 7),
  G_NV (GDK_GC_SUBWINDOW,	subwindow,	1 << 8),
  G_NV (GDK_GC_TS_X_ORIGIN,	ts-x-origin,	1 << 9),
  G_NV (GDK_GC_TS_Y_ORIGIN,	ts-y-origin,	1 << 10),
  G_NV (GDK_GC_CLIP_X_ORIGIN,	clip-x-origin,	1 << 11),
  G_NV (GDK_GC_CLIP_Y_ORIGIN,	clip-y-origin,	1 << 12),
  G_NV (GDK_GC_EXPOSURES,	exposures,	1 << 13),
  G_NV (GDK_GC_LINE_WIDTH,	line-width,	1 << 14),
  G_NV (GDK_GC_LINE_STYLE,	line-style,	1 << 15),
  G_NV (GDK_GC_CAP_STYLE,	cap-style,	1 << 16),
  G_NV (GDK_GC_JOIN_STYLE,	join-style,	1 << 17)
} G_FLAGS (GdkGCValuesMask);

typedef enum
{
  G_NV (GDK_SELECTION_PRIMARY,		primary,	1),
  G_NV (GDK_SELECTION_SECONDARY,	secondary,	2)
} G_ENUM (GdkSelection);

typedef enum
{
  G_SV (GDK_PROPERTY_NEW_VALUE,	new-value),
  G_SV (GDK_PROPERTY_DELETE,	delete)
} G_ENUM (GdkPropertyState);

typedef enum
{
  G_SV (GDK_PROP_MODE_REPLACE,	replace),
  G_SV (GDK_PROP_MODE_PREPEND,	prepend),
  G_SV (GDK_PROP_MODE_APPEND,	append)
} G_ENUM (GdkPropMode);

/* These definitions are for version 1 of the OffiX D&D protocol,
   taken from <OffiX/DragAndDropTypes.h> */
typedef enum
{
  G_NV (GDK_DNDTYPE_NOTDND,	NOTDND,		-1),
  G_NV (GDK_DNDTYPE_UNKNOWN,	UNKNOWN,	0),
  G_NV (GDK_DNDTYPE_RAWDATA,	RAWDATA,	1),
  G_NV (GDK_DNDTYPE_FILE,	FILE,		2),
  G_NV (GDK_DNDTYPE_FILES,	FILES,		3),
  G_NV (GDK_DNDTYPE_TEXT,	text,		4),
  G_NV (GDK_DNDTYPE_DIR,	dir,		5),
  G_NV (GDK_DNDTYPE_LINK,	link,		6),
  G_NV (GDK_DNDTYPE_EXE,	exe,		7),
  G_NV (GDK_DNDTYPE_URL,	url,		8),
  G_NV (GDK_DNDTYPE_MIME,	mime,		9),
  G_NV (GDK_DNDTYPE_END,	end,		10)
} G_ENUM (GdkDndType);

/* Enums for XInput support */

typedef enum
{
  G_SV (GDK_SOURCE_MOUSE,	mouse),
  G_SV (GDK_SOURCE_PEN,		pen),
  G_SV (GDK_SOURCE_ERASER,	eraser),
  G_SV (GDK_SOURCE_CURSOR,	cursor)
} G_ENUM (GdkInputSource);

typedef enum
{
  G_SV (GDK_MODE_DISABLED,	disabled),
  G_SV (GDK_MODE_SCREEN,	screen),
  G_SV (GDK_MODE_WINDOW,	window)
} G_ENUM (GdkInputMode);

typedef enum
{
  G_SV (GDK_AXIS_IGNORE,	ignore),
  G_SV (GDK_AXIS_X,		x),
  G_SV (GDK_AXIS_Y,		y),
  G_SV (GDK_AXIS_PRESSURE,	pressure),
  G_SV (GDK_AXIS_XTILT,		xtilt),
  G_SV (GDK_AXIS_YTILT,		ytilt),
  G_SV (GDK_AXIS_LAST,		last)
} G_ENUM (GdkAxisUse);

/* The next two types define enums for predefined atoms relating
 * to selections. In general, one will need to use gdk_intern_atom
 */
typedef enum
{
  G_NV (GDK_TARGET_BITMAP,	bitmap,		5),
  G_NV (GDK_TARGET_COLORMAP,	colormap,	7),
  G_NV (GDK_TARGET_DRAWABLE,	drawable,	17),
  G_NV (GDK_TARGET_PIXMAP,	pixmap,		20),
  G_NV (GDK_TARGET_STRING,	string,		31)
} G_ENUM (GdkTarget);

typedef enum
{
  G_NV (GDK_SELECTION_TYPE_ATOM,	atom,		4),
  G_NV (GDK_SELECTION_TYPE_BITMAP,	bitmap,		5),
  G_NV (GDK_SELECTION_TYPE_COLORMAP,	colormap,	7),
  G_NV (GDK_SELECTION_TYPE_DRAWABLE,	drawable,	17),
  G_NV (GDK_SELECTION_TYPE_INTEGER,	integer,	19),
  G_NV (GDK_SELECTION_TYPE_PIXMAP,	pixmap,		20),
  G_NV (GDK_SELECTION_TYPE_WINDOW,	window,		33),
  G_NV (GDK_SELECTION_TYPE_STRING,	string,		31)
} G_ENUM (GdkSelectionType);

typedef enum
{
  G_SV (GDK_EXTENSION_EVENTS_NONE,	none),
  G_SV (GDK_EXTENSION_EVENTS_ALL,	all),
  G_SV (GDK_EXTENSION_EVENTS_CURSOR,	cursor)
} G_ENUM (GdkExtensionMode);

typedef enum
{
  GdkIMPreeditArea	= 0x0001L,
  GdkIMPreeditCallbacks = 0x0002L,
  GdkIMPreeditPosition	= 0x0004L,
  GdkIMPreeditNothing	= 0x0008L,
  GdkIMPreeditNone	= 0x0010L,
  GdkIMStatusArea	= 0x0100L,
  GdkIMStatusCallbacks	= 0x0200L,
  GdkIMStatusNothing	= 0x0400L,
  GdkIMStatusNone	= 0x0800L
} GdkIMStyle;

/* The next two enumeration values current match the
 * Motif constants. If this is changed, the implementation
 * of gdk_window_set_decorations/gdk_window_set_functions
 * will need to change as well.
 */
typedef enum
{
  G_NV (GDK_DECOR_ALL,		all,		1 << 0),
  G_NV (GDK_DECOR_BORDER,	border,		1 << 1),
  G_NV (GDK_DECOR_RESIZEH,	resizeh,	1 << 2),
  G_NV (GDK_DECOR_TITLE,	title,		1 << 3),
  G_NV (GDK_DECOR_MENU,		menu,		1 << 4),
  G_NV (GDK_DECOR_MINIMIZE,	minimize,	1 << 5),
  G_NV (GDK_DECOR_MAXIMIZE,	maximize,	1 << 6)
} G_FLAGS (GdkWMDecoration);

typedef enum
{
  G_NV (GDK_FUNC_ALL,		all,		1 << 0),
  G_NV (GDK_FUNC_RESIZE,	resize,		1 << 1),
  G_NV (GDK_FUNC_MOVE,		move,		1 << 2),
  G_NV (GDK_FUNC_MINIMIZE,	minimize,	1 << 3),
  G_NV (GDK_FUNC_MAXIMIZE,	maximize,	1 << 4),
  G_NV (GDK_FUNC_CLOSE,		close,		1 << 5)
} G_FLAGS (GdkWMFunction);

#define GdkIMPreeditMask \
	( GdkIMPreeditArea     | GdkIMPreeditCallbacks | \
	  GdkIMPreeditPosition | GdkIMPreeditNothing | \
	  GdkIMPreeditNone )

#define GdkIMStatusMask \
	( GdkIMStatusArea | GdkIMStatusCallbacks | \
	  GdkIMStatusNothing | GdkIMStatusNone )

typedef void (*GdkInputFunction) (gpointer	    data,
				  gint		    source,
				  GdkInputCondition condition);

typedef void (*GdkDestroyNotify) (gpointer data);

/* Color Context modes.
 *
 * GDK_CC_MODE_UNDEFINED - unknown
 * GDK_CC_MODE_BW	 - default B/W
 * GDK_CC_MODE_STD_CMAP	 - has a standard colormap
 * GDK_CC_MODE_TRUE	 - is a TrueColor/DirectColor visual
 * GDK_CC_MODE_MY_GRAY	 - my grayramp
 * GDK_CC_MODE_PALETTE	 - has a pre-allocated palette
 */ 

typedef enum
{
  G_SV (GDK_CC_MODE_UNDEFINED,	undefined),
  G_SV (GDK_CC_MODE_BW,		bw),
  G_SV (GDK_CC_MODE_STD_CMAP,	std-cmap),
  G_SV (GDK_CC_MODE_TRUE,	true),
  G_SV (GDK_CC_MODE_MY_GRAY,	my-gray),
  G_SV (GDK_CC_MODE_PALETTE,	palette)
} G_ENUM (GdkColorContextMode);

/* Types of overlapping between a rectangle and a region
 * GDK_OVERLAP_RECTANGLE_IN: rectangle is in region
 * GDK_OVERLAP_RECTANGLE_OUT: rectangle in not in region
 * GDK_OVERLAP_RECTANGLE_PART: rectangle in partially in region
 */

typedef enum
{
  G_SV (GDK_OVERLAP_RECTANGLE_IN,	in),
  G_SV (GDK_OVERLAP_RECTANGLE_OUT,	out),
  G_SV (GDK_OVERLAP_RECTANGLE_PART,	part)
} G_ENUM (GdkOverlapType);

/* The color type.
 *   A color consists of red, green and blue values in the
 *    range 0-65535 and a pixel value. The pixel value is highly
 *    dependent on the depth and colormap which this color will
 *    be used to draw into. Therefore, sharing colors between
 *    colormaps is a bad idea.
 */
struct _GdkColor
{
  gulong  pixel;
  gushort red;
  gushort green;
  gushort blue;
};

/* The colormap type.
 *   Colormaps consist of 256 colors.
 */
struct _GdkColormap
{
  gint	    size;
  GdkColor *colors;
};

/* The visual type.
 *   "type" is the type of visual this is (PseudoColor, TrueColor, etc).
 *   "depth" is the bit depth of this visual.
 *   "colormap_size" is the size of a colormap for this visual.
 *   "bits_per_rgb" is the number of significant bits per red, green and blue.
 *  The red, green and blue masks, shifts and precisions refer
 *   to value needed to calculate pixel values in TrueColor and DirectColor
 *   visuals. The "mask" is the significant bits within the pixel. The
 *   "shift" is the number of bits left we must shift a primary for it
 *   to be in position (according to the "mask"). "prec" refers to how
 *   much precision the pixel value contains for a particular primary.
 */
struct _GdkVisual
{
  GdkVisualType type;
  gint depth;
  GdkByteOrder byte_order;
  gint colormap_size;
  gint bits_per_rgb;
  
  guint32 red_mask;
  gint red_shift;
  gint red_prec;
  
  guint32 green_mask;
  gint green_shift;
  gint green_prec;
  
  guint32 blue_mask;
  gint blue_shift;
  gint blue_prec;
};

struct _GdkWindowAttr
{
  gchar *title;
  gint event_mask;
  gint16 x, y;
  gint16 width;
  gint16 height;
  GdkWindowClass wclass;
  GdkVisual *visual;
  GdkColormap *colormap;
  GdkWindowType window_type;
  GdkCursor *cursor;
  gchar *wmclass_name;
  gchar *wmclass_class;
  gboolean override_redirect;
};

struct _GdkWindow
{
  gpointer user_data;
};

struct _GdkImage
{
  GdkImageType	type;
  GdkVisual    *visual;	    /* visual used to create the image */
  GdkByteOrder	byte_order;
  guint16	width;
  guint16	height;
  guint16	depth;
  guint16	bpp;	    /* bytes per pixel */
  guint16	bpl;	    /* bytes per line */
  gpointer	mem;
};

struct _GdkGCValues
{
  GdkColor	    foreground;
  GdkColor	    background;
  GdkFont	   *font;
  GdkFunction	    function;
  GdkFill	    fill;
  GdkPixmap	   *tile;
  GdkPixmap	   *stipple;
  GdkPixmap	   *clip_mask;
  GdkSubwindowMode  subwindow_mode;
  gint		    ts_x_origin;
  gint		    ts_y_origin;
  gint		    clip_x_origin;
  gint		    clip_y_origin;
  gint		    graphics_exposures;
  gint		    line_width;
  GdkLineStyle	    line_style;
  GdkCapStyle	    cap_style;
  GdkJoinStyle	    join_style;
};

struct _GdkGC
{
  gint dummy_var;
};

struct _GdkPoint
{
  gint16 x;
  gint16 y;
};

struct _GdkRectangle
{
  gint16 x;
  gint16 y;
  guint16 width;
  guint16 height;
};

struct _GdkSegment
{
  gint16 x1;
  gint16 y1;
  gint16 x2;
  gint16 y2;
};

struct _GdkFont
{
  GdkFontType type;
  gint ascent;
  gint descent;
};

struct _GdkCursor
{
  GdkCursorType type;
};

struct _GdkColorContextDither
{
  gint fast_rgb[32][32][32]; /* quick look-up table for faster rendering */
  gint fast_err[32][32][32]; /* internal RGB error information */
  gint fast_erg[32][32][32];
  gint fast_erb[32][32][32];
};

struct _GdkColorContext
{
  GdkVisual *visual;
  GdkColormap *colormap;
  
  gint num_colors;		/* available no. of colors in colormap */
  gint max_colors;		/* maximum no. of colors */
  gint num_allocated;		/* no. of allocated colors */
  
  GdkColorContextMode mode;
  gint need_to_free_colormap;
  GdkAtom std_cmap_atom;
  
  gulong *clut;			/* color look-up table */
  GdkColor *cmap;		/* colormap */
  
  GHashTable *color_hash;	/* hash table of allocated colors */
  GdkColor *palette;		/* preallocated palette */
  gint num_palette;		/* size of palette */
  
  GdkColorContextDither *fast_dither;	/* fast dither matrix */
  
  struct
  {
    gint red;
    gint green;
    gint blue;
  } shifts;
  
  struct
  {
    gulong red;
    gulong green;
    gulong blue;
  } masks;
  
  struct
  {
    gint red;
    gint green;
    gint blue;
  } bits;
  
  gulong max_entry;
  
  gulong black_pixel;
  gulong white_pixel;
};

/* Types for XInput support */

struct _GdkDeviceKey
{
  guint keyval;
  GdkModifierType modifiers;
};

struct _GdkDeviceInfo
{
  guint32 deviceid;
  gchar *name;
  GdkInputSource source;
  GdkInputMode mode;
  gint has_cursor;	/* TRUE if the X pointer follows device motion */
  gint num_axes;
  GdkAxisUse *axes;    /* Specifies use for each axis */
  gint num_keys;
  GdkDeviceKey *keys;
};

struct _GdkTimeCoord
{
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble pressure;
  gdouble xtilt;
  gdouble ytilt;
};

/* Event filtering */

typedef void GdkXEvent;	  /* Can be cast to XEvent */

typedef GdkFilterReturn (*GdkFilterFunc) (GdkXEvent *xevent,
					  GdkEvent *event,
					  gpointer  data);

struct _GdkEventAny
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
};

struct _GdkEventExpose
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkRectangle area;
  gint count; /* If non-zero, how many more events follow. */
};

struct _GdkEventNoExpose
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  /* XXX: does anyone need the X major_code or minor_code fields? */
};

struct _GdkEventVisibility
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkVisibilityState state;
};

struct _GdkEventMotion
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble pressure;
  gdouble xtilt;
  gdouble ytilt;
  guint state;
  gint16 is_hint;
  GdkInputSource source;
  guint32 deviceid;
  gdouble x_root, y_root;
};

struct _GdkEventButton
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble pressure;
  gdouble xtilt;
  gdouble ytilt;
  guint state;
  guint button;
  GdkInputSource source;
  guint32 deviceid;
  gdouble x_root, y_root;
};

struct _GdkEventKey
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  guint state;
  guint keyval;
  gint length;
  gchar *string;
};

struct _GdkEventCrossing
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkWindow *subwindow;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble x_root;
  gdouble y_root;
  GdkCrossingMode mode;
  GdkNotifyType detail;
  gboolean focus;
  guint state;
};

struct _GdkEventFocus
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  gint16 in;
};

struct _GdkEventConfigure
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  gint16 x, y;
  gint16 width;
  gint16 height;
};

struct _GdkEventProperty
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkAtom atom;
  guint32 time;
  guint state;
};

struct _GdkEventSelection
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkAtom selection;
  GdkAtom target;
  GdkAtom property;
  guint32 requestor;
  guint32 time;
};

/* This event type will be used pretty rarely. It only is important
   for XInput aware programs that are drawing their own cursor */

struct _GdkEventProximity
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  GdkInputSource source;
  guint32 deviceid;
};

struct _GdkEventDragRequest
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 requestor;
  union {
    struct {
      guint protocol_version:4;
      guint sendreply:1;
      guint willaccept:1;
      guint delete_data:1; /* Do *not* delete if link is sent, only
			      if data is sent */
      guint senddata:1;
      guint reserved:22;
    } flags;
    glong allflags;
  } u;
  guint8 isdrop; /* This gdk event can be generated by a couple of
		    X events - this lets the app know whether the
		    drop really occurred or we just set the data */
  
  GdkPoint drop_coords;
  gchar *data_type;
  guint32 timestamp;
};

struct _GdkEventDragBegin
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  union {
    struct {
      guint protocol_version:4;
      guint reserved:28;
    } flags;
    glong allflags;
  } u;
};

struct _GdkEventDropEnter
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 requestor;
  union {
    struct {
      guint protocol_version:4;
      guint sendreply:1;
      guint extended_typelist:1;
      guint reserved:26;
    } flags;
    glong allflags;
  } u;
};

struct _GdkEventDropLeave
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 requestor;
  union {
    struct {
      guint protocol_version:4;
      guint reserved:28;
    } flags;
    glong allflags;
  } u;
};

struct _GdkEventDropDataAvailable
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 requestor;
  union {
    struct {
      guint protocol_version:4;
      guint isdrop:1;
      guint reserved:25;
    } flags;
    glong allflags;
  } u;
  gchar *data_type; /* MIME type */
  gulong data_numbytes;
  gpointer data;
  guint32 timestamp;
  GdkPoint coords;
};

struct _GdkEventClient
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkAtom message_type;
  gushort data_format;
  union {
    char b[20];
    short s[10];
    long l[5];
  } data;
};

struct _GdkEventOther
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkXEvent *xevent;
};

union _GdkEvent
{
  GdkEventType		    type;
  GdkEventAny		    any;
  GdkEventExpose	    expose;
  GdkEventNoExpose	    no_expose;
  GdkEventVisibility	    visibility;
  GdkEventMotion	    motion;
  GdkEventButton	    button;
  GdkEventKey		    key;
  GdkEventCrossing	    crossing;
  GdkEventFocus		    focus_change;
  GdkEventConfigure	    configure;
  GdkEventProperty	    property;
  GdkEventSelection	    selection;
  GdkEventProximity	    proximity;
  GdkEventDragBegin	    dragbegin;
  GdkEventDragRequest	    dragrequest;
  GdkEventDropEnter	    dropenter;
  GdkEventDropLeave	    dropleave;
  GdkEventDropDataAvailable dropdataavailable;
  GdkEventClient	    client;
  GdkEventOther		    other;
};

struct _GdkRegion
{
  gpointer user_data;
};



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_TYPES_H__ */
