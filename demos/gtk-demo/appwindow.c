/* Application main window
 *
 * Demonstrates a typical application window, with menubar, toolbar, statusbar.
 */

#include <gtk/gtk.h>
#include "demo-common.h"

static GtkWidget *window = NULL;

static void
activate_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "You activated action: \"%s\" of type \"%s\"",
                                    name, typename);

  /* Close dialog on user response */
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  
  gtk_widget_show (dialog);
}


#ifndef N_
#define N_(String) String
#endif

static GtkActionGroupEntry entries[] = {
  { "FileMenu", N_("_File"), NULL, NULL, NULL, NULL, NULL },
  { "PreferencesMenu", N_("_Preferences"), NULL, NULL, NULL, NULL, NULL },
  { "ColorMenu", N_("_Color"), NULL, NULL, NULL, NULL, NULL },
  { "ShapeMenu", N_("_Shape"), NULL, NULL, NULL, NULL, NULL },
  { "HelpMenu", N_("_Help"), NULL, NULL, NULL, NULL, NULL },

  { "New", N_("_New"), GTK_STOCK_NEW, "<control>N", N_("Create a new file"), G_CALLBACK (activate_action), NULL },
  { "Open", N_("_Open"), GTK_STOCK_OPEN, "<control>O", N_("Open a file"), G_CALLBACK (activate_action), NULL },
  { "Save", N_("_Save"), GTK_STOCK_SAVE, "<control>S", N_("Save current file"), G_CALLBACK (activate_action), NULL },
  { "SaveAs", N_("Save _As..."), GTK_STOCK_SAVE, NULL, N_("Save to a file"), G_CALLBACK (activate_action), NULL },
  { "Quit", N_("_Quit"), GTK_STOCK_QUIT, "<control>Q", N_("Quit"), G_CALLBACK (activate_action), NULL },

  { "Red", N_("_Red"), NULL, "<control>R", N_("Blood"), G_CALLBACK (activate_action), NULL, RADIO_ACTION },
  { "Green", N_("_Green"), NULL, "<control>G", N_("Grass"), G_CALLBACK (activate_action), NULL, RADIO_ACTION, "Red" },
  { "Blue", N_("_Blue"), NULL, "<control>B", N_("Sky"), G_CALLBACK (activate_action), NULL, RADIO_ACTION, "Red" },

  { "Square", N_("_Square"), NULL, "<control>S", N_("Square"), G_CALLBACK (activate_action), NULL, RADIO_ACTION },
  { "Rectangle", N_("_Rectangle"), NULL, "<control>R", N_("Rectangle"), G_CALLBACK (activate_action), NULL, RADIO_ACTION, "Square" },
  { "Oval", N_("_Oval"), NULL, "<control>O", N_("Egg"), G_CALLBACK (activate_action), NULL, RADIO_ACTION, "Square" },
  { "About", N_("_About"), NULL, "<control>A", N_("About"), G_CALLBACK (activate_action), NULL },
  { "Logo", NULL, "demo-gtk-logo", NULL, N_("GTK+"), G_CALLBACK (activate_action), NULL },
};
static guint n_entries = G_N_ELEMENTS (entries);

static const gchar *ui_info = 
"<Root>\n"
"  <menu>\n"
"    <submenu name='FileMenu'>\n"
"      <menuitem name='New'/>\n"
"      <menuitem name='Open'/>\n"
"      <menuitem name='Save'/>\n"
"      <menuitem name='SaveAs'/>\n"
"      <separator name='Sep1'/>\n"
"      <menuitem name='Quit'/>\n"
"    </submenu>\n"
"    <submenu name='PreferencesMenu'>\n"
"      <submenu name='ColorMenu'>\n"
"	<menuitem name='Red'/>\n"
"	<menuitem name='Green'/>\n"
"	<menuitem name='Blue'/>\n"
"      </submenu>\n"
"      <submenu name='ShapeMenu'>\n"
"        <menuitem name='Square'/>\n"
"        <menuitem name='Rectangle'/>\n"
"        <menuitem name='Oval'/>\n"
"      </submenu>\n"
"    </submenu>\n"
"    <submenu name='HelpMenu'>\n"
"      <menuitem name='About'/>\n"
"    </submenu>\n"
"  </menu>\n"
"  <dockitem>\n"
"    <toolitem name='Open'/>\n"
"    <toolitem name='Quit'/>\n"
"    <separator name='Sep1'/>\n"
"    <toolitem name='Logo'/>\n"
"  </dockitem>\n"
"</Root>\n";



/* This function registers our custom toolbar icons, so they can be themed.
 *
 * It's totally optional to do this, you could just manually insert icons
 * and have them not be themeable, especially if you never expect people
 * to theme your app.
 */
static void
register_stock_icons (void)
{
  static gboolean registered = FALSE;
  
  if (!registered)
    {
      GdkPixbuf *pixbuf;
      GtkIconFactory *factory;
      char *filename;

      static GtkStockItem items[] = {
        { "demo-gtk-logo",
          "_GTK!",
          0, 0, NULL }
      };
      
      registered = TRUE;

      /* Register our stock items */
      gtk_stock_add (items, G_N_ELEMENTS (items));
      
      /* Add our custom icon factory to the list of defaults */
      factory = gtk_icon_factory_new ();
      gtk_icon_factory_add_default (factory);

      /* demo_find_file() looks in the the current directory first,
       * so you can run gtk-demo without installing GTK, then looks
       * in the location where the file is installed.
       */
      pixbuf = NULL;
      filename = demo_find_file ("gtk-logo-rgb.gif", NULL);
      if (filename)
	{
	  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	  g_free (filename);
	}

      /* Register icon to accompany stock item */
      if (pixbuf != NULL)
        {
          GtkIconSet *icon_set;
          GdkPixbuf *transparent;

          /* The gtk-logo-rgb icon has a white background, make it transparent */
          transparent = gdk_pixbuf_add_alpha (pixbuf, TRUE, 0xff, 0xff, 0xff);
          
          icon_set = gtk_icon_set_new_from_pixbuf (transparent);
          gtk_icon_factory_add (factory, "demo-gtk-logo", icon_set);
          gtk_icon_set_unref (icon_set);
          g_object_unref (pixbuf);
          g_object_unref (transparent);
        }
      else
        g_warning ("failed to load GTK logo for toolbar");
      
      /* Drop our reference to the factory, GTK will hold a reference. */
      g_object_unref (factory);
    }
}

