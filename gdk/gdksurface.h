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

#ifndef __GDK_SURFACE_H__
#define __GDK_SURFACE_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkdrawingcontext.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkframeclock.h>
#include <gdk/gdkmonitor.h>

G_BEGIN_DECLS

typedef struct _GdkGeometry          GdkGeometry;

/**
 * GdkSurfaceType:
 * @GDK_SURFACE_ROOT: root window; this window has no parent, covers the entire
 *  screen, and is created by the window system
 * @GDK_SURFACE_TOPLEVEL: toplevel window (used to implement #GtkWindow)
 * @GDK_SURFACE_CHILD: child window (used to implement e.g. #GtkEntry)
 * @GDK_SURFACE_TEMP: override redirect temporary window (used to implement
 *  #GtkMenu)
 * @GDK_SURFACE_FOREIGN: foreign window (see gdk_surface_foreign_new())
 * @GDK_SURFACE_SUBSURFACE: subsurface-based window; This window is visually
 *  tied to a toplevel, and is moved/stacked with it. Currently this window
 *  type is only implemented in Wayland. Since 3.14
 *
 * Describes the kind of window.
 */
typedef enum
{
  GDK_SURFACE_ROOT,
  GDK_SURFACE_TOPLEVEL,
  GDK_SURFACE_CHILD,
  GDK_SURFACE_TEMP,
  GDK_SURFACE_FOREIGN,
  GDK_SURFACE_SUBSURFACE
} GdkSurfaceType;

/* Size restriction enumeration.
 */
/**
 * GdkSurfaceHints:
 * @GDK_HINT_POS: indicates that the program has positioned the window
 * @GDK_HINT_MIN_SIZE: min size fields are set
 * @GDK_HINT_MAX_SIZE: max size fields are set
 * @GDK_HINT_BASE_SIZE: base size fields are set
 * @GDK_HINT_ASPECT: aspect ratio fields are set
 * @GDK_HINT_RESIZE_INC: resize increment fields are set
 * @GDK_HINT_WIN_GRAVITY: window gravity field is set
 * @GDK_HINT_USER_POS: indicates that the window’s position was explicitly set
 *  by the user
 * @GDK_HINT_USER_SIZE: indicates that the window’s size was explicitly set by
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
} GdkSurfaceHints;

/* The next two enumeration values current match the
 * Motif constants. If this is changed, the implementation
 * of gdk_surface_set_decorations/gdk_surface_set_functions
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
 * X protocol. If you change that, gdksurface-x11.c/gdk_surface_set_geometry_hints()
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
 * GdkAnchorHints:
 * @GDK_ANCHOR_FLIP_X: allow flipping anchors horizontally
 * @GDK_ANCHOR_FLIP_Y: allow flipping anchors vertically
 * @GDK_ANCHOR_SLIDE_X: allow sliding window horizontally
 * @GDK_ANCHOR_SLIDE_Y: allow sliding window vertically
 * @GDK_ANCHOR_RESIZE_X: allow resizing window horizontally
 * @GDK_ANCHOR_RESIZE_Y: allow resizing window vertically
 * @GDK_ANCHOR_FLIP: allow flipping anchors on both axes
 * @GDK_ANCHOR_SLIDE: allow sliding window on both axes
 * @GDK_ANCHOR_RESIZE: allow resizing window on both axes
 *
 * Positioning hints for aligning a window relative to a rectangle.
 *
 * These hints determine how the window should be positioned in the case that
 * the window would fall off-screen if placed in its ideal position.
 *
 * For example, %GDK_ANCHOR_FLIP_X will replace %GDK_GRAVITY_NORTH_WEST with
 * %GDK_GRAVITY_NORTH_EAST and vice versa if the window extends beyond the left
 * or right edges of the monitor.
 *
 * If %GDK_ANCHOR_SLIDE_X is set, the window can be shifted horizontally to fit
 * on-screen. If %GDK_ANCHOR_RESIZE_X is set, the window can be shrunken
 * horizontally to fit.
 *
 * In general, when multiple flags are set, flipping should take precedence over
 * sliding, which should take precedence over resizing.
 *
 * Since: 3.22
 * Stability: Unstable
 */
