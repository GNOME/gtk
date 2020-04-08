/* GDK - The GIMP Drawing Kit
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

#ifndef __GDK_TYPES_H__
#define __GDK_TYPES_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

/* GDK uses "glib". (And so does GTK).
 */
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <cairo.h>
#include <pango/pango.h>

/* The system specific file gdkconfig.h contains such configuration
 * settings that are needed not only when compiling GDK (or GTK)
 * itself, but also occasionally when compiling programs that use GDK
 * (or GTK). One such setting is what windowing API backend is in use.
 */
#include <gdk/gdkconfig.h>

/* some common magic values */

/**
 * GDK_CURRENT_TIME:
 *
 * Represents the current time, and can be used anywhere a time is expected.
 */
#define GDK_CURRENT_TIME     0L

/**
 * GDK_PARENT_RELATIVE:
 *
 * A special value, indicating that the background
 * for a surface should be inherited from the parent surface.
 */
#define GDK_PARENT_RELATIVE  1L



G_BEGIN_DECLS


/**
 * GdkPoint:
 * @x: the x coordinate of the point
 * @y: the y coordinate of the point
 *
 * Defines the x and y coordinates of a point.
 */
struct _GdkPoint
{
  gint x;
  gint y;
};
typedef struct _GdkPoint              GdkPoint;

/**
 * GdkRectangle:
 * @x: the x coordinate of the top left corner
 * @y: the y coordinate of the top left corner
 * @width: the width of the rectangle
 * @height: the height of the rectangle
 *
 * Defines the position and size of a rectangle. It is identical to
 * #cairo_rectangle_int_t.
 */
#ifdef __GI_SCANNER__
/* The introspection scanner is currently unable to lookup how
 * cairo_rectangle_int_t is actually defined. This prevents
 * introspection data for the GdkRectangle type to include fields
 * descriptions. To workaround this issue, we define it with the same
 * content as cairo_rectangle_int_t, but only under the introspection
 * define.
 */
struct _GdkRectangle
{
    int x, y;
    int width, height;
};
typedef struct _GdkRectangle          GdkRectangle;
#else
typedef cairo_rectangle_int_t         GdkRectangle;
#endif

/* Forward declarations of commonly used types */
typedef struct _GdkRGBA               GdkRGBA;
typedef struct _GdkContentFormats     GdkContentFormats;
typedef struct _GdkContentProvider    GdkContentProvider;
typedef struct _GdkCursor             GdkCursor;
typedef struct _GdkTexture            GdkTexture;
typedef struct _GdkDevice             GdkDevice;
typedef struct _GdkDrag               GdkDrag;
typedef struct _GdkDrop               GdkDrop;

typedef struct _GdkClipboard          GdkClipboard;
typedef struct _GdkDisplayManager     GdkDisplayManager;
typedef struct _GdkDisplay            GdkDisplay;
typedef struct _GdkSurface             GdkSurface;
typedef struct _GdkAppLaunchContext   GdkAppLaunchContext;
typedef struct _GdkSeat               GdkSeat;
typedef struct _GdkSnapshot           GdkSnapshot;

typedef struct _GdkGeometry           GdkGeometry;

typedef struct _GdkDrawingContext     GdkDrawingContext;
typedef struct _GdkDrawContext        GdkDrawContext;
typedef struct _GdkCairoContext       GdkCairoContext;
typedef struct _GdkGLContext          GdkGLContext;
typedef struct _GdkVulkanContext      GdkVulkanContext;

/* Currently, these are the same values numerically as in the
 * X protocol. If you change that, gdksurface-x11.c/gdk_surface_set_geometry_hints()
 * will need fixing.
 */
/**
 * GdkGravity:
 * @GDK_GRAVITY_NORTH_WEST: the reference point is at the top left corner.
 * @GDK_GRAVITY_NORTH: the reference point is in the middle of the top edge.
 * @GDK_GRAVITY_NORTH_EAST: the reference point is at the top right corner.
 * @GDK_GRAVITY_WEST: the reference point is at the middle of the left edge.
 * @GDK_GRAVITY_CENTER: the reference point is at the center of the surface.
 * @GDK_GRAVITY_EAST: the reference point is at the middle of the right edge.
 * @GDK_GRAVITY_SOUTH_WEST: the reference point is at the lower left corner.
 * @GDK_GRAVITY_SOUTH: the reference point is at the middle of the lower edge.
 * @GDK_GRAVITY_SOUTH_EAST: the reference point is at the lower right corner.
 * @GDK_GRAVITY_STATIC: the reference point is at the top left corner of the
 *  surface itself, ignoring window manager decorations.
 *
 * Defines the reference point of a surface and the meaning of coordinates
 * passed to gtk_window_move(). See gtk_window_move() and the "implementation
 * notes" section of the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec)
 * specification for more details.
 */
