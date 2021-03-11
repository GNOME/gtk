#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS
#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtkunixprint.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xatom.h>
#include <gdkx.h>
#include "widgets.h"
#include "gtkgears.h"

#define SMALL_WIDTH  240
#define SMALL_HEIGHT 75
#define MEDIUM_WIDTH 240
#define MEDIUM_HEIGHT 165
#define LARGE_WIDTH 240
#define LARGE_HEIGHT 240

static WidgetInfo *
new_widget_info (const char *name,
		 GtkWidget  *widget,
		 WidgetSize  size)
{
  WidgetInfo *info;

  info = g_new0 (WidgetInfo, 1);
  info->name = g_strdup (name);
  info->size = size;
  if (GTK_IS_WINDOW (widget))
    {
      info->window = widget;
      gtk_window_set_resizable (GTK_WINDOW (info->window), FALSE);
      info->include_decorations = TRUE;
    }
  else if (GTK_IS_POPOVER (widget))
    {
      GtkWidget *button;

      info->snapshot_popover = TRUE;
      info->window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (info->window), 200, 200);
      gtk_window_set_decorated (GTK_WINDOW (info->window), FALSE);
      info->include_decorations = TRUE;
      button = gtk_menu_button_new ();
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), widget);
      gtk_window_set_child (GTK_WINDOW (info->window), button);
    }
  else
    {
      info->window = gtk_window_new ();
      gtk_window_set_decorated (GTK_WINDOW (info->window), FALSE);
      info->include_decorations = FALSE;
      gtk_window_set_child (GTK_WINDOW (info->window), widget);
    }
  info->no_focus = TRUE;

  switch (size)
    {
    case SMALL:
      gtk_widget_set_size_request (info->window, 240, 75);
      break;
    case MEDIUM:
      gtk_widget_set_size_request (info->window, 240, 165);
      break;
    case LARGE:
      gtk_widget_set_size_request (info->window, 240, 240);
      break;
    default:
      break;
    }

  return info;
}

static void
add_margin (GtkWidget *widget)
{
  gtk_widget_set_margin_start (widget, 10);
  gtk_widget_set_margin_end (widget, 10);
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
}

static WidgetInfo *
create_button (void)
{
  GtkWidget *widget;

  widget = gtk_button_new_with_mnemonic ("_Button");
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  add_margin (widget);

  return new_widget_info ("button", widget, SMALL);
}

static WidgetInfo *
create_switch (void)
{
  GtkWidget *widget;
  GtkWidget *sw;

  widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  sw = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (sw), TRUE);
  gtk_box_append (GTK_BOX (widget), sw);
  sw = gtk_switch_new ();
  gtk_box_append (GTK_BOX (widget), sw);

  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("switch", widget, SMALL);
}

static WidgetInfo *
create_toggle_button (void)
{
  GtkWidget *widget;
  GtkWidget *button;

  widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (widget), TRUE);
  gtk_widget_add_css_class (widget, "linked");
  button = gtk_toggle_button_new_with_label ("Toggle");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_box_append (GTK_BOX (widget), button);
  button = gtk_toggle_button_new_with_label ("Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_box_append (GTK_BOX (widget), button);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("toggle-button", widget, SMALL);
}

static WidgetInfo *
create_check_button (void)
{
  GtkWidget *widget;
  GtkWidget *button;

  widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  button = gtk_check_button_new_with_mnemonic ("_Check Button");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
  gtk_box_append (GTK_BOX (widget), button);

  button = gtk_check_button_new_with_mnemonic ("_Check Button");
  gtk_box_append (GTK_BOX (widget), button);

  return new_widget_info ("check-button", widget, SMALL);
}

static WidgetInfo *
create_radio_button (void)
{
  GtkWidget *widget;
  GtkWidget *button;
  GtkWidget *group;

  widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  button = gtk_check_button_new_with_mnemonic ("Radio Button");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
  gtk_box_append (GTK_BOX (widget), button);
  group = button;

  button = gtk_check_button_new_with_mnemonic ("Radio Button");
  gtk_box_append (GTK_BOX (widget), button);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));

  return new_widget_info ("radio-button", widget, SMALL);
}

static WidgetInfo *
create_link_button (void)
{
  GtkWidget *widget;

  widget = gtk_link_button_new_with_label ("http://www.gtk.org", "Link Button");
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("link-button", widget, MEDIUM);
}

static WidgetInfo *
create_menu_button (void)
{
  GtkWidget *widget;
  GtkWidget *menu;
  GtkWidget *vbox;

  widget = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (widget), "emblem-system-symbolic");
  menu = gtk_popover_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (widget), menu);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Menu Button"));

  add_margin (vbox);

  return new_widget_info ("menu-button", vbox, SMALL);
}

#define G_TYPE_TEST_PERMISSION      (g_test_permission_get_type ())
#define G_TEST_PERMISSION(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                     G_TYPE_TEST_PERMISSION,             \
                                     GTestPermission))
#define G_IS_TEST_PERMISSION(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                                     G_TYPE_TEST_PERMISSION))

typedef struct _GTestPermission GTestPermission;
typedef struct _GTestPermissionClass GTestPermissionClass;

struct _GTestPermission
{
  GPermission parent;

  gboolean success;
};

struct _GTestPermissionClass
{
  GPermissionClass parent_class;
};

G_DEFINE_TYPE (GTestPermission, g_test_permission, G_TYPE_PERMISSION)

static void
g_test_permission_init (GTestPermission *test)
{
  g_permission_impl_update (G_PERMISSION (test), FALSE, TRUE, TRUE);
}

static void
g_test_permission_class_init (GTestPermissionClass *class)
{
}

static WidgetInfo *
create_lockbutton (void)
{
  GtkWidget *vbox;
  GtkWidget *widget;

  widget = gtk_lock_button_new (g_object_new (G_TYPE_TEST_PERMISSION, NULL));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Lock Button"));
  gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

  return new_widget_info ("lock-button", vbox, SMALL);
}

static WidgetInfo *
create_entry (void)
{
  GtkWidget *widget;

  widget = gtk_entry_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_editable_set_text (GTK_EDITABLE (widget), "Entry");
  gtk_editable_set_position (GTK_EDITABLE (widget), -1);

  add_margin (widget);

  return  new_widget_info ("entry", widget, SMALL);
}

static WidgetInfo *
create_password_entry (void)
{
  GtkWidget *widget;

  widget = gtk_password_entry_new ();
  gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (widget), TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_editable_set_text (GTK_EDITABLE (widget), "Entry");
  gtk_editable_set_position (GTK_EDITABLE (widget), -1);

  add_margin (widget);

  return  new_widget_info ("password-entry", widget, SMALL);
}

static WidgetInfo *
create_search_entry (void)
{
  GtkWidget *widget;

  widget = gtk_search_entry_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  g_object_set (widget, "placeholder-text", "Search…", NULL);

  add_margin (widget);

  return  new_widget_info ("search-entry", widget, SMALL);
}

