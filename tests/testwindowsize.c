/*  gcc -g -Wall -O2 -o dialog-test dialog-test.c `pkg-config --cflags --libs gtk+-3.0` */
#include <gtk/gtk.h>

static GtkWidget *window;
static GtkWidget *width_chars_spin;
static GtkWidget *max_width_chars_spin;
static GtkWidget *default_width_spin;
static GtkWidget *default_height_spin;
static GtkWidget *resizable_check;

static gboolean
configure_event_cb (GtkWidget *window, GdkEventConfigure *event, GtkLabel *label)
{
  gchar *str;
  gint width, height;

  gtk_window_get_size (GTK_WINDOW (window), &width, &height);
  str = g_strdup_printf ("%d x %d", width, height);
  gtk_label_set_label (label, str);
  g_free (str);

  return FALSE;
}

static void
show_dialog (void)
{
  GtkWidget *dialog;
  GtkWidget *label;
  gint width_chars, max_width_chars, default_width, default_height;
  gboolean resizable;

  width_chars = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (width_chars_spin));
  max_width_chars = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (max_width_chars_spin));
  default_width = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (default_width_spin));
  default_height = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (default_height_spin));
  resizable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (resizable_check));

  dialog = gtk_dialog_new_with_buttons ("Test", GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL,
                                        "_Close", GTK_RESPONSE_CANCEL,
                                        NULL);

  label = gtk_label_new ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                         "Nulla innn urna ac dui malesuada ornare. Nullam dictum "
                         "tempor mi et tincidunt. Aliquam metus nulla, auctor "
                         "vitae pulvinar nec, egestas at mi. Class aptent taciti "
                         "sociosqu ad litora torquent per conubia nostra, per "
                         "inceptos himenaeos. Aliquam sagittis, tellus congue "
                         "cursus congue, diam massa mollis enim, sit amet gravida "
                         "magna turpis egestas sapien. Aenean vel molestie nunc. "
                         "In hac habitasse platea dictumst. Suspendisse lacinia"
                         "mi eu ipsum vestibulum in venenatis enim commodo. "
                         "Vivamus non malesuada ligula.");

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), width_chars);
  gtk_label_set_max_width_chars (GTK_LABEL (label), max_width_chars);
  gtk_window_set_default_size (GTK_WINDOW (dialog), default_width, default_height);
  gtk_window_set_resizable (GTK_WINDOW (dialog), resizable);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      label, 0, TRUE, TRUE);
  gtk_widget_show (label);

  label = gtk_label_new ("? x ?");
  //gtk_widget_show (label);

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), label, GTK_RESPONSE_HELP);
  g_signal_connect (dialog, "configure-event",
                    G_CALLBACK (configure_event_cb), label);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}

static void
create_window (void)
{
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *button;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Window size");
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (window), grid);

  label = gtk_label_new ("Width chars");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  width_chars_spin = gtk_spin_button_new_with_range (-1, 1000, 1);
  gtk_widget_set_halign (width_chars_spin, GTK_ALIGN_START);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), width_chars_spin, 1, 0, 1, 1);

  label = gtk_label_new ("Max width chars");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  max_width_chars_spin = gtk_spin_button_new_with_range (-1, 1000, 1);
  gtk_widget_set_halign (width_chars_spin, GTK_ALIGN_START);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), max_width_chars_spin, 1, 1, 1, 1);

  label = gtk_label_new ("Default size");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  default_width_spin = gtk_spin_button_new_with_range (-1, 1000, 1);
  gtk_widget_set_halign (default_width_spin, GTK_ALIGN_START);
  default_height_spin = gtk_spin_button_new_with_range (-1, 1000, 1);
  gtk_widget_set_halign (default_height_spin, GTK_ALIGN_START);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), default_width_spin, 1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), default_height_spin, 2, 2, 1, 1);

  label = gtk_label_new ("Resizable");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  resizable_check = gtk_check_button_new ();
  gtk_widget_set_halign (resizable_check, GTK_ALIGN_START);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), resizable_check, 1, 3, 1, 1);

  button = gtk_button_new_with_label ("Show");
  g_signal_connect (button, "clicked", G_CALLBACK (show_dialog), NULL);
  gtk_grid_attach (GTK_GRID (grid), button, 2, 4, 1, 1);

  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (NULL, NULL);

  create_window ();

  gtk_main ();

  return 0;
}
