#include <gdk/gdkkeysyms.h>
#include "widgets.h"

typedef enum
{
  SMALL,
  MEDIUM,
  LARGE
} WidgetSize;

static WidgetInfo *
new_widget_info (const char *name,
		 GtkWidget  *widget,
		 WidgetSize  size)
{
  WidgetInfo *info;

  info = g_new0 (WidgetInfo, 1);
  info->name = g_strdup (name);
  info->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  info->no_focus = TRUE;
  info->include_decorations = FALSE;

  gtk_widget_set_app_paintable (info->window, TRUE);
  g_signal_connect (info->window, "focus", G_CALLBACK (gtk_true), NULL);
  gtk_container_set_border_width (GTK_CONTAINER (info->window), 12);
  gtk_widget_show_all (widget);
  gtk_container_add (GTK_CONTAINER (info->window), widget);

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
  GtkWidget *align;

  widget = gtk_button_new_with_mnemonic ("_Button");
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return new_widget_info ("button", align, SMALL);
}

static WidgetInfo *
create_toggle_button (void)
{
  GtkWidget *widget;
  GtkWidget *align;

  widget = gtk_toggle_button_new_with_mnemonic ("_Toggle Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return new_widget_info ("toggle-button", align, SMALL);
}

static WidgetInfo *
create_check_button (void)
{
  GtkWidget *widget;
  GtkWidget *align;

  widget = gtk_check_button_new_with_mnemonic ("_Check Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return new_widget_info ("check-button", align, SMALL);
}

static WidgetInfo *
create_entry (void)
{
  GtkWidget *widget;
  GtkWidget *align;

  widget = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (widget), "Entry");
  gtk_editable_set_position (GTK_EDITABLE (widget), -1);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return  new_widget_info ("entry", align, SMALL);
}

static WidgetInfo *
create_radio (void)
{
  GtkWidget *widget;
  GtkWidget *radio;
  GtkWidget *align;

  widget = gtk_vbox_new (FALSE, 3);
  radio = gtk_radio_button_new_with_mnemonic (NULL, "Radio Button _One");
  gtk_box_pack_start (GTK_BOX (widget), radio, FALSE, FALSE, 0);
  radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), "Radio Button _Two");
  gtk_box_pack_start (GTK_BOX (widget), radio, FALSE, FALSE, 0);
  radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), "Radio Button T_hree");
  gtk_box_pack_start (GTK_BOX (widget), radio, FALSE, FALSE, 0);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return new_widget_info ("radio-group", align, MEDIUM);
}

static WidgetInfo *
create_label (void)
{
  GtkWidget *widget;
  GtkWidget *align;

  widget = gtk_label_new ("Label");
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return new_widget_info ("label", align, SMALL);
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
  gtk_widget_set_no_show_all (button, TRUE);

  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (widget), button);
  accel_group = gtk_accel_group_new();

  info = new_widget_info ("accel-label", box, SMALL);

  gtk_widget_add_accelerator (button, "activate", accel_group, GDK_Q, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);

  return info;
}

static WidgetInfo *
create_combo_box_entry (void)
{
  GtkWidget *widget;
  GtkWidget *align;
  
  widget = gtk_combo_box_entry_new_text ();
  gtk_entry_set_text (GTK_ENTRY (GTK_BIN (widget)->child), "Combo Box Entry");
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), widget);

  return new_widget_info ("combo-box-entry", align, SMALL);
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
  GtkListStore *list_store;
  GtkTreeIter iter;
  WidgetInfo *info;

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter, 0, "Line One", -1);
  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter, 0, "Line Two", -1);
  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter, 0, "Line Three", -1);

  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       0, "List and Tree",
					       gtk_cell_renderer_text_new (),
					       "text", 0, NULL);
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
  GtkWidget *align;
  GtkWidget *icon_view;
  GtkListStore *list_store;
  GtkTreeIter iter;
  GdkPixbuf *pixbuf;
  WidgetInfo *info;

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
  list_store = gtk_list_store_new (2, G_TYPE_STRING, GDK_TYPE_PIXBUF);
  gtk_list_store_append (list_store, &iter);
  pixbuf = gdk_pixbuf_new_from_file ("gnome-gmush.png", NULL);
  gtk_list_store_set (list_store, &iter, 0, "One", 1, pixbuf, -1);
  gtk_list_store_append (list_store, &iter);
  pixbuf = gdk_pixbuf_new_from_file ("gnome-foot.png", NULL);
  gtk_list_store_set (list_store, &iter, 0, "Two", 1, pixbuf, -1);

  icon_view = gtk_icon_view_new();

  gtk_icon_view_set_model (GTK_ICON_VIEW (icon_view), GTK_TREE_MODEL (list_store));
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (icon_view), 0);
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (icon_view), 1);

  gtk_container_add (GTK_CONTAINER (widget), icon_view);

  vbox = gtk_vbox_new (FALSE, 3);
  align = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_container_add (GTK_CONTAINER (align), widget);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_label_new ("Icon View"),
		      FALSE, FALSE, 0);

  info = new_widget_info ("icon-view", vbox, MEDIUM);
  info->no_focus = FALSE;

  return info;
}