static void
update_statusbar (GtkTextBuffer *buffer,
                  GtkStatusbar  *statusbar)
{
  gchar *msg;
  gint row, col;
  gint count;
  GtkTextIter iter;
  
  gtk_statusbar_pop (statusbar, 0); /* clear any previous message, underflow is allowed */

  count = gtk_text_buffer_get_char_count (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &iter,
                                    gtk_text_buffer_get_insert (buffer));

  row = gtk_text_iter_get_line (&iter);
  col = gtk_text_iter_get_line_offset (&iter);

  msg = g_strdup_printf ("Cursor at row %d column %d - %d chars in document",
                         row, col, count);

  gtk_statusbar_push (statusbar, 0, msg);

  g_free (msg);
}

static void
mark_set_callback (GtkTextBuffer     *buffer,
                   const GtkTextIter *new_location,
                   GtkTextMark       *mark,
                   gpointer           data)
{
  update_statusbar (buffer, GTK_STATUSBAR (data));
}

static void
update_resize_grip (GtkWidget           *widget,
		    GdkEventWindowState *event,
		    GtkStatusbar        *statusbar)
{
  if (event->changed_mask & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))
    gtk_statusbar_set_has_resize_grip (statusbar, !(event->new_window_state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)));
}
		    

static void
add_widget (GtkMenuMerge *merge,
	    GtkWidget   *widget,
	    GtkTable *table)
{
  if (GTK_IS_MENU_BAR (widget)) 
    {
      gtk_table_attach (GTK_TABLE (table),
			widget, 
                        /* X direction */          /* Y direction */
                        0, 1,                      0, 1,
                        GTK_EXPAND | GTK_FILL,     0,
                        0,                         0);
    }
  else if (GTK_IS_TOOLBAR (widget)) 
    {
      gtk_table_attach (GTK_TABLE (table),
                        widget,
                        /* X direction */       /* Y direction */
                        0, 1,                   1, 2,
                        GTK_EXPAND | GTK_FILL,  0,
                        0,                      0);
    }

  gtk_widget_show (widget);
}

GtkWidget *
do_appwindow (void)
{  
  if (!window)
    {
      GtkWidget *table;
      GtkWidget *statusbar;
      GtkWidget *contents;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkActionGroup *action_group;
      GtkAction *action;
      GtkMenuMerge *merge;
      GError *error = NULL;

      register_stock_icons ();
      
      /* Create the toplevel window
       */
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Application Window");

      /* NULL window variable when window is closed */
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      table = gtk_table_new (1, 4, FALSE);
      
      gtk_container_add (GTK_CONTAINER (window), table);
      
      /* Create the menubar and toolbar
       */
      
      action_group = gtk_action_group_new ("AppWindowActions");
      gtk_action_group_add_actions (action_group, entries, n_entries);

      action = gtk_action_group_get_action (action_group, "red");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
      action = gtk_action_group_get_action (action_group, "square");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      merge = gtk_menu_merge_new ();
      gtk_menu_merge_insert_action_group (merge, action_group, 0);
      g_signal_connect (merge, "add_widget", G_CALLBACK (add_widget), table);
      gtk_window_add_accel_group (GTK_WINDOW (window), 
				  gtk_menu_merge_get_accel_group (merge));
      
      if (!gtk_menu_merge_add_ui_from_string (merge, ui_info, -1, &error))
	{
	  g_message ("building menus failed: %s", error->message);
	  g_error_free (error);
	}

      /* Create document
       */

      sw = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                           GTK_SHADOW_IN);
      
      gtk_table_attach (GTK_TABLE (table),
                        sw,
                        /* X direction */       /* Y direction */
                        0, 1,                   2, 3,
                        GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
                        0,                      0);

      gtk_window_set_default_size (GTK_WINDOW (window),
                                   200, 200);
      
      contents = gtk_text_view_new ();

      gtk_container_add (GTK_CONTAINER (sw),
                         contents);

      /* Create statusbar */

      statusbar = gtk_statusbar_new ();
      gtk_table_attach (GTK_TABLE (table),
                        statusbar,
                        /* X direction */       /* Y direction */
                        0, 1,                   3, 4,
                        GTK_EXPAND | GTK_FILL,  0,
                        0,                      0);

      /* Show text widget info in the statusbar */
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));
      
      g_signal_connect_object (buffer,
                               "changed",
                               G_CALLBACK (update_statusbar),
                               statusbar,
                               0);

      g_signal_connect_object (buffer,
                               "mark_set", /* cursor moved */
                               G_CALLBACK (mark_set_callback),
                               statusbar,
                               0);

      g_signal_connect_object (window, 
			       "window_state_event", 
			       G_CALLBACK (update_resize_grip),
			       statusbar,
			       0);
      
      update_statusbar (buffer, GTK_STATUSBAR (statusbar));
    }

  if (!GTK_WIDGET_VISIBLE (window))
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

