#include <gtk/gtk.h>



int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkLabel *label1;
  GtkLabel *label2;
  GtkLabel *label3;
  GtkLabel *label4;

  g_setenv ("GTK_DEBUG", "baselines,layout", TRUE);
  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 30);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 30);
  gtk_container_add (GTK_CONTAINER (window), grid);

  label1 = gtk_label_new ("Some Text");
  label2 = gtk_label_new ("QQQQQQQQQ");
  label3 = gtk_label_new ("Some Text");
  label4 = gtk_label_new ("Some Text");

  g_message ("label1: %p", label1);
  g_message ("label2: %p", label2);
  g_message ("label3: %p", label3);
  g_message ("label4: %p", label4);

  gtk_widget_set_valign (GTK_WIDGET (label1), GTK_ALIGN_BASELINE);
  gtk_widget_set_valign (GTK_WIDGET (label2), GTK_ALIGN_BASELINE);
  gtk_widget_set_valign (GTK_WIDGET (label3), GTK_ALIGN_START);
  gtk_widget_set_valign (GTK_WIDGET (label4), GTK_ALIGN_START);

  gtk_widget_set_margin_top (GTK_WIDGET (label1), 12);
  gtk_widget_set_margin_bottom (GTK_WIDGET (label2), 18);
  gtk_widget_set_margin_top (GTK_WIDGET (label3), 30);


  /*
   * Since none of the widgets in the second row request baseline alignment,
   * GtkGrid should not compute or apply a baseline for those widgets.
   */


  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label1), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label2), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label3), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label4), 1, 1, 1, 1);

  gtk_widget_show (window);
  gtk_main ();
  return 0;
}
