/* testcombo.c
 * Copyright (C) 2003  Kristian Rietveld
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>

/**
 * oh yes, this test app surely has a lot of ugly code
 */

/* grid combo demo */
static GdkPixbuf *
create_color_pixbuf (const char *color)
{
        GdkPixbuf *pixbuf;
        GdkRGBA rgba;

        int x;
        int num;
        guchar *pixels, *p;

        if (!gdk_rgba_parse (&rgba, color))
                return NULL;

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                 FALSE, 8,
                                 16, 16);

        p = pixels = gdk_pixbuf_get_pixels (pixbuf);

        num = gdk_pixbuf_get_width (pixbuf) *
                gdk_pixbuf_get_height (pixbuf);

        for (x = 0; x < num; x++) {
                p[0] = rgba.red * 255;
                p[1] = rgba.green * 255;
                p[2] = rgba.blue * 255;
                p += 3;
        }

        return pixbuf;
}

static GtkWidget *
create_combo_box_grid_demo (void)
{
        GtkWidget *combo;
        GtkTreeIter iter;
        GdkPixbuf *pixbuf;
        GtkCellRenderer *cell = gtk_cell_renderer_pixbuf_new ();
        GtkListStore *store;

        store = gtk_list_store_new (1, GDK_TYPE_PIXBUF);

        combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo),
                                    cell, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo),
                                        cell, "pixbuf", 0, NULL);
        gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (combo),
                                      3);

        /* first row */
        pixbuf = create_color_pixbuf ("red");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        pixbuf = create_color_pixbuf ("green");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        pixbuf = create_color_pixbuf ("blue");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        /* second row */
        pixbuf = create_color_pixbuf ("yellow");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        pixbuf = create_color_pixbuf ("black");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        pixbuf = create_color_pixbuf ("white");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        /* third row */
        pixbuf = create_color_pixbuf ("gray");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        pixbuf = create_color_pixbuf ("snow");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        pixbuf = create_color_pixbuf ("magenta");
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            -1);
        g_object_unref (pixbuf);

        g_object_unref (store);

        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

        return combo;
}

/* blaat */
static GtkTreeModel *
create_tree_blaat (void)
{
        GdkPixbuf *pixbuf;
        GtkWidget *cellview;
        GtkTreeIter iter, iter2;
        GtkTreeStore *store;

        cellview = gtk_cell_view_new ();

	store = gtk_tree_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_BOOLEAN);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_DIALOG_WARNING,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-dialog-warning",
			    2, FALSE,
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_STOP,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, pixbuf,
                            1, "gtk-stock-stop",
			    2, FALSE,
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_NEW,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, pixbuf,
                            1, "gtk-stock-new",
			    2, FALSE,
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_CLEAR,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-clear",
			    2, FALSE,
                            -1);

#if 0
        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            0, NULL,
                            1, "separator",
			    2, TRUE,
                            -1);
#endif

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_OPEN,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-open",
			    2, FALSE,
                            -1);

        gtk_widget_destroy (cellview);

        return GTK_TREE_MODEL (store);
}

static GtkTreeModel *
create_empty_list_blaat (void)
{
        GdkPixbuf *pixbuf;
        GtkWidget *cellview;
        GtkTreeIter iter;
        GtkListStore *store;

        cellview = gtk_cell_view_new ();

        store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_DIALOG_WARNING,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-dialog-warning",
                            -1);

        gtk_widget_destroy (cellview);

        return GTK_TREE_MODEL (store);
}