static WidgetInfo *
create_color_button (void)
{
  GtkWidget *vbox;
  GtkWidget *picker;
  GtkWidget *align;
  GdkColor color;

  vbox = gtk_vbox_new (FALSE, 3);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  color.red = 0x1e<<8;  /* Go Gagne! */
  color.green = 0x90<<8;
  color.blue = 0xff<<8;
  picker = gtk_color_button_new_with_color (&color);
  gtk_container_add (GTK_CONTAINER (align), picker);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_label_new ("Color Button"),
		      FALSE, FALSE, 0);

  return new_widget_info ("color-button", vbox, SMALL);
}

static WidgetInfo *
create_font_button (void)
{
  GtkWidget *vbox;
  GtkWidget *picker;
  GtkWidget *align;

  vbox = gtk_vbox_new (FALSE, 3);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  picker = gtk_font_button_new_with_font ("Sans Serif 10");
  gtk_container_add (GTK_CONTAINER (align), picker);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_label_new ("Font Button"),
		      FALSE, FALSE, 0);

  return new_widget_info ("font-button", vbox, SMALL);
}

static WidgetInfo *
create_file_button (void)
{
  GtkWidget *vbox;
  GtkWidget *picker;
  GtkWidget *align;

  vbox = gtk_vbox_new (FALSE, 3);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  picker = gtk_file_chooser_button_new ("File Button");
  gtk_widget_set_size_request (picker, 150, -1);
  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (picker), "/etc/yum.conf");
  gtk_container_add (GTK_CONTAINER (align), picker);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_label_new ("File Button"),
		      FALSE, FALSE, 0);

  return new_widget_info ("file-button", vbox, SMALL);
}

static WidgetInfo *
create_separator (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;

  vbox = gtk_vbox_new (FALSE, 3);
  hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox),
		      gtk_hseparator_new (),
		      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox),
		      gtk_vseparator_new (),
		      TRUE, TRUE, 0);
  //  gtk_widget_set_size_request (hbox, 200, 150);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      g_object_new (GTK_TYPE_LABEL,
				    "label", "Horizontal and Vertical\nSeparators",
				    "justify", GTK_JUSTIFY_CENTER,
				    NULL),
		      FALSE, FALSE, 0);
  return new_widget_info ("separator", vbox, LARGE);
}

static WidgetInfo *
create_panes (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *pane;

  vbox = gtk_vbox_new (FALSE, 3);
  hbox = gtk_hbox_new (TRUE, 12);
  pane = gtk_hpaned_new ();
  gtk_paned_pack1 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_paned_pack2 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox),
		      pane,
		      TRUE, TRUE, 0);
  pane = gtk_vpaned_new ();
  gtk_paned_pack1 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_paned_pack2 (GTK_PANED (pane),
		   g_object_new (GTK_TYPE_FRAME,
				 "shadow", GTK_SHADOW_IN,
				 NULL),
		   FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox),
		      pane,
		      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      g_object_new (GTK_TYPE_LABEL,
				    "label", "Horizontal and Vertical\nPanes",
				    "justify", GTK_JUSTIFY_CENTER,
				    NULL),
		      FALSE, FALSE, 0);
  return new_widget_info ("panes", vbox, LARGE);
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

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
  info = new_widget_info ("window", widget, MEDIUM);
  info->include_decorations = TRUE;
  gtk_window_set_title (GTK_WINDOW (info->window), "Window");

  return info;
}

GList *
get_all_widgets (void)
{
  GList *retval = NULL;

  retval = g_list_prepend (retval, create_button ());
  retval = g_list_prepend (retval, create_toggle_button ());
  retval = g_list_prepend (retval, create_check_button ());
  retval = g_list_prepend (retval, create_entry ());
  retval = g_list_prepend (retval, create_radio ());
  retval = g_list_prepend (retval, create_label ());
  retval = g_list_prepend (retval, create_combo_box_entry ());
  retval = g_list_prepend (retval, create_text_view ());
  retval = g_list_prepend (retval, create_tree_view ());
  retval = g_list_prepend (retval, create_color_button ());
  retval = g_list_prepend (retval, create_font_button ());
  retval = g_list_prepend (retval, create_separator ());
  retval = g_list_prepend (retval, create_panes ());
  retval = g_list_prepend (retval, create_frame ());
  retval = g_list_prepend (retval, create_window ());
  retval = g_list_prepend (retval, create_accel_label ());
  retval = g_list_prepend (retval, create_file_button ());
  retval = g_list_prepend (retval, create_icon_view ());

  return retval;
}
