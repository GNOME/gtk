#include "config.h"
#include "glib.h"
#include <gtk/gtk.h>
#include <gtk/gtkhidingboxprivate.h>

static GtkWidget *hiding_box;

static void
on_path_selected (GtkPathBar *path_bar,
                  GParamSpec *pspec,
                  gpointer   *user_data)
{
  g_print ("Path selected: %s\n", gtk_path_bar_get_selected_path (path_bar));
}

static void
on_button_clicked (GtkWidget *button,
                   gpointer   user_data)
{
  gtk_container_remove (GTK_CONTAINER (user_data), button);
}

static void
on_reset_button_clicked (GtkButton *reset_button)
{
  GtkWidget *button;

  gtk_container_foreach (GTK_CONTAINER (hiding_box), gtk_widget_destroy);

  button = gtk_button_new_with_label ("test1");
  g_signal_connect (button, "clicked", on_button_clicked, hiding_box);
  gtk_container_add (GTK_CONTAINER (hiding_box), );
  gtk_container_add (GTK_CONTAINER (hiding_box), gtk_button_new_with_label ("test2"));
  gtk_container_add (GTK_CONTAINER (hiding_box), gtk_button_new_with_label ("test3"));
  gtk_container_add (GTK_CONTAINER (hiding_box), gtk_button_new_with_label ("test4"));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *reset_button;
  GtkWidget *label;
  GFile *file = NULL;
  GIcon *icon;

  gtk_init (&argc, &argv);

  window = g_object_connect (g_object_new (gtk_window_get_type (),
                                           "type", GTK_WINDOW_TOPLEVEL,
                                           "title", "Test path bar",
                                           "resizable", TRUE,
                                           "default-height", 200,
                                           NULL),
                             "signal::destroy", gtk_main_quit, NULL,
                             NULL);

  grid = gtk_grid_new ();
  g_type_ensure (GTK_TYPE_HIDING_BOX);

  label = gtk_label_new ("Generic GtkPathBar tests");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

  /* ----------------------------------------------------------------------- */
  hiding_box = gtk_hiding_box ();
  gtk_grid_attach (GTK_GRID (grid), hiding_box, 0, 1, 1, 1);

  /* Reset button */
  reset_button = gtk_button_new_with_label ("Reset State");
  gtk_widget_set_hexpand (reset_button, TRUE);
  g_signal_connect (GTK_BUTTON (reset_button), "clicked",
                    G_CALLBACK (on_reset_button_clicked), window);
  gtk_grid_attach (GTK_GRID (grid), reset_button, 0, 11, 2, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
