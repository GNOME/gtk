/* Lists/Objects in GTK
 * #Keywords: GtkListItemFactory, GtkSortListModel, GtkStringList
 *
 * This demo uses the GtkListView widget to show all the objects in GTK
 * grouped by their type.
 *
 * It shows how to use sections in GtkListView
 */

#include <gtk/gtk.h>

/* This is the function that creates the GListModel that we need.
 */
static GListModel *
create_object_list (void)
{
  GtkStringList *strings;
  const GType *types;
  guint i, n;

  /* We use a GtkStringList here, because it requires the smallest amount of
   * code, not because it's a great fit.
   */
  strings = gtk_string_list_new (NULL);

  /* This function is meant for testing, but we use it here to get some data
   * to operate on
   */
  gtk_test_register_all_types ();
  types = gtk_test_list_all_types (&n);

  for (i = 0; i < n; i++)
    {
      /* Add all the names of the object types in GTK */
      if (g_type_is_a (types[i], G_TYPE_OBJECT))
        gtk_string_list_append (strings, g_type_name (types[i]));
    }

  return G_LIST_MODEL (strings);
}

/* Make a function that returns a section name for all our types.
 * Do this by adding a few type checks and returning a made up
 * section name for it.
 */
static char *
get_section (GtkStringObject *object)
{
  GType type;

  type = g_type_from_name (gtk_string_object_get_string (object));

  if (g_type_is_a (type, GTK_TYPE_WIDGET))
    return g_strdup ("Widget");
  else if (g_type_is_a (type, GTK_TYPE_FILTER))
    return g_strdup ("Filter");
  else if (g_type_is_a (type, GTK_TYPE_SORTER))
    return g_strdup ("Sorter");
  else if (g_type_is_a (type, G_TYPE_LIST_MODEL))
    return g_strdup ("ListModel");
  else
    return g_strdup ("Zzz..."); /* boring stuff, cleverly sorted at the end */
}

/* These functions set up the object names
 */
static void
setup_section_listitem_cb (GtkListItemFactory *factory,
                           GtkListItem        *list_item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_add_css_class (label, "heading");
  gtk_widget_set_margin_top (label, 4);
  gtk_widget_set_margin_bottom (label, 4);
  gtk_list_item_set_child (list_item, label);
}

/* Here we need to prepare the listitem for displaying its item. We get the
 * listitem already set up from the previous function, so we can reuse the
 * GtkImage widget we set up above.
 * We get the item - which we know is a GAppInfo because it comes out of
 * the model we set up above, grab its icon and display it.
 */
static void
bind_section_listitem_cb (GtkListItemFactory *factory,
                          GtkListItem        *list_item)
{
  GtkWidget *label;
  GtkStringObject *item;
  char *text;

  label = gtk_list_item_get_child (list_item);
  item = gtk_list_item_get_item (list_item);

  text = get_section (item);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

/* Here we need to prepare the listitem for displaying its item. We get the
 * listitem already set up from the previous function, so we can reuse the
 * GtkImage widget we set up above.
 * We get the item - which we know is a GAppInfo because it comes out of
 * the model we set up above, grab its icon and display it.
 */
static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
  GtkWidget *label;
  GtkStringObject *item;

  label = gtk_list_item_get_child (list_item);
  item = gtk_list_item_get_item (list_item);

  gtk_label_set_label (GTK_LABEL (label), gtk_string_object_get_string (item));
}

/* In more complex code, we would also need functions to unbind and teardown
 * the listitem, but this is simple code, so the default implementations are
 * enough. If we had connected signals, this step would have been necessary.
 *
 * The GtkSignalListItemFactory documentation contains more information about
 * this step.
 */

static GtkWidget *window = NULL;

GtkWidget *
do_listview_objects (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *list, *sw;
      GListModel *model;
      GtkListItemFactory *factory;
      GtkSorter *sorter;

      /* Create a window and set a few defaults */
      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Objects in GTK");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      /* The GtkListitemFactory is what is used to create GtkListItems
       * to display the data from the model. So it is absolutely necessary
       * to create one.
       * We will use a GtkSignalListItemFactory because it is the simplest
       * one to use. Different ones are available for different use cases.
       * The most powerful one is GtkBuilderListItemFactory which uses
       * GtkBuilder .ui files, so it requires little code.
       */
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);

      /* And of course we need to set the data model. Here we call the function
       * we wrote above that gives us the list of applications. Then we set
       * it on the list widget.
       * The list will now take items from the model and use the factory
       * to create as many listitems as it needs to show itself to the user.
       */
      model = create_object_list ();

      /* Wrap the model in a sort model that sorts the objects alphabetically.
       */
      sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
      model = G_LIST_MODEL (gtk_sort_list_model_new (model, sorter));

      /* Create a sorter for the sections and tell the sort model about it
       */
      sorter = GTK_SORTER (gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, G_CALLBACK (get_section), NULL, NULL)));
      gtk_string_sorter_set_ignore_case (GTK_STRING_SORTER (sorter), FALSE);
      gtk_sort_list_model_set_section_sorter (GTK_SORT_LIST_MODEL (model), sorter);
      g_object_unref (sorter);

      /* Create the list widget here.
       */
      list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (model)), factory);

      /* Set a factory for sections, otherwise the listview won't use sections.
       */
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_section_listitem_cb), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_section_listitem_cb), NULL);
      gtk_list_view_set_section_factory (GTK_LIST_VIEW (list), factory);
      g_object_unref (factory);

      /* List widgets should always be contained in a GtkScrolledWindow,
       * because otherwise they might get too large or they might not
       * be scrollable.
       */
      sw = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), sw);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
