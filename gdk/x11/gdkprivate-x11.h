#ifndef __GDK_PRIVATE_X11_H__
#define __GDK_PRIVATE_X11_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gdk/gdkfont.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkcursor.h>

typedef struct _GdkGCXData          GdkGCXData;
typedef struct _GdkDrawableXData    GdkDrawableXData;
typedef struct _GdkWindowXData      GdkWindowXData;
typedef struct _GdkXPositionInfo    GdkXPositionInfo;
typedef struct _GdkColormapPrivateX GdkColormapPrivateX;
typedef struct _GdkCursorPrivate    GdkCursorPrivate;
typedef struct _GdkFontPrivateX     GdkFontPrivateX;
typedef struct _GdkImagePrivateX    GdkImagePrivateX;
typedef struct _GdkVisualPrivate    GdkVisualPrivate;

#ifdef USE_XIM
typedef struct _GdkICPrivate        GdkICPrivate;
#endif /* USE_XIM */

#define GDK_DRAWABLE_XDATA(win) ((GdkDrawableXData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_WINDOW_XDATA(win) ((GdkWindowXData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_GC_XDATA(gc) ((GdkGCXData *)(((GdkGCPrivate*)(gc))->klass_data))

struct _GdkGCXData
{
  GC xgc;
  Display *xdisplay;
  GdkRegion *clip_region;
  guint dirty_mask;
};

struct _GdkDrawableXData
{
  Window xid;
  Display *xdisplay;
};

struct _GdkXPositionInfo
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x_offset;		/* Offsets to add to X coordinates within window */
  gint y_offset;		/*   to get GDK coodinates within window */
  gboolean big : 1;
  gboolean mapped : 1;
  gboolean no_bg : 1;	        /* Set when the window background is temporarily
				 * unset during resizing and scaling */
  GdkRectangle clip_rect;	/* visible rectangle of window */
};

struct _GdkWindowXData
{
  GdkDrawableXData drawable_data;
  GdkXPositionInfo position_info;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  Display *xdisplay;
};

struct _GdkFontPrivateX
{
  GdkFontPrivate base;
  /* XFontStruct *xfont; */
  /* generic pointer point to XFontStruct or XFontSet */
  gpointer xfont;
  Display *xdisplay;

  GSList *names;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
  Visual *xvisual;
};

struct _GdkColormapPrivateX
{
  GdkColormapPrivate base;

  Colormap xcolormap;
  Display *xdisplay;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  time_t last_sync_time;
};

struct _GdkImagePrivateX
{
  GdkImagePrivate base;
  
  XImage *ximage;
  Display *xdisplay;
  gpointer x_shm_info;
};


#ifdef USE_XIM

struct _GdkICPrivate
{
  XIC xic;
  GdkICAttr *attr;
  GdkICAttributesType mask;
};

#endif /* USE_XIM */

void          gdk_xid_table_insert     (XID             *xid,
					gpointer         data);
void          gdk_xid_table_remove     (XID              xid);
gpointer      gdk_xid_table_lookup     (XID              xid);
gint          gdk_send_xevent          (Window           window,
					gboolean         propagate,
					glong            event_mask,
					XEvent          *event_send);
GdkGC *       _gdk_x11_gc_new          (GdkDrawable     *drawable,
					GdkGCValues     *values,
					GdkGCValuesMask  values_mask);
GdkColormap * gdk_colormap_lookup      (Colormap         xcolormap);
GdkVisual *   gdk_visual_lookup        (Visual          *xvisual);

/* Please see gdkwindow.c for comments on how to use */ 
Window gdk_window_xid_at        (Window    base,
				 gint      bx,
				 gint      by,
				 gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);
Window gdk_window_xid_at_coords (gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);

/* Routines from gdkgeometry-x11.c */
void _gdk_window_init_position     (GdkWindow    *window);
void _gdk_window_move_resize_child (GdkWindow    *window,
				    gint          x,
				    gint          y,
				    gint          width,
				    gint          height);
void _gdk_window_process_expose    (GdkWindow    *window,
				    gulong        serial,
				    GdkRectangle *area);

#define GDK_GC_XGC(gc)       (GDK_GC_XDATA(gc)->xgc)
#define GDK_GC_GET_XGC(gc)   (GDK_GC_XDATA(gc)->dirty_mask ? _gdk_x11_gc_flush (gc) : GDK_GC_XGC (gc))

GC _gdk_x11_gc_flush (GdkGC *gc);

extern GdkDrawableClass  _gdk_x11_drawable_class;
extern gboolean	         gdk_use_xshm;
extern gchar		*gdk_display_name;
extern Display		*gdk_display;
extern Window		 gdk_root_window;
extern Window		 gdk_leader_window;
extern Atom		 gdk_wm_delete_window;
extern Atom		 gdk_wm_take_focus;
extern Atom		 gdk_wm_protocols;
extern Atom		 gdk_wm_window_protocols[];
extern Atom		 gdk_selection_property;
extern GdkWindow	*selection_owner[];
extern gchar		*gdk_progclass;
extern gboolean          gdk_null_window_warnings;
extern const int         gdk_nevent_masks;
extern const int         gdk_event_mask_table[];

extern GdkWindowPrivate *gdk_xgrab_window;  /* Window that currently holds the
					     * x pointer grab
					     */

#ifdef USE_XIM
extern GdkICPrivate *gdk_xim_ic;		/* currently using IC */
extern GdkWindow *gdk_xim_window;	        /* currently using Window */
#endif /* USE_XIM */


#endif /* __GDK_PRIVATE_X11_H__ */


