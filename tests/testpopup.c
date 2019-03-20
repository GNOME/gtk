#include <gtk/gtk.h>

static void
clicked (GtkButton *button)
{
  g_print ("Yes!\n");
}

static GtkWidget *
add_content (GtkWidget *parent)
{
  GtkWidget *box, *label, *entry, *button;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

  label = gtk_label_new_with_mnemonic ("_Test");
  entry = gtk_entry_new ();
  button = gtk_button_new_with_mnemonic ("_Yes!");
  g_signal_connect (button, "clicked", G_CALLBACK (clicked), NULL);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_set_can_default (button, TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), entry);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_container_add (GTK_CONTAINER (parent), box);

  gtk_widget_grab_default (button);

  return label;
}

static GtkWidget *
create_popup (GtkWidget *parent)
{
  GtkWidget *popup;

  popup = gtk_popup_new ();

  gtk_popup_set_relative_to (GTK_POPUP (popup), parent);
  gtk_style_context_add_class (gtk_widget_get_style_context (popup), "background");
  gtk_style_context_add_class (gtk_widget_get_style_context (popup), "frame");

  add_content (popup);

  return popup;
}

static void
show_popup (GtkToggleButton *button, GtkWidget *popup)
{
  gtk_widget_show (popup);
}

static GtkWidget *
enum_combo (GType type)
{
  GEnumClass *class;
  GtkWidget *combo;
  int i;

  class = g_type_class_ref (type);

  combo = gtk_combo_box_text_new ();

  for (i = 0; i < class->n_values; i++)
    {
      GEnumValue *value = &class->values[i];
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo),
                                 value->value_name, value->value_nick);
    }

  g_type_class_unref (class);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  return combo;
}

static gboolean
to_enum (GBinding     *binding,
         const GValue *from_value,
         GValue       *to_value,
         gpointer      data)
{
  GEnumClass *class = data;
  GEnumValue *value;
  const char *id;

  id = g_value_get_string (from_value);
  value = g_enum_get_value_by_name (class, id);
  g_value_set_enum (to_value, value->value);

  return TRUE;
}

static gboolean
from_enum (GBinding     *binding,
           const GValue *from_value,
           GValue       *to_value,
           gpointer      data)
{
  GEnumClass *class = data;
  GEnumValue *value;
  int v;

  v = g_value_get_enum (from_value);
  value = g_enum_get_value (class, v);
  g_value_set_string (to_value, value->value_name);

  return TRUE;
}

static GtkWidget *flip_x;
static GtkWidget *flip_y;
static GtkWidget *slide_x;
static GtkWidget *slide_y;
static GtkWidget *resize_x;
static GtkWidget *resize_y;
static GtkWidget *offset_x;
static GtkWidget *offset_y;

static void
update_hints (GtkPopup *popup)
{
  GdkAnchorHints hints = 0;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (flip_x)))
    hints |= GDK_ANCHOR_FLIP_X; 
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (flip_y)))
    hints |= GDK_ANCHOR_FLIP_Y; 
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (slide_x)))
    hints |= GDK_ANCHOR_SLIDE_X; 
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (slide_y)))
    hints |= GDK_ANCHOR_SLIDE_Y; 
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (resize_x)))
    hints |= GDK_ANCHOR_RESIZE_X; 
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (resize_y)))
    hints |= GDK_ANCHOR_RESIZE_Y; 

  g_object_set (popup, "anchor-hints", hints, NULL);
}

static void
update_offset (GtkPopup *popup)
{
  double x, y;

  g_object_get (offset_x, "value", &x, NULL);
  g_object_get (offset_y, "value", &y, NULL);

  g_object_set (popup,
                "anchor-offset-x", (int)x,
                "anchor-offset-y", (int)y,
                NULL);
}
                 
int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *entry;
  GtkWidget *box;
  GtkWidget *popup;
  GtkWidget *button;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *combo;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_object_set (box, "margin", 10, NULL);

  entry = add_content (box);

  popup = create_popup (entry);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_container_add (GTK_CONTAINER (box), grid);

  combo = enum_combo (GDK_TYPE_GRAVITY);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "GDK_GRAVITY_SOUTH");
  g_object_bind_property_full (combo, "active-id",
                               popup, "parent-anchor",
                               G_BINDING_SYNC_CREATE,
                               to_enum,
                               from_enum,
                               g_type_class_ref (GDK_TYPE_GRAVITY),
                               g_type_class_unref);

  label = gtk_label_new ("Parent anchor:");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 2, 1);
                              
  combo = enum_combo (GDK_TYPE_GRAVITY);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "GDK_GRAVITY_NORTH");
  g_object_bind_property_full (combo, "active-id",
                               popup, "surface-anchor",
                               G_BINDING_SYNC_CREATE,
                               to_enum,
                               from_enum,
                               g_type_class_ref (GDK_TYPE_GRAVITY),
                               g_type_class_unref);

  label = gtk_label_new ("Surface anchor:");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, 1, 2, 1);

  flip_x = gtk_check_button_new_with_label ("X");
  g_signal_connect_swapped (flip_x, "toggled", G_CALLBACK (update_hints), popup);
  flip_y = gtk_check_button_new_with_label ("Y");
  g_signal_connect_swapped (flip_y, "toggled", G_CALLBACK (update_hints), popup);
  
  label = gtk_label_new ("Flip:");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), flip_x, 1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), flip_y, 2, 2, 1, 1);

  slide_x = gtk_check_button_new_with_label ("X");
  g_signal_connect_swapped (slide_x, "toggled", G_CALLBACK (update_hints), popup);
  slide_y = gtk_check_button_new_with_label ("Y");
  g_signal_connect_swapped (slide_y, "toggled", G_CALLBACK (update_hints), popup);
  
  label = gtk_label_new ("Slide:");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), slide_x, 1, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), slide_y, 2, 3, 1, 1);

  resize_x = gtk_check_button_new_with_label ("X");
  g_signal_connect_swapped (resize_x, "toggled", G_CALLBACK (update_hints), popup);
  resize_y = gtk_check_button_new_with_label ("Y");
  g_signal_connect_swapped (resize_y, "toggled", G_CALLBACK (update_hints), popup);
  
  label = gtk_label_new ("Resize:");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), resize_x, 1, 4, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), resize_y, 2, 4, 1, 1);

  label = gtk_label_new ("Offset:");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  offset_x = gtk_spin_button_new_with_range (-20, 20, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (offset_x), 0);
  g_signal_connect_swapped (offset_x, "notify::value", G_CALLBACK (update_offset), popup);
  offset_y = gtk_spin_button_new_with_range (-20, 20, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (offset_y), 0);
  g_signal_connect_swapped (offset_y, "notify::value", G_CALLBACK (update_offset), popup);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 5, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), offset_x, 1, 5, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), offset_y, 2, 5, 1, 1);

  button = gtk_button_new_with_mnemonic ("_Popup");
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  g_signal_connect (button, "clicked", G_CALLBACK (show_popup), popup);
  gtk_container_add (GTK_CONTAINER (box), button);
  
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