static WidgetInfo *
create_label (void)
{
  GtkWidget *widget;

  widget = gtk_label_new ("Label");
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("label", widget, SMALL);
}

static WidgetInfo *
create_combo_box_entry (void)
{
  GtkWidget *widget;
  GtkWidget *child;
  GtkTreeModel *model;

  model = (GtkTreeModel *)gtk_list_store_new (1, G_TYPE_STRING);
  widget = g_object_new (GTK_TYPE_COMBO_BOX,
                         "has-entry", TRUE,
                         "model", model,
                         "entry-text-column", 0,
                         NULL);
  g_object_unref (model);

  child = gtk_combo_box_get_child (GTK_COMBO_BOX (widget));
  gtk_editable_set_text (GTK_EDITABLE (child), "Combo Box Entry");
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("combo-box-entry", widget, SMALL);
}

static WidgetInfo *
create_combo_box (void)
{
  GtkWidget *widget;
  GtkCellRenderer *cell;
  GtkListStore *store;

  widget = gtk_combo_box_new ();
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (widget));
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), cell, "text", 0, NULL);

  store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_list_store_insert_with_values (store, NULL, -1, 0, "Combo Box", -1);
  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));

  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("combo-box", widget, SMALL);
}

static WidgetInfo *
create_combo_box_text (void)
{
  GtkWidget *widget;

  widget = gtk_combo_box_text_new ();

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Combo Box Text");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("combo-box-text", widget, SMALL);
}

static WidgetInfo *
create_drop_down (void)
{
  GtkWidget *widget;

  widget = gtk_drop_down_new_from_strings ((const char * const []){"Drop Down", "Almost a combo", NULL});

  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("drop-down", widget, SMALL);
}

static WidgetInfo *
create_info_bar (void)
{
  GtkWidget *widget;
  WidgetInfo *info;

  widget = gtk_info_bar_new ();
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (widget), TRUE);
  gtk_info_bar_set_message_type (GTK_INFO_BAR (widget), GTK_MESSAGE_INFO);
  gtk_info_bar_add_child (GTK_INFO_BAR (widget), gtk_label_new ("Info Bar"));

  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  add_margin (widget);

  info = new_widget_info ("info-bar", widget, SMALL);

  return info;
}

static WidgetInfo *
create_search_bar (void)
{
  GtkWidget *widget;
  GtkWidget *entry;
  WidgetInfo *info;
  GtkWidget *view;
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  widget = gtk_search_bar_new ();

  entry = gtk_search_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "Search Bar");
  gtk_search_bar_set_child (GTK_SEARCH_BAR (widget), entry);

  gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (widget), TRUE);
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (widget), TRUE);

  gtk_box_append (GTK_BOX (box), widget);

  view = gtk_text_view_new ();
  gtk_box_append (GTK_BOX (box), view);

  info = new_widget_info ("search-bar", box, SMALL);

  return info;
}

static WidgetInfo *
create_action_bar (void)
{
  GtkWidget *widget;
  GtkWidget *button;
  WidgetInfo *info;
  GtkWidget *view;
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  view = gtk_text_view_new ();
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_box_append (GTK_BOX (box), view);

  widget = gtk_action_bar_new ();

  button = gtk_button_new_from_icon_name ("object-select-symbolic");
  gtk_action_bar_pack_start (GTK_ACTION_BAR (widget), button);
  button = gtk_button_new_from_icon_name ("call-start-symbolic");
  gtk_action_bar_pack_start (GTK_ACTION_BAR (widget), button);
  g_object_set (gtk_widget_get_parent (button),
                "margin-start", 6,
                "margin-end", 6,
                "margin-top", 6,
                "margin-bottom", 6,
                "spacing", 6,
                NULL);

  gtk_box_append (GTK_BOX (box), widget);

  info = new_widget_info ("action-bar", box, SMALL);

  return info;
}

static WidgetInfo *
create_text_view (void)
{
  GtkWidget *widget;
  GtkWidget *text_view;

  widget = gtk_frame_new (NULL);
  text_view = gtk_text_view_new ();
  gtk_frame_set_child (GTK_FRAME (widget), text_view);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view)),
                            "Multiline\nText\n\n", -1);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_widget_set_size_request (text_view, 100, -1);

  add_margin (widget);

  return new_widget_info ("multiline-text", widget, MEDIUM);
}

static WidgetInfo *
create_tree_view (void)
{
  GtkWidget *widget;
  GtkWidget *tree_view;
  GtkTreeStore *store;
  GtkTreeIter iter;
  WidgetInfo *info;

  widget = gtk_frame_new (NULL);
  store = gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Line One", 1, FALSE, 2, "A", -1);
  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Line Two", 1, TRUE, 2, "B", -1);
  gtk_tree_store_append (store, &iter, &iter);
  gtk_tree_store_set (store, &iter, 0, "Line Three", 1, FALSE, 2, "C", -1);

  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       0, "List",
					       gtk_cell_renderer_text_new (),
					       "text", 0, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       1, "and",
					       gtk_cell_renderer_toggle_new (),
                                               "active", 1, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       2, "Tree",
					       g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "xalign", 0.5, NULL),
					       "text", 2, NULL);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));
  gtk_frame_set_child (GTK_FRAME (widget), tree_view);

  info = new_widget_info ("list-and-tree", widget, MEDIUM);
  info->no_focus = FALSE;

  return info;
}

static WidgetInfo *
create_icon_view (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;
  GtkWidget *icon_view;
  GtkListStore *list_store;
  GtkTreeIter iter;
  GdkPixbuf *pixbuf;
  WidgetInfo *info;
  GtkWidget *sw;

  widget = gtk_frame_new (NULL);
  list_store = gtk_list_store_new (2, G_TYPE_STRING, GDK_TYPE_PIXBUF);
  gtk_list_store_append (list_store, &iter);
  pixbuf = gdk_pixbuf_new_from_file ("folder.png", NULL);
  gtk_list_store_set (list_store, &iter, 0, "One", 1, pixbuf, -1);
  gtk_list_store_append (list_store, &iter);
  pixbuf = gdk_pixbuf_new_from_file ("gnome.png", NULL);
  gtk_list_store_set (list_store, &iter, 0, "Two", 1, pixbuf, -1);

  icon_view = gtk_icon_view_new ();

  gtk_icon_view_set_item_orientation (GTK_ICON_VIEW (icon_view), GTK_ORIENTATION_HORIZONTAL);
  gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (icon_view), 0);

  gtk_icon_view_set_model (GTK_ICON_VIEW (icon_view), GTK_TREE_MODEL (list_store));
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (icon_view), 0);
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (icon_view), 1);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), icon_view);

  gtk_frame_set_child (GTK_FRAME (widget), sw);

  gtk_widget_set_size_request (widget, 96, 128);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Icon View"));

  add_margin (vbox);

  info = new_widget_info ("icon-view", vbox, MEDIUM);
  info->no_focus = FALSE;

  return info;
}

