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

static gboolean
focus_handled (void)
{
  return TRUE;
}

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
  else
    {
      info->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      info->include_decorations = FALSE;
      gtk_widget_show (widget);
      gtk_container_add (GTK_CONTAINER (info->window), widget);
    }
  info->no_focus = TRUE;

  g_signal_connect (info->window, "focus", G_CALLBACK (focus_handled), NULL);

  switch (size)
    {
    case SMALL:
      gtk_widget_set_size_request (info->window,
				   240, 75);
      break;
    case MEDIUM:
      gtk_widget_set_size_request (info->window,
				   240, 165);
      break;
    case LARGE:
      gtk_widget_set_size_request (info->window,
				   240, 240);
      break;
    default:
	break;
    }

  return info;
}

static WidgetInfo *
create_button (void)
{
  GtkWidget *widget;

  widget = gtk_button_new_with_mnemonic ("_Button");
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

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
  gtk_container_add (GTK_CONTAINER (widget), sw);
  sw = gtk_switch_new ();
  gtk_container_add (GTK_CONTAINER (widget), sw);

  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("switch", widget, SMALL);
}

static WidgetInfo *
create_toggle_button (void)
{
  GtkWidget *widget;

  widget = gtk_toggle_button_new_with_mnemonic ("_Toggle Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("toggle-button", widget, SMALL);
}

static WidgetInfo *
create_check_button (void)
{
  GtkWidget *widget;

  widget = gtk_check_button_new_with_mnemonic ("_Check Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("check-button", widget, SMALL);
}

static WidgetInfo *
create_link_button (void)
{
  GtkWidget *widget;

  widget = gtk_link_button_new_with_label ("http://www.gtk.org", "Link Button");
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("link-button", widget, SMALL);
}

static WidgetInfo *
create_menu_button (void)
{
  GtkWidget *widget;
  GtkWidget *image;
  GtkWidget *menu;
  GtkWidget *vbox;

  widget = gtk_menu_button_new ();
  image = gtk_image_new ();
  gtk_image_set_from_icon_name (GTK_IMAGE (image), "emblem-system-symbolic");
  gtk_container_add (GTK_CONTAINER (widget), image);
  menu = gtk_menu_new ();
  gtk_menu_button_set_popup (GTK_MENU_BUTTON (widget), menu);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  gtk_container_add (GTK_CONTAINER (vbox), gtk_label_new ("Menu Button"));

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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Lock Button"));
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

  return  new_widget_info ("entry", widget, SMALL);
}

static WidgetInfo *
create_search_entry (void)
{
  GtkWidget *widget;

  widget = gtk_search_entry_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_entry_set_placeholder_text (GTK_ENTRY (widget), "Search...");

  return  new_widget_info ("search-entry", widget, SMALL);
}

static WidgetInfo *
create_radio (void)
{
  GtkWidget *widget;
  GtkWidget *radio;

  widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  radio = gtk_radio_button_new_with_mnemonic (NULL, "Radio Button _One");
  gtk_container_add (GTK_CONTAINER (widget), radio);
  radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), "Radio Button _Two");
  gtk_container_add (GTK_CONTAINER (widget), radio);
  radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), "Radio Button T_hree");
  gtk_container_add (GTK_CONTAINER (widget), radio);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

  return new_widget_info ("radio-group", widget, MEDIUM);
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
create_accel_label (void)
{
  WidgetInfo *info;
  GtkWidget *widget, *button, *box;
  GtkAccelGroup *accel_group;

  widget = gtk_accel_label_new ("Accel Label");

  button = gtk_button_new_with_label ("Quit");
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (widget), button);
  gtk_widget_hide (button);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (widget), button);
  accel_group = gtk_accel_group_new();

  info = new_widget_info ("accel-label", box, SMALL);

  gtk_widget_add_accelerator (button, "activate", accel_group, GDK_KEY_Q, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);

  return info;
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

  child = gtk_bin_get_child (GTK_BIN (widget));
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
create_info_bar (void)
{
  GtkWidget *widget;
  WidgetInfo *info;

  widget = gtk_info_bar_new ();
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (widget), TRUE);
  gtk_info_bar_set_message_type (GTK_INFO_BAR (widget), GTK_MESSAGE_INFO);
  gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (widget))),
                     gtk_label_new ("Info Bar"));

  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);

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
  gtk_container_add (GTK_CONTAINER (widget), entry);
  gtk_widget_show (entry);

  gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (widget), TRUE);
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (widget), TRUE);

  gtk_container_add (GTK_CONTAINER (box), widget);

  view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (box), view);

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
  gtk_container_add (GTK_CONTAINER (box), view);

  widget = gtk_action_bar_new ();

  button = gtk_button_new_from_icon_name ("object-select-symbolic");
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (widget), button);
  button = gtk_button_new_from_icon_name ("call-start-symbolic");
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (widget), button);
  g_object_set (gtk_widget_get_parent (button), "margin", 6, "spacing", 6, NULL);

  gtk_widget_show (widget);

  gtk_container_add (GTK_CONTAINER (box), widget);

  info = new_widget_info ("action-bar", box, SMALL);

  return info;
}

