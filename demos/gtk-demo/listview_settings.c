/* Lists/Settings
 *
 * This demo shows a settings viwer for GSettings.
 *
 * It demonstrates how to implement support for trees with listview.
 */

#include <gtk/gtk.h>

static int
strvcmp (gconstpointer p1,
         gconstpointer p2)
{
  const char * const *s1 = p1;
  const char * const *s2 = p2;

  return strcmp (*s1, *s2);
}

static GListModel *
create_settings_model (gpointer item,
                       gpointer unused)
{
  GSettings *settings = item;
  char **schemas;
  GListStore *result;
  guint i;

  if (settings == NULL)
    {
      g_settings_schema_source_list_schemas (g_settings_schema_source_get_default (),
                                             TRUE,
                                             &schemas,
                                             NULL);
    }
  else
    {
      schemas = g_settings_list_children (settings);
    }

  if (schemas == NULL || schemas[0] == NULL)
    {
      g_free (schemas);
      return NULL;
    }

  qsort (schemas, g_strv_length (schemas), sizeof (char *), strvcmp);

  result = g_list_store_new (G_TYPE_SETTINGS);
  for (i = 0;  schemas[i] != NULL; i++)
    {
      GSettings *child;

      if (settings == NULL)
        child = g_settings_new (schemas[i]);
      else
        child = g_settings_get_child (settings, schemas[i]);

      g_list_store_append (result, child);
      g_object_unref (child);
    }

  return G_LIST_MODEL (result);
}

static void
setup_widget (GtkListItem *list_item,
              gpointer     unused)
{
  GtkWidget *label, *expander;

  expander = gtk_tree_expander_new ();
  gtk_list_item_set_child (list_item, expander);

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), label);
}

static void
bind_widget (GtkListItem *list_item,
             gpointer     unused)
{
  GSettings *settings;
  GtkWidget *label, *expander;
  GSettingsSchema *schema;

  expander = gtk_list_item_get_child (list_item);
  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), gtk_list_item_get_item (list_item));
  label = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));
  settings = gtk_tree_expander_get_item (GTK_TREE_EXPANDER (expander));

  g_object_get (settings, "settings-schema", &schema, NULL);

  gtk_label_set_label (GTK_LABEL (label), g_settings_schema_get_id (schema));

  g_settings_schema_unref (schema);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_settings (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *listview, *sw;;
      GListModel *model;
      GtkTreeListModel *treemodel;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Settings");
      g_signal_connect (window, "destroy",
                        G_CALLBACK(gtk_widget_destroyed), &window);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (window), sw);
    
      listview = gtk_list_view_new_with_factory (
        gtk_functions_list_item_factory_new (setup_widget,
                                             bind_widget,
                                             NULL, NULL));

      model = create_settings_model (NULL, NULL);
      treemodel = gtk_tree_list_model_new (FALSE,
                                           model,
                                           TRUE,
                                           create_settings_model,
                                           NULL,
                                           NULL);
      gtk_list_view_set_model (GTK_LIST_VIEW (listview), G_LIST_MODEL (treemodel));
      g_object_unref (treemodel);
      g_object_unref (model);

      gtk_container_add (GTK_CONTAINER (sw), listview);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
