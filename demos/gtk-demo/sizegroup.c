/* Size Groups
 *
 * GtkSizeGroup provides a mechanism for grouping a number of
 * widgets together so they all request the same amount of space.
 * This is typically useful when you want a column of widgets to 
 * have the same size, but you can't use a GtkTable widget.
 * 
 * Note that size groups only affect the amount of space requested,
 * not the size that the widgets finally receive. If you want the
 * widgets in a GtkSizeGroup to actually be the same size, you need
 * to pack them in such a way that they get the size they request
 * and not more. For example, if you are packing your widgets
 * into a table, you would not include the GTK_FILL flag.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

/* Convenience function to create an option menu holding a number of strings
 */
GtkWidget *
create_option_menu (const char **strings)
{
  GtkWidget *menu;
  GtkWidget *option_menu;
  const char **str;

  menu = gtk_menu_new ();
  
  for (str = strings; *str; str++)
    {
      GtkWidget *menu_item = gtk_menu_item_new_with_label (*str);
      gtk_widget_show (menu_item);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  option_menu = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

  return option_menu;
}

static void
add_row (GtkTable     *table,
	 int           row,
	 GtkSizeGroup *size_group,
	 const char   *label_text,
	 const char  **options)
{
  GtkWidget *option_menu;
  GtkWidget *label;

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 1);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1,                  row, row + 1,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);
  
  option_menu = create_option_menu (options);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), option_menu);
  gtk_size_group_add_widget (size_group, option_menu);
  gtk_table_attach (GTK_TABLE (table), option_menu,
		    1, 2,                  row, row + 1,
		    0,                     0,
		    0,                     0);
}

static void
toggle_grouping (GtkToggleButton *check_button,
		 GtkSizeGroup    *size_group)
{
  GtkSizeGroupMode new_mode;

  /* GTK_SIZE_GROUP_NONE is not generally useful, but is useful
   * here to show the effect of GTK_SIZE_GROUP_HORIZONTAL by
   * contrast.
   */
  if (gtk_toggle_button_get_active (check_button))
    new_mode = GTK_SIZE_GROUP_HORIZONTAL;
  else
    new_mode = GTK_SIZE_GROUP_NONE;
  
  gtk_size_group_set_mode (size_group, new_mode);
}

GtkWidget *
do_sizegroup (void)
{
  GtkWidget *table;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *check_button;
  GtkSizeGroup *size_group;

  static const char *color_options[] = {
    "Red", "Green", "Blue", NULL
  };
  
  static const char *dash_options[] = {
    "Solid", "Dashed", "Dotted", NULL
  };
  
  static const char *end_options[] = {
    "Square", "Round", "Arrow", NULL
  };
  
  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("GtkSizeGroup",
					    NULL, 0,
					    GTK_STOCK_CLOSE,
					    GTK_RESPONSE_NONE,
					    NULL);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      
      g_signal_connect (window, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      
      /* Create one frame holding color options
       */
      frame = gtk_frame_new ("Color Options");
      gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

      table = gtk_table_new (2, 2, FALSE);
      gtk_container_set_border_width (GTK_CONTAINER (table), 5);
      gtk_table_set_row_spacings (GTK_TABLE (table), 5);
      gtk_table_set_col_spacings (GTK_TABLE (table), 10);
      gtk_container_add (GTK_CONTAINER (frame), table);

      add_row (GTK_TABLE (table), 0, size_group, "_Foreground", color_options);
      add_row (GTK_TABLE (table), 1, size_group, "_Background", color_options);

      /* And another frame holding line style options
       */
      frame = gtk_frame_new ("Line Options");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      table = gtk_table_new (2, 2, FALSE);
      gtk_container_set_border_width (GTK_CONTAINER (table), 5);
      gtk_table_set_row_spacings (GTK_TABLE (table), 5);
      gtk_table_set_col_spacings (GTK_TABLE (table), 10);
      gtk_container_add (GTK_CONTAINER (frame), table);

      add_row (GTK_TABLE (table), 0, size_group, "_Dashing", dash_options);
      add_row (GTK_TABLE (table), 1, size_group, "_Line ends", end_options);

      /* And a check button to turn grouping on and off */
      check_button = gtk_check_button_new_with_mnemonic ("_Enable grouping");
      gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, FALSE, 0);
      
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);
      g_signal_connect (check_button, "toggled",
			G_CALLBACK (toggle_grouping), size_group);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
