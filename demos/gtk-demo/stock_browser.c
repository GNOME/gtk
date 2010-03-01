/* Stock Item and Icon Browser
 *
 * This source code for this demo doesn't demonstrate anything
 * particularly useful in applications. The purpose of the "demo" is
 * just to provide a handy place to browse the available stock icons
 * and stock items.
 */

#include <string.h>

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

typedef struct _StockItemInfo StockItemInfo;
struct _StockItemInfo
{
  gchar *id;
  GtkStockItem item;
  GdkPixbuf *small_icon;
  gchar *macro;
  gchar *accel_str;
};

/* Make StockItemInfo a boxed type so we can automatically
 * manage memory
 */
#define STOCK_ITEM_INFO_TYPE stock_item_info_get_type ()

static void
stock_item_info_free (StockItemInfo *info)
{
  g_free (info->id);
  g_free (info->macro);
  g_free (info->accel_str);
  if (info->small_icon)
    g_object_unref (info->small_icon);
  
  g_free (info);
}

static StockItemInfo*
stock_item_info_copy (StockItemInfo *src)
{
  StockItemInfo *info;

  info = g_new (StockItemInfo, 1);
  info->id = g_strdup (src->id);
  info->macro = g_strdup (src->macro);
  info->accel_str = g_strdup (src->accel_str);
  
  info->item = src->item;

  info->small_icon = src->small_icon;
  if (info->small_icon)
    g_object_ref (info->small_icon);

  return info;
}

static GType
stock_item_info_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("StockItemInfo",
                                             (GBoxedCopyFunc) stock_item_info_copy,
                                             (GBoxedFreeFunc) stock_item_info_free);

  return our_type;
}

typedef struct _StockItemDisplay StockItemDisplay;
struct _StockItemDisplay
{
  GtkWidget *type_label;
  GtkWidget *macro_label;
  GtkWidget *id_label;
  GtkWidget *label_accel_label;
  GtkWidget *icon_image;
};

static gchar*
id_to_macro (const gchar *id)
{
  GString *macro = NULL;
  const gchar *cp;

  /* gtk-foo-bar -> GTK_STOCK_FOO_BAR */

  macro = g_string_new (NULL);
  
  cp = id;
  
  if (strncmp (cp, "gtk-", 4) == 0)
    {
      g_string_append (macro, "GTK_STOCK_");
      cp += 4;
    }

  while (*cp)
    {
      if (*cp == '-')
	g_string_append_c (macro, '_');
      else if (g_ascii_islower (*cp))
	g_string_append_c (macro, g_ascii_toupper (*cp));
      else
	g_string_append_c (macro, *cp);

      cp++;
    }

  return g_string_free (macro, FALSE);
}

static GtkTreeModel*
create_model (void)
{
  GtkListStore *store;
  GSList *ids;
  GSList *tmp_list;
  
  store = gtk_list_store_new (2, STOCK_ITEM_INFO_TYPE, G_TYPE_STRING);

  ids = gtk_stock_list_ids ();
  ids = g_slist_sort (ids, (GCompareFunc) strcmp);
  tmp_list = ids;
  while (tmp_list != NULL)
    {
      StockItemInfo info;
      GtkStockItem item;
      GtkTreeIter iter;
      GtkIconSet *icon_set;
      
      info.id = tmp_list->data;
      
      if (gtk_stock_lookup (info.id, &item))
        {
          info.item = item;
        }
      else
        {
          info.item.label = NULL;
          info.item.stock_id = NULL;
          info.item.modifier = 0;
          info.item.keyval = 0;
          info.item.translation_domain = NULL;
        }

      /* only show icons for stock IDs that have default icons */
      icon_set = gtk_icon_factory_lookup_default (info.id);
      if (icon_set)
        {
          GtkIconSize *sizes = NULL;
          gint n_sizes = 0;
          gint i;
          GtkIconSize size;

          /* See what sizes this stock icon really exists at */
          gtk_icon_set_get_sizes (icon_set, &sizes, &n_sizes);

          /* Use menu size if it exists, otherwise first size found */
          size = sizes[0];
          i = 0;
          while (i < n_sizes)
            {
              if (sizes[i] == GTK_ICON_SIZE_MENU)
                {
                  size = GTK_ICON_SIZE_MENU;
                  break;
                }
              ++i;
            }
          g_free (sizes);
          
          info.small_icon = gtk_widget_render_icon (window, info.id,
                                                    size,
                                                    NULL);
          
          if (size != GTK_ICON_SIZE_MENU)
            {
              /* Make the result the proper size for our thumbnail */
              gint w, h;
              GdkPixbuf *scaled;
              
              gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
              
              scaled = gdk_pixbuf_scale_simple (info.small_icon,
                                                w, h,
                                                GDK_INTERP_BILINEAR);

              g_object_unref (info.small_icon);
              info.small_icon = scaled;
            }
        }
      else
        info.small_icon = NULL;

      if (info.item.keyval != 0)
        {
          info.accel_str = gtk_accelerator_name (info.item.keyval,
                                                 info.item.modifier);
        }
      else
        {
          info.accel_str = g_strdup ("");
        }

      info.macro = id_to_macro (info.id);
      
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, &info, 1, info.id, -1);

      g_free (info.macro);
      g_free (info.accel_str);
      if (info.small_icon)
        g_object_unref (info.small_icon);
      
      tmp_list = g_slist_next (tmp_list);
    }
  
  g_slist_foreach (ids, (GFunc)g_free, NULL);
  g_slist_free (ids);

  return GTK_TREE_MODEL (store);
}

