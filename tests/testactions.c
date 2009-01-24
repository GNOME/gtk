/* testactions.c
 * Copyright (C) 2003  Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gtk/gtk.h>

static GtkActionGroup *action_group = NULL;
static GtkToolbar *toolbar = NULL;

static void
activate_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  g_message ("Action %s (type=%s) activated", name, typename);
}

static void
toggle_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  g_message ("Action %s (type=%s) activated (active=%d)", name, typename,
	     gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}


static void
radio_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  g_message ("Action %s (type=%s) activated (active=%d) (value %d)", name, typename,
	     gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)),
	     gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action)));
}

static void
recent_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);
  gchar *uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (action));

  g_message ("Action %s (type=%s) activated (uri=%s)",
             name, typename,
             uri ? uri : "no item selected");
  g_free (uri);
}

static void
toggle_cnp_actions (GtkAction *action)
{
  gboolean sensitive;

  sensitive = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
  action = gtk_action_group_get_action (action_group, "cut");
  g_object_set (action, "sensitive", sensitive, NULL);
  action = gtk_action_group_get_action (action_group, "copy");
  g_object_set (action, "sensitive", sensitive, NULL);
  action = gtk_action_group_get_action (action_group, "paste");
  g_object_set (action, "sensitive", sensitive, NULL);

  action = gtk_action_group_get_action (action_group, "toggle-cnp");
  if (sensitive)
    g_object_set (action, "label", "Disable Cut and paste ops", NULL);
  else
    g_object_set (action, "label", "Enable Cut and paste ops", NULL);
}

static void
show_accel_dialog (GtkAction *action)
{
  g_message ("Sorry, accel dialog not available");
}

static void
toolbar_style (GtkAction *action)
{
  GtkToolbarStyle style;

  g_return_if_fail (toolbar != NULL);

  radio_action (action);

  style = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

  gtk_toolbar_set_style (toolbar, style);
}

static void
toolbar_size_small (GtkAction *action)
{
  g_return_if_fail (toolbar != NULL);

  gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
}

static void
toolbar_size_large (GtkAction *action)
{
  g_return_if_fail (toolbar != NULL);

  gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* convenience functions for declaring actions */
static GtkActionEntry entries[] = {
  { "Menu1Action", NULL, "Menu _1" },
  { "Menu2Action", NULL, "Menu _2" },
  { "Menu3Action", NULL, "_Dynamic Menu" },

  { "attach", "mail-attachment", "_Attachment...", "<Control>m",
    "Attach a file", G_CALLBACK (activate_action) },
  { "cut", GTK_STOCK_CUT, "C_ut", "<control>X",
    "Cut the selected text to the clipboard", G_CALLBACK (activate_action) },
  { "copy", GTK_STOCK_COPY, "_Copy", "<control>C",
    "Copy the selected text to the clipboard", G_CALLBACK (activate_action) },
  { "paste", GTK_STOCK_PASTE, "_Paste", "<control>V",
    "Paste the text from the clipboard", G_CALLBACK (activate_action) },
  { "quit", GTK_STOCK_QUIT,  NULL, "<control>Q",
    "Quit the application", G_CALLBACK (gtk_main_quit) },
  { "customise-accels", NULL, "Customise _Accels", NULL,
    "Customise keyboard shortcuts", G_CALLBACK (show_accel_dialog) },
  { "toolbar-small-icons", NULL, "Small Icons", NULL, 
    NULL, G_CALLBACK (toolbar_size_small) },
  { "toolbar-large-icons", NULL, "Large Icons", NULL,
    NULL, G_CALLBACK (toolbar_size_large) }
};
static guint n_entries = G_N_ELEMENTS (entries);

