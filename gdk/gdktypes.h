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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
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
typedef struct _GdkGeometry           GdkGeometry;
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
typedef struct _GdkDragContext        GdkDragContext;

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
typedef struct _GdkEventClient	    GdkEventClient;

typedef struct _GdkEventDND         GdkEventDND;

typedef union  _GdkEvent	    GdkEvent;

typedef struct _GdkDeviceKey	    GdkDeviceKey;
typedef struct _GdkDeviceInfo	    GdkDeviceInfo;
typedef struct _GdkTimeCoord	    GdkTimeCoord;
typedef struct _GdkRegion	    GdkRegion;
typedef void (*GdkEventFunc) (GdkEvent *event,
			      gpointer	data);

typedef struct _GdkIC               GdkIC;
typedef struct _GdkICAttr	    GdkICAttr;

typedef guint32			    GdkWChar;
 

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
  GDK_WINDOW_ROOT,
  GDK_WINDOW_TOPLEVEL,
  GDK_WINDOW_CHILD,
  GDK_WINDOW_DIALOG,
  GDK_WINDOW_TEMP,
  GDK_WINDOW_PIXMAP,
  GDK_WINDOW_FOREIGN
} GdkWindowType;

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
  GDK_INPUT_OUTPUT,
  GDK_INPUT_ONLY
} GdkWindowClass;

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
  GDK_IMAGE_NORMAL,
  GDK_IMAGE_SHARED,
  GDK_IMAGE_FASTEST
} GdkImageType;

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
  GDK_VISUAL_STATIC_GRAY,
  GDK_VISUAL_GRAYSCALE,
  GDK_VISUAL_STATIC_COLOR,
  GDK_VISUAL_PSEUDO_COLOR,
  GDK_VISUAL_TRUE_COLOR,
  GDK_VISUAL_DIRECT_COLOR
} GdkVisualType;

/* Types of font.
 *   GDK_FONT_FONT: the font is an XFontStruct.
 *   GDK_FONT_FONTSET: the font is an XFontSet used for I18N.
 */
typedef enum
{
  GDK_FONT_FONT,
  GDK_FONT_FONTSET
} GdkFontType;

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
  GDK_WA_TITLE	  = 1 << 1,
  GDK_WA_X	  = 1 << 2,
  GDK_WA_Y	  = 1 << 3,
  GDK_WA_CURSOR	  = 1 << 4,
  GDK_WA_COLORMAP = 1 << 5,
  GDK_WA_VISUAL	  = 1 << 6,
  GDK_WA_WMCLASS  = 1 << 7,
  GDK_WA_NOREDIR  = 1 << 8
} GdkWindowAttributesType;

/* Size restriction enumeration.
 */
typedef enum
{
  GDK_HINT_POS	      = 1 << 0,
  GDK_HINT_MIN_SIZE   = 1 << 1,
  GDK_HINT_MAX_SIZE   = 1 << 2,
  GDK_HINT_BASE_SIZE  = 1 << 3,
  GDK_HINT_ASPECT     = 1 << 4,
  GDK_HINT_RESIZE_INC = 1 << 5
} GdkWindowHints;

/* GC function types.
 *   Copy: Overwrites destination pixels with the source pixels.
 *   Invert: Inverts the destination pixels.
 *   Xor: Xor's the destination pixels with the source pixels.
 *   Clear: set pixels to 0
 *   And: source AND destination
 *   And Reverse: source AND (NOT destination)
 *   And Invert: (NOT source) AND destination
 *   Noop: destination
 *   Or: source OR destination
 *   Nor: (NOT source) AND (NOT destination)
 *   Equiv: (NOT source) XOR destination
 *   Xor Reverse: source OR (NOT destination)
 *   Copy Inverted: NOT source
 *   Xor Inverted: (NOT source) OR destination
 *   Nand: (NOT source) OR (NOT destination)
 *   Set: set pixels to 1
 */