static void
populate_list_blaat (gpointer data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkListStore *store;
  GdkPixbuf *pixbuf;
  GtkWidget *cellview;
  GtkTreeIter iter;
  
  store = GTK_LIST_STORE (gtk_combo_box_get_model (combo_box));

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);

  if (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter))
    return;

  cellview = gtk_cell_view_new ();
  
  pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_STOP,
				          GTK_ICON_SIZE_BUTTON);
  gtk_list_store_append (store, &iter);			       
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      1, "gtk-stock-stop",
		      -1);
  
  pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_NEW,
				          GTK_ICON_SIZE_BUTTON);
  gtk_list_store_append (store, &iter);			       
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      1, "gtk-stock-new",
		      -1);
  
  pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_CLEAR,
				          GTK_ICON_SIZE_BUTTON);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      1, "gtk-stock-clear",
		      -1);
  
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, NULL,
		      1, "separator",
		      -1);
  
  pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_OPEN,
				          GTK_ICON_SIZE_BUTTON);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      1, "gtk-stock-open",
		      -1);
  
  gtk_widget_destroy (cellview);  
}

static GtkTreeModel *
create_list_blaat (void)
{
        GdkPixbuf *pixbuf;
        GtkWidget *cellview;
        GtkTreeIter iter;
        GtkListStore *store;

        cellview = gtk_cell_view_new ();

        store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_DIALOG_WARNING,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-dialog-warning",
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_STOP,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_list_store_append (store, &iter);			       
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-stop",
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_NEW,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_list_store_append (store, &iter);			       
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-new",
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_CLEAR,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-clear",
                            -1);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, NULL,
                            1, "separator",
                            -1);

        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_OPEN,
                                                GTK_ICON_SIZE_BUTTON);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, pixbuf,
                            1, "gtk-stock-open",
                            -1);

        gtk_widget_destroy (cellview);

        return GTK_TREE_MODEL (store);
}


static GtkTreeModel *
create_list_long (void)
{
        GtkTreeIter iter;
        GtkListStore *store;

        store = gtk_list_store_new (1, G_TYPE_STRING);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, "here is some long long text that grows out of the combo's allocation",
                            -1);


        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, "with at least a few of these rows",
                            -1);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, "so that we can get some ellipsized text here",
                            -1);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, "and see the combo box menu being allocated without any constraints",
                            -1);

        return GTK_TREE_MODEL (store);
}

static GtkTreeModel *
create_food_list (void)
{
        GtkTreeIter iter;
        GtkListStore *store;

        store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, "Pepperoni",
                            1, "Pizza",
                            -1);

        gtk_list_store_append (store, &iter);			       
        gtk_list_store_set (store, &iter,
                            0, "Cheese",
                            1, "Burger",
                            -1);

        gtk_list_store_append (store, &iter);			       
        gtk_list_store_set (store, &iter,
                            0, "Pineapple",
                            1, "Milkshake",
                            -1);

        gtk_list_store_append (store, &iter);			       
        gtk_list_store_set (store, &iter,
                            0, "Orange",
                            1, "Soda",
                            -1);

        gtk_list_store_append (store, &iter);			       
        gtk_list_store_set (store, &iter,
                            0, "Club",
                            1, "Sandwich",
                            -1);

        return GTK_TREE_MODEL (store);
}


