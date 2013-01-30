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

#ifndef __GDK_WINDOW_H__
#define __GDK_WINDOW_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkframeclock.h>

G_BEGIN_DECLS

typedef struct _GdkGeometry          GdkGeometry;
typedef struct _GdkWindowAttr        GdkWindowAttr;
typedef struct _GdkWindowRedirect    GdkWindowRedirect;

/**
 * GdkWindowWindowClass:
 * @GDK_INPUT_OUTPUT: window for graphics and events
 * @GDK_INPUT_ONLY: window for events only
 *
 * @GDK_INPUT_OUTPUT windows are the standard kind of window you might expect.
 * Such windows receive events and are also displayed on screen.
 * @GDK_INPUT_ONLY windows are invisible; they are usually placed above other
 * windows in order to trap or filter the events. You can't draw on
 * @GDK_INPUT_ONLY windows.
 */
typedef enum
{
  GDK_INPUT_OUTPUT, /*< nick=input-output >*/
  GDK_INPUT_ONLY    /*< nick=input-only >*/
} GdkWindowWindowClass;

/**
 * GdkWindowType:
 * @GDK_WINDOW_ROOT: root window; this window has no parent, covers the entire
 *  screen, and is created by the window system
 * @GDK_WINDOW_TOPLEVEL: toplevel window (used to implement #GtkWindow)
 * @GDK_WINDOW_CHILD: child window (used to implement e.g. #GtkEntry)
 * @GDK_WINDOW_TEMP: override redirect temporary window (used to implement
 *  #GtkMenu)
 * @GDK_WINDOW_FOREIGN: foreign window (see gdk_window_foreign_new())
 * @GDK_WINDOW_OFFSCREEN: offscreen window (see
 *  <xref linkend="OFFSCREEN-WINDOWS"/>). Since 2.18
 *
 * Describes the kind of window.
 */
typedef enum
{
  GDK_WINDOW_ROOT,
  GDK_WINDOW_TOPLEVEL,
  GDK_WINDOW_CHILD,
  GDK_WINDOW_TEMP,
  GDK_WINDOW_FOREIGN,
  GDK_WINDOW_OFFSCREEN
} GdkWindowType;

/**
 * GdkWindowAttributesType:
 * @GDK_WA_TITLE: Honor the title field
 * @GDK_WA_X: Honor the X coordinate field
 * @GDK_WA_Y: Honor the Y coordinate field
 * @GDK_WA_CURSOR: Honor the cursor field
 * @GDK_WA_VISUAL: Honor the visual field
 * @GDK_WA_WMCLASS: Honor the wmclass_class and wmclass_name fields
 * @GDK_WA_NOREDIR: Honor the override_redirect field
 * @GDK_WA_TYPE_HINT: Honor the type_hint field
 *
 * Used to indicate which fields in the #GdkWindowAttr struct should be honored.
 * For example, if you filled in the "cursor" and "x" fields of #GdkWindowAttr,
 * pass "@GDK_WA_X | @GDK_WA_CURSOR" to gdk_window_new(). Fields in
 * #GdkWindowAttr not covered by a bit in this enum are required; for example,
 * the @width/@height, @wclass, and @window_type fields are required, they have
 * no corresponding flag in #GdkWindowAttributesType.
 */
typedef enum
{
  GDK_WA_TITLE	   = 1 << 1,
  GDK_WA_X	   = 1 << 2,
  GDK_WA_Y	   = 1 << 3,
  GDK_WA_CURSOR	   = 1 << 4,
  GDK_WA_VISUAL	   = 1 << 5,
  GDK_WA_WMCLASS   = 1 << 6,
  GDK_WA_NOREDIR   = 1 << 7,
  GDK_WA_TYPE_HINT = 1 << 8
} GdkWindowAttributesType;

/* Size restriction enumeration.
 */
/**
 * GdkWindowHints:
 * @GDK_HINT_POS: indicates that the program has positioned the window
 * @GDK_HINT_MIN_SIZE: min size fields are set
 * @GDK_HINT_MAX_SIZE: max size fields are set
 * @GDK_HINT_BASE_SIZE: base size fields are set
 * @GDK_HINT_ASPECT: aspect ratio fields are set
 * @GDK_HINT_RESIZE_INC: resize increment fields are set
 * @GDK_HINT_WIN_GRAVITY: window gravity field is set
 * @GDK_HINT_USER_POS: indicates that the window's position was explicitly set
 *  by the user
 * @GDK_HINT_USER_SIZE: indicates that the window's size was explicitly set by
 *  the user
 *
 * Used to indicate which fields of a #GdkGeometry struct should be paid
 * attention to. Also, the presence/absence of @GDK_HINT_POS,
 * @GDK_HINT_USER_POS, and @GDK_HINT_USER_SIZE is significant, though they don't
 * directly refer to #GdkGeometry fields. @GDK_HINT_USER_POS will be set
 * automatically by #GtkWindow if you call gtk_window_move().
 * @GDK_HINT_USER_POS and @GDK_HINT_USER_SIZE should be set if the user
 * specified a size/position using a --geometry command-line argument;
 * gtk_window_parse_geometry() automatically sets these flags.
 */
