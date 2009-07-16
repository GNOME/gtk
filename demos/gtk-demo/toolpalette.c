/* Tool Palette
 *
 * A tool palette widget shows groups of toolbar items as a grid of icons
 * or a list of names.
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

static GtkWidget *window = NULL;

static void load_stock_items (GtkToolPalette *palette);
static void load_toggle_items (GtkToolPalette *palette);
static void load_special_items (GtkToolPalette *palette);

static void on_combo_orientation_changed(GtkComboBox *combo_box, gpointer user_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (user_data);
  GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW (GTK_WIDGET (palette)->parent);
  GtkTreeModel *model = gtk_combo_box_get_model (combo_box);
  
  GtkTreeIter iter;
  if(!gtk_combo_box_get_active_iter(combo_box, &iter))
    return;

  gint val = 0;
  gtk_tree_model_get (model, &iter, 1, &val, -1);
  
  gtk_orientable_set_orientation (GTK_ORIENTABLE (palette), val);

  if (val == GTK_ORIENTATION_HORIZONTAL)
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  else
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
}

static void on_combo_style_changed(GtkComboBox *combo_box, gpointer user_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (user_data);
  GtkTreeModel *model = gtk_combo_box_get_model (combo_box);
  
  GtkTreeIter iter;
  if(!gtk_combo_box_get_active_iter(combo_box, &iter))
    return;

  gint val = 0;
  gtk_tree_model_get (model, &iter, 1, &val, -1);
  
  if (val == -1)
    gtk_tool_palette_unset_style (palette);
  else
    gtk_tool_palette_set_style (palette, val);
}

GtkWidget *
do_toolpalette (GtkWidget *do_widget)
{
  GtkWidget *box = NULL;
  GtkWidget *combo_orientation = NULL;
  GtkListStore *combo_orientation_model = NULL;
  GtkWidget *combo_style = NULL;
  GtkListStore *combo_style_model = NULL;
  GtkCellRenderer *cell_renderer = NULL;
  GtkTreeIter iter;
  GtkWidget *palette = NULL;
  GtkWidget *palette_scroller = NULL;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Tool Palette");
      gtk_window_set_default_size (GTK_WINDOW (window), 200, 600);

      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      /* Add widgets to control the ToolPalette appearance: */
      box = gtk_vbox_new (FALSE, 6);
      gtk_container_add (GTK_CONTAINER (window), box);
      
      /* Orientation combo box: */
      combo_orientation_model = gtk_list_store_new (2, 
        G_TYPE_STRING, G_TYPE_INT);
      gtk_list_store_append (combo_orientation_model, &iter);
      gtk_list_store_set (combo_orientation_model, &iter,
        0, "Horizontal", 1, GTK_ORIENTATION_HORIZONTAL, -1);
      gtk_list_store_append (combo_orientation_model, &iter);
      gtk_list_store_set (combo_orientation_model, &iter,
        0, "Vertical", 1, GTK_ORIENTATION_VERTICAL, -1);
      combo_orientation = gtk_combo_box_new_with_model (  
        GTK_TREE_MODEL (combo_orientation_model));
      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_orientation), 
        cell_renderer, TRUE);
      gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_orientation), 
        cell_renderer, "text", 0, NULL); 
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_orientation), 
        &iter);
      gtk_box_pack_start (GTK_BOX (box), combo_orientation, FALSE, FALSE, 0);
      
      /* Style combo box: */
      combo_style_model = gtk_list_store_new (2, 
        G_TYPE_STRING, G_TYPE_INT);
      gtk_list_store_append (combo_style_model, &iter);
      gtk_list_store_set (combo_style_model, &iter,
        0, "Text", 1, GTK_TOOLBAR_TEXT, -1);
      gtk_list_store_append (combo_style_model, &iter);
      gtk_list_store_set (combo_style_model, &iter,
        0, "Both", 1, GTK_TOOLBAR_BOTH, -1);
      gtk_list_store_append (combo_style_model, &iter);
      gtk_list_store_set (combo_style_model, &iter,
        0, "Both: Horizontal", 1, GTK_TOOLBAR_BOTH_HORIZ, -1);
      gtk_list_store_append (combo_style_model, &iter);
      gtk_list_store_set (combo_style_model, &iter,
        0, "Icons", 1, GTK_TOOLBAR_ICONS, -1);
      gtk_list_store_append (combo_style_model, &iter);
      gtk_list_store_set (combo_style_model, &iter,
        0, "Default", 1, -1 /* A custom meaning for this demo. */, -1);
      combo_style = gtk_combo_box_new_with_model (  
        GTK_TREE_MODEL (combo_style_model));
      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_style), 
        cell_renderer, TRUE);
      gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_style), 
        cell_renderer, "text", 0, NULL); 
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_style), 
        &iter);
      gtk_box_pack_start (GTK_BOX (box), combo_style, FALSE, FALSE, 0);

      /* Add and fill the ToolPalette: */
      palette = gtk_tool_palette_new ();
      
      load_stock_items (GTK_TOOL_PALETTE (palette));
      load_toggle_items (GTK_TOOL_PALETTE (palette));
      load_special_items (GTK_TOOL_PALETTE (palette));
      
      palette_scroller = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (palette_scroller),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_container_set_border_width (GTK_CONTAINER (palette_scroller), 6);
      gtk_container_add (GTK_CONTAINER (palette_scroller), palette);
      
      gtk_container_add (GTK_CONTAINER (box), palette_scroller);
      
      gtk_widget_show_all (box);
      
      /* Connect signals: */
      g_signal_connect (combo_orientation, "changed", 
        G_CALLBACK (on_combo_orientation_changed), palette);
      g_signal_connect (combo_style, "changed", 
        G_CALLBACK (on_combo_style_changed), palette);
      
      /* Keep the widgets in sync: */
      on_combo_orientation_changed (GTK_COMBO_BOX (combo_orientation), palette);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}