static GtkToggleActionEntry toggle_entries[] = {
  { "bold", GTK_STOCK_BOLD, "_Bold", "<control>B",
    "Change to bold face", 
    G_CALLBACK (toggle_action), FALSE },
  { "toggle-cnp", NULL, "Enable Cut/Copy/Paste", NULL,
    "Change the sensitivity of the cut, copy and paste actions",
    G_CALLBACK (toggle_cnp_actions), TRUE },
};
static guint n_toggle_entries = G_N_ELEMENTS (toggle_entries);

enum {
  JUSTIFY_LEFT,
  JUSTIFY_CENTER,
  JUSTIFY_RIGHT,
  JUSTIFY_FILL
};

static GtkRadioActionEntry justify_entries[] = {
  { "justify-left", GTK_STOCK_JUSTIFY_LEFT, "_Left", "<control>L",
    "Left justify the text", JUSTIFY_LEFT },
  { "justify-center", GTK_STOCK_JUSTIFY_CENTER, "C_enter", "<control>E",
    "Center justify the text", JUSTIFY_CENTER },
  { "justify-right", GTK_STOCK_JUSTIFY_RIGHT, "_Right", "<control>R",
    "Right justify the text", JUSTIFY_RIGHT },
  { "justify-fill", GTK_STOCK_JUSTIFY_FILL, "_Fill", "<control>J",
    "Fill justify the text", JUSTIFY_FILL }
};
static guint n_justify_entries = G_N_ELEMENTS (justify_entries);

static GtkRadioActionEntry toolbar_entries[] = {
  { "toolbar-icons", NULL, "Icons", NULL, NULL, GTK_TOOLBAR_ICONS },
  { "toolbar-text", NULL, "Text", NULL, NULL, GTK_TOOLBAR_TEXT },
  { "toolbar-both", NULL, "Both", NULL, NULL, GTK_TOOLBAR_BOTH },
  { "toolbar-both-horiz", NULL, "Both Horizontal", NULL, NULL, GTK_TOOLBAR_BOTH_HORIZ }
};
static guint n_toolbar_entries = G_N_ELEMENTS (toolbar_entries);

/* XML description of the menus for the test app.  The parser understands
 * a subset of the Bonobo UI XML format, and uses GMarkup for parsing */
