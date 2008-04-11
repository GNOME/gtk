#include <gtk/gtk.h>

static void
draw_page_cb(GtkPrintOperation *operation, GtkPrintContext *context,
             gint page_nr, gpointer user_data)
{
    cairo_t *cr = gtk_print_context_get_cairo_context(context);

    cairo_rectangle(cr, 50, 50, 50, 50);
    cairo_stroke(cr);
}


int
main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    GtkPrintOperation *print;

    print = gtk_print_operation_new();
    gtk_print_operation_set_use_full_page(print, FALSE);
    gtk_print_operation_set_unit(print, GTK_UNIT_POINTS);
    gtk_print_operation_set_n_pages(print, 1);
    g_signal_connect(print, "draw_page", G_CALLBACK(draw_page_cb), NULL);

    gtk_print_operation_run(print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                            NULL, NULL);
    g_object_unref(print);

    return 0;
}
