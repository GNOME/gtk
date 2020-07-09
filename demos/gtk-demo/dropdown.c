/* Lists/Selections
 *
 * The GtkDropDown and GtkSuggestionEntry widgets are modern
 * alternatives to GtkComboBox and GtkEntryCompletion.
 *
 * They use list models instead of tree models, and the content
 * is displayed using widgets instead of cell renderers.
 *
 * The examples here demonstrate how to use different kinds of
 * list models with GtkDropDown and GtkSuggestionEntry, how to
 * use search and how to display the selected item differently
 * from the presentation in the popup.
 */

#include <gtk/gtk.h>


#define STRING_TYPE_HOLDER (string_holder_get_type ())
G_DECLARE_FINAL_TYPE (StringHolder, string_holder, STRING, HOLDER, GObject)

struct _StringHolder {
  GObject parent_instance;
  char *title;
  char *icon;
  char *description;
};

G_DEFINE_TYPE (StringHolder, string_holder, G_TYPE_OBJECT);

static void
string_holder_init (StringHolder *holder)
{
}

static void
string_holder_finalize (GObject *object)
{
  StringHolder *holder = STRING_HOLDER (object);

  g_free (holder->title);
  g_free (holder->icon);
  g_free (holder->description);

  G_OBJECT_CLASS (string_holder_parent_class)->finalize (object);
}

static void
string_holder_class_init (StringHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = string_holder_finalize;
}

static StringHolder *
string_holder_new (const char *title, const char *icon, const char *description)
{
  StringHolder *holder = g_object_new (STRING_TYPE_HOLDER, NULL);
  holder->title = g_strdup (title);
  holder->icon = g_strdup (icon);
  holder->description = g_strdup (description);
  return holder;
}

static void
strings_setup_item_single_line (GtkSignalListItemFactory *factory,
                                GtkListItem              *item)
{
  GtkWidget *box, *image, *title;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  image = gtk_image_new ();
  title = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), title);

  g_object_set_data (G_OBJECT (item), "title", title);
  g_object_set_data (G_OBJECT (item), "image", image);

  gtk_list_item_set_child (item, box);
}

static void
strings_setup_item_full (GtkSignalListItemFactory *factory,
                         GtkListItem              *item)
{
  GtkWidget *box, *box2, *image, *title, *description;

  image = gtk_image_new ();
  title = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);
  description = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (description), 0.0);
  gtk_widget_add_css_class (description, "dim-label");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box2), title);
  gtk_box_append (GTK_BOX (box2), description);

  g_object_set_data (G_OBJECT (item), "title", title);
  g_object_set_data (G_OBJECT (item), "image", image);
  g_object_set_data (G_OBJECT (item), "description", description);

  gtk_list_item_set_child (item, box);
}

static void
strings_bind_item (GtkSignalListItemFactory *factory,
                    GtkListItem              *item)
{
  GtkWidget *image, *title, *description;
  StringHolder *holder;

  holder = gtk_list_item_get_item (item);

  title = g_object_get_data (G_OBJECT (item), "title");
  image = g_object_get_data (G_OBJECT (item), "image");
  description = g_object_get_data (G_OBJECT (item), "description");

  gtk_label_set_label (GTK_LABEL (title), holder->title);
  if (image)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (image), holder->icon);
      gtk_widget_set_visible (image, holder->icon != NULL);
    }
  if (description)
    {
      gtk_label_set_label (GTK_LABEL (description), holder->description);
      gtk_widget_set_visible (description , holder->description != NULL);
    }
}

static GtkListItemFactory *
strings_factory_new (gboolean full)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();
  if (full)
    g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_full), NULL);
  else
    g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_single_line), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (strings_bind_item), NULL);

  return factory;
}

static GListModel *
strings_model_new (const char *const *titles,
                   const char *const *icons,
                   const char *const *descriptions)
{
  GListStore *store;
  int i;

  store = g_list_store_new (STRING_TYPE_HOLDER);
  for (i = 0; titles[i]; i++)
    {
      StringHolder *holder = string_holder_new (titles[i],
                                                icons ? icons[i] : NULL,
                                                descriptions ? descriptions[i] : NULL);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }

  return G_LIST_MODEL (store);
}

