#ifndef __GDK_DISPLAY_MANAGER__
#define __GDK_DISPLAY_MANAGER__

#include <X11/Xlib.h>
#include "gdkdisplay-x11.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void  gdk_x11_display_manager_class_init (GdkDisplayManagerClass * class);
GdkDisplay * gdk_x11_display_manager_open_display (GdkDisplayManager * dpy_mgr,
						   gchar *display_name);
void	     gdk_x11_display_manager_set_default (GdkDisplayManager * dpy_mgr,
						  GdkDisplay *default_display);
GdkDisplay * gdk_x11_display_manager_get_default	    (GdkDisplayManager * dpy_mgr);
gint	     gdk_x11_display_manager_get_num_display    (GdkDisplayManager * dpy_mgr);
GdkScreen *  gdk_x11_display_manager_get_screen_for_root (GdkDisplayManager * dpy_mgr,Window root);
GdkDisplay * gdk_x11_display_manager_get_display    (GdkDisplayManager * dpy_mgr,Display *dpy);
GdkDisplayImplX11 * gdk_lookup_xdisplay(Display *dpy);



/*#define gdk_get_default_display gdk_x11_display_manager_get_default(dpy_mgr)*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GDK_DISPLAY_MANAGER__*/
