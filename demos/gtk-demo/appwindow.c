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


static GtkActionGroupEntry entries[] = {
  { "FileMenu", "_File" },               /* name, label */
  { "PreferencesMenu", "_Preferences" }, /* name, label */
  { "ColorMenu", "_Color"  },            /* name, label */
  { "ShapeMenu", "_Shape" },             /* name, label */
  { "HelpMenu", "_Help" },               /* name, label */
  { "New", "_New",                       /* name, label */
    GTK_STOCK_NEW, "<control>N",         /* stock_id, accelerator */
    "Create a new file",                 /* tooltip */ 
    G_CALLBACK (activate_action) },      
  { "Open", "_Open",                     /* name, label */
    GTK_STOCK_OPEN, "<control>O",        /* stock_id, accelerator */     
    "Open a file",                       /* tooltip */
    G_CALLBACK (activate_action) },
  { "Save", "_Save",                     /* name, label */
    GTK_STOCK_SAVE, "<control>S",        /* stock_id, accelerator */     
    "Save current file",                 /* tooltip */
    G_CALLBACK (activate_action) },
  { "SaveAs", "Save _As...",             /* name, label */
    GTK_STOCK_SAVE, NULL,                /* stock_id, accelerator */     
    "Save to a file",                    /* tooltip */
    G_CALLBACK (activate_action) },
  { "Quit", "_Quit",                     /* name, label */
    GTK_STOCK_QUIT, "<control>Q",        /* stock_id, accelerator */     
    "Quit",                              /* tooltip */
    G_CALLBACK (activate_action) },
  { "Red", "_Red",                       /* name, label */
    NULL, "<control>R",                  /* stock_id, accelerator */     
    "Blood",                             /* tooltip */
    G_CALLBACK (activate_action), NULL,  
    GTK_ACTION_RADIO },                  /* entry type */
  { "Green", "_Green",                   /* name, label */
    NULL, "<control>G",                  /* stock_id, accelerator */     
    "Grass",                             /* tooltip */
    G_CALLBACK (activate_action), NULL, 
    GTK_ACTION_RADIO, "Red" },           /* entry type, radio group */
  { "Blue", "_Blue",                     /* name, label */
    NULL, "<control>B",                  /* stock_id, accelerator */     
    "Sky",                               /* tooltip */
    G_CALLBACK (activate_action), NULL, 
    GTK_ACTION_RADIO, "Red" },           /* entry type, radio group */
  { "Square", "_Square",                 /* name, label */
    NULL, "<control>S",                  /* stock_id, accelerator */     
    "Square",                            /* tooltip */
    G_CALLBACK (activate_action), NULL, 
    GTK_ACTION_RADIO },                  /* entry type */
  { "Rectangle", "_Rectangle",           /* name, label */
    NULL, "<control>R",                  /* stock_id, accelerator */     
    "Rectangle",                         /* tooltip */
    G_CALLBACK (activate_action), NULL, 
    GTK_ACTION_RADIO, "Square" },        /* entry type, radio group */
  { "Oval", "_Oval",                     /* name, label */
    NULL, "<control>O",                  /* stock_id, accelerator */     
    "Egg",                               /* tooltip */  
    G_CALLBACK (activate_action), NULL, 
    GTK_ACTION_RADIO, "Square" },        /* entry type, radio group */
  { "About", "_About",                   /* name, label */
    NULL, "<control>A",                  /* stock_id, accelerator */     
    "About",                             /* tooltip */  
    G_CALLBACK (activate_action) },
  { "Logo", NULL,                        /* name, label */
    "demo-gtk-logo", NULL,               /* stock_id, accelerator */     
    "GTK+",                              /* tooltip */
    G_CALLBACK (activate_action) },
};
static guint n_entries = G_N_ELEMENTS (entries);

static const gchar *ui_info = 
"<Root>"
"  <menu name='MenuBar'>"
"    <submenu name='FileMenu'>"
"      <menuitem name='New'/>"
"      <menuitem name='Open'/>"
"      <menuitem name='Save'/>"
"      <menuitem name='SaveAs'/>"
"      <separator name='Sep1'/>"
"      <menuitem name='Quit'/>"
"    </submenu>"
"    <submenu name='PreferencesMenu'>"
"      <submenu name='ColorMenu'>"
"	<menuitem name='Red'/>"
"	<menuitem name='Green'/>"
"	<menuitem name='Blue'/>"
"      </submenu>"
"      <submenu name='ShapeMenu'>"
"        <menuitem name='Square'/>"
"        <menuitem name='Rectangle'/>"
"        <menuitem name='Oval'/>"
"      </submenu>"
"    </submenu>"
"    <submenu name='HelpMenu'>"
"      <menuitem name='About'/>"
"    </submenu>"
"  </menu>"
"  <dockitem name='ToolBar'>"
"    <toolitem name='Open'/>"
"    <toolitem name='Quit'/>"
"    <separator name='Sep1'/>"
"    <toolitem name='Logo'/>"
"  </dockitem>"
"</Root>";



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
		    

GtkWidget *
do_appwindow (void)
{  
  if (!window)
    {
      GtkWidget *table;
      GtkWidget *statusbar;
      GtkWidget *contents;
      GtkWidget *sw;
      GtkWidget *bar;
      GtkTextBuffer *buffer;
      GtkActionGroup *action_group;
      GtkAction *action;
      GtkUIManager *merge;
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

      action = gtk_action_group_get_action (action_group, "Red");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
      action = gtk_action_group_get_action (action_group, "Square");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      merge = gtk_ui_manager_new ();
      gtk_ui_manager_insert_action_group (merge, action_group, 0);
      gtk_window_add_accel_group (GTK_WINDOW (window), 
				  gtk_ui_manager_get_accel_group (merge));
      
      if (!gtk_ui_manager_add_ui_from_string (merge, ui_info, -1, &error))
	{
	  g_message ("building menus failed: %s", error->message);
	  g_error_free (error);
	}

      bar = gtk_ui_manager_get_widget (merge, "/MenuBar");
      gtk_widget_show (bar);
      gtk_table_attach (GTK_TABLE (table),
			bar, 
                        /* X direction */          /* Y direction */
                        0, 1,                      0, 1,
                        GTK_EXPAND | GTK_FILL,     0,
                        0,                         0);

      bar = gtk_ui_manager_get_widget (merge, "/ToolBar");
      gtk_widget_show (bar);
      gtk_table_attach (GTK_TABLE (table),
			bar, 
                        /* X direction */       /* Y direction */
                        0, 1,                   1, 2,
                        GTK_EXPAND | GTK_FILL,  0,
                        0,                      0);

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

