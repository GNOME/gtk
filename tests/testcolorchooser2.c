#include <gtk/gtk.h>

static void rgba_changed (GtkColorChooser *chooser, GParamSpec *pspec, gpointer data);

static void
text_activated (GtkEntry *entry, gpointer data)
{
  GtkColorChooser *chooser = data;
  GdkRGBA rgba;
  const char *text;

  text = gtk_entry_get_text (entry);

  g_signal_handlers_block_by_func (entry, rgba_changed, entry);
  if (gdk_rgba_parse (&rgba, text))
    gtk_color_chooser_set_rgba (chooser, &rgba);
  g_signal_handlers_unblock_by_func (entry, rgba_changed, entry);
}

static void
rgba_changed (GtkColorChooser *chooser, GParamSpec *pspec, gpointer data)
{
  GtkWidget *entry = data;
  GdkRGBA color;
  char *s;

  gtk_color_chooser_get_rgba (chooser, &color);
  s = gdk_rgba_to_string (&color);

  g_signal_handlers_block_by_func (entry, text_activated, chooser);
  gtk_entry_set_text (GTK_ENTRY (entry), s);
  g_signal_handlers_unblock_by_func (entry, text_activated, chooser);

  g_free (s);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *chooser;
  GtkWidget *entry;
  GtkBuilder *builder;

  gtk_init (NULL, NULL);

  builder = gtk_builder_new_from_file ("testcolorchooser2.ui");
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
  chooser = GTK_WIDGET (gtk_builder_get_object (builder, "chooser"));
  entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));

  g_signal_connect (chooser, "notify::rgba", G_CALLBACK (rgba_changed), entry);
  g_signal_connect (entry, "activate", G_CALLBACK (text_activated), chooser);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