/* Finds the largest size at which the given image stock id is
 * available. This would not be useful for a normal application
 */
static GtkIconSize
get_largest_size (const char *id)
{
  GtkIconSet *set = gtk_icon_factory_lookup_default (id);
  GtkIconSize *sizes;
  gint n_sizes, i;
  GtkIconSize best_size = GTK_ICON_SIZE_INVALID;
  gint best_pixels = 0;

  gtk_icon_set_get_sizes (set, &sizes, &n_sizes);

  for (i = 0; i < n_sizes; i++)
    {
      gint width, height;
      
      gtk_icon_size_lookup (sizes[i], &width, &height);

      if (width * height > best_pixels)
	{
	  best_size = sizes[i];
	  best_pixels = width * height;
	}
    }
  
  g_free (sizes);

  return best_size;
}

static void
selection_changed (GtkTreeSelection *selection)
{
  GtkTreeView *treeview;
  StockItemDisplay *display;
  GtkTreeModel *model;
  GtkTreeIter iter;
  
  treeview = gtk_tree_selection_get_tree_view (selection);
  display = g_object_get_data (G_OBJECT (treeview), "stock-display");

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      StockItemInfo *info;
      gchar *str;
      
      gtk_tree_model_get (model, &iter,
                          0, &info,
                          -1);

      if (info->small_icon && info->item.label)
        gtk_label_set_text (GTK_LABEL (display->type_label), "Icon and Item");
      else if (info->small_icon)
        gtk_label_set_text (GTK_LABEL (display->type_label), "Icon Only");
      else if (info->item.label)
        gtk_label_set_text (GTK_LABEL (display->type_label), "Item Only");
      else
        gtk_label_set_text (GTK_LABEL (display->type_label), "???????");

      gtk_label_set_text (GTK_LABEL (display->macro_label), info->macro);
      gtk_label_set_text (GTK_LABEL (display->id_label), info->id);

      if (info->item.label)
        {
          str = g_strdup_printf ("%s %s", info->item.label, info->accel_str);
          gtk_label_set_text_with_mnemonic (GTK_LABEL (display->label_accel_label), str);
          g_free (str);
        }
      else
        {
          gtk_label_set_text (GTK_LABEL (display->label_accel_label), "");
        }

      if (info->small_icon)
        gtk_image_set_from_stock (GTK_IMAGE (display->icon_image), info->id,
                                  get_largest_size (info->id));
      else
        gtk_image_set_from_pixbuf (GTK_IMAGE (display->icon_image), NULL);

      stock_item_info_free (info);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (display->type_label), "No selected item");
      gtk_label_set_text (GTK_LABEL (display->macro_label), "");
      gtk_label_set_text (GTK_LABEL (display->id_label), "");
      gtk_label_set_text (GTK_LABEL (display->label_accel_label), "");
      gtk_image_set_from_pixbuf (GTK_IMAGE (display->icon_image), NULL);
    }
}

static void
macro_set_func_text (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  StockItemInfo *info;
  
  gtk_tree_model_get (model, iter,
                      0, &info,
                      -1);
  
  g_object_set (GTK_CELL_RENDERER (cell),
                "text", info->macro,
                NULL);
  
  stock_item_info_free (info);
}