static WidgetInfo *
create_color_button (void)
{
  GtkWidget *vbox;
  GtkWidget *picker;
  GdkRGBA color;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  color.red = 0x1e<<8;
  color.green = 0x90<<8;
  color.blue = 0xff<<8;
  color.alpha = 0xffff;
  picker = gtk_color_button_new_with_rgba (&color);
  gtk_widget_set_halign (picker, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picker, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), picker);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Color Button"));

  add_margin (vbox);

  return new_widget_info ("color-button", vbox, SMALL);
}

static WidgetInfo *
create_font_button (void)
{
  GtkWidget *vbox;
  GtkWidget *picker;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  picker = gtk_font_button_new_with_font ("Sans Serif 10");
  gtk_widget_set_halign (picker, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picker, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), picker);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Font Button"));

  add_margin (vbox);

  return new_widget_info ("font-button", vbox, SMALL);
}

static WidgetInfo *
create_editable_label (void)
{
  GtkWidget *vbox;
  GtkWidget *widget;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  widget = gtk_editable_label_new ("Editable Label");
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  widget = gtk_editable_label_new ("Editable Label");
  gtk_editable_label_start_editing (GTK_EDITABLE_LABEL (widget));
  gtk_widget_add_css_class (widget, "frame");
  gtk_box_append (GTK_BOX (vbox), widget);

  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

  add_margin (vbox);

  return new_widget_info ("editable-label", vbox, SMALL);
}

static WidgetInfo *
create_separator (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *widget;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_size_request (widget, 100, -1);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hbox), widget);
  widget = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_size_request (widget, -1, 100);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (vbox), g_object_new (GTK_TYPE_LABEL,
                                                "label", "Horizontal and Vertical\nSeparators",
                                                "justify", GTK_JUSTIFY_CENTER,
                                                NULL));
  add_margin (vbox);

  return new_widget_info ("separator", vbox, MEDIUM);
}

static WidgetInfo *
create_panes (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *pane;
  GtkWidget *frame;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

  frame = gtk_frame_new ("");
  gtk_paned_set_start_child (GTK_PANED (pane), frame);
  gtk_paned_set_resize_start_child (GTK_PANED (pane), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (pane), FALSE);

  frame = gtk_frame_new ("");
  gtk_paned_set_end_child (GTK_PANED (pane), frame);
  gtk_paned_set_resize_end_child (GTK_PANED (pane), FALSE);
  gtk_paned_set_shrink_end_child (GTK_PANED (pane), FALSE);

  gtk_widget_set_size_request (pane, 96, 96);

  gtk_box_append (GTK_BOX (hbox), pane);
  pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);

  frame = gtk_frame_new ("");
  gtk_paned_set_start_child (GTK_PANED (pane), frame);
  gtk_paned_set_resize_start_child (GTK_PANED (pane), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (pane), FALSE);

  frame = gtk_frame_new ("");
  gtk_paned_set_end_child (GTK_PANED (pane), frame);
  gtk_paned_set_resize_end_child (GTK_PANED (pane), FALSE);
  gtk_paned_set_shrink_end_child (GTK_PANED (pane), FALSE);

  gtk_widget_set_size_request (pane, 96, 96);

  gtk_box_append (GTK_BOX (hbox), pane);

  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (vbox), g_object_new (GTK_TYPE_LABEL,
                                                "label", "Horizontal and Vertical\nPanes",
                                                "justify", GTK_JUSTIFY_CENTER,
                                                NULL));

  add_margin (vbox);

  return new_widget_info ("panes", vbox, MEDIUM);
}

static WidgetInfo *
create_frame (void)
{
  GtkWidget *widget;

  widget = gtk_frame_new ("Frame");
  gtk_widget_set_size_request (widget, 96, 96);

  add_margin (widget);

  return new_widget_info ("frame", widget, MEDIUM);
}

static WidgetInfo *
create_window (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_window_new ();
  info = new_widget_info ("window", widget, MEDIUM);
  info->include_decorations = TRUE;
  gtk_window_set_title (GTK_WINDOW (info->window), "Window");

  return info;
}

static WidgetInfo *
create_filesel (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_file_chooser_dialog_new ("File Chooser Dialog",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					"Cancel", GTK_RESPONSE_CANCEL,
					"Open", GTK_RESPONSE_ACCEPT,
					NULL); 
  gtk_window_set_default_size (GTK_WINDOW (widget), 505, 305);
  
  info = new_widget_info ("filechooser", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_print_dialog (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_print_unix_dialog_new ("Print Dialog", NULL);   
  gtk_widget_set_size_request (widget, 505, 350);
  info = new_widget_info ("printdialog", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_page_setup_dialog (void)
{
  WidgetInfo *info;
  GtkWidget *widget;
  GtkPageSetup *page_setup;
  GtkPrintSettings *settings;

  page_setup = gtk_page_setup_new ();
  settings = gtk_print_settings_new ();
  widget = gtk_page_setup_unix_dialog_new ("Page Setup Dialog", NULL);   
  gtk_page_setup_unix_dialog_set_page_setup (GTK_PAGE_SETUP_UNIX_DIALOG (widget),
					     page_setup);
  gtk_page_setup_unix_dialog_set_print_settings (GTK_PAGE_SETUP_UNIX_DIALOG (widget),
						 settings);

  info = new_widget_info ("pagesetupdialog", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_message_dialog (void)
{
  GtkWidget *widget;

  widget = gtk_message_dialog_new (NULL,
				   0,
				   GTK_MESSAGE_INFO,
				   GTK_BUTTONS_OK,
				   NULL);
  gtk_window_set_icon_name (GTK_WINDOW (widget), "edit-copy");
  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (widget), "Message Dialog");
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (widget), "%s", "With secondary text");
  return new_widget_info ("messagedialog", widget, ASIS);
}

static WidgetInfo *
create_dialog (void)
{
  GtkWidget *widget;
  GtkWidget *content;
  GtkWidget *label;

  widget = g_object_new (GTK_TYPE_DIALOG, "use-header-bar", TRUE, NULL);
  gtk_window_set_title (GTK_WINDOW (widget), "Dialog");

  gtk_dialog_add_button (GTK_DIALOG (widget), "Accept", GTK_RESPONSE_OK);
  gtk_dialog_add_button (GTK_DIALOG (widget), "Cancel", GTK_RESPONSE_CANCEL);

  gtk_dialog_set_default_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);

  content = gtk_dialog_get_content_area (GTK_DIALOG (widget));
  label = gtk_label_new ("Content");
  g_object_set (label,
                "margin-start", 20,
                "margin-end", 20,
                "margin-top", 20,
                "margin-bottom", 20,
                NULL);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (content), label);

  return new_widget_info ("dialog", widget, ASIS);
}

static WidgetInfo *
create_about_dialog (void)
{
  GtkWidget *widget;
  const char *authors[] = {
    "Peter Mattis",
    "Spencer Kimball",
    "Josh MacDonald",
    "and many more...",
    NULL
  };
  GFile *file;
  GdkTexture *logo;

  file = g_file_new_for_path ("docs/tools/gtk-logo.png");
  logo = gdk_texture_new_from_file (file, NULL);

  widget = gtk_about_dialog_new ();
  g_object_set (widget,
                "program-name", "GTK Code Demos",
                "version", PACKAGE_VERSION,
                "copyright", "© 1997-2021 The GTK Team",
                "website", "http://www.gtk.org",
                "comments", "Program to demonstrate GTK functions.",
                "logo", logo,
                "title", "About GTK Code Demos",
                "authors", authors,
		NULL);

  g_object_unref (logo);
  g_object_unref (file);

  return new_widget_info ("aboutdialog", widget, ASIS);
}

static WidgetInfo *
create_notebook (void)
{
  GtkWidget *widget;

  widget = gtk_notebook_new ();

  gtk_notebook_append_page (GTK_NOTEBOOK (widget), 
			    gtk_label_new ("Notebook"),
			    NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (widget),
			    gtk_label_new ("Notebook"),
			    NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (widget),
			    gtk_label_new ("Notebook"),
			    NULL);

  return new_widget_info ("notebook", widget, MEDIUM);
}

static WidgetInfo *
create_progressbar (void)
{
  GtkWidget *vbox;
  GtkWidget *widget;

  widget = gtk_progress_bar_new ();
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (widget), 0.5);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Progress Bar"));

  add_margin (vbox);

  return new_widget_info ("progressbar", vbox, SMALL);
}

static WidgetInfo *
create_level_bar (void)
{
  GtkWidget *vbox;
  GtkWidget *widget;

  widget = gtk_level_bar_new ();
  gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), 0.333);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Level Bar"));

  add_margin (vbox);

  return new_widget_info ("levelbar", vbox, SMALL);
}