typedef enum
{
  GDK_HINT_POS	       = 1 << 0,
  GDK_HINT_MIN_SIZE    = 1 << 1,
  GDK_HINT_MAX_SIZE    = 1 << 2,
  GDK_HINT_BASE_SIZE   = 1 << 3,
  GDK_HINT_ASPECT      = 1 << 4,
  GDK_HINT_RESIZE_INC  = 1 << 5,
  GDK_HINT_WIN_GRAVITY = 1 << 6,
  GDK_HINT_USER_POS    = 1 << 7,
  GDK_HINT_USER_SIZE   = 1 << 8
} GdkWindowHints;


/**
 * GdkWindowTypeHint:
 * @GDK_WINDOW_TYPE_HINT_NORMAL: Normal toplevel window.
 * @GDK_WINDOW_TYPE_HINT_DIALOG: Dialog window.
 * @GDK_WINDOW_TYPE_HINT_MENU: Window used to implement a menu; GTK+ uses
 *  this hint only for torn-off menus, see #GtkTearoffMenuItem.
 * @GDK_WINDOW_TYPE_HINT_TOOLBAR: Window used to implement toolbars.
 * @GDK_WINDOW_TYPE_HINT_SPLASHSCREEN: Window used to display a splash
 *  screen during application startup.
 * @GDK_WINDOW_TYPE_HINT_UTILITY: Utility windows which are not detached
 *  toolbars or dialogs.
 * @GDK_WINDOW_TYPE_HINT_DOCK: Used for creating dock or panel windows.
 * @GDK_WINDOW_TYPE_HINT_DESKTOP: Used for creating the desktop background
 *  window.
 * @GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: A menu that belongs to a menubar.
 * @GDK_WINDOW_TYPE_HINT_POPUP_MENU: A menu that does not belong to a menubar,
 *  e.g. a context menu.
 * @GDK_WINDOW_TYPE_HINT_TOOLTIP: A tooltip.
 * @GDK_WINDOW_TYPE_HINT_NOTIFICATION: A notification - typically a "bubble"
 *  that belongs to a status icon.
 * @GDK_WINDOW_TYPE_HINT_COMBO: A popup from a combo box.
 * @GDK_WINDOW_TYPE_HINT_DND: A window that is used to implement a DND cursor.
 *
 * These are hints for the window manager that indicate what type of function
 * the window has. The window manager can use this when determining decoration
 * and behaviour of the window. The hint must be set before mapping the window.
 *
 * See the
 * <ulink url="http://www.freedesktop.org/Standards/wm-spec">Extended
 * Window Manager Hints</ulink> specification for more details about
 * window types.
 */
typedef enum
{
  GDK_WINDOW_TYPE_HINT_NORMAL,
  GDK_WINDOW_TYPE_HINT_DIALOG,
  GDK_WINDOW_TYPE_HINT_MENU,		/* Torn off menu */
  GDK_WINDOW_TYPE_HINT_TOOLBAR,
  GDK_WINDOW_TYPE_HINT_SPLASHSCREEN,
  GDK_WINDOW_TYPE_HINT_UTILITY,
  GDK_WINDOW_TYPE_HINT_DOCK,
  GDK_WINDOW_TYPE_HINT_DESKTOP,
  GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU,	/* A drop down menu (from a menubar) */
  GDK_WINDOW_TYPE_HINT_POPUP_MENU,	/* A popup menu (from right-click) */
  GDK_WINDOW_TYPE_HINT_TOOLTIP,
  GDK_WINDOW_TYPE_HINT_NOTIFICATION,
  GDK_WINDOW_TYPE_HINT_COMBO,
  GDK_WINDOW_TYPE_HINT_DND
} GdkWindowTypeHint;

/* The next two enumeration values current match the
 * Motif constants. If this is changed, the implementation
 * of gdk_window_set_decorations/gdk_window_set_functions
 * will need to change as well.
 */
