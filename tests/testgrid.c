#include <gtk/gtk.h>

static GtkWidget *
oriented_test_widget (const gchar *label, const gchar *color, gdouble angle)
{
  GtkWidget *box;
  GtkWidget *widget;
  GdkRGBA c;

  widget = gtk_label_new (label);
  gtk_label_set_angle (GTK_LABEL (widget), angle);
  box = gtk_event_box_new ();
  gdk_rgba_parse (&c, color);
  gtk_widget_override_background_color (box, 0, &c);
  gtk_container_add (GTK_CONTAINER (box), widget);

  return box;
}

static GtkWidget *
test_widget (const gchar *label, const gchar *color)
{
  return oriented_test_widget (label, color, 0.0);
}

static GtkOrientation o;

static gboolean
toggle_orientation (GtkWidget *window, GdkEventButton *event, GtkGrid *grid)
{
  o = 1 - o;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), o);

  return FALSE;
}

static void
simple_grid (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *test1, *test2, *test3, *test4, *test5, *test6;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Orientation");
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);
  g_signal_connect (window, "button-press-event", G_CALLBACK (toggle_orientation), grid);

  gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
  test1 = test_widget ("1", "red");
  gtk_container_add (GTK_CONTAINER (grid), test1);
  test2 = test_widget ("2", "green");
  gtk_container_add (GTK_CONTAINER (grid), test2);
  test3 = test_widget ("3", "blue");
  gtk_container_add (GTK_CONTAINER (grid), test3);
  test4 = test_widget ("4", "green");
  gtk_grid_attach (GTK_GRID (grid), test4, 0, 1, 1, 1);
  gtk_widget_set_vexpand (test4, TRUE);
  test5 = test_widget ("5", "blue");
  gtk_grid_attach_next_to (GTK_GRID (grid), test5, test4, GTK_POS_RIGHT, 2, 1);
  test6 = test_widget ("6", "yellow");
  gtk_grid_attach (GTK_GRID (grid), test6, -1, 0, 1, 2);
  gtk_widget_set_hexpand (test6, TRUE);

  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 0, -1) == NULL);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 0, 0) == test1);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 1, 0) == test2);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 0, 1) == test4);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), -1, 0) == test6);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), -1, 1) == test6);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), -1, 2) == NULL);
  gtk_widget_show_all (window);
}

static void
text_grid (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *paned1;
  GtkWidget *box;
  GtkWidget *label;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Height-for-Width");
  paned1 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (window), paned1);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_paned_pack1 (GTK_PANED (paned1), box, TRUE, FALSE);
  gtk_paned_pack2 (GTK_PANED (paned1), gtk_label_new ("Space"), TRUE, FALSE);

  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("Above"));
  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (box), grid);
  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("Below"));

  label = gtk_label_new ("Some text that may wrap if it has to");
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("1", "red"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("2", "blue"), 0, 1, 1, 1);

  label = gtk_label_new ("Some text that may wrap if it has to");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

  gtk_widget_show_all (window);
}

static void
box_comparison (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *grid;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Grid vs. Box");
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  gtk_container_add (GTK_CONTAINER (vbox), gtk_label_new ("Above"));
  gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (vbox), box);

  gtk_box_pack_start (GTK_BOX (box), test_widget ("1", "white"), FALSE, FALSE, 0);

  label = gtk_label_new ("Some ellipsizing text");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box), test_widget ("2", "green"), FALSE, FALSE, 0);

  label = gtk_label_new ("Some text that may wrap if needed");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box), test_widget ("3", "red"), FALSE, FALSE, 0);

  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (vbox), grid);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("1", "white"), 0, 0, 1, 1);

  label = gtk_label_new ("Some ellipsizing text");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("2", "green"), 2, 0, 1, 1);

  label = gtk_label_new ("Some text that may wrap if needed");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_grid_attach (GTK_GRID (grid), label, 3, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("3", "red"), 4, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (vbox), gtk_label_new ("Below"));

  gtk_widget_show_all (window);
}

static void
empty_line (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *child;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Empty row");
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  child = test_widget ("(0, 0)", "red");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "blue"), 0, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(10, 0)", "green"), 10, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(10, 1)", "magenta"), 10, 1, 1, 1);

  gtk_widget_show_all (window);
}

static void
empty_grid (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *child;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Empty grid");
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);

  child = test_widget ("(0, 0)", "red");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);

  gtk_widget_show_all (window);
  gtk_widget_hide (child);
}

static void
scrolling (void)
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *viewport;
  GtkWidget *grid;
  GtkWidget *child;
  gint i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Scrolling");
  sw = gtk_scrolled_window_new (NULL, NULL);
  viewport = gtk_viewport_new (NULL, NULL);
  grid = gtk_grid_new ();

  gtk_container_add (GTK_CONTAINER (window), sw);
  gtk_container_add (GTK_CONTAINER (sw), viewport);
  gtk_container_add (GTK_CONTAINER (viewport), grid);

  child = oriented_test_widget ("#800080", "#800080", -45.0);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);

  for (i = 1; i < 16; i++)
    {
      gchar *color;
      color = g_strdup_printf ("#%02x00%02x", 128 + 8*i, 128 - 8*i);
      child = test_widget (color, color);
      gtk_grid_attach (GTK_GRID (grid), child, 0, i, i + 1, 1);
      gtk_widget_set_hexpand (child, TRUE);
      g_free (color);
    }

  for (i = 1; i < 16; i++)
    {
      gchar *color;
      color = g_strdup_printf ("#%02x00%02x", 128 - 8*i, 128 + 8*i);
      child = oriented_test_widget (color, color, -90.0);
      gtk_grid_attach (GTK_GRID (grid), child, i, 0, 1, i);
      gtk_widget_set_vexpand (child, TRUE);
      g_free (color);
    }

  gtk_widget_show_all (window);
}

static void
insert (void)
{
  GtkWidget *window;
  GtkWidget *g;
  GtkWidget *grid;
  GtkWidget *child;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Insertion");

  g = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (g), 10);
  gtk_grid_set_column_spacing (GTK_GRID (g), 10);
  gtk_container_add (GTK_CONTAINER (window), g);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 0)", "blue"), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "blue"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "green"), 1, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 0)", "yellow"), 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 1)", "yellow"), 2, 1, 1, 1);

  gtk_grid_insert_row (GTK_GRID (grid), 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "red"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 1)", "red"), 2, 1, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 1, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 0)", "blue"), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "blue"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "green"), 0, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 2)", "yellow"), 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 2)", "yellow"), 1, 2, 1, 1);

  gtk_grid_insert_column (GTK_GRID (grid), 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "red"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 2)", "red"), 1, 2, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 0, 1, 1, 1);

  child = test_widget ("(0, 0)", "blue");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "blue"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "green"), 1, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 0)", "yellow"), 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 1)", "yellow"), 2, 1, 1, 1);

  gtk_grid_insert_next_to (GTK_GRID (grid), child, GTK_POS_BOTTOM);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "red"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 1)", "red"), 2, 1, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 1, 1, 1, 1);

  child = test_widget ("(0, 0)", "blue");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "blue"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "green"), 0, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 2)", "yellow"), 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 2)", "yellow"), 1, 2, 1, 1);

  gtk_grid_insert_next_to (GTK_GRID (grid), child, GTK_POS_RIGHT);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "red"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 2)", "red"), 1, 2, 1, 1);

  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (NULL, NULL);

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  simple_grid ();
  text_grid ();
  box_comparison ();
  empty_line ();
  scrolling ();
  insert ();
  empty_grid ();

  gtk_main ();

  return 0;
}
