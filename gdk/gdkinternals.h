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

/* Uninstalled header defining types and functions internal to GDK */

#include <gdk/gdktypes.h>
#include <gdk/gdkprivate.h>

#ifndef __GDK_INTERNALS_H__
#define __GDK_INTERNALS_H__

/**********************
 * General Facilities * 
 **********************/

/* Debugging support */

typedef enum {
  GDK_DEBUG_MISC          = 1 << 0,
  GDK_DEBUG_EVENTS        = 1 << 1,
  GDK_DEBUG_DND           = 1 << 2,
  GDK_DEBUG_COLOR_CONTEXT = 1 << 3,
  GDK_DEBUG_XIM           = 1 << 4
} GdkDebugFlag;

extern gint		 gdk_debug_level;
extern gboolean		 gdk_show_events;
extern GList            *gdk_default_filters;

GDKVAR guint gdk_debug_flags;

#ifdef G_ENABLE_DEBUG

#define GDK_NOTE(type,action)		     G_STMT_START { \
    if (gdk_debug_flags & GDK_DEBUG_##type)		    \
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

extern GdkEventFunc   gdk_event_func;    /* Callback for events */
extern gpointer       gdk_event_data;
extern GDestroyNotify gdk_event_notify;

/* FIFO's for event queue, and for events put back using
 * gdk_event_put().
 */
extern GList *gdk_queued_events;
extern GList *gdk_queued_tail;

GdkEvent* gdk_event_new (void);

void      gdk_events_init   (void);
void      gdk_events_queue  (void);
GdkEvent* gdk_event_unqueue (void);

GList* gdk_event_queue_find_first  (void);
void   gdk_event_queue_remove_link (GList    *node);
void   gdk_event_queue_append      (GdkEvent *event);

void gdk_event_button_generate (GdkEvent *event);

/*************************************
 * Interfaces used by windowing code *
 *************************************/

#ifdef USE_XIM
/* XIM support */
gint   gdk_im_open		 (void);
void   gdk_im_close		 (void);
void   gdk_ic_cleanup		 (void);
#endif /* USE_XIM */

GdkWindow* _gdk_window_alloc             (void);
void       _gdk_window_draw_image        (GdkDrawable *drawable,
					  GdkGC       *gc,
					  GdkImage    *image,
					  gint         xsrc,
					  gint         ysrc,
					  gint         xdest,
					  gint         ydest,
					  gint         width,
					  gint         height);
void       _gdk_window_destroy           (GdkWindow   *window,
					  gboolean     foreign_destroy);
void       _gdk_window_clear_update_area (GdkWindow   *window);
      

/*****************************************
 * Interfaces provided by windowing code *
 *****************************************/

/* Font/string functions implemented in module-specific code */
gint _gdk_font_strlen (GdkFont *font, const char *str);
void _gdk_font_destroy (GdkFont *font);

void _gdk_colormap_real_destroy (GdkColormap *colormap);

void _gdk_cursor_destroy (GdkCursor *cursor);

/* Class supplied by windowing-system-dependent code for GdkWindow.
 */
extern GdkDrawableClass _gdk_windowing_window_class;

/* Class for covering over windowing-system-dependent and backing-store
 * code for GdkWindow
 */
extern GdkDrawableClass _gdk_window_class;

extern GdkArgDesc _gdk_windowing_args[];
gboolean _gdk_windowing_init_check              (int         argc,
						 char      **argv);
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

/************************************
 * Initialization and exit routines *
 ************************************/

void gdk_window_init (void);
void gdk_visual_init (void);
void gdk_dnd_init    (void);

void gdk_image_init  (void);
void gdk_image_exit  (void);

void gdk_input_init  (void);
void gdk_input_exit  (void);

void gdk_windowing_exit (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_INTERNALS_H__ */
