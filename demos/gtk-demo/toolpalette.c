/* Tool Palette
 *
 * A tool palette widget shows groups of toolbar items as a grid of icons
 * or a list of names.
 */

#include <string.h>
#include <gtk/gtk.h>
#include "config.h"

static GtkWidget *window = NULL;

static void load_stock_items (GtkToolPalette *palette);
static void load_toggle_items (GtkToolPalette *palette);
static void load_special_items (GtkToolPalette *palette);

typedef struct _CanvasItem CanvasItem;

struct _CanvasItem
{
  GdkPixbuf *pixbuf;
  gdouble x, y;
};

static CanvasItem *drop_item = NULL;
static GList *canvas_items = NULL;

/********************************/
/* ====== Canvas drawing ====== */
/********************************/

static CanvasItem*
canvas_item_new (GtkWidget     *widget,
                 GtkToolButton *button,
                 gdouble        x,
                 gdouble        y)
{
  CanvasItem *item = NULL;
  const gchar *stock_id;
  GdkPixbuf *pixbuf;

  stock_id = gtk_tool_button_get_stock_id (button);
  pixbuf = gtk_widget_render_icon_pixbuf (widget, stock_id, GTK_ICON_SIZE_DIALOG);

  if (pixbuf)
    {
      item = g_slice_new0 (CanvasItem);
      item->pixbuf = pixbuf;
      item->x = x;
      item->y = y;
    }

  return item;
}

static void
canvas_item_free (CanvasItem *item)
{
  g_object_unref (item->pixbuf);
  g_slice_free (CanvasItem, item);
}

static void
canvas_item_draw (const CanvasItem *item,
                  cairo_t          *cr,
                  gboolean          preview)
{
  gdouble cx = gdk_pixbuf_get_width (item->pixbuf);
  gdouble cy = gdk_pixbuf_get_height (item->pixbuf);

  gdk_cairo_set_source_pixbuf (cr,
                               item->pixbuf,
                               item->x - cx * 0.5,
                               item->y - cy * 0.5);

  if (preview)
    cairo_paint_with_alpha (cr, 0.6);
  else
    cairo_paint (cr);
}

static gboolean
canvas_draw (GtkWidget *widget,
             cairo_t   *cr)
{
  GList *iter;

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  for (iter = canvas_items; iter; iter = iter->next)
    canvas_item_draw (iter->data, cr, FALSE);

  if (drop_item)
    canvas_item_draw (drop_item, cr, TRUE);

  return TRUE;
}

/*****************************/
/* ====== Palette DnD ====== */
/*****************************/

static void
palette_drop_item (GtkToolItem      *drag_item,
                   GtkToolItemGroup *drop_group,
                   gint              x,
                   gint              y)
{
  GtkWidget *drag_group = gtk_widget_get_parent (GTK_WIDGET (drag_item));
  GtkToolItem *drop_item = gtk_tool_item_group_get_drop_item (drop_group, x, y);
  gint drop_position = -1;

  if (drop_item)
    drop_position = gtk_tool_item_group_get_item_position (GTK_TOOL_ITEM_GROUP (drop_group), drop_item);

  if (GTK_TOOL_ITEM_GROUP (drag_group) != drop_group)
    {
      gboolean homogeneous, expand, fill, new_row;

      g_object_ref (drag_item);
      gtk_container_child_get (GTK_CONTAINER (drag_group), GTK_WIDGET (drag_item),
                               "homogeneous", &homogeneous,
                               "expand", &expand,
                               "fill", &fill,
                               "new-row", &new_row,
                               NULL);
      gtk_container_remove (GTK_CONTAINER (drag_group), GTK_WIDGET (drag_item));
      gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (drop_group),
                                  drag_item, drop_position);
      gtk_container_child_set (GTK_CONTAINER (drop_group), GTK_WIDGET (drag_item),
                               "homogeneous", homogeneous,
                               "expand", expand,
                               "fill", fill,
                               "new-row", new_row,
                               NULL);
      g_object_unref (drag_item);
    }
  else
    gtk_tool_item_group_set_item_position (GTK_TOOL_ITEM_GROUP (drop_group),
                                           drag_item, drop_position);
}

static void
palette_drop_group (GtkToolPalette   *palette,
                    GtkToolItemGroup *drag_group,
                    GtkToolItemGroup *drop_group)
{
  gint drop_position = -1;

  if (drop_group)
    drop_position = gtk_tool_palette_get_group_position (palette, drop_group);

  gtk_tool_palette_set_group_position (palette, drag_group, drop_position);
}