typedef enum
{
  GDK_COPY,
  GDK_INVERT,
  GDK_XOR,
  GDK_CLEAR,
  GDK_AND,
  GDK_AND_REVERSE,
  GDK_AND_INVERT,
  GDK_NOOP,
  GDK_OR,
  GDK_EQUIV,
  GDK_OR_REVERSE,
  GDK_COPY_INVERT,
  GDK_OR_INVERT,
  GDK_NAND,
  GDK_SET
} GdkFunction;

/* GC fill types.
 *  Solid:
 *  Tiled:
 *  Stippled:
 *  OpaqueStippled:
 */
typedef enum
{
  GDK_SOLID,
  GDK_TILED,
  GDK_STIPPLED,
  GDK_OPAQUE_STIPPLED
} GdkFill;

/* GC fill rule for polygons
 *  EvenOddRule
 *  WindingRule
 */
typedef enum
{
  GDK_EVEN_ODD_RULE,
  GDK_WINDING_RULE
} GdkFillRule;

/* GC line styles
 *  Solid:
 *  OnOffDash:
 *  DoubleDash:
 */
typedef enum
{
  GDK_LINE_SOLID,
  GDK_LINE_ON_OFF_DASH,
  GDK_LINE_DOUBLE_DASH
} GdkLineStyle;

/* GC cap styles
 *  CapNotLast:
 *  CapButt:
 *  CapRound:
 *  CapProjecting:
 */
typedef enum
{
  GDK_CAP_NOT_LAST,
  GDK_CAP_BUTT,
  GDK_CAP_ROUND,
  GDK_CAP_PROJECTING
} GdkCapStyle;

/* GC join styles
 *  JoinMiter:
 *  JoinRound:
 *  JoinBevel:
 */
typedef enum
{
  GDK_JOIN_MITER,
  GDK_JOIN_ROUND,
  GDK_JOIN_BEVEL
} GdkJoinStyle;

/* Cursor types.
 */
typedef enum
{
#include <gdk/gdkcursors.h>
  GDK_LAST_CURSOR,
  GDK_CURSOR_IS_PIXMAP = -1
} GdkCursorType;

typedef enum {
  GDK_FILTER_CONTINUE,	  /* Event not handled, continue processesing */
  GDK_FILTER_TRANSLATE,	  /* Translated event stored */
  GDK_FILTER_REMOVE	  /* Terminate processing, removing event */
} GdkFilterReturn;

typedef enum {
  GDK_VISIBILITY_UNOBSCURED,
  GDK_VISIBILITY_PARTIAL,
  GDK_VISIBILITY_FULLY_OBSCURED
} GdkVisibilityState;

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
  GDK_NOTHING		= -1,
  GDK_DELETE		= 0,
  GDK_DESTROY		= 1,
  GDK_EXPOSE		= 2,
  GDK_MOTION_NOTIFY	= 3,
  GDK_BUTTON_PRESS	= 4,
  GDK_2BUTTON_PRESS	= 5,
  GDK_3BUTTON_PRESS	= 6,
  GDK_BUTTON_RELEASE	= 7,
  GDK_KEY_PRESS		= 8,
  GDK_KEY_RELEASE	= 9,
  GDK_ENTER_NOTIFY	= 10,
  GDK_LEAVE_NOTIFY	= 11,
  GDK_FOCUS_CHANGE	= 12,
  GDK_CONFIGURE		= 13,
  GDK_MAP		= 14,
  GDK_UNMAP		= 15,
  GDK_PROPERTY_NOTIFY	= 16,
  GDK_SELECTION_CLEAR	= 17,
  GDK_SELECTION_REQUEST = 18,
  GDK_SELECTION_NOTIFY	= 19,
  GDK_PROXIMITY_IN	= 20,
  GDK_PROXIMITY_OUT	= 21,
  GDK_DRAG_ENTER        = 22,
  GDK_DRAG_LEAVE        = 23,
  GDK_DRAG_MOTION       = 24,
  GDK_DRAG_STATUS       = 25,
  GDK_DROP_START        = 26,
  GDK_DROP_FINISHED     = 27,
  GDK_CLIENT_EVENT	= 28,
  GDK_VISIBILITY_NOTIFY = 29,
  GDK_NO_EXPOSE		= 30
} GdkEventType;

