#include <gtk/gtk.h>
#include <stdio.h>
#include "prop-editor.h"

static gboolean
delete_event_cb (GtkWidget *editor,
                 gint       response,
                 gpointer   user_data)
{
  gtk_widget_hide (editor);

  return TRUE;
}

static void
properties_cb (GtkWidget *button,
               GObject   *entry)
{
  GtkWidget *editor;

  editor = g_object_get_data (entry, "properties-dialog");

  if (editor == NULL)
    {
      editor = create_prop_editor (G_OBJECT (entry), G_TYPE_INVALID);
      gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
      gtk_window_set_transient_for (GTK_WINDOW (editor),
                                    GTK_WINDOW (gtk_widget_get_toplevel (button)));
      g_signal_connect (editor, "delete-event", G_CALLBACK (delete_event_cb), NULL);
      g_object_set_data (entry, "properties-dialog", editor);
    }

  gtk_window_present (GTK_WINDOW (editor));
}

static void
clear_pressed (GtkEntry *entry, gint icon, GdkEvent *event, gpointer data)
{
   if (icon == GTK_ENTRY_ICON_SECONDARY)
     gtk_entry_set_text (entry, "");
}

static void
drag_begin_cb (GtkWidget      *widget,
               GdkDragContext *context,
               gpointer        user_data)
{
  gint pos;

  pos = gtk_entry_get_current_icon_drag_source (GTK_ENTRY (widget));
  if (pos != -1)
    gtk_drag_set_icon_stock (context, GTK_STOCK_INFO, 2, 2);

  g_print ("drag begin %d\n", pos);
}

static void
drag_data_get_cb (GtkWidget        *widget,
                  GdkDragContext   *context,
                  GtkSelectionData *data,
                  guint             info,
                  guint             time,
                  gpointer          user_data)
{
  gint pos;

  pos = gtk_entry_get_current_icon_drag_source (GTK_ENTRY (widget));

  if (pos == GTK_ENTRY_ICON_PRIMARY)
    {
#if 0
      gint start, end;
      
      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
        {
          gchar *str;
          
          str = gtk_editable_get_chars (GTK_EDITABLE (widget), start, end);
          gtk_selection_data_set_text (data, str, -1);
          g_free (str);
        }
#else
      gtk_selection_data_set_text (data, "XXX", -1);
#endif
    }
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GIcon *icon;
  GtkTargetList *tlist;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Gtk Entry Icons Test");
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);

  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);

  table = gtk_table_new (2, 4, FALSE);
  gtk_container_add (GTK_CONTAINER (window), table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);

  /*
   * Open File - Sets the icon using a GIcon
   */
  label = gtk_label_new ("Open File:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  icon = g_themed_icon_new ("folder");
  g_themed_icon_append_name (G_THEMED_ICON (icon), "gtk-directory");

  gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 icon);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry),
			        GTK_ENTRY_ICON_PRIMARY,
				FALSE);

  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				   GTK_ENTRY_ICON_PRIMARY,
				   "Open a file");

  button = gtk_button_new_with_label ("Properties");
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);
  g_signal_connect (button, "clicked", 
                    G_CALLBACK (properties_cb), entry);                    

  
  /*
   * Save File - sets the icon using a stock id.
   */
  label = gtk_label_new ("Save File:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_entry_set_text (GTK_ENTRY (entry), "‏Right-to-left");
  gtk_widget_set_direction (entry, GTK_TEXT_DIR_RTL);
  
  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 GTK_STOCK_SAVE);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				   GTK_ENTRY_ICON_PRIMARY,
				   "Save a file");
  tlist = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (tlist, 0);
  gtk_entry_set_icon_drag_source (GTK_ENTRY (entry),
                                  GTK_ENTRY_ICON_PRIMARY,
                                  tlist, GDK_ACTION_COPY); 
  g_signal_connect_after (entry, "drag-begin", 
                          G_CALLBACK (drag_begin_cb), NULL);
  g_signal_connect (entry, "drag-data-get", 
                    G_CALLBACK (drag_data_get_cb), NULL);
  gtk_target_list_unref (tlist);

  button = gtk_button_new_with_label ("Properties");
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 1, 2,
		    GTK_FILL, GTK_FILL, 0, 0);
  g_signal_connect (button, "clicked", 
                    G_CALLBACK (properties_cb), entry);                    

  /*
   * Search - Uses a helper function
   */
  label = gtk_label_new ("Search:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 2, 3,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 GTK_STOCK_FIND);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_SECONDARY,
				 GTK_STOCK_CLEAR);

  g_signal_connect (entry, "icon-press", G_CALLBACK (clear_pressed), NULL);

  button = gtk_button_new_with_label ("Properties");
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  g_signal_connect (button, "clicked", 
                    G_CALLBACK (properties_cb), entry);                    

  /*
   * Password - Sets the icon using a stock id
   */
  label = gtk_label_new ("Password:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 3, 4,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 GTK_STOCK_DIALOG_AUTHENTICATION);

  gtk_entry_set_icon_activatable (GTK_ENTRY (entry),
				  GTK_ENTRY_ICON_PRIMARY,
				  FALSE);

  button = gtk_button_new_with_label ("Properties");
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);
  g_signal_connect (button, "clicked", 
                    G_CALLBACK (properties_cb), entry);                    

  /* Name - Does not set any icons. */
  label = gtk_label_new ("Name:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 4, 5,
		    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  button = gtk_button_new_with_label ("Properties");
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 4, 5,
		    GTK_FILL, GTK_FILL, 0, 0);
  g_signal_connect (button, "clicked", 
                    G_CALLBACK (properties_cb), entry);                    

  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
