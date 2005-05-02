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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_FB_H__
#define __GDK_PRIVATE_FB_H__

#include <gdk/gdkprivate.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdk.h>
#include "gdkfb.h"
#include "gdkregion-generic.h"
#include <linux/fb.h>
#include <stdio.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define GDK_TYPE_DRAWABLE_IMPL_FBDATA (gdk_drawable_impl_fb_get_type ())
#define GDK_DRAWABLE_IMPL_FBDATA(win) ((GdkDrawableFBData *)((GdkWindowObject *)(win))->impl)
#define GDK_IS_DRAWABLE_IMPL_FBDATA(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_FBDATA))
#define GDK_DRAWABLE_IMPL(win)        ((GdkDrawable *)((GdkWindowObject *)(win))->impl)
#define GDK_WINDOW_IMPL_FBDATA(win)   ((GdkWindowFBData *)((GdkWindowObject *)(win))->impl)
#define GDK_PIXMAP_IMPL_FBDATA(win)   ((GdkPixmapFBData *)((GdkWindowObject *)(win))->impl)
#define GDK_DRAWABLE_FBDATA(win)      ((GdkDrawableFBData *)(win))
#define GDK_PIXMAP_FBDATA(win)        ((GdkPixmapFBData *)(win))
#define GDK_WINDOW_FBDATA(win)        ((GdkWindowFBData *)(win))
#define GDK_FONT_FB(f)                ((GdkFontPrivateFB *)(f))
#define GDK_CURSOR_FB(c)              ((GdkCursorPrivateFB *)(c))

#define CHECK_IMPL_AND_INTF

#ifdef CHECK_IMPL_AND_INTF
#define GDK_CHECK_IMPL(drawable) \
 g_assert(G_OBJECT_TYPE(drawable) == _gdk_window_impl_get_type() || G_OBJECT_TYPE(drawable) == _gdk_pixmap_impl_get_type())
#define GDK_CHECK_INTF(drawable) \
 g_assert(G_OBJECT_TYPE(drawable) == gdk_window_object_get_type() || G_OBJECT_TYPE(drawable) == gdk_pixmap_get_type())
#else
#define GDK_CHECK_IMPL(drawable) 
#define GDK_CHECK_INTF(drawable)
#endif
typedef struct _GdkDrawableFBData GdkDrawableFBData;
typedef struct _GdkWindowFBData GdkWindowFBData;
typedef struct _GdkPixmapFBData GdkPixmapFBData;
typedef struct _GdkFBDrawingContext GdkFBDrawingContext;

#define GDK_DRAWABLE_PIXMAP (GDK_WINDOW_FOREIGN+1)

struct _GdkDrawableFBData
{
  GdkDrawable parent_object;

  GdkDrawable *wrapper;

  guchar *mem;

  gint abs_x, abs_y, lim_x, lim_y, llim_x, llim_y; /* computed values */

  guint rowstride;

  /* Due to excursions in gdk, this stuff has to be stored here
     instead of in GdkDrawable where it belongs */
  gint width, height, depth;
  GdkColormap *colormap;
  GdkWindowType window_type;
};

typedef struct {
  GdkDrawableClass base_class;
} GdkDrawableFBClass;

struct _GdkPixmapFBData
{
  GdkDrawableFBData drawable_data;
};

typedef struct {
  GdkDrawableFBClass klass;
} GdkPixmapFBClass;

GType gdk_drawable_impl_fb_get_type (void) G_GNUC_CONST;

typedef struct {
  gulong length;
  GdkAtom type;
  gint format;
  guchar data[1];
} GdkWindowProperty;

struct _GdkWindowFBData
{
  GdkDrawableFBData drawable_data;
  GdkCursor *cursor;
  GHashTable *properties;

  GdkRegion *shape; /* Can also be GDK_FB_USE_CHILD_SHAPE */
  
  guint realized : 1;
};

