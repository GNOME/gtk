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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_SURFACE_WIN32_H__
#define __GDK_SURFACE_WIN32_H__

#include "gdk/win32/gdkprivate-win32.h"
#include "gdk/win32/gdkwin32cursor.h"
#include "gdk/win32/gdkwin32surface.h"
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdkcursor.h"

#include <windows.h>

G_BEGIN_DECLS

enum _GdkWin32AeroSnapCombo
{
  GDK_WIN32_AEROSNAP_COMBO_NOTHING = 0,
  GDK_WIN32_AEROSNAP_COMBO_UP,
  GDK_WIN32_AEROSNAP_COMBO_DOWN,
  GDK_WIN32_AEROSNAP_COMBO_LEFT,
  GDK_WIN32_AEROSNAP_COMBO_RIGHT,
  /* Same order as non-shift variants. We use it to do things like:
   * AEROSNAP_UP + 4 = AEROSNAP_SHIFTUP
   */
  GDK_WIN32_AEROSNAP_COMBO_SHIFTUP,
  GDK_WIN32_AEROSNAP_COMBO_SHIFTDOWN,
  GDK_WIN32_AEROSNAP_COMBO_SHIFTLEFT,
  GDK_WIN32_AEROSNAP_COMBO_SHIFTRIGHT
};

typedef enum _GdkWin32AeroSnapCombo GdkWin32AeroSnapCombo;

enum _GdkWin32AeroSnapState
{
  GDK_WIN32_AEROSNAP_STATE_UNDETERMINED = 0,
  GDK_WIN32_AEROSNAP_STATE_HALFLEFT,
  GDK_WIN32_AEROSNAP_STATE_HALFRIGHT,
  GDK_WIN32_AEROSNAP_STATE_FULLUP,
  /* Maximize state is only used by edge-snap */
  GDK_WIN32_AEROSNAP_STATE_MAXIMIZE
};

typedef enum _GdkWin32AeroSnapState GdkWin32AeroSnapState;

struct _GdkRectangleDouble
{
  gdouble x;
  gdouble y;
  gdouble width;
  gdouble height;
};

typedef struct _GdkRectangleDouble GdkRectangleDouble;

enum _GdkW32WindowDragOp
{
  GDK_WIN32_DRAGOP_NONE = 0,
  GDK_WIN32_DRAGOP_RESIZE,
  GDK_WIN32_DRAGOP_MOVE,
  GDK_WIN32_DRAGOP_COUNT
};

typedef enum _GdkW32WindowDragOp GdkW32WindowDragOp;

struct _GdkW32DragMoveResizeContext
{
  /* The window that is being moved/resized */
  GdkSurface         *window;

  /* The kind of drag-operation going on. */
  GdkW32WindowDragOp op;

  /* The edge that was grabbed for resizing. Not used for moving. */
  GdkSurfaceEdge      edge;

  /* The device used to initiate the op.
   * We grab it at the beginning and ungrab it at the end.
   */
  GdkDevice         *device;

  /* The button pressed down to initiate the op.
   * The op will be canceled only when *this* button
   * is released.
   */
  gint               button;

  /* Initial cursor position when the operation began.
   * Current cursor position is subtracted from it to find how far
   * to move window border(s).
   */
  gint               start_root_x;
  gint               start_root_y;

  /* Initial window rectangle (position and size).
   * The window is resized/moved relative to this (see start_root_*).
   */
  RECT               start_rect;

  /* Not used */
  guint32            timestamp;

  /* TRUE if during the next redraw we should call SetWindowPos() to push
   * the window size and poistion to the native window.
   */
  gboolean           native_move_resize_pending;

  /* The cursor we should use while the operation is running. */
  GdkCursor         *cursor;

  /* This window looks like an outline and is drawn under the window
   * that is being dragged. It indicates the shape the dragged window
   * will take if released at a particular point.
   * Indicator window size always matches the target indicator shape,
   * the the actual indicator drawn on it might not, depending on
   * how much time elapsed since the animation started.
   */
  HWND               shape_indicator;

