#include <gtk/gtk.h>

static void scale_moved(GtkWidget *widget, gpointer data)
{
    g_print("%.1f\n", gtk_adjustment_get_value(GTK_ADJUSTMENT(data)));
}

int main(int argc, char **argv)
{
    GtkWidget *window, *vbox, *toolbar, *scale;
    GtkAdjustment *adjustment;
    GtkStyleContext *context;
    GtkToolItem *boxitem, *button;

    gtk_init(&argc, &argv);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);

    toolbar = gtk_toolbar_new();
    context = gtk_widget_get_style_context(toolbar);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

    adjustment = gtk_adjustment_new(0, 0, 10, 1, 2, 0);
    scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_widget_set_size_request(scale, 255, -1);

    button = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);

    boxitem = gtk_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, 0);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), boxitem, 1);

    gtk_container_add(GTK_CONTAINER(vbox), toolbar);
    gtk_container_add(GTK_CONTAINER(boxitem), scale);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(button, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(scale, "value-changed", G_CALLBACK(scale_moved), adjustment);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}

