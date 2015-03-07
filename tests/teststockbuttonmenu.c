#include <glib.h>
#define GDK_VERSION_MIN_REQUIRED G_ENCODE_VERSION (3, 8)
#include <gtk/gtk.h>

int main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *button;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *box;
	GtkWidget *label;
	GtkAction *action1;
	GtkAction *action2;
        GtkAccelGroup *accel_group;

	gtk_init (&argc, &argv);

	action1 = gtk_action_new ("bold", NULL, NULL, GTK_STOCK_BOLD);
	action2 = gtk_action_new ("new", NULL, NULL, GTK_STOCK_NEW);
	gtk_action_set_always_show_image (action2, TRUE);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

        accel_group = gtk_accel_group_new ();
        gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	grid = gtk_grid_new ();
	gtk_container_add (GTK_CONTAINER (window), grid);

        /* plain old stock button */
        button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
        gtk_container_add (GTK_CONTAINER (grid), button);

	/* gtk_button_set_always_show_image still works */
        button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
        gtk_container_add (GTK_CONTAINER (grid), button);

	/* old-style image-only button */
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name ("edit-find", GTK_ICON_SIZE_BUTTON));
        gtk_container_add (GTK_CONTAINER (grid), button);

	/* new-style image-only button */
        button = gtk_button_new_from_icon_name ("edit-clear", GTK_ICON_SIZE_BUTTON);
        gtk_container_add (GTK_CONTAINER (grid), button);

	/* GtkAction-backed button */
	button = gtk_button_new ();
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action1);
        gtk_container_add (GTK_CONTAINER (grid), button);

	/* gtk_action_set_always_show_image still works for buttons */
	button = gtk_button_new ();
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action2);
        gtk_container_add (GTK_CONTAINER (grid), button);

	button = gtk_menu_button_new ();
        gtk_container_add (GTK_CONTAINER (grid), button);

	menu = gtk_menu_new ();
        gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
        gtk_menu_set_accel_path (GTK_MENU (menu), "<menu>/TEST");

	gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu);

	/* plain old stock menuitem */
	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* gtk_image_menu_item_set_always_show_image still works */
	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, accel_group);
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* new-style menuitem with image */
	item = gtk_menu_item_new ();
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add (GTK_CONTAINER (item), box);
	gtk_container_add (GTK_CONTAINER (box), gtk_image_new_from_icon_name ("edit-clear", GTK_ICON_SIZE_MENU));
        label = gtk_accel_label_new ("C_lear");
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_widget_set_halign (label, GTK_ALIGN_FILL);

        gtk_widget_add_accelerator (item, "activate", accel_group,
                                    GDK_KEY_x, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
        gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), item);
	gtk_box_pack_end (GTK_BOX (box), label, TRUE, TRUE, 0);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* GtkAction-backed menuitem */
	item = gtk_image_menu_item_new ();
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (item), action1);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* gtk_action_set_always_show_image still works for menuitems */
	item = gtk_image_menu_item_new ();
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (item), action2);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_widget_show_all (menu);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
