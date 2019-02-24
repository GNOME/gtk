#include <gtk/gtk.h>

static void
draw_popup (GtkDrawingArea  *da,
            cairo_t         *cr,
            int              width,
            int              height,
            gpointer         data)
{
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
}

static gboolean
create_popup (GtkWidget *parent,
              GtkWidget *label)
{
  GtkWidget *popup, *da;

  popup = gtk_popup_new ();
  gtk_popup_set_relative_to (GTK_POPUP (popup), label);
  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), draw_popup, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (popup), da);

  gtk_widget_set_size_request (GTK_WIDGET (popup), 20, 20);

  gtk_widget_show (popup);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *label;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);

  label = gtk_label_new ("x");
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (window), label);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "map", G_CALLBACK (create_popup), label);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
