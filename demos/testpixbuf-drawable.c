#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gdk-pixbuf.h"

void close_app(GtkWidget *widget, gpointer data)
{
   gtk_main_quit();
}

void expose_cb(GtkWidget *drawing_area, GdkEventExpose *evt, gpointer
data)
{
   GdkPixbuf *pixbuf;
         
   pixbuf = (GdkPixbuf *) gtk_object_get_data(GTK_OBJECT(drawing_area),
                                              "pixbuf");
      
   if(pixbuf->art_pixbuf->has_alpha)
   {
      gdk_draw_rgb_32_image(drawing_area->window,
                            drawing_area->style->black_gc,
                            evt->area.x, evt->area.y,
                            evt->area.width,
                            evt->area.height,
                            GDK_RGB_DITHER_MAX,
                            pixbuf->art_pixbuf->pixels +
                              (evt->area.y * pixbuf->art_pixbuf->rowstride) +
                              (evt->area.x *  pixbuf->art_pixbuf->n_channels),
                            pixbuf->art_pixbuf->rowstride);
   }
   else
   {
      gdk_draw_rgb_image(drawing_area->window, 
                         drawing_area->style->white_gc, 
                         evt->area.x, evt->area.y,
                         evt->area.width,
                         evt->area.height,  
                         GDK_RGB_DITHER_NORMAL,
                         pixbuf->art_pixbuf->pixels +
                           (evt->area.y * pixbuf->art_pixbuf->rowstride) +
                           (evt->area.x * pixbuf->art_pixbuf->n_channels),
                         pixbuf->art_pixbuf->rowstride);
   }
}

void configure_cb(GtkWidget *drawing_area, GdkEventConfigure *evt,
gpointer data)
{
   GdkPixbuf *pixbuf;
                           
   pixbuf = (GdkPixbuf *) gtk_object_get_data(GTK_OBJECT(drawing_area),   
                                              "pixbuf");
    
   g_print("X:%d Y:%d\n", evt->width, evt->height);
#if 0
   if(((evt->width) != (pixbuf->art_pixbuf->width)) ||
      ((evt->height) != (pixbuf->art_pixbuf->height)))
      gdk_pixbuf_scale(pixbuf, evt->width, evt->height);
#endif                     
}

int main(int argc, char **argv)
{   
   GdkWindow     *root;
   GtkWidget     *window;
   GtkWidget     *vbox;
   GtkWidget     *drawing_area;
   GdkPixbuf     *pixbuf;    
   
   gtk_init(&argc, &argv);   
   gdk_rgb_set_verbose(TRUE);
   gdk_rgb_init();

   gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
   gtk_widget_set_default_visual(gdk_rgb_get_visual());

   root = gdk_window_foreign_new(GDK_ROOT_WINDOW());
   pixbuf = gdk_pixbuf_get_from_drawable(NULL, root, NULL,
					 0, 0, 0, 0, 150, 160);
   
   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_signal_connect(GTK_OBJECT(window), "delete_event",
                      GTK_SIGNAL_FUNC(close_app), NULL);
   gtk_signal_connect(GTK_OBJECT(window), "destroy",   

                      GTK_SIGNAL_FUNC(close_app), NULL);
   
   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);  
   
   drawing_area = gtk_drawing_area_new();
   gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area),
                         pixbuf->art_pixbuf->width,
                         pixbuf->art_pixbuf->height);
   gtk_signal_connect(GTK_OBJECT(drawing_area), "expose_event",
                      GTK_SIGNAL_FUNC(expose_cb), NULL);

   gtk_signal_connect(GTK_OBJECT(drawing_area), "configure_event",
                      GTK_SIGNAL_FUNC(configure_cb), NULL);
   gtk_object_set_data(GTK_OBJECT(drawing_area), "pixbuf", pixbuf);
   gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
   
   gtk_widget_show_all(window);
   gtk_main();
   return 0;
}