static WidgetInfo *
create_text_view (void)
{
  GtkWidget *widget;
  GtkWidget *text_view;

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
  text_view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (widget), text_view);
  /* Bad hack to add some size to the widget */
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view)),
			    "Multiline\nText\n\n", -1);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);

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
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
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
  gtk_container_add (GTK_CONTAINER (widget), tree_view);

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

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
  list_store = gtk_list_store_new (2, G_TYPE_STRING, GDK_TYPE_PIXBUF);
  gtk_list_store_append (list_store, &iter);
  pixbuf = gdk_pixbuf_new_from_file ("folder.png", NULL);
  gtk_list_store_set (list_store, &iter, 0, "One", 1, pixbuf, -1);
  gtk_list_store_append (list_store, &iter);
  pixbuf = gdk_pixbuf_new_from_file ("gnome.png", NULL);
  gtk_list_store_set (list_store, &iter, 0, "Two", 1, pixbuf, -1);

  icon_view = gtk_icon_view_new();

  gtk_icon_view_set_model (GTK_ICON_VIEW (icon_view), GTK_TREE_MODEL (list_store));
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (icon_view), 0);
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (icon_view), 1);

  gtk_container_add (GTK_CONTAINER (widget), icon_view);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Icon View"));

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
  color.red = 0x1e<<8;  /* Go Gagne! */
  color.green = 0x90<<8;
  color.blue = 0xff<<8;
  picker = gtk_color_button_new_with_rgba (&color);
  gtk_widget_set_halign (picker, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picker, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox), picker);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Color Button"));

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
  gtk_container_add (GTK_CONTAINER (vbox), picker);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Font Button"));

  return new_widget_info ("font-button", vbox, SMALL);
}

static WidgetInfo *
create_file_button (void)
{
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *picker;
  char *path;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  picker = gtk_file_chooser_button_new ("File Chooser Button",
		  			GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_widget_set_size_request (picker, 150, -1);
  gtk_widget_set_halign (picker, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picker, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox2), picker);
  gtk_container_add (GTK_CONTAINER (vbox2),
		      gtk_label_new ("File Button (Files)"));

  gtk_container_add (GTK_CONTAINER (vbox),
		      vbox2);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  picker = gtk_file_chooser_button_new ("File Chooser Button",
		  			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_widget_set_size_request (picker, 150, -1);
  path = g_build_filename (g_get_home_dir (), "Documents", NULL);
  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (picker), path);
  g_free (path);
  gtk_widget_set_halign (picker, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picker, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox2), picker);
  gtk_container_add (GTK_CONTAINER (vbox2),
		      gtk_label_new ("File Button (Select Folder)"));
  gtk_container_add (GTK_CONTAINER (vbox),
		      vbox2);

  return new_widget_info ("file-button", vbox, MEDIUM);
}

static WidgetInfo *
create_separator (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_container_add (GTK_CONTAINER (hbox),
		      gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (hbox),
		      gtk_separator_new (GTK_ORIENTATION_VERTICAL));
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_container_add (GTK_CONTAINER (vbox),
		      g_object_new (GTK_TYPE_LABEL,
				    "label", "Horizontal and Vertical\nSeparators",
				    "justify", GTK_JUSTIFY_CENTER,
				    NULL));
  return new_widget_info ("separator", vbox, MEDIUM);
}

