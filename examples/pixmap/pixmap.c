
#include "config.h"
#include <gtk/gtk.h>


/* XPM data of Open-File icon */
static const char * xpm_data[] = {
"16 16 3 1",
"       c None",
".      c #000000000000",
"X      c #FFFFFFFFFFFF",
"                ",
"   ......       ",
"   .XXX.X.      ",
"   .XXX.XX.     ",
"   .XXX.XXX.    ",
"   .XXX.....    ",
"   .XXXXXXX.    ",
"   .XXXXXXX.    ",
"   .XXXXXXX.    ",
"   .XXXXXXX.    ",
"   .XXXXXXX.    ",
"   .XXXXXXX.    ",
"   .XXXXXXX.    ",
"   .........    ",
"                ",
"                "};


/* when invoked (via signal delete_event), terminates the application.
 */
gint close_application( GtkWidget *widget,
                        GdkEvent  *event,
                        gpointer   data )
{
    gtk_main_quit ();
    return FALSE;
}


/* is invoked when the button is clicked.  It just prints a message.
 */
void button_clicked( GtkWidget *widget,
                     gpointer   data ) {
    g_print ("button clicked\n");
}

int main( int   argc,
          char *argv[] )
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window, *pixmapwid, *button;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    GtkStyle *style;

    /* create the main window, and attach delete_event signal to terminating
       the application */
    gtk_init (&argc, &argv);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "delete-event",
                      G_CALLBACK (close_application), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_show (window);

    /* now for the pixmap from gdk */
    style = gtk_widget_get_style (window);
    pixmap = gdk_pixmap_create_from_xpm_d (window->window,  &mask,
                                           &style->bg[GTK_STATE_NORMAL],
                                           (gchar **)xpm_data);

    /* a pixmap widget to contain the pixmap */
    pixmapwid = gtk_image_new_from_pixmap (pixmap, mask);
    gtk_widget_show (pixmapwid);

    /* a button to contain the pixmap widget */
    button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button), pixmapwid);
    gtk_container_add (GTK_CONTAINER (window), button);
    gtk_widget_show (button);

    g_signal_connect (button, "clicked",
                      G_CALLBACK (button_clicked), NULL);

    /* show the window */
    gtk_main ();

    return 0;
}