static void
palette_drag_data_received (GtkWidget        *widget,
                            GdkDragContext   *context,
                            gint              x,
                            gint              y,
                            GtkSelectionData *selection,
                            guint             info,
                            guint             time,
                            gpointer          data)
{
  GtkAllocation     allocation;
  GtkToolItemGroup *drop_group = NULL;
  GtkWidget        *drag_palette = gtk_drag_get_source_widget (context);
  GtkWidget        *drag_item = NULL;

  while (drag_palette && !GTK_IS_TOOL_PALETTE (drag_palette))
    drag_palette = gtk_widget_get_parent (drag_palette);

  if (drag_palette)
    {
      drag_item = gtk_tool_palette_get_drag_item (GTK_TOOL_PALETTE (drag_palette),
                                                  selection);
      drop_group = gtk_tool_palette_get_drop_group (GTK_TOOL_PALETTE (widget),
                                                    x, y);
    }

  if (GTK_IS_TOOL_ITEM_GROUP (drag_item))
    palette_drop_group (GTK_TOOL_PALETTE (drag_palette),
                        GTK_TOOL_ITEM_GROUP (drag_item),
                        drop_group);
  else if (GTK_IS_TOOL_ITEM (drag_item) && drop_group)
    {
      gtk_widget_get_allocation (GTK_WIDGET (drop_group), &allocation);
      palette_drop_item (GTK_TOOL_ITEM (drag_item),
                         drop_group,
                         x - allocation.x,
                         y - allocation.y);
    }
}

/********************************/
/* ====== Passive Canvas ====== */
/********************************/

static void
passive_canvas_drag_data_received (GtkWidget        *widget,
                                   GdkDragContext   *context,
                                   gint              x,
                                   gint              y,
                                   GtkSelectionData *selection,
                                   guint             info,
                                   guint             time,
                                   gpointer          data)
{
  /* find the tool button, which is the source of this DnD operation */

  GtkWidget *palette = gtk_drag_get_source_widget (context);
  CanvasItem *canvas_item = NULL;
  GtkWidget *tool_item = NULL;

  while (palette && !GTK_IS_TOOL_PALETTE (palette))
    palette = gtk_widget_get_parent (palette);

  if (palette)
    tool_item = gtk_tool_palette_get_drag_item (GTK_TOOL_PALETTE (palette),
                                                selection);

  g_assert (NULL == drop_item);

  /* append a new canvas item when a tool button was found */

  if (GTK_IS_TOOL_ITEM (tool_item))
    canvas_item = canvas_item_new (widget, GTK_TOOL_BUTTON (tool_item), x, y);

  if (canvas_item)
    {
      canvas_items = g_list_append (canvas_items, canvas_item);
      gtk_widget_queue_draw (widget);
    }
}

/************************************/
/* ====== Interactive Canvas ====== */
/************************************/

static gboolean
interactive_canvas_drag_motion (GtkWidget      *widget,
                                GdkDragContext *context,
                                gint            x,
                                gint            y,
                                guint           time,
                                gpointer        data)
{
  if (drop_item)
    {
      /* already have a drop indicator - just update position */

      drop_item->x = x;
      drop_item->y = y;

      gtk_widget_queue_draw (widget);
      gdk_drag_status (context, GDK_ACTION_COPY, time);
    }
  else
    {
      /* request DnD data for creating a drop indicator */

      GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

      if (!target)
        return FALSE;

      gtk_drag_get_data (widget, context, target, time);
    }

  return TRUE;
}

static void
interactive_canvas_drag_data_received (GtkWidget        *widget,
                                       GdkDragContext   *context,
                                       gint              x,
                                       gint              y,
                                       GtkSelectionData *selection,
                                       guint             info,
                                       guint             time,
                                       gpointer          data)

{
  /* find the tool button which is the source of this DnD operation */

  GtkWidget *palette = gtk_drag_get_source_widget (context);
  GtkWidget *tool_item = NULL;

  while (palette && !GTK_IS_TOOL_PALETTE (palette))
    palette = gtk_widget_get_parent (palette);

  if (palette)
    tool_item = gtk_tool_palette_get_drag_item (GTK_TOOL_PALETTE (palette),
                                                selection);

  /* create a drop indicator when a tool button was found */

  g_assert (NULL == drop_item);

  if (GTK_IS_TOOL_ITEM (tool_item))
    {
      drop_item = canvas_item_new (widget, GTK_TOOL_BUTTON (tool_item), x, y);
      gdk_drag_status (context, GDK_ACTION_COPY, time);
      gtk_widget_queue_draw (widget);
    }
}

