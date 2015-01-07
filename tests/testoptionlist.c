#include <gtk/gtk.h>

static void
add_one (GtkButton *button, gpointer data)
{
  static gint count = 3;
  gchar *text;
  gchar *sort;
  gchar *id;

  count++;

  id = g_strdup_printf ("%d", count);
  text = g_strdup_printf ("Value %d", count);
  sort = g_strdup_printf ("Value %03d", count);
  gtk_option_list_add_item (GTK_OPTION_LIST (data), id, text);
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (data), id, sort);
  gtk_option_list_select_item (GTK_OPTION_LIST (data), id);
  g_free (id);
  g_free (text);
  g_free (sort);
}

static void
remove_selected (GtkButton *button, gpointer data)
{
  const gchar **ids;

  ids = gtk_option_list_get_selected_items (GTK_OPTION_LIST (data));
  if (ids[0] != NULL)
    gtk_option_list_remove_item (GTK_OPTION_LIST (data), ids[0]);
}

static void
select_a (GtkButton *button, gpointer data)
{
  gtk_option_list_select_item (GTK_OPTION_LIST (data), "1");
}

static void
unselect_a (GtkButton *button, gpointer data)
{
  gtk_option_list_unselect_item (GTK_OPTION_LIST (data), "1");
}

const gchar data[] =
  "<interface>"
  "  <object class='GtkOptionButton' id='button'>"
  "    <property name='visible'>True</property>"
  "    <property name='halign'>center</property>"
  "    <property name='placeholder-text'>None</property>"
  "    <child internal-child='list'>"
  "      <object class='GtkOptionList'>"
  "        <property name='custom-text'>Other</property>"
  "        <property name='selection-mode'>multiple</property>"
  "        <property name='selected'>1</property>"
  "        <items>"
  "          <item translatable='yes' id='1' sort='Value 001'>Value 1</item>"
  "          <item translatable='yes' id='2' sort='Value 002'>Value 2</item>"
  "          <item translatable='yes' id='3' sort='Value 003'>Value 3</item>"
  "          <item translatable='yes' id='4' sort='Value 004' group='1'>Value 4</item>"
  "          <item translatable='yes' id='5' sort='Value 005' group='1'>Value 5</item>"
  "        </items>"
  "        <groups>"
  "          <group id='1' translatable='yes'>Group 1</group>"
  "        </groups>"
  "      </object>"
  "    </child>"
  "  </object>"
  "</interface>";

static gboolean
selected_to_bool (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      data)
{
  const gchar * const *ids;

  ids = g_value_get_boxed (from_value);
  g_value_set_boolean (to_value, ids[0] != NULL);
  return TRUE;
}

static gboolean
selected_to_string (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      data)
{
  gchar **ids;
  gchar *text;

  ids = g_value_get_boxed (from_value);
  text = g_strjoinv (", ", ids);
  g_value_set_string (to_value, text);
  g_free (text);

  return TRUE;
}

static gboolean
selected_to_text (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      data)
{
  const gchar * const *ids;
  GString *str;
  gint i;

  str = g_string_new ("");
  ids = g_value_get_boxed (from_value);
  for (i = 0; ids[i]; i++)
    {
      const gchar *text;
      if (i > 0)
        g_string_append (str, ", ");
      text = gtk_option_list_item_get_text (GTK_OPTION_LIST (g_binding_get_source (binding)), ids[i]);
      g_string_append (str, text);
    }

  g_value_set_string (to_value, str->str);
  g_string_free (str, TRUE);

  return TRUE;
}

static void
list_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
  if (before != NULL)
    {
      if (gtk_list_box_row_get_header (row) == NULL)
        gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
    }
}

static void
selected_changed (GObject    *olist,
                  GParamSpec *pspec,
                  gpointer    data)
{
  GtkWidget *label = data;
  const gchar **ids;
  gchar *text;

  ids = gtk_option_list_get_selected_items (GTK_OPTION_LIST (olist));
  text = g_strdup_printf ("Row 2: %s", gtk_option_list_item_get_text (GTK_OPTION_LIST (olist), ids[0]));

  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);
}

