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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* Uninstalled header defining types and functions internal to GDK */

#include <gdk/gdktypes.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkprivate.h>

#ifndef __GDK_INTERNALS_H__
#define __GDK_INTERNALS_H__

/**********************
 * General Facilities * 
 **********************/

/* Debugging support */

typedef struct _GdkColorInfo           GdkColorInfo;
typedef struct _GdkEventFilter	       GdkEventFilter;
typedef struct _GdkClientFilter	       GdkClientFilter;

typedef enum {
  GDK_COLOR_WRITEABLE = 1 << 0
} GdkColorInfoFlags;

struct _GdkColorInfo
{
  GdkColorInfoFlags flags;
  guint ref_count;
};

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
};

struct _GdkClientFilter {
  GdkAtom       type;
  GdkFilterFunc function;
  gpointer      data;
};

typedef enum {
  GDK_DEBUG_MISC          = 1 << 0,
  GDK_DEBUG_EVENTS        = 1 << 1,
  GDK_DEBUG_DND           = 1 << 2,
  GDK_DEBUG_XIM           = 1 << 3,
  GDK_DEBUG_NOGRABS       = 1 << 4,
  GDK_DEBUG_COLORMAP	  = 1 << 5,
  GDK_DEBUG_GDKRGB	  = 1 << 6,
  GDK_DEBUG_GC		  = 1 << 7,
  GDK_DEBUG_PIXMAP	  = 1 << 8,
  GDK_DEBUG_IMAGE	  = 1 << 9,
  GDK_DEBUG_INPUT	  = 1 <<10,
  GDK_DEBUG_CURSOR	  = 1 <<11,
  GDK_DEBUG_MULTIHEAD	  = 1 <<12,
  GDK_DEBUG_XINERAMA	  = 1 <<13,
  GDK_DEBUG_DRAW	  = 1 <<14
} GdkDebugFlag;

#ifndef GDK_DISABLE_DEPRECATED

typedef struct _GdkFontPrivate	       GdkFontPrivate;

struct _GdkFontPrivate
{
  GdkFont font;
  guint ref_count;
};

#endif /* GDK_DISABLE_DEPRECATED */

extern GList            *_gdk_default_filters;
extern GdkWindow  	*_gdk_parent_root;
extern gint		 _gdk_error_code;
extern gint		 _gdk_error_warnings;

extern guint _gdk_debug_flags;

#ifdef G_ENABLE_DEBUG

#define GDK_NOTE(type,action)		     G_STMT_START { \
    if (_gdk_debug_flags & GDK_DEBUG_##type)		    \
       { action; };			     } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GDK_NOTE(type,action)

#endif /* G_ENABLE_DEBUG */

/* Arg parsing */

typedef enum 
{
  GDK_ARG_STRING,
  GDK_ARG_INT,
  GDK_ARG_BOOL,
  GDK_ARG_NOBOOL,
  GDK_ARG_CALLBACK
} GdkArgType;

typedef struct _GdkArgContext GdkArgContext;
typedef struct _GdkArgDesc GdkArgDesc;

typedef void (*GdkArgFunc) (const char *name, const char *arg, gpointer data);

struct _GdkArgContext
{
  GPtrArray *tables;
  gpointer cb_data;
};

struct _GdkArgDesc
{
  const char *name;
  GdkArgType type;
  gpointer location;
  GdkArgFunc callback;
};

/* Event handling */

typedef struct _GdkEventPrivate GdkEventPrivate;

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0
} GdkEventFlags;

struct _GdkEventPrivate
{
  GdkEvent   event;
  guint      flags;
  GdkScreen *screen;
};

extern GdkEventFunc   _gdk_event_func;    /* Callback for events */
extern gpointer       _gdk_event_data;
extern GDestroyNotify _gdk_event_notify;

extern GSList    *_gdk_displays;
extern gchar     *_gdk_display_name;
extern gint       _gdk_screen_number;
extern gchar     *_gdk_display_arg_name;

void      _gdk_events_queue  (GdkDisplay *display);
GdkEvent* _gdk_event_unqueue (GdkDisplay *display);

GList* _gdk_event_queue_find_first  (GdkDisplay *display);
void   _gdk_event_queue_remove_link (GdkDisplay *display,
				     GList      *node);
GList*  _gdk_event_queue_append     (GdkDisplay *display,
				     GdkEvent   *event);