static WidgetInfo *
create_scrolledwindow (void)
{
  GtkWidget *scrolledwin, *label;

  scrolledwin = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scrolledwin), FALSE);
  label = gtk_label_new ("Scrolled Window");

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwin), label);

  return new_widget_info ("scrolledwindow", scrolledwin, MEDIUM);
}

static WidgetInfo *
create_scrollbar (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;

  widget = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  gtk_widget_set_size_request (widget, 100, -1);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Scrollbar"));

  add_margin (vbox);

  return new_widget_info ("scrollbar", vbox, SMALL);
}

static WidgetInfo *
create_spinbutton (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;

  widget = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Spin Button"));

  add_margin (vbox);

  return new_widget_info ("spinbutton", vbox, SMALL);
}

static WidgetInfo *
create_statusbar (void)
{
  WidgetInfo *info;
  GtkWidget *widget;
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (vbox),
                      gtk_label_new ("Status Bar"));
  widget = gtk_statusbar_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_statusbar_push (GTK_STATUSBAR (widget), 0, "Hold on...");

  gtk_box_append (GTK_BOX (vbox), widget);

  add_margin (vbox);

  info = new_widget_info ("statusbar", vbox, SMALL);

  return info;
}

static WidgetInfo *
create_scales (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *widget;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  widget = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
  gtk_scale_set_draw_value (GTK_SCALE (widget), FALSE);
  gtk_range_set_value (GTK_RANGE (widget), 50.);
  gtk_widget_set_size_request (widget, 100, -1);
  gtk_box_append (GTK_BOX (hbox), widget);
  widget = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0.0, 100.0, 1.0);
  gtk_scale_set_draw_value (GTK_SCALE (widget), FALSE);
  gtk_widget_set_size_request (widget, -1, 100);
  gtk_range_set_value (GTK_RANGE (widget), 50.);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (vbox),
		      g_object_new (GTK_TYPE_LABEL,
				    "label", "Horizontal and Vertical\nScales",
				    "justify", GTK_JUSTIFY_CENTER,
				    NULL));
  add_margin (vbox);

  return new_widget_info ("scales", vbox, MEDIUM);}

static WidgetInfo *
create_image (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;

  widget = gtk_image_new_from_icon_name ("applications-graphics");
  gtk_image_set_icon_size (GTK_IMAGE (widget), GTK_ICON_SIZE_LARGE);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Image"));

  add_margin (vbox);

  return new_widget_info ("image", vbox, SMALL);
}

static WidgetInfo *
create_picture (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;
  GtkIconTheme *theme;
  GdkPaintable *paintable;
  GtkWidget *box;

  theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  paintable = GDK_PAINTABLE (gtk_icon_theme_lookup_icon (theme,
                                                         "applications-graphics",
                                                         NULL,
                                                         48, 1, GTK_TEXT_DIR_LTR,
                                                         0));

  widget = gtk_picture_new_for_paintable (paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (widget), TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (box), g_object_new (GTK_TYPE_IMAGE, "hexpand", TRUE, NULL));
  gtk_box_append (GTK_BOX (box), widget);
  gtk_box_append (GTK_BOX (box), g_object_new (GTK_TYPE_IMAGE, "hexpand", TRUE, NULL));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), box);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Picture"));

  add_margin (vbox);

  return new_widget_info ("picture", vbox, SMALL);
}

static WidgetInfo *
create_video (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;
  WidgetInfo *info;

  widget = gtk_video_new_for_filename ("demos/gtk-demo/gtk-logo.webm");
  gtk_video_set_autoplay (GTK_VIDEO (widget), TRUE);

  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_FILL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Video"));

  add_margin (vbox);

  info = new_widget_info ("video", vbox, MEDIUM);
  info->wait = 2000;

  return info;
}

static WidgetInfo *
create_media_controls (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;
  GtkMediaStream *stream;
  WidgetInfo *info;

  stream = gtk_media_file_new_for_filename ("demos/gtk-demo/gtk-logo.webm");
  gtk_media_stream_play (stream);
  widget = gtk_media_controls_new (stream);
  gtk_widget_set_size_request (widget, 210, -1);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Media Controls"));

  add_margin (vbox);

  info = new_widget_info ("media-controls", vbox, SMALL);
  info->wait = 2000;

  return info;
}

static WidgetInfo *
create_spinner (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;

  widget = gtk_spinner_new ();
  gtk_widget_set_size_request (widget, 24, 24);
  gtk_spinner_start (GTK_SPINNER (widget));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Spinner"));

  add_margin (vbox);

  return new_widget_info ("spinner", vbox, SMALL);
}

static WidgetInfo *
create_volume_button (void)
{
  GtkWidget *widget, *vbox;

  widget = gtk_volume_button_new ();
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (widget), 33);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Volume Button"));

  return new_widget_info ("volumebutton", vbox, SMALL);
}