/* blaat */
static GtkTreeModel *
create_phylogenetic_tree (void)
{
        GtkTreeIter iter, iter2, iter3;
        GtkTreeStore *store;

	store = gtk_tree_store_new (1,G_TYPE_STRING);

        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            0, "Eubacteria",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Aquifecales",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Thermotogales",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Thermodesulfobacterium",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Thermus-Deinococcus group",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Chloroflecales",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Cyanobacteria",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Firmicutes",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Leptospirillium Group",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Synergistes",
                            -1);
        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Chlorobium-Flavobacteria group",
                            -1);
        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Chlamydia-Verrucomicrobia group",
                            -1);

        gtk_tree_store_append (store, &iter3, &iter2);			       
        gtk_tree_store_set (store, &iter3,
                            0, "Verrucomicrobia",
                            -1);
        gtk_tree_store_append (store, &iter3, &iter2);			       
        gtk_tree_store_set (store, &iter3,
                            0, "Chlamydia",
                            -1);

        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Flexistipes",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Fibrobacter group",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "spirocheteus",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Proteobacteria",
                            -1);


        gtk_tree_store_append (store, &iter3, &iter2);			       
        gtk_tree_store_set (store, &iter3,
                            0, "alpha",
                            -1);


        gtk_tree_store_append (store, &iter3, &iter2);			       
        gtk_tree_store_set (store, &iter3,
                            0, "beta",
                            -1);


        gtk_tree_store_append (store, &iter3, &iter2);			       
        gtk_tree_store_set (store, &iter3,
                            0, "delta ",
                            -1);


        gtk_tree_store_append (store, &iter3, &iter2);			       
        gtk_tree_store_set (store, &iter3,
                            0, "epsilon",
                            -1);


        gtk_tree_store_append (store, &iter3, &iter2);  
        gtk_tree_store_set (store, &iter3,
                            0, "gamma ",
                            -1);


        gtk_tree_store_append (store, &iter, NULL);			       
        gtk_tree_store_set (store, &iter,
                            0, "Eukaryotes",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Metazoa",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Bilateria",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Myxozoa",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Cnidaria",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Ctenophora",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Placozoa",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Porifera",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "choanoflagellates",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Fungi",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Microsporidia",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Aleveolates",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Stramenopiles",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Rhodophyta",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Viridaeplantae",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "crytomonads et al",
                            -1);


        gtk_tree_store_append (store, &iter, NULL);			       
        gtk_tree_store_set (store, &iter,
                            0, "Archaea ",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Korarchaeota",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Crenarchaeota",
                            -1);


        gtk_tree_store_append (store, &iter2, &iter);			       
        gtk_tree_store_set (store, &iter2,
                            0, "Buryarchaeota",
                            -1);

        return GTK_TREE_MODEL (store);
}


/* blaat */
static GtkTreeModel *
create_capital_tree (void)
{
        GtkTreeIter iter, iter2;
        GtkTreeStore *store;

	store = gtk_tree_store_new (1, G_TYPE_STRING);

        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, 0, "A - B", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Albany", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Annapolis", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Atlanta", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Augusta", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Austin", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Baton Rouge", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Bismarck", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Boise", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Boston", -1);

        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, 0, "C - D", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Carson City", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Charleston", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Cheyenne", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Columbia", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Columbus", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Concord", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Denver", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Des Moines", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Dover", -1);


        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, 0, "E - J", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Frankfort", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Harrisburg", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Hartford", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Helena", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Honolulu", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Indianapolis", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Jackson", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Jefferson City", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Juneau", -1);


        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, 0, "K - O", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Lansing", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Lincoln", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Little Rock", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Madison", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Montgomery", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Montpelier", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Nashville", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Oklahoma City", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Olympia", -1);


        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, 0, "P - S", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Phoenix", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Pierre", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Providence", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Raleigh", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Richmond", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Sacramento", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Salem", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Salt Lake City", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Santa Fe", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Springfield", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "St. Paul", -1);


        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, 0, "T - Z", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Tallahassee", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Topeka", -1);

        gtk_tree_store_append (store, &iter2, &iter);
        gtk_tree_store_set (store, &iter2, 0, "Trenton", -1);

        return GTK_TREE_MODEL (store);
}