/**
 * GdkWMDecoration:
 * @GDK_DECOR_ALL: all decorations should be applied.
 * @GDK_DECOR_BORDER: a frame should be drawn around the window.
 * @GDK_DECOR_RESIZEH: the frame should have resize handles.
 * @GDK_DECOR_TITLE: a titlebar should be placed above the window.
 * @GDK_DECOR_MENU: a button for opening a menu should be included.
 * @GDK_DECOR_MINIMIZE: a minimize button should be included.
 * @GDK_DECOR_MAXIMIZE: a maximize button should be included.
 *
 * These are hints originally defined by the Motif toolkit.
 * The window manager can use them when determining how to decorate
 * the window. The hint must be set before mapping the window.
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

/**
 * GdkWMFunction:
 * @GDK_FUNC_ALL: all functions should be offered.
 * @GDK_FUNC_RESIZE: the window should be resizable.
 * @GDK_FUNC_MOVE: the window should be movable.
 * @GDK_FUNC_MINIMIZE: the window should be minimizable.
 * @GDK_FUNC_MAXIMIZE: the window should be maximizable.
 * @GDK_FUNC_CLOSE: the window should be closable.
 *
 * These are hints originally defined by the Motif toolkit. The window manager
 * can use them when determining the functions to offer for the window. The
 * hint must be set before mapping the window.
 */
typedef enum
{
  GDK_FUNC_ALL		= 1 << 0,
  GDK_FUNC_RESIZE	= 1 << 1,
  GDK_FUNC_MOVE		= 1 << 2,
  GDK_FUNC_MINIMIZE	= 1 << 3,
  GDK_FUNC_MAXIMIZE	= 1 << 4,
  GDK_FUNC_CLOSE	= 1 << 5
} GdkWMFunction;

/* Currently, these are the same values numerically as in the
 * X protocol. If you change that, gdkwindow-x11.c/gdk_window_set_geometry_hints()
 * will need fixing.
 */
/**
 * GdkGravity:
 * @GDK_GRAVITY_NORTH_WEST: the reference point is at the top left corner.
 * @GDK_GRAVITY_NORTH: the reference point is in the middle of the top edge.
 * @GDK_GRAVITY_NORTH_EAST: the reference point is at the top right corner.
 * @GDK_GRAVITY_WEST: the reference point is at the middle of the left edge.
 * @GDK_GRAVITY_CENTER: the reference point is at the center of the window.
 * @GDK_GRAVITY_EAST: the reference point is at the middle of the right edge.
 * @GDK_GRAVITY_SOUTH_WEST: the reference point is at the lower left corner.
 * @GDK_GRAVITY_SOUTH: the reference point is at the middle of the lower edge.
 * @GDK_GRAVITY_SOUTH_EAST: the reference point is at the lower right corner.
 * @GDK_GRAVITY_STATIC: the reference point is at the top left corner of the
 *  window itself, ignoring window manager decorations.
 *
 * Defines the reference point of a window and the meaning of coordinates
 * passed to gtk_window_move(). See gtk_window_move() and the "implementation
 * notes" section of the
 * <ulink url="http://www.freedesktop.org/Standards/wm-spec">Extended
 * Window Manager Hints</ulink> specification for more details.
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
 * GdkWindowEdge:
 * @GDK_WINDOW_EDGE_NORTH_WEST: the top left corner.
 * @GDK_WINDOW_EDGE_NORTH: the top edge.
 * @GDK_WINDOW_EDGE_NORTH_EAST: the top right corner.
 * @GDK_WINDOW_EDGE_WEST: the left edge.
 * @GDK_WINDOW_EDGE_EAST: the right edge.
 * @GDK_WINDOW_EDGE_SOUTH_WEST: the lower left corner.
 * @GDK_WINDOW_EDGE_SOUTH: the lower edge.
 * @GDK_WINDOW_EDGE_SOUTH_EAST: the lower right corner.
 *
 * Determines a window edge or corner.
 */
typedef enum
{
  GDK_WINDOW_EDGE_NORTH_WEST,
  GDK_WINDOW_EDGE_NORTH,
  GDK_WINDOW_EDGE_NORTH_EAST,
  GDK_WINDOW_EDGE_WEST,
  GDK_WINDOW_EDGE_EAST,
  GDK_WINDOW_EDGE_SOUTH_WEST,
  GDK_WINDOW_EDGE_SOUTH,
  GDK_WINDOW_EDGE_SOUTH_EAST  
} GdkWindowEdge;

/**
 * GdkFullscreenMode:
 * @GDK_FULLSCREEN_ON_CURRENT_MONITOR: Fullscreen on current monitor only.
 * @GDK_FULLSCREEN_ON_ALL_MONITORS: Span across all monitors when fullscreen.
 *
 * Indicates which monitor (in a multi-head setup) a window should span over
 * when in fullscreen mode.
 *
 * Since: 3.8
 **/
typedef enum
{
  GDK_FULLSCREEN_ON_CURRENT_MONITOR,
  GDK_FULLSCREEN_ON_ALL_MONITORS
} GdkFullscreenMode;