static WidgetInfo *
create_assistant (void)
{
  GtkWidget *widget;
  GtkWidget *page1, *page2;
  WidgetInfo *info;

  widget = gtk_assistant_new ();
  gtk_window_set_title (GTK_WINDOW (widget), "Assistant");

  page1 = gtk_label_new ("Assistant");
  gtk_widget_set_size_request (page1, 300, 140);
  gtk_assistant_prepend_page (GTK_ASSISTANT (widget), page1);
  gtk_assistant_set_page_title (GTK_ASSISTANT (widget), page1, "Assistant page");
  gtk_assistant_set_page_complete (GTK_ASSISTANT (widget), page1, TRUE);

  page2 = gtk_label_new (NULL);
  gtk_assistant_append_page (GTK_ASSISTANT (widget), page2);
  gtk_assistant_set_page_type (GTK_ASSISTANT (widget), page2, GTK_ASSISTANT_PAGE_CONFIRM);

  info = new_widget_info ("assistant", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_appchooserbutton (void)
{
  GtkWidget *picker;
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  picker = gtk_app_chooser_button_new ("text/plain");
  gtk_widget_set_halign (picker, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picker, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), picker);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Application Button"));

  add_margin (vbox);

  return new_widget_info ("appchooserbutton", vbox, SMALL);
}

static WidgetInfo *
create_appchooserdialog (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_app_chooser_dialog_new_for_content_type (NULL, 0, "image/png");
  gtk_window_set_default_size (GTK_WINDOW (widget), 200, 300);

  info = new_widget_info ("appchooserdialog", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_fontchooserdialog (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_font_chooser_dialog_new ("Font Chooser Dialog", NULL);
  gtk_window_set_default_size (GTK_WINDOW (widget), 200, 300);
  info = new_widget_info ("fontchooser", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_colorchooserdialog (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_color_chooser_dialog_new ("Color Chooser Dialog", NULL);
  info = new_widget_info ("colorchooser", widget, ASIS);
  info->include_decorations = TRUE;

  return info;
}

static WidgetInfo *
create_headerbar (void)
{
  GtkWidget *window;
  GtkWidget *bar;
  GtkWidget *view;
  GtkWidget *button;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Header Bar");
  view = gtk_text_view_new ();
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_widget_set_size_request (window, 220, 150);
  gtk_window_set_child (GTK_WINDOW (window), view);
  bar = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), bar);
  button = gtk_button_new ();
  gtk_button_set_child (GTK_BUTTON (button), gtk_image_new_from_icon_name ("bookmark-new-symbolic"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (bar), button);

  return new_widget_info ("headerbar", window, ASIS);
}

static WidgetInfo *
create_stack (void)
{
  GtkWidget *stack;
  GtkWidget *switcher;
  GtkWidget *vbox;
  GtkWidget *view;

  stack = gtk_stack_new ();
  gtk_widget_set_margin_top (stack, 10);
  gtk_widget_set_margin_bottom (stack, 10);
  gtk_widget_set_size_request (stack, 120, 120);
  view = gtk_text_view_new ();
  gtk_stack_add_titled (GTK_STACK (stack), view, "page1", "Page 1");
  view = gtk_text_view_new ();
  gtk_stack_add_titled (GTK_STACK (stack), view, "page2", "Page 2");

  switcher = gtk_stack_switcher_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_widget_set_halign (switcher, GTK_ALIGN_CENTER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_append (GTK_BOX (vbox), switcher);
  gtk_box_append (GTK_BOX (vbox), stack);
  gtk_box_append (GTK_BOX (vbox),
                      gtk_label_new ("Stack"));

  return new_widget_info ("stack", vbox, ASIS);
}

static WidgetInfo *
create_stack_switcher (void)
{
  GtkWidget *stack;
  GtkWidget *switcher;
  GtkWidget *vbox;
  GtkWidget *view;

  stack = gtk_stack_new ();
  gtk_widget_set_margin_top (stack, 10);
  gtk_widget_set_margin_bottom (stack, 10);
  gtk_widget_set_size_request (stack, 120, 120);
  view = gtk_text_view_new ();
  gtk_stack_add_titled (GTK_STACK (stack), view, "page1", "Page 1");
  view = gtk_text_view_new ();
  gtk_stack_add_titled (GTK_STACK (stack), view, "page2", "Page 2");

  switcher = gtk_stack_switcher_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_widget_set_halign (switcher, GTK_ALIGN_CENTER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_append (GTK_BOX (vbox), switcher);
  gtk_box_append (GTK_BOX (vbox), stack);
  gtk_box_append (GTK_BOX (vbox),
                      gtk_label_new ("Stack Switcher"));

  return new_widget_info ("stackswitcher", vbox, ASIS);
}

static WidgetInfo *
create_sidebar (void)
{
  GtkWidget *stack;
  GtkWidget *sidebar;
  GtkWidget *hbox;
  GtkWidget *view;
  GtkWidget *frame;

  stack = gtk_stack_new ();
  gtk_widget_set_size_request (stack, 120, 120);
  view = gtk_label_new ("Sidebar");
  gtk_widget_add_css_class (view, "view");
  gtk_widget_set_halign (view, GTK_ALIGN_FILL);
  gtk_widget_set_valign (view, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (view, TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page1", "Page 1");
  view = gtk_text_view_new ();
  gtk_stack_add_titled (GTK_STACK (stack), view, "page2", "Page 2");

  sidebar = gtk_stack_sidebar_new ();
  gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (sidebar), GTK_STACK (stack));

  frame = gtk_frame_new (NULL);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_box_append (GTK_BOX (hbox), sidebar);
  gtk_box_append (GTK_BOX (hbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));
  gtk_box_append (GTK_BOX (hbox), stack);
  gtk_frame_set_child (GTK_FRAME (frame), hbox);

  return new_widget_info ("sidebar", frame, MEDIUM);
}

static WidgetInfo *
create_list_box (void)
{
  GtkWidget *widget;
  GtkWidget *list;
  GtkWidget *row;
  GtkWidget *button;
  WidgetInfo *info;

  widget = gtk_frame_new (NULL);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_BROWSE);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  button = gtk_label_new ("List Box");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (row), button);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (row), gtk_label_new ("Line One"));
  button = gtk_check_button_new ();
  gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (row), button);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
  gtk_list_box_select_row (GTK_LIST_BOX (list), GTK_LIST_BOX_ROW (gtk_widget_get_parent (row)));
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (row), gtk_label_new ("Line Two"));
  button = gtk_button_new_with_label ("2");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (row), button);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (row), gtk_label_new ("Line Three"));
  button = gtk_entry_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (row), button);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);

  gtk_frame_set_child (GTK_FRAME (widget), list);

  info = new_widget_info ("list-box", widget, MEDIUM);
  info->no_focus = FALSE;

  return info;
}

