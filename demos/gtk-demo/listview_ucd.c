/* Lists/Characters (scroll_to)
 *
 * This demo shows a multi-column representation of some parts
 * of the Unicode Character Database, or UCD.
 *
 * The dataset used here has 33 796 items.
 *
 * It includes some widgets to demo the scroll_to() API functionality.
 */

#include <gtk/gtk.h>
#include "script-names.h"
#include "unicode-names.h"

#define UCD_TYPE_ITEM (ucd_item_get_type ())
G_DECLARE_FINAL_TYPE (UcdItem, ucd_item, UCD, ITEM, GObject)

struct _UcdItem
{
  GObject parent_instance;
  gunichar codepoint;
  const char *name;
  guint colnumber;
};

struct _UcdItemClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (UcdItem, ucd_item, G_TYPE_OBJECT)

static void
ucd_item_init (UcdItem *item)
{
}

static void
ucd_item_class_init (UcdItemClass *class)
{
}

static UcdItem *
ucd_item_new (gunichar    codepoint,
              const char *name,
              guint       col)
{
  UcdItem *item;

  item = g_object_new (UCD_TYPE_ITEM, NULL);

  item->codepoint = codepoint;
  item->name = name;
  item->colnumber = col;

  return item;
}

static gunichar
ucd_item_get_codepoint (UcdItem *item)
{
  return item->codepoint;
}

static const char *
ucd_item_get_name (UcdItem *item)
{
  return item->name;
}

static guint
ucd_item_get_colnumber (UcdItem *item)
{
  return item->colnumber;
}

static GListModel *
ucd_model_new (void)
{
  GBytes *bytes;
  GVariant *v;
  GVariantIter *iter;
  GListStore *store;
  guint u;
  guint colnumber;
  char *name;

  bytes = g_resources_lookup_data ("/listview_ucd_data/ucdnames.data", 0, NULL);
  v = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(us)"), bytes, TRUE));

  iter = g_variant_iter_new (v);

  store = g_list_store_new (G_TYPE_OBJECT);
  colnumber = 1;
  while (g_variant_iter_next (iter, "(u&s)", &u, &name))
    {
      if (u == 0)
        continue;

      UcdItem *item = ucd_item_new (u, name, colnumber);
      g_list_store_append (store, item);
      colnumber++;
      g_object_unref (item);
    }

  g_variant_iter_free (iter);
  g_variant_unref (v);
  g_bytes_unref (bytes);

  return G_LIST_MODEL (store);
}

static void
setup_centered_label (GtkSignalListItemFactory *factory,
                      GObject                  *listitem)
{
  GtkWidget *label;
  label = gtk_label_new ("");
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), label);
}

static void
setup_label (GtkSignalListItemFactory *factory,
             GObject                  *listitem)
{
  GtkWidget *label;
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), label);
}

static void
setup_ellipsizing_label (GtkSignalListItemFactory *factory,
                         GObject                  *listitem)
{
  GtkWidget *label;
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 20);
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), label);
}

static void
bind_colnumber (GtkSignalListItemFactory *factory,
                GObject                  *listitem,
                GListModel               *ucd_model)
{
  GtkWidget *label;
  GObject *item;
  uint colnumber;
  char buffer[16] = { 0, };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  colnumber = ucd_item_get_colnumber (UCD_ITEM (item));

  g_snprintf (buffer, 10, "%u", colnumber);
  gtk_label_set_label (GTK_LABEL (label), buffer);
}

static void
bind_codepoint (GtkSignalListItemFactory *factory,
                GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  char buffer[16] = { 0, };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  g_snprintf (buffer, 10, "%#06x", codepoint);
  gtk_label_set_label (GTK_LABEL (label), buffer);
}

static void
bind_char (GtkSignalListItemFactory *factory,
           GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  char buffer[16] = { 0, };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  if (g_unichar_isprint (codepoint))
    g_unichar_to_utf8 (codepoint, buffer);

  gtk_label_set_label (GTK_LABEL (label), buffer);
}

static void
bind_name (GtkSignalListItemFactory *factory,
           GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  const char *name;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  name = ucd_item_get_name (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), name);
}

static void
bind_type (GtkSignalListItemFactory *factory,
           GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), get_unicode_type_name (g_unichar_type (codepoint)));
}

static void
bind_break_type (GtkSignalListItemFactory *factory,
                 GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), get_break_type_name (g_unichar_break_type (codepoint)));
}