/**
 * GdkWindowAttr:
 * @title: title of the window (for toplevel windows)
 * @event_mask: event mask (see gdk_window_set_events())
 * @x: X coordinate relative to parent window (see gdk_window_move())
 * @y: Y coordinate relative to parent window (see gdk_window_move())
 * @width: width of window
 * @height: height of window
 * @wclass: #GDK_INPUT_OUTPUT (normal window) or #GDK_INPUT_ONLY (invisible
 *  window that receives events)
 * @visual: #GdkVisual for window
 * @window_type: type of window
 * @cursor: cursor for the window (see gdk_window_set_cursor())
 * @wmclass_name: don't use (see gtk_window_set_wmclass())
 * @wmclass_class: don't use (see gtk_window_set_wmclass())
 * @override_redirect: %TRUE to bypass the window manager
 * @type_hint: a hint of the function of the window
 *
 * Attributes to use for a newly-created window.
 */
struct _GdkWindowAttr
{
  gchar *title;
  gint event_mask;
  gint x, y;
  gint width;
  gint height;
  GdkWindowWindowClass wclass;
  GdkVisual *visual;
  GdkWindowType window_type;
  GdkCursor *cursor;
  gchar *wmclass_name;
  gchar *wmclass_class;
  gboolean override_redirect;
  GdkWindowTypeHint type_hint;
};

/**
 * GdkGeometry:
 * @min_width: minimum width of window (or -1 to use requisition, with
 *  #GtkWindow only)
 * @min_height: minimum height of window (or -1 to use requisition, with
 *  #GtkWindow only)
 * @max_width: maximum width of window (or -1 to use requisition, with
 *  #GtkWindow only)
 * @max_height: maximum height of window (or -1 to use requisition, with
 *  #GtkWindow only)
 * @base_width: allowed window widths are @base_width + @width_inc * N where N
 *  is any integer (-1 allowed with #GtkWindow)
 * @base_height: allowed window widths are @base_height + @height_inc * N where
 *  N is any integer (-1 allowed with #GtkWindow)
 * @width_inc: width resize increment
 * @height_inc: height resize increment
 * @min_aspect: minimum width/height ratio
 * @max_aspect: maximum width/height ratio
 * @win_gravity: window gravity, see gtk_window_set_gravity()
 *
 * The #GdkGeometry struct gives the window manager information about
 * a window's geometry constraints. Normally you would set these on
 * the GTK+ level using gtk_window_set_geometry_hints(). #GtkWindow
 * then sets the hints on the #GdkWindow it creates.
 *
 * gdk_window_set_geometry_hints() expects the hints to be fully valid already
 * and simply passes them to the window manager; in contrast,
 * gtk_window_set_geometry_hints() performs some interpretation. For example,
 * #GtkWindow will apply the hints to the geometry widget instead of the
 * toplevel window, if you set a geometry widget. Also, the
 * @min_width/@min_height/@max_width/@max_height fields may be set to -1, and
 * #GtkWindow will substitute the size request of the window or geometry widget.
 * If the minimum size hint is not provided, #GtkWindow will use its requisition
 * as the minimum size. If the minimum size is provided and a geometry widget is
 * set, #GtkWindow will take the minimum size as the minimum size of the
 * geometry widget rather than the entire window. The base size is treated
 * similarly.
 *
 * The canonical use-case for gtk_window_set_geometry_hints() is to get a
 * terminal widget to resize properly. Here, the terminal text area should be
 * the geometry widget; #GtkWindow will then automatically set the base size to
 * the size of other widgets in the terminal window, such as the menubar and
 * scrollbar. Then, the @width_inc and @height_inc fields should be set to the
 * size of one character in the terminal. Finally, the base size should be set
 * to the size of one character. The net effect is that the minimum size of the
 * terminal will have a 1x1 character terminal area, and only terminal sizes on
 * the "character grid" will be allowed.
 *
 * Here's an example of how the terminal example would be implemented, assuming
 * a terminal area widget called "terminal" and a toplevel window "toplevel":
 *
 * <informalexample><programlisting><![CDATA[
 * 	GdkGeometry hints;
 *
 * 	hints.base_width = terminal->char_width;
 *         hints.base_height = terminal->char_height;
 *         hints.min_width = terminal->char_width;
 *         hints.min_height = terminal->char_height;
 *         hints.width_inc = terminal->char_width;
 *         hints.height_inc = terminal->char_height;
 *
 *  gtk_window_set_geometry_hints (GTK_WINDOW (toplevel),
 *                                 GTK_WIDGET (terminal),
 *                                 &hints,
 *                                 GDK_HINT_RESIZE_INC |
 *                                 GDK_HINT_MIN_SIZE |
 *                                 GDK_HINT_BASE_SIZE);
 * ]]></programlisting></informalexample>
 *
 * The other useful fields are the @min_aspect and @max_aspect fields; these
 * contain a width/height ratio as a floating point number. If a geometry widget
 * is set, the aspect applies to the geometry widget rather than the entire
 * window. The most common use of these hints is probably to set @min_aspect and
 * @max_aspect to the same value, thus forcing the window to keep a constant
 * aspect ratio.
 */
