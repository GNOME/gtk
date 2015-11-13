#include <gtk/gtk.h>

static GtkWidget *
create_level_bar (void)
{
  GtkWidget *bar;

  bar = gtk_level_bar_new ();
  gtk_level_bar_set_min_value (GTK_LEVEL_BAR (bar), 0.0);
  gtk_level_bar_set_max_value (GTK_LEVEL_BAR (bar), 10.0);

  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (bar),
                                  GTK_LEVEL_BAR_OFFSET_LOW, 1.0);

  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (bar),
                                  GTK_LEVEL_BAR_OFFSET_HIGH, 9.0);

  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (bar),
                                  "full", 10.0);

  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (bar),
                                  "my-offset", 5.0);

  return bar;
}

static void
add_custom_css (void)
{
  GtkCssProvider *provider;
  const gchar data[] =
  "levelbar block.my-offset {"
  "   background: magenta;"
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
  value += 0.1;
  if (value >= gtk_level_bar_get_max_value (bar))
    value = gtk_level_bar_get_min_value (bar);
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

static void
toggle (GtkSwitch *sw, GParamSpec *pspec, GtkLevelBar *bar)
{
  if (gtk_switch_get_active (sw))
    gtk_level_bar_set_mode (bar, GTK_LEVEL_BAR_MODE_DISCRETE);
  else
    gtk_level_bar_set_mode (bar, GTK_LEVEL_BAR_MODE_CONTINUOUS);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *bar;
  GtkWidget *box2;
  GtkWidget *sw;

  gtk_init (&argc, &argv);

  add_custom_css ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 100);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  g_object_set (box, "margin", 20, NULL);
  bar = create_level_bar ();
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), bar);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (box), box2);
  gtk_container_add (GTK_CONTAINER (box2), gtk_label_new ("Discrete"));
  sw = gtk_switch_new ();
  gtk_container_add (GTK_CONTAINER (box2), sw);
  g_signal_connect (sw, "notify::active", G_CALLBACK (toggle), bar);

  gtk_widget_show_all (window);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (window_delete_event), NULL);

  g_timeout_add (100, increase_level, bar);
  gtk_main ();

  return 0;
}