  /* Used to draw the indicator */
  cairo_surface_t   *indicator_surface;
  gint               indicator_surface_width;
  gint               indicator_surface_height;

  /* Size/position of shape_indicator */
  GdkRectangle       indicator_window_rect;

  /* Indicator will animate to occupy this rectangle */
  GdkRectangle       indicator_target;

  /* Indicator will start animating from this rectangle */
  GdkRectangle       indicator_start;

  /* Timestamp of the animation start */
  gint64             indicator_start_time;

  /* Timer that drives the animation */
  guint              timer;

  /* A special timestamp, if we want to draw not how
   * the animation should look *now*, but how it should
   * look at arbitrary moment of time.
   * Set to 0 to tell GDK to use current time.
   */
  gint64             draw_timestamp;

  /* Indicates that a transformation was revealed:
   *
   * For drag-resize: If it's FALSE,
   * then the pointer have not yet hit a trigger that triggers fullup.
   * If TRUE, then the pointer did hit a trigger that triggers fullup
   * at some point during this drag op.
   * This is used to prevent drag-resize from triggering
   * a transformation when first approaching a trigger of the work area -
   * one must drag it all the way to the very trigger to trigger; afterwards
   * a transformation will start triggering at some distance from the trigger
   * for as long as the op is still running. This is how AeroSnap works.
   *
   * For drag-move: If it's FALSE,
   * then the pointer have not yet hit a trigger, even if it is
   * already within an edge region.
   * If it's TRUE, then the pointer did hit a trigger within an
   * edge region, and have not yet left an edge region
   * (passing from one edge region into another doesn't count).
   */
  gboolean           revealed;

  /* Arrays of GdkRectangle pairs, describing the areas of the virtual
   * desktop that trigger various AeroSnap window transofrmations
   * Coordinates are GDK screen coordinates.
   */
  GArray            *halfleft_regions;
  GArray            *halfright_regions;
  GArray            *maximize_regions;
  GArray            *fullup_regions;

  /* Current pointer position will result in this kind of snapping,
   * if the drag op is finished.
   */
  GdkWin32AeroSnapState current_snap;
};

typedef struct _GdkW32DragMoveResizeContext GdkW32DragMoveResizeContext;

/* defined in gdkdrop-win32.c */
typedef struct _drop_target_context drop_target_context;

struct _GdkWin32Surface
{
  GdkSurface parent_instance;

  HANDLE handle;

  HICON   hicon_big;
  HICON   hicon_small;

  /* The cursor that GDK set for this window via GdkDevice */
  GdkWin32HCursor *cursor;

  /* When VK_PACKET sends us a leading surrogate, it's stashed here.
   * Later, when another VK_PACKET sends a tailing surrogate, we make up
   * a full unicode character from them, or discard the leading surrogate,
   * if the next key is not a tailing surrogate.
   */
  wchar_t leading_surrogate_keydown;
  wchar_t leading_surrogate_keyup;

  /* Window size hints */
  gint hint_flags;
  GdkGeometry hints;

  /* Non-NULL for any window that is registered as a drop target.
   * For OLE2 protocol only.
   */
  drop_target_context *drop_target;
  /* Temporarily holds the GdkDrop currently associated with this window.
   * For LOCAL protocol only.
   */
  GdkDrop             *drop;

  GdkSurfaceTypeHint type_hint;

  GdkSurface *transient_owner;
  GSList    *transient_children;
  gint       num_transients;
  gboolean   changing_state;

  gint initial_x;
  gint initial_y;

  /* left/right/top/bottom width of the shadow/resize-grip around the window */
  RECT margins;

  /* left+right and top+bottom from @margins */
  gint margins_x;
  gint margins_y;