typedef enum
{
  GDK_GRAVITY_NORTH_WEST = 1,
  GDK_GRAVITY_NORTH,
  GDK_GRAVITY_NORTH_EAST,
  GDK_GRAVITY_WEST,
  GDK_GRAVITY_CENTER,
  GDK_GRAVITY_EAST,
  GDK_GRAVITY_SOUTH_WEST,
  GDK_GRAVITY_SOUTH,
  GDK_GRAVITY_SOUTH_EAST,
  GDK_GRAVITY_STATIC
} GdkGravity;

/**
 * GdkByteOrder:
 * @GDK_LSB_FIRST: The values are stored with the least-significant byte
 *   first. For instance, the 32-bit value 0xffeecc would be stored
 *   in memory as 0xcc, 0xee, 0xff, 0x00.
 * @GDK_MSB_FIRST: The values are stored with the most-significant byte
 *   first. For instance, the 32-bit value 0xffeecc would be stored
 *   in memory as 0x00, 0xff, 0xee, 0xcc.
 *
 * A set of values describing the possible byte-orders
 * for storing pixel values in memory.
 */
typedef enum
{
  GDK_LSB_FIRST,
  GDK_MSB_FIRST
} GdkByteOrder;

/* Types of modifiers.
 */
/**
 * GdkModifierType:
 * @GDK_SHIFT_MASK: the Shift key.
 * @GDK_LOCK_MASK: a Lock key (depending on the modifier mapping of the
 *  X server this may either be CapsLock or ShiftLock).
 * @GDK_CONTROL_MASK: the Control key.
 * @GDK_ALT_MASK: the fourth modifier key (it depends on the modifier
 *  mapping of the X server which key is interpreted as this modifier, but
 *  normally it is the Alt key).
 * @GDK_BUTTON1_MASK: the first mouse button.
 * @GDK_BUTTON2_MASK: the second mouse button.
 * @GDK_BUTTON3_MASK: the third mouse button.
 * @GDK_BUTTON4_MASK: the fourth mouse button.
 * @GDK_BUTTON5_MASK: the fifth mouse button.
 * @GDK_SUPER_MASK: the Super modifier
 * @GDK_HYPER_MASK: the Hyper modifier
 * @GDK_META_MASK: the Meta modifier
 *
 * A set of bit-flags to indicate the state of modifier keys and mouse buttons
 * in various event types. Typical modifier keys are Shift, Control, Meta,
 * Super, Hyper, Alt, Compose, Apple, CapsLock or ShiftLock.
 *
 * Note that GDK may add internal values to events which include values outside
 * of this enumeration. Your code should preserve and ignore them.  You can use
 * %GDK_MODIFIER_MASK to remove all private values.
 */
typedef enum
{
  GDK_SHIFT_MASK    = 1 << 0,
  GDK_LOCK_MASK     = 1 << 1,
  GDK_CONTROL_MASK  = 1 << 2,
  GDK_ALT_MASK      = 1 << 3,

  GDK_BUTTON1_MASK  = 1 << 8,
  GDK_BUTTON2_MASK  = 1 << 9,
  GDK_BUTTON3_MASK  = 1 << 10,
  GDK_BUTTON4_MASK  = 1 << 11,
  GDK_BUTTON5_MASK  = 1 << 12,

  GDK_SUPER_MASK    = 1 << 26,
  GDK_HYPER_MASK    = 1 << 27,
  GDK_META_MASK     = 1 << 28,
} GdkModifierType;


/**
 * GDK_MODIFIER_MASK:
 *
 * A mask covering all entries in #GdkModifierType.
 */
#define GDK_MODIFIER_MASK (GDK_SHIFT_MASK|GDK_LOCK_MASK|GDK_CONTROL_MASK| \
                           GDK_ALT_MASK|GDK_SUPER_MASK|GDK_HYPER_MASK|GDK_META_MASK| \
                           GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK| \
                           GDK_BUTTON4_MASK|GDK_BUTTON5_MASK)

