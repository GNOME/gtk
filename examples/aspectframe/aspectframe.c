
#include <gtk/gtk.h>
   
int main( int argc,
          char *argv[] )
{
    GtkWidget *window;
    GtkWidget *aspect_frame;
    GtkWidget *drawing_area;
    gtk_init (&argc, &argv);
   
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Aspect Frame");
    g_signal_connect (G_OBJECT (window), "destroy",
	              G_CALLBACK (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
   
    /* Create an aspect_frame and add it to our toplevel window */
   
    aspect_frame = gtk_aspect_frame_new ("2x1", /* label */
                                         0.5, /* center x */
                                         0.5, /* center y */
                                         2, /* xsize/ysize = 2 */
                                         FALSE /* ignore child's aspect */);
   
    gtk_container_add (GTK_CONTAINER (window), aspect_frame);
    gtk_widget_show (aspect_frame);
   
    /* Now add a child widget to the aspect frame */
   
    drawing_area = gtk_drawing_area_new ();
   
    /* Ask for a 200x200 window, but the AspectFrame will give us a 200x100
     * window since we are forcing a 2x1 aspect ratio */
    gtk_widget_set_size_request (drawing_area, 200, 200);
    gtk_container_add (GTK_CONTAINER (aspect_frame), drawing_area);
    gtk_widget_show (drawing_area);
   
    gtk_widget_show (window);
    gtk_main ();
    return 0;
}
