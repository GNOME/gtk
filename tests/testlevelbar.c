#include <gtk/gtk.h>

static GtkWidget *
create_level_bar (void)
{
  GtkWidget *level_bar;

  level_bar = gtk_level_bar_new ();

  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (level_bar),
                                  GTK_LEVEL_BAR_OFFSET_LOW, 0.10);

  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (level_bar),
                                  "my-offset", 0.50);

  return level_bar;
}

static void
add_custom_css (void)
{
  GtkCssProvider *provider;
  const gchar data[] =
  ".level-bar.fill-block.empty-fill-block {"
  "   background-color: transparent;"
  "   background-image: none;"
  "   border-color: alpha(@theme_fg_color, 0.1);"
  "}"
  ".level-bar.fill-block.level-my-offset {"
  "   background-image: linear-gradient(to bottom,"
  "                                     shade(magenta,0.9),"
  "                                     magenta,"
  "                                     shade(magenta,0.85));"
  "}";

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, data, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static gboolean
increase_level (gpointer data)
{
  GtkLevelBar *bar = data;
  gdouble value;

  value = gtk_level_bar_get_value (bar);
  value += 0.01;
  if (value >= 1.0)
    value = 0.0;
  gtk_level_bar_set_value (bar, value);

  return G_SOURCE_CONTINUE;
}

static gboolean
window_delete_event (GtkWidget *widget,
                     GdkEvent *event,
                     gpointer _data)
{
  gtk_main_quit ();
  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *bar;

  gtk_init (&argc, &argv);

  add_custom_css ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 100);
  bar = create_level_bar ();
  g_object_set (bar, "margin", 20, NULL);
  gtk_container_add (GTK_CONTAINER (window), bar);
  gtk_widget_show_all (window);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (window_delete_event), NULL);

  g_timeout_add (100, increase_level, bar);
  gtk_main ();

  return 0;
}