static GtkWidget *
drop_down_new_from_strings (const char *const *titles,
                            const char *const *icons,
                            const char *const *descriptions)
{
  GtkWidget *widget;
  GListModel *model;
  GtkListItemFactory *factory;
  GtkListItemFactory *list_factory;

  g_return_val_if_fail (titles != NULL, NULL);
  g_return_val_if_fail (icons == NULL || g_strv_length ((char **)icons) == g_strv_length ((char **)titles), NULL);
  g_return_val_if_fail (descriptions == NULL || g_strv_length ((char **)icons) == g_strv_length ((char **)descriptions), NULL);

  model = strings_model_new (titles, icons, descriptions);
  factory = strings_factory_new (FALSE);
  if (icons != NULL || descriptions != NULL)
    list_factory = strings_factory_new (TRUE);
  else
    list_factory = NULL;

  widget = g_object_new (GTK_TYPE_DROP_DOWN,
                         "model", model,
                         "factory", factory,
                         "list-factory", list_factory,
                         NULL);

  g_object_unref (model);
  g_object_unref (factory);
  if (list_factory)
    g_object_unref (list_factory);

  return widget;
}

static char *
get_family_name (gpointer item)
{
  return g_strdup (pango_font_family_get_name (PANGO_FONT_FAMILY (item)));
}

static char *
get_title (gpointer item)
{
  return g_strdup (STRING_HOLDER (item)->title);
}

static char *
get_file_name (gpointer item)
{
  return g_strdup (g_file_info_get_display_name (G_FILE_INFO (item)));
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *box;
  GtkWidget *icon;
  GtkWidget *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  icon = gtk_image_new ();
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_box_append (GTK_BOX (box), icon);
  gtk_box_append (GTK_BOX (box), label);
  gtk_list_item_set_child (item, box);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  GFileInfo *info = G_FILE_INFO (gtk_list_item_get_item (item));
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *icon = gtk_widget_get_first_child (box);
  GtkWidget *label = gtk_widget_get_last_child (box);

  gtk_image_set_from_gicon (GTK_IMAGE (icon), g_file_info_get_icon (info));
  gtk_label_set_label (GTK_LABEL (label), g_file_info_get_display_name (info));
}