struct _GdkGeometry
{
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
  GdkGravity win_gravity;
};

typedef struct _GdkWindowClass GdkWindowClass;

#define GDK_TYPE_WINDOW              (gdk_window_get_type ())
#define GDK_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW, GdkWindow))
#define GDK_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW, GdkWindowClass))
#define GDK_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW))
#define GDK_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW))
#define GDK_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW, GdkWindowClass))


struct _GdkWindowClass
{
  GObjectClass      parent_class;

  GdkWindow       * (* pick_embedded_child) (GdkWindow *window,
                                             gdouble    x,
                                             gdouble    y);

  /*  the following 3 signals will only be emitted by offscreen windows */
  void              (* to_embedder)         (GdkWindow *window,
                                             gdouble    offscreen_x,
                                             gdouble    offscreen_y,
                                             gdouble   *embedder_x,
                                             gdouble   *embedder_y);
  void              (* from_embedder)       (GdkWindow *window,
                                             gdouble    embedder_x,
                                             gdouble    embedder_y,
                                             gdouble   *offscreen_x,
                                             gdouble   *offscreen_y);
  cairo_surface_t * (* create_surface)      (GdkWindow *window,
                                             gint       width,
                                             gint       height);

  /* Padding for future expansion */
  void (*_gdk_reserved1) (void);
  void (*_gdk_reserved2) (void);
  void (*_gdk_reserved3) (void);
  void (*_gdk_reserved4) (void);
  void (*_gdk_reserved5) (void);
  void (*_gdk_reserved6) (void);
  void (*_gdk_reserved7) (void);
  void (*_gdk_reserved8) (void);
};

/* Windows
 */
GType         gdk_window_get_type              (void) G_GNUC_CONST;
GdkWindow*    gdk_window_new                   (GdkWindow     *parent,
                                                GdkWindowAttr *attributes,
                                                gint           attributes_mask);
void          gdk_window_destroy               (GdkWindow     *window);
GdkWindowType gdk_window_get_window_type       (GdkWindow     *window);
gboolean      gdk_window_is_destroyed          (GdkWindow     *window);

GdkVisual *   gdk_window_get_visual            (GdkWindow     *window);
GdkScreen *   gdk_window_get_screen            (GdkWindow     *window);
GdkDisplay *  gdk_window_get_display           (GdkWindow     *window);
#ifndef GDK_MULTIDEVICE_SAFE
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_get_window_at_position)
GdkWindow*    gdk_window_at_pointer            (gint          *win_x,
                                                gint          *win_y);
#endif /* GDK_MULTIDEVICE_SAFE */
void          gdk_window_show                  (GdkWindow     *window);
void          gdk_window_hide                  (GdkWindow     *window);
void          gdk_window_withdraw              (GdkWindow     *window);
void          gdk_window_show_unraised         (GdkWindow     *window);
void          gdk_window_move                  (GdkWindow     *window,
                                                gint           x,
                                                gint           y);
void          gdk_window_resize                (GdkWindow     *window,
                                                gint           width,
                                                gint           height);
void          gdk_window_move_resize           (GdkWindow     *window,
                                                gint           x,
                                                gint           y,
                                                gint           width,
                                                gint           height);
void          gdk_window_reparent              (GdkWindow     *window,
                                                GdkWindow     *new_parent,
                                                gint           x,
                                                gint           y);
void          gdk_window_raise                 (GdkWindow     *window);
void          gdk_window_lower                 (GdkWindow     *window);
void          gdk_window_restack               (GdkWindow     *window,
						GdkWindow     *sibling,
						gboolean       above);
void          gdk_window_focus                 (GdkWindow     *window,
                                                guint32        timestamp);
void          gdk_window_set_user_data         (GdkWindow     *window,
                                                gpointer       user_data);
void          gdk_window_set_override_redirect (GdkWindow     *window,
                                                gboolean       override_redirect);
gboolean      gdk_window_get_accept_focus      (GdkWindow     *window);
void          gdk_window_set_accept_focus      (GdkWindow     *window,
					        gboolean       accept_focus);
gboolean      gdk_window_get_focus_on_map      (GdkWindow     *window);
void          gdk_window_set_focus_on_map      (GdkWindow     *window,
					        gboolean       focus_on_map);
