#include "gdkprivate.h"
#include "gdkdisplaymgr.h"

static void	    gdk_display_manager_class_init (GObjectClass *class);


GType gdk_display_manager_get_type (void){

  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDisplayManagerClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_display_manager_class_init,
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

static void
gdk_display_manager_class_init (GObjectClass *class)
{

}


GdkDisplay*
gdk_display_manager_get_default	(GdkDisplayManager * dpy_mgr)
{
  return GDK_DISPLAY_MGR_GET_CLASS(dpy_mgr)->get_default(dpy_mgr);	
}
GdkDisplay* gdk_display_manager_open_display (GdkDisplayManager * dpy_mgr,
					      gchar *display_name)
{
  return GDK_DISPLAY_MGR_GET_CLASS(dpy_mgr)->open_display(dpy_mgr, display_name);
}

gint
gdk_display_num_display (GdkDisplayManager * dpy_mgr)
{
  return GDK_DISPLAY_MGR_GET_CLASS(dpy_mgr)->get_num_display(dpy_mgr);
}


GdkDisplay*
gdk_get_default_display	(void)
{
  return GDK_DISPLAY_MGR_GET_CLASS(gdk_display_manager)->get_default(gdk_display_manager);
}
GdkScreen*
gdk_get_default_screen (void)
{
  GdkDisplay* dpy = gdk_get_default_display();
  return GDK_DISPLAY_GET_CLASS(dpy)->get_default_screen(dpy);
}



GdkDisplayManager*
gdk_get_display_manager	(void)
{
  return gdk_display_manager;
}