void _gdk_event_button_generate     (GdkDisplay *display,
				     GdkEvent   *event);

void gdk_synthesize_window_state (GdkWindow     *window,
                                  GdkWindowState unset_flags,
                                  GdkWindowState set_flags);

#define GDK_SCRATCH_IMAGE_WIDTH 256
#define GDK_SCRATCH_IMAGE_HEIGHT 64

GdkImage* _gdk_image_new_for_depth (GdkScreen    *screen,
				    GdkImageType  type,
				    GdkVisual    *visual,
				    gint          width,
				    gint          height,
				    gint          depth);
GdkImage *_gdk_image_get_scratch (GdkScreen *screen,
				  gint	     width,
				  gint	     height,
				  gint	     depth,
				  gint	    *x,
				  gint	    *y);

GdkImage *_gdk_drawable_copy_to_image (GdkDrawable  *drawable,
				       GdkImage     *image,
				       gint          src_x,
				       gint          src_y,
				       gint          dest_x,
				       gint          dest_y,
				       gint          width,
				       gint          height);

cairo_surface_t *_gdk_drawable_ref_cairo_surface (GdkDrawable *drawable);

/* GC caching */
GdkGC *_gdk_drawable_get_scratch_gc (GdkDrawable *drawable,
				     gboolean     graphics_exposures);

void _gdk_gc_update_context (GdkGC     *gc,
			     cairo_t   *cr,
			     GdkColor  *override_foreground,
			     GdkBitmap *override_stipple,
			     gboolean   gc_changed);

/*************************************
 * Interfaces used by windowing code *
 *************************************/

void       _gdk_window_destroy           (GdkWindow   *window,
					  gboolean     foreign_destroy);
void       _gdk_window_clear_update_area (GdkWindow   *window);

void       _gdk_screen_close             (GdkScreen   *screen);

const char *_gdk_get_sm_client_id (void);

void _gdk_gc_init (GdkGC           *gc,
		   GdkDrawable     *drawable,
		   GdkGCValues     *values,
		   GdkGCValuesMask  values_mask);

GdkRegion *_gdk_gc_get_clip_region (GdkGC *gc);
GdkFill    _gdk_gc_get_fill        (GdkGC *gc);
GdkPixmap *_gdk_gc_get_tile        (GdkGC *gc);
GdkBitmap *_gdk_gc_get_stipple     (GdkGC *gc);
guint32    _gdk_gc_get_fg_pixel    (GdkGC *gc);
guint32    _gdk_gc_get_bg_pixel    (GdkGC *gc);

/*****************************************
 * Interfaces provided by windowing code *
 *****************************************/

/* Font/string functions implemented in module-specific code */
gint _gdk_font_strlen (GdkFont *font, const char *str);
void _gdk_font_destroy (GdkFont *font);

void _gdk_colormap_real_destroy (GdkColormap *colormap);

void _gdk_cursor_destroy (GdkCursor *cursor);

void     _gdk_windowing_init                    (void);

extern const GOptionEntry _gdk_windowing_args[];
void     _gdk_windowing_set_default_display     (GdkDisplay *display);

gchar *_gdk_windowing_substitute_screen_number (const gchar *display_name,
					        gint         screen_number);

void     _gdk_windowing_window_get_offsets      (GdkWindow  *window,
						 gint       *x_offset,
						 gint       *y_offset);
void     _gdk_windowing_window_clear_area       (GdkWindow  *window,
						 gint        x,
						 gint        y,
						 gint        width,
						 gint        height);
void     _gdk_windowing_window_clear_area_e     (GdkWindow  *window,
						 gint        x,
						 gint        y,
						 gint        width,
						 gint        height);

void       _gdk_windowing_get_pointer        (GdkDisplay       *display,
					      GdkScreen       **screen,
					      gint             *x,
					      gint             *y,
					      GdkModifierType  *mask);
GdkWindow* _gdk_windowing_window_get_pointer (GdkDisplay       *display,
					      GdkWindow        *window,
					      gint             *x,
					      gint             *y,
					      GdkModifierType  *mask);
GdkWindow* _gdk_windowing_window_at_pointer  (GdkDisplay       *display,
					      gint             *win_x,
					      gint             *win_y);

/* Return the number of bits-per-pixel for images of the specified depth. */
gint _gdk_windowing_get_bits_for_depth (GdkDisplay *display,
					gint        depth);

