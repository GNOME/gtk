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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_FB_H__
#define __GDK_PRIVATE_FB_H__

#include <gdk/gdkprivate.h>
#include <gdk/gdk.h>
#include "gdkfb.h"
#include "gdkregion-generic.h"
#include <linux/fb.h>
#include <stdio.h>
#include <freetype/freetype.h>

#define GDK_DRAWABLE_FBDATA(win) ((GdkDrawableFBData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_PIXMAP_FBDATA(win) ((GdkPixmapFBData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_WINDOW_FBDATA(win) ((GdkWindowFBData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_FONT_FB(f) ((GdkFontPrivateFB *)(f))
#define GDK_CURSOR_FB(c) ((GdkCursorPrivateFB *)(c))

typedef struct _GdkDrawableFBData GdkDrawableFBData;
typedef struct _GdkWindowFBData GdkWindowFBData;
typedef struct _GdkPixmapFBData GdkPixmapFBData;

struct _GdkDrawableFBData
{
  guchar *mem;

  gint abs_x, abs_y, lim_x, lim_y, llim_x, llim_y; /* computed values */

  guint rowstride;
};

struct _GdkPixmapFBData
{
  GdkDrawableFBData drawable_data;
};

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

  GdkEventMask event_mask;
  gint level;
  gboolean realized : 1;
};

struct _GdkFBDisplay
{
  int fd;
  guchar *fbmem;
  gpointer active_cmap;
  gulong mem_len;
  struct fb_fix_screeninfo sinfo;
  struct fb_var_screeninfo modeinfo;
  int red_byte, green_byte, blue_byte; /* For truecolor */
};

typedef struct {
  GdkVisual base;
} GdkVisualPrivateFB;

typedef struct {
  GdkColormapPrivate base;

  GHashTable *hash;
  GdkColorInfo *info;
  guint sync_tag;
} GdkColormapPrivateFB;

typedef struct {
  GdkCursor base;
  GdkPixmap *cursor, *mask;
  int hot_x, hot_y;
} GdkCursorPrivateFB;

typedef struct {
  GdkFontPrivate base;

  FT_Face face;
  double size;
} GdkFontPrivateFB;

void gdk_fb_font_init(void);
void gdk_fb_font_fini(void);

typedef struct {
  GdkImagePrivate base;
} GdkImagePrivateFB;

#define GDK_GC_FBDATA(x) ((GdkGCFBData *)((GdkGCPrivate *)x)->klass_data)
typedef struct {
  GdkRegion *clip_region;
  gchar *dash_list;
  GdkGCValuesMask values_mask;
  GdkGCValues values;
  gint dash_offset;
  gushort dash_list_len;
  guchar depth, alu;
} GdkGCFBData;

GdkGC *       _gdk_fb_gc_new          (GdkDrawable     *drawable,
				       GdkGCValues     *values,
				       GdkGCValuesMask  values_mask);

/* Routines from gdkgeometry-fb.c */

void _gdk_window_init_position     (GdkWindow    *window);
void _gdk_window_move_resize_child (GdkWindow    *window,
				    gint          x,
				    gint          y,
				    gint          width,
				    gint          height);
void _gdk_window_process_expose    (GdkWindow    *window,
				    gulong        serial,
				    GdkRectangle *area);
GdkGC *_gdk_fb_gc_new(GdkDrawable *drawable, GdkGCValues *values, GdkGCValuesMask values_mask);

void gdk_fb_drawable_clear(GdkDrawable *drawable);
void gdk_fb_draw_drawable  (GdkDrawable    *drawable,
			    GdkGC          *gc,
			    GdkPixmap      *src,
			    gint            xsrc,
			    gint            ysrc,
			    gint            xdest,
			    gint            ydest,
			    gint            width,
			    gint            height);

typedef struct {
  GdkWindow *bg_relto;
  GdkPixmap *bgpm;

  GdkRegion *real_clip_region;

  guchar *mem, *clipmem;
  gpointer cursor_dc;

  guint rowstride, clip_rowstride;
  int clipxoff, clipyoff;

  gboolean draw_bg : 1;
  gboolean copy_region : 1;
  gboolean handle_cursor : 1;
} GdkFBDrawingContext;

void gdk_fb_drawing_context_init(GdkFBDrawingContext *dc, GdkDrawable *drawable,
				 GdkGC *gc, gboolean draw_bg, gboolean do_clipping);
void gdk_fb_drawing_context_finalize(GdkFBDrawingContext *dc);

void gdk_fb_draw_drawable_3 (GdkDrawable *drawable,
			     GdkGC *gc,
			     GdkPixmap *src,
			     GdkFBDrawingContext *dc,
			     gint xsrc,
			     gint ysrc,
			     gint xdest,
			     gint ydest,
			     gint width,
			     gint height);

void gdk_fb_draw_drawable_2 (GdkDrawable *drawable,
			     GdkGC       *gc,
			     GdkPixmap   *src,
			     gint         xsrc,
			     gint         ysrc,
			     gint         xdest,
			     gint         ydest,
			     gint         width,
			     gint         height,
			     gboolean     draw_bg,
			     gboolean     do_clipping);
void gdk_fb_draw_rectangle (GdkDrawable    *drawable,
			    GdkGC          *gc,
			    gint            filled,
			    gint            x,
			    gint            y,
			    gint            width,
			    gint            height);
void gdk_fb_fill_spans(GdkDrawable *drawable, GdkGC *gc, GdkRectangle *rects, int nrects);

extern GdkWindow *_gdk_fb_pointer_grab_window, *_gdk_fb_keyboard_grab_window, *_gdk_fb_pointer_grab_confine;
extern GdkEventMask _gdk_fb_pointer_grab_events, _gdk_fb_keyboard_grab_events;
extern GdkCursor *_gdk_fb_pointer_grab_cursor;
extern GdkFBDisplay *gdk_display;
extern GdkDrawableClass _gdk_fb_drawable_class;
extern FILE *debug_out;
GdkEvent *gdk_event_make(GdkWindow *window, GdkEventType type, gboolean append_to_queue);

void gdk_fb_get_cursor_rect(GdkRectangle *rect);
void gdk_fb_cursor_unhide(void);
void gdk_fb_cursor_hide(void);
void gdk_fb_redraw_all(void);

void gdk_input_ps2_get_mouseinfo(gint *x, gint *y, GdkModifierType *mask);

#define PANGO_TYPE_FB_FONT              (pango_fb_font_get_type ())
#define PANGO_FB_FONT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), PANGO_TYPE_FB_FONT, PangoFBFont))
typedef struct _PangoFBFont        PangoFBFont;
struct _PangoFBFont
{
  PangoFont parent;

  FT_Face ftf;
  PangoFontDescription desc;
  PangoCoverage *coverage;
  GHashTable *extents;
};
GType pango_fb_font_get_type (void);
gboolean pango_fb_has_glyph(PangoFont *font, PangoGlyph glyph);
PangoGlyph pango_fb_get_unknown_glyph(PangoFont *font);

extern void CM(void); /* Check for general mem corruption */
extern void RP(GdkDrawable *drawable); /* Same, for pixmaps */

#endif /* __GDK_PRIVATE_FB_H__ */