typedef enum
{
  GDK_ANCHOR_FLIP_X   = 1 << 0,
  GDK_ANCHOR_FLIP_Y   = 1 << 1,
  GDK_ANCHOR_SLIDE_X  = 1 << 2,
  GDK_ANCHOR_SLIDE_Y  = 1 << 3,
  GDK_ANCHOR_RESIZE_X = 1 << 4,
  GDK_ANCHOR_RESIZE_Y = 1 << 5,
  GDK_ANCHOR_FLIP     = GDK_ANCHOR_FLIP_X | GDK_ANCHOR_FLIP_Y,
  GDK_ANCHOR_SLIDE    = GDK_ANCHOR_SLIDE_X | GDK_ANCHOR_SLIDE_Y,
  GDK_ANCHOR_RESIZE   = GDK_ANCHOR_RESIZE_X | GDK_ANCHOR_RESIZE_Y
} GdkAnchorHints;

/**
 * GdkSurfaceEdge:
 * @GDK_SURFACE_EDGE_NORTH_WEST: the top left corner.
 * @GDK_SURFACE_EDGE_NORTH: the top edge.
 * @GDK_SURFACE_EDGE_NORTH_EAST: the top right corner.
 * @GDK_SURFACE_EDGE_WEST: the left edge.
 * @GDK_SURFACE_EDGE_EAST: the right edge.
 * @GDK_SURFACE_EDGE_SOUTH_WEST: the lower left corner.
 * @GDK_SURFACE_EDGE_SOUTH: the lower edge.
 * @GDK_SURFACE_EDGE_SOUTH_EAST: the lower right corner.
 *
 * Determines a window edge or corner.
 */
typedef enum
{
  GDK_SURFACE_EDGE_NORTH_WEST,
  GDK_SURFACE_EDGE_NORTH,
  GDK_SURFACE_EDGE_NORTH_EAST,
  GDK_SURFACE_EDGE_WEST,
  GDK_SURFACE_EDGE_EAST,
  GDK_SURFACE_EDGE_SOUTH_WEST,
  GDK_SURFACE_EDGE_SOUTH,
  GDK_SURFACE_EDGE_SOUTH_EAST  
} GdkSurfaceEdge;

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
 * a window’s geometry constraints. Normally you would set these on
 * the GTK+ level using gtk_window_set_geometry_hints(). #GtkWindow
 * then sets the hints on the #GdkSurface it creates.
 *
 * gdk_surface_set_geometry_hints() expects the hints to be fully valid already
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
 * the “character grid” will be allowed.
 *
 * Here’s an example of how the terminal example would be implemented, assuming
 * a terminal area widget called “terminal” and a toplevel window “toplevel”:
 *
 * |[<!-- language="C" -->
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
 * ]|
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

/**
 * GdkSurfaceState:
 * @GDK_SURFACE_STATE_WITHDRAWN: the window is not shown.
 * @GDK_SURFACE_STATE_ICONIFIED: the window is minimized.
 * @GDK_SURFACE_STATE_MAXIMIZED: the window is maximized.
 * @GDK_SURFACE_STATE_STICKY: the window is sticky.
 * @GDK_SURFACE_STATE_FULLSCREEN: the window is maximized without
 *   decorations.
 * @GDK_SURFACE_STATE_ABOVE: the window is kept above other windows.
 * @GDK_SURFACE_STATE_BELOW: the window is kept below other windows.
 * @GDK_SURFACE_STATE_FOCUSED: the window is presented as focused (with active decorations).
 * @GDK_SURFACE_STATE_TILED: the window is in a tiled state, Since 3.10. Since 3.91.2, this
 *                          is deprecated in favor of per-edge information.
 * @GDK_SURFACE_STATE_TOP_TILED: whether the top edge is tiled, Since 3.91.2
 * @GDK_SURFACE_STATE_TOP_RESIZABLE: whether the top edge is resizable, Since 3.91.2
 * @GDK_SURFACE_STATE_RIGHT_TILED: whether the right edge is tiled, Since 3.91.2
 * @GDK_SURFACE_STATE_RIGHT_RESIZABLE: whether the right edge is resizable, Since 3.91.2
 * @GDK_SURFACE_STATE_BOTTOM_TILED: whether the bottom edge is tiled, Since 3.91.2
 * @GDK_SURFACE_STATE_BOTTOM_RESIZABLE: whether the bottom edge is resizable, Since 3.91.2
 * @GDK_SURFACE_STATE_LEFT_TILED: whether the left edge is tiled, Since 3.91.2
 * @GDK_SURFACE_STATE_LEFT_RESIZABLE: whether the left edge is resizable, Since 3.91.2
 *
 * Specifies the state of a toplevel window.
 */