void          gdk_window_add_filter            (GdkWindow     *window,
                                                GdkFilterFunc  function,
                                                gpointer       data);
void          gdk_window_remove_filter         (GdkWindow     *window,
                                                GdkFilterFunc  function,
                                                gpointer       data);
void          gdk_window_scroll                (GdkWindow     *window,
                                                gint           dx,
                                                gint           dy);
void	      gdk_window_move_region           (GdkWindow       *window,
						const cairo_region_t *region,
						gint             dx,
						gint             dy);
gboolean      gdk_window_ensure_native        (GdkWindow       *window);

/* 
 * This allows for making shaped (partially transparent) windows
 * - cool feature, needed for Drag and Drag for example.
 */
void gdk_window_shape_combine_region (GdkWindow	      *window,
                                      const cairo_region_t *shape_region,
                                      gint	       offset_x,
                                      gint	       offset_y);

/*
 * This routine allows you to quickly take the shapes of all the child windows
 * of a window and use their shapes as the shape mask for this window - useful
 * for container windows that dont want to look like a big box
 * 
 * - Raster
 */
void gdk_window_set_child_shapes (GdkWindow *window);

gboolean gdk_window_get_composited (GdkWindow *window);
void gdk_window_set_composited   (GdkWindow *window,
                                  gboolean   composited);

/*
 * This routine allows you to merge (ie ADD) child shapes to your
 * own window's shape keeping its current shape and ADDING the child
 * shapes to it.
 * 
 * - Raster
 */
void gdk_window_merge_child_shapes         (GdkWindow       *window);

void gdk_window_input_shape_combine_region (GdkWindow       *window,
                                            const cairo_region_t *shape_region,
                                            gint             offset_x,
                                            gint             offset_y);
void gdk_window_set_child_input_shapes     (GdkWindow       *window);
void gdk_window_merge_child_input_shapes   (GdkWindow       *window);


/*
 * Check if a window has been shown, and whether all its
 * parents up to a toplevel have been shown, respectively.
 * Note that a window that is_viewable below is not necessarily
 * viewable in the X sense.
 */
gboolean gdk_window_is_visible     (GdkWindow *window);
gboolean gdk_window_is_viewable    (GdkWindow *window);
gboolean gdk_window_is_input_only  (GdkWindow *window);
gboolean gdk_window_is_shaped      (GdkWindow *window);

GdkWindowState gdk_window_get_state (GdkWindow *window);

/* Set static bit gravity on the parent, and static
 * window gravity on all children.
 */
gboolean gdk_window_set_static_gravities (GdkWindow *window,
                                          gboolean   use_static);

/* GdkWindow */

gboolean      gdk_window_has_native         (GdkWindow       *window);
void              gdk_window_set_type_hint (GdkWindow        *window,
                                            GdkWindowTypeHint hint);
GdkWindowTypeHint gdk_window_get_type_hint (GdkWindow        *window);

gboolean      gdk_window_get_modal_hint   (GdkWindow       *window);
void          gdk_window_set_modal_hint   (GdkWindow       *window,
                                           gboolean         modal);

void gdk_window_set_skip_taskbar_hint (GdkWindow *window,
                                       gboolean   skips_taskbar);
void gdk_window_set_skip_pager_hint   (GdkWindow *window,
                                       gboolean   skips_pager);
void gdk_window_set_urgency_hint      (GdkWindow *window,
				       gboolean   urgent);

void          gdk_window_set_geometry_hints (GdkWindow          *window,
					     const GdkGeometry  *geometry,
					     GdkWindowHints      geom_mask);

cairo_region_t *gdk_window_get_clip_region  (GdkWindow          *window);
cairo_region_t *gdk_window_get_visible_region(GdkWindow         *window);


void	      gdk_window_begin_paint_rect   (GdkWindow          *window,
					     const GdkRectangle *rectangle);
void	      gdk_window_begin_paint_region (GdkWindow          *window,
					     const cairo_region_t    *region);
void	      gdk_window_end_paint          (GdkWindow          *window);
void	      gdk_window_flush             (GdkWindow          *window);

void	      gdk_window_set_title	   (GdkWindow	  *window,
					    const gchar	  *title);
void          gdk_window_set_role          (GdkWindow     *window,
					    const gchar   *role);
void          gdk_window_set_startup_id    (GdkWindow     *window,
					    const gchar   *startup_id);
void          gdk_window_set_transient_for (GdkWindow     *window,
					    GdkWindow     *parent);
GDK_DEPRECATED_IN_3_4_FOR(gdk_window_set_background_rgba)
void	      gdk_window_set_background	 (GdkWindow	  *window,
					  const GdkColor  *color);
void          gdk_window_set_background_rgba (GdkWindow     *window,
                                              const GdkRGBA *rgba);
