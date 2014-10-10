#include <gtk/gtk.h>

static void
fill_text_view (GtkWidget *tv, const gchar *text)
{
  gint i;
  GString *s;

  s = g_string_new ("");
  for (i = 0; i < 200; i++)
    g_string_append_printf (s, "%s %d\n", text, i);

  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv)),
                            g_string_free (s, FALSE), -1); 
}

int
main (int argc, char *argv[])
{
  GtkWidget *win, *box, *tv, *sw, *sb;
  GtkAdjustment *adj;

  gtk_init (NULL, NULL);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 640, 480);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_container_add (GTK_CONTAINER (win), box);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_box_pack_start (GTK_BOX (box), sw, TRUE, TRUE, 0);
  tv = gtk_text_view_new ();
  fill_text_view (tv, "Left");
  gtk_container_add (GTK_CONTAINER (sw), tv);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));

  sw = gtk_scrolled_window_new (NULL, adj);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_box_pack_start (GTK_BOX (box), sw, TRUE, TRUE, 0);
  tv = gtk_text_view_new ();
  fill_text_view (tv, "Middle");
  gtk_container_add (GTK_CONTAINER (sw), tv);

  sw = gtk_scrolled_window_new (NULL, adj);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_box_pack_start (GTK_BOX (box), sw, TRUE, TRUE, 0);
  tv = gtk_text_view_new ();
  fill_text_view (tv, "Right");
  gtk_container_add (GTK_CONTAINER (sw), tv);

  sb = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adj);

  gtk_container_add (GTK_CONTAINER (box), sb);

  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