/* Event masks. (Used to select what types of events a window
 *  will receive).
 */
typedef enum
{
  GDK_EXPOSURE_MASK		= 1 << 1,
  GDK_POINTER_MOTION_MASK	= 1 << 2,
  GDK_POINTER_MOTION_HINT_MASK	= 1 << 3,
  GDK_BUTTON_MOTION_MASK	= 1 << 4,
  GDK_BUTTON1_MOTION_MASK	= 1 << 5,
  GDK_BUTTON2_MOTION_MASK	= 1 << 6,
  GDK_BUTTON3_MOTION_MASK	= 1 << 7,
  GDK_BUTTON_PRESS_MASK		= 1 << 8,
  GDK_BUTTON_RELEASE_MASK	= 1 << 9,
  GDK_KEY_PRESS_MASK		= 1 << 10,
  GDK_KEY_RELEASE_MASK		= 1 << 11,
  GDK_ENTER_NOTIFY_MASK		= 1 << 12,
  GDK_LEAVE_NOTIFY_MASK		= 1 << 13,
  GDK_FOCUS_CHANGE_MASK		= 1 << 14,
  GDK_STRUCTURE_MASK		= 1 << 15,
  GDK_PROPERTY_CHANGE_MASK	= 1 << 16,
  GDK_VISIBILITY_NOTIFY_MASK	= 1 << 17,
  GDK_PROXIMITY_IN_MASK		= 1 << 18,
  GDK_PROXIMITY_OUT_MASK	= 1 << 19,
  GDK_SUBSTRUCTURE_MASK		= 1 << 20,
  GDK_ALL_EVENTS_MASK		= 0x0FFFFF
} GdkEventMask;

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
  GDK_NOTIFY_ANCESTOR		= 0,
  GDK_NOTIFY_VIRTUAL		= 1,
  GDK_NOTIFY_INFERIOR		= 2,
  GDK_NOTIFY_NONLINEAR		= 3,
  GDK_NOTIFY_NONLINEAR_VIRTUAL	= 4,
  GDK_NOTIFY_UNKNOWN		= 5
} GdkNotifyType;

/* Enter/leave event modes.
 *   NotifyNormal
 *   NotifyGrab
 *   NotifyUngrab
 */
typedef enum
{
  GDK_CROSSING_NORMAL,
  GDK_CROSSING_GRAB,
  GDK_CROSSING_UNGRAB
} GdkCrossingMode;

/* Types of modifiers.
 */
typedef enum
{
  GDK_SHIFT_MASK    = 1 << 0,
  GDK_LOCK_MASK	    = 1 << 1,
  GDK_CONTROL_MASK  = 1 << 2,
  GDK_MOD1_MASK	    = 1 << 3,
  GDK_MOD2_MASK	    = 1 << 4,
  GDK_MOD3_MASK	    = 1 << 5,
  GDK_MOD4_MASK	    = 1 << 6,
  GDK_MOD5_MASK	    = 1 << 7,
  GDK_BUTTON1_MASK  = 1 << 8,
  GDK_BUTTON2_MASK  = 1 << 9,
  GDK_BUTTON3_MASK  = 1 << 10,
  GDK_BUTTON4_MASK  = 1 << 11,
  GDK_BUTTON5_MASK  = 1 << 12,
  GDK_RELEASE_MASK  = 1 << 13,
  GDK_MODIFIER_MASK = 0x3fff
} GdkModifierType;

typedef enum
{
  GDK_CLIP_BY_CHILDREN	= 0,
  GDK_INCLUDE_INFERIORS = 1
} GdkSubwindowMode;

