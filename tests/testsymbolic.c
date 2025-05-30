#include <gtk/gtk.h>

static const char css[] =
".testimage {\n"
"  background: white;\n"
"  color: black;\n"
"  -gtk-icon-palette: success lightgreen, warning orange, error red;\n"
"  border: 1px solid gray;\n"
"}\n"
".testimage.dark {\n"
"  background: black;\n"
"  color: white;\n"
"  -gtk-icon-palette: success lightskyblue, warning #fc3, error magenta;\n"
"}\n";

static const char large_css[] =
".testimage {\n"
"  background: white;\n"
"  color: black;\n"
"  -gtk-icon-palette: success lightgreen, warning orange, error red;\n"
"  -gtk-icon-size: 128px;\n"
"  border: 1px solid gray;\n"
"}\n"
".testimage.dark {\n"
"  background: black;\n"
"  color: white;\n"
"  -gtk-icon-palette: success lightskyblue, warning #fc3, error magenta;\n"
"  -gtk-icon-size: 128px;\n"
"}\n";

static void
setup_item (GtkSignalListItemFactory *factory,
            GObject                  *object)
{
  GtkListItem *item = GTK_LIST_ITEM (object);
  GtkWidget *box, *child;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  child = gtk_image_new ();
  gtk_widget_add_css_class (child, "testimage");
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_image_new ();
  gtk_widget_add_css_class (child, "testimage");
  gtk_widget_add_css_class (child, "dark");
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("");
  gtk_box_append (GTK_BOX (box), child);

  gtk_list_item_set_child (item, box);
}

static void
bind_item (GtkSignalListItemFactory *factory,
            GObject                  *object)
{
  GtkWidget *box;
  GObject *item;
  const char *name;
  GtkWidget *child;

  box = gtk_list_item_get_child (GTK_LIST_ITEM (object));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (object));

  name = gtk_string_object_get_string (GTK_STRING_OBJECT (item));
  for (child = gtk_widget_get_first_child (box);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_IMAGE (child))
        gtk_image_set_from_icon_name (GTK_IMAGE (child), name);
      else if (GTK_IS_LABEL (child))
        gtk_label_set_label (GTK_LABEL (child), name);
    }
}

static gpointer
file_info_to_icon_name (gpointer item,
                        gpointer user_data)
{
  const char *name;
  char *icon_name;
  GtkStringObject *obj;

  name = g_file_info_get_name (G_FILE_INFO (item));

  if (g_str_has_suffix (name, ".svg"))
    icon_name = g_strndup (name, strlen (name) - strlen (".svg"));
  else
    icon_name = g_strdup (name);

  g_object_unref (item);

  obj = gtk_string_object_new (icon_name);

  g_free (icon_name);

  return obj;
}

static void
large_toggled (GObject    *obj,
               GParamSpec *pspec,
               gpointer    data)
{
  GtkCssProvider *provider = data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (obj)))
    gtk_css_provider_load_from_string (provider, large_css);
  else
    gtk_css_provider_load_from_string (provider, css);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkCssProvider *provider;
  GtkListItemFactory *factory;
  GtkWidget *sw;
  GtkWidget *list;
  GFile *file;
  GFileInfo *info;
  GListModel *dirlist;
  GListModel *model;
  GtkWidget *headerbar;
  GtkWidget *large_toggle;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);

  file = g_file_new_for_commandline_arg (argv[1]);

  info = g_file_query_info (file, "standard::name,standard::type", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR)
    {
      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
      dirlist = G_LIST_MODEL (g_list_store_new (G_TYPE_FILE_INFO));
      g_list_store_append (G_LIST_STORE (dirlist), info);
    }
  else
    {
      g_object_unref (info);
      dirlist = G_LIST_MODEL (gtk_directory_list_new ("standard::name", file));
    }

  model = G_LIST_MODEL (gtk_map_list_model_new (dirlist, file_info_to_icon_name, NULL, NULL));
  g_object_unref (file);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  large_toggle = gtk_toggle_button_new_with_label ("Large");
  g_signal_connect (large_toggle, "notify::active", G_CALLBACK (large_toggled), provider);

  headerbar = gtk_header_bar_new ();
  gtk_header_bar_pack_end (GTK_HEADER_BAR (headerbar), large_toggle);

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);

  list = GTK_WIDGET (gtk_list_view_new (GTK_SELECTION_MODEL (gtk_no_selection_new (model)), factory));

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  gtk_window_set_child (GTK_WINDOW (window), sw);

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
