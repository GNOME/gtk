
#include <gtk/gtk.h>

#define EVENT_METHOD(i, x) GTK_WIDGET_GET_CLASS(i)->x

#define XSIZE  600
#define YSIZE  400

/* This routine gets control when the close button is clicked */
static gboolean close_application( GtkWidget *widget,
                                   GdkEvent  *event,
                                   gpointer   data )
{
    gtk_main_quit ();
    return FALSE;
}

/* The main routine */
int main( int   argc,
          char *argv[] ) {
    GtkWidget *window, *table, *area, *hrule, *vrule;

    /* Initialize GTK and create the main window */
    gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "delete-event",
                      G_CALLBACK (close_application), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    /* Create a table for placing the ruler and the drawing area */
    table = gtk_table_new (3, 2, FALSE);
    gtk_container_add (GTK_CONTAINER (window), table);

    area = gtk_drawing_area_new ();
    gtk_widget_set_size_request (GTK_WIDGET (area), XSIZE, YSIZE);
    gtk_table_attach (GTK_TABLE (table), area, 1, 2, 1, 2,
                      GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_set_events (area, GDK_POINTER_MOTION_MASK |
                                 GDK_POINTER_MOTION_HINT_MASK);

    /* The horizontal ruler goes on top. As the mouse moves across the
     * drawing area, a motion_notify_event is passed to the
     * appropriate event handler for the ruler. */
    hrule = gtk_hruler_new ();
    gtk_ruler_set_metric (GTK_RULER (hrule), GTK_PIXELS);
    gtk_ruler_set_range (GTK_RULER (hrule), 7, 13, 0, 20);
    g_signal_connect_swapped (area, "motion-notify-event",
                              G_CALLBACK (EVENT_METHOD (hrule,
                                                        motion_notify_event)),
                              hrule);
    gtk_table_attach (GTK_TABLE (table), hrule, 1, 2, 0, 1,
                      GTK_EXPAND|GTK_SHRINK|GTK_FILL, GTK_FILL, 0, 0);

    /* The vertical ruler goes on the left. As the mouse moves across
     * the drawing area, a motion_notify_event is passed to the
     * appropriate event handler for the ruler. */
    vrule = gtk_vruler_new ();
    gtk_ruler_set_metric (GTK_RULER (vrule), GTK_PIXELS);
    gtk_ruler_set_range (GTK_RULER (vrule), 0, YSIZE, 10, YSIZE );
    g_signal_connect_swapped (area, "motion-notify-event",
                              G_CALLBACK (EVENT_METHOD (vrule,
                                                        motion_notify_event)),
                              vrule);
    gtk_table_attach (GTK_TABLE (table), vrule, 0, 1, 1, 2,
                      GTK_FILL, GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0);

    /* Now show everything */
    gtk_widget_show (area);
    gtk_widget_show (hrule);
    gtk_widget_show (vrule);
    gtk_widget_show (table);
    gtk_widget_show (window);
    gtk_main ();

    return 0;
}