typedef enum
{
  GDK_INPUT_READ       = 1 << 0,
  GDK_INPUT_WRITE      = 1 << 1,
  GDK_INPUT_EXCEPTION  = 1 << 2
} GdkInputCondition;

typedef enum
{
  GDK_OK	  = 0,
  GDK_ERROR	  = -1,
  GDK_ERROR_PARAM = -2,
  GDK_ERROR_FILE  = -3,
  GDK_ERROR_MEM	  = -4
} GdkStatus;

typedef enum
{
  GDK_LSB_FIRST,
  GDK_MSB_FIRST
} GdkByteOrder;

typedef enum
{
  GDK_GC_FOREGROUND    = 1 << 0,
  GDK_GC_BACKGROUND    = 1 << 1,
  GDK_GC_FONT	       = 1 << 2,
  GDK_GC_FUNCTION      = 1 << 3,
  GDK_GC_FILL	       = 1 << 4,
  GDK_GC_TILE	       = 1 << 5,
  GDK_GC_STIPPLE       = 1 << 6,
  GDK_GC_CLIP_MASK     = 1 << 7,
  GDK_GC_SUBWINDOW     = 1 << 8,
  GDK_GC_TS_X_ORIGIN   = 1 << 9,
  GDK_GC_TS_Y_ORIGIN   = 1 << 10,
  GDK_GC_CLIP_X_ORIGIN = 1 << 11,
  GDK_GC_CLIP_Y_ORIGIN = 1 << 12,
  GDK_GC_EXPOSURES     = 1 << 13,
  GDK_GC_LINE_WIDTH    = 1 << 14,
  GDK_GC_LINE_STYLE    = 1 << 15,
  GDK_GC_CAP_STYLE     = 1 << 16,
  GDK_GC_JOIN_STYLE    = 1 << 17
} GdkGCValuesMask;

typedef enum
{
  GDK_SELECTION_PRIMARY = 1,
  GDK_SELECTION_SECONDARY = 2
} GdkSelection;

typedef enum
{
  GDK_PROPERTY_NEW_VALUE,
  GDK_PROPERTY_DELETE
} GdkPropertyState;

typedef enum
{
  GDK_PROP_MODE_REPLACE,
  GDK_PROP_MODE_PREPEND,
  GDK_PROP_MODE_APPEND
} GdkPropMode;

/* Enums for XInput support */

typedef enum
{
  GDK_SOURCE_MOUSE,
  GDK_SOURCE_PEN,
  GDK_SOURCE_ERASER,
  GDK_SOURCE_CURSOR
} GdkInputSource;

typedef enum
{
  GDK_MODE_DISABLED,
  GDK_MODE_SCREEN,
  GDK_MODE_WINDOW
} GdkInputMode;

typedef enum
{
  GDK_AXIS_IGNORE,
  GDK_AXIS_X,
  GDK_AXIS_Y,
  GDK_AXIS_PRESSURE,
  GDK_AXIS_XTILT,
  GDK_AXIS_YTILT,
  GDK_AXIS_LAST
} GdkAxisUse;

/* The next two types define enums for predefined atoms relating
   to selections. In general, one will need to use gdk_intern_atom */

typedef enum
{
  GDK_TARGET_BITMAP = 5,
  GDK_TARGET_COLORMAP = 7,
  GDK_TARGET_DRAWABLE = 17,
  GDK_TARGET_PIXMAP = 20,
  GDK_TARGET_STRING = 31
} GdkTarget;

typedef enum
{
  GDK_SELECTION_TYPE_ATOM = 4,
  GDK_SELECTION_TYPE_BITMAP = 5,
  GDK_SELECTION_TYPE_COLORMAP = 7,
  GDK_SELECTION_TYPE_DRAWABLE = 17,
  GDK_SELECTION_TYPE_INTEGER = 19,
  GDK_SELECTION_TYPE_PIXMAP = 20,
  GDK_SELECTION_TYPE_WINDOW = 33,
  GDK_SELECTION_TYPE_STRING = 31
} GdkSelectionType;