static gboolean
interactive_canvas_drag_drop (GtkWidget      *widget,
                              GdkDragContext *context,
                              gint            x,
                              gint            y,
                              guint           time,
                              gpointer        data)
{
  if (drop_item)
    {
      /* turn the drop indicator into a real canvas item */

      drop_item->x = x;
      drop_item->y = y;

      canvas_items = g_list_append (canvas_items, drop_item);
      drop_item = NULL;

      /* signal the item was accepted and redraw */

      gtk_drag_finish (context, TRUE, FALSE, time);
      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  return FALSE;
}

static gboolean
interactive_canvas_real_drag_leave (gpointer data)
{
  if (drop_item)
    {
      GtkWidget *widget = GTK_WIDGET (data);

      canvas_item_free (drop_item);
      drop_item = NULL;

      gtk_widget_queue_draw (widget);
    }

  return G_SOURCE_REMOVE;
}

static void
interactive_canvas_drag_leave (GtkWidget      *widget,
                               GdkDragContext *context,
                               guint           time,
                               gpointer        data)
{
  /* defer cleanup until a potential "drag-drop" signal was received */
  g_idle_add (interactive_canvas_real_drag_leave, widget);
}

static void
on_combo_orientation_changed (GtkComboBox *combo_box,
                              gpointer     user_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (user_data);
  GtkScrolledWindow *sw;
  GtkTreeModel *model = gtk_combo_box_get_model (combo_box);
  GtkTreeIter iter;
  gint val = 0;

  sw = GTK_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (palette)));

  if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    return;

  gtk_tree_model_get (model, &iter, 1, &val, -1);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (palette), val);

  if (val == GTK_ORIENTATION_HORIZONTAL)
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  else
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
}

