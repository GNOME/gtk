
#include "gdk.h"
#include "config.h"

#include "gdkwindow.h"
#include "gdkprivate-nanox.h"
#include "gdkprivate.h"

static void create_toplevel (GR_WINDOW_ID parent, GR_WINDOW_ID win, int x, int y, int width, int height);
static int  manage_event (GR_EVENT *event);
static void set_title (GR_WINDOW_ID win, char* title);

typedef struct {
		char * name;
		void (*create_toplevel) (GR_WINDOW_ID parent, GR_WINDOW_ID win, int x, int y, int width, int height);
		int  (*manage_event) (GR_EVENT *event);
		void (*set_title) (GR_WINDOW_ID win, char* title);
} GdkWindowManager;

typedef struct {
		GR_WINDOW_ID pwin;
		GR_WINDOW_ID win;
		char *title;
		GdkWMFunction functions;
		GdkWMDecoration decors;
} WMInfo;

static GdkWindowManager test_wm = {
		"test",
		create_toplevel,
		manage_event,
		set_title
};

static GdkWindowManager *default_wm = &test_wm;
static GHashTable * wm_hash = NULL;
GdkDrawableClass _gdk_windowing_window_class;

static void create_toplevel (GR_WINDOW_ID parent, GR_WINDOW_ID win, int x, int y, int width, int height)
{
		WMInfo *winfo;

		winfo = g_new0(WMInfo, 1);
    	winfo->pwin = GrNewWindow(parent, x, y-20, width, height+20, 0, RGB(150, 50,150), WHITE);
		winfo->win = win;
		GrReparentWindow(winfo->pwin, win, 20, 0);
		if (!wm_hash)
				wm_hash = g_hash_table_new(g_int_hash, g_int_equal);
		g_hash_table_insert(wm_hash, winfo->pwin, winfo);
}

static int  manage_event (GR_EVENT *event) {
		return 0;
}

static void set_title (GR_WINDOW_ID win, char* title) {
}


static void
gdk_nanox_window_destroy (GdkDrawable *drawable)
{
  if (!GDK_DRAWABLE_DESTROYED (drawable))
    {
      if (GDK_DRAWABLE_TYPE (drawable) == GDK_WINDOW_FOREIGN)
	gdk_xid_table_remove (GDK_DRAWABLE_XID (drawable));
      else
	g_warning ("losing last reference to undestroyed window\n");
    }

  g_free (GDK_DRAWABLE_XDATA (drawable));
}

static GdkWindowPrivate*
gdk_window_nanox_alloc() {
  GdkWindow *window;
  GdkWindowPrivate *private;
  
  static GdkDrawableClass klass;
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      
      klass = _gdk_nanox_drawable_class;
      klass.destroy = gdk_nanox_window_destroy;
    }

  window = _gdk_window_alloc ();
  private = (GdkWindowPrivate *)window;

  private->drawable.klass = &klass;
  private->drawable.klass_data = g_new (GdkDrawableXData, 1);

  return window;
}

static void
gdk_window_internal_destroy (GdkWindow *window,
			     gboolean   xdestroy,
			     gboolean   our_destroy)
{
}

GR_WINDOW_ID gdk_root_window = GR_ROOT_WINDOW_ID;

void
gdk_window_init (void)
{
  GdkWindowPrivate *private;

  gdk_parent_root = gdk_window_nanox_alloc ();
  private = (GdkWindowPrivate *)gdk_parent_root;
  
  GDK_DRAWABLE_XDATA (gdk_parent_root)->xid = gdk_root_window;

  private->drawable.window_type = GDK_WINDOW_ROOT;
  private->drawable.width = gdk_screen_width();
  private->drawable.height = gdk_screen_height();
  
  gdk_window_set_events(private, -1);
  gdk_xid_table_insert (&gdk_root_window, gdk_parent_root);
}


GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GR_WINDOW_ID new_win;
  GdkWindowPrivate *private, *parent_private;
  int x, y, width, height;
  int border = 1;
  
  if (!parent)
    parent = gdk_parent_root;

  if (GDK_DRAWABLE_DESTROYED (parent))
    return NULL;
 
  private->parent = parent;

  parent_private = (GdkWindowPrivate*)parent;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;
  
  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;
  
  width = attributes->width;
  height = attributes->height;
  
  private = gdk_window_nanox_alloc();
  private->x = x;
  private->y = y;
  private->drawable.width = (attributes->width > 1) ? (attributes->width) : (1);
  private->drawable.height = (attributes->height > 1) ? (attributes->height) : (1);
  private->drawable.window_type = attributes->window_type;

  if (attributes->window_type == GDK_WINDOW_TOPLEVEL || attributes->window_type == GDK_WINDOW_DIALOG)
		  border = 2;
  /* if toplevel reparent to our own window managed window... (check override_redirect) */
  if (attributes->wclass == GDK_INPUT_OUTPUT)
    new_win = GrNewWindow(GDK_WINDOW_XWINDOW(parent), x, y, width, height, border, RGB(150,150,150), WHITE);
  else
    new_win = GrNewInputWindow(GDK_WINDOW_XWINDOW(parent), x, y, width, height);

  GDK_DRAWABLE_XDATA(private)->xid = new_win;
  gdk_drawable_ref(private);
  
  private->drawable.colormap = gdk_colormap_get_system (); 

  gdk_xid_table_insert (&GDK_DRAWABLE_XID(private), private);
  g_message("created window %d %d %d %d %d", new_win, x, y, width, height);
  GrSelectEvents(GDK_DRAWABLE_XID(private), -1);
  return (GdkWindow*)private;;
}


GdkWindow *
gdk_window_foreign_new (guint32 anid)
{
		g_message("unimplemented %s", __FUNCTION__);
	return NULL;
}

void
gdk_window_destroy (GdkWindow *window)
{
  gdk_window_internal_destroy (window, TRUE, TRUE);
  gdk_drawable_unref (window);
}

void
gdk_window_destroy_notify (GdkWindow *window)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->drawable.destroyed)
    {
      private->mapped = TRUE;
      GrRaiseWindow (GDK_DRAWABLE_XID (window));
      GrMapWindow (GDK_DRAWABLE_XID (window));
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->drawable.destroyed)
    {
      private->mapped = FALSE;
      GrUnmapWindow (GDK_DRAWABLE_XID (window));
    }
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    GrRaiseWindow (GDK_DRAWABLE_XID (window));
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    GrLowerWindow (GDK_DRAWABLE_XID (window));
}

void
gdk_window_withdraw (GdkWindow *window)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GrMoveWindow(GDK_DRAWABLE_XID(window), x, y);
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GrResizeWindow(GDK_DRAWABLE_XID(window), width, height);
}

void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GrMoveWindow(GDK_DRAWABLE_XID(window), x, y);
  GrResizeWindow(GDK_DRAWABLE_XID(window), width, height);
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GrReparentWindow(GDK_DRAWABLE_XID(window), GDK_DRAWABLE_XID(new_parent), x, y);
}

void
gdk_window_clear (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    GrClearWindow (GDK_DRAWABLE_XID (window), 0);
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    GrClearWindow (GDK_DRAWABLE_XID (window), 0);
}

void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    GrClearWindow (GDK_DRAWABLE_XID (window), 1);
}

void
gdk_window_set_hints (GdkWindow *window,
		      gint       x,
		      gint       y,
		      gint       min_width,
		      gint       min_height,
		      gint       max_width,
		      gint       max_height,
		      gint       flags)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
		g_message("unimplemented %s", __FUNCTION__);
  if (GDK_DRAWABLE_DESTROYED (window))
    return;
}


void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gboolean   parent_relative)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GR_WINDOW_INFO winfo;
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    window = gdk_parent_root;
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      GrGetWindowInfo(GDK_DRAWABLE_XID(window), &winfo);
	if (x)
	  *x = winfo.x;
	if (y)
	  *y = winfo.y;
	if (width)
	  *width = winfo.width;
	if (height)
	  *height = winfo.height;
	if (depth)
	  *depth = 24;
    }
}


gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
		g_message("unimplemented %s", __FUNCTION__);
	return 0;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
		g_message("unimplemented %s", __FUNCTION__);
	return 0;
}


void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
		g_message("unimplemented %s", __FUNCTION__);
}


GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
			gint            *x,
			gint            *y,
			GdkModifierType *mask)
{
		g_message("unimplemented %s", __FUNCTION__);
	return NULL;
}

GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
		g_message("unimplemented %s", __FUNCTION__);
	return NULL;
}


GList*
gdk_window_get_children (GdkWindow *window)
{
		g_message("unimplemented %s", __FUNCTION__);
	return NULL;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
	return -1;
}


void          
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  GrSelectEvents(GDK_DRAWABLE_XID(window), -1);
}


void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void          
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
		g_message("unimplemented %s", __FUNCTION__);
}


gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
		g_message("unimplemented %s", __FUNCTION__);
	return 0;
}

void
_gdk_windowing_window_get_offsets (GdkWindow *window, gint      *x_offset, gint      *y_offset) {
		*x_offset = *y_offset = 0;
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow *window, GdkRegion *area) {
	return FALSE;
}