typedef enum
{
  GDK_SURFACE_STATE_WITHDRAWN        = 1 << 0,
  GDK_SURFACE_STATE_ICONIFIED        = 1 << 1,
  GDK_SURFACE_STATE_MAXIMIZED        = 1 << 2,
  GDK_SURFACE_STATE_STICKY           = 1 << 3,
  GDK_SURFACE_STATE_FULLSCREEN       = 1 << 4,
  GDK_SURFACE_STATE_ABOVE            = 1 << 5,
  GDK_SURFACE_STATE_BELOW            = 1 << 6,
  GDK_SURFACE_STATE_FOCUSED          = 1 << 7,
  GDK_SURFACE_STATE_TILED            = 1 << 8,
  GDK_SURFACE_STATE_TOP_TILED        = 1 << 9,
  GDK_SURFACE_STATE_TOP_RESIZABLE    = 1 << 10,
  GDK_SURFACE_STATE_RIGHT_TILED      = 1 << 11,
  GDK_SURFACE_STATE_RIGHT_RESIZABLE  = 1 << 12,
  GDK_SURFACE_STATE_BOTTOM_TILED     = 1 << 13,
  GDK_SURFACE_STATE_BOTTOM_RESIZABLE = 1 << 14,
  GDK_SURFACE_STATE_LEFT_TILED       = 1 << 15,
  GDK_SURFACE_STATE_LEFT_RESIZABLE   = 1 << 16
} GdkSurfaceState;


typedef struct _GdkSurfaceClass GdkSurfaceClass;

#define GDK_TYPE_SURFACE              (gdk_surface_get_type ())
#define GDK_SURFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SURFACE, GdkSurface))
#define GDK_SURFACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SURFACE, GdkSurfaceClass))
#define GDK_IS_SURFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SURFACE))
#define GDK_IS_SURFACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SURFACE))
#define GDK_SURFACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SURFACE, GdkSurfaceClass))


