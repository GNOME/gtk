#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gdk-pixbuf.h"

int close_app(GtkWidget *widget, gpointer data)
{
   gtk_main_quit();
   return TRUE;
}

int expose_cb(GtkWidget *drawing_area, GdkEventExpose *evt, gpointer data)
{
   GdkPixbuf *pixbuf;
         
   pixbuf = (GdkPixbuf *) gtk_object_get_data(GTK_OBJECT(drawing_area),
                                              "pixbuf");
   if(gdk_pixbuf_get_has_alpha (pixbuf))
   {
      gdk_draw_rgb_32_image(drawing_area->window,
                            drawing_area->style->black_gc,
                            evt->area.x, evt->area.y,
                            evt->area.width,
                            evt->area.height,
                            GDK_RGB_DITHER_MAX,
                            gdk_pixbuf_get_pixels (pixbuf) +
			    (evt->area.y * gdk_pixbuf_get_rowstride (pixbuf)) +
			    (evt->area.x * gdk_pixbuf_get_n_channels (pixbuf)),
                            gdk_pixbuf_get_rowstride (pixbuf));
   }
   else
   {
      gdk_draw_rgb_image(drawing_area->window, 
			 drawing_area->style->black_gc, 
			 evt->area.x, evt->area.y,
			 evt->area.width,
			 evt->area.height,  
			 GDK_RGB_DITHER_NORMAL,
			 gdk_pixbuf_get_pixels (pixbuf) +
			 (evt->area.y * gdk_pixbuf_get_rowstride (pixbuf)) +
			 (evt->area.x * gdk_pixbuf_get_n_channels (pixbuf)),
			 gdk_pixbuf_get_rowstride (pixbuf));
   }
   return FALSE;
}

int configure_cb(GtkWidget *drawing_area, GdkEventConfigure *evt, gpointer data)
{
   GdkPixbuf *pixbuf;
                           
   pixbuf = (GdkPixbuf *) gtk_object_get_data(GTK_OBJECT(drawing_area),   
                                              "pixbuf");
    
   g_print("X:%d Y:%d\n", evt->width, evt->height);
   if(evt->width != gdk_pixbuf_get_width (pixbuf) || evt->height != gdk_pixbuf_get_height (pixbuf))
   {
      GdkWindow *root;
      GdkPixbuf *new_pixbuf;

      root = GDK_ROOT_PARENT();
      new_pixbuf = gdk_pixbuf_get_from_drawable(NULL, root, NULL,
						0, 0, 0, 0, evt->width, evt->height);
      gtk_object_set_data(GTK_OBJECT(drawing_area), "pixbuf", new_pixbuf);
      gdk_pixbuf_unref(pixbuf);
   }

   return FALSE;
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

   root = GDK_ROOT_PARENT();
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
                         gdk_pixbuf_get_width (pixbuf),
                         gdk_pixbuf_get_height (pixbuf));
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
