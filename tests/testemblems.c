#include <gtk/gtk.h>

int main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *grid;
        GIcon *icon;
        GIcon *icon2;

	gtk_init ();

	window = gtk_window_new ();

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
        gtk_window_set_child (GTK_WINDOW (window), grid);

        icon = g_themed_icon_new ("folder");
        button = gtk_image_new_from_gicon (icon);
	gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 1);

        icon2 = g_themed_icon_new ("folder-symbolic");
        button = gtk_image_new_from_gicon (icon2);
	gtk_grid_attach (GTK_GRID (grid), button, 2, 1, 1, 1);

	icon = g_emblemed_icon_new (icon, g_emblem_new (g_themed_icon_new ("emblem-new")));
        button = gtk_image_new_from_gicon (icon);
	gtk_grid_attach (GTK_GRID (grid), button, 1, 2, 1, 1);

	icon2 = g_emblemed_icon_new (icon2, g_emblem_new (g_themed_icon_new ("emblem-new")));
        button = gtk_image_new_from_gicon (icon2);
	gtk_grid_attach (GTK_GRID (grid), button, 2, 2, 1, 1);

	gtk_widget_show (window);

        while (TRUE)
                g_main_context_iteration (NULL, TRUE);

	return 0;
}
