#include "gdkscreen.h"

static void	    gdk_screen_class_init (GObjectClass *class);


GType gdk_screen_get_type (void){

  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkScreenClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_screen_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkScreen),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkScreen",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_screen_class_init (GObjectClass *class)
{

}

GdkColormap *
gdk_screen_get_default_colormap(GdkScreen *screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_default_colormap(screen);
}
void 
gdk_screen_set_default_colormap(GdkScreen *screen, 
				GdkColormap *colormap)
{
  GDK_SCREEN_GET_CLASS(screen)->set_default_colormap(screen, colormap);
}
GdkWindow *
gdk_screen_get_root_window(GdkScreen *screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_root_window(screen);
}
GdkDisplay *
gdk_screen_get_display(GdkScreen *screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_display(screen);
}
gint
gdk_screen_get_screen_num(GdkScreen *screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_screen_num(screen);
}
GList*
gdk_screen_get_list_visuals(GdkScreen *screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_list_visuals(screen);
}
GdkWindow *
gdk_screen_get_window_at_pointer (GdkScreen *screen,
				  gint * win_x,
				  gint * win_y)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_window_at_pointer(screen,
							     win_x,
							     win_y);
}

gint
gdk_screen_get_width (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_width (screen);
}

gint
gdk_screen_get_height (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_height (screen);
}


gint
gdk_screen_get_width_mm (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_width_mm (screen);
}

gint
gdk_screen_get_height_mm (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_height_mm (screen);
}


