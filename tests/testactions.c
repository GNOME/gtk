#undef GTK_DISABLE_DEPRECATED
#include <gtk/gtk.h>

#ifndef _
#  define _(String) (String)
#  define N_(String) (String)
#endif

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
    g_object_set (action, "label", _("Disable Cut and paste ops"), NULL);
  else
    g_object_set (action, "label", _("Enable Cut and paste ops"), NULL);
}

static void
show_accel_dialog (GtkAction *action)
{
  g_message ("Sorry, accel dialog not available");
}

static void
toolbar_style (GtkAction *action, 
	       gpointer   user_data)
{
  GtkToolbarStyle style;

  g_return_if_fail (toolbar != NULL);
  style = GPOINTER_TO_INT (user_data);

  gtk_toolbar_set_style (toolbar, style);
}

static void
toolbar_size (GtkAction *action, 
	      gpointer   user_data)
{
  GtkIconSize size;

  g_return_if_fail (toolbar != NULL);
  size = GPOINTER_TO_INT (user_data);

  gtk_toolbar_set_icon_size (toolbar, size);
}

/* convenience functions for declaring actions */
static GtkActionGroupEntry entries[] = {
  { "Menu1Action", N_("Menu _1"), NULL, NULL, NULL, NULL, NULL },
  { "Menu2Action", N_("Menu _2"), NULL, NULL, NULL, NULL, NULL },

  { "cut", N_("C_ut"), GTK_STOCK_CUT, "<control>X",
    N_("Cut the selected text to the clipboard"),
    G_CALLBACK (activate_action), NULL },
  { "copy", N_("_Copy"), GTK_STOCK_COPY, "<control>C",
    N_("Copy the selected text to the clipboard"),
    G_CALLBACK (activate_action), NULL },
  { "paste", N_("_Paste"), GTK_STOCK_PASTE, "<control>V",
    N_("Paste the text from the clipboard"),
    G_CALLBACK (activate_action), NULL },
  { "bold", N_("_Bold"), GTK_STOCK_BOLD, "<control>B",
    N_("Change to bold face"),
    G_CALLBACK (toggle_action), NULL, GTK_ACTION_TOGGLE },

  { "justify-left", N_("_Left"), GTK_STOCK_JUSTIFY_LEFT, "<control>L",
    N_("Left justify the text"),
    G_CALLBACK (toggle_action), NULL, GTK_ACTION_RADIO },
  { "justify-center", N_("C_enter"), GTK_STOCK_JUSTIFY_CENTER, "<control>E",
    N_("Center justify the text"),
    G_CALLBACK (toggle_action), NULL, GTK_ACTION_RADIO, "justify-left" },
  { "justify-right", N_("_Right"), GTK_STOCK_JUSTIFY_RIGHT, "<control>R",
    N_("Right justify the text"),
    G_CALLBACK (toggle_action), NULL, GTK_ACTION_RADIO, "justify-left" },
  { "justify-fill", N_("_Fill"), GTK_STOCK_JUSTIFY_FILL, "<control>J",
    N_("Fill justify the text"),
    G_CALLBACK (toggle_action), NULL, GTK_ACTION_RADIO, "justify-left" },
  { "quit", NULL, GTK_STOCK_QUIT, "<control>Q",
    N_("Quit the application"),
    G_CALLBACK (gtk_main_quit), NULL },
  { "toggle-cnp", N_("Enable Cut/Copy/Paste"), NULL, NULL,
    N_("Change the sensitivity of the cut, copy and paste actions"),
    G_CALLBACK (toggle_cnp_actions), NULL, GTK_ACTION_TOGGLE },
  { "customise-accels", N_("Customise _Accels"), NULL, NULL,
    N_("Customise keyboard shortcuts"),
    G_CALLBACK (show_accel_dialog), NULL },
  { "toolbar-icons", N_("Icons"), NULL, NULL,
    NULL, G_CALLBACK (toolbar_style), GINT_TO_POINTER (GTK_TOOLBAR_ICONS),
    GTK_ACTION_RADIO, NULL },
  { "toolbar-text", N_("Text"), NULL, NULL,
    NULL, G_CALLBACK (toolbar_style), GINT_TO_POINTER (GTK_TOOLBAR_TEXT),
    GTK_ACTION_RADIO, "toolbar-icons" },
  { "toolbar-both", N_("Both"), NULL, NULL,
    NULL, G_CALLBACK (toolbar_style), GINT_TO_POINTER (GTK_TOOLBAR_BOTH),
    GTK_ACTION_RADIO, "toolbar-icons" },
  { "toolbar-both-horiz", N_("Both Horizontal"), NULL, NULL,
    NULL, G_CALLBACK (toolbar_style), GINT_TO_POINTER(GTK_TOOLBAR_BOTH_HORIZ),
    GTK_ACTION_RADIO, "toolbar-icons" },
  { "toolbar-small-icons", N_("Small Icons"), NULL, NULL,
    NULL,
    G_CALLBACK (toolbar_size), GINT_TO_POINTER (GTK_ICON_SIZE_SMALL_TOOLBAR) },
  { "toolbar-large-icons", N_("Large Icons"), NULL, NULL,
    NULL,
    G_CALLBACK (toolbar_size), GINT_TO_POINTER (GTK_ICON_SIZE_LARGE_TOOLBAR) },
};
static guint n_entries = G_N_ELEMENTS (entries);