void	      gdk_window_set_background_pattern (GdkWindow	 *window,
                                                 cairo_pattern_t *pattern);
cairo_pattern_t *gdk_window_get_background_pattern (GdkWindow     *window);

void	      gdk_window_set_cursor	 (GdkWindow	  *window,
					  GdkCursor	  *cursor);
GdkCursor    *gdk_window_get_cursor      (GdkWindow       *window);
void	      gdk_window_set_device_cursor (GdkWindow	  *window,
                                            GdkDevice     *device,
                                            GdkCursor	  *cursor);
GdkCursor    *gdk_window_get_device_cursor (GdkWindow     *window,
                                            GdkDevice     *device);
void	      gdk_window_get_user_data	 (GdkWindow	  *window,
					  gpointer	  *data);
void	      gdk_window_get_geometry	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y,
					  gint		  *width,
					  gint		  *height);
int           gdk_window_get_width       (GdkWindow       *window);
int           gdk_window_get_height      (GdkWindow       *window);
void	      gdk_window_get_position	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y);
gint	      gdk_window_get_origin	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y);
void	      gdk_window_get_root_coords (GdkWindow	  *window,
					  gint             x,
					  gint             y,
					  gint		  *root_x,
					  gint		  *root_y);
void       gdk_window_coords_to_parent   (GdkWindow       *window,
                                          gdouble          x,
                                          gdouble          y,
                                          gdouble         *parent_x,
                                          gdouble         *parent_y);
void       gdk_window_coords_from_parent (GdkWindow       *window,
                                          gdouble          parent_x,
                                          gdouble          parent_y,
                                          gdouble         *x,
                                          gdouble         *y);

void	      gdk_window_get_root_origin (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y);
void          gdk_window_get_frame_extents (GdkWindow     *window,
                                            GdkRectangle  *rect);

#ifndef GDK_MULTIDEVICE_SAFE
GDK_DEPRECATED_IN_3_0_FOR(gdk_window_get_device_position)
GdkWindow *   gdk_window_get_pointer     (GdkWindow       *window,
                                          gint            *x,
                                          gint            *y,
                                          GdkModifierType *mask);
#endif /* GDK_MULTIDEVICE_SAFE */
GdkWindow *   gdk_window_get_device_position (GdkWindow       *window,
                                              GdkDevice       *device,
                                              gint            *x,
                                              gint            *y,
                                              GdkModifierType *mask);
GdkWindow *   gdk_window_get_parent      (GdkWindow       *window);
GdkWindow *   gdk_window_get_toplevel    (GdkWindow       *window);

GdkWindow *   gdk_window_get_effective_parent   (GdkWindow *window);
GdkWindow *   gdk_window_get_effective_toplevel (GdkWindow *window);

GList *	      gdk_window_get_children	 (GdkWindow	  *window);
GList *       gdk_window_peek_children   (GdkWindow       *window);
GdkEventMask  gdk_window_get_events	 (GdkWindow	  *window);
void	      gdk_window_set_events	 (GdkWindow	  *window,
					  GdkEventMask	   event_mask);
void          gdk_window_set_device_events (GdkWindow    *window,
                                            GdkDevice    *device,
                                            GdkEventMask  event_mask);
GdkEventMask  gdk_window_get_device_events (GdkWindow    *window,
                                            GdkDevice    *device);

void          gdk_window_set_source_events (GdkWindow      *window,
                                            GdkInputSource  source,
                                            GdkEventMask    event_mask);
GdkEventMask  gdk_window_get_source_events (GdkWindow      *window,
                                            GdkInputSource  source);

void          gdk_window_set_icon_list   (GdkWindow       *window,
					  GList           *pixbufs);
void	      gdk_window_set_icon_name	 (GdkWindow	  *window, 
					  const gchar	  *name);
void	      gdk_window_set_group	 (GdkWindow	  *window, 
					  GdkWindow	  *leader);
GdkWindow*    gdk_window_get_group	 (GdkWindow	  *window);
void	      gdk_window_set_decorations (GdkWindow	  *window,
					  GdkWMDecoration  decorations);
gboolean      gdk_window_get_decorations (GdkWindow       *window,
					  GdkWMDecoration *decorations);
void	      gdk_window_set_functions	 (GdkWindow	  *window,
					  GdkWMFunction	   functions);

cairo_surface_t *
              gdk_window_create_similar_surface (GdkWindow *window,
                                          cairo_content_t  content,
                                          int              width,
                                          int              height);