static WidgetInfo *
create_panes (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *pane;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_pack1 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow-type", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_paned_pack2 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow-type", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_container_add (GTK_CONTAINER (hbox),
		      pane);
  pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_paned_pack1 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow-type", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_paned_pack2 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow-type", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_container_add (GTK_CONTAINER (hbox),
		      pane);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_container_add (GTK_CONTAINER (vbox),
		      g_object_new (GTK_TYPE_LABEL,
				    "label", "Horizontal and Vertical\nPanes",
				    "justify", GTK_JUSTIFY_CENTER,
				    NULL));
  return new_widget_info ("panes", vbox, MEDIUM);
}

static WidgetInfo *
create_frame (void)
{
  GtkWidget *widget;

  widget = gtk_frame_new ("Frame");

  return new_widget_info ("frame", widget, MEDIUM);
}

static WidgetInfo *
create_window (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
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
create_toolbar (void)
{
  GtkWidget *widget;
  GtkToolItem *item;

  widget = gtk_toolbar_new ();

  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "document-new");
  gtk_toolbar_insert (GTK_TOOLBAR (widget), item, -1);

  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "document-open");
  gtk_toolbar_insert (GTK_TOOLBAR (widget), item, -1);

  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "view-refresh");
  gtk_toolbar_insert (GTK_TOOLBAR (widget), item, -1);

  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (widget), FALSE);

  return new_widget_info ("toolbar", widget, SMALL);
}

static WidgetInfo *
create_menubar (void)
{
  GtkWidget *widget, *vbox, *item;

  widget = gtk_menu_bar_new ();

  item = gtk_menu_item_new_with_mnemonic ("_File");
  gtk_menu_shell_append (GTK_MENU_SHELL (widget), item);

  item = gtk_menu_item_new_with_mnemonic ("_Edit");
  gtk_menu_shell_append (GTK_MENU_SHELL (widget), item);

  item = gtk_menu_item_new_with_mnemonic ("_Help");
  gtk_menu_shell_append (GTK_MENU_SHELL (widget), item);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Menu Bar"));

  return new_widget_info ("menubar", vbox, SMALL);
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
create_about_dialog (void)
{
  GtkWidget *widget;
  const gchar *authors[] = {
    "Peter Mattis",
    "Spencer Kimball",
    "Josh MacDonald",
    "and many more...",
    NULL
  };

  widget = gtk_about_dialog_new ();
  g_object_set (widget,
                "program-name", "GTK Code Demos",
                "version", PACKAGE_VERSION,
                "copyright", "© 1997-2013 The GTK Team",
                "website", "http://www.gtk.org",
                "comments", "Program to demonstrate GTK functions.",
                "logo-icon-name", "help-about",
                "title", "About GTK Code Demos",
                "authors", authors,
		NULL);
  gtk_window_set_icon_name (GTK_WINDOW (widget), "help-about");
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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Progress Bar"));

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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Level Bar"));

  return new_widget_info ("levelbar", vbox, SMALL);
}

static WidgetInfo *
create_scrolledwindow (void)
{
  GtkWidget *scrolledwin, *label;

  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  label = gtk_label_new ("Scrolled Window");

  gtk_container_add (GTK_CONTAINER (scrolledwin), label);

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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Scrollbar"));

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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Spin Button"));

  return new_widget_info ("spinbutton", vbox, SMALL);
}

static WidgetInfo *
create_statusbar (void)
{
  WidgetInfo *info;
  GtkWidget *widget;
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (vbox),
                      gtk_label_new ("Status Bar"));
  widget = gtk_statusbar_new ();
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_statusbar_push (GTK_STATUSBAR (widget), 0, "Hold on...");

  gtk_container_add (GTK_CONTAINER (vbox), widget);

  info = new_widget_info ("statusbar", vbox, SMALL);

  return info;
}

static WidgetInfo *
create_scales (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_container_add (GTK_CONTAINER (hbox),
		      gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                0.0, 100.0, 1.0));
  gtk_container_add (GTK_CONTAINER (hbox),
		      gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                                0.0, 100.0, 1.0));
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_container_add (GTK_CONTAINER (vbox),
		      g_object_new (GTK_TYPE_LABEL,
				    "label", "Horizontal and Vertical\nScales",
				    "justify", GTK_JUSTIFY_CENTER,
				    NULL));
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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Image"));

  return new_widget_info ("image", vbox, SMALL);
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
  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox),
		      gtk_label_new ("Spinner"));

  return new_widget_info ("spinner", vbox, SMALL);
}

