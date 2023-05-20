#include <gtk/gtk.h>

static void
setup_item (GtkSignalListItemFactory *self,
            GObject                  *object)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  GtkWidget *child = gtk_label_new ("");

  gtk_label_set_xalign (GTK_LABEL (child), 0);
  gtk_list_item_set_child (list_item, child);
}

static void
bind_item (GtkSignalListItemFactory *self,
           GObject                  *object)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  GObject *item = gtk_list_item_get_item (list_item);
  GtkWidget *child = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (child),
                       gtk_string_object_get_string (GTK_STRING_OBJECT (item)));
}

static void
setup_header (GtkSignalListItemFactory *self,
              GObject                  *object)
{
  GtkListHeader *header = GTK_LIST_HEADER (object);
  GtkWidget *child = gtk_label_new ("");

  gtk_label_set_xalign (GTK_LABEL (child), 0);
  gtk_list_header_set_child (header, child);
}

static char *
get_first (GObject *this)
{
  const char *s = gtk_string_object_get_string (GTK_STRING_OBJECT (this));
  char buffer[6] = { 0, };

  g_unichar_to_utf8 (g_unichar_toupper (g_utf8_get_char (s)), buffer);

  return g_strdup (buffer);
}

static void
bind_header (GtkSignalListItemFactory *self,
             GObject                  *object)
{
  GtkListHeader *header = GTK_LIST_HEADER (object);
  GObject *item = gtk_list_header_get_item (header);
  GtkWidget *child = gtk_list_header_get_child (header);
  PangoAttrList *attrs;
  char *string;

  string = get_first (item);

  gtk_label_set_label (GTK_LABEL (child), string);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (child), attrs);
  pango_attr_list_unref (attrs);
  g_free (string);
}

static const char *strings[] = {
  "Alpha", "Andromeda", "Anaphylaxis", "Anaheim", "Beer", "Branch", "Botulism", "Banana",
  "Bee", "Crane", "Caldera", "Copper", "Crowd", "Dora", "Dolphin", "Dam", "Ding",
 NULL,
};

static void
read_lines_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      data)
{
  GBufferedInputStream *stream = G_BUFFERED_INPUT_STREAM (object);
  GtkStringList *stringlist = data;
  GError *error = NULL;
  gsize size;
  GPtrArray *lines;
  gssize n_filled;
  const char *buffer, *newline;

  n_filled = g_buffered_input_stream_fill_finish (stream, result, &error);
  if (n_filled < 0)
    {
      g_print ("Could not read data: %s\n", error->message);
      g_clear_error (&error);
      g_object_unref (stringlist);
      return;
    }

  buffer = g_buffered_input_stream_peek_buffer (stream, &size);

  if (n_filled == 0)
    {
      if (size)
        gtk_string_list_take (stringlist, g_utf8_make_valid (buffer, size));
      g_object_unref (stringlist);
      return;
    }

  lines = NULL;
  while ((newline = memchr (buffer, '\n', size)))
    {
      if (newline > buffer)
        {
          if (lines == NULL)
            lines = g_ptr_array_new_with_free_func (g_free);
          g_ptr_array_add (lines, g_utf8_make_valid (buffer, newline - buffer));
        }
      if (g_input_stream_skip (G_INPUT_STREAM (stream), newline - buffer + 1, NULL, &error) < 0)
        {
          g_clear_error (&error);
          break;
        }
      buffer = g_buffered_input_stream_peek_buffer (stream, &size);
    }
  if (lines == NULL)
    {
      g_buffered_input_stream_set_buffer_size (stream, g_buffered_input_stream_get_buffer_size (stream) + 4096);
    }
  else
    {
      g_ptr_array_add (lines, NULL);
      gtk_string_list_splice (stringlist, g_list_model_get_n_items (G_LIST_MODEL (stringlist)), 0, (const char **) lines->pdata);
      g_ptr_array_free (lines, TRUE);
    }

  g_buffered_input_stream_fill_async (stream, -1, G_PRIORITY_HIGH_IDLE, NULL, read_lines_cb, data);
}

