#include <gtk/gtk.h>
#include <glib/gstdio.h>

static void
clicked (GtkButton *button)
{
  GtkWidget *toplevel;
  GtkWindow *window;
  GtkAllocation alloc;
  graphene_rect_t bounds;

  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_style_context_add_class (gtk_widget_get_style_context (window), "view");
  gtk_window_set_resizable (window, FALSE);
  toplevel = gtk_widget_get_toplevel (button);
  gtk_window_set_transient_for (window, toplevel);
  gtk_container_add (GTK_CONTAINER (window), gtk_label_new ("test content"));

  gtk_widget_compute_bounds (button, toplevel, &bounds);
  alloc.x = floorf (bounds.origin.x);
  alloc.y = floorf (bounds.origin.y);
  alloc.width = ceilf (bounds.size.width);
  alloc.height = ceilf (bounds.size.height);

  gtk_widget_realize (window);
  gdk_surface_move_to_rect (gtk_widget_get_surface (window),
                            &alloc,
                            GDK_GRAVITY_SOUTH,
                            GDK_GRAVITY_NORTH,
                            GDK_ANCHOR_FLIP | GDK_ANCHOR_SLIDE,
                            0, 0);
                      
  gtk_widget_show (window);
}

int
main (int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *button;

  gtk_init ();

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  button = gtk_button_new_with_label ("Pop");
  g_object_set (button, "margin", 30, NULL);
  gtk_container_add (GTK_CONTAINER (win), button);
  g_signal_connect (button, "clicked", G_CALLBACK (clicked), NULL);

  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (win);

  gtk_main ();

  return 0;
}