/* XML description of the menus for the test app.  The parser understands
 * a subset of the Bonobo UI XML format, and uses GMarkup for parsing */
static const gchar *ui_info =
"<Root>\n"
"  <menu>\n"
"    <submenu name=\"Menu _1\" verb=\"Menu1Action\">\n"
"      <menuitem name=\"cut\" verb=\"cut\" />\n"
"      <menuitem name=\"copy\" verb=\"copy\" />\n"
"      <menuitem name=\"paste\" verb=\"paste\" />\n"
"      <separator name=\"sep1\" />\n"
"      <menuitem name=\"bold1\" verb=\"bold\" />\n"
"      <menuitem name=\"bold2\" verb=\"bold\" />\n"
"      <separator name=\"sep2\" />\n"
"      <menuitem name=\"toggle-cnp\" verb=\"toggle-cnp\" />\n"
"      <separator name=\"sep3\" />\n"
"      <menuitem name=\"quit\" verb=\"quit\" />\n"
"    </submenu>\n"
"    <submenu name=\"Menu _2\" verb=\"Menu2Action\">\n"
"      <menuitem name=\"cut\" verb=\"cut\" />\n"
"      <menuitem name=\"copy\" verb=\"copy\" />\n"
"      <menuitem name=\"paste\" verb=\"paste\" />\n"
"      <separator name=\"sep4\"/>\n"
"      <menuitem name=\"bold\" verb=\"bold\" />\n"
"      <separator name=\"sep5\"/>\n"
"      <menuitem name=\"justify-left\" verb=\"justify-left\" />\n"
"      <menuitem name=\"justify-center\" verb=\"justify-center\" />\n"
"      <menuitem name=\"justify-right\" verb=\"justify-right\" />\n"
"      <menuitem name=\"justify-fill\" verb=\"justify-fill\" />\n"
"      <separator name=\"sep6\"/>\n"
"      <menuitem  name=\"customise-accels\" verb=\"customise-accels\" />\n"
"      <separator name=\"sep7\"/>\n"
"      <menuitem verb=\"toolbar-icons\" />\n"
"      <menuitem verb=\"toolbar-text\" />\n"
"      <menuitem verb=\"toolbar-both\" />\n"
"      <menuitem verb=\"toolbar-both-horiz\" />\n"
"      <separator name=\"sep8\"/>\n"
"      <menuitem verb=\"toolbar-small-icons\" />\n"
"      <menuitem verb=\"toolbar-large-icons\" />\n"
"    </submenu>\n"
"  </menu>\n"
"  <dockitem name=\"toolbar\">\n"
"    <toolitem name=\"cut\" verb=\"cut\" />\n"
"    <toolitem name=\"copy\" verb=\"copy\" />\n"
"    <toolitem name=\"paste\" verb=\"paste\" />\n"
"    <separator name=\"sep9\" />\n"
"    <toolitem name=\"bold\" verb=\"bold\" />\n"
"    <separator name=\"sep10\" />\n"
"    <toolitem name=\"justify-left\" verb=\"justify-left\" />\n"
"    <toolitem name=\"justify-center\" verb=\"justify-center\" />\n"
"    <toolitem name=\"justify-right\" verb=\"justify-right\" />\n"
"    <toolitem name=\"justify-fill\" verb=\"justify-fill\" />\n"
"    <separator name=\"sep11\"/>\n"
"    <toolitem name=\"quit\" verb=\"quit\" />\n"
"  </dockitem>\n"
"</Root>\n";

static void
add_widget (GtkMenuMerge *merge,
	    GtkWidget   *widget,
	    GtkContainer *container)
{

  gtk_container_add (container, widget);
  gtk_widget_show (widget);

  if (GTK_IS_TOOLBAR (widget)) 
    {
      toolbar = GTK_TOOLBAR (widget);
      gtk_toolbar_set_show_arrow (toolbar, TRUE);
    }
}

static void
create_window (GtkActionGroup *action_group)
{
  GtkMenuMerge *merge;
  GtkWidget *window;
  GtkWidget *box;
  GError *error = NULL;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, -1);
  gtk_window_set_title (GTK_WINDOW (window), "Action Test");
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  merge = gtk_menu_merge_new ();
  gtk_menu_merge_insert_action_group (merge, action_group, 0);
  g_signal_connect (merge, "add_widget", G_CALLBACK (add_widget), box);

  gtk_window_add_accel_group (GTK_WINDOW (window), 
			      gtk_menu_merge_get_accel_group (merge));

  if (!gtk_menu_merge_add_ui_from_string (merge, ui_info, -1, &error))
    {
      g_message ("building menus failed: %s", error->message);
      g_error_free (error);
    }

  gtk_widget_show (window);
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  if (g_file_test ("accels", G_FILE_TEST_IS_REGULAR))
    gtk_accel_map_load ("accels");

  action_group = gtk_action_group_new ("TestActions");
  gtk_action_group_add_actions (action_group, entries, n_entries);

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (gtk_action_group_get_action (action_group, "toggle-cnp")), TRUE);

  create_window (action_group);

  gtk_main ();

  g_object_unref (action_group);

  gtk_accel_map_save ("accels");

  return 0;
}