/**
 * GdkEventMask:
 * @GDK_EXPOSURE_MASK: receive expose events
 * @GDK_POINTER_MOTION_MASK: receive all pointer motion events
 * @GDK_BUTTON_MOTION_MASK: receive pointer motion events while any button is pressed
 * @GDK_BUTTON1_MOTION_MASK: receive pointer motion events while 1 button is pressed
 * @GDK_BUTTON2_MOTION_MASK: receive pointer motion events while 2 button is pressed
 * @GDK_BUTTON3_MOTION_MASK: receive pointer motion events while 3 button is pressed
 * @GDK_BUTTON_PRESS_MASK: receive button press events
 * @GDK_BUTTON_RELEASE_MASK: receive button release events
 * @GDK_KEY_PRESS_MASK: receive key press events
 * @GDK_KEY_RELEASE_MASK: receive key release events
 * @GDK_ENTER_NOTIFY_MASK: receive surface enter events
 * @GDK_LEAVE_NOTIFY_MASK: receive surface leave events
 * @GDK_FOCUS_CHANGE_MASK: receive focus change events
 * @GDK_STRUCTURE_MASK: receive events about surface configuration change
 * @GDK_PROPERTY_CHANGE_MASK: receive property change events
 * @GDK_PROXIMITY_IN_MASK: receive proximity in events
 * @GDK_PROXIMITY_OUT_MASK: receive proximity out events
 * @GDK_SUBSTRUCTURE_MASK: receive events about surface configuration changes of child surfaces
 * @GDK_SCROLL_MASK: receive scroll events
 * @GDK_TOUCH_MASK: receive touch events
 * @GDK_SMOOTH_SCROLL_MASK: receive smooth scrolling events
   @GDK_TOUCHPAD_GESTURE_MASK: receive touchpad gesture events
 * @GDK_TABLET_PAD_MASK: receive tablet pad events
 * @GDK_ALL_EVENTS_MASK: the combination of all the above event masks.
 *
 * A set of bit-flags to indicate which events a surface is to receive.
 * Most of these masks map onto one or more of the #GdkEventType event types
 * above.
 *
 * See the [input handling overview][chap-input-handling] for details of
 * [event masks][event-masks] and [event propagation][event-propagation].
 *
 * If %GDK_TOUCH_MASK is enabled, the surface will receive touch events
 * from touch-enabled devices. Those will come as sequences of #GdkEventTouch
 * with type %GDK_TOUCH_UPDATE, enclosed by two events with
 * type %GDK_TOUCH_BEGIN and %GDK_TOUCH_END (or %GDK_TOUCH_CANCEL).
 * gdk_event_get_event_sequence() returns the event sequence for these
 * events, so different sequences may be distinguished.
 */
typedef enum
{
  GDK_EXPOSURE_MASK             = 1 << 1,
  GDK_POINTER_MOTION_MASK       = 1 << 2,
  GDK_BUTTON_MOTION_MASK        = 1 << 4,
  GDK_BUTTON1_MOTION_MASK       = 1 << 5,
  GDK_BUTTON2_MOTION_MASK       = 1 << 6,
  GDK_BUTTON3_MOTION_MASK       = 1 << 7,
  GDK_BUTTON_PRESS_MASK         = 1 << 8,
  GDK_BUTTON_RELEASE_MASK       = 1 << 9,
  GDK_KEY_PRESS_MASK            = 1 << 10,
  GDK_KEY_RELEASE_MASK          = 1 << 11,
  GDK_ENTER_NOTIFY_MASK         = 1 << 12,
  GDK_LEAVE_NOTIFY_MASK         = 1 << 13,
  GDK_FOCUS_CHANGE_MASK         = 1 << 14,
  GDK_STRUCTURE_MASK            = 1 << 15,
  GDK_PROPERTY_CHANGE_MASK      = 1 << 16,
  GDK_PROXIMITY_IN_MASK         = 1 << 18,
  GDK_PROXIMITY_OUT_MASK        = 1 << 19,
  GDK_SUBSTRUCTURE_MASK         = 1 << 20,
  GDK_SCROLL_MASK               = 1 << 21,
  GDK_TOUCH_MASK                = 1 << 22,
  GDK_SMOOTH_SCROLL_MASK        = 1 << 23,
  GDK_TOUCHPAD_GESTURE_MASK     = 1 << 24,
  GDK_TABLET_PAD_MASK           = 1 << 25,
  GDK_ALL_EVENTS_MASK           = 0x3FFFFFE
} GdkEventMask;

