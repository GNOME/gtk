#include <gtk/gtk.h>
#include "prop-editor.h"

int main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *grid;
	GtkWidget *entry;
	GtkWidget *menu_widget;
	GtkAccelGroup *accel_group;
	guint i;
	GMenu *menu;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize (GTK_WINDOW (window), 400, 300);

	grid = gtk_grid_new ();
	gtk_container_add (GTK_CONTAINER (window), grid);

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	/* Button next to entry */
	entry = gtk_entry_new ();
	gtk_grid_attach (GTK_GRID (grid),
			 entry,
			 0, 0,
			 1, 1);
	button = gtk_menu_button_new ();
	gtk_grid_attach (GTK_GRID (grid),
			 button,
			 1, 0,
			 1, 1);

	/* Button with GtkMenu */
	menu_widget = gtk_menu_new ();
	for (i = 5; i > 0; i--) {
		GtkWidget *item;

		if (i == 3) {
			item = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, accel_group);
		} else {
			char *label;

			label = g_strdup_printf ("Item _%d", i);
			item = gtk_menu_item_new_with_mnemonic (label);
			g_free (label);
		}
		gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);
		gtk_menu_attach (GTK_MENU (menu_widget),
				 item,
				 0, 1,
				 i - 1, i);
	}
	gtk_widget_show_all (menu_widget);

	button = gtk_menu_button_new ();
	gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu_widget);
	gtk_grid_attach (GTK_GRID (grid),
			 button,
			 1, 1,
			 1, 1);
	gtk_widget_show (create_prop_editor (G_OBJECT (button), 0));

	/* Button with GMenuModel */
	menu = g_menu_new ();
	for (i = 5; i > 0; i--) {
		char *label;
		label = g_strdup_printf ("Item _%d", i);
		g_menu_insert (menu, i - 1, label, NULL);
		g_free (label);
	}
	button = gtk_menu_button_new ();
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));
	gtk_grid_attach (GTK_GRID (grid),
			 button,
			 1, 2,
			 1, 1);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
