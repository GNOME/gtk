/* Application main window
 *
 * Demonstrates a typical application window, with menubar, toolbar, statusbar.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;


static void
menuitem_cb (gpointer             callback_data,
             guint                callback_action,
             GtkWidget           *widget)
{
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (GTK_WINDOW (callback_data),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "You selected or toggled the menu item: \"%s\"",
                                    gtk_item_factory_path_from_widget (widget));

  /* Close dialog on user response */
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  
  gtk_widget_show (dialog);
}


static GtkItemFactoryEntry menu_items[] =
{
  { "/_File",            NULL,         0,                     0, "<Branch>" },
  { "/File/tearoff1",    NULL,         menuitem_cb,       0, "<Tearoff>" },
  { "/File/_New",        "<control>N", menuitem_cb,       0, "<StockItem>", GTK_STOCK_NEW },
  { "/File/_Open",       "<control>O", menuitem_cb,       0, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/_Save",       "<control>S", menuitem_cb,       0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As...", NULL,         menuitem_cb,       0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/sep1",        NULL,         menuitem_cb,       0, "<Separator>" },
  { "/File/_Quit",       "<control>Q", menuitem_cb,       0, "<StockItem>", GTK_STOCK_QUIT },

  { "/_Preferences",                    NULL, 0,               0, "<Branch>" },
  { "/_Preferences/_Color",             NULL, 0,               0, "<Branch>" },
  { "/_Preferences/Color/_Red",         NULL, menuitem_cb, 0, "<RadioItem>" },
  { "/_Preferences/Color/_Green",       NULL, menuitem_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/Color/_Blue",        NULL, menuitem_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/_Shape",             NULL, 0,               0, "<Branch>" },
  { "/_Preferences/Shape/_Square",      NULL, menuitem_cb, 0, "<RadioItem>" },
  { "/_Preferences/Shape/_Rectangle",   NULL, menuitem_cb, 0, "/Preferences/Shape/Square" },
  { "/_Preferences/Shape/_Oval",        NULL, menuitem_cb, 0, "/Preferences/Shape/Rectangle" },

  { "/_Help",            NULL,         0,                     0, "<LastBranch>" },
  { "/Help/_About",      NULL,         menuitem_cb,       0 },
};

static void
toolbar_cb (GtkWidget *button,
            gpointer   data)
{
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (GTK_WINDOW (data),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "You selected a toolbar button");

  /* Close dialog on user response */
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  
  gtk_widget_show (dialog);
}

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

      /* Try current directory */
      pixbuf = gdk_pixbuf_new_from_file ("./gtk-logo-rgb.gif", NULL);

      /* Try install directory */
      if (pixbuf == NULL)
        pixbuf = gdk_pixbuf_new_from_file (DEMOCODEDIR"/gtk-logo-rgb.gif", NULL);

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
          g_object_unref (G_OBJECT (pixbuf));
          g_object_unref (G_OBJECT (transparent));
        }
      else
        g_warning ("failed to load GTK logo for toolbar");
      
      /* Drop our reference to the factory, GTK will hold a reference. */
      g_object_unref (G_OBJECT (factory));
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

GtkWidget *
do_appwindow (void)
{  
  if (!window)
    {
      GtkWidget *table;
      GtkWidget *toolbar;
      GtkWidget *statusbar;
      GtkWidget *contents;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkAccelGroup *accel_group;      
      GtkItemFactory *item_factory;

      register_stock_icons ();
      
      /* Create the toplevel window
       */
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Application Window");

      /* NULL window variable when window is closed */
      g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      table = gtk_table_new (1, 4, FALSE);
      
      gtk_container_add (GTK_CONTAINER (window), table);
      
      /* Create the menubar
       */
      
      accel_group = gtk_accel_group_new ();
      gtk_accel_group_attach (accel_group, G_OBJECT (window));
      gtk_accel_group_unref (accel_group);
      
      item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);

      /* Set up item factory to go away with the window */
      gtk_object_ref (GTK_OBJECT (item_factory));
      gtk_object_sink (GTK_OBJECT (item_factory));
      g_object_set_data_full (G_OBJECT (window),
                              "<main>",
                              item_factory,
                              (GDestroyNotify) g_object_unref);

      /* create menu items */
      gtk_item_factory_create_items (item_factory, G_N_ELEMENTS (menu_items),
                                     menu_items, window);

      gtk_table_attach (GTK_TABLE (table),
                        gtk_item_factory_get_widget (item_factory, "<main>"),
                        /* X direction */          /* Y direction */
                        0, 1,                      0, 1,
                        GTK_EXPAND | GTK_FILL,     0,
                        0,                         0);

      /* Create the toolbar
       */
      toolbar = gtk_toolbar_new ();

      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
                                GTK_STOCK_OPEN,
                                "This is a demo button with an 'open' icon",
                                NULL,
                                G_CALLBACK (toolbar_cb),
                                window, /* user data for callback */
                                -1);  /* -1 means "append" */

      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
                                GTK_STOCK_QUIT,
                                "This is a demo button with a 'quit' icon",
                                NULL,
                                G_CALLBACK (toolbar_cb),
                                window, /* user data for callback */
                                -1);  /* -1 means "append" */

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
                                "demo-gtk-logo",
                                "This is a demo button with a 'gtk' icon",
                                NULL,
                                G_CALLBACK (toolbar_cb),
                                window, /* user data for callback */
                                -1);  /* -1 means "append" */

      gtk_table_attach (GTK_TABLE (table),
                        toolbar,
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

