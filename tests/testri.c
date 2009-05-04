#include <gtk/gtk.h>

int
main (int argc, char **argv)
{
    GError *error = NULL;

    gtk_enable_resolution_independence ();
    gtk_init (&argc, &argv);

    GtkBuilder *ui = gtk_builder_new ();
    gtk_builder_add_from_file (ui, "testri.ui", &error);
    if (error)
    {
        g_error ("%s:%i: %s", __FILE__, __LINE__, error->message);
    }

    GtkWidget *window = GTK_WIDGET (gtk_builder_get_object (ui, "testri-window"));

    gtk_widget_show (window);

    g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

    /* get the image sizes */
    GtkWidget *image1 = GTK_WIDGET (gtk_builder_get_object (ui, "image1"));
    GtkWidget *image2 = GTK_WIDGET (gtk_builder_get_object (ui, "image2"));
    GtkWidget *image3 = GTK_WIDGET (gtk_builder_get_object (ui, "image3"));

    GtkSize size;
    char *str;

    gtk_widget_get_unit (image1, "pixel-size", &size, NULL);
    str = gtk_size_to_string (size);
    g_print ("image1 size = %s\n", str);
    g_free (str);

    gtk_widget_get_unit (image2, "pixel-size", &size, NULL);
    str = gtk_size_to_string (size);
    g_print ("image2 size = %s\n", str);
    g_free (str);
    
    gtk_widget_get_unit (image3, "pixel-size", &size, NULL);
    str = gtk_size_to_string (size);
    g_print ("image3 size = %s\n", str);
    g_free (str);

    gtk_main ();

    return 0;
}