  /* Set to TRUE when GTK tells us that margins are 0 everywhere.
   * We don't actually set margins to 0, we just set this bit.
   */
  guint zero_margins : 1;
  guint inhibit_configure : 1;

  /* Set to TRUE if window is using true layered mode adjustments
   * via UpdateLayeredWindow().
   * Layered windows that get SetLayeredWindowAttributes() called
   * on them are not true layered windows.
   */
  guint layered : 1;

  /* If TRUE, the @temp_styles is set to the styles that were temporarily
   * added to this window.
   */
  guint have_temp_styles : 1;

  /* If TRUE, the window is in the process of being maximized.
   * This is set by WM_SYSCOMMAND and by gdk_win32_surface_maximize (),
   * and is unset when WM_WINDOWPOSCHANGING is handled.
   */
  guint maximizing : 1;

  /* GDK does not keep window contents around, it just draws new
   * stuff over the window where changes occurred.
   * cache_surface retains old window contents, because
   * UpdateLayeredWindow() doesn't do partial redraws.
   */
  cairo_surface_t *cache_surface;

  /* Unlike window-backed surfaces, DIB-backed surface
   * does not provide a way to query its size,
   * so we have to remember it ourselves.
   */
  gint             dib_width;
  gint             dib_height;

  /* If the client wants uniformly-transparent window,
   * we remember the opacity value here and apply it
   * during UpdateLayredWindow() call, for layered windows.
   */
  gdouble          layered_opacity;

  HDC              hdc;
  int              hdc_count;
  HBITMAP          saved_dc_bitmap; /* Original bitmap for dc */

  GdkW32DragMoveResizeContext drag_move_resize_context;

  /* Remembers where the window was snapped.
   * Some snap operations change their meaning if
   * the window is already snapped.
   */
  GdkWin32AeroSnapState snap_state;

  /* Remembers window position before it was snapped.
   * This is used to unsnap it.
   * Position and size are percentages of the workarea
   * of the monitor on which the window was before it was snapped.
   */
  GdkRectangleDouble *snap_stash;

  /* Also remember the same position, but in absolute form. */
  GdkRectangle *snap_stash_int;

  /* Decorations set by gdk_surface_set_decorations() or NULL if unset */
  GdkWMDecoration* decorations;

  /* No. of windows to force layered windows off */
  guint suppress_layered;

  /* Temporary styles that this window got for the purpose of
   * handling WM_SYSMENU.
   * They are removed at the first opportunity (usually WM_INITMENU).
   */
  LONG_PTR temp_styles;

  /* scale of window on HiDPI */
  gint surface_scale;
  gint unscaled_width;
  gint unscaled_height;
};

struct _GdkWin32SurfaceClass
{
  GdkSurfaceClass parent_class;
};

GType _gdk_win32_surface_get_type (void);

void  _gdk_win32_surface_update_style_bits   (GdkSurface *window);

gint  _gdk_win32_surface_get_scale_factor    (GdkSurface *window);

void  _gdk_win32_get_window_client_area_rect (GdkSurface *window,
                                              gint        scale,
                                              RECT       *rect);
void  _gdk_win32_update_layered_window_from_cache (GdkSurface *window,
                                                   RECT       *client_rect,
                                                   gboolean    do_move,
                                                   gboolean    do_resize,
                                                   gboolean    do_paint);

void gdk_win32_surface_move (GdkSurface *surface,
                             gint        x,
                             gint        y);

void gdk_win32_surface_move_resize (GdkSurface *window,
                                    gint        x,
                                    gint        y,
                                    gint        width,
                                    gint        height);

void
gdk_win32_surface_get_queued_window_rect (GdkSurface *surface,
                                          gint        scale,
                                          RECT       *return_window_rect);

void
gdk_win32_surface_apply_queued_move_resize (GdkSurface *surface,
                                            RECT        window_rect);

G_END_DECLS

#endif /* __GDK_SURFACE_WIN32_H__ */
