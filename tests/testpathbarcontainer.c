#include "config.h"
#include "glib.h"
#include <gtk/gtk.h>
#define GTK_COMPILATION
#include <gtk/gtkpathbarcontainer.h>
#include <gtk/gtkpathbarcontainer.c>
#include <gtk/gtkpathbarboxprivate.h>
#include <gtk/gtkpathbarbox.c>

#define N_BUTTONS 10

static GtkWidget *path_bar_container;
static GtkWidget *path_bar_box;
static char *lorem_ipsum = "Loremipsumdolorsitamet, consecteturadipisicingelit,";

static char*
get_lorem_ipsum ()
{
  static char **lorem_ipsum_split;
  static int n_lorem_ipsum_words;

  if (!lorem_ipsum_split)
    {
      lorem_ipsum_split = g_strsplit (lorem_ipsum, " ", -1);
      n_lorem_ipsum_words = g_strv_length (lorem_ipsum_split);
    }

  return lorem_ipsum_split [g_random_int_range (0, n_lorem_ipsum_words)];
}

static void
on_button_clicked (GtkWidget *button,
                   gpointer   user_data)
{
  g_print ("button clicked\n");
  gtk_path_bar_container_remove (GTK_PATH_BAR_CONTAINER (user_data), button, TRUE);
}

static void
on_reset_button_clicked (GtkButton *reset_button)
{
  GtkWidget *button;

  gtk_path_bar_container_remove_all_children (GTK_PATH_BAR_CONTAINER (path_bar_container));

  for (int i = 0; i < N_BUTTONS; i++)
    {
      button = gtk_button_new_with_label (get_lorem_ipsum ());
      g_signal_connect (button, "clicked", (GCallback) on_button_clicked, path_bar_container);
      gtk_path_bar_container_add (GTK_PATH_BAR_CONTAINER (path_bar_container), button, FALSE);
    }

  gtk_widget_show_all (path_bar_container);
}

static void
on_add_button (gint line)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (get_lorem_ipsum ());
  gtk_widget_show (button);
  g_signal_connect (button, "clicked", (GCallback) on_button_clicked, path_bar_container);
  gtk_path_bar_container_add (GTK_PATH_BAR_CONTAINER (path_bar_container), button, TRUE);
  gtk_container_add (GTK_CONTAINER (path_bar_box), gtk_button_new_with_label ("eeeeoo"));
}

static void
on_remove_button (gint line)
{
  GList *children;
  GList *last;

  children = gtk_path_bar_container_get_children (GTK_PATH_BAR_CONTAINER (path_bar_container));
  last = g_list_last (children);
  if (last)
    gtk_path_bar_container_remove (GTK_PATH_BAR_CONTAINER (path_bar_container),
                                   last->data, TRUE);

  g_list_free (children);
}

static void
on_invert_button (gint line)
{
  gtk_path_bar_container_set_inverted (GTK_PATH_BAR_CONTAINER (path_bar_container),
                               !gtk_path_bar_container_get_inverted (GTK_PATH_BAR_CONTAINER (path_bar_container)));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *reset_button;
  GtkWidget *add_button;
  GtkWidget *remove_button;
  GtkWidget *invert_button;
  GtkWidget *label;

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
  g_type_ensure (GTK_TYPE_PATH_BAR_CONTAINER);
  g_type_ensure (GTK_TYPE_PATH_BAR_BOX);

  label = gtk_label_new ("Generic GtkPathBar tests");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

  /* ----------------------------------------------------------------------- */
  path_bar_container = gtk_path_bar_container_new ();
  path_bar_box = gtk_path_bar_box_new ();
  gtk_container_add (GTK_CONTAINER (path_bar_box), path_bar_container);
  gtk_grid_attach (GTK_GRID (grid), path_bar_box, 0, 1, 1, 1);
  gtk_widget_show_all (path_bar_box);
  /* Add/Remove buttons */
  add_button = gtk_button_new_with_label ("Add");
  gtk_widget_set_halign (add_button, GTK_ALIGN_END);
  remove_button = gtk_button_new_with_label ("Remove");
  gtk_widget_set_halign (remove_button, GTK_ALIGN_END);
  gtk_grid_attach_next_to (GTK_GRID (grid), add_button, path_bar_box, GTK_POS_RIGHT, 1, 1);
  g_signal_connect_swapped (add_button, "clicked", (GCallback) on_add_button, GINT_TO_POINTER (0));
  gtk_grid_attach_next_to (GTK_GRID (grid), remove_button, add_button, GTK_POS_RIGHT, 1, 1);
  g_signal_connect_swapped (remove_button, "clicked", (GCallback) on_remove_button, GINT_TO_POINTER (0));
  gtk_widget_show (add_button);
  gtk_widget_show (remove_button);
  /* Inverted button */
  invert_button = gtk_button_new_with_label ("Invert");
  gtk_widget_set_halign (invert_button, GTK_ALIGN_END);
  gtk_grid_attach_next_to (GTK_GRID (grid), invert_button, remove_button, GTK_POS_RIGHT, 1, 1);
  g_signal_connect_swapped (invert_button, "clicked", (GCallback) on_invert_button, GINT_TO_POINTER (0));

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
