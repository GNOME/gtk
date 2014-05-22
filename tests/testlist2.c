#include <gtk/gtk.h>

static void
row_unrevealed (GObject *revealer, GParamSpec *pspec, gpointer data)
{
  GtkWidget *row, *list;

  row = gtk_widget_get_parent (GTK_WIDGET (revealer));
  list = gtk_widget_get_parent (row);

  gtk_container_remove (GTK_CONTAINER (list), row);
}

static void
remove_this_row (GtkButton *button, GtkWidget *child)
{
  GtkWidget *row, *revealer;

  row = gtk_widget_get_parent (child);
  revealer = gtk_revealer_new ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
  gtk_widget_show (revealer);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_widget_reparent (child, revealer);
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_container_add (GTK_CONTAINER (row), revealer);
  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (row_unrevealed), NULL);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
}

static GtkWidget *create_row (const gchar *label);

static void
row_revealed (GObject *revealer, GParamSpec *pspec, gpointer data)
{
  GtkWidget *row, *child;

  row = gtk_widget_get_parent (GTK_WIDGET (revealer));
  child = gtk_bin_get_child (GTK_BIN (revealer));
  g_object_ref (child);
  gtk_container_remove (GTK_CONTAINER (revealer), child);
  gtk_widget_destroy (GTK_WIDGET (revealer));
  gtk_container_add (GTK_CONTAINER (row), child);
  g_object_unref (child);
}

static void
add_row_below (GtkButton *button, GtkWidget *child)
{
  GtkWidget *revealer, *row, *list;
  gint index;

  row = gtk_widget_get_parent (child);
  index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));
  list = gtk_widget_get_parent (row);
  row = create_row ("Extra row");
  revealer = gtk_revealer_new ();
  gtk_container_add (GTK_CONTAINER (revealer), row);
  gtk_widget_show_all (revealer);
  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (row_revealed), NULL);
  gtk_list_box_insert (GTK_LIST_BOX (list), revealer, index + 1);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
}

static void
add_separator (GtkListBoxRow *row, GtkListBoxRow *before, gpointer data)
{
  if (!before)
    return;

  gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static GtkWidget *
create_row (const gchar *text)
{
  GtkWidget *row, *label, *button;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (row), label);
  button = gtk_button_new_with_label ("x");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (row), button);
  g_signal_connect (button, "clicked", G_CALLBACK (remove_this_row), row);
  button = gtk_button_new_with_label ("+");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (row), button);
  g_signal_connect (button, "clicked", G_CALLBACK (add_row_below), row);

  return row;
}

int main (int argc, char *argv[])
{
  GtkWidget *window, *list, *sw, *row;
  gint i;
  gchar *text;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (list), add_separator, NULL, NULL);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (window), sw);
  gtk_container_add (GTK_CONTAINER (sw), list);

  for (i = 0; i < 20; i++)
    {
      text = g_strdup_printf ("Row %d", i);
      row = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