static void
capital_sensitive (GtkCellLayout   *cell_layout,
		   GtkCellRenderer *cell,
		   GtkTreeModel    *tree_model,
		   GtkTreeIter     *iter,
		   gpointer         data)
{
  gboolean sensitive;

  sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

static gboolean
capital_animation (gpointer data)
{
  static gint insert_count = 0;
  GtkTreeModel *model = GTK_TREE_MODEL (data);
  GtkTreePath *path;
  GtkTreeIter iter, parent;

  switch (insert_count % 8)
    {
    case 0:
      gtk_tree_store_insert (GTK_TREE_STORE (model), &iter, NULL, 0);
      gtk_tree_store_set (GTK_TREE_STORE (model), 
			  &iter,
			  0, "Europe", -1);
      break;

    case 1:
      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_tree_model_get_iter (model, &parent, path);
      gtk_tree_path_free (path);
      gtk_tree_store_insert (GTK_TREE_STORE (model), &iter, &parent, 0);
      gtk_tree_store_set (GTK_TREE_STORE (model), 
			  &iter,
			  0, "Berlin", -1);
      break;

    case 2:
      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_tree_model_get_iter (model, &parent, path);
      gtk_tree_path_free (path);
      gtk_tree_store_insert (GTK_TREE_STORE (model), &iter, &parent, 1);
      gtk_tree_store_set (GTK_TREE_STORE (model), 
			  &iter,
			  0, "London", -1);
      break;

    case 3:
      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_tree_model_get_iter (model, &parent, path);
      gtk_tree_path_free (path);
      gtk_tree_store_insert (GTK_TREE_STORE (model), &iter, &parent, 2);
      gtk_tree_store_set (GTK_TREE_STORE (model), 
			  &iter,
			  0, "Paris", -1);
      break;

    case 4:
      path = gtk_tree_path_new_from_indices (0, 2, -1);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);
      gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
      break;

    case 5:
      path = gtk_tree_path_new_from_indices (0, 1, -1);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);
      gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
      break;

    case 6:
      path = gtk_tree_path_new_from_indices (0, 0, -1);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);
      gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
      break;

    case 7:
      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);
      gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
      break;

    default: ;

    }
  insert_count++;

  return TRUE;
}

static void
setup_combo_entry (GtkComboBoxText *combo)
{
  gtk_combo_box_text_append_text (combo,
				   "dum de dum");
  gtk_combo_box_text_append_text (combo,
				   "la la la");
  gtk_combo_box_text_append_text (combo,
				   "la la la dum de dum la la la la la la boom de da la la");
  gtk_combo_box_text_append_text (combo,
				   "bloop");
  gtk_combo_box_text_append_text (combo,
				   "bleep");
  gtk_combo_box_text_append_text (combo,
				   "klaas");
  gtk_combo_box_text_append_text (combo,
				   "klaas0");
  gtk_combo_box_text_append_text (combo,
				   "klaas1");
  gtk_combo_box_text_append_text (combo,
				   "klaas2");
  gtk_combo_box_text_append_text (combo,
				   "klaas3");
  gtk_combo_box_text_append_text (combo,
				   "klaas4");
  gtk_combo_box_text_append_text (combo,
				   "klaas5");
  gtk_combo_box_text_append_text (combo,
				   "klaas6");
  gtk_combo_box_text_append_text (combo,
				   "klaas7");
  gtk_combo_box_text_append_text (combo,
				   "klaas8");
  gtk_combo_box_text_append_text (combo,
				   "klaas9");
  gtk_combo_box_text_append_text (combo,
				   "klaasa");
  gtk_combo_box_text_append_text (combo,
				   "klaasb");
  gtk_combo_box_text_append_text (combo,
				   "klaasc");
  gtk_combo_box_text_append_text (combo,
				   "klaasd");
  gtk_combo_box_text_append_text (combo,
				   "klaase");
  gtk_combo_box_text_append_text (combo,
				   "klaasf");
  gtk_combo_box_text_append_text (combo,
				   "klaas10");
  gtk_combo_box_text_append_text (combo,
				   "klaas11");
  gtk_combo_box_text_append_text (combo,
				   "klaas12");
}

