#include <gtk/gtk.h>

#define TEST_TYPE_OBJECT (test_object_get_type ())
G_DECLARE_FINAL_TYPE (TestObject, test_object, TEST, OBJECT, GObject)

struct _TestObject {
  GObject parent_instance;
  char *string;
  guint number;
};

enum {
  PROP_STRING = 1,
  PROP_NUMBER,
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

    case PROP_NUMBER:
      obj->number = g_value_get_uint (value);
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

    case PROP_NUMBER:
      g_value_set_uint (value, obj->number);
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

  g_object_class_install_property (object_class, PROP_NUMBER,
      g_param_spec_uint ("number", "Number", "Number",
                         0, G_MAXUINT, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static TestObject *
test_object_new (const char *string,
                 guint       number)
{
  return g_object_new (TEST_TYPE_OBJECT,
                       "string", string,
                       "number", number,
                       NULL);
}

static const char *
test_object_get_string (TestObject *obj)
{
  return obj->string;
}

static guint
test_object_get_number (TestObject *obj)
{
  return obj->number;
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
create_model (guint base,
              guint n,
              guint increment)
{
  GListStore *store;
  guint i;

  store = g_list_store_new (TEST_TYPE_OBJECT);
  for (i = 0; i < n; i++)
    {
      char *string;
      guint number;
      TestObject *obj;

      number = base + i * increment;
      string = g_strdup_printf ("%u", number);
      obj = test_object_new (string, number);
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
  GtkDragSource *dragsource;
  GtkWidget *box2;
  GtkWidget *stack;
  GtkWidget *switcher;
  GtkWidget *sw;
  GtkWidget *grid;
  GtkWidget *list;
  GtkWidget *cv;
  GListModel *model;
  GtkListItemFactory *factory;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_window_set_child (GTK_WINDOW (window), box2);

  switcher = gtk_stack_switcher_new ();
  gtk_widget_set_halign (GTK_WIDGET (switcher), GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (GTK_WIDGET (switcher), 10);
  gtk_widget_set_margin_bottom (GTK_WIDGET (switcher), 10);
  gtk_box_append (GTK_BOX (box2), switcher);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_box_append (GTK_BOX (box2), box);

  label = gtk_label_new ("Drag me");
  gtk_box_append (GTK_BOX (box), label);

  dragsource = gtk_drag_source_new ();
  g_signal_connect (dragsource, "prepare", G_CALLBACK (prepare_drag), NULL);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (dragsource));

  stack = gtk_stack_new ();
  gtk_widget_set_vexpand (stack, TRUE);
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_box_append (GTK_BOX (box), stack);

  /* grid */
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "grid", "GtkGridView");

  grid = gtk_grid_view_new ();
  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (grid), 20);
  gtk_grid_view_set_max_columns (GTK_GRID_VIEW (grid), 20);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), grid);

  model = create_model (0, 400, 1);
  gtk_grid_view_set_model (GTK_GRID_VIEW (grid), model);
  g_object_unref (model);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  gtk_grid_view_set_factory (GTK_GRID_VIEW (grid), factory);
  g_object_unref (factory);

  /* list */
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "list", "GtkListView");

  list = gtk_list_view_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  model = create_model (0, 400, 1);
  gtk_list_view_set_model (GTK_LIST_VIEW (list), model);
  g_object_unref (model);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  gtk_list_view_set_factory (GTK_LIST_VIEW (list), factory);
  g_object_unref (factory);

  /* columnview */
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "column", "GtkColumnView");

  cv = gtk_column_view_new ();

  model = create_model (0, 400, 1);
  gtk_column_view_set_model (GTK_COLUMN_VIEW (cv), model);
  g_object_unref (model);

  for (guint i = 0; i < 20; i++)
    {
      GtkColumnViewColumn *column;
      char *title;

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

      title = g_strdup_printf ("Column %u", i);
      column = gtk_column_view_column_new_with_factory (title, factory);
      gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
      g_object_unref (column);
      g_free (title);
    }

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), cv);
  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));

  return 0;
}
