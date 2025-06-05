#include <gtk/gtk.h>

#include "gtk/gtkiconpaintableprivate.h"

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

static GtkCssProvider *provider;
static GListModel *model;

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

  child = gtk_image_new ();
  gtk_widget_add_css_class (child, "testimage");
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_image_new ();
  gtk_widget_add_css_class (child, "testimage");
  gtk_widget_add_css_class (child, "dark");
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_image_new ();
  gtk_widget_add_css_class (child, "testimage");
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_image_new ();
  gtk_widget_add_css_class (child, "testimage");
  gtk_widget_add_css_class (child, "dark");
  gtk_box_append (GTK_BOX (box), child);

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
  GFile *file;
  const char *name;
  int icon_size;
  char *icon_name;
  GtkWidget *child;
  GtkIconPaintable *icon;

  box = gtk_list_item_get_child (GTK_LIST_ITEM (object));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (object));

  file = G_FILE (g_file_info_get_attribute_object (G_FILE_INFO (item), "standard::file"));
  name = g_file_info_get_name (G_FILE_INFO (item));
  icon_size = g_file_info_get_attribute_int32 (G_FILE_INFO (item), "private::icon-size");

  if (g_str_has_suffix (name, ".svg"))
    icon_name = g_strndup (name, strlen (name) - strlen (".svg"));
  else
    icon_name = g_strdup (name);

  /* the first two icons allow svg-to-node and recoloring */
  child = gtk_widget_get_first_child (box);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  /* the next two icons allow svg-to-node, but don't allow recoloring */
  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_icon_paintable_set_debug (icon, TRUE, FALSE, TRUE);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_icon_paintable_set_debug (icon, TRUE, FALSE, TRUE);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  /* the next two icons don't allow svg-to-node, but allow masks */
  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_icon_paintable_set_debug (icon, FALSE, FALSE, TRUE);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_icon_paintable_set_debug (icon, FALSE, FALSE, TRUE);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  /* the next two icons don't allow svg-to-node or masks */
  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_icon_paintable_set_debug (icon, FALSE, FALSE, FALSE);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  child = gtk_widget_get_next_sibling (child);
  icon = gtk_icon_paintable_new_for_file (file, icon_size, 2);
  gtk_icon_paintable_set_debug (icon, FALSE, FALSE, FALSE);
  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (icon));
  g_object_unref (icon);

  child = gtk_widget_get_next_sibling (child);
  gtk_label_set_label (GTK_LABEL (child), icon_name);

  g_free (icon_name);
}

static gpointer
add_icon_size_to_file_info (gpointer item,
                            gpointer user_data)
{
  GFileInfo *info;

  info = g_file_info_dup (G_FILE_INFO (item));
  g_file_info_set_attribute_int32 (info, "private::icon-size", GPOINTER_TO_INT (user_data));

  g_object_unref (item);

  return info;
}

static void
large_toggled (GObject    *obj,
               GParamSpec *pspec,
               gpointer    data)
{
  int icon_size;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (obj)))
    {
      icon_size = 128;
      gtk_css_provider_load_from_string (provider, large_css);
    }
  else
    {
      icon_size = 16;
      gtk_css_provider_load_from_string (provider, css);
    }

  gtk_map_list_model_set_map_func (GTK_MAP_LIST_MODEL (model),
                                   add_icon_size_to_file_info, GINT_TO_POINTER (icon_size), NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkListItemFactory *factory;
  GtkWidget *sw;
  GtkWidget *list;
  GFile *file;
  GFileInfo *info;
  GListModel *dirlist;
  GtkWidget *headerbar;
  GtkWidget *large_toggle;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);

  if (argc > 1)
    file = g_file_new_for_commandline_arg (argv[1]);
  else
    file = g_file_new_for_path ("/usr/share/icons/Adwaita/symbolic/actions");

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

  model = G_LIST_MODEL (gtk_map_list_model_new (dirlist, add_icon_size_to_file_info, GINT_TO_POINTER (16), NULL));
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