static WidgetInfo *
create_flow_box (void)
{
  GtkWidget *widget;
  GtkWidget *box;
  GtkWidget *vbox;
  GtkWidget *child;
  GtkWidget *button;
  WidgetInfo *info;

  widget = gtk_frame_new (NULL);

  box = gtk_flow_box_new ();
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 2);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (box), 2);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_BROWSE);
  button = gtk_label_new ("Child One");
  gtk_flow_box_insert (GTK_FLOW_BOX (box), button, -1);
  button = gtk_button_new_with_label ("Child Two");
  gtk_flow_box_insert (GTK_FLOW_BOX (box), button, -1);
  child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (child), gtk_label_new ("Child Three"));
  button = gtk_check_button_new ();
  gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
  gtk_box_append (GTK_BOX (child), button);
  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, -1);
  gtk_flow_box_select_child (GTK_FLOW_BOX (box),
                             GTK_FLOW_BOX_CHILD (gtk_widget_get_parent (child)));

  gtk_frame_set_child (GTK_FRAME (widget), box);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Flow Box"));
  info = new_widget_info ("flow-box", vbox, ASIS);
  info->no_focus = FALSE;

  return info;
}

static WidgetInfo *
create_gl_area (void)
{
  GtkWidget *vbox;
  WidgetInfo *info;
  GtkWidget *widget;
  GtkWidget *gears;
  GtkCssProvider *provider;

  widget = gtk_frame_new (NULL);
  gears = gtk_gears_new ();
  gtk_widget_add_css_class (gears, "velvet");
  gtk_widget_set_size_request (gears, 96, 96);
  gtk_frame_set_child (GTK_FRAME (widget), gears);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, ".velvet { background: black; }", -1);
  gtk_style_context_add_provider (gtk_widget_get_style_context (gears), GTK_STYLE_PROVIDER (provider), 800);
  g_object_unref (provider);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("GL Area"));

  add_margin (vbox);

  info = new_widget_info ("glarea", vbox, MEDIUM);

  return info;
}

static WidgetInfo *
create_window_controls (void)
{
  GtkWidget *controls;
  GtkWidget *vbox;

  controls = gtk_window_controls_new (GTK_PACK_END);
  gtk_window_controls_set_decoration_layout (GTK_WINDOW_CONTROLS (controls),
                                             ":minimize,maximize,close");
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (controls, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (controls, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), controls);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Window Controls"));

  add_margin (vbox);

  return new_widget_info ("windowcontrols", vbox, SMALL);
}

static WidgetInfo *
create_calendar (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;

  widget = gtk_calendar_new ();

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Calendar"));

  add_margin (vbox);

  return new_widget_info ("calendar", vbox, MEDIUM);
}

static WidgetInfo *
create_emojichooser (void)
{
  GtkWidget *widget;
  WidgetInfo *info;

  widget = gtk_emoji_chooser_new ();
  g_object_set (widget, "autohide", FALSE, NULL);

  info = new_widget_info ("emojichooser", widget, ASIS);
  info->wait = 2000;

  return info;
}

static WidgetInfo *
create_expander (void)
{
  GtkWidget *widget;

  widget = gtk_expander_new ("Expander");
  gtk_expander_set_child (GTK_EXPANDER (widget), gtk_label_new ("Hidden Content"));
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("expander", widget, SMALL);
}

static void
mapped_cb (GtkWidget *widget)
{
  gtk_widget_child_focus (widget, GTK_DIR_RIGHT);
}

static WidgetInfo *
create_menu_bar (void)
{
  GtkWidget *widget;
  GtkWidget *vbox;
  GMenu *menu, *menu1;
  GMenuItem *item;

  menu = g_menu_new ();
  menu1 = g_menu_new ();
  item = g_menu_item_new ("Item", "action");
  g_menu_append_item (menu1, item);
  g_menu_append_submenu (menu, "File", G_MENU_MODEL (menu1));
  g_object_unref (item);
  g_object_unref (menu1);
  menu1 = g_menu_new ();
  item = g_menu_item_new ("Item", "action");
  g_menu_append_item (menu1, item);
  g_menu_append_submenu (menu, "Edit", G_MENU_MODEL (menu1));
  g_object_unref (item);
  g_object_unref (menu1);
  menu1 = g_menu_new ();
  item = g_menu_item_new ("Item", "action");
  g_menu_append_item (menu1, item);
  g_menu_append_submenu (menu, "View", G_MENU_MODEL (menu1));
  g_object_unref (item);
  g_object_unref (menu1);

  widget = gtk_popover_menu_bar_new_from_model (G_MENU_MODEL (menu));

  g_object_unref (menu);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Menu Bar"));

  add_margin (vbox);

  g_signal_connect (widget, "map", G_CALLBACK (mapped_cb), NULL);

  return new_widget_info ("menubar", vbox, SMALL);
}

static WidgetInfo *
create_popover (void)
{
  GtkWidget *widget;
  GtkWidget *child;
  WidgetInfo *info;

  widget = gtk_popover_new ();
  gtk_widget_set_size_request (widget, 180, 180);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  g_object_set (widget, "autohide", FALSE, NULL);
  child = gtk_label_new ("Popover");
  gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
  gtk_popover_set_child (GTK_POPOVER (widget), child);

  info = new_widget_info ("popover", widget, ASIS);
  info->wait = 100;

  return info;
}

static WidgetInfo *
create_menu (void)
{
  GtkWidget *widget;
  GMenu *menu, *menu1;
  GMenuItem *item;
  GSimpleActionGroup *group;
  GSimpleAction *action;
  GtkEventController *controller;
  GtkShortcut *shortcut;

  menu = g_menu_new ();
  menu1 = g_menu_new ();
  item = g_menu_item_new ("Item", "action");
  g_menu_append_item (menu1, item);
  g_menu_append_submenu (menu, "Style", G_MENU_MODEL (menu1));
  g_object_unref (item);
  g_object_unref (menu1);
  item = g_menu_item_new ("Transition", "menu.transition");
  g_menu_append_item (menu, item);
  g_object_unref (item);

  menu1 = g_menu_new ();
  item = g_menu_item_new ("Inspector", "menu.inspector");
  g_menu_append_item (menu1, item);
  g_object_unref (item);
  item = g_menu_item_new ("About", "menu.about");
  g_menu_append_item (menu1, item);
  g_object_unref (item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (menu1));
  g_object_unref (menu1);

  widget = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));

  g_object_unref (menu);

  group = g_simple_action_group_new ();
  action = g_simple_action_new_stateful ("transition", NULL, g_variant_new_boolean (TRUE));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_object_unref (action);
  action = g_simple_action_new ("inspector", NULL);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_object_unref (action);
  action = g_simple_action_new ("about", NULL);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_object_unref (action);

  gtk_widget_insert_action_group (widget, "menu", G_ACTION_GROUP (group));

  g_object_unref (group);

  g_object_set (widget, "autohide", FALSE, NULL);

  controller = gtk_shortcut_controller_new ();
  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_F1, 0),
                               gtk_named_action_new ("menu.about"));
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_add_controller (widget, controller);

  return new_widget_info ("menu", widget, ASIS);
}