static const gchar *ui_info =
"  <menubar>\n"
"    <menu name=\"Menu _1\" action=\"Menu1Action\">\n"
"      <menuitem name=\"cut\" action=\"cut\" />\n"
"      <menuitem name=\"copy\" action=\"copy\" />\n"
"      <menuitem name=\"paste\" action=\"paste\" />\n"
"      <separator name=\"sep1\" />\n"
"      <menuitem name=\"bold1\" action=\"bold\" />\n"
"      <menuitem name=\"bold2\" action=\"bold\" />\n"
"      <separator name=\"sep2\" />\n"
"      <menuitem name=\"recent\" action=\"recent\" />\n"
"      <separator name=\"sep3\" />\n"
"      <menuitem name=\"toggle-cnp\" action=\"toggle-cnp\" />\n"
"      <separator name=\"sep4\" />\n"
"      <menuitem name=\"quit\" action=\"quit\" />\n"
"    </menu>\n"
"    <menu name=\"Menu _2\" action=\"Menu2Action\">\n"
"      <menuitem name=\"cut\" action=\"cut\" />\n"
"      <menuitem name=\"copy\" action=\"copy\" />\n"
"      <menuitem name=\"paste\" action=\"paste\" />\n"
"      <separator name=\"sep5\"/>\n"
"      <menuitem name=\"bold\" action=\"bold\" />\n"
"      <separator name=\"sep6\"/>\n"
"      <menuitem name=\"justify-left\" action=\"justify-left\" />\n"
"      <menuitem name=\"justify-center\" action=\"justify-center\" />\n"
"      <menuitem name=\"justify-right\" action=\"justify-right\" />\n"
"      <menuitem name=\"justify-fill\" action=\"justify-fill\" />\n"
"      <separator name=\"sep7\"/>\n"
"      <menuitem  name=\"customise-accels\" action=\"customise-accels\" />\n"
"      <separator name=\"sep8\"/>\n"
"      <menuitem action=\"toolbar-icons\" />\n"
"      <menuitem action=\"toolbar-text\" />\n"
"      <menuitem action=\"toolbar-both\" />\n"
"      <menuitem action=\"toolbar-both-horiz\" />\n"
"      <separator name=\"sep9\"/>\n"
"      <menuitem action=\"toolbar-small-icons\" />\n"
"      <menuitem action=\"toolbar-large-icons\" />\n"
"    </menu>\n"
"    <menu name=\"DynamicMenu\" action=\"Menu3Action\" />\n"
"  </menubar>\n"
"  <toolbar name=\"toolbar\">\n"
"    <toolitem name=\"attach\" action=\"attach\" />\n"
"    <toolitem name=\"cut\" action=\"cut\" />\n"
"    <toolitem name=\"copy\" action=\"copy\" />\n"
"    <toolitem name=\"paste\" action=\"paste\" />\n"
"    <toolitem name=\"recent\" action=\"recent\" />\n"
"    <separator name=\"sep10\" />\n"
"    <toolitem name=\"bold\" action=\"bold\" />\n"
"    <separator name=\"sep11\" />\n"
"    <toolitem name=\"justify-left\" action=\"justify-left\" />\n"
"    <toolitem name=\"justify-center\" action=\"justify-center\" />\n"
"    <toolitem name=\"justify-right\" action=\"justify-right\" />\n"
"    <toolitem name=\"justify-fill\" action=\"justify-fill\" />\n"
"    <separator name=\"sep12\"/>\n"
"    <toolitem name=\"quit\" action=\"quit\" />\n"
"  </toolbar>\n"
"  <popup name=\"popup\">\n"
"    <menuitem name=\"popcut\" action=\"cut\" />\n"
"    <menuitem name=\"popcopy\" action=\"copy\" />\n"
"    <menuitem name=\"poppaste\" action=\"paste\" />\n"
"  </popup>\n";

static void
add_widget (GtkUIManager *merge,
	    GtkWidget   *widget,
	    GtkContainer *container)
{

  gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  if (GTK_IS_TOOLBAR (widget)) 
    {
      toolbar = GTK_TOOLBAR (widget);
      gtk_toolbar_set_show_arrow (toolbar, TRUE);
    }
}

static guint ui_id = 0;
static GtkActionGroup *dag = NULL;

static void
ensure_update (GtkUIManager *manager)
{
  GTimer *timer;
  double seconds;
  gulong microsecs;
  
  timer = g_timer_new ();
  g_timer_start (timer);
  
  gtk_ui_manager_ensure_update (manager);
  
  g_timer_stop (timer);
  seconds = g_timer_elapsed (timer, &microsecs);
  g_timer_destroy (timer);
  
  g_print ("Time: %fs\n", seconds);
}

static void
add_cb (GtkWidget *button,
	GtkUIManager *manager)
{
  GtkWidget *spinbutton;
  GtkAction *action;
  int i, num;
  char *name, *label;
  
  if (ui_id != 0 || dag != NULL)
    return;
  
  spinbutton = g_object_get_data (G_OBJECT (button), "spinbutton");
  num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinbutton));
  
  dag = gtk_action_group_new ("DynamicActions");
  gtk_ui_manager_insert_action_group (manager, dag, 0);
  
  ui_id = gtk_ui_manager_new_merge_id (manager);
  
  for (i = 0; i < num; i++)
    {
      name = g_strdup_printf ("DynAction%u", i);
      label = g_strdup_printf ("Dynamic Item %d", i);
      
      action = g_object_new (GTK_TYPE_ACTION,
			     "name", name,
			     "label", label,
			     NULL);
      gtk_action_group_add_action (dag, action);
      g_object_unref (action);
      
      gtk_ui_manager_add_ui (manager, ui_id, "/menubar/DynamicMenu",
			     name, name,
			     GTK_UI_MANAGER_MENUITEM, FALSE);
    }
  
  ensure_update (manager);
}