/**
 * GdkGLError:
 * @GDK_GL_ERROR_NOT_AVAILABLE: OpenGL support is not available
 * @GDK_GL_ERROR_UNSUPPORTED_FORMAT: The requested visual format is not supported
 * @GDK_GL_ERROR_UNSUPPORTED_PROFILE: The requested profile is not supported
 * @GDK_GL_ERROR_COMPILATION_FAILED: The shader compilation failed
 * @GDK_GL_ERROR_LINK_FAILED: The shader linking failed
 *
 * Error enumeration for #GdkGLContext.
 */
typedef enum {
  GDK_GL_ERROR_NOT_AVAILABLE,
  GDK_GL_ERROR_UNSUPPORTED_FORMAT,
  GDK_GL_ERROR_UNSUPPORTED_PROFILE,
  GDK_GL_ERROR_COMPILATION_FAILED,
  GDK_GL_ERROR_LINK_FAILED
} GdkGLError;

/**
 * GdkVulkanError:
 * @GDK_VULKAN_ERROR_UNSUPPORTED: Vulkan is not supported on this backend or has not been
 *     compiled in.
 * @GDK_VULKAN_ERROR_NOT_AVAILABLE: Vulkan support is not available on this Surface
 *
 * Error enumeration for #GdkVulkanContext.
 */
typedef enum {
  GDK_VULKAN_ERROR_UNSUPPORTED,
  GDK_VULKAN_ERROR_NOT_AVAILABLE,
} GdkVulkanError;

/**
 * GdkSurfaceTypeHint:
 * @GDK_SURFACE_TYPE_HINT_NORMAL: Normal toplevel window.
 * @GDK_SURFACE_TYPE_HINT_DIALOG: Dialog window.
 * @GDK_SURFACE_TYPE_HINT_MENU: Window used to implement a menu; GTK uses
 *  this hint only for torn-off menus, see #GtkTearoffMenuItem.
 * @GDK_SURFACE_TYPE_HINT_TOOLBAR: Window used to implement toolbars.
 * @GDK_SURFACE_TYPE_HINT_SPLASHSCREEN: Window used to display a splash
 *  screen during application startup.
 * @GDK_SURFACE_TYPE_HINT_UTILITY: Utility windows which are not detached
 *  toolbars or dialogs.
 * @GDK_SURFACE_TYPE_HINT_DOCK: Used for creating dock or panel windows.
 * @GDK_SURFACE_TYPE_HINT_DESKTOP: Used for creating the desktop background
 *  window.
 * @GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU: A menu that belongs to a menubar.
 * @GDK_SURFACE_TYPE_HINT_POPUP_MENU: A menu that does not belong to a menubar,
 *  e.g. a context menu.
 * @GDK_SURFACE_TYPE_HINT_TOOLTIP: A tooltip.
 * @GDK_SURFACE_TYPE_HINT_NOTIFICATION: A notification - typically a “bubble”
 *  that belongs to a status icon.
 * @GDK_SURFACE_TYPE_HINT_COMBO: A popup from a combo box.
 * @GDK_SURFACE_TYPE_HINT_DND: A window that is used to implement a DND cursor.
 *
 * These are hints for the window manager that indicate what type of function
 * the window has. The window manager can use this when determining decoration
 * and behaviour of the window. The hint must be set before mapping the window.
 *
 * See the [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec)
 * specification for more details about window types.
 */
typedef enum
{
  GDK_SURFACE_TYPE_HINT_NORMAL,
  GDK_SURFACE_TYPE_HINT_DIALOG,
  GDK_SURFACE_TYPE_HINT_MENU,		/* Torn off menu */
  GDK_SURFACE_TYPE_HINT_TOOLBAR,
  GDK_SURFACE_TYPE_HINT_SPLASHSCREEN,
  GDK_SURFACE_TYPE_HINT_UTILITY,
  GDK_SURFACE_TYPE_HINT_DOCK,
  GDK_SURFACE_TYPE_HINT_DESKTOP,
  GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU,	/* A drop down menu (from a menubar) */
  GDK_SURFACE_TYPE_HINT_POPUP_MENU,	/* A popup menu (from right-click) */
  GDK_SURFACE_TYPE_HINT_TOOLTIP,
  GDK_SURFACE_TYPE_HINT_NOTIFICATION,
  GDK_SURFACE_TYPE_HINT_COMBO,
  GDK_SURFACE_TYPE_HINT_DND
} GdkSurfaceTypeHint;

