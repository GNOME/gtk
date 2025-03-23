
#include <gtk/gtk.h>

static const char *names[] = {
  "none",
  "default",
  "help",
  "pointer",
  "context-menu",
  "progress",
  "wait",
  "cell",
  "crosshair",
  "text",
  "vertical-text",
  "alias",
  "copy",
  "move",
  "dnd-ask",
  "no-drop",
  "not-allowed",
  "grab",
  "grabbing",
  "n-resize",
  "e-resize",
  "s-resize",
  "w-resize",
  "ne-resize",
  "nw-resize",
  "sw-resize",
  "se-resize",
  "col-resize",
  "row-resize",
  "ew-resize",
  "ns-resize",
  "nesw-resize",
  "nwse-resize",
  "all-resize",
  "all-scroll",
  "zoom-in",
  "zoom-out"
};

static int count = 0;

static gboolean
change_cursor (gpointer data)
{
  GtkWidget *window = data;
  GtkWidget *label;
  char buffer[128];

  gtk_widget_set_cursor_from_name (window, names[count % G_N_ELEMENTS (names)]);
  count++;

  label = gtk_window_get_child (GTK_WINDOW (window));
  g_snprintf (buffer, sizeof (buffer), "%d", count);
  gtk_label_set_label (GTK_LABEL (label), buffer);

  return G_SOURCE_CONTINUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  label = gtk_label_new ("");
  gtk_window_set_child (GTK_WINDOW (window), label);

  g_timeout_add (4, change_cursor, window);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
