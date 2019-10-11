/* Benchmark/Icons
 *
 * This demo scrolls a view with many icons.
 */

#include <gtk/gtk.h>

static guint tick_cb;
static GtkAdjustment *adjustment;
int increment = 5;

static gboolean
scroll_cb (GtkWidget *widget,
           GdkFrameClock *frame_clock,
           gpointer data)
{
  double value;

  value = gtk_adjustment_get_value (adjustment);
  if (value + increment <= gtk_adjustment_get_lower (adjustment) ||
     (value + increment >= gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment)))
    increment = - increment;

  gtk_adjustment_set_value (adjustment, value + increment);

  return G_SOURCE_CONTINUE;
}

extern GtkWidget *create_icon (void);

static void
populate (GtkWidget *grid)
{
  int top, left;

  for (top = 0; top < 100; top++)
    for (left = 0; left < 15; left++)
      gtk_grid_attach (GTK_GRID (grid), create_icon (), left, top, 1, 1);
}

GtkWidget *
do_iconscroll (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *grid;

      builder = gtk_builder_new_from_resource ("/iconscroll/iconscroll.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      grid = GTK_WIDGET (gtk_builder_get_object (builder, "grid"));
      populate (grid);
      gtk_widget_realize (window);
      adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "adjustment"));
      tick_cb = gtk_widget_add_tick_callback (window, scroll_cb, NULL, NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