void          gdk_window_beep            (GdkWindow       *window);
void          gdk_window_iconify         (GdkWindow       *window);
void          gdk_window_deiconify       (GdkWindow       *window);
void          gdk_window_stick           (GdkWindow       *window);
void          gdk_window_unstick         (GdkWindow       *window);
void          gdk_window_maximize        (GdkWindow       *window);
void          gdk_window_unmaximize      (GdkWindow       *window);
void          gdk_window_fullscreen      (GdkWindow       *window);
GDK_AVAILABLE_IN_3_8
void          gdk_window_set_fullscreen_mode (GdkWindow   *window,
                                          GdkFullscreenMode mode);
GDK_AVAILABLE_IN_3_8
GdkFullscreenMode
              gdk_window_get_fullscreen_mode (GdkWindow   *window);
void          gdk_window_unfullscreen    (GdkWindow       *window);
void          gdk_window_set_keep_above  (GdkWindow       *window,
                                          gboolean         setting);
void          gdk_window_set_keep_below  (GdkWindow       *window,
                                          gboolean         setting);
void          gdk_window_set_opacity     (GdkWindow       *window,
                                          gdouble          opacity);
void          gdk_window_register_dnd    (GdkWindow       *window);

GdkDragProtocol
              gdk_window_get_drag_protocol(GdkWindow      *window,
                                           GdkWindow     **target);

void gdk_window_begin_resize_drag            (GdkWindow     *window,
                                              GdkWindowEdge  edge,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);
GDK_AVAILABLE_IN_3_4
void gdk_window_begin_resize_drag_for_device (GdkWindow     *window,
                                              GdkWindowEdge  edge,
                                              GdkDevice     *device,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);
void gdk_window_begin_move_drag              (GdkWindow     *window,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);
GDK_AVAILABLE_IN_3_4
void gdk_window_begin_move_drag_for_device   (GdkWindow     *window,
                                              GdkDevice     *device,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);

/* Interface for dirty-region queueing */
void       gdk_window_invalidate_rect           (GdkWindow          *window,
					         const GdkRectangle *rect,
					         gboolean            invalidate_children);
void       gdk_window_invalidate_region         (GdkWindow          *window,
					         const cairo_region_t    *region,
					         gboolean            invalidate_children);

/**
 * GdkWindowChildFunc:
 * @window: a #GdkWindow
 * @user_data: user data
 *
 * A function of this type is passed to gdk_window_invalidate_maybe_recurse().
 * It gets called for each child of the window to determine whether to
 * recursively invalidate it or now.
 *
 * Returns: %TRUE to invalidate @window recursively
 */
typedef gboolean (*GdkWindowChildFunc)          (GdkWindow *window,
                                                 gpointer   user_data);

void       gdk_window_invalidate_maybe_recurse  (GdkWindow            *window,
						 const cairo_region_t *region,
						 GdkWindowChildFunc    child_func,
						 gpointer              user_data);
cairo_region_t *gdk_window_get_update_area      (GdkWindow            *window);

void       gdk_window_freeze_updates      (GdkWindow    *window);
void       gdk_window_thaw_updates        (GdkWindow    *window);

void       gdk_window_freeze_toplevel_updates_libgtk_only (GdkWindow *window);
void       gdk_window_thaw_toplevel_updates_libgtk_only   (GdkWindow *window);

void       gdk_window_process_all_updates (void);
void       gdk_window_process_updates     (GdkWindow    *window,
					   gboolean      update_children);

/* Enable/disable flicker, so you can tell if your code is inefficient. */
void       gdk_window_set_debug_updates   (gboolean      setting);

void       gdk_window_constrain_size      (GdkGeometry  *geometry,
                                           guint         flags,
                                           gint          width,
                                           gint          height,
                                           gint         *new_width,
                                           gint         *new_height);

GDK_DEPRECATED_IN_3_8
void gdk_window_enable_synchronized_configure (GdkWindow *window);
GDK_DEPRECATED_IN_3_8
void gdk_window_configure_finished            (GdkWindow *window);

GdkWindow *gdk_get_default_root_window (void);

/* Offscreen redirection */
cairo_surface_t *
           gdk_offscreen_window_get_surface    (GdkWindow     *window);
void       gdk_offscreen_window_set_embedder   (GdkWindow     *window,
						GdkWindow     *embedder);
GdkWindow *gdk_offscreen_window_get_embedder   (GdkWindow     *window);
void       gdk_window_geometry_changed         (GdkWindow     *window);

/* Multidevice support */
void       gdk_window_set_support_multidevice (GdkWindow *window,
                                               gboolean   support_multidevice);
gboolean   gdk_window_get_support_multidevice (GdkWindow *window);

/* Frame clock */
GDK_AVAILABLE_IN_3_8
GdkFrameClock* gdk_window_get_frame_clock      (GdkWindow     *window);

G_END_DECLS

#endif /* __GDK_WINDOW_H__ */