static void
row_activated (GtkListBox    *list,
               GtkListBoxRow *row,
               gpointer       data)
{
  GtkWidget *popover = data;

  if (GTK_WIDGET (row) == gtk_popover_get_relative_to (GTK_POPOVER (popover)))
    gtk_widget_show (popover);
}

static gboolean
row_key_press (GtkWidget *row,
               GdkEvent  *event,
               gpointer   data)
{
  if (gtk_option_list_handle_key_event (GTK_OPTION_LIST (data), event) == GDK_EVENT_STOP)
    {
      gtk_widget_show (gtk_widget_get_parent (GTK_WIDGET (data)));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *hbox;
  GtkWidget *box;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *olist;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *list;
  GtkWidget *row;
  GtkWidget *popover;
  GtkBuilder *builder;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (hbox, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add (GTK_CONTAINER (hbox), box);

  label = gtk_label_new ("Simple");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  button = gtk_option_button_new ();
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_option_button_set_placeholder_text (GTK_OPTION_BUTTON (button), "None");
  olist = gtk_option_button_get_option_list (GTK_OPTION_BUTTON (button));
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "1", "Value 1");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "2", "Value 2");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "3", "Value 3");
  gtk_option_list_select_item (GTK_OPTION_LIST (olist), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("With search and collapsing");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  button = gtk_option_button_new ();
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_option_button_set_placeholder_text (GTK_OPTION_BUTTON (button), "None");
  olist = gtk_option_button_get_option_list (GTK_OPTION_BUTTON (button));
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "1",  "Value 1");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "2",  "Value 2");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "3",  "Value 3");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "4",  "Value 4");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "5",  "Value 5");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "6",  "Value 6");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "7",  "Value 7");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "8",  "Value 8");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "9",  "Value 9");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "10", "Value 10");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "11", "Value 11");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "1",  "Value 01");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "2",  "Value 02");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "3",  "Value 03");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "4",  "Value 04");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "5",  "Value 05");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "6",  "Value 06");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "7",  "Value 07");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "8",  "Value 08");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "9",  "Value 09");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "10",  "Value 10");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "11",  "Value 11");
  gtk_option_list_select_item (GTK_OPTION_LIST (olist), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("With free-form text");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  olist = gtk_option_button_new ();
  gtk_widget_set_halign (olist, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), olist);
  olist = gtk_option_button_get_option_list (GTK_OPTION_BUTTON (olist));
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "1", "Value 1");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "2", "Value 2");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "3", "Value 3");
  gtk_option_list_set_allow_custom (GTK_OPTION_LIST (olist), TRUE);
  gtk_option_list_select_item (GTK_OPTION_LIST (olist), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("With grouping");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  olist = gtk_option_button_new ();
  gtk_widget_set_halign (olist, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), olist);
  olist = gtk_option_button_get_option_list (GTK_OPTION_BUTTON (olist));
  gtk_option_list_add_group (GTK_OPTION_LIST (olist), "Group 3", "G 3", "Group 3");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "1",  "Value 1");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "2",  "Value 2");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "3",  "Value 3");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "4",  "Value 4");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "5",  "Value 5");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "6",  "Value 6");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "7",  "Value 7");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "8",  "Value 8");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist),  "9",  "Value 9");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "10", "Value 10");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "11", "Value 11");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "12", "Value 12");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "13", "Value 13");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "14", "Value 14");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "15", "Value 15");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "16", "Value 16");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "17", "Value 17");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "18", "Value 18");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "1",  "Value 01");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "2",  "Value 02");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "3",  "Value 03");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "4",  "Value 04");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "5",  "Value 05");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "6",  "Value 06");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "7",  "Value 07");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "8",  "Value 08");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist),  "9",  "Value 09");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "10",  "Value 10");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "11",  "Value 11");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "12",  "Value 12");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "13",  "Value 13");
  gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (olist), "14",  "Value 14");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "1",  "Group 1");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "2",  "Group 1");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "3",  "Group 1");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "4",  "Group 1");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "5",  "Group 2");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "6",  "Group 2");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "7",  "Group 2");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "8",  "Group 2");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist),  "9",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "10",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "11",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "12",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "13",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "14",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "15",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "16",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "17",  "Group 3");
  gtk_option_list_item_set_group_key (GTK_OPTION_LIST (olist), "18",  "Group 3");
  gtk_option_list_select_item (GTK_OPTION_LIST (olist), "7");
  button = gtk_button_new_with_label ("Remove selected");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  g_signal_connect (button, "clicked", G_CALLBACK (remove_selected), olist);
  gtk_container_add (GTK_CONTAINER (box), button);
  g_object_bind_property_full (olist, "selected",
                               button, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               selected_to_bool, NULL, NULL, NULL);

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("Builder");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  builder = gtk_builder_new_from_string (data, -1);
  button = (GtkWidget *)gtk_builder_get_object (builder, "button");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), button);
  olist = gtk_option_button_get_option_list (GTK_OPTION_BUTTON (button));
  g_object_unref (builder);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), box2);
  button = gtk_button_new_with_label ("Add value");
  g_signal_connect (button, "clicked", G_CALLBACK (add_one), olist);
  gtk_container_add (GTK_CONTAINER (box2), button);
  button = gtk_button_new_with_label ("Select 1");
  g_signal_connect (button, "clicked", G_CALLBACK (select_a), olist);
  gtk_container_add (GTK_CONTAINER (box2), button);
  button = gtk_button_new_with_label ("Unselect 1");
  g_signal_connect (button, "clicked", G_CALLBACK (unselect_a), olist);
  gtk_container_add (GTK_CONTAINER (box2), button);
  button = gtk_button_new_with_label ("Remove selected");
  g_signal_connect (button, "clicked", G_CALLBACK (remove_selected), olist);
  gtk_container_add (GTK_CONTAINER (box2), button);
  g_object_bind_property_full (olist, "selected",
                               button, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               selected_to_bool, NULL, NULL, NULL);
  button = gtk_check_button_new_with_label ("Allow custom");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  g_object_bind_property (button, "active",
                          olist, "allow-custom",
                          0);
  gtk_container_add (GTK_CONTAINER (box), button);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), box2);
  label = gtk_label_new ("Active:");
  gtk_container_add (GTK_CONTAINER (box2), label);
  label = gtk_label_new ("");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  g_object_bind_property_full (olist, "selected",
                               label, "label",
                               G_BINDING_SYNC_CREATE,
                               selected_to_string, NULL, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (box2), label);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), box2);
  label = gtk_label_new ("Label:");
  gtk_container_add (GTK_CONTAINER (box2), label);
  label = gtk_label_new ("");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  g_object_bind_property_full (olist, "selected",
                               label, "label",
                               G_BINDING_SYNC_CREATE,
                               selected_to_text, NULL, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (box2), label);

  gtk_container_add (GTK_CONTAINER (hbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add (GTK_CONTAINER (hbox), box);

  label = gtk_label_new ("Embedded");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  olist = gtk_option_list_new ();
  gtk_widget_set_halign (olist, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), olist);
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "1", "Value 1");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "2", "Value 2");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "3", "Value 3");
  gtk_option_list_set_allow_custom (GTK_OPTION_LIST (olist), TRUE);
  gtk_option_list_select_item (GTK_OPTION_LIST (olist), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("On a list");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (box), frame);
  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (frame), list);
  gtk_list_box_set_header_func (GTK_LIST_BOX (list), list_header_func, NULL, NULL);
  label = gtk_label_new ("Row 1");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (list), label);
  row = gtk_widget_get_parent (label);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  label = gtk_label_new ("Row 2");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (list), label);
  row = gtk_widget_get_parent (label);

  olist = gtk_option_list_new ();
  gtk_widget_set_halign (olist, GTK_ALIGN_CENTER);
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "1", "Value 1");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "2", "Value 2");
  gtk_option_list_add_item (GTK_OPTION_LIST (olist), "3", "Value 3");
  gtk_widget_show (olist);

  popover = gtk_popover_new (row);
  gtk_container_add (GTK_CONTAINER (popover), olist);

  g_signal_connect (olist, "notify::selected", G_CALLBACK (selected_changed), label);
  g_signal_connect (list, "row-activated", G_CALLBACK (row_activated), popover);
  g_signal_connect (row, "key-press-event", G_CALLBACK (row_key_press), olist);

  label = gtk_label_new ("Row 3");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (list), label);
  row = gtk_widget_get_parent (label);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