static void
id_set_func (GtkTreeViewColumn *tree_column,
             GtkCellRenderer   *cell,
             GtkTreeModel      *model,
             GtkTreeIter       *iter,
             gpointer           data)
{
  StockItemInfo *info;
  
  gtk_tree_model_get (model, iter,
                      0, &info,
                      -1);
  
  g_object_set (GTK_CELL_RENDERER (cell),
                "text", info->id,
                NULL);
  
  stock_item_info_free (info);
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  StockItemInfo *info;
  
  gtk_tree_model_get (model, iter,
                      0, &info,
                      -1);
  
  g_object_set (GTK_CELL_RENDERER (cell),
                "text", info->accel_str,
                NULL);
  
  stock_item_info_free (info);
}

static void
label_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  StockItemInfo *info;
  
  gtk_tree_model_get (model, iter,
                      0, &info,
                      -1);
  
  g_object_set (GTK_CELL_RENDERER (cell),
                "text", info->item.label,
                NULL);
  
  stock_item_info_free (info);
}

GtkWidget *
do_stock_browser (GtkWidget *do_widget)
{  
  if (!window)
    {
      GtkWidget *frame;
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *sw;
      GtkWidget *treeview;
      GtkWidget *align;
      GtkTreeModel *model;
      GtkCellRenderer *cell_renderer;
      StockItemDisplay *display;
      GtkTreeSelection *selection;
      GtkTreeViewColumn *column;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Stock Icons and Items");
      gtk_window_set_default_size (GTK_WINDOW (window), -1, 500);

      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      hbox = gtk_hbox_new (FALSE, 8);
      gtk_container_add (GTK_CONTAINER (window), hbox);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);

      model = create_model ();
      
      treeview = gtk_tree_view_new_with_model (model);

      g_object_unref (model);

      gtk_container_add (GTK_CONTAINER (sw), treeview);
      
      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, "Macro");

      cell_renderer = gtk_cell_renderer_pixbuf_new ();
      gtk_tree_view_column_pack_start (column,
				       cell_renderer,
				       FALSE);
      gtk_tree_view_column_set_attributes (column, cell_renderer,
					   "stock_id", 1, NULL);
      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_tree_view_column_pack_start (column,
				       cell_renderer,
				       TRUE);
      gtk_tree_view_column_set_cell_data_func (column, cell_renderer,
					       macro_set_func_text, NULL, NULL);

      gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
				   column);

      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview),
                                                  -1,
                                                  "Label",
                                                  cell_renderer,
                                                  label_set_func,
                                                  NULL,
                                                  NULL);

      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview),
                                                  -1,
                                                  "Accel",
                                                  cell_renderer,
                                                  accel_set_func,
                                                  NULL,
                                                  NULL);

      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview),
                                                  -1,
                                                  "ID",
                                                  cell_renderer,
                                                  id_set_func,
                                                  NULL,
                                                  NULL);
      
      align = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
      gtk_box_pack_end (GTK_BOX (hbox), align, FALSE, FALSE, 0);
      
      frame = gtk_frame_new ("Selected Item");
      gtk_container_add (GTK_CONTAINER (align), frame);

      vbox = gtk_vbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
      gtk_container_add (GTK_CONTAINER (frame), vbox);

      display = g_new (StockItemDisplay, 1);
      g_object_set_data_full (G_OBJECT (treeview),
                              "stock-display",
                              display,
                              g_free); /* free display with treeview */
      
      display->type_label = gtk_label_new (NULL);
      display->macro_label = gtk_label_new (NULL);
      display->id_label = gtk_label_new (NULL);
      display->label_accel_label = gtk_label_new (NULL);
      display->icon_image = gtk_image_new_from_pixbuf (NULL); /* empty image */

      gtk_box_pack_start (GTK_BOX (vbox), display->type_label,
                          FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (vbox), display->icon_image,
                          FALSE, FALSE, 0);
      
      gtk_box_pack_start (GTK_BOX (vbox), display->label_accel_label,
                          FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), display->macro_label,
                          FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), display->id_label,
                          FALSE, FALSE, 0);

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
      
      g_signal_connect (selection,
			"changed",
			G_CALLBACK (selection_changed),
			NULL);
    }

  if (!gtk_widget_get_visible (window))
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