typedef enum
{
  GDK_EXTENSION_EVENTS_NONE,
  GDK_EXTENSION_EVENTS_ALL,
  GDK_EXTENSION_EVENTS_CURSOR
} GdkExtensionMode;

typedef enum			/*< flags >*/
{
  GDK_IM_PREEDIT_AREA	   = 0x0001, 
  GDK_IM_PREEDIT_CALLBACKS = 0x0002, 
  GDK_IM_PREEDIT_POSITION  = 0x0004,
  GDK_IM_PREEDIT_NOTHING   = 0x0008,
  GDK_IM_PREEDIT_NONE	   = 0x0010,
  GDK_IM_PREEDIT_MASK      = 0x001f,

  GDK_IM_STATUS_AREA	   = 0x0100, 
  GDK_IM_STATUS_CALLBACKS  = 0x0200,
  GDK_IM_STATUS_NOTHING	   = 0x0400,
  GDK_IM_STATUS_NONE	   = 0x0800,
  GDK_IM_STATUS_MASK	   = 0x0f00 
} GdkIMStyle;

typedef enum
{
  GDK_IC_STYLE			= 1 << 0,
  GDK_IC_CLIENT_WINDOW		= 1 << 1,
  GDK_IC_FOCUS_WINDOW		= 1 << 2,
  GDK_IC_FILTER_EVENTS		= 1 << 3,
  GDK_IC_SPOT_LOCATION		= 1 << 4,
  GDK_IC_LINE_SPACING		= 1 << 5,
  GDK_IC_CURSOR			= 1 << 6,

  GDK_IC_PREEDIT_FONTSET	= 1 << 10,
  GDK_IC_PREEDIT_AREA		= 1 << 11,
  GDK_IC_PREEDIT_AREA_NEEDED	= 1 << 12,
  GDK_IC_PREEDIT_FOREGROUND	= 1 << 13,
  GDK_IC_PREEDIT_BACKGROUND	= 1 << 14,
  GDK_IC_PREEDIT_PIXMAP		= 1 << 15,
  GDK_IC_PREEDIT_COLORMAP	= 1 << 16,

  GDK_IC_STATUS_FONTSET		= 1 << 21,
  GDK_IC_STATUS_AREA		= 1 << 22,
  GDK_IC_STATUS_AREA_NEEDED	= 1 << 23,
  GDK_IC_STATUS_FOREGROUND	= 1 << 24,
  GDK_IC_STATUS_BACKGROUND	= 1 << 25,
  GDK_IC_STATUS_PIXMAP		= 1 << 26,
  GDK_IC_STATUS_COLORMAP	= 1 << 27,

  GDK_IC_ALL_REQ		= GDK_IC_STYLE |
    				  GDK_IC_CLIENT_WINDOW,

  GDK_IC_PREEDIT_AREA_REQ	= GDK_IC_PREEDIT_AREA | 
				  GDK_IC_PREEDIT_FONTSET,
  GDK_IC_PREEDIT_POSITION_REQ	= GDK_IC_PREEDIT_AREA | GDK_IC_SPOT_LOCATION |
				  GDK_IC_PREEDIT_FONTSET,

  GDK_IC_STATUS_AREA_REQ	= GDK_IC_STATUS_AREA | 
				  GDK_IC_STATUS_FONTSET
} GdkICAttributesType;

/* The next two enumeration values current match the
 * Motif constants. If this is changed, the implementation
 * of gdk_window_set_decorations/gdk_window_set_functions
 * will need to change as well.
 */
typedef enum
{
  GDK_DECOR_ALL		= 1 << 0,
  GDK_DECOR_BORDER	= 1 << 1,
  GDK_DECOR_RESIZEH	= 1 << 2,
  GDK_DECOR_TITLE	= 1 << 3,
  GDK_DECOR_MENU	= 1 << 4,
  GDK_DECOR_MINIMIZE	= 1 << 5,
  GDK_DECOR_MAXIMIZE	= 1 << 6
} GdkWMDecoration;

