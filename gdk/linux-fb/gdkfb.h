#ifndef GDKFB_H
#define GDKFB_H 1

#include "gdk/gdkprivate.h"

typedef struct _GdkFBDisplay GdkFBDisplay;
typedef struct _GdkFBWindow GdkFBWindow;

extern GdkFBWindow *gdk_root_window;
extern GdkFBDisplay *gdk_display;

#define GDK_ROOT_WINDOW()             gdk_root_window
#define GDK_ROOT_PARENT()             ((GdkWindow *)gdk_parent_root)
#define GDK_DISPLAY()                 gdk_display

extern const char *gdk_progclass;

#define gdk_window_foreign_new(xid) gdk_window_ref(xid)
#define gdk_window_lookup(xid)	   ((GdkWindow*) (xid))
#define gdk_pixmap_lookup(xid)	   ((GdkPixmap*) (xid))
#define gdk_font_lookup(xid)	   ((GdkFont*) (xid))

extern GdkAtom gdk_selection_property;

/* FB specific functions: */

typedef gboolean (*GdkWindowChildChanged) (GdkWindow *window,
					   gint x, gint y,
					   gint width, gint height,
					   gpointer user_data);
typedef void     (*GdkWindowChildGetPos)  (GdkWindow *window,
					   gint *x, gint *y,
					   gpointer user_data);

gboolean  _gdk_window_get_decorations   (GdkWindow       *window,
					 GdkWMDecoration *decorations);
void      _gdk_window_set_child_handler (GdkWindow             *window,
					 GdkWindowChildChanged changed,
					 GdkWindowChildGetPos  get_pos,
					 gpointer              user_data);
#endif /* GDKFB_H */