/**
 * GdkAxisUse:
 * @GDK_AXIS_IGNORE: the axis is ignored.
 * @GDK_AXIS_X: the axis is used as the x axis.
 * @GDK_AXIS_Y: the axis is used as the y axis.
 * @GDK_AXIS_PRESSURE: the axis is used for pressure information.
 * @GDK_AXIS_XTILT: the axis is used for x tilt information.
 * @GDK_AXIS_YTILT: the axis is used for y tilt information.
 * @GDK_AXIS_WHEEL: the axis is used for wheel information.
 * @GDK_AXIS_DISTANCE: the axis is used for pen/tablet distance information
 * @GDK_AXIS_ROTATION: the axis is used for pen rotation information
 * @GDK_AXIS_SLIDER: the axis is used for pen slider information
 * @GDK_AXIS_LAST: a constant equal to the numerically highest axis value.
 *
 * An enumeration describing the way in which a device
 * axis (valuator) maps onto the predefined valuator
 * types that GTK understands.
 *
 * Note that the X and Y axes are not really needed; pointer devices
 * report their location via the x/y members of events regardless. Whether
 * X and Y are present as axes depends on the GDK backend.
 */
typedef enum
{
  GDK_AXIS_IGNORE,
  GDK_AXIS_X,
  GDK_AXIS_Y,
  GDK_AXIS_PRESSURE,
  GDK_AXIS_XTILT,
  GDK_AXIS_YTILT,
  GDK_AXIS_WHEEL,
  GDK_AXIS_DISTANCE,
  GDK_AXIS_ROTATION,
  GDK_AXIS_SLIDER,
  GDK_AXIS_LAST
} GdkAxisUse;

/**
 * GdkAxisFlags:
 * @GDK_AXIS_FLAG_X: X axis is present
 * @GDK_AXIS_FLAG_Y: Y axis is present
 * @GDK_AXIS_FLAG_PRESSURE: Pressure axis is present
 * @GDK_AXIS_FLAG_XTILT: X tilt axis is present
 * @GDK_AXIS_FLAG_YTILT: Y tilt axis is present
 * @GDK_AXIS_FLAG_WHEEL: Wheel axis is present
 * @GDK_AXIS_FLAG_DISTANCE: Distance axis is present
 * @GDK_AXIS_FLAG_ROTATION: Z-axis rotation is present
 * @GDK_AXIS_FLAG_SLIDER: Slider axis is present
 *
 * Flags describing the current capabilities of a device/tool.
 */
typedef enum
{
  GDK_AXIS_FLAG_X        = 1 << GDK_AXIS_X,
  GDK_AXIS_FLAG_Y        = 1 << GDK_AXIS_Y,
  GDK_AXIS_FLAG_PRESSURE = 1 << GDK_AXIS_PRESSURE,
  GDK_AXIS_FLAG_XTILT    = 1 << GDK_AXIS_XTILT,
  GDK_AXIS_FLAG_YTILT    = 1 << GDK_AXIS_YTILT,
  GDK_AXIS_FLAG_WHEEL    = 1 << GDK_AXIS_WHEEL,
  GDK_AXIS_FLAG_DISTANCE = 1 << GDK_AXIS_DISTANCE,
  GDK_AXIS_FLAG_ROTATION = 1 << GDK_AXIS_ROTATION,
  GDK_AXIS_FLAG_SLIDER   = 1 << GDK_AXIS_SLIDER,
} GdkAxisFlags;

/**
 * GdkDragAction:
 * @GDK_ACTION_COPY: Copy the data.
 * @GDK_ACTION_MOVE: Move the data, i.e. first copy it, then delete
 *  it from the source using the DELETE target of the X selection protocol.
 * @GDK_ACTION_LINK: Add a link to the data. Note that this is only
 *  useful if source and destination agree on what it means, and is not
 *  supported on all platforms.
 * @GDK_ACTION_ASK: Ask the user what to do with the data.
 *
 * Used in #GdkDrop and #GdkDrag to indicate the actions that the
 * destination can and should do with the dropped data.
 */
typedef enum
{
  GDK_ACTION_COPY    = 1 << 0,
  GDK_ACTION_MOVE    = 1 << 1,
  GDK_ACTION_LINK    = 1 << 2,
  GDK_ACTION_ASK     = 1 << 3
} GdkDragAction;