static WidgetInfo *
create_shortcuts_window (void)
{
  GtkBuilder *builder;
  GtkWidget *overlay;

  builder = gtk_builder_new_from_resource ("/shortcuts-boxes.ui");
  overlay = GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts-boxes"));
  g_object_set (overlay, "view-name", "display", NULL);
  g_object_ref (overlay);
  g_object_unref (builder);

  return new_widget_info ("shortcuts-window", overlay, ASIS);
}

static void
oval_path (cairo_t *cr,
           double xc, double yc,
           double xr, double yr)
{
  cairo_save (cr);

  cairo_translate (cr, xc, yc);
  cairo_scale (cr, 1.0, yr / xr);
  cairo_move_to (cr, xr, 0.0);
  cairo_arc (cr,
             0, 0,
             xr,
             0, 2 * G_PI);
  cairo_close_path (cr);

  cairo_restore (cr);
}

static void
fill_checks (cairo_t *cr,
             int x,     int y,
             int width, int height)
{
  int i, j;

#define CHECK_SIZE 16

  cairo_rectangle (cr, x, y, width, height);
  cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
  cairo_fill (cr);

  /* Only works for CHECK_SIZE a power of 2 */
  j = x & (-CHECK_SIZE);

  for (; j < height; j += CHECK_SIZE)
    {
      i = y & (-CHECK_SIZE);
      for (; i < width; i += CHECK_SIZE)
        if ((i / CHECK_SIZE + j / CHECK_SIZE) % 2 == 0)
          cairo_rectangle (cr, i, j, CHECK_SIZE, CHECK_SIZE);
    }

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_fill (cr);

#undef CHECK_SIZE
}

static void
draw_3circles (cairo_t *cr,
               double xc, double yc,
               double radius,
               double alpha)
{
  double subradius = radius * (2 / 3. - 0.1);

  cairo_set_source_rgba (cr, 1., 0., 0., alpha);
  oval_path (cr,
             xc + radius / 3. * cos (G_PI * (0.5)),
             yc - radius / 3. * sin (G_PI * (0.5)),
             subradius, subradius);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0., 1., 0., alpha);
  oval_path (cr,
             xc + radius / 3. * cos (G_PI * (0.5 + 2/.3)),
             yc - radius / 3. * sin (G_PI * (0.5 + 2/.3)),
             subradius, subradius);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0., 0., 1., alpha);
  oval_path (cr,
             xc + radius / 3. * cos (G_PI * (0.5 + 4/.3)),
             yc - radius / 3. * sin (G_PI * (0.5 + 4/.3)),
             subradius, subradius);
  cairo_fill (cr);
}

static void
groups_draw (GtkDrawingArea *darea,
             cairo_t        *cr,
             int             width,
             int             height,
             gpointer        data)
{
  cairo_surface_t *overlay, *punch, *circles;
  cairo_t *overlay_cr, *punch_cr, *circles_cr;

  /* Fill the background */
  double radius = 0.5 * (width < height ? width : height) - 10;
  double xc = width / 2.;
  double yc = height / 2.;

  overlay = cairo_surface_create_similar (cairo_get_target (cr),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          width, height);

  punch = cairo_surface_create_similar (cairo_get_target (cr),
                                        CAIRO_CONTENT_ALPHA,
                                        width, height);

  circles = cairo_surface_create_similar (cairo_get_target (cr),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          width, height);

  fill_checks (cr, 0, 0, width, height);

  /* Draw a black circle on the overlay
   */
  overlay_cr = cairo_create (overlay);
  cairo_set_source_rgb (overlay_cr, 0., 0., 0.);
  oval_path (overlay_cr, xc, yc, radius, radius);
  cairo_fill (overlay_cr);

  /* Draw 3 circles to the punch surface, then cut
   * that out of the main circle in the overlay
   */
  punch_cr = cairo_create (punch);
  draw_3circles (punch_cr, xc, yc, radius, 1.0);
  cairo_destroy (punch_cr);

  cairo_set_operator (overlay_cr, CAIRO_OPERATOR_DEST_OUT);
  cairo_set_source_surface (overlay_cr, punch, 0, 0);
  cairo_paint (overlay_cr);

  /* Now draw the 3 circles in a subgroup again
   * at half intensity, and use OperatorAdd to join up
   * without seams.
   */
  circles_cr = cairo_create (circles);

  cairo_set_operator (circles_cr, CAIRO_OPERATOR_OVER);
  draw_3circles (circles_cr, xc, yc, radius, 0.5);
  cairo_destroy (circles_cr);

  cairo_set_operator (overlay_cr, CAIRO_OPERATOR_ADD);
  cairo_set_source_surface (overlay_cr, circles, 0, 0);
  cairo_paint (overlay_cr);

  cairo_destroy (overlay_cr);

  cairo_set_source_surface (cr, overlay, 0, 0);
  cairo_paint (cr);

  cairo_surface_destroy (overlay);
  cairo_surface_destroy (punch);
  cairo_surface_destroy (circles);
}


static WidgetInfo *
create_drawing_area (void)
{
  GtkWidget *vbox;
  WidgetInfo *info;
  GtkWidget *widget;
  GtkWidget *da;

  widget = gtk_frame_new (NULL);
  da = gtk_drawing_area_new ();
  gtk_widget_set_size_request (da, 96, 96);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), groups_draw, NULL, NULL);
  gtk_frame_set_child (GTK_FRAME (widget), da);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_FILL);

  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Drawing Area"));

  add_margin (vbox);

  info = new_widget_info ("drawingarea", vbox, MEDIUM);

  return info;
}

static WidgetInfo *
create_box (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *widget;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  gtk_widget_set_margin_top (hbox, 20);
  gtk_widget_set_margin_bottom (hbox, 20);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  for (int i = 0; i < 2; i++)
    {
      GtkWidget *button = gtk_button_new ();
      gtk_widget_add_css_class (button, "small");
      gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (widget), button);
    }
  gtk_box_append (GTK_BOX (widget), gtk_label_new ("⋯"));
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hbox), widget);
  widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  for (int i = 0; i < 2; i++)
    {
      GtkWidget *button = gtk_button_new ();
      gtk_widget_add_css_class (button, "small");
      gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (widget), button);
    }
  gtk_box_append (GTK_BOX (widget), gtk_label_new ("⋮"));
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (vbox), g_object_new (GTK_TYPE_LABEL,
                                                "label", "Horizontal and Vertical Boxes",
                                                "justify", GTK_JUSTIFY_CENTER,
                                                NULL));
  add_margin (vbox);

  return new_widget_info ("box", vbox, MEDIUM);
}

static WidgetInfo *
create_center_box (void)
{
  GtkWidget *vbox;
  GtkWidget *widget;
  GtkWidget *button;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  widget = gtk_center_box_new ();
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
  gtk_widget_set_margin_start (widget, 20);
  gtk_widget_set_margin_end (widget, 20);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (widget), button);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (widget), button);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (widget), button);

  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), g_object_new (GTK_TYPE_LABEL,
                                                "label", "Center Box",
                                                "justify", GTK_JUSTIFY_CENTER,
                                                NULL));
  add_margin (vbox);

  return new_widget_info ("centerbox", vbox, SMALL);
}