static void
remove_cb (GtkWidget *button,
	   GtkUIManager *manager)
{
  if (ui_id == 0 || dag == NULL)
    return;
  
  gtk_ui_manager_remove_ui (manager, ui_id);
  ensure_update (manager);
  ui_id = 0;
  
  gtk_ui_manager_remove_action_group (manager, dag);
  g_object_unref (dag);
  dag = NULL;
}

static void
create_window (GtkActionGroup *action_group)
{
  GtkUIManager *merge;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *hbox, *spinbutton, *button;
  GError *error = NULL;

  merge = gtk_ui_manager_new ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, -1);
  gtk_window_set_title (GTK_WINDOW (window), "Action Test");
  g_signal_connect_swapped (window, "destroy", G_CALLBACK (g_object_unref), merge);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  gtk_ui_manager_insert_action_group (merge, action_group, 0);
  g_signal_connect (merge, "add_widget", G_CALLBACK (add_widget), box);

  gtk_window_add_accel_group (GTK_WINDOW (window), 
			      gtk_ui_manager_get_accel_group (merge));

  if (!gtk_ui_manager_add_ui_from_string (merge, ui_info, -1, &error))
    {
      g_message ("building menus failed: %s", error->message);
      g_error_free (error);
    }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_end (GTK_BOX (box), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  spinbutton = gtk_spin_button_new_with_range (100, 10000, 100);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);
  
  button = gtk_button_new_with_label ("Add");
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  
  g_object_set_data (G_OBJECT (button), "spinbutton", spinbutton);
  g_signal_connect (button, "clicked", G_CALLBACK (add_cb), merge);
  
  button = gtk_button_new_with_label ("Remove");
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  
  g_signal_connect (button, "clicked", G_CALLBACK (remove_cb), merge);
  
  gtk_widget_show (window);
}

int
main (int argc, char **argv)
{
  GtkAction *action;

  gtk_init (&argc, &argv);

  if (g_file_test ("accels", G_FILE_TEST_IS_REGULAR))
    gtk_accel_map_load ("accels");

  action = gtk_recent_action_new ("recent",
                                  "Open Recent", "Open recent files",
                                  NULL);
  g_signal_connect (action, "item-activated",
                    G_CALLBACK (recent_action),
                    NULL);
  g_signal_connect (action, "activate",
                    G_CALLBACK (recent_action),
                    NULL);

  action_group = gtk_action_group_new ("TestActions");
  gtk_action_group_add_actions (action_group, 
				entries, n_entries, 
				NULL);
  gtk_action_group_add_toggle_actions (action_group, 
				       toggle_entries, n_toggle_entries, 
				       NULL);
  gtk_action_group_add_radio_actions (action_group, 
				      justify_entries, n_justify_entries, 
				      JUSTIFY_LEFT,
				      G_CALLBACK (radio_action), NULL);
  gtk_action_group_add_radio_actions (action_group, 
				      toolbar_entries, n_toolbar_entries, 
				      GTK_TOOLBAR_BOTH,
				      G_CALLBACK (toolbar_style), NULL);
  gtk_action_group_add_action_with_accel (action_group, action, NULL);

  create_window (action_group);

  gtk_main ();

#ifdef DEBUG_UI_MANAGER
  {
    GList *action;

    for (action = gtk_action_group_list_actions (action_group);
	 action; 
	 action = action->next)
      {
	GtkAction *a = action->data;
	g_print ("action %s ref count %d\n", 
		 gtk_action_get_name (a), G_OBJECT (a)->ref_count);
      }
  }
#endif
  
  g_object_unref (action);
  g_object_unref (action_group);

  gtk_accel_map_save ("accels");

  return 0;
}