GtkWidget *
do_dropdown (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button, *box, *spin, *check, *hbox, *label, *entry;
  GListModel *model;
  GtkExpression *expression;
  GtkListItemFactory *factory;
  const char * const times[] = { "1 minute", "2 minutes", "5 minutes", "20 minutes", NULL };
  const char * const many_times[] = {
    "1 minute", "2 minutes", "5 minutes", "10 minutes", "15 minutes", "20 minutes",
    "25 minutes", "30 minutes", "35 minutes", "40 minutes", "45 minutes", "50 minutes",
    "55 minutes", "1 hour", "2 hours", "3 hours", "5 hours", "6 hours", "7 hours",
    "8 hours", "9 hours", "10 hours", "11 hours", "12 hours", NULL
  };
  const char * const device_titles[] = { "Digital Output", "Headphones", "Digital Output", "Analog Output", NULL };
  const char * const device_icons[] = {  "audio-card-symbolic", "audio-headphones-symbolic", "audio-card-symbolic", "audio-card-symbolic", NULL };
  const char * const device_descriptions[] = {
    "Built-in Audio", "Built-in audio", "Thinkpad Tunderbolt 3 Dock USB Audio", "Thinkpad Tunderbolt 3 Dock USB Audio", NULL
  };
  const char *words[] = {
    "GNOME",
    "gnominious",
    "Gnomonic projection",
    "total",
    "totally",
    "toto",
    "tottery",
    "totterer",
    "Totten trust",
    "totipotent",
    "totipotency",
    "totemism",
    "totem pole",
    "Totara",
    "totalizer",
    "totalizator",
    "totalitarianism",
    "total parenteral nutrition",
    "total hysterectomy",
    "total eclipse",
    "Totipresence",
    "Totipalmi",
    "Tomboy",
    "zombie",
    NULL
  };

  char *cwd;
  GFile *file;
  GListModel *dir;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Selections");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);

      gtk_widget_set_margin_start (hbox, 20);
      gtk_widget_set_margin_end (hbox, 20);
      gtk_widget_set_margin_top (hbox, 20);
      gtk_widget_set_margin_bottom (hbox, 20);
      gtk_window_set_child (GTK_WINDOW (window), hbox);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (hbox), box);

      label = gtk_label_new ("Dropdowns");
      gtk_widget_add_css_class (label, "title-4");
      gtk_box_append (GTK_BOX (box), label);

      /* A basic dropdown */
      button = drop_down_new_from_strings (times, NULL, NULL);
      gtk_box_append (GTK_BOX (box), button);

      /* A dropdown using an expression to obtain strings */
      button = drop_down_new_from_strings (many_times, NULL, NULL);
      gtk_drop_down_set_enable_search (GTK_DROP_DOWN (button), TRUE);
      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                (GCallback)get_title,
                                                NULL, NULL);
      gtk_drop_down_set_expression (GTK_DROP_DOWN (button), expression);
      gtk_expression_unref (expression);
      gtk_box_append (GTK_BOX (box), button);

      /* A dropdown using a non-trivial model, and search */
      button = gtk_drop_down_new ();

      model = G_LIST_MODEL (pango_cairo_font_map_get_default ());
      gtk_drop_down_set_model (GTK_DROP_DOWN (button), model);
      gtk_drop_down_set_selected (GTK_DROP_DOWN (button), 0);

      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                (GCallback)get_family_name,
                                                NULL, NULL);
      gtk_drop_down_set_expression (GTK_DROP_DOWN (button), expression);
      gtk_expression_unref (expression);
      gtk_box_append (GTK_BOX (box), button);

      spin = gtk_spin_button_new_with_range (-1, g_list_model_get_n_items (G_LIST_MODEL (model)), 1);
      gtk_widget_set_halign (spin, GTK_ALIGN_START);
      gtk_widget_set_margin_start (spin, 20);
      g_object_bind_property  (button, "selected", spin, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (box), spin);

      check = gtk_check_button_new_with_label ("Enable search");
      gtk_widget_set_margin_start (check, 20);
      g_object_bind_property  (button, "enable-search", check, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (box), check);

      g_object_unref (model);

      /* A dropdown with a separate list factory */
      button = drop_down_new_from_strings (device_titles, device_icons, device_descriptions);
      gtk_box_append (GTK_BOX (box), button);

      gtk_box_append (GTK_BOX (hbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (hbox), box);

      label = gtk_label_new ("Suggestions");
      gtk_widget_add_css_class (label, "title-4");
      gtk_box_append (GTK_BOX (box), label);

      /* A basic suggestion entry */
      entry = gtk_suggestion_entry_new ();
      g_object_set (entry, "placeholder-text", "Words with T or G…", NULL);
      gtk_suggestion_entry_set_from_strings (GTK_SUGGESTION_ENTRY (entry), words);

      gtk_box_append (GTK_BOX (box), entry);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_set_halign (hbox, GTK_ALIGN_START);
      gtk_widget_set_margin_start (hbox, 20);
      spin = gtk_spin_button_new_with_range (0, 10, 1);
      g_object_bind_property  (entry, "minimum-length", spin, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (hbox), gtk_label_new ("Min. length"));
      gtk_box_append (GTK_BOX (hbox), spin);
      gtk_box_append (GTK_BOX (box), hbox);

      check = gtk_check_button_new_with_label ("Auto-Insert");
      gtk_widget_set_margin_start (check, 20);
      g_object_bind_property  (entry, "insert-prefix", check, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (box), check);

      check = gtk_check_button_new_with_label ("Auto-Select");
      gtk_widget_set_margin_start (check, 20);
      g_object_bind_property  (entry, "insert-selection", check, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (box), check);

      /* A suggestion entry using a custom model, and no filtering */
      entry = gtk_suggestion_entry_new ();

      cwd = g_get_current_dir ();
      file = g_file_new_for_path (cwd);
      dir = G_LIST_MODEL (gtk_directory_list_new ("standard::display-name,standard::content-type,standard::icon,standard::size", file));
      gtk_suggestion_entry_set_model (GTK_SUGGESTION_ENTRY (entry), dir);
      g_object_unref (dir);
      g_object_unref (file);
      g_free (cwd);

      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                (GCallback)get_file_name,
                                                NULL, NULL);
      gtk_suggestion_entry_set_expression (GTK_SUGGESTION_ENTRY (entry), expression);
      gtk_expression_unref (expression);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

      gtk_suggestion_entry_set_factory (GTK_SUGGESTION_ENTRY (entry), factory);
      g_object_unref (factory);

      gtk_suggestion_entry_set_use_filter (GTK_SUGGESTION_ENTRY (entry), FALSE);
      gtk_suggestion_entry_set_show_button (GTK_SUGGESTION_ENTRY (entry), TRUE);
      gtk_suggestion_entry_set_insert_selection (GTK_SUGGESTION_ENTRY (entry), TRUE);

      gtk_box_append (GTK_BOX (box), entry);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
