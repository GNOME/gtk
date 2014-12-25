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
  gtk_combo_add_item (GTK_COMBO (data), id, text, sort);
  gtk_combo_set_active (GTK_COMBO (data), id);
  g_free (id);
  g_free (text);
  g_free (sort);
}

static void
remove_active (GtkButton *button, gpointer data)
{
  const gchar *id;

  id = gtk_combo_get_active (GTK_COMBO (data));
  gtk_combo_remove_item (GTK_COMBO (data), id);
}

static void
select_a (GtkButton *button, gpointer data)
{
  gtk_combo_set_active (GTK_COMBO (data), "1");
}

const gchar data[] =
  "<interface>"
  "  <object class='GtkCombo' id='combo'>"
  "    <property name='visible'>True</property>"
  "    <property name='halign'>center</property>"
  "    <property name='placeholder'>None</property>"
  "    <property name='active'>1</property>"
  "    <items>"
  "      <item translatable='yes' id='1' sort='Value 001'>Value 1</item>"
  "      <item translatable='yes' id='2' sort='Value 002'>Value 2</item>"
  "      <item translatable='yes' id='3' sort='Value 003'>Value 3</item>"
  "    </items>"
  "  </object>"
  "</interface>";

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *box2, *button;
  GtkWidget *combo;
  GtkWidget *label;
  GtkBuilder *builder;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add (GTK_CONTAINER (window), box);

  label = gtk_label_new ("Simple");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  combo = gtk_combo_new ();
  gtk_widget_set_halign (combo, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), combo);
  gtk_combo_add_item (GTK_COMBO (combo), "1", "Value 1", NULL);
  gtk_combo_add_item (GTK_COMBO (combo), "2", "Value 2", NULL);
  gtk_combo_add_item (GTK_COMBO (combo), "3", "Value 3", NULL);
  gtk_combo_set_placeholder (GTK_COMBO (combo), "None");
  gtk_combo_set_active (GTK_COMBO (combo), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("With search and collapsing");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  combo = gtk_combo_new ();
  gtk_widget_set_halign (combo, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), combo);
  gtk_combo_add_item (GTK_COMBO (combo),  "1",  "Value 1", "Value 01");
  gtk_combo_add_item (GTK_COMBO (combo),  "2",  "Value 2", "Value 02");
  gtk_combo_add_item (GTK_COMBO (combo),  "3",  "Value 3", "Value 03");
  gtk_combo_add_item (GTK_COMBO (combo),  "4",  "Value 4", "Value 04");
  gtk_combo_add_item (GTK_COMBO (combo),  "5",  "Value 5", "Value 05");
  gtk_combo_add_item (GTK_COMBO (combo),  "6",  "Value 6", "Value 06");
  gtk_combo_add_item (GTK_COMBO (combo),  "7",  "Value 7", "Value 07");
  gtk_combo_add_item (GTK_COMBO (combo),  "8",  "Value 8", "Value 08");
  gtk_combo_add_item (GTK_COMBO (combo),  "9",  "Value 9", "Value 09");
  gtk_combo_add_item (GTK_COMBO (combo), "10", "Value 10", "Value 10");
  gtk_combo_add_item (GTK_COMBO (combo), "11", "Value 11", "Value 11");
  gtk_combo_set_placeholder (GTK_COMBO (combo), "None");
  gtk_combo_set_active (GTK_COMBO (combo), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("With free-form text");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  combo = gtk_combo_new ();
  gtk_widget_set_halign (combo, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), combo);
  gtk_combo_add_item (GTK_COMBO (combo), "1", "Value 1", NULL);
  gtk_combo_add_item (GTK_COMBO (combo), "2", "Value 2", NULL);
  gtk_combo_add_item (GTK_COMBO (combo), "3", "Value 3", NULL);
  gtk_combo_set_placeholder (GTK_COMBO (combo), "None");
  gtk_combo_set_allow_custom (GTK_COMBO (combo), TRUE);
  gtk_combo_set_active (GTK_COMBO (combo), "1");

  gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  label = gtk_label_new ("Builder");
  gtk_widget_set_margin_start (label, 10);
  gtk_container_add (GTK_CONTAINER (box), label);

  builder = gtk_builder_new_from_string (data, -1);
  combo = (GtkWidget *)gtk_builder_get_object (builder, "combo");
  gtk_widget_set_halign (combo, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), combo);
  g_object_unref (builder);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), box2);
  button = gtk_button_new_with_label ("Add value");
  g_signal_connect (button, "clicked", G_CALLBACK (add_one), combo);
  gtk_container_add (GTK_CONTAINER (box2), button);
  button = gtk_button_new_with_label ("Select 1");
  g_signal_connect (button, "clicked", G_CALLBACK (select_a), combo);
  gtk_container_add (GTK_CONTAINER (box2), button);
  button = gtk_button_new_with_label ("Remove active");
  g_signal_connect (button, "clicked", G_CALLBACK (remove_active), combo);
  gtk_container_add (GTK_CONTAINER (box2), button);
  button = gtk_check_button_new_with_label ("Allow custom");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  g_object_bind_property (button, "active",
                          combo, "allow-custom",
                          0);
  gtk_container_add (GTK_CONTAINER (box), button);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), box2);
  label = gtk_label_new ("Active:");
  gtk_container_add (GTK_CONTAINER (box2), label);
  label = gtk_label_new ("");
  g_object_bind_property (combo, "active",
                          label, "label",
                          G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box2), label);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
