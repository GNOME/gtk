#undef GTK_DISABLE_DEPRECATED

#include <config.h>
#include "x11/gdkx.h"
#include <gtk/gtk.h>

enum
{
  ACTION_FILE_NEW,
  ACTION_FILE_OPEN,
  ACTION_OK,
  ACTION_HELP_ABOUT
};

static void
print_hello (GtkWidget *w,
	     guint      action)
{
  switch (action)
    {
    case ACTION_FILE_NEW:
      g_message ("File New activated");
      break;
    case ACTION_FILE_OPEN:
      g_message ("File Open activated");
      break;
    case ACTION_OK:
      g_message ("OK activated");
      break;
    case ACTION_HELP_ABOUT:
      g_message ("Help About activated ");
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkItemFactoryEntry menu_items[] = {
  { "/_File",         NULL,         NULL,           0, "<Branch>" },
  { "/File/_New",     "<control>N", print_hello,    ACTION_FILE_NEW, "<Item>" },
  { "/File/_Open",    "<control>O", print_hello,    ACTION_FILE_OPEN, "<Item>" },
  { "/File/sep1",     NULL,         NULL,           0, "<Separator>" },
  { "/File/Quit",     "<control>Q", gtk_main_quit,  0, "<Item>" },
  { "/O_K",            "<control>K",print_hello,    ACTION_OK, "<Item>" },
  { "/_Help",         NULL,         NULL,           0, "<LastBranch>" },
  { "/_Help/About",   NULL,         print_hello,    ACTION_HELP_ABOUT, "<Item>" },
};

static void
remove_buttons (GtkWidget *widget, GtkWidget *other_button)
{
  gtk_widget_destroy (other_button);
  gtk_widget_destroy (widget);
}

static gboolean
blink_cb (gpointer data)
{
  GtkWidget *widget = data;

  gtk_widget_show (widget);
  g_object_set_data (G_OBJECT (widget), "blink", NULL);

  return FALSE;
}

static void
blink (GtkWidget *widget,
       GtkWidget *window)
{
  guint blink_timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "blink"));
  
  if (!blink_timeout)
    {
      blink_timeout = g_timeout_add (1000, blink_cb, window);
      gtk_widget_hide (window);

      g_object_set_data (G_OBJECT (window), "blink", GUINT_TO_POINTER (blink_timeout));
    }
}

static void
local_destroy (GtkWidget *window)
{
  guint blink_timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "blink"));
  if (blink_timeout)
    g_source_remove (blink_timeout);
}

static void
remote_destroy (GtkWidget *window)
{
  local_destroy (window);
  gtk_main_quit ();
}

static void
add_buttons (GtkWidget *widget, GtkWidget *box)
{
  GtkWidget *add_button;
  GtkWidget *remove_button;

  add_button = gtk_button_new_with_mnemonic ("_Add");
  gtk_box_pack_start (GTK_BOX (box), add_button, TRUE, TRUE, 0);
  gtk_widget_show (add_button);

  g_signal_connect (add_button, "clicked",
		    G_CALLBACK (add_buttons),
		    box);

  remove_button = gtk_button_new_with_mnemonic ("_Remove");
  gtk_box_pack_start (GTK_BOX (box), remove_button, TRUE, TRUE, 0);
  gtk_widget_show (remove_button);

  g_signal_connect (remove_button, "clicked",
		    G_CALLBACK (remove_buttons),
		    add_button);
}

static GtkWidget *
create_combo (void)
{
  GList *cbitems;
  GtkCombo *combo;

  cbitems = NULL;
  cbitems = g_list_append (cbitems, "item0");
  cbitems = g_list_append (cbitems, "item1 item1");
  cbitems = g_list_append (cbitems, "item2 item2 item2");
  cbitems = g_list_append (cbitems, "item3 item3 item3 item3");
  cbitems = g_list_append (cbitems, "item4 item4 item4 item4 item4");
  cbitems = g_list_append (cbitems, "item5 item5 item5 item5 item5 item5");
  cbitems = g_list_append (cbitems, "item6 item6 item6 item6 item6");
  cbitems = g_list_append (cbitems, "item7 item7 item7 item7");
  cbitems = g_list_append (cbitems, "item8 item8 item8");
  cbitems = g_list_append (cbitems, "item9 item9");

  combo = GTK_COMBO (gtk_combo_new ());
  gtk_combo_set_popdown_strings (combo, cbitems);
  gtk_entry_set_text (GTK_ENTRY (combo->entry), "hello world");
  gtk_editable_select_region (GTK_EDITABLE (combo->entry), 0, -1);

  return GTK_WIDGET (combo);
}

static GtkWidget *
create_menubar (GtkWindow *window)
{
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group=NULL;
  GtkWidget *menubar;
  
  accel_group = gtk_accel_group_new ();
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                       accel_group);
  gtk_item_factory_create_items (item_factory,
				 G_N_ELEMENTS (menu_items),
				 menu_items, NULL);
  
  gtk_window_add_accel_group (window, accel_group);
  menubar = gtk_item_factory_get_widget (item_factory, "<main>");

  return menubar;
}

static GtkWidget *
create_combo_box (void)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (gtk_combo_box_new_text ());

  gtk_combo_box_append_text (combo_box, "This");
  gtk_combo_box_append_text (combo_box, "Is");
  gtk_combo_box_append_text (combo_box, "A");
  gtk_combo_box_append_text (combo_box, "ComboBox");
  
  return GTK_WIDGET (combo_box);
}

static GtkWidget *
create_content (GtkWindow *window, gboolean local)
{
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *frame;

  frame = gtk_frame_new (local? "Local" : "Remote");
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  vbox = gtk_vbox_new (TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  /* Combo */
  gtk_box_pack_start (GTK_BOX (vbox), create_combo(), TRUE, TRUE, 0);

  /* Entry */
  gtk_box_pack_start (GTK_BOX (vbox), gtk_entry_new(), TRUE, TRUE, 0);

  /* Close Button */
  button = gtk_button_new_with_mnemonic ("_Close");
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy), window);

  /* Blink Button */
  button = gtk_button_new_with_mnemonic ("_Blink");
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (blink),
		    window);

  /* Menubar */
  gtk_box_pack_start (GTK_BOX (vbox), create_menubar (GTK_WINDOW (window)),
		      TRUE, TRUE, 0);

  /* Combo Box */
  gtk_box_pack_start (GTK_BOX (vbox), create_combo_box (), TRUE, TRUE, 0);
  
  add_buttons (NULL, vbox);

  return frame;
}

guint32
create_child_plug (guint32  xid,
		   gboolean local)
{
  GtkWidget *window;
  GtkWidget *content;

  window = gtk_plug_new (xid);

  g_signal_connect (window, "destroy",
		    local ? G_CALLBACK (local_destroy)
			  : G_CALLBACK (remote_destroy),
		    NULL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  content = create_content (GTK_WINDOW (window), local);
  
  gtk_container_add (GTK_CONTAINER (window), content);

  gtk_widget_show_all (window);

  if (GTK_WIDGET_REALIZED (window))
    return GDK_WINDOW_XID (window->window);
  else
    return 0;
}