typedef struct {
  GdkDrawableFBClass base_class;
} GdkWindowFBClass;
#define GDK_WINDOW_P(x) ((GdkWindowObject *)(x))

struct _GdkFBDisplay
{
  int tty_fd;
  int console_fd;
  int vt, start_vt;

  /* Used by rendering code: */
  guchar *fb_mem;
  gint fb_width; /* In pixels */
  gint fb_height; /* In pixels */
  gint fb_stride; /* In bytes */

  int fb_fd;
  guchar *fb_mmap;
  gpointer active_cmap;
  gulong mem_len;
  struct fb_fix_screeninfo sinfo;
  struct fb_var_screeninfo modeinfo;
  struct fb_var_screeninfo orig_modeinfo;
  int red_byte, green_byte, blue_byte; /* For truecolor */

  /* fb manager */
  int manager_fd;
  int manager_tag;
  int manager_blocked;
};

struct _GdkVisualClass
{
  GObjectClass parent_class;
};


typedef struct {
  GHashTable *hash;
  GdkColorInfo *info;
  guint sync_tag;
} GdkColormapPrivateFB;

typedef struct {
  GdkCursor base;
  GdkPixmap *cursor, *mask;
  int hot_x, hot_y;
} GdkCursorPrivateFB;

/* Define this to emulate slow emulation of
   GdkFonts using PangoFont */
#define EMULATE_GDKFONT

typedef struct {
  GdkFontPrivate base;
#ifdef EMULATE_GDKFONT
  /* For PangoFont emulation: */
  char *name;
  PangoFont *pango_font;
#else
  /* For bogus GdkFonts: */
  int size;
#endif
} GdkFontPrivateFB;

typedef struct {
  /* Empty */
} GdkImagePrivateFB;

#define GDK_GC_FBDATA(x) ((GdkGCFBData *)(x))
#define GDK_GC_P(x) ((GdkGC *)(x))

typedef enum {
  GPR_USED_BG,
  GPR_AA_GRAYVAL,
  GPR_NONE,
  GPR_ERR_BOUNDS
} GetPixelRet;

typedef enum {
  GDK_FB_SRC_BPP_1,
  GDK_FB_SRC_BPP_8,
  GDK_FB_SRC_BPP_16,
  GDK_FB_SRC_BPP_24,
  GDK_FB_SRC_BPP_32,
  GDK_FB_SRC_BPP_7_AA_GRAYVAL,
  GDK_FB_SRC_BPP_8_AA_GRAYVAL,
  GDK_NUM_FB_SRCBPP
} GdkFbSrcBPP;

typedef void gdk_fb_draw_drawable_func (GdkDrawable *drawable,
					GdkGC       *gc,
					GdkPixmap   *src,
					GdkFBDrawingContext *dc,
					gint         start_y,
					gint         end_y,
					gint         start_x,
					gint         end_x,
					gint         src_x_off,
					gint         src_y_off,
					gint         draw_direction);

typedef struct {
  GdkGC parent_instance;

  gchar *dash_list;
  GdkGCValuesMask values_mask;
  GdkGCValues values;
  gint dash_offset;
  gushort dash_list_len;
  guchar alu;

  /* The GC can only be used with target drawables of
   * the same depth as the initial drawable
   * specified in gd_gc_new().
   */
  guchar depth;
  
  /* Calculated state: */
  /* These functions can only be called for drawables
   * that have the same depth as the gc. 
   */
  void (*set_pixel)        (GdkDrawable    *drawable,
			    GdkGC          *gc,
			    int             x,
			    int             y,
			    gulong          pixel);

  GetPixelRet (*get_color) (GdkDrawable      *drawable,
			    GdkGC            *gc,
			    int               x,
			    int               y,
			    GdkColor         *color);
  
  void (*fill_span)        (GdkDrawable  *drawable,
			    GdkGC        *gc,
			    GdkSpan      *span,
			    GdkColor     *color);

  void (*fill_rectangle)   (GdkDrawable  *drawable,
			    GdkGC	 *gc,
			    GdkRectangle *rect,
			    GdkColor     *color);
  
  gdk_fb_draw_drawable_func *draw_drawable[GDK_NUM_FB_SRCBPP];
} GdkGCFBData;

