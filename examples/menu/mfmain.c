/* This file extracted from the GTK tutorial. */

/* mfmain.c */

#include <gtk/gtk.h>

#include "mfmain.h"
#include "menufactory.h"


int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *main_vbox;
    GtkWidget *menubar;
    
    GtkAcceleratorTable *accel;
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_signal_connect(GTK_OBJECT(window), "destroy", 
		       GTK_SIGNAL_FUNC(file_quit_cmd_callback), 
		       "WM destroy");
    gtk_window_set_title(GTK_WINDOW(window), "Menu Factory");
    gtk_widget_set_usize(GTK_WIDGET(window), 300, 200);
    
    main_vbox = gtk_vbox_new(FALSE, 1);
    gtk_container_border_width(GTK_CONTAINER(main_vbox), 1);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);
    gtk_widget_show(main_vbox);
    
    get_main_menu(&menubar, &accel);
    gtk_window_add_accelerator_table(GTK_WINDOW(window), accel);
    gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, TRUE, 0);
    gtk_widget_show(menubar);
    
    gtk_widget_show(window);
    gtk_main();
    
    return(0);
}

/* This is just to demonstrate how callbacks work when using the
 * menufactory.  Often, people put all the callbacks from the menus
 * in a separate file, and then have them call the appropriate functions
 * from there.  Keeps it more organized. */
void file_quit_cmd_callback (GtkWidget *widget, gpointer data)
{
    g_print ("%s\n", (char *) data);
    gtk_exit(0);
}