/**
 * GDK_ACTION_ALL:
 *
 * Defines all possible DND actions. This can be used in gdk_drop_status()
 * messages when any drop can be accepted or a more specific drop method
 * is not yet known.
 */
#define GDK_ACTION_ALL (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)

/*
 * GDK_DECLARE_INTERNAL_TYPE:
 * @ModuleObjName: The name of the new type, in camel case (like GtkWidget)
 * @module_obj_name: The name of the new type in lowercase, with words
 *  separated by '_' (like 'gtk_widget')
 * @MODULE: The name of the module, in all caps (like 'GTK')
 * @OBJ_NAME: The bare name of the type, in all caps (like 'WIDGET')
 * @ParentName: the name of the parent type, in camel case (like GtkWidget)
 *
 * A convenience macro for emitting the usual declarations in the
 * header file for a type which is intended to be subclassed only
 * by internal consumers.
 *
 * This macro differs from %G_DECLARE_DERIVABLE_TYPE and %G_DECLARE_FINAL_TYPE
 * by declaring a type that is only derivable internally. Internal users can
 * derive this type, assuming they have access to the instance and class
 * structures; external users will not be able to subclass this type.
 */
#define GDK_DECLARE_INTERNAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName) \
  GType module_obj_name##_get_type (void);                                                               \
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                       \
  typedef struct _##ModuleObjName ModuleObjName;                                                         \
  typedef struct _##ModuleObjName##Class ModuleObjName##Class;                                           \
                                                                                                         \
  _GLIB_DEFINE_AUTOPTR_CHAINUP (ModuleObjName, ParentName)                                               \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (ModuleObjName##Class, g_type_class_unref)                               \
                                                                                                         \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) {                       \
    return G_TYPE_CHECK_INSTANCE_CAST (ptr, module_obj_name##_get_type (), ModuleObjName); }             \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_CLASS (gpointer ptr) {        \
    return G_TYPE_CHECK_CLASS_CAST (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }         \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) {                           \
    return G_TYPE_CHECK_INSTANCE_TYPE (ptr, module_obj_name##_get_type ()); }                            \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME##_CLASS (gpointer ptr) {                   \
    return G_TYPE_CHECK_CLASS_TYPE (ptr, module_obj_name##_get_type ()); }                               \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) {    \
    return G_TYPE_INSTANCE_GET_CLASS (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }       \
  G_GNUC_END_IGNORE_DEPRECATIONS

typedef struct _GdkKeymapKey GdkKeymapKey;

/**
 * GdkKeymapKey:
 * @keycode: the hardware keycode. This is an identifying number for a
 *   physical key.
 * @group: indicates movement in a horizontal direction. Usually groups are used
 *   for two different languages. In group 0, a key might have two English
 *   characters, and in group 1 it might have two Hebrew characters. The Hebrew
 *   characters will be printed on the key next to the English characters.
 * @level: indicates which symbol on the key will be used, in a vertical direction.
 *   So on a standard US keyboard, the key with the number “1” on it also has the
 *   exclamation point ("!") character on it. The level indicates whether to use
 *   the “1” or the “!” symbol. The letter keys are considered to have a lowercase
 *   letter at level 0, and an uppercase letter at level 1, though only the
 *   uppercase letter is printed.
 *
 * A #GdkKeymapKey is a hardware key that can be mapped to a keyval.
 */
struct _GdkKeymapKey
{
  guint keycode;
  gint  group;
  gint  level;
};


G_END_DECLS

/*< private >
 * GDK_EXTERN_VAR:
 *
 * A macro to annotate extern variables so that they show up properly in
 * Windows DLLs.
 */
#ifndef GDK_EXTERN_VAR
#  ifdef G_PLATFORM_WIN32
#    ifdef GTK_COMPILATION
#      ifdef DLL_EXPORT
#        define GDK_EXTERN_VAR __declspec(dllexport)
#      else /* !DLL_EXPORT */
#        define GDK_EXTERN_VAR extern
#      endif /* !DLL_EXPORT */
#    else /* !GTK_COMPILATION */
#      define GDK_EXTERN_VAR extern __declspec(dllimport)
#    endif /* !GTK_COMPILATION */
#  else /* !G_PLATFORM_WIN32 */
#    define GDK_EXTERN_VAR _GDK_EXTERN
#  endif /* !G_PLATFORM_WIN32 */
#endif /* GDK_EXTERN_VAR */

#endif /* __GDK_TYPES_H__ */