static void
set_sensitive (GtkCellLayout   *cell_layout,
	       GtkCellRenderer *cell,
	       GtkTreeModel    *tree_model,
	       GtkTreeIter     *iter,
	       gpointer         data)
{
  GtkTreePath *path;
  gint *indices;
  gboolean sensitive;

  path = gtk_tree_model_get_path (tree_model, iter);
  indices = gtk_tree_path_get_indices (path);
  sensitive = indices[0] != 1;
  gtk_tree_path_free (path);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

static gboolean
is_separator (GtkTreeModel *model,
	      GtkTreeIter  *iter,
	      gpointer      data)
{
  GtkTreePath *path;
  gboolean result;

  path = gtk_tree_model_get_path (model, iter);
  result = gtk_tree_path_get_indices (path)[0] == 4;
  gtk_tree_path_free (path);

  return result;
  
}

static void
displayed_row_changed (GtkComboBox *combo,
                       GtkCellView *cell)
{
  gint row;
  GtkTreePath *path;

  row = gtk_combo_box_get_active (combo);
  path = gtk_tree_path_new_from_indices (row, -1);
  gtk_cell_view_set_displayed_row (cell, path);
  gtk_tree_path_free (path);
}

int
main (int argc, char **argv)
{
        GtkWidget *window, *cellview, *mainbox;
        GtkWidget *combobox, *comboboxtext, *comboboxgrid;
        GtkWidget *tmp, *boom;
        GtkCellRenderer *renderer;
        GdkPixbuf *pixbuf;
        GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkRGBA color;
	GtkCellArea *area;

        gtk_init (&argc, &argv);

	if (g_getenv ("RTL"))
	  gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

	if (g_getenv ("LISTMODE"))
	  {
	    GtkCssProvider *provider = gtk_css_provider_new ();

	    gtk_css_provider_load_from_data (provider,
					     "* { -GtkComboBox-appears-as-list: true; }", 
					     -1, NULL);

	    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
						       GTK_STYLE_PROVIDER (provider),
						       GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

	  }

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_container_set_border_width (GTK_CONTAINER (window), 5);
        g_signal_connect (window, "destroy", gtk_main_quit, NULL);

        mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        gtk_container_add (GTK_CONTAINER (window), mainbox);

        /* GtkCellView */
        tmp = gtk_frame_new ("GtkCellView");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        cellview = gtk_cell_view_new ();
        renderer = gtk_cell_renderer_pixbuf_new ();
        pixbuf = gtk_widget_render_icon_pixbuf (cellview, GTK_STOCK_DIALOG_WARNING,
                                                GTK_ICON_SIZE_BUTTON);

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                    renderer,
                                    FALSE);
        g_object_set (renderer, "pixbuf", pixbuf, NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                    renderer,
                                    TRUE);
        g_object_set (renderer, "text", "la la la", NULL);
        gtk_container_add (GTK_CONTAINER (boom), cellview);

        /* GtkComboBox list */
        tmp = gtk_frame_new ("GtkComboBox (list)");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_list_blaat ();
        combobox = gtk_combo_box_new_with_model (model);
	gtk_combo_box_set_add_tearoffs (GTK_COMBO_BOX (combobox), TRUE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);

        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "pixbuf", 0,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 1,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combobox), 
					      is_separator, NULL, NULL);
						
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

        /* GtkComboBox dynamic list */
        tmp = gtk_frame_new ("GtkComboBox (dynamic list)");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_empty_list_blaat ();
        combobox = gtk_combo_box_new_with_model (model);
	g_signal_connect (combobox, "notify::popup-shown", 
			  G_CALLBACK (populate_list_blaat), combobox);

	gtk_combo_box_set_add_tearoffs (GTK_COMBO_BOX (combobox), TRUE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);

        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "pixbuf", 0,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 1,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combobox), 
					      is_separator, NULL, NULL);
						
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
	gtk_combo_box_set_title (GTK_COMBO_BOX (combobox), "Dynamic list");

        /* GtkComboBox custom entry */
        tmp = gtk_frame_new ("GtkComboBox (custom)");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_list_blaat ();
        combobox = gtk_combo_box_new_with_model (model);
	gtk_combo_box_set_add_tearoffs (GTK_COMBO_BOX (combobox), TRUE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);

        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "pixbuf", 0,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 1,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combobox), 
					      is_separator, NULL, NULL);
						
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

        tmp = gtk_cell_view_new ();
        gtk_widget_show (tmp);
        gtk_cell_view_set_model (GTK_CELL_VIEW (tmp), model);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tmp), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (tmp), renderer,
                                        "text", 1,
                                        NULL);
        color.red = 1.0;
        color.blue = 1.0;
        color.green = 0;
        color.alpha = 1.0;
        gtk_cell_view_set_background_rgba (GTK_CELL_VIEW (tmp), &color);
        displayed_row_changed (GTK_COMBO_BOX (combobox), GTK_CELL_VIEW (tmp));
        g_signal_connect (combobox, "changed", G_CALLBACK (displayed_row_changed), tmp); 
           
        gtk_container_add (GTK_CONTAINER (combobox), tmp);

        /* GtkComboBox tree */
        tmp = gtk_frame_new ("GtkComboBox (tree)");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_tree_blaat ();
        combobox = gtk_combo_box_new_with_model (model);
	gtk_combo_box_set_add_tearoffs (GTK_COMBO_BOX (combobox), TRUE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);

        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "pixbuf", 0,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 1,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    set_sensitive,
					    NULL, NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combobox), 
					      is_separator, NULL, NULL);
						
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
#if 0
	g_timeout_add (1000, (GSourceFunc) animation_timer, model);
