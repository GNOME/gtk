#ifndef __GDK_SCREEN_H__
#define __GDK_SCREEN_H__

#include "gdk/gdktypes.h"
#include "gdk/gdkdisplaymgr.h"
#include "gdk/gdkdisplay.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkScreenClass	GdkScreenClass;

#define GDK_TYPE_SCREEN              (gdk_screen_get_type ())
#define GDK_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN, GdkScreen))
#define GDK_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SCREEN, GdkScreenClass))
#define GDK_IS_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN))
#define GDK_IS_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SCREEN))
#define GDK_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SCREEN, GdkScreenClass))



struct _GdkScreen
{
 GObject	parent_instance;
};

struct _GdkScreenClass 
{
 GObjectClass 	    parent_class;
 GdkDisplay *	    (*get_display)		(GdkScreen * screen);
 gint 		    (*get_width)		(GdkScreen * screen);
 gint 		    (*get_height)		(GdkScreen * screen);
 gint 		    (*get_width_mm)		(GdkScreen * screen);
 gint 		    (*get_height_mm)		(GdkScreen * screen);
 gint 		    (*get_root_depth)		(GdkScreen * screen);
 gint		    (*get_screen_num)		(GdkScreen * screen);
 GdkWindow *	    (*get_parent_root)		(GdkScreen * screen);
 GList *	    (*get_list_visuals)		(GdkScreen * screen);
 GdkColormap *	    (*get_default_colormap)	(GdkScreen * screen);
 void		    (*set_default_colormap)	(GdkScreen * screen,
						 GdkColormap *colormap);
 GdkWindow *	    (*get_window_at_pointer)	(GdkScreen * screen,
						 gint * win_x,
						 gint * win_y);
};
#define DEFAULT_GDK_SCREEN  (GDK_DISPLAY_GET_CLASS(DEFAULT_GDK_DISPLAY)->get_default_screen(DEFAULT_GDK_DISPLAY))
GType 		    gdk_screen_get_type	(void);
GdkScreen *	    gdk_screen_ref	(GdkScreen * screen);
void		    gdk_screen_unref	(GdkScreen * screen);

GdkColormap *	    gdk_screen_get_default_colormap	(GdkScreen *screen);
void		    gdk_screen_set_default_colormap	(GdkScreen *screen,
							 GdkColormap *colormap);
GdkWindow *	    gdk_screen_get_parent_root		(GdkScreen *screen);
GdkDisplay *	    gdk_screen_get_display		(GdkScreen *screen);
gint		    gdk_screen_get_screen_num		(GdkScreen *screen);
GList *		    gdk_screen_get_list_visuals		(GdkScreen *screen);
GdkWindow *	    gdk_screen_get_window_at_pointer	(GdkScreen *screen,
							 gint * win_x,
							 gint * win_y);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __GDK_SCREEN_H__ */