static void
on_combo_style_changed (GtkComboBox *combo_box,
                        gpointer     user_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (user_data);
  GtkTreeModel *model = gtk_combo_box_get_model (combo_box);
  GtkTreeIter iter;
  gint val = 0;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    return;

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
  GtkWidget *hbox = NULL;
  GtkWidget *combo_orientation = NULL;
  GtkListStore *orientation_model = NULL;
  GtkWidget *combo_style = NULL;
  GtkListStore *style_model = NULL;
  GtkCellRenderer *cell_renderer = NULL;
  GtkTreeIter iter;
  GtkWidget *palette = NULL;
  GtkWidget *palette_scroller = NULL;
  GtkWidget *notebook = NULL;
  GtkWidget *contents = NULL;
  GtkWidget *contents_scroller = NULL;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Tool Palette");
      gtk_window_set_default_size (GTK_WINDOW (window), 200, 600);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      /* Add widgets to control the ToolPalette appearance: */
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_container_add (GTK_CONTAINER (window), box);

      /* Orientation combo box: */
      orientation_model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
      gtk_list_store_append (orientation_model, &iter);
      gtk_list_store_set (orientation_model, &iter,
                          0, "Horizontal",
                          1, GTK_ORIENTATION_HORIZONTAL,
                          -1);
      gtk_list_store_append (orientation_model, &iter);
      gtk_list_store_set (orientation_model, &iter,
                          0, "Vertical",
                          1, GTK_ORIENTATION_VERTICAL,
                          -1);

      combo_orientation =
        gtk_combo_box_new_with_model (GTK_TREE_MODEL (orientation_model));
      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_orientation),
                                  cell_renderer,
                                  TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_orientation),
                                      cell_renderer,
                                      "text", 0,
                                      NULL);
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_orientation), &iter);
      gtk_box_pack_start (GTK_BOX (box), combo_orientation, FALSE, FALSE, 0);

      /* Style combo box: */
      style_model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
      gtk_list_store_append (style_model, &iter);
      gtk_list_store_set (style_model, &iter,
                          0, "Text",
                          1, GTK_TOOLBAR_TEXT,
                          -1);
      gtk_list_store_append (style_model, &iter);
      gtk_list_store_set (style_model, &iter,
                          0, "Both",
                          1, GTK_TOOLBAR_BOTH,
                          -1);
      gtk_list_store_append (style_model, &iter);
      gtk_list_store_set (style_model, &iter,
                          0, "Both: Horizontal",
                          1, GTK_TOOLBAR_BOTH_HORIZ,
                          -1);
      gtk_list_store_append (style_model, &iter);
      gtk_list_store_set (style_model, &iter,
                          0, "Icons",
                          1, GTK_TOOLBAR_ICONS,
                          -1);
      gtk_list_store_append (style_model, &iter);
      gtk_list_store_set (style_model, &iter,
                          0, "Default",
                          1, -1,  /* A custom meaning for this demo. */
                          -1);
      combo_style = gtk_combo_box_new_with_model (GTK_TREE_MODEL (style_model));
      cell_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_style),
                                  cell_renderer,
                                  TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_style),
                                      cell_renderer,
                                      "text", 0,
                                      NULL);
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_style), &iter);
      gtk_box_pack_start (GTK_BOX (box), combo_style, FALSE, FALSE, 0);

      /* Add hbox */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 0);

      /* Add and fill the ToolPalette: */
      palette = gtk_tool_palette_new ();

      load_stock_items (GTK_TOOL_PALETTE (palette));
      load_toggle_items (GTK_TOOL_PALETTE (palette));
      load_special_items (GTK_TOOL_PALETTE (palette));

      palette_scroller = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (palette_scroller),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_set_border_width (GTK_CONTAINER (palette_scroller), 6);
      gtk_widget_set_hexpand (palette_scroller, TRUE);

      gtk_container_add (GTK_CONTAINER (palette_scroller), palette);
      gtk_container_add (GTK_CONTAINER (hbox), palette_scroller);

      gtk_widget_show_all (box);

      /* Connect signals: */
      g_signal_connect (combo_orientation, "changed",
                        G_CALLBACK (on_combo_orientation_changed), palette);
      g_signal_connect (combo_style, "changed",
                        G_CALLBACK (on_combo_style_changed), palette);

      /* Keep the widgets in sync: */
      on_combo_orientation_changed (GTK_COMBO_BOX (combo_orientation), palette);

      /* ===== notebook ===== */

      notebook = gtk_notebook_new ();
      gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
      gtk_box_pack_end (GTK_BOX(hbox), notebook, FALSE, FALSE, 0);

      /* ===== DnD for tool items ===== */

      g_signal_connect (palette, "drag-data-received",
                        G_CALLBACK (palette_drag_data_received), NULL);

      gtk_tool_palette_add_drag_dest (GTK_TOOL_PALETTE (palette),
                                      palette,
                                      GTK_DEST_DEFAULT_ALL,
                                      GTK_TOOL_PALETTE_DRAG_ITEMS |
                                      GTK_TOOL_PALETTE_DRAG_GROUPS,
                                      GDK_ACTION_MOVE);

      /* ===== passive DnD dest ===== */

      contents = gtk_drawing_area_new ();
      gtk_widget_set_app_paintable (contents, TRUE);

      g_object_connect (contents,
                        "signal::draw", canvas_draw, NULL,
                        "signal::drag-data-received", passive_canvas_drag_data_received, NULL,
                        NULL);

      gtk_tool_palette_add_drag_dest (GTK_TOOL_PALETTE (palette),
                                      contents,
                                      GTK_DEST_DEFAULT_ALL,
                                      GTK_TOOL_PALETTE_DRAG_ITEMS,
                                      GDK_ACTION_COPY);

      contents_scroller = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (contents_scroller),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_ALWAYS);
      gtk_container_add (GTK_CONTAINER (contents_scroller), contents);
      gtk_container_set_border_width (GTK_CONTAINER (contents_scroller), 6);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                contents_scroller,
                                gtk_label_new ("Passive DnD Mode"));

      /* ===== interactive DnD dest ===== */

      contents = gtk_drawing_area_new ();
      gtk_widget_set_app_paintable (contents, TRUE);

      g_object_connect (contents,
                        "signal::draw", canvas_draw, NULL,
                        "signal::drag-motion", interactive_canvas_drag_motion, NULL,
                        "signal::drag-data-received", interactive_canvas_drag_data_received, NULL,
                        "signal::drag-leave", interactive_canvas_drag_leave, NULL,
                        "signal::drag-drop", interactive_canvas_drag_drop, NULL,
                        NULL);

      gtk_tool_palette_add_drag_dest (GTK_TOOL_PALETTE (palette),
                                      contents,
                                      GTK_DEST_DEFAULT_HIGHLIGHT,
                                      GTK_TOOL_PALETTE_DRAG_ITEMS,
                                      GDK_ACTION_COPY);

      contents_scroller = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (contents_scroller),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_ALWAYS);
      gtk_container_add (GTK_CONTAINER (contents_scroller), contents);
      gtk_container_set_border_width (GTK_CONTAINER (contents_scroller), 6);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), contents_scroller,
                                gtk_label_new ("Interactive DnD Mode"));
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


static void
load_stock_items (GtkToolPalette *palette)
{
  GtkWidget *group_af = gtk_tool_item_group_new ("Stock Icons (A-F)");
  GtkWidget *group_gn = gtk_tool_item_group_new ("Stock Icons (G-N)");
  GtkWidget *group_or = gtk_tool_item_group_new ("Stock Icons (O-R)");
  GtkWidget *group_sz = gtk_tool_item_group_new ("Stock Icons (S-Z)");
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

  group = gtk_tool_item_group_new ("Radio Item");
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
  label_button = gtk_button_new_with_label ("Advanced Features");
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