#endif

        /* GtkComboBox (grid mode) */
        tmp = gtk_frame_new ("GtkComboBox (grid mode)");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        comboboxgrid = create_combo_box_grid_demo ();
        gtk_box_pack_start (GTK_BOX (boom), comboboxgrid, FALSE, FALSE, 0);


        /* GtkComboBoxEntry */
        tmp = gtk_frame_new ("GtkComboBox with entry");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        comboboxtext = gtk_combo_box_text_new_with_entry ();
        setup_combo_entry (GTK_COMBO_BOX_TEXT (comboboxtext));
        gtk_container_add (GTK_CONTAINER (boom), comboboxtext);


        /* Phylogenetic tree */
        tmp = gtk_frame_new ("What are you ?");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_phylogenetic_tree ();
        combobox = gtk_combo_box_new_with_model (model);
	gtk_combo_box_set_add_tearoffs (GTK_COMBO_BOX (combobox), TRUE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 0,
                                        NULL);
	
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

        /* Capitals */
        tmp = gtk_frame_new ("Where are you ?");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_capital_tree ();
	combobox = gtk_combo_box_new_with_model (model);
	gtk_combo_box_set_add_tearoffs (GTK_COMBO_BOX (combobox), TRUE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 0,
                                        NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox),
					    renderer,
					    capital_sensitive,
					    NULL, NULL);
	path = gtk_tree_path_new_from_indices (0, 8, -1);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

#if 1
	gdk_threads_add_timeout (1000, (GSourceFunc) capital_animation, model);
#endif

        /* Aligned Food */
        tmp = gtk_frame_new ("Hungry ?");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

        model = create_food_list ();
	combobox = gtk_combo_box_new_with_model (model);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);

	area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (combobox));

        renderer = gtk_cell_renderer_text_new ();
	gtk_cell_area_add_with_properties (area, renderer, 
					   "align", TRUE, 
					   "expand", TRUE, 
					   NULL);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 0,
                                        NULL);

        renderer = gtk_cell_renderer_text_new ();
	gtk_cell_area_add_with_properties (area, renderer, 
					   "align", TRUE, 
					   "expand", TRUE, 
					   NULL);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 1,
                                        NULL);

        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

	/* Ellipsizing growing combos */
        tmp = gtk_frame_new ("Unconstrained Menu");
        gtk_box_pack_start (GTK_BOX (mainbox), tmp, FALSE, FALSE, 0);

        boom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (boom), 5);
        gtk_container_add (GTK_CONTAINER (tmp), boom);

	model = create_list_long ();
	combobox = gtk_combo_box_new_with_model (model);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (boom), combobox);
        renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", 0, NULL);
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
	gtk_combo_box_set_popup_fixed_width (GTK_COMBO_BOX (combobox), FALSE);

        gtk_widget_show_all (window);

        gtk_main ();

        return 0;
}

/* vim:expandtab
 */
