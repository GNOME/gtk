#ifndef __GDK_PRIVATE_X11_H__
#define __GDK_PRIVATE_X11_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gdk/gdkfont.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkcursor.h>

typedef struct _GdkGCXData          GdkGCXData;
typedef struct _GdkDrawableXData    GdkDrawableXData;
typedef struct _GdkColormapPrivateX GdkColormapPrivateX;
typedef struct _GdkCursorPrivate    GdkCursorPrivate;
typedef struct _GdkFontPrivateX     GdkFontPrivateX;
typedef struct _GdkImagePrivateX    GdkImagePrivateX;
typedef struct _GdkVisualPrivate    GdkVisualPrivate;
typedef struct _GdkRegionPrivate    GdkRegionPrivate;

#ifdef USE_XIM
typedef struct _GdkICPrivate        GdkICPrivate;
#endif /* USE_XIM */

#define GDK_DRAWABLE_XDATA(win) ((GdkDrawableXData *)(((GdkDrawablePrivate*)(win))->klass_data))
#define GDK_GC_XDATA(gc) ((GdkGCXData *)(((GdkGCPrivate*)(gc))->klass_data))

struct _GdkGCXData
{
  GC xgc;
  Display *xdisplay;
};

struct _GdkDrawableXData
{
  Window xid;
  Display *xdisplay;
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

struct _GdkRegionPrivate
{
  GdkRegion region;
  Region xregion;
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

