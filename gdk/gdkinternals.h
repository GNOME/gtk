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
  GDK_DEBUG_XINERAMA	  = 1 <<13
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

/* GC caching */
GdkGC *_gdk_drawable_get_scratch_gc (GdkDrawable *drawable,
				     gboolean     graphics_exposures);

/*************************************
 * Interfaces used by windowing code *
 *************************************/

void       _gdk_window_destroy           (GdkWindow   *window,
					  gboolean     foreign_destroy);
void       _gdk_window_clear_update_area (GdkWindow   *window);

void       _gdk_screen_close             (GdkScreen   *screen);

const char *_gdk_get_sm_client_id (void);

/*****************************************
 * Interfaces provided by windowing code *
 *****************************************/

/* Font/string functions implemented in module-specific code */
gint _gdk_font_strlen (GdkFont *font, const char *str);
void _gdk_font_destroy (GdkFont *font);

void _gdk_colormap_real_destroy (GdkColormap *colormap);

void _gdk_cursor_destroy (GdkCursor *cursor);

void     _gdk_windowing_init                    (void);

extern GOptionEntry _gdk_windowing_args[];
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

/* Implementation types */
GType _gdk_window_impl_get_type (void) G_GNUC_CONST;
GType _gdk_pixmap_impl_get_type (void) G_GNUC_CONST;


/* Queries the current foreground color of a GdkGC */
void _gdk_windowing_gc_get_foreground (GdkGC    *gc,
				       GdkColor *color);

/************************************
 * Initialization and exit routines *
 ************************************/

void _gdk_image_exit  (void);
void _gdk_windowing_exit (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_INTERNALS_H__ */
