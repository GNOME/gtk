/* Item Factory
 *
 * The GtkItemFactory object allows the easy creation of menus
 * from an array of descriptions of menu items.
 */

#include <gtk/gtk.h>

static void
gtk_ifactory_cb (gpointer   callback_data,
		 guint      callback_action,
		 GtkWidget *widget)
{
  g_message ("ItemFactory: activated \"%s\"",
	     gtk_item_factory_path_from_widget (widget));
}

static GtkItemFactoryEntry menu_items[] =
{
  { "/_File",		 NULL,	       0,		      0, "<Branch>" },
  { "/File/tearoff1",	 NULL,	       gtk_ifactory_cb,	      0, "<Tearoff>" },
  { "/File/_New",	 "<control>N", gtk_ifactory_cb,	      0 },
  { "/File/_Open",	 "<control>O", gtk_ifactory_cb,	      0 },
  { "/File/_Save",	 "<control>S", gtk_ifactory_cb,	      0 },
  { "/File/Save _As...", NULL,	       gtk_ifactory_cb,	      0 },
  { "/File/sep1",	 NULL,	       gtk_ifactory_cb,	      0, "<Separator>" },
  { "/File/_Quit",	 "<control>Q", gtk_ifactory_cb,	      0 },

  { "/_Preferences",			NULL, 0,	       0, "<Branch>" },
  { "/_Preferences/_Color",		NULL, 0,	       0, "<Branch>" },
  { "/_Preferences/Color/_Red",		NULL, gtk_ifactory_cb, 0, "<RadioItem>" },
  { "/_Preferences/Color/_Green",	NULL, gtk_ifactory_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/Color/_Blue",	NULL, gtk_ifactory_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/_Shape",		NULL, 0,	       0, "<Branch>" },
  { "/_Preferences/Shape/_Square",	NULL, gtk_ifactory_cb, 0, "<RadioItem>" },
  { "/_Preferences/Shape/_Rectangle",	NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Square" },
  { "/_Preferences/Shape/_Oval",	NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Rectangle" },

  { "/_Help",		 NULL,	       0,		      0, "<LastBranch>" },
  { "/Help/_About",	 NULL,	       gtk_ifactory_cb,	      0 },
};

static int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

GtkWidget *
do_item_factory (void)
{
  static GtkWidget *window = NULL;
  
  if (!window)
    {
      GtkWidget *box1;
      GtkWidget *box2;
      GtkWidget *separator;
      GtkWidget *label;
      GtkWidget *button;
      GtkAccelGroup *accel_group;
      GtkItemFactory *item_factory;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);
      g_signal_connect (window, "delete-event",
			G_CALLBACK (gtk_true), NULL);
      
      accel_group = gtk_accel_group_new ();
      item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
      g_object_set_data_full (G_OBJECT (window), "<main>",
			      item_factory, (GDestroyNotify) g_object_unref);
      gtk_window_add_accel_group (window, accel_group);
      gtk_window_set_title (GTK_WINDOW (window), "Item Factory");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

      /* preselect /Preferences/Shape/Oval over the other radios
       */
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory,
							 "/Preferences/Shape/Oval")),
	 TRUE);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      
      gtk_box_pack_start (GTK_BOX (box1),
			  gtk_item_factory_get_widget (item_factory, "<main>"),
			  FALSE, FALSE, 0);

      label = gtk_label_new ("Type\n<alt>\nto start");
      gtk_widget_set_size_request (label, 200, 200);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