typedef struct {
  GdkGCClass parent_class;
} GdkGCFBClass;


extern GdkGC *_gdk_fb_screen_gc;

GType gdk_gc_fb_get_type (void) G_GNUC_CONST;

void       _gdk_selection_window_destroyed    (GdkWindow       *window);
void       gdk_window_invalidate_region_clear (GdkWindow       *window,
					       GdkRegion       *region);
void       gdk_window_invalidate_rect_clear   (GdkWindow       *window,
					       GdkRectangle    *rect);
void       gdk_fb_window_send_crossing_events (GdkWindow       *src,
					       GdkWindow       *dest,
					       GdkCrossingMode  mode);
void       gdk_fb_window_move_resize          (GdkWindow       *window,
					       gint             x,
					       gint             y,
					       gint             width,
					       gint             height,
					       gboolean         send_expose_events);
GdkWindow *gdk_fb_window_find_focus           (void);
GdkRegion *gdk_fb_window_get_abs_shape        (GdkDrawable     *window);
GdkRegion *gdk_fb_window_peek_shape           (GdkDrawable     *window,
					       gint            *dx,
					       gint            *dy);
GdkGC *   _gdk_fb_gc_new                      (GdkDrawable     *drawable,
					       GdkGCValues     *values,
					       GdkGCValuesMask  values_mask);

#define _GDK_FB_GC_DEPTH (1<<31)
void      _gdk_fb_gc_calc_state              (GdkGC           *gc,
					      GdkGCValuesMask  changed);

GdkImage *_gdk_fb_copy_to_image              (GdkDrawable    *drawable,
					      GdkImage       *image,
					      gint            src_x,
					      gint            src_y,
					      gint            dest_x,
					      gint            dest_y,
					      gint            width,
					      gint            height);
void      gdk_fb_drawable_clear              (GdkDrawable     *drawable);
void      gdk_fb_draw_drawable               (GdkDrawable     *drawable,
					      GdkGC           *gc,
					      GdkPixmap       *src,
					      gint             xsrc,
					      gint             ysrc,
					      gint             xdest,
					      gint             ydest,
					      gint             width,
					      gint             height);

struct _GdkFBDrawingContext {
  GdkWindow *bg_relto;
  GdkPixmap *bgpm;

  GdkRegion *real_clip_region;

  guchar *mem, *clipmem;
  gpointer cursor_dc;

  guint rowstride, clip_rowstride;
  int clipxoff, clipyoff;

  guint draw_bg : 1;
  guint copy_region : 1;
  guint handle_cursor : 1;
};

void       gdk_fb_drawing_context_init     (GdkFBDrawingContext *dc,
					    GdkDrawable         *drawable,
					    GdkGC               *gc,
					    gboolean             draw_bg,
					    gboolean             do_clipping);
void       gdk_fb_drawing_context_finalize (GdkFBDrawingContext *dc);
void       gdk_fb_draw_drawable_3          (GdkDrawable         *drawable,
					    GdkGC               *gc,
					    GdkPixmap           *src,
					    GdkFBDrawingContext *dc,
					    gint                 xsrc,
					    gint                 ysrc,
					    gint                 xdest,
					    gint                 ydest,
					    gint                 width,
					    gint                 height);
void       gdk_fb_draw_drawable_2          (GdkDrawable         *drawable,
					    GdkGC               *gc,
					    GdkPixmap           *src,
					    gint                 xsrc,
					    gint                 ysrc,
					    gint                 xdest,
					    gint                 ydest,
					    gint                 width,
					    gint                 height,
					    gboolean             draw_bg,
					    gboolean             do_clipping);
void       gdk_fb_draw_rectangle           (GdkDrawable         *drawable,
					    GdkGC               *gc,
					    gboolean             filled,
					    gint                 x,
					    gint                 y,
					    gint                 width,
					    gint                 height);
