#include "widgets.h"
      

static WidgetInfo *
new_widget_info (const char *name,
		 GtkWidget  *widget)
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
    
  return info;
}

static WidgetInfo *
create_button (void)
{
  GtkWidget *widget;
  
  widget = gtk_button_new_with_mnemonic ("_Button");

  return new_widget_info ("button", widget);
}

static WidgetInfo *
create_toggle_button (void)
{
  GtkWidget *widget;

  widget = gtk_toggle_button_new_with_mnemonic ("_Toggle Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  return new_widget_info ("toggle-button", widget);
}

static WidgetInfo *
create_check_button (void)
{
  GtkWidget *widget;
  
  widget = gtk_check_button_new_with_mnemonic ("_Check Button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

  return new_widget_info ("check-button", widget);
}

static WidgetInfo *
create_entry (void)
{
  GtkWidget *widget;
  
  widget = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (widget), "Entry");
  gtk_editable_set_position (GTK_EDITABLE (widget), -1);
  
  return  new_widget_info ("entry", widget);
}

static WidgetInfo *
create_radio (void)
{
  GtkWidget *widget;
  GtkWidget *radio;
  
  widget = gtk_vbox_new (FALSE, 3);
  radio = gtk_radio_button_new_with_mnemonic (NULL, "Radio Button Item _One");
  gtk_box_pack_start (GTK_BOX (widget), radio, FALSE, FALSE, 0);
  radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), "Radio Button Item _Two");
  gtk_box_pack_start (GTK_BOX (widget), radio, FALSE, FALSE, 0);
  radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), "Radio Button Item T_hree");
  gtk_box_pack_start (GTK_BOX (widget), radio, FALSE, FALSE, 0);

  return new_widget_info ("radio-group", widget);
}

static WidgetInfo *
create_label (void)
{
  GtkWidget *widget;

  widget = gtk_label_new ("Label");

  return new_widget_info ("label", widget);
}

static WidgetInfo *
create_combo_box_entry (void)
{
  GtkWidget *widget;

  widget = gtk_combo_box_entry_new_text ();
  gtk_entry_set_text (GTK_ENTRY (GTK_BIN (widget)->child), "Combo Box Entry");


  return new_widget_info ("combo-box-entry", widget);
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
			    "Multiline             \nText\n\n", -1);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
  
  return new_widget_info ("multiline-text", widget);
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
  
  info = new_widget_info ("list-and-tree", widget);
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

  return new_widget_info ("color-button", vbox);
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

  return new_widget_info ("font-button", vbox);
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
  gtk_widget_set_size_request (hbox, 200, 150);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_label_new ("Horizontal and Vertical Separators"),
		      FALSE, FALSE, 0);
  return new_widget_info ("separator", vbox);
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
  gtk_widget_set_size_request (hbox, 200, 150);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_label_new ("Horizontal and Vertical Panes"),
		      FALSE, FALSE, 0);
  return new_widget_info ("panes", vbox);
}

static WidgetInfo *
create_frame (void)
{
  GtkWidget *widget;

  widget = gtk_frame_new ("Frame");
  gtk_widget_set_size_request (widget, 150, 150);

  return new_widget_info ("frame", widget);
}

static WidgetInfo *
create_window (void)
{
  WidgetInfo *info;
  GtkWidget *widget;

  widget = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
  gtk_widget_set_size_request (widget, 150, 150);
  info = new_widget_info ("window", widget);
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
  
  return retval;
}
