/* example-start menu menufactory.c */

#include <gtk/gtk.h>
#include <strings.h>

#include "mfmain.h"

static void print_hello(GtkWidget *widget, gpointer data);


/* this is the GtkMenuEntry structure used to create new menus.  The
 * first member is the menu definition string.  The second, the
 * default accelerator key used to access this menu function with
 * the keyboard.  The third is the callback function to call when
 * this menu item is selected (by the accelerator key, or with the
 * mouse.) The last member is the data to pass to your callback function.
 */

static GtkMenuEntry menu_items[] =
{
    {"<Main>/File/New", "<control>N", print_hello, NULL},
    {"<Main>/File/Open", "<control>O", print_hello, NULL},
    {"<Main>/File/Save", "<control>S", print_hello, NULL},
    {"<Main>/File/Save as", NULL, NULL, NULL},
    {"<Main>/File/<separator>", NULL, NULL, NULL},
    {"<Main>/File/Quit", "<control>Q", file_quit_cmd_callback, "OK, I'll quit"},
    {"<Main>/Options/Test", NULL, NULL, NULL}
};


static void
print_hello(GtkWidget *widget, gpointer data)
{
    printf("hello!\n");
}

void get_main_menu(GtkWidget *window, GtkWidget ** menubar)
{
    int nmenu_items = sizeof(menu_items) / sizeof(menu_items[0]);
    GtkMenuFactory *factory;
    GtkMenuFactory *subfactory;

    factory = gtk_menu_factory_new(GTK_MENU_FACTORY_MENU_BAR);
    subfactory = gtk_menu_factory_new(GTK_MENU_FACTORY_MENU_BAR);

    gtk_menu_factory_add_subfactory(factory, subfactory, "<Main>");
    gtk_menu_factory_add_entries(factory, menu_items, nmenu_items);
    gtk_window_add_accelerator_table(GTK_WINDOW(window), subfactory->table);
    
    if (menubar)
        *menubar = subfactory->widget;
}

/* example-end */
