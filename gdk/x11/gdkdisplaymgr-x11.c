#include <glib.h>
#include "gdkdisplaymgr.h"
#include "gdkdisplaymgr-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"




GType gdk_display_manager_type(void){
 static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDisplayManagerClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_x11_display_manager_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDisplayManager),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDisplayManager",
                                            &object_info, 0);
    }
  return object_type;
}
void  gdk_x11_display_manager_class_init(GdkDisplayManagerClass * class){

/*GdkDisplayManagerClass * dpymgr_class = GDK_DISPLAY_MGR_CLASS(class);*/

class->open_display = gdk_x11_display_manager_open_display;
class->set_default = gdk_x11_display_manager_set_default;
class->get_default = gdk_x11_display_manager_get_default;
class->get_num_display = gdk_x11_display_manager_get_num_display;
}
	
GdkDisplay * gdk_x11_display_manager_open_display (GdkDisplayManager * dpy_mgr,
						gchar *display_name){
    GdkDisplay * dpy;
    dpy = gdk_x11_display_impl_display_new(display_name);
    if(!dpy)
        return NULL;

    if(dpy_mgr->open_displays == NULL)
	gdk_x11_display_manager_set_default(dpy_mgr,dpy);

    dpy_mgr->open_displays = g_slist_append(dpy_mgr->open_displays,dpy);
    return dpy;
}


void gdk_x11_display_manager_set_default(GdkDisplayManager * dpy_mgr,
				     GdkDisplay *default_display){
    dpy_mgr->default_display = default_display;
}

GdkDisplay * gdk_x11_display_manager_get_default(GdkDisplayManager * dpy_mgr){
    g_assert(dpy_mgr->default_display != NULL);
    return dpy_mgr->default_display;
}

gint gdk_x11_display_manager_get_num_display(GdkDisplayManager * dpy_mgr){
    gint i = 0;
    GSList * tmp = dpy_mgr->open_displays;
    
    while(tmp != NULL){
        tmp = g_slist_next(tmp);i++;
    }
    return i;
}

GdkScreen *  gdk_x11_display_manager_get_screen_for_root (GdkDisplayManager * dpy_mgr,
							  Window root){
  GdkDisplayImplX11 * tmp_dpy;							  
  GdkScreenImplX11 * tmp_scr;
  GSList * tmp_dpy_list = dpy_mgr->open_displays;
  GSList * tmp_scr_list;
  g_assert(tmp_dpy_list != NULL);						  
  while(tmp_dpy_list != NULL){
    tmp_dpy = (GdkDisplayImplX11*)tmp_dpy_list->data;
    tmp_scr_list = tmp_dpy->screen_list;
    g_assert(tmp_scr_list != NULL);
    while(tmp_scr_list != NULL){
      tmp_scr = (GdkScreenImplX11*)tmp_scr_list->data;
      if(tmp_scr->xroot_window == root)
	return (GdkScreen *) tmp_scr;
      tmp_scr_list = g_slist_next(tmp_scr_list);
    }
    tmp_dpy_list = g_slist_next(tmp_dpy_list);
  }
  return NULL;
}

GdkDisplay * gdk_x11_display_manager_get_display (GdkDisplayManager * dpy_mgr,Display *dpy){
  GdkDisplayImplX11 * tmp_dpy;							  
  GSList * tmp_dpy_list = dpy_mgr->open_displays;
  g_assert(tmp_dpy_list != NULL);						  
  while(tmp_dpy_list != NULL){
    tmp_dpy = (GdkDisplayImplX11*)tmp_dpy_list->data;
    if(tmp_dpy->xdisplay == dpy)
	    return (GdkDisplay*) tmp_dpy;
    tmp_dpy_list = g_slist_next(tmp_dpy_list);
  }
  return NULL;
}
/* Help functions & Macros */


GdkDisplayImplX11 * gdk_lookup_xdisplay(Display *dpy)
{
    return GDK_DISPLAY_IMPL_X11(gdk_x11_display_manager_get_display(gdk_display_manager, dpy));
}



/*#define DEFAULT_GDK_SCREEN(display) GDK_DISPLAY_GET_CLASS((display))->get_default_screen(display)
#define DEFAULT_GDK_SCREEN_IMPL_X11(display) GDK_SCREEN_IMPL_X11(DEFAULT_GDK_SCREEN(display))
*/