static void
load_stock_items (GtkToolPalette *palette)
{
  GtkWidget *group_af = gtk_tool_item_group_new (_("Stock Icons (A-F)"));
  GtkWidget *group_gn = gtk_tool_item_group_new (_("Stock Icons (G-N)"));
  GtkWidget *group_or = gtk_tool_item_group_new (_("Stock Icons (O-R)"));
  GtkWidget *group_sz = gtk_tool_item_group_new (_("Stock Icons (S-Z)"));
  GtkWidget *group = NULL;

  GtkToolItem *item;
  GSList *stock_ids;
  GSList *iter;

  stock_ids = gtk_stock_list_ids ();
  stock_ids = g_slist_sort (stock_ids, (GCompareFunc) strcmp);

  gtk_container_add (GTK_CONTAINER (palette), group_af);
  gtk_container_add (GTK_CONTAINER (palette), group_gn);
  gtk_container_add (GTK_CONTAINER (palette), group_or);
  gtk_container_add (GTK_CONTAINER (palette), group_sz);

  for (iter = stock_ids; iter; iter = g_slist_next (iter))
    {
      GtkStockItem stock_item;
      gchar *id = iter->data;

      switch (id[4])
        {
          case 'a':
            group = group_af;
            break;

          case 'g':
            group = group_gn;
            break;

          case 'o':
            group = group_or;
            break;

          case 's':
            group = group_sz;
            break;
        }

      item = gtk_tool_button_new_from_stock (id);
      gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (item), id);
      gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
      gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);

      if (!gtk_stock_lookup (id, &stock_item) || !stock_item.label)
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), id);

      g_free (id);
    }

  g_slist_free (stock_ids);
}

static void
load_toggle_items (GtkToolPalette *palette)
{
  GSList *toggle_group = NULL;
  GtkToolItem *item;
  GtkWidget *group;
  char *label;
  int i;

  group = gtk_tool_item_group_new (_("Radio Item"));
  gtk_container_add (GTK_CONTAINER (palette), group);

  for (i = 1; i <= 10; ++i)
    {
      label = g_strdup_printf ("#%d", i);
      item = gtk_radio_tool_button_new (toggle_group);
      gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), label);
      g_free (label);

      gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
      toggle_group = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (item));
    }
}

static GtkToolItem *
create_entry_item (const char *text)
{
  GtkToolItem *item;
  GtkWidget *entry;

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), text);
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 5);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), entry);

  return item;
}

static void
load_special_items (GtkToolPalette *palette)
{
  GtkToolItem *item;
  GtkWidget *group;
  GtkWidget *label_button;

  group = gtk_tool_item_group_new (NULL);
  label_button = gtk_button_new_with_label (_("Advanced Features"));
  gtk_widget_show (label_button);
  gtk_tool_item_group_set_label_widget (GTK_TOOL_ITEM_GROUP (group),
    label_button);
  gtk_container_add (GTK_CONTAINER (palette), group);

  item = create_entry_item ("homogeneous=FALSE");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_container_child_set (GTK_CONTAINER (group), GTK_WIDGET (item),
                           "homogeneous", FALSE, NULL);

  item = create_entry_item ("homogeneous=FALSE, expand=TRUE");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_container_child_set (GTK_CONTAINER (group), GTK_WIDGET (item),
                           "homogeneous", FALSE, "expand", TRUE,
                           NULL);

  item = create_entry_item ("homogeneous=FALSE, expand=TRUE, fill=FALSE");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_container_child_set (GTK_CONTAINER (group), GTK_WIDGET (item),
                           "homogeneous", FALSE, "expand", TRUE,
                           "fill", FALSE, NULL);

  item = create_entry_item ("homogeneous=FALSE, expand=TRUE, new-row=TRUE");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_container_child_set (GTK_CONTAINER (group), GTK_WIDGET (item),
                           "homogeneous", FALSE, "expand", TRUE,
                           "new-row", TRUE, NULL);

  item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_UP);
  gtk_tool_item_set_tooltip_text (item, "Show on vertical palettes only");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_tool_item_set_visible_horizontal (item, FALSE);

  item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  gtk_tool_item_set_tooltip_text (item, "Show on horizontal palettes only");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_tool_item_set_visible_vertical (item, FALSE);

  item = gtk_tool_button_new_from_stock (GTK_STOCK_DELETE);
  gtk_tool_item_set_tooltip_text (item, "Do not show at all");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_widget_set_no_show_all (GTK_WIDGET (item), TRUE);

  item = gtk_tool_button_new_from_stock (GTK_STOCK_FULLSCREEN);
  gtk_tool_item_set_tooltip_text (item, "Expanded this item");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
  gtk_container_child_set (GTK_CONTAINER (group), GTK_WIDGET (item),
                           "homogeneous", FALSE,
                           "expand", TRUE,
                           NULL);

  item = gtk_tool_button_new_from_stock (GTK_STOCK_HELP);
  gtk_tool_item_set_tooltip_text (item, "A regular item");
  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
}
