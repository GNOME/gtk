/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2023 the GTK team
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

#pragma once

#include "gdk/win32/gdkprivate-win32.h"
#include "gdk/win32/gdkwin32cursor.h"
#include "gdk/win32/gdkwin32surface.h"
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdkcursor.h"

#include <windows.h>
#include "gdk/win32/dcomp.h"
#include <directmanipulation.h>

#ifdef HAVE_EGL
# include <epoxy/egl.h>
#endif

G_BEGIN_DECLS

GType gdk_win32_toplevel_get_type (void) G_GNUC_CONST;
GType gdk_win32_popup_get_type (void) G_GNUC_CONST;
GType gdk_win32_drag_surface_get_type (void) G_GNUC_CONST;

#define GDK_TYPE_WIN32_TOPLEVEL (gdk_win32_toplevel_get_type ())
#define GDK_TYPE_WIN32_POPUP (gdk_win32_popup_get_type ())
#define GDK_TYPE_WIN32_DRAG_SURFACE (gdk_win32_drag_surface_get_type ())

typedef enum
{
  GDK_DECOR_ALL         = 1 << 0,
  GDK_DECOR_BORDER      = 1 << 1,
  GDK_DECOR_RESIZEH     = 1 << 2,
  GDK_DECOR_TITLE       = 1 << 3,
  GDK_DECOR_MENU        = 1 << 4,
  GDK_DECOR_MINIMIZE    = 1 << 5,
  GDK_DECOR_MAXIMIZE    = 1 << 6
} GdkWMDecoration;

struct _GdkRectangleDouble
{
  double x;
  double y;
  double width;
  double height;
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
  /* The surface that is being moved/resized */
  GdkSurface         *surface;

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
  int                button;

  /* Initial cursor position when the operation began.
   * Current cursor position is subtracted from it to find how far
   * to move surface border(s).
   */
  int                start_root_x;
  int                start_root_y;

  /* Last processed cursor position. Values are divided by the surface
   * scale.
   */
  int                current_root_x;
  int                current_root_y;

  /* Initial surface HWND rectangle (position and size).
   * The surface is resized/moved relative to this (see start_root_*).
   */
  RECT               start_rect;

  /* Not used */
  guint32            timestamp;

  /* The cursor we should use while the operation is running. */
  GdkCursor         *cursor;
};

typedef struct _GdkW32DragMoveResizeContext GdkW32DragMoveResizeContext;

/* defined in gdkdrop-win32.c */
typedef struct _drop_target_context drop_target_context;

struct _GdkWin32Surface
{
  GdkSurface parent_instance;

  HANDLE handle;

  IDCompositionTarget *dcomp_target;
  IDCompositionVisual *dcomp_visual;

  HICON   hicon_big;
  HICON   hicon_small;

  /* The cursor that GDK set for this surface via GdkDevice */
  GdkWin32HCursor *cursor;

  /* surface size hints */
  int hint_flags;
  GdkGeometry hints;

  /* Non-NULL for any surface that is registered as a drop target */
  drop_target_context *drop_target;

  GdkSurface *transient_owner;
  GSList    *transient_children;
  int        num_transients;
  gboolean   changing_state;

  int initial_x;
  int initial_y;

  /* left/right/top/bottom width of the shadow/resize-grip around the surface HWND 
     in application units, not device pixels */
  RECT shadow;

  guint inhibit_configure : 1;

  /* If TRUE, the @temp_styles is set to the styles that were temporarily
   * added to this surface.
   */
  guint have_temp_styles : 1;

  /* If TRUE, the surface is in the process of being maximized.
   * This is set by WM_SYSCOMMAND and by gdk_win32_surface_maximize (),
   * and is unset when WM_WINDOWPOSCHANGING is handled.
   */
  guint maximizing : 1;

  GdkW32DragMoveResizeContext drag_move_resize_context;

  /* Enable all decorations? */
  gboolean decorate_all;

  /* Temporary styles that this HWND got for the purpose of
   * handling WM_SYSMENU.
   * They are removed at the first opportunity (usually WM_INITMENU).
   */
  LONG_PTR temp_styles;

  /* scale of surface on HiDPI */
  int surface_scale;

  GdkToplevelLayout *toplevel_layout;
  struct {
    int configured_width;
    int configured_height;
    RECT configured_rect;
  } next_layout;
  gboolean force_recompute_size;

  IDirectManipulationViewport *dmanipulation_viewport_pan;
  IDirectManipulationViewport *dmanipulation_viewport_zoom;
};

struct _GdkWin32SurfaceClass
{
  GdkSurfaceClass parent_class;
};

GType _gdk_win32_surface_get_type (void);

void  _gdk_win32_surface_update_style_bits   (GdkSurface *surface);

void gdk_win32_surface_move (GdkSurface *surface,
                             int         x,
                             int         y);

void gdk_win32_surface_move_resize (GdkSurface *surface,
                                    int         x,
                                    int         y,
                                    int         width,
                                    int         height);

GdkSurface *    gdk_win32_drag_surface_new                      (GdkDisplay             *display);

void            gdk_win32_surface_set_dcomp_content             (GdkWin32Surface        *self,
                                                                 IUnknown               *dcomp_content);

#ifdef HAVE_EGL
EGLSurface gdk_win32_surface_get_egl_surface (GdkSurface *surface,
                                              EGLConfig   config,
                                              gboolean    is_dummy);
#endif

G_END_DECLS

