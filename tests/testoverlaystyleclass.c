#include <gtk/gtk.h>

static void
child_size_allocate (GtkWidget *child,
                     GdkRectangle *allocation,
                     gpointer user_data)
{
  GtkStyleContext *context;
  context = gtk_widget_get_style_context (child);

  g_print ("Child %p\nHas left? %d\nHas right? %d\nHas top? %d\nHas bottom? %d\n",
           child,
           gtk_style_context_has_class (context, "left"),
           gtk_style_context_has_class (context, "right"),
           gtk_style_context_has_class (context, "top"),
           gtk_style_context_has_class (context, "bottom"));
}

static gboolean
overlay_get_child_position (GtkOverlay *overlay,
                            GtkWidget *child,
                            GdkRectangle *allocation,
                            gpointer user_data)
{
  GtkWidget *custom_child = user_data;
  GtkRequisition req;

  if (child != custom_child)
    return FALSE;

  gtk_widget_get_preferred_size (child, NULL, &req);

  allocation->x = 120;
  allocation->y = 0;
  allocation->width = req.width;
  allocation->height = req.height;

  return TRUE;
}

int 
main (int argc, char *argv[])
{
  GtkWidget *win, *overlay, *grid, *main_child, *child, *label, *sw;
  GdkRGBA color;
  gchar *str;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 600, 600);

  grid = gtk_grid_new ();
  child = gtk_event_box_new ();
  gdk_rgba_parse (&color, "red");
  gtk_widget_override_background_color (child, 0, &color);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);
  gtk_container_add (GTK_CONTAINER (grid), child);
  label = gtk_label_new ("Out of overlay");
  gtk_container_add (GTK_CONTAINER (child), label);

  overlay = gtk_overlay_new ();
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_ALWAYS,
                                  GTK_POLICY_ALWAYS);
  gtk_container_add (GTK_CONTAINER (overlay), sw);

  main_child = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (sw), main_child);
  gdk_rgba_parse (&color, "green");
  gtk_widget_override_background_color (main_child, 0, &color);
  gtk_widget_set_hexpand (main_child, TRUE);
  gtk_widget_set_vexpand (main_child, TRUE);
  label = gtk_label_new ("Main child");
  gtk_container_add (GTK_CONTAINER (main_child), label);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Bottom/Right child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  g_signal_connect (child, "size-allocate",
                    G_CALLBACK (child_size_allocate), overlay);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Left/Top child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  g_signal_connect (child, "size-allocate",
                    G_CALLBACK (child_size_allocate), overlay);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Right/Center child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  g_signal_connect (child, "size-allocate",
                    G_CALLBACK (child_size_allocate), overlay);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  gtk_widget_set_margin_start (child, 55);
  gtk_widget_set_margin_top (child, 4);
  g_print ("Left/Top margined child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  g_signal_connect (child, "size-allocate",
                    G_CALLBACK (child_size_allocate), overlay);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Custom get-child-position child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  g_signal_connect (child, "size-allocate",
                    G_CALLBACK (child_size_allocate), overlay);
  g_signal_connect (overlay, "get-child-position",
                    G_CALLBACK (overlay_get_child_position), child);

  gtk_grid_attach (GTK_GRID (grid), overlay, 1, 0, 1, 3);
  gtk_container_add (GTK_CONTAINER (win), grid);

  g_print ("\n");

  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
