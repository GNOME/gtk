#include <gtk/gtk.h>

#define TEST_TYPE_OBJECT (test_object_get_type ())
G_DECLARE_FINAL_TYPE (TestObject, test_object, TEST, OBJECT, GObject)

struct _TestObject {
  GObject parent_instance;
  char *string;
};

enum {
  PROP_STRING = 1,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT);

static void
test_object_init (TestObject *obj)
{
}

static void
test_object_finalize (GObject *object)
{
  TestObject *obj = TEST_OBJECT (object);

  g_free (obj->string);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  TestObject *obj = TEST_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_free (obj->string);
      obj->string = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
test_object_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  TestObject *obj = TEST_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, obj->string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
test_object_class_init (TestObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = test_object_finalize;
  object_class->set_property = test_object_set_property;
  object_class->get_property = test_object_get_property;

  g_object_class_install_property (object_class, PROP_STRING,
      g_param_spec_string ("string", "String", "String",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static TestObject *
test_object_new (const char *string)
{
  return g_object_new (TEST_TYPE_OBJECT, "string", string, NULL);
}

static const char *
test_object_get_string (TestObject *obj)
{
  return obj->string;
}

/* * * */

static GdkContentProvider *
prepare_drag (GtkDragSource *source,
              double         x,
              double         y)
{
  GtkWidget *label;

  label = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  return gdk_content_provider_new_typed (G_TYPE_STRING,
                                         gtk_label_get_label (GTK_LABEL (label)));
}

static GListModel *
create_model (guint n)
{
  GListStore *store;
  guint i;

  store = g_list_store_new (TEST_TYPE_OBJECT);
  for (i = 0; i < n; i++)
    {
      char *string;
      TestObject *obj;

      string = g_strdup_printf ("%u", i);
      obj = test_object_new (string);
      g_list_store_append (store, obj);
      g_object_unref (obj);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *entry;

  entry = gtk_entry_new ();
  gtk_editable_set_width_chars (GTK_EDITABLE (entry), 3);
  gtk_list_item_set_child (item, entry);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  TestObject *obj;
  GtkWidget *entry;

  obj = gtk_list_item_get_item (item);
  entry = gtk_list_item_get_child (item);

  gtk_editable_set_text (GTK_EDITABLE (entry), test_object_get_string (obj));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *sw;
  GtkWidget *list;
  GtkDragSource *dragsource;
  GListModel *model;
  GtkListItemFactory *factory;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);

  gtk_window_set_child (GTK_WINDOW (window), box);

  label = gtk_label_new ("Drag me");
  gtk_box_append (GTK_BOX (box), label);

  dragsource = gtk_drag_source_new ();
  g_signal_connect (dragsource, "prepare", G_CALLBACK (prepare_drag), NULL);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (dragsource));

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_append (GTK_BOX (box), sw);

  list = gtk_grid_view_new ();
  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (list), 20);
  gtk_grid_view_set_max_columns (GTK_GRID_VIEW (list), 20);
  gtk_grid_view_set_enable_rubberband (GTK_GRID_VIEW (list), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  model = create_model (400);
  g_object_set (list, "model", model, NULL);
  g_object_unref (model);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  g_object_set (list, "factory", factory, NULL);
  g_object_unref (factory);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));

  return 0;
}