static void
bind_combining_class (GtkSignalListItemFactory *factory,
                      GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), get_combining_class_name (g_unichar_combining_class (codepoint)));
}

static void
bind_script (GtkSignalListItemFactory *factory,
             GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  GUnicodeScript script;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));
  script = g_unichar_get_script (codepoint);

  gtk_label_set_label (GTK_LABEL (label), get_script_name (script));
}

static void
selection_changed (GObject    *object,
                   GParamSpec *pspec,
                   GtkWidget  *label)
{
  UcdItem *item;
  guint codepoint;
  char buffer[16] = { 0, };

  item = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (object));
  codepoint = ucd_item_get_codepoint (item);

  if (g_unichar_isprint (codepoint))
    g_unichar_to_utf8 (codepoint, buffer);

  gtk_label_set_label (GTK_LABEL (label), buffer);
}

GtkWidget *
create_ucd_view (GtkWidget *label)
{
  GtkWidget *cv;
  GListModel *ucd_model;
  GtkSingleSelection *selection;
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;

  ucd_model = ucd_model_new ();

  selection = gtk_single_selection_new (ucd_model);
  gtk_single_selection_set_autoselect (selection, TRUE);
  gtk_single_selection_set_can_unselect (selection, FALSE);
  if (label)
    g_signal_connect (selection, "notify::selected", G_CALLBACK (selection_changed), label);

  cv = gtk_column_view_new (GTK_SELECTION_MODEL (selection));
  gtk_column_view_set_show_column_separators (GTK_COLUMN_VIEW (cv), TRUE);


  // HACK: This is a non-visible empty column we add here because later we use this model
  // for the GtkDropDown of columns, and we need an empty item to mean "nothing is selected".
  column = gtk_column_view_column_new (NULL, NULL);
  gtk_column_view_column_set_visible (column, FALSE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_centered_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_colnumber), NULL);
  column = gtk_column_view_column_new ("Row nº", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_centered_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_codepoint), NULL);
  column = gtk_column_view_column_new ("Codepoint", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_centered_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_char), NULL);
  column = gtk_column_view_column_new ("Char", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_ellipsizing_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_name), NULL);
  column = gtk_column_view_column_new ("Name", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_ellipsizing_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_type), NULL);
  column = gtk_column_view_column_new ("Type", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_ellipsizing_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_break_type), NULL);
  column = gtk_column_view_column_new ("Break Type", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_combining_class), NULL);
  column = gtk_column_view_column_new ("Combining Class", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_script), NULL);
  column = gtk_column_view_column_new ("Script", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
  g_object_unref (column);

  return cv;
}

static GtkWidget *window;
static GtkWidget *spin, *check_h, *check_v, *check_ha, *check_va, *list_view, *dropdown_cols;

static void
remove_provider (gpointer data)
{
  GtkStyleProvider *provider = GTK_STYLE_PROVIDER (data);

  gtk_style_context_remove_provider_for_display (gdk_display_get_default (), provider);
  g_object_unref (provider);
}

static void
dropdown_cols_changed (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    data)
{
  guint selected_pos;
  gboolean any_col_selected;

  selected_pos = gtk_drop_down_get_selected (GTK_DROP_DOWN (object));
  any_col_selected = (selected_pos && selected_pos != GTK_INVALID_LIST_POSITION);

  if (any_col_selected)
    {
      gtk_widget_set_sensitive (check_h, TRUE);
      gtk_widget_set_sensitive (check_ha, TRUE);
    }
  else
    {
      gtk_check_button_set_active (GTK_CHECK_BUTTON (check_h), FALSE);
      gtk_check_button_set_active (GTK_CHECK_BUTTON (check_ha), FALSE);
      gtk_widget_set_sensitive (check_h, FALSE);
      gtk_widget_set_sensitive (check_ha, FALSE);
    }
}