static WidgetInfo *
create_volume_button (void)
{
  GtkWidget *button, *box;
  GtkWidget *widget;
  GtkWidget *popup;

  widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (widget, 100, 250);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (widget), box);

  button = gtk_volume_button_new ();
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_scale_button_set_value (GTK_SCALE_BUTTON (button), 33);
  popup = gtk_scale_button_get_popup (GTK_SCALE_BUTTON (button));
  gtk_widget_realize (widget);
  gtk_widget_show (box);
  gtk_widget_show (popup);

  return new_widget_info ("volumebutton", widget, ASIS);
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
  gtk_widget_show (page1);
  gtk_widget_set_size_request (page1, 300, 140);
  gtk_assistant_prepend_page (GTK_ASSISTANT (widget), page1);
  gtk_assistant_set_page_title (GTK_ASSISTANT (widget), page1, "Assistant page");
  gtk_assistant_set_page_complete (GTK_ASSISTANT (widget), page1, TRUE);

  page2 = gtk_label_new (NULL);
  gtk_widget_show (page2);
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
  gtk_container_add (GTK_CONTAINER (vbox), picker);
  gtk_container_add (GTK_CONTAINER (vbox),
                      gtk_label_new ("Application Button"));

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

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  view = gtk_text_view_new ();
  gtk_widget_show (view);
  gtk_widget_set_size_request (window, 220, 150);
  gtk_container_add (GTK_CONTAINER (window), view);
  bar = gtk_header_bar_new ();
  gtk_header_bar_set_title (GTK_HEADER_BAR (bar), "Header Bar");
  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (bar), "(subtitle)");
  gtk_window_set_titlebar (GTK_WINDOW (window), bar);
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), gtk_image_new_from_icon_name ("bookmark-new-symbolic"));
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
  gtk_widget_show (view);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page1", "Page 1");
  view = gtk_text_view_new ();
  gtk_widget_show (view);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page2", "Page 2");

  switcher = gtk_stack_switcher_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_widget_set_halign (switcher, GTK_ALIGN_CENTER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (vbox), switcher);
  gtk_container_add (GTK_CONTAINER (vbox), stack);
  gtk_container_add (GTK_CONTAINER (vbox),
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
  gtk_widget_show (view);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page1", "Page 1");
  view = gtk_text_view_new ();
  gtk_widget_show (view);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page2", "Page 2");

  switcher = gtk_stack_switcher_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_widget_set_halign (switcher, GTK_ALIGN_CENTER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (vbox), switcher);
  gtk_container_add (GTK_CONTAINER (vbox), stack);
  gtk_container_add (GTK_CONTAINER (vbox),
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
  gtk_style_context_add_class (gtk_widget_get_style_context (view), "view");
  gtk_widget_set_halign (view, GTK_ALIGN_FILL);
  gtk_widget_set_valign (view, GTK_ALIGN_FILL);
  gtk_widget_show (view);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page1", "Page 1");
  view = gtk_text_view_new ();
  gtk_widget_show (view);
  gtk_stack_add_titled (GTK_STACK (stack), view, "page2", "Page 2");

  sidebar = gtk_stack_sidebar_new ();
  gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (sidebar), GTK_STACK (stack));

  frame = gtk_frame_new (NULL);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_container_add (GTK_CONTAINER (hbox), sidebar);
  gtk_container_add (GTK_CONTAINER (hbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));
  gtk_container_add (GTK_CONTAINER (hbox), stack);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  return new_widget_info ("sidebar", frame, ASIS);
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
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_BROWSE);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  button = gtk_label_new ("List Box");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (row), button);
  gtk_container_add (GTK_CONTAINER (list), row);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (row), gtk_label_new ("Line One"));
  button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_container_add (GTK_CONTAINER (row), button);
  gtk_container_add (GTK_CONTAINER (list), row);
  gtk_list_box_select_row (GTK_LIST_BOX (list), GTK_LIST_BOX_ROW (gtk_widget_get_parent (row)));
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (row), gtk_label_new ("Line Two"));
  button = gtk_button_new_with_label ("2");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_container_add (GTK_CONTAINER (row), button);
  gtk_container_add (GTK_CONTAINER (list), row);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (row), gtk_label_new ("Line Three"));
  button = gtk_entry_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_container_add (GTK_CONTAINER (row), button);
  gtk_container_add (GTK_CONTAINER (list), row);

  gtk_container_add (GTK_CONTAINER (widget), list);

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
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);

  box = gtk_flow_box_new ();
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 2);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (box), 2);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_BROWSE);
  button = gtk_label_new ("Child One");
  gtk_container_add (GTK_CONTAINER (box), button);
  button = gtk_button_new_with_label ("Child Two");
  gtk_container_add (GTK_CONTAINER (box), button);
  child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (child), gtk_label_new ("Child Three"));
  button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_container_add (GTK_CONTAINER (child), button);
  gtk_container_add (GTK_CONTAINER (box), child);
  gtk_flow_box_select_child (GTK_FLOW_BOX (box),
                             GTK_FLOW_BOX_CHILD (gtk_widget_get_parent (child)));

  gtk_container_add (GTK_CONTAINER (widget), box);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (vbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox), gtk_label_new ("Flow Box"));
  info = new_widget_info ("flow-box", vbox, ASIS);
  info->no_focus = FALSE;

  return info;
}