static WidgetInfo *
create_grid (void)
{
  GtkWidget *vbox;
  GtkWidget *widget;
  GtkWidget *button;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  widget = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (widget), 4);
  gtk_grid_set_column_spacing (GTK_GRID (widget), 4);
  gtk_widget_set_margin_top (widget, 20);
  gtk_widget_set_margin_bottom (widget, 20);
  gtk_widget_set_margin_start (widget, 20);
  gtk_widget_set_margin_end (widget, 20);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (widget), button, 0, 0, 1, 1);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (widget), button, 0, 1, 1, 1);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (widget), button, 1, 0, 1, 1);

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "small");
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (widget), button, 1, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (widget), gtk_label_new ("⋯"), 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (widget), gtk_label_new ("⋮"), 0, 2, 1, 1);

  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), g_object_new (GTK_TYPE_LABEL,
                                                "label", "Grid",
                                                "justify", GTK_JUSTIFY_CENTER,
                                                NULL));
  add_margin (vbox);

  return new_widget_info ("grid", vbox, MEDIUM);
}

static WidgetInfo *
create_overlay (void)
{
  GtkWidget *vbox;
  WidgetInfo *info;
  GtkWidget *widget;
  GtkWidget *overlay;
  GtkWidget *label;
  GtkWidget *child;

  widget = gtk_frame_new (NULL);
  overlay = gtk_overlay_new ();
  gtk_widget_add_css_class (widget, "view");
  label = gtk_label_new ("Content");
  gtk_widget_set_vexpand (label, TRUE);
  gtk_frame_set_child (GTK_FRAME (widget), overlay);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), label);

  child = gtk_frame_new (NULL);
  gtk_widget_add_css_class (child, "app-notification");
  gtk_frame_set_child (GTK_FRAME (child), gtk_label_new ("Overlay"));
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_FILL);

  gtk_box_append (GTK_BOX (vbox), widget);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Overlay"));

  add_margin (vbox);

  info = new_widget_info ("overlay", vbox, MEDIUM);

  return info;
}

GList *
get_all_widgets (void)
{
  GList *retval = NULL;
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   "button.small {\n"
                                   "  min-width: 16px;\n"
                                   "  min-height: 16px;\n"
                                   "  padding: 0;\n"
                                   "}", -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);

  retval = g_list_prepend (retval, create_search_bar ());
  retval = g_list_prepend (retval, create_action_bar ());
  retval = g_list_prepend (retval, create_list_box());
  retval = g_list_prepend (retval, create_flow_box());
  retval = g_list_prepend (retval, create_headerbar ());
  retval = g_list_prepend (retval, create_stack ());
  retval = g_list_prepend (retval, create_stack_switcher ());
  retval = g_list_prepend (retval, create_spinner ());
  retval = g_list_prepend (retval, create_about_dialog ());
  retval = g_list_prepend (retval, create_button ());
  retval = g_list_prepend (retval, create_check_button ());
  retval = g_list_prepend (retval, create_radio_button ());
  retval = g_list_prepend (retval, create_color_button ());
  retval = g_list_prepend (retval, create_combo_box ());
  retval = g_list_prepend (retval, create_combo_box_entry ());
  retval = g_list_prepend (retval, create_combo_box_text ());
  retval = g_list_prepend (retval, create_dialog ());
  retval = g_list_prepend (retval, create_entry ());
  retval = g_list_prepend (retval, create_font_button ());
  retval = g_list_prepend (retval, create_frame ());
  retval = g_list_prepend (retval, create_icon_view ());
  retval = g_list_prepend (retval, create_image ());
  retval = g_list_prepend (retval, create_label ());
  retval = g_list_prepend (retval, create_link_button ());
  retval = g_list_prepend (retval, create_message_dialog ());
  retval = g_list_prepend (retval, create_notebook ());
  retval = g_list_prepend (retval, create_panes ());
  retval = g_list_prepend (retval, create_progressbar ());
  retval = g_list_prepend (retval, create_scales ());
  retval = g_list_prepend (retval, create_scrolledwindow ());
  retval = g_list_prepend (retval, create_scrollbar ());
  retval = g_list_prepend (retval, create_separator ());
  retval = g_list_prepend (retval, create_spinbutton ());
  retval = g_list_prepend (retval, create_statusbar ());
  retval = g_list_prepend (retval, create_text_view ());
  retval = g_list_prepend (retval, create_toggle_button ());
  retval = g_list_prepend (retval, create_tree_view ());
  retval = g_list_prepend (retval, create_window ());
  retval = g_list_prepend (retval, create_filesel ());
  retval = g_list_prepend (retval, create_assistant ());
  retval = g_list_prepend (retval, create_page_setup_dialog ());
  retval = g_list_prepend (retval, create_print_dialog ());
  retval = g_list_prepend (retval, create_volume_button ());
  retval = g_list_prepend (retval, create_switch ());
  retval = g_list_prepend (retval, create_appchooserbutton ());
  retval = g_list_prepend (retval, create_appchooserdialog ());
  retval = g_list_prepend (retval, create_lockbutton ());
  retval = g_list_prepend (retval, create_fontchooserdialog ());
  retval = g_list_prepend (retval, create_colorchooserdialog ());
  retval = g_list_prepend (retval, create_menu_button ());
  retval = g_list_prepend (retval, create_search_entry ());
  retval = g_list_prepend (retval, create_level_bar ());
  retval = g_list_prepend (retval, create_info_bar ());
  retval = g_list_prepend (retval, create_gl_area ());
  retval = g_list_prepend (retval, create_sidebar ());
  retval = g_list_prepend (retval, create_video ());
  retval = g_list_prepend (retval, create_media_controls ());
  retval = g_list_prepend (retval, create_picture ());
  retval = g_list_prepend (retval, create_password_entry ());
  retval = g_list_prepend (retval, create_editable_label ());
  retval = g_list_prepend (retval, create_drop_down ());
  retval = g_list_prepend (retval, create_window_controls ());
  retval = g_list_prepend (retval, create_calendar ());
  retval = g_list_prepend (retval, create_emojichooser ());
  retval = g_list_prepend (retval, create_expander ());
  retval = g_list_prepend (retval, create_menu_bar ());
  retval = g_list_prepend (retval, create_popover ());
  retval = g_list_prepend (retval, create_menu ());
  retval = g_list_prepend (retval, create_shortcuts_window ());
  retval = g_list_prepend (retval, create_drawing_area ());
  retval = g_list_prepend (retval, create_box ());
  retval = g_list_prepend (retval, create_center_box ());
  retval = g_list_prepend (retval, create_grid ());
  retval = g_list_prepend (retval, create_overlay ());

  return retval;
}