static void
file_is_open_cb (GObject      *file,
                 GAsyncResult *result,
                 gpointer      data)
{
  GError *error = NULL;
  GFileInputStream *file_stream;
  GBufferedInputStream *stream;

  file_stream = g_file_read_finish (G_FILE (file), result, &error);
  if (file_stream == NULL)
    {
      g_print ("Could not open file: %s\n", error->message);
      g_error_free (error);
      g_object_unref (data);
      return;
    }

  stream = G_BUFFERED_INPUT_STREAM (g_buffered_input_stream_new (G_INPUT_STREAM (file_stream)));
  g_buffered_input_stream_fill_async (stream, -1, G_PRIORITY_HIGH_IDLE, NULL, read_lines_cb, data);
  g_object_unref (stream);
}

static void
load_file (GtkStringList *list,
           GFile         *file)
{
  gtk_string_list_splice (list, 0, g_list_model_get_n_items (G_LIST_MODEL (list)), NULL);
  g_file_read_async (file, G_PRIORITY_HIGH_IDLE, NULL, file_is_open_cb, g_object_ref (list));
}

static void
toggle_cb (GtkCheckButton *check, GtkWidget *list)
{
  GtkListItemFactory *header_factory = NULL;

  if (gtk_check_button_get_active (check))
    {
      header_factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (header_factory, "setup", G_CALLBACK (setup_header), NULL);
      g_signal_connect (header_factory, "bind", G_CALLBACK (bind_header), NULL);
    }

  if (GTK_IS_LIST_VIEW (list))
    gtk_list_view_set_header_factory (GTK_LIST_VIEW (list), header_factory);
  else
    gtk_grid_view_set_header_factory (GTK_GRID_VIEW (list), header_factory);

  g_clear_object (&header_factory);
}

static void
value_changed_cb (GtkAdjustment *adj, gpointer data)
{
  g_print ("horizontal adjustment changed to %f\n",  gtk_adjustment_get_value (adj));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *lv;
  GtkWidget *gv;
  GtkWidget *header;
  GtkWidget *toggle;
  GtkWidget *switcher;
  GtkWidget *stack;
  GtkListItemFactory *factory;
  GtkExpression *expression;
  GtkSortListModel *sortmodel;
  GtkSelectionModel *selection;
  GtkStringList *stringlist;
  GtkAdjustment *adj;

  stringlist = gtk_string_list_new (NULL);

  if (argc > 1)
    {
      GFile *file = g_file_new_for_commandline_arg (argv[1]);
      load_file (stringlist, file);
      g_object_unref (file);
    }
  else
    {
      for (int i = 0; strings[i]; i++)
        gtk_string_list_append (stringlist, strings[i]);
    }

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  toggle = gtk_check_button_new ();
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), toggle);

  stack = gtk_stack_new ();
  gtk_window_set_child (GTK_WINDOW (window), stack);

  switcher = gtk_stack_switcher_new ();
  gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), switcher);

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  expression = gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string");
  sortmodel = gtk_sort_list_model_new (G_LIST_MODEL (stringlist),
                                       GTK_SORTER (gtk_string_sorter_new (expression)));
  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback) get_first, NULL, NULL);
  gtk_sort_list_model_set_section_sorter (sortmodel, GTK_SORTER (gtk_string_sorter_new (expression)));
  selection = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (sortmodel)));

  sw = gtk_scrolled_window_new ();
  gtk_stack_add_titled (GTK_STACK (stack), sw, "list", "List");

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  lv = gtk_list_view_new (g_object_ref (selection), factory);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), lv);

  g_signal_connect (toggle, "toggled", G_CALLBACK (toggle_cb), lv);

  sw = gtk_scrolled_window_new ();
  gtk_stack_add_titled (GTK_STACK (stack), sw, "grid", "Grid");

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  gv = gtk_grid_view_new (g_object_ref (selection), factory);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), gv);

  g_signal_connect (toggle, "toggled", G_CALLBACK (toggle_cb), gv);

  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (gv), 5);

  adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (sw));
  g_signal_connect (adj, "value-changed", G_CALLBACK (value_changed_cb), NULL);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, FALSE);

  g_object_unref (selection);
  g_object_unref (stringlist);

  return 0;
}
