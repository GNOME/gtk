/* This file extracted from the GTK tutorial. */

/* buttons.c */

#include <gtk/gtk.h>

/* create a new hbox with an image and a label packed into it
 * and return the box.. */

GtkWidget *xpm_label_box (GtkWidget *parent, gchar *xpm_filename, gchar *label_text)
{
    GtkWidget *box1;
    GtkWidget *label;
    GtkWidget *pixmapwid;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    GtkStyle *style;

    /* create box for xpm and label */
    box1 = gtk_hbox_new (FALSE, 0);
    gtk_container_border_width (GTK_CONTAINER (box1), 2);

    /* get style of button.. I assume it's to get the background color.
     * if someone knows the real reason, please enlighten me. */
    style = gtk_widget_get_style(parent);

    /* now on to the xpm stuff.. load xpm */
    pixmap = gdk_pixmap_create_from_xpm (parent->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 xpm_filename);
    pixmapwid = gtk_pixmap_new (pixmap, mask);

    /* create label for button */
    label = gtk_label_new (label_text);

    /* pack the pixmap and label into the box */
    gtk_box_pack_start (GTK_BOX (box1),
			pixmapwid, FALSE, FALSE, 3);

    gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 3);

    gtk_widget_show(pixmapwid);
    gtk_widget_show(label);

    return (box1);
}

/* our usual callback function */
void callback (GtkWidget *widget, gpointer data)
{
    g_print ("Hello again - %s was pressed\n", (char *) data);
}


int main (int argc, char *argv[])
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box1;

    gtk_init (&argc, &argv);

    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (window), "Pixmap'd Buttons!");

    /* It's a good idea to do this for all windows. */
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC (gtk_exit), NULL);

    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC (gtk_exit), NULL);


    /* sets the border width of the window. */
    gtk_container_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_realize(window);

    /* create a new button */
    button = gtk_button_new ();

    /* You should be getting used to seeing most of these functions by now */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (callback), (gpointer) "cool button");

    /* this calls our box creating function */
    box1 = xpm_label_box(window, "info.xpm", "cool button");

    /* pack and show all our widgets */
    gtk_widget_show(box1);

    gtk_container_add (GTK_CONTAINER (button), box1);

    gtk_widget_show(button);

    gtk_container_add (GTK_CONTAINER (window), button);

    gtk_widget_show (window);

    /* rest in gtk_main and wait for the fun to begin! */
    gtk_main ();

    return 0;
}