struct _GdkSurfaceClass
{
  GObjectClass      parent_class;

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
GDK_AVAILABLE_IN_ALL
GType         gdk_surface_get_type              (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_new_toplevel          (GdkDisplay    *display,
                                                int            width,
                                                int            height);
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_new_popup             (GdkDisplay    *display,
                                                const GdkRectangle *position);
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_new_temp              (GdkDisplay    *display);
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_new_child             (GdkSurface     *parent,
                                                const GdkRectangle *position);

GDK_AVAILABLE_IN_ALL
void          gdk_surface_destroy               (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
GdkSurfaceType gdk_surface_get_surface_type       (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
gboolean      gdk_surface_is_destroyed          (GdkSurface     *window);

GDK_AVAILABLE_IN_ALL
GdkDisplay *  gdk_surface_get_display           (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_show                  (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_hide                  (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_withdraw              (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_show_unraised         (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_move                  (GdkSurface     *window,
                                                gint           x,
                                                gint           y);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_resize                (GdkSurface     *window,
                                                gint           width,
                                                gint           height);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_move_resize           (GdkSurface     *window,
                                                gint           x,
                                                gint           y,
                                                gint           width,
                                                gint           height);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_raise                 (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_lower                 (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_restack               (GdkSurface     *window,
						GdkSurface     *sibling,
						gboolean       above);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_focus                 (GdkSurface     *window,
                                                guint32        timestamp);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_user_data         (GdkSurface     *window,
                                                gpointer       user_data);
GDK_AVAILABLE_IN_ALL
gboolean      gdk_surface_get_accept_focus      (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_accept_focus      (GdkSurface     *window,
					        gboolean       accept_focus);
GDK_AVAILABLE_IN_ALL
gboolean      gdk_surface_get_focus_on_map      (GdkSurface     *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_focus_on_map      (GdkSurface     *window,
					        gboolean       focus_on_map);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_scroll                (GdkSurface     *window,
                                                gint           dx,
                                                gint           dy);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_move_region           (GdkSurface       *window,
						const cairo_region_t *region,
						gint             dx,
						gint             dy);

/* 
 * This allows for making shaped (partially transparent) windows
 * - cool feature, needed for Drag and Drag for example.
 */
GDK_AVAILABLE_IN_ALL
void gdk_surface_shape_combine_region (GdkSurface	      *window,
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
GDK_AVAILABLE_IN_ALL
void gdk_surface_set_child_shapes (GdkSurface *window);

/*
 * This routine allows you to merge (ie ADD) child shapes to your
 * own window’s shape keeping its current shape and ADDING the child
 * shapes to it.
 * 
 * - Raster
 */
GDK_AVAILABLE_IN_ALL
void gdk_surface_merge_child_shapes         (GdkSurface       *window);

GDK_AVAILABLE_IN_ALL
void gdk_surface_input_shape_combine_region (GdkSurface       *window,
                                            const cairo_region_t *shape_region,
                                            gint             offset_x,
                                            gint             offset_y);
GDK_AVAILABLE_IN_ALL
void gdk_surface_set_child_input_shapes     (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void gdk_surface_merge_child_input_shapes   (GdkSurface       *window);


GDK_AVAILABLE_IN_ALL
void gdk_surface_set_pass_through (GdkSurface *window,
                                  gboolean   pass_through);
GDK_AVAILABLE_IN_ALL
gboolean gdk_surface_get_pass_through (GdkSurface *window);

/*
 * Check if a window has been shown, and whether all its
 * parents up to a toplevel have been shown, respectively.
 * Note that a window that is_viewable below is not necessarily
 * viewable in the X sense.
 */
GDK_AVAILABLE_IN_ALL
gboolean gdk_surface_is_visible     (GdkSurface *window);
GDK_AVAILABLE_IN_ALL
gboolean gdk_surface_is_viewable    (GdkSurface *window);
GDK_AVAILABLE_IN_ALL
gboolean gdk_surface_is_input_only  (GdkSurface *window);
GDK_AVAILABLE_IN_ALL
gboolean gdk_surface_is_shaped      (GdkSurface *window);

GDK_AVAILABLE_IN_ALL
GdkSurfaceState gdk_surface_get_state (GdkSurface *window);


/* GdkSurface */

GDK_AVAILABLE_IN_ALL
gboolean      gdk_surface_has_native         (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void              gdk_surface_set_type_hint (GdkSurface        *window,
                                            GdkSurfaceTypeHint hint);
GDK_AVAILABLE_IN_ALL
GdkSurfaceTypeHint gdk_surface_get_type_hint (GdkSurface        *window);

GDK_AVAILABLE_IN_ALL
gboolean      gdk_surface_get_modal_hint   (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_modal_hint   (GdkSurface       *window,
                                           gboolean         modal);

GDK_AVAILABLE_IN_ALL
void gdk_surface_set_skip_taskbar_hint (GdkSurface *window,
                                       gboolean   skips_taskbar);
GDK_AVAILABLE_IN_ALL
void gdk_surface_set_skip_pager_hint   (GdkSurface *window,
                                       gboolean   skips_pager);
GDK_AVAILABLE_IN_ALL
void gdk_surface_set_urgency_hint      (GdkSurface *window,
				       gboolean   urgent);

GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_geometry_hints (GdkSurface          *window,
					     const GdkGeometry  *geometry,
					     GdkSurfaceHints      geom_mask);

GDK_AVAILABLE_IN_ALL
cairo_region_t *gdk_surface_get_clip_region  (GdkSurface          *window);
GDK_AVAILABLE_IN_ALL
cairo_region_t *gdk_surface_get_visible_region(GdkSurface         *window);

GDK_AVAILABLE_IN_ALL
GdkDrawingContext *gdk_surface_begin_draw_frame  (GdkSurface            *window,
                                                 GdkDrawContext       *context,
                                                 const cairo_region_t *region);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_end_draw_frame    (GdkSurface            *window,
                                            GdkDrawingContext    *context);

GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_title	   (GdkSurface	  *window,
					    const gchar	  *title);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_role          (GdkSurface     *window,
					    const gchar   *role);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_startup_id    (GdkSurface     *window,
					    const gchar   *startup_id);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_transient_for (GdkSurface     *window,
					    GdkSurface     *parent);

GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_cursor	 (GdkSurface	  *window,
					  GdkCursor	  *cursor);
GDK_AVAILABLE_IN_ALL
GdkCursor    *gdk_surface_get_cursor      (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_device_cursor (GdkSurface	  *window,
                                            GdkDevice     *device,
                                            GdkCursor	  *cursor);
GDK_AVAILABLE_IN_ALL
GdkCursor    *gdk_surface_get_device_cursor (GdkSurface     *window,
                                            GdkDevice     *device);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_get_user_data	 (GdkSurface	  *window,
					  gpointer	  *data);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_get_geometry	 (GdkSurface	  *window,
					  gint		  *x,
					  gint		  *y,
					  gint		  *width,
					  gint		  *height);
GDK_AVAILABLE_IN_ALL
int           gdk_surface_get_width       (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
int           gdk_surface_get_height      (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_get_position	 (GdkSurface	  *window,
					  gint		  *x,
					  gint		  *y);
GDK_AVAILABLE_IN_ALL
gint	      gdk_surface_get_origin	 (GdkSurface	  *window,
					  gint		  *x,
					  gint		  *y);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_get_root_coords (GdkSurface	  *window,
					  gint             x,
					  gint             y,
					  gint		  *root_x,
					  gint		  *root_y);
GDK_AVAILABLE_IN_ALL
void       gdk_surface_coords_to_parent   (GdkSurface       *window,
                                          gdouble          x,
                                          gdouble          y,
                                          gdouble         *parent_x,
                                          gdouble         *parent_y);
GDK_AVAILABLE_IN_ALL
void       gdk_surface_coords_from_parent (GdkSurface       *window,
                                          gdouble          parent_x,
                                          gdouble          parent_y,
                                          gdouble         *x,
                                          gdouble         *y);

GDK_AVAILABLE_IN_ALL
void	      gdk_surface_get_root_origin (GdkSurface	  *window,
					  gint		  *x,
					  gint		  *y);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_get_frame_extents (GdkSurface     *window,
                                            GdkRectangle  *rect);

GDK_AVAILABLE_IN_ALL
gint          gdk_surface_get_scale_factor  (GdkSurface     *window);

GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_get_device_position (GdkSurface       *window,
                                              GdkDevice       *device,
                                              gint            *x,
                                              gint            *y,
                                              GdkModifierType *mask);
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_get_device_position_double (GdkSurface       *window,
                                                     GdkDevice       *device,
                                                     gdouble         *x,
                                                     gdouble         *y,
                                                     GdkModifierType *mask);
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_get_parent      (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_surface_get_toplevel    (GdkSurface       *window);

GDK_AVAILABLE_IN_ALL
GList *	      gdk_surface_get_children	 (GdkSurface	  *window);
GDK_AVAILABLE_IN_ALL
GList *       gdk_surface_peek_children   (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
GList *       gdk_surface_get_children_with_user_data (GdkSurface *window,
						      gpointer   user_data);

GDK_AVAILABLE_IN_ALL
GdkEventMask  gdk_surface_get_events	 (GdkSurface	  *window);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_events	 (GdkSurface	  *window,
					  GdkEventMask	   event_mask);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_device_events (GdkSurface    *window,
                                            GdkDevice    *device,
                                            GdkEventMask  event_mask);
GDK_AVAILABLE_IN_ALL
GdkEventMask  gdk_surface_get_device_events (GdkSurface    *window,
                                            GdkDevice    *device);

GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_icon_list   (GdkSurface       *window,
					  GList           *surfaces);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_icon_name	 (GdkSurface	  *window, 
					  const gchar	  *name);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_group	 (GdkSurface	  *window, 
					  GdkSurface	  *leader);
GDK_AVAILABLE_IN_ALL
GdkSurface*    gdk_surface_get_group	 (GdkSurface	  *window);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_decorations (GdkSurface	  *window,
					  GdkWMDecoration  decorations);
GDK_AVAILABLE_IN_ALL
gboolean      gdk_surface_get_decorations (GdkSurface       *window,
					  GdkWMDecoration *decorations);
GDK_AVAILABLE_IN_ALL
void	      gdk_surface_set_functions	 (GdkSurface	  *window,
					  GdkWMFunction	   functions);

GDK_AVAILABLE_IN_ALL
cairo_surface_t *
              gdk_surface_create_similar_surface (GdkSurface *window,
                                          cairo_content_t  content,
                                          int              width,
                                          int              height);
GDK_AVAILABLE_IN_ALL
cairo_surface_t *
              gdk_surface_create_similar_image_surface (GdkSurface *window,
						       cairo_format_t format,
						       int            width,
						       int            height,
						       int            scale);

GDK_AVAILABLE_IN_ALL
void          gdk_surface_beep            (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_iconify         (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_deiconify       (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_stick           (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_unstick         (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_maximize        (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_unmaximize      (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_fullscreen      (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_fullscreen_on_monitor (GdkSurface      *window,
                                                GdkMonitor     *monitor);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_fullscreen_mode (GdkSurface   *window,
                                          GdkFullscreenMode mode);
GDK_AVAILABLE_IN_ALL
GdkFullscreenMode
              gdk_surface_get_fullscreen_mode (GdkSurface   *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_unfullscreen    (GdkSurface       *window);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_keep_above  (GdkSurface       *window,
                                          gboolean         setting);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_keep_below  (GdkSurface       *window,
                                          gboolean         setting);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_set_opacity     (GdkSurface       *window,
                                          gdouble          opacity);
GDK_AVAILABLE_IN_ALL
void          gdk_surface_register_dnd    (GdkSurface       *window);

GDK_AVAILABLE_IN_ALL
void gdk_surface_begin_resize_drag            (GdkSurface     *window,
                                              GdkSurfaceEdge  edge,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);
GDK_AVAILABLE_IN_ALL
void gdk_surface_begin_resize_drag_for_device (GdkSurface     *window,
                                              GdkSurfaceEdge  edge,
                                              GdkDevice     *device,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);
GDK_AVAILABLE_IN_ALL
void gdk_surface_begin_move_drag              (GdkSurface     *window,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);
GDK_AVAILABLE_IN_ALL
void gdk_surface_begin_move_drag_for_device   (GdkSurface     *window,
                                              GdkDevice     *device,
                                              gint           button,
                                              gint           root_x,
                                              gint           root_y,
                                              guint32        timestamp);

/* Interface for dirty-region queueing */
GDK_AVAILABLE_IN_ALL
void       gdk_surface_invalidate_rect           (GdkSurface          *window,
					         const GdkRectangle *rect,
					         gboolean            invalidate_children);
GDK_AVAILABLE_IN_ALL
void       gdk_surface_invalidate_region         (GdkSurface          *window,
					         const cairo_region_t    *region,
					         gboolean            invalidate_children);

/**
 * GdkSurfaceChildFunc:
 * @window: a #GdkSurface
 * @user_data: user data
 *
 * A function of this type is passed to gdk_surface_invalidate_maybe_recurse().
 * It gets called for each child of the window to determine whether to
 * recursively invalidate it or now.
 *
 * Returns: %TRUE to invalidate @window recursively
 */
typedef gboolean (*GdkSurfaceChildFunc)          (GdkSurface *window,
                                                 gpointer   user_data);

GDK_AVAILABLE_IN_ALL
void       gdk_surface_invalidate_maybe_recurse  (GdkSurface            *window,
						 const cairo_region_t *region,
						 GdkSurfaceChildFunc    child_func,
						 gpointer              user_data);
GDK_AVAILABLE_IN_ALL
cairo_region_t *gdk_surface_get_update_area      (GdkSurface            *window);

GDK_AVAILABLE_IN_ALL
void       gdk_surface_freeze_updates      (GdkSurface    *window);
GDK_AVAILABLE_IN_ALL
void       gdk_surface_thaw_updates        (GdkSurface    *window);

GDK_AVAILABLE_IN_ALL
void       gdk_surface_constrain_size      (GdkGeometry    *geometry,
                                           GdkSurfaceHints  flags,
                                           gint            width,
                                           gint            height,
                                           gint           *new_width,
                                           gint           *new_height);

/* Multidevice support */
GDK_AVAILABLE_IN_ALL
void       gdk_surface_set_support_multidevice (GdkSurface *window,
                                               gboolean   support_multidevice);
GDK_AVAILABLE_IN_ALL
gboolean   gdk_surface_get_support_multidevice (GdkSurface *window);

/* Frame clock */
GDK_AVAILABLE_IN_ALL
GdkFrameClock* gdk_surface_get_frame_clock      (GdkSurface     *window);

GDK_AVAILABLE_IN_ALL
void       gdk_surface_set_opaque_region        (GdkSurface      *window,
                                                cairo_region_t *region);

GDK_AVAILABLE_IN_ALL
void       gdk_surface_set_shadow_width         (GdkSurface      *window,
                                                gint            left,
                                                gint            right,
                                                gint            top,
                                                gint            bottom);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_surface_show_window_menu          (GdkSurface      *window,
                                                GdkEvent       *event);

GDK_AVAILABLE_IN_ALL
GdkGLContext * gdk_surface_create_gl_context    (GdkSurface      *window,
                                                GError        **error);
GDK_AVAILABLE_IN_ALL
GdkVulkanContext *
               gdk_surface_create_vulkan_context(GdkSurface      *window,
                                                GError        **error);

G_END_DECLS

#endif /* __GDK_SURFACE_H__ */
