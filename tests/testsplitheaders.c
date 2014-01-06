#include <gtk/gtk.h>

static void
split_decorations (GObject    *obj,
                   GParamSpec *pspec,
                   gpointer    data)
{
  GtkBuilder *builder = data;
  GtkWidget *sheader, *mheader;
  gchar *layout, *p1, *p2;
  gchar **p;

  sheader = gtk_builder_get_object (builder, "sidebar-header");
  mheader = gtk_builder_get_object (builder, "main-header");

  g_object_get (obj, "gtk-decoration-layout", &layout, NULL);

  p = g_strsplit (layout, ":", -1);

  p1 = g_strconcat (p[0], ":", NULL);
  p2 = g_strconcat (":", p[1], NULL);

  gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (sheader), p1);
  gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (mheader), p2);
 
  g_free (p1);
  g_free (p2);
  g_strfreev (p);
  g_free (layout);
}

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkSettings *settings;
  GtkWidget *win;
  GtkWidget *entry;
  GtkWidget *check;
  GtkWidget *header;

  gtk_init (NULL, NULL);

  builder = gtk_builder_new_from_file ("testsplitheaders.ui");

  win = gtk_builder_get_object (builder, "window");
  settings = gtk_widget_get_settings (win);

  g_signal_connect (settings, "notify::gtk-decoration-layout",
                    G_CALLBACK (split_decorations), builder);
  split_decorations (settings, NULL, builder);
  
  entry = gtk_builder_get_object (builder, "layout-entry");
  g_object_bind_property (settings, "gtk-decoration-layout",
                          entry, "text",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);                      
  check = gtk_builder_get_object (builder, "decorations");
  header = gtk_builder_get_object (builder, "sidebar-header");
  g_object_bind_property (check, "active", 
                          header, "show-close-button",
			  G_BINDING_DEFAULT);                      
  header = gtk_builder_get_object (builder, "main-header");
  g_object_bind_property (check, "active", 
                          header, "show-close-button",
			  G_BINDING_DEFAULT);                      
  gtk_window_present (GTK_WINDOW (win));

  gtk_main ();

  return 0;
}