#define GDK_WINDOW_IS_MAPPED(window) ((((GdkWindowObject*)window)->state & GDK_WINDOW_STATE_WITHDRAWN) == 0)

/* Called before processing updates for a window. This gives the windowing
 * layer a chance to save the region for later use in avoiding duplicate
 * exposes. The return value indicates whether the function has a saved
 * the region; if the result is TRUE, then the windowing layer is responsible
 * for destroying the region later.
 */
gboolean _gdk_windowing_window_queue_antiexpose (GdkWindow  *window,
						 GdkRegion  *area);

/* Called to do the windowing system specific part of gdk_window_destroy(),
 *
 * window: The window being destroyed
 * recursing: If TRUE, then this is being called because a parent
 *            was destroyed. This generally means that the call to the windowing system
 *            to destroy the window can be omitted, since it will be destroyed as a result
 *            of the parent being destroyed. Unless @foreign_destroy
 *            
 * foreign_destroy: If TRUE, the window or a parent was destroyed by some external 
 *            agency. The window has already been destroyed and no windowing
 *            system calls should be made. (This may never happen for some
 *            windowing systems.)
 */
void _gdk_windowing_window_destroy (GdkWindow *window,
				    gboolean   recursing,
				    gboolean   foreign_destroy);

/* Called when gdk_window_destroy() is called on a foreign window
 * or an ancestor of the foreign window. It should generally reparent
 * the window out of it's current heirarchy, hide it, and then
 * send a message to the owner requesting that the window be destroyed.
 */
void _gdk_windowing_window_destroy_foreign (GdkWindow *window);

void _gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					      const gchar *sm_client_id);

#define GDK_TYPE_PAINTABLE            (_gdk_paintable_get_type ())
#define GDK_PAINTABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_PAINTABLE, GdkPaintable))
#define GDK_IS_PAINTABLE(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_PAINTABLE))
#define GDK_PAINTABLE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDK_TYPE_PAINTABLE, GdkPaintableIface))

typedef struct _GdkPaintable        GdkPaintable;
typedef struct _GdkPaintableIface   GdkPaintableIface;

struct _GdkPaintableIface
{
  GTypeInterface g_iface;
  
  void (* begin_paint_region) (GdkPaintable *paintable,
			       GdkRegion    *region);
  void (* end_paint)          (GdkPaintable *paintable);

  void (* invalidate_maybe_recurse) (GdkPaintable *paintable,
				     GdkRegion    *region,
				     gboolean    (*child_func) (GdkWindow *, gpointer),
				     gpointer      user_data);
  void (* process_updates)          (GdkPaintable *paintable,
				     gboolean      update_children);
};

GType _gdk_paintable_get_type (void) G_GNUC_CONST;

/* Implementation types */
GType _gdk_window_impl_get_type (void) G_GNUC_CONST;
GType _gdk_pixmap_impl_get_type (void) G_GNUC_CONST;


/**
 * _gdk_windowing_gc_set_clip_region:
 * @gc: a #GdkGC
 * @region: the new clip region
 * 
 * Do any window-system specific processing necessary
 * for a change in clip region. Since the clip origin
 * will likely change before the GC is used with the
 * new clip, frequently this function will only set a flag and
 * do the real processing later.
 *
 * When this function is called, _gdk_gc_get_clip_region
 * will already return the new region.
 **/
void _gdk_windowing_gc_set_clip_region (GdkGC     *gc,
					GdkRegion *region);

/**
 * _gdk_windowing_gc_copy:
 * @dst_gc: a #GdkGC from the GDK backend
 * @src_gc: a #GdkGC from the GDK backend
 * 
 * Copies backend specific state from @src_gc to @dst_gc.
 * This is called before the generic state is copied, so
 * the old generic state is still available from @dst_gc
 **/
void _gdk_windowing_gc_copy (GdkGC *dst_gc,
			     GdkGC *src_gc);
     
/* Queries the current foreground color of a GdkGC */
void _gdk_windowing_gc_get_foreground (GdkGC    *gc,
				       GdkColor *color);
/* Queries the current background color of a GdkGC */
void _gdk_windowing_gc_get_background (GdkGC    *gc,
				       GdkColor *color);

/************************************
 * Initialization and exit routines *
 ************************************/

void _gdk_image_exit  (void);
void _gdk_windowing_exit (void);

G_END_DECLS

#endif /* __GDK_INTERNALS_H__ */