static WidgetInfo *
create_gl_area (void)
{
  WidgetInfo *info;
  GtkWidget *widget;
  GtkWidget *gears;

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);

  gears = gtk_gears_new ();
  gtk_container_add (GTK_CONTAINER (widget), gears);
 
  info = new_widget_info ("glarea", widget, MEDIUM);

  return info;
}

GList *
get_all_widgets (void)
{
  GList *retval = NULL;

  retval = g_list_prepend (retval, create_search_bar ());
  retval = g_list_prepend (retval, create_action_bar ());
  retval = g_list_prepend (retval, create_list_box());
  retval = g_list_prepend (retval, create_flow_box());
  retval = g_list_prepend (retval, create_headerbar ());
  retval = g_list_prepend (retval, create_stack ());
  retval = g_list_prepend (retval, create_stack_switcher ());
  retval = g_list_prepend (retval, create_spinner ());
  retval = g_list_prepend (retval, create_about_dialog ());
  retval = g_list_prepend (retval, create_accel_label ());
  retval = g_list_prepend (retval, create_button ());
  retval = g_list_prepend (retval, create_check_button ());
  retval = g_list_prepend (retval, create_color_button ());
  retval = g_list_prepend (retval, create_combo_box ());
  retval = g_list_prepend (retval, create_combo_box_entry ());
  retval = g_list_prepend (retval, create_combo_box_text ());
  retval = g_list_prepend (retval, create_entry ());
  retval = g_list_prepend (retval, create_file_button ());
  retval = g_list_prepend (retval, create_font_button ());
  retval = g_list_prepend (retval, create_frame ());
  retval = g_list_prepend (retval, create_icon_view ());
  retval = g_list_prepend (retval, create_image ());
  retval = g_list_prepend (retval, create_label ());
  retval = g_list_prepend (retval, create_link_button ());
  retval = g_list_prepend (retval, create_menubar ());
  retval = g_list_prepend (retval, create_message_dialog ());
  retval = g_list_prepend (retval, create_notebook ());
  retval = g_list_prepend (retval, create_panes ());
  retval = g_list_prepend (retval, create_progressbar ());
  retval = g_list_prepend (retval, create_radio ());
  retval = g_list_prepend (retval, create_scales ());
  retval = g_list_prepend (retval, create_scrolledwindow ());
  retval = g_list_prepend (retval, create_scrollbar ());
  retval = g_list_prepend (retval, create_separator ());
  retval = g_list_prepend (retval, create_spinbutton ());
  retval = g_list_prepend (retval, create_statusbar ());
  retval = g_list_prepend (retval, create_text_view ());
  retval = g_list_prepend (retval, create_toggle_button ());
  retval = g_list_prepend (retval, create_toolbar ());
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

  return retval;
}
