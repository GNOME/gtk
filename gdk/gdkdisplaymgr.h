#ifndef __GDK_DISPLAY_MANAGER__
#define __GDK_DISPLAY_MANAGER__

#include "gdkdisplay.h"

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */
#define GDK_TYPE_DISPLAY_MGR              (gdk_display_manager_type())
#define GDK_DISPLAY_MANAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_MGR, GdkDisplayManager))
#define GDK_DISPLAY_MANAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_MGR, GdkDisplayManagerClass))
#define GDK_IS_DISPLAY_MGR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_MGR))
#define GDK_IS_DISPLAY_MGR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_MGR))
#define GDK_DISPLAY_MANAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_MGR, GdkDisplayManagerClass))

  typedef struct _GdkDisplayManager GdkDisplayManager;
  typedef struct _GdkDisplayManagerClass GdkDisplayManagerClass;

  struct _GdkDisplayManager
  {
    GObject parent_instance;
    GdkDisplay *default_display;
    GSList *open_displays;
  };

  struct _GdkDisplayManagerClass
  {
    GObjectClass parent_class;

    GdkDisplay *(*open_display) (GdkDisplayManager * dpy_mgr,
				 gchar * display_name);
    void (*set_default_display) (GdkDisplayManager * dpy_mgr,
				 GdkDisplay * default_display);
    GdkDisplay *(*get_default_display) (GdkDisplayManager * dpy_mgr);
    gint (*get_display_count) (GdkDisplayManager * dpy_mgr);
  };

GType              gdk_display_manager_get_type            (void);
GdkDisplay *       gdk_display_manager_get_default_display (GdkDisplayManager *dpy_mgr);
GdkDisplay *       gdk_display_manager_open_display        (GdkDisplayManager *dpy_mgr,
							    gchar             *display_name);
gint               gdk_display_manager_get_display_count   (GdkDisplayManager *dpy_mgr);
GdkDisplay *       gdk_get_default_display                 (void);
GdkScreen *        gdk_get_default_screen                  (void);
GdkDisplayManager *gdk_get_display_manager                 (void);




#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* __GDK_DISPLAY_MANAGER__ */
