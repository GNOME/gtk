#ifndef __GDK_DISPLAY_H__
#define __GDK_DISPLAY_H__

#include <gdk/gdktypes.h>
#include <gobject/gobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkDisplayClass	    GdkDisplayClass;

#define GDK_TYPE_DISPLAY              (gdk_display_get_type ())
#define GDK_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY, GdkDisplay))
#define GDK_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY, GdkDisplayClass))
#define GDK_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY))
#define GDK_IS_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY))
#define GDK_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY, GdkDisplayClass))


struct _GdkDisplay 
{
  GObject parent_instance;
};

struct _GdkDisplayClass
{
  GObjectClass 	    parent_class;
  GdkDisplay *	    (*new_display)	    (gchar * display_name);	    
  gchar *	    (*get_display_name)	    (GdkDisplay *dpy);	    
  gint 		    (*get_n_screens)	    (GdkDisplay *dpy);	    
  GdkScreen *	    (*get_screen)	    (GdkDisplay *dpy,gint screen_num);
  GdkScreen *	    (*get_default_screen)   (GdkDisplay *dpy);	    
};

GType		    gdk_display_get_type	(void);
GdkDisplay *
gdk_display_init_new(int argc, char **argv, char *display_name);

GdkDisplay *	gdk_display_new			(gchar * display_name);	    
gchar *		gdk_display_get_name		(GdkDisplay *dpy);	    
gint		gdk_display_get_n_screens	(GdkDisplay *dpy);	    
GdkScreen *	gdk_display_get_screen		(GdkDisplay *dpy,gint screen_num);
GdkScreen *	gdk_display_get_default_screen	(GdkDisplay *dpy);	  

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __GDK_DISPLAY_H__ */
