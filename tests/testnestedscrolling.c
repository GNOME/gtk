#include <gtk/gtk.h>

/* This is the function that creates the #GListModel that we need.
 * GTK list widgets need a #GListModel to display, as models support change
 * notifications.
 * Unfortunately various older APIs do not provide list models, so we create
 * our own.
 */
static GListModel *
create_application_list (void)
{
  GListStore *store;
  GList *apps, *l;

  /* We use a #GListStore here, which is a simple array-like list implementation
   * for manual management.
   * List models need to know what type of data they provide, so we need to
   * provide the type here. As we want to do a list of applications, #GAppInfo
   * is the object we provide.
   */
  store = g_list_store_new (G_TYPE_APP_INFO);

  apps = g_app_info_get_all ();

  for (l = apps; l; l = l->next)
    g_list_store_append (store, l->data);

  g_list_free_full (apps, g_object_unref);

  return G_LIST_MODEL (store);
}

static void
setup_list_item (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  GtkWidget *label = gtk_label_new (NULL);

  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);

  gtk_list_item_set_child (list_item, label);
}

static void
bind_list_item (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *label;
  gpointer item;
  const char *s;

  item = gtk_list_item_get_item (list_item);

  if (item)
    s = g_app_info_get_name (G_APP_INFO (item));
  else
    s = NULL;

  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), s);
}

static void
on_application_activate (GtkApplication *app)
{
  GtkWidget *window, *swindow, *box;
  gint i = 0;

  window = gtk_application_window_new (app);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  gtk_window_set_child (GTK_WINDOW (window), swindow);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), box);

  for (i = 0; i < 20; i++)
    {
      GtkWidget *list, *separator;
      GtkSelectionModel *model;
      GtkListItemFactory *factory;

      if (i > 0)
        {
          separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
          gtk_box_append (GTK_BOX (box), separator);
        }

      swindow = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_box_append (GTK_BOX (box), swindow);

      model = GTK_SELECTION_MODEL (gtk_single_selection_new (create_application_list ()));
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_list_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_list_item), NULL);

      list = gtk_list_view_new (model, factory);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), list);
    }

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  GtkApplication *application = gtk_application_new ("org.gtk.test.nestedscrolling",
                                                     G_APPLICATION_FLAGS_NONE);
  int result;

  g_signal_connect (application, "activate",
                    G_CALLBACK (on_application_activate), NULL);

  result = g_application_run (G_APPLICATION (application), argc, argv);
  g_object_unref (application);

  return result;
}