typedef enum
{
  GDK_FUNC_ALL		= 1 << 0,
  GDK_FUNC_RESIZE	= 1 << 1,
  GDK_FUNC_MOVE		= 1 << 2,
  GDK_FUNC_MINIMIZE	= 1 << 3,
  GDK_FUNC_MAXIMIZE	= 1 << 4,
  GDK_FUNC_CLOSE	= 1 << 5
} GdkWMFunction;

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
  GDK_CC_MODE_UNDEFINED,
  GDK_CC_MODE_BW,
  GDK_CC_MODE_STD_CMAP,
  GDK_CC_MODE_TRUE,
  GDK_CC_MODE_MY_GRAY,
  GDK_CC_MODE_PALETTE
} GdkColorContextMode;

/* Types of overlapping between a rectangle and a region
 * GDK_OVERLAP_RECTANGLE_IN: rectangle is in region
 * GDK_OVERLAP_RECTANGLE_OUT: rectangle in not in region
 * GDK_OVERLAP_RECTANGLE_PART: rectangle in partially in region
 */

typedef enum
{
  GDK_OVERLAP_RECTANGLE_IN,
  GDK_OVERLAP_RECTANGLE_OUT,
  GDK_OVERLAP_RECTANGLE_PART
} GdkOverlapType;

typedef enum {
  GDK_ACTION_DEFAULT = 1 << 0,
  GDK_ACTION_COPY    = 1 << 1,
  GDK_ACTION_MOVE    = 1 << 2,
  GDK_ACTION_LINK    = 1 << 3,
  GDK_ACTION_PRIVATE = 1 << 4,
  GDK_ACTION_ASK     = 1 << 5
} GdkDragAction;

typedef enum {
  GDK_DRAG_PROTO_MOTIF,
  GDK_DRAG_PROTO_XDND,
  GDK_DRAG_PROTO_ROOTWIN,	/* A root window with nobody claiming
				 * drags */
  GDK_DRAG_PROTO_NONE		/* Not a valid drag window */
} GdkDragProtocol;

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
  gint      size;
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

struct _GdkGeometry {
  gint min_width;
  gint min_height;
  gint max_width;
  gint max_height;
  gint base_width;
  gint base_height;
  gint width_inc;
  gint height_inc;
  gdouble min_aspect;
  gdouble max_aspect;
  /* GdkGravity gravity; */
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

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContext {
  GdkDragProtocol protocol;
  
  gboolean is_source;
  
  GdkWindow *source_window;
  GdkWindow *dest_window;

  GList *targets;
  GdkDragAction actions;
  GdkDragAction suggested_action;
  GdkDragAction action; 

  guint32 start_time;
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

/* Event types for DND */

struct _GdkEventDND {
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkDragContext *context;

  guint32 time;
  gshort x_root, y_root;
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
  GdkEventClient	    client;
  GdkEventDND               dnd;
};

struct _GdkRegion
{
  gpointer user_data;
};

struct _GdkICAttr
{
  GdkIMStyle style;
  GdkWindow *client_window;
  GdkWindow *focus_window;
  GdkEventMask filter_events;
  GdkPoint spot_location;
  gint line_spacing;
  GdkCursor *cursor;

  GdkFont *preedit_fontset;
  GdkRectangle preedit_area;
  GdkRectangle preedit_area_needed; 
  GdkColor preedit_foreground;
  GdkColor preedit_background;
  GdkPixmap *preedit_pixmap;
  GdkColormap *preedit_colormap;

  GdkFont *status_fontset;
  GdkRectangle status_area;
  GdkRectangle status_area_needed; 
  GdkColor status_foreground;
  GdkColor status_background;
  GdkPixmap *status_pixmap;
  GdkColormap *status_colormap;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_TYPES_H__ */