static void
scroll_to_cb (GtkWidget *button, gpointer data)
{
  GtkColumnViewColumn *center_column;
  GtkScrollInfo *scroll_info;
  gboolean col, col_always, row, row_always;
  guint pos;
  GtkListScrollFlags flags = GTK_LIST_SCROLL_SELECT;

  row = gtk_check_button_get_active (GTK_CHECK_BUTTON (check_v));
  row_always = gtk_check_button_get_active (GTK_CHECK_BUTTON (check_va));
  col = gtk_check_button_get_active (GTK_CHECK_BUTTON (check_h));
  col_always = gtk_check_button_get_active (GTK_CHECK_BUTTON (check_ha));

  scroll_info = NULL;
  if (row || row_always || col || col_always)
    {
      GtkScrollInfoCenter center_flags = GTK_SCROLL_INFO_CENTER_NONE;

      if (row)
        center_flags |= GTK_SCROLL_INFO_CENTER_ROW;
      if (row_always)
        center_flags |= GTK_SCROLL_INFO_CENTER_ROW_ALWAYS;
      if (col)
        center_flags |= GTK_SCROLL_INFO_CENTER_COL;
      if (col_always)
        center_flags |= GTK_SCROLL_INFO_CENTER_COL_ALWAYS;

      scroll_info = gtk_scroll_info_new ();
      gtk_scroll_info_set_center_flags (scroll_info, center_flags);
    }

  center_column = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (dropdown_cols));
  if (!gtk_column_view_column_get_visible (center_column))
    center_column = NULL;

  pos = (guint) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
  gtk_column_view_scroll_to (GTK_COLUMN_VIEW (list_view), pos - 1, center_column, flags, scroll_info);
}

static GtkWidget *
pack_with_label (const char *str, GtkWidget *widget1, GtkWidget *widget2)
{
  GtkWidget *box, *label;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  if (str)
    {
      label = gtk_label_new (str);
      gtk_box_append (GTK_BOX (box), label);
    }

  gtk_box_append (GTK_BOX (box), widget1);
  if (widget2)
    gtk_box_append (GTK_BOX (box), widget2);

  return box;
}

GtkWidget *
do_listview_ucd (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *listview, *sw;
      GtkWidget *box, *label, *box2, *button;
      GtkCssProvider *provider;

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 800, 400);
      gtk_window_set_title (GTK_WINDOW (window), "Characters");
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_set_margin_start (GTK_WIDGET (box2), 20);
      gtk_widget_set_margin_top (GTK_WIDGET (box2), 20);
      gtk_widget_set_margin_bottom (GTK_WIDGET (box2), 15);

      spin = gtk_spin_button_new_with_range (1.0, 33796.0, 1.0);
      gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spin), TRUE);
      gtk_widget_set_valign (spin, GTK_ALIGN_CENTER);

      check_v = gtk_check_button_new_with_label ("center row");
      check_va = gtk_check_button_new_with_label ("center row always");
      check_h = gtk_check_button_new_with_label ("center col");
      check_ha = gtk_check_button_new_with_label ("center col always");
      gtk_widget_set_sensitive (check_h, FALSE);
      gtk_widget_set_sensitive (check_ha, FALSE);

      button = gtk_button_new_with_label ("Scroll to row / col");
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      g_signal_connect (button, "clicked", G_CALLBACK (scroll_to_cb), NULL);

      dropdown_cols = gtk_drop_down_new (NULL, NULL);
      gtk_drop_down_set_show_arrow (GTK_DROP_DOWN (dropdown_cols), FALSE);

      gtk_box_append (GTK_BOX (box2), pack_with_label ("Row nº to scroll to:", spin, NULL));
      gtk_box_append (GTK_BOX (box2), pack_with_label ("Col to scroll (optional):", dropdown_cols, NULL));
      gtk_box_append (GTK_BOX (box2), pack_with_label (NULL, pack_with_label (NULL, check_v, check_va),
                                                             pack_with_label (NULL, check_h, check_ha)));
      gtk_box_append (GTK_BOX (box2), button);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      label = gtk_label_new ("");
      gtk_label_set_width_chars (GTK_LABEL (label), 2);
      gtk_widget_add_css_class (label, "enormous");
      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_string (provider, "label.enormous { font-size: 80px; }");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider), 800);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), label);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
      gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);
      listview = create_ucd_view (label);
      gtk_drop_down_set_model (GTK_DROP_DOWN (dropdown_cols),
                               gtk_column_view_get_columns (GTK_COLUMN_VIEW (listview)));
      gtk_drop_down_set_expression (GTK_DROP_DOWN (dropdown_cols),
                                    gtk_property_expression_new (GTK_TYPE_COLUMN_VIEW_COLUMN,
                                                                 NULL, "title"));
      g_signal_connect (dropdown_cols, "notify::selected", G_CALLBACK (dropdown_cols_changed), NULL);
      list_view = listview;
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);
      gtk_box_prepend (GTK_BOX (box), sw);
      gtk_window_set_child (GTK_WINDOW (window), pack_with_label (NULL, box2, box));

      g_object_set_data_full (G_OBJECT (window), "provider", provider, remove_provider);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}