void       gdk_fb_fill_spans               (GdkDrawable         *real_drawable,
					    GdkGC               *gc,
					    GdkSpan             *spans,
					    int                  nspans,
					    gboolean             sorted);
GdkRegion *gdk_fb_clip_region              (GdkDrawable         *drawable,
					    GdkGC               *gc,
					    gboolean             do_clipping,
					    gboolean             do_children,
					    gboolean             full_shapes);


GdkGrabStatus gdk_fb_pointer_grab          (GdkWindow           *window,
					    gint		 owner_events,
					    GdkEventMask	 event_mask,
					    GdkWindow           *confine_to,
					    GdkCursor           *cursor,
					    guint32              time,
					    gboolean             implicit_grab);
void       gdk_fb_pointer_ungrab           (guint32 time,
					    gboolean implicit_grab);

guint32    gdk_fb_get_time                 (void);

void       gdk_shadow_fb_update            (gint                 minx,
					    gint                 miny,
					    gint                 maxx,
					    gint                 maxy);
void       gdk_shadow_fb_init              (void);
void       gdk_shadow_fb_stop_updates      (void);
void       gdk_fb_recompute_all            (void);

extern GdkAtom _gdk_selection_property;

extern GdkFBAngle _gdk_fb_screen_angle;

/* Pointer grab info */
extern GdkWindow *_gdk_fb_pointer_grab_window;
extern gboolean _gdk_fb_pointer_grab_owner_events;
extern GdkWindow *_gdk_fb_pointer_grab_confine;
extern GdkEventMask _gdk_fb_pointer_grab_events;
extern GdkCursor *_gdk_fb_pointer_grab_cursor;
/* Keyboard grab info */
extern GdkWindow *_gdk_fb_keyboard_grab_window;
extern GdkEventMask _gdk_fb_keyboard_grab_events;
extern gboolean _gdk_fb_keyboard_grab_owner_events;

extern GdkFBDisplay *gdk_display;
extern GdkDisplay *_gdk_display;
extern GdkScreen *_gdk_screen;

extern volatile gboolean _gdk_fb_is_active_vt;

GdkEvent *gdk_event_make(GdkWindow *window,
			 GdkEventType type,
			 gboolean append_to_queue);
GdkWindow * gdk_fb_pointer_event_window (GdkWindow *window,
					 GdkEventType type);
GdkWindow *gdk_fb_keyboard_event_window (GdkWindow *window,
					 GdkEventType type);
GdkWindow *gdk_fb_other_event_window (GdkWindow *window,
				      GdkEventType type);

void gdk_fb_get_cursor_rect(GdkRectangle *rect);
gboolean gdk_fb_cursor_need_hide(GdkRectangle *rect);
gboolean gdk_fb_cursor_region_need_hide(GdkRegion *region);
void gdk_fb_cursor_unhide(void);
void gdk_fb_cursor_reset(void);
void gdk_fb_cursor_hide(void);
void gdk_fb_redraw_all(void);
void gdk_fb_cursor_move (gint x, gint y, GdkWindow *in_window);

guint gdk_fb_keyboard_modifiers (void);
gboolean gdk_fb_keyboard_init  (gboolean open_dev);
gboolean gdk_fb_keyboard_open  (void);
void     gdk_fb_keyboard_close (void);
gboolean gdk_fb_mouse_init     (gboolean open_dev);
gboolean gdk_fb_mouse_open     (void);
void     gdk_fb_mouse_close    (void);
void     gdk_fb_mouse_get_info (gint            *x,
				gint            *y,
				GdkModifierType *mask);

/* Initialization */
void _gdk_windowing_window_init (void);
void _gdk_visual_init (void);
void _gdk_dnd_init    (void);
void _gdk_windowing_image_init  (void);
void _gdk_events_init (void);
void _gdk_input_init (void);

#endif /* __GDK_PRIVATE_FB_H__ */
