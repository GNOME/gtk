/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include "gtk.h"
#include "../gdk/gdk.h"
#include "../gdk/gdkx.h"

#include "circles.xbm"

GtkWidget *shape_create_icon (char     *xpm_file,
			      gint      x,
			      gint      y,
			      gint      px,
			      gint      py,
			      gint      window_type);

/* macro, structure and variables used by tree window demos */
#define DEFAULT_NUMBER_OF_ITEM  3
#define DEFAULT_RECURSION_LEVEL 3

struct {
  GSList* selection_mode_group;
  GtkWidget* single_button;
  GtkWidget* browse_button;
  GtkWidget* multiple_button;
  GtkWidget* draw_line_button;
  GtkWidget* view_line_button;
  GtkWidget* no_root_item_button;
  GtkWidget* nb_item_spinner;
  GtkWidget* recursion_spinner;
} sTreeSampleSelection;

typedef struct sTreeButtons {
  guint nb_item_add;
  GtkWidget* add_button;
  GtkWidget* remove_button;
} sTreeButtons;
/* end of tree section */

static void
destroy_tooltips (GtkWidget *widget, GtkWindow **window)
{
  GtkTooltips *tt = gtk_object_get_data (GTK_OBJECT (*window), "tooltips");
  gtk_object_unref (GTK_OBJECT (tt));
  
  *window = NULL;
}

static void
button_window (GtkWidget *widget,
	       GtkWidget *button)
{
  if (!GTK_WIDGET_VISIBLE (button))
    gtk_widget_show (button);
  else
    gtk_widget_hide (button);
}

static void
create_buttons ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *table;
  GtkWidget *button[10];
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "buttons");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      table = gtk_table_new (3, 3, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 5);
      gtk_table_set_col_spacings (GTK_TABLE (table), 5);
      gtk_container_border_width (GTK_CONTAINER (table), 10);
      gtk_box_pack_start (GTK_BOX (box1), table, TRUE, TRUE, 0);
      gtk_widget_show (table);


      button[0] = gtk_button_new_with_label ("button1");
      button[1] = gtk_button_new_with_label ("button2");
      button[2] = gtk_button_new_with_label ("button3");
      button[3] = gtk_button_new_with_label ("button4");
      button[4] = gtk_button_new_with_label ("button5");
      button[5] = gtk_button_new_with_label ("button6");
      button[6] = gtk_button_new_with_label ("button7");
      button[7] = gtk_button_new_with_label ("button8");
      button[8] = gtk_button_new_with_label ("button9");

      gtk_signal_connect (GTK_OBJECT (button[0]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[1]);

      gtk_table_attach (GTK_TABLE (table), button[0], 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[0]);

      gtk_signal_connect (GTK_OBJECT (button[1]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[2]);

      gtk_table_attach (GTK_TABLE (table), button[1], 1, 2, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[1]);

      gtk_signal_connect (GTK_OBJECT (button[2]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[3]);
      gtk_table_attach (GTK_TABLE (table), button[2], 2, 3, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[2]);

      gtk_signal_connect (GTK_OBJECT (button[3]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[4]);
      gtk_table_attach (GTK_TABLE (table), button[3], 0, 1, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[3]);

      gtk_signal_connect (GTK_OBJECT (button[4]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[5]);
      gtk_table_attach (GTK_TABLE (table), button[4], 2, 3, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[4]);

      gtk_signal_connect (GTK_OBJECT (button[5]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[6]);
      gtk_table_attach (GTK_TABLE (table), button[5], 1, 2, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[5]);

      gtk_signal_connect (GTK_OBJECT (button[6]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[7]);
      gtk_table_attach (GTK_TABLE (table), button[6], 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[6]);

      gtk_signal_connect (GTK_OBJECT (button[7]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[8]);
      gtk_table_attach (GTK_TABLE (table), button[7], 2, 3, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[7]);

      gtk_signal_connect (GTK_OBJECT (button[8]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[0]);
      gtk_table_attach (GTK_TABLE (table), button[8], 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (button[8]);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button[9] = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button[9]), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button[9], TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button[9], GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button[9]);
      gtk_widget_show (button[9]);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
create_toggle_buttons ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "toggle buttons");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_toggle_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
create_check_buttons ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "check buttons");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_check_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_check_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_check_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
create_radio_buttons ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "radio buttons");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_radio_button_new_with_label (NULL, "button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_radio_button_new_with_label (
	         gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		 "button2");
      gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		 "button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
bbox_widget_destroy (GtkWidget* widget, GtkWidget* todestroy)
{
}

static void
create_bbox_window (gint  horizontal,
		    char* title, 
		    gint  pos, 
		    gint  spacing,
		    gint  child_w, 
		    gint  child_h, 
		    gint  layout)
{
  GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* bbox;
  GtkWidget* button;
	
  /* create a new window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), title);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC(bbox_widget_destroy), window);
  
  if (horizontal)
  {
    gtk_widget_set_usize (window, 550, 60);
    gtk_widget_set_uposition (window, 150, pos);
    box1 = gtk_vbox_new (FALSE, 0);
  }
  else
  {
    gtk_widget_set_usize (window, 150, 400);
    gtk_widget_set_uposition (window, pos, 200);
    box1 = gtk_vbox_new (FALSE, 0);
  }
  
  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_widget_show (box1);
  
  if (horizontal)
    bbox = gtk_hbutton_box_new();
  else
    bbox = gtk_vbutton_box_new();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), layout);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), spacing);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), child_w, child_h);
  gtk_widget_show (bbox);
  
  gtk_container_border_width (GTK_CONTAINER(box1), 25);
  gtk_box_pack_start (GTK_BOX (box1), bbox, TRUE, TRUE, 0);
  
  button = gtk_button_new_with_label ("OK");
  gtk_container_add (GTK_CONTAINER(bbox), button);

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC(bbox_widget_destroy), window);

  gtk_widget_show (button);
  
  button = gtk_button_new_with_label ("Cancel");
  gtk_container_add (GTK_CONTAINER(bbox), button);
  gtk_widget_show (button);
  
  button = gtk_button_new_with_label ("Help");
  gtk_container_add (GTK_CONTAINER(bbox), button);
  gtk_widget_show (button);
  
  gtk_widget_show (window);
}

static void
test_hbbox ()
{
  create_bbox_window (TRUE, "Spread", 50, 40, 85, 28, GTK_BUTTONBOX_SPREAD);
  create_bbox_window (TRUE, "Edge", 200, 40, 85, 25, GTK_BUTTONBOX_EDGE);
  create_bbox_window (TRUE, "Start", 350, 40, 85, 25, GTK_BUTTONBOX_START);
  create_bbox_window (TRUE, "End", 500, 15, 30, 25, GTK_BUTTONBOX_END);
}

static void
test_vbbox ()
{
  create_bbox_window (FALSE, "Spread", 50, 40, 85, 25, GTK_BUTTONBOX_SPREAD);
  create_bbox_window (FALSE, "Edge", 250, 40, 85, 28, GTK_BUTTONBOX_EDGE);
  create_bbox_window (FALSE, "Start", 450, 40, 85, 25, GTK_BUTTONBOX_START);
  create_bbox_window (FALSE, "End", 650, 15, 30, 25, GTK_BUTTONBOX_END);
} 

static void
create_button_box ()
{
  static GtkWidget* window = NULL;
  GtkWidget* bbox;
  GtkWidget* button;
	
  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window),
			  "Button Box Test");
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			&window);
    
    gtk_container_border_width (GTK_CONTAINER (window), 20);
    
    /* 
     *these 15 lines are a nice and easy example for GtkHButtonBox 
     */
    bbox = gtk_hbutton_box_new ();
    gtk_container_add (GTK_CONTAINER (window), bbox);
    gtk_widget_show (bbox);
    
    button = gtk_button_new_with_label ("Horizontal");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC(test_hbbox), 0);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    gtk_widget_show (button);
    
    button = gtk_button_new_with_label ("Vertical");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC(test_vbbox), 0);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    gtk_widget_show (button);
  }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget*
new_pixmap (char      *filename,
	    GdkWindow *window,
	    GdkColor  *background)
{
  GtkWidget *wpixmap;
  GdkPixmap *pixmap;
  GdkBitmap *mask;

  pixmap = gdk_pixmap_create_from_xpm (window, &mask,
				       background,
				       filename);
  wpixmap = gtk_pixmap_new (pixmap, mask);

  return wpixmap;
}

static void
set_toolbar_horizontal (GtkWidget *widget,
			gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_HORIZONTAL);
}

static void
set_toolbar_vertical (GtkWidget *widget,
		      gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_VERTICAL);
}

static void
set_toolbar_icons (GtkWidget *widget,
		   gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_ICONS);
}

static void
set_toolbar_text (GtkWidget *widget,
	          gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_TEXT);
}

static void
set_toolbar_both (GtkWidget *widget,
		  gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_BOTH);
}

static void
set_toolbar_small_space (GtkWidget *widget,
			 gpointer   data)
{
  gtk_toolbar_set_space_size (GTK_TOOLBAR (data), 5);
}

static void
set_toolbar_big_space (GtkWidget *widget,
		       gpointer   data)
{
  gtk_toolbar_set_space_size (GTK_TOOLBAR (data), 10);
}

static void
set_toolbar_enable (GtkWidget *widget,
		    gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), TRUE);
}

static void
set_toolbar_disable (GtkWidget *widget,
		     gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), FALSE);
}

static void
create_toolbar (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *toolbar;
  GtkWidget *entry;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Toolbar test");
      gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, TRUE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_container_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize (window);

      toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Horizontal", "Horizontal toolbar layout", "Toolbar/Horizontal",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_horizontal, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Vertical", "Vertical toolbar layout", "Toolbar/Vertical",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_vertical, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Icons", "Only show toolbar icons", "Toolbar/IconsOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_icons, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Text", "Only show toolbar text", "Toolbar/TextOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_text, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Both", "Show toolbar icons and text", "Toolbar/Both",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_both, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      entry = gtk_entry_new ();
      gtk_widget_show(entry);
      gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), entry, "This is an unusable GtkEntry ;)", "Hey don't click me!!!");

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Small", "Use small spaces", "Toolbar/Small",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_small_space, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Big", "Use big spaces", "Toolbar/Big",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_big_space, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Enable", "Enable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_enable, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Disable", "Disable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_disable, toolbar);

      gtk_container_add (GTK_CONTAINER (window), toolbar);
      gtk_widget_show (toolbar);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget*
make_toolbar (GtkWidget *window)
{
  GtkWidget *toolbar;

  if (!GTK_WIDGET_REALIZED (window))
    gtk_widget_realize (window);

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Horizontal", "Horizontal toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_horizontal, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Vertical", "Vertical toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_vertical, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Icons", "Only show toolbar icons", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_icons, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Text", "Only show toolbar text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_text, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Both", "Show toolbar icons and text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_both, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Small", "Use small spaces", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_small_space, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Big", "Use big spaces", "Toolbar/Big",
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_big_space, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Enable", "Enable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_enable, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Disable", "Disable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_disable, toolbar);

  return toolbar;
}

static guint statusbar_counter = 1;

static void
statusbar_push (GtkWidget *button,
		GtkStatusbar *statusbar)
{
  gchar text[1024];

  sprintf (text, "something %d", statusbar_counter++);

  gtk_statusbar_push (statusbar, 1, text);
}

static void
statusbar_pop (GtkWidget *button,
	       GtkStatusbar *statusbar)
{
  gtk_statusbar_pop (statusbar, 1);
}

static void
statusbar_steal (GtkWidget *button,
	         GtkStatusbar *statusbar)
{
  gtk_statusbar_remove (statusbar, 1, 4);
}

static void
statusbar_popped (GtkStatusbar  *statusbar,
		  guint          context_id,
		  const gchar	*text)
{
  if (!statusbar->messages)
    statusbar_counter = 1;
}

static void
statusbar_contexts (GtkWidget *button,
		    GtkStatusbar *statusbar)
{
  gchar *string;

  string = "any context";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "idle messages";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "some text";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse2";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
}

static void
statusbar_dump_stack (GtkWidget *button,
		      GtkStatusbar *statusbar)
{
  GSList *list;

  for (list = statusbar->messages; list; list = list->next)
    {
      GtkStatusbarMsg *msg;

      msg = list->data;
      g_print ("context_id: %d, message_id: %d, status_text: \"%s\"\n",
               msg->context_id,
               msg->message_id,
               msg->text);
    }
}

static void
create_statusbar ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *statusbar;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "statusbar");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);

      statusbar = gtk_statusbar_new ();
      gtk_box_pack_end (GTK_BOX (box1), statusbar, TRUE, TRUE, 0);
      gtk_widget_show (statusbar);
      gtk_signal_connect (GTK_OBJECT (statusbar),
			  "text_popped",
			  GTK_SIGNAL_FUNC (statusbar_popped),
			  NULL);

      button = gtk_widget_new (gtk_button_get_type (),
			       "GtkButton::label", "push something",
			       "GtkWidget::visible", TRUE,
			       "GtkWidget::parent", box2,
			       "GtkObject::signal::clicked", statusbar_push, statusbar,
			       NULL);

      button = gtk_widget_new (gtk_button_get_type (),
			       "GtkButton::label", "pop",
			       "GtkWidget::visible", TRUE,
			       "GtkWidget::parent", box2,
			       "GtkObject::signal::clicked", statusbar_pop, statusbar,
			       NULL);

      button = gtk_widget_new (gtk_button_get_type (),
			       "GtkButton::label", "steal #4",
			       "GtkWidget::visible", TRUE,
			       "GtkWidget::parent", box2,
			       "GtkObject::signal::clicked", statusbar_steal, statusbar,
			       NULL);

      button = gtk_widget_new (gtk_button_get_type (),
			       "GtkButton::label", "dump stack",
			       "GtkWidget::visible", TRUE,
			       "GtkWidget::parent", box2,
			       "GtkObject::signal::clicked", statusbar_dump_stack, statusbar,
			       NULL);

      button = gtk_widget_new (gtk_button_get_type (),
			       "GtkButton::label", "test contexts",
			       "GtkWidget::visible", TRUE,
			       "GtkWidget::parent", box2,
			       "GtkObject::signal::clicked", statusbar_contexts, statusbar,
			       NULL);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
handle_box_child_signal (GtkHandleBox *hb,
			 GtkWidget    *child,
			 const gchar  *action)
{
  printf ("%s: child <%s> %sed\n",
	  gtk_type_name (GTK_OBJECT_TYPE (hb)),
	  gtk_type_name (GTK_OBJECT_TYPE (child)),
	  action);
}

static void
cb_tree_destroy_event(GtkWidget* w)
{
  sTreeButtons* tree_buttons;

  /* free buttons structure associate at this tree */
  tree_buttons = gtk_object_get_user_data(GTK_OBJECT(w));
  free(tree_buttons);
}
static void
cb_add_new_item(GtkWidget* w, GtkTree* tree)
{
  sTreeButtons* tree_buttons;
  GList* selected_list;
  GtkWidget* selected_item;
  GtkWidget* subtree;
  GtkWidget* item_new;
  char buffer[255];

  tree_buttons = gtk_object_get_user_data(GTK_OBJECT(tree));

  selected_list = GTK_TREE_SELECTION(tree);

  if(selected_list == NULL)
    {
      /* there is no item in tree */
      subtree = GTK_WIDGET(tree);
    }
  else
    {
      /* list can have only one element */
      selected_item = GTK_WIDGET(selected_list->data);
      
      subtree = GTK_TREE_ITEM_SUBTREE(selected_item);

      if(subtree == NULL)
	{
	  /* current selected item have not subtree ... create it */
	  subtree = gtk_tree_new();
	  gtk_tree_item_set_subtree(GTK_TREE_ITEM(selected_item), 
				    subtree);
	}
    }

  /* at this point, we know which subtree will be used to add new item */
  /* create a new item */
  sprintf(buffer, "item add %d", tree_buttons->nb_item_add);
  item_new = gtk_tree_item_new_with_label(buffer);
  gtk_tree_append(GTK_TREE(subtree), item_new);
  gtk_widget_show(item_new);

  tree_buttons->nb_item_add++;
}

static void
cb_remove_item(GtkWidget*w, GtkTree* tree)
{
  GList* selected_list;
  GList* clear_list;
  
  selected_list = GTK_TREE_SELECTION(tree);

  clear_list = NULL;
    
  while (selected_list) 
    {
      clear_list = g_list_prepend (clear_list, selected_list->data);
      selected_list = selected_list->next;
    }
  
  clear_list = g_list_reverse (clear_list);
  gtk_tree_remove_items(tree, clear_list);

  g_list_free (clear_list);
}

static void
cb_tree_changed(GtkTree* tree)
{
  sTreeButtons* tree_buttons;
  GList* selected_list;
  guint nb_selected;

  tree_buttons = gtk_object_get_user_data(GTK_OBJECT(tree));

  selected_list = GTK_TREE_SELECTION(tree);
  nb_selected = g_list_length(selected_list);

  if(nb_selected == 0) 
    {
      if(tree->children == NULL)
	gtk_widget_set_sensitive(tree_buttons->add_button, TRUE);
      else
	gtk_widget_set_sensitive(tree_buttons->add_button, FALSE);
      gtk_widget_set_sensitive(tree_buttons->remove_button, FALSE);
    } 
  else 
    {
      gtk_widget_set_sensitive(tree_buttons->remove_button, TRUE);
      gtk_widget_set_sensitive(tree_buttons->add_button, (nb_selected == 1));
    }  
}

static void 
create_subtree(GtkWidget* item, guint level, guint nb_item_max, guint recursion_level_max)
{
  GtkWidget* item_subtree;
  GtkWidget* item_new;
  guint nb_item;
  char buffer[255];
  int no_root_item;

  if(level == recursion_level_max) return;

  if(level == -1)
    {
      /* query with no root item */
      level = 0;
      item_subtree = item;
      no_root_item = 1;
    }
  else
    {
      /* query with no root item */
      /* create subtree and associate it with current item */
      item_subtree = gtk_tree_new();
      no_root_item = 0;
    }
  
  for(nb_item = 0; nb_item < nb_item_max; nb_item++)
    {
      sprintf(buffer, "item %d-%d", level, nb_item);
      item_new = gtk_tree_item_new_with_label(buffer);
      gtk_tree_append(GTK_TREE(item_subtree), item_new);
      create_subtree(item_new, level+1, nb_item_max, recursion_level_max);
      gtk_widget_show(item_new);
    }

  if(!no_root_item)
    gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), item_subtree);
}

static void
create_tree_sample(guint selection_mode, 
		   guint draw_line, guint view_line, guint no_root_item,
		   guint nb_item_max, guint recursion_level_max) 
{
  GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* box2;
  GtkWidget* separator;
  GtkWidget* button;
  GtkWidget* scrolled_win;
  GtkWidget* root_tree;
  GtkWidget* root_item;
  sTreeButtons* tree_buttons;

  /* create tree buttons struct */
  if((tree_buttons = g_malloc(sizeof(sTreeButtons))) == NULL)
    {
      g_error("can't allocate memory for tree structure !\n");
      return;
    }
  tree_buttons->nb_item_add = 0;

  /* create top level window */
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Tree Sample");
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
		     (GtkSignalFunc) cb_tree_destroy_event, NULL);
  gtk_object_set_user_data(GTK_OBJECT(window), tree_buttons);

  box1 = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), box1);
  gtk_widget_show(box1);

  /* create tree box */
  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
  gtk_container_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  /* create scrolled window */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box2), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_set_usize (scrolled_win, 200, 200);
  gtk_widget_show (scrolled_win);
  
  /* create root tree widget */
  root_tree = gtk_tree_new();
  gtk_signal_connect(GTK_OBJECT(root_tree), "selection_changed",
		     (GtkSignalFunc)cb_tree_changed,
		     (gpointer)NULL);
  gtk_object_set_user_data(GTK_OBJECT(root_tree), tree_buttons);
  gtk_container_add(GTK_CONTAINER(scrolled_win), root_tree);
  gtk_tree_set_selection_mode(GTK_TREE(root_tree), selection_mode);
  gtk_tree_set_view_lines(GTK_TREE(root_tree), draw_line);
  gtk_tree_set_view_mode(GTK_TREE(root_tree), !view_line);
  gtk_widget_show(root_tree);

  if ( no_root_item )
    {
      /* set root tree to subtree function with root item variable */
      root_item = GTK_WIDGET(root_tree);
    }
  else
    {
      /* create root tree item widget */
      root_item = gtk_tree_item_new_with_label("root item");
      gtk_tree_append(GTK_TREE(root_tree), root_item);
      gtk_widget_show(root_item);
     }
  create_subtree(root_item, -no_root_item, nb_item_max, recursion_level_max);

  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_container_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  button = gtk_button_new_with_label("Add Item");
  gtk_widget_set_sensitive(button, FALSE);
  gtk_signal_connect(GTK_OBJECT (button), "clicked",
		     (GtkSignalFunc) cb_add_new_item, 
		     (gpointer)root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->add_button = button;

  button = gtk_button_new_with_label("Remove Item(s)");
  gtk_widget_set_sensitive(button, FALSE);
  gtk_signal_connect(GTK_OBJECT (button), "clicked",
		     (GtkSignalFunc) cb_remove_item, 
		     (gpointer)root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->remove_button = button;

  /* create separator */
  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, FALSE, 0);
  gtk_widget_show(separator);

  /* create button box */
  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_container_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  button = gtk_button_new_with_label("Close");
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_signal_connect_object(GTK_OBJECT (button), "clicked",
			    (GtkSignalFunc) gtk_widget_destroy, 
			    GTK_OBJECT(window));
  gtk_widget_show(button);

  gtk_widget_show(window);
}

static void
cb_create_tree(GtkWidget* w)
{
  guint selection_mode = GTK_SELECTION_SINGLE;
  guint view_line;
  guint draw_line;
  guint no_root_item;
  guint nb_item;
  guint recursion_level;

  /* get selection mode choice */
  if(GTK_TOGGLE_BUTTON(sTreeSampleSelection.single_button)->active)
    selection_mode = GTK_SELECTION_SINGLE;
  else
    if(GTK_TOGGLE_BUTTON(sTreeSampleSelection.browse_button)->active)
      selection_mode = GTK_SELECTION_BROWSE;
    else
      selection_mode = GTK_SELECTION_MULTIPLE;

  /* get options choice */
  draw_line = GTK_TOGGLE_BUTTON(sTreeSampleSelection.draw_line_button)->active;
  view_line = GTK_TOGGLE_BUTTON(sTreeSampleSelection.view_line_button)->active;
  no_root_item = GTK_TOGGLE_BUTTON(sTreeSampleSelection.no_root_item_button)->active;
    
  /* get levels */
  nb_item = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sTreeSampleSelection.nb_item_spinner));
  recursion_level = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sTreeSampleSelection.recursion_spinner));

  if (pow (nb_item, recursion_level) > 10000)
    {
      g_print ("%g total items? That will take a very long time. Try less\n",
	       pow (nb_item, recursion_level));
      return;
    }

  create_tree_sample(selection_mode, draw_line, view_line, no_root_item, nb_item, recursion_level);
}

void 
create_tree_mode_window(void)
{
  static GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* box2;
  GtkWidget* box3;
  GtkWidget* box4;
  GtkWidget* box5;
  GtkWidget* button;
  GtkWidget* frame;
  GtkWidget* separator;
  GtkWidget* label;
  GtkWidget* spinner;
  GtkAdjustment *adj;

  if (!window)
    {
      /* create toplevel window  */
      window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title(GTK_WINDOW(window), "Tree Mode Selection Window");
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);
      box1 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(window), box1);
      gtk_widget_show(box1);

  /* create upper box - selection box */
      box2 = gtk_vbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
      gtk_container_border_width(GTK_CONTAINER(box2), 5);
      gtk_widget_show(box2);

      box3 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box2), box3, TRUE, TRUE, 0);
      gtk_widget_show(box3);

      /* create selection mode frame */
      frame = gtk_frame_new("Selection Mode");
      gtk_box_pack_start(GTK_BOX(box3), frame, TRUE, TRUE, 0);
      gtk_widget_show(frame);

      box4 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_border_width(GTK_CONTAINER(box4), 5);
      gtk_widget_show(box4);

      /* create radio button */  
      button = gtk_radio_button_new_with_label(NULL, "SINGLE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_widget_show(button);
      sTreeSampleSelection.single_button = button;

      button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
					       "BROWSE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_widget_show(button);
      sTreeSampleSelection.browse_button = button;

      button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
					       "MULTIPLE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_widget_show(button);
      sTreeSampleSelection.multiple_button = button;

      sTreeSampleSelection.selection_mode_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));

      /* create option mode frame */
      frame = gtk_frame_new("Options");
      gtk_box_pack_start(GTK_BOX(box3), frame, TRUE, TRUE, 0);
      gtk_widget_show(frame);

      box4 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_border_width(GTK_CONTAINER(box4), 5);
      gtk_widget_show(box4);

      /* create check button */
      button = gtk_check_button_new_with_label("Draw line");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
      gtk_widget_show(button);
      sTreeSampleSelection.draw_line_button = button;
  
      button = gtk_check_button_new_with_label("View Line mode");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
      gtk_widget_show(button);
      sTreeSampleSelection.view_line_button = button;
  
      button = gtk_check_button_new_with_label("Without Root item");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_widget_show(button);
      sTreeSampleSelection.no_root_item_button = button;

      /* create recursion parameter */
      frame = gtk_frame_new("Size Parameters");
      gtk_box_pack_start(GTK_BOX(box2), frame, TRUE, TRUE, 0);
      gtk_widget_show(frame);

      box4 = gtk_hbox_new(FALSE, 5);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_border_width(GTK_CONTAINER(box4), 5);
      gtk_widget_show(box4);

      /* create number of item spin button */
      box5 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box4), box5, FALSE, FALSE, 0);
      gtk_widget_show(box5);

      label = gtk_label_new("Number of Item");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (box5), label, FALSE, TRUE, 0);
      gtk_widget_show(label);

      adj = (GtkAdjustment *) gtk_adjustment_new ((gfloat)DEFAULT_NUMBER_OF_ITEM, 1.0, 255.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (box5), spinner, FALSE, TRUE, 0);
      gtk_widget_show(spinner);
      sTreeSampleSelection.nb_item_spinner = spinner;
  
      /* create recursion level spin button */
      box5 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box4), box5, FALSE, FALSE, 0);
      gtk_widget_show(box5);

      label = gtk_label_new("Depth Level");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (box5), label, FALSE, TRUE, 0);
      gtk_widget_show(label);

      adj = (GtkAdjustment *) gtk_adjustment_new ((gfloat)DEFAULT_RECURSION_LEVEL, 0.0, 255.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (box5), spinner, FALSE, TRUE, 0);
      gtk_widget_show(spinner);
      sTreeSampleSelection.recursion_spinner = spinner;
  
      /* create horizontal separator */
      separator = gtk_hseparator_new();
      gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, FALSE, 0);
      gtk_widget_show(separator);

      /* create bottom button box */
      box2 = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
      gtk_container_border_width(GTK_CONTAINER(box2), 5);
      gtk_widget_show(box2);

      button = gtk_button_new_with_label("Create Tree Sample");
      gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
      gtk_signal_connect(GTK_OBJECT (button), "clicked",
			 (GtkSignalFunc) cb_create_tree, NULL);
      gtk_widget_show(button);

      button = gtk_button_new_with_label("Close");
      gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_widget_show(button);
  
    }
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/* end of function used by tree demos */

static void
create_handle_box ()
{
  static GtkWidget* window = NULL;
  GtkWidget *handle_box;
  GtkWidget *handle_box2;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *toolbar;
  GtkWidget *label;
  GtkWidget *separator;
	
  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window),
			  "Handle Box Test");
    gtk_window_set_policy (GTK_WINDOW (window),
			   TRUE,
			   TRUE,
			   FALSE);
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			&window);
    
    gtk_container_border_width (GTK_CONTAINER (window), 20);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    label = gtk_label_new ("Above");
    gtk_container_add (GTK_CONTAINER (vbox), label);
    gtk_widget_show (label);

    separator = gtk_hseparator_new ();
    gtk_container_add (GTK_CONTAINER (vbox), separator);
    gtk_widget_show (separator);
    
    hbox = gtk_hbox_new (FALSE, 10);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    gtk_widget_show (hbox);

    separator = gtk_hseparator_new ();
    gtk_container_add (GTK_CONTAINER (vbox), separator);
    gtk_widget_show (separator);

    label = gtk_label_new ("Below");
    gtk_container_add (GTK_CONTAINER (vbox), label);
    gtk_widget_show (label);

    handle_box = gtk_handle_box_new ();
    gtk_container_add (GTK_CONTAINER (hbox), handle_box);
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_attached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"attached");
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_detached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"detached");
    gtk_widget_show (handle_box);

    toolbar = make_toolbar (window);
    gtk_container_add (GTK_CONTAINER (handle_box), toolbar);
    gtk_widget_show (toolbar);

    handle_box = gtk_handle_box_new ();
    gtk_container_add (GTK_CONTAINER (hbox), handle_box);
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_attached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"attached");
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_detached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"detached");
    gtk_widget_show (handle_box);

    handle_box2 = gtk_handle_box_new ();
    gtk_container_add (GTK_CONTAINER (handle_box), handle_box2);
    gtk_signal_connect (GTK_OBJECT (handle_box2),
			"child_attached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"attached");
    gtk_signal_connect (GTK_OBJECT (handle_box2),
			"child_detached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"detached");
    gtk_widget_show (handle_box2);

    label = gtk_label_new ("Fooo!");
    gtk_container_add (GTK_CONTAINER (handle_box2), label);
    gtk_widget_show (label);
  }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


static void
reparent_label (GtkWidget *widget,
		GtkWidget *new_parent)
{
  GtkWidget *label;

  label = gtk_object_get_user_data (GTK_OBJECT (widget));

  gtk_widget_reparent (label, new_parent);
}

static void
set_parent_signal (GtkWidget *child,
		   GtkWidget *old_parent,
		   gpointer   func_data)
{
  g_print ("set_parent for \"%s\": new parent: \"%s\", old parent: \"%s\", data: %d\n",
	   gtk_type_name (GTK_OBJECT_TYPE (child)),
	   child->parent ? gtk_type_name (GTK_OBJECT_TYPE (child->parent)) : "NULL",
	   old_parent ? gtk_type_name (GTK_OBJECT_TYPE (old_parent)) : "NULL",
	   (gint) func_data);
}

static void
create_reparent ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "buttons");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      label = gtk_label_new ("Hello World");

      frame = gtk_frame_new ("Frame 1");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);
      gtk_widget_show (frame);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);
      gtk_widget_show (box3);

      button = gtk_button_new_with_label ("switch");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(reparent_label),
			  box3);
      gtk_object_set_user_data (GTK_OBJECT (button), label);
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      gtk_box_pack_start (GTK_BOX (box3), label, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (label),
			  "parent_set",
			  GTK_SIGNAL_FUNC (set_parent_signal),
			  (GtkObject*) 42);
      gtk_widget_show (label);


      frame = gtk_frame_new ("Frame 2");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);
      gtk_widget_show (frame);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);
      gtk_widget_show (box3);

      button = gtk_button_new_with_label ("switch");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(reparent_label),
			  box3);
      gtk_object_set_user_data (GTK_OBJECT (button), label);
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);
      gtk_widget_show (button);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
create_pixmap ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &window);

      gtk_window_set_title (GTK_WINDOW (window), "pixmap");
      gtk_container_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize(window);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      style=gtk_widget_get_style(button);

      pixmap = gdk_pixmap_create_from_xpm (window->window, &mask, 
					   &style->bg[GTK_STATE_NORMAL],
					   "test.xpm");
      pixmapwid = gtk_pixmap_new (pixmap, mask);

      label = gtk_label_new ("Pixmap\ntest");
      box3 = gtk_hbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixmapwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);
      gtk_widget_show (pixmapwid);
      gtk_widget_show (label);
      gtk_widget_show (box3);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
tips_query_widget_entered (GtkTipsQuery   *tips_query,
			   GtkWidget      *widget,
			   const gchar    *tip_text,
			   const gchar    *tip_private,
			   GtkWidget	  *toggle)
{
  if (GTK_TOGGLE_BUTTON (toggle)->active)
    {
      gtk_label_set (GTK_LABEL (tips_query), tip_text ? "There is a Tip!" : "There is no Tip!");
      /* don't let GtkTipsQuery reset it's label */
      gtk_signal_emit_stop_by_name (GTK_OBJECT (tips_query), "widget_entered");
    }
}

static gint
tips_query_widget_selected (GtkWidget      *tips_query,
			    GtkWidget      *widget,
			    const gchar    *tip_text,
			    const gchar    *tip_private,
			    GdkEventButton *event,
			    gpointer        func_data)
{
  if (widget)
    g_print ("Help \"%s\" requested for <%s>\n",
	     tip_private ? tip_private : "None",
	     gtk_type_name (GTK_OBJECT_TYPE (widget)));

  return TRUE;
}

static void
create_tooltips ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *toggle;
  GtkWidget *frame;
  GtkWidget *tips_query;
  GtkWidget *separator;
  GtkTooltips *tooltips;

  if (!window)
    {
      window =
	gtk_widget_new (gtk_window_get_type (),
			"GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			"GtkContainer::border_width", 0,
			"GtkWindow::title", "Tooltips",
			"GtkWindow::allow_shrink", TRUE,
			"GtkWindow::allow_grow", FALSE,
			"GtkWindow::auto_shrink", TRUE,
			"GtkWidget::width", 200,
			NULL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC (destroy_tooltips),
                          &window);

      tooltips=gtk_tooltips_new();
      gtk_object_set_data (GTK_OBJECT (window), "tooltips", tooltips);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      gtk_tooltips_set_tip (tooltips,button,"This is button 1", "ContextHelp/buttons/1");

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      gtk_tooltips_set_tip (tooltips,
			    button,
			    "This is button 2. This is also a really long tooltip which probably won't fit on a single line and will therefore need to be wrapped. Hopefully the wrapping will work correctly.",
			    "ContextHelp/buttons/2_long");

      toggle = gtk_toggle_button_new_with_label ("Override TipsQuery Label");
      gtk_box_pack_start (GTK_BOX (box2), toggle, TRUE, TRUE, 0);
      gtk_widget_show (toggle);

      gtk_tooltips_set_tip (tooltips, toggle, "Toggle TipsQuery view.", "Hi msw! ;)");

      box3 =
	gtk_widget_new (gtk_vbox_get_type (),
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 5,
			"GtkWidget::visible", TRUE,
			NULL);

      tips_query = gtk_tips_query_new ();

      button =
	gtk_widget_new (gtk_button_get_type (),
			"GtkButton::label", "[?]",
			"GtkWidget::visible", TRUE,
			"GtkWidget::parent", box3,
			"GtkObject::object_signal::clicked", gtk_tips_query_start_query, tips_query,
			NULL);
      gtk_box_set_child_packing (GTK_BOX (box3), button, FALSE, FALSE, 0, GTK_PACK_START);
      gtk_tooltips_set_tip (tooltips,
			    button,
			    "Start the Tooltips Inspector",
			    "ContextHelp/buttons/?");
      
      
      gtk_widget_set (tips_query,
		      "GtkWidget::visible", TRUE,
		      "GtkWidget::parent", box3,
		      "GtkTipsQuery::caller", button,
		      "GtkObject::signal::widget_entered", tips_query_widget_entered, toggle,
		      "GtkObject::signal::widget_selected", tips_query_widget_selected, NULL,
		      NULL);
      
      frame =
	gtk_widget_new (gtk_frame_get_type (),
			"GtkFrame::label", "ToolTips Inspector",
			"GtkFrame::label_xalign", (double) 0.5,
			"GtkContainer::border_width", 0,
			"GtkWidget::visible", TRUE,
			"GtkWidget::parent", box2,
			"GtkContainer::child", box3,
			NULL);
      gtk_box_set_child_packing (GTK_BOX (box2), frame, TRUE, TRUE, 10, GTK_PACK_START);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      gtk_tooltips_set_tip (tooltips, button, "Push this button to close window", "ContextHelp/buttons/Close");
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget*
create_menu (int depth)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GSList *group;
  char buf[32];
  int i, j;

  if (depth < 1)
    return NULL;

  menu = gtk_menu_new ();
  group = NULL;

  for (i = 0, j = 1; i < 5; i++, j++)
    {
      sprintf (buf, "item %2d - %d", depth, j);
      menuitem = gtk_radio_menu_item_new_with_label (group, buf);
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
      if (depth % 2)
	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menuitem), TRUE);
      gtk_menu_append (GTK_MENU (menu), menuitem);
      gtk_widget_show (menuitem);
      if (i == 3)
	gtk_widget_set_sensitive (menuitem, FALSE);

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (depth - 1));
    }

  return menu;
}

static void
create_menus ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *menu;
  GtkWidget *menubar;
  GtkWidget *menuitem;
  GtkWidget *optionmenu;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC (gtk_true),
			  NULL);

      gtk_window_set_title (GTK_WINDOW (window), "menus");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);

      menu = create_menu (2);

      menuitem = gtk_menu_item_new_with_label ("test\nline2");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_label ("foo");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (3));
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_label ("bar");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (4));
      gtk_menu_item_right_justify (GTK_MENU_ITEM (menuitem));
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      optionmenu = gtk_option_menu_new ();
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), create_menu (1));
      gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 4);
      gtk_box_pack_start (GTK_BOX (box2), optionmenu, TRUE, TRUE, 0);
      gtk_widget_show (optionmenu);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkScrolledWindow
 */
static void
create_scrolled_windows ()
{
  static GtkWidget *window;
  GtkWidget *scrolled_window;
  GtkWidget *table;
  GtkWidget *button;
  char buffer[32];
  int i, j;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "dialog");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_border_width (GTK_CONTAINER (scrolled_window), 10);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  scrolled_window, TRUE, TRUE, 0);
      gtk_widget_show (scrolled_window);

      table = gtk_table_new (20, 20, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 10);
      gtk_table_set_col_spacings (GTK_TABLE (table), 10);
      gtk_container_add (GTK_CONTAINER (scrolled_window), table);
      gtk_widget_show (table);

      for (i = 0; i < 20; i++)
	for (j = 0; j < 20; j++)
	  {
	    sprintf (buffer, "button (%d,%d)\n", i, j);
	    button = gtk_toggle_button_new_with_label (buffer);
	    gtk_table_attach_defaults (GTK_TABLE (table), button,
				       i, i+1, j, j+1);
	    gtk_widget_show (button);
	  }


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkEntry
 */

static void
entry_toggle_editable (GtkWidget *checkbutton,
		       GtkWidget *entry)
{
   gtk_entry_set_editable(GTK_ENTRY(entry),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
create_entry ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *editable_check;
  GtkWidget *entry, *cb;
  GtkWidget *button;
  GtkWidget *separator;
  GList *cbitems = NULL;

  if (!window)
    {
      cbitems = g_list_append(cbitems, "item0");
      cbitems = g_list_append(cbitems, "item1 item1");
      cbitems = g_list_append(cbitems, "item2 item2 item2");
      cbitems = g_list_append(cbitems, "item3 item3 item3 item3");
      cbitems = g_list_append(cbitems, "item4 item4 item4 item4 item4");
      cbitems = g_list_append(cbitems, "item5 item5 item5 item5 item5 item5");
      cbitems = g_list_append(cbitems, "item6 item6 item6 item6 item6");
      cbitems = g_list_append(cbitems, "item7 item7 item7 item7");
      cbitems = g_list_append(cbitems, "item8 item8 item8");
      cbitems = g_list_append(cbitems, "item9 item9");

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "entry");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "hello world");
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
      gtk_box_pack_start (GTK_BOX (box2), entry, TRUE, TRUE, 0);
      gtk_widget_show (entry);

      cb = gtk_combo_new ();
      gtk_combo_set_popdown_strings (GTK_COMBO (cb), cbitems);
      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO(cb)->entry), "hello world");
      gtk_editable_select_region (GTK_EDITABLE (GTK_COMBO(cb)->entry),
				  0, -1);
      gtk_box_pack_start (GTK_BOX (box2), cb, TRUE, TRUE, 0);
      gtk_widget_show (cb);

      editable_check = gtk_check_button_new_with_label("Editable");
      gtk_box_pack_start (GTK_BOX (box2), editable_check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT(editable_check), "toggled",
			  GTK_SIGNAL_FUNC(entry_toggle_editable), entry);
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editable_check), TRUE);
      gtk_widget_show (editable_check);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkSpinButton
 */

static GtkWidget *spinner1;

static void
toggle_snap (GtkWidget *widget, GtkSpinButton *spin)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    gtk_spin_button_set_update_policy (spin, GTK_UPDATE_ALWAYS 
				       | GTK_UPDATE_SNAP_TO_TICKS);
  else
    gtk_spin_button_set_update_policy (spin, GTK_UPDATE_ALWAYS);
}

static void
toggle_numeric (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_numeric (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
change_digits (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinner1),
			      gtk_spin_button_get_value_as_int (spin));
}

static void
get_value (GtkWidget *widget, gint data)
{
  gchar buf[32];
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (spinner1);
  label = GTK_LABEL (gtk_object_get_user_data (GTK_OBJECT (widget)));
  if (data == 1)
    sprintf (buf, "%d", gtk_spin_button_get_value_as_int (spin));
  else
    sprintf (buf, "%0.*f", spin->digits,
	     gtk_spin_button_get_value_as_float (spin));
  gtk_label_set (label, buf);
}

static void
create_spins ()
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *spinner2;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *val_label;
  GtkAdjustment *adj;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "GtkSpinButton");
      
      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (main_vbox), 10);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);
      
      frame = gtk_frame_new ("Not accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
      
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      /* Day, month, year spinners */
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Day :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 1.0, 31.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Month :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 1.0, 12.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);

      label = gtk_label_new ("Year :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (1998.0, 0.0, 2100.0, 
						  1.0, 100.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_widget_set_usize (spinner, 55, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Value :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (0.0, -10000.0, 10000.0,
						  0.5, 100.0, 0.0);
      spinner1 = gtk_spin_button_new (adj, 1.0, 2);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
      gtk_widget_set_usize (spinner1, 100, 0);
      gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinner1),
					 GTK_UPDATE_ALWAYS);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner1, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);

      label = gtk_label_new ("Digits :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (2, 1, 5, 1, 1, 0);
      spinner2 = gtk_spin_button_new (adj, 0.0, 0);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner2), TRUE);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (change_digits),
			  (gpointer) spinner2);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner2, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

      button = gtk_check_button_new_with_label ("Snap to 0.5-ticks");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (toggle_snap),
			  spinner1);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);

      button = gtk_check_button_new_with_label ("Numeric only input mode");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (toggle_numeric),
			  spinner1);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);

      val_label = gtk_label_new ("");

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("Value as Int");
      gtk_object_set_user_data (GTK_OBJECT (button), val_label);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (get_value),
			  (gpointer) 1);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      button = gtk_button_new_with_label ("Value as Float");
      gtk_object_set_user_data (GTK_OBJECT (button), val_label);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (get_value),
			  (gpointer) 2);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      gtk_box_pack_start (GTK_BOX (vbox), val_label, TRUE, TRUE, 0);
      gtk_label_set (GTK_LABEL (val_label), "0");

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);
  
      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Cursors
 */

static gint
cursor_expose_event (GtkWidget *widget,
		     GdkEvent  *event,
		     gpointer   user_data)
{
  GtkDrawingArea *darea;
  GdkDrawable *drawable;
  GdkGC *black_gc;
  GdkGC *gray_gc;
  GdkGC *white_gc;
  guint max_width;
  guint max_height;

  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GTK_IS_DRAWING_AREA (widget), TRUE);

  darea = GTK_DRAWING_AREA (widget);
  drawable = widget->window;
  white_gc = widget->style->white_gc;
  gray_gc = widget->style->bg_gc[GTK_STATE_NORMAL];
  black_gc = widget->style->black_gc;
  max_width = widget->allocation.width;
  max_height = widget->allocation.height;

  gdk_draw_rectangle (drawable, white_gc,
		      TRUE,
		      0,
		      0,
		      max_width,
		      max_height / 2);

  gdk_draw_rectangle (drawable, black_gc,
		      TRUE,
		      0,
		      max_height / 2,
		      max_width,
		      max_height / 2);

  gdk_draw_rectangle (drawable, gray_gc,
		      TRUE,
		      max_width / 3,
		      max_height / 3,
		      max_width / 3,
		      max_height / 3);

  return TRUE;
}

static void
set_cursor (GtkWidget *spinner,
	    GtkWidget *widget)
{
  guint c;
  GdkCursor *cursor;

  c = CLAMP (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner)), 0, 152);
  c &= 0xfe;

  cursor = gdk_cursor_new (c);
  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_destroy (cursor);
}

static gint
cursor_event (GtkWidget          *widget,
	      GdkEvent           *event,
	      GtkSpinButton	 *spinner)
{
  if ((event->type == GDK_BUTTON_PRESS) &&
      ((event->button.button == 1) ||
       (event->button.button == 3)))
    {
      gtk_spin_button_spin (spinner,
			    event->button.button == 1 ? GTK_ARROW_UP : GTK_ARROW_DOWN,
			    spinner->adjustment->step_increment);
      return TRUE;
    }

  return FALSE;
}

static void
create_cursors ()
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *darea;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *any;
  GtkAdjustment *adj;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "Cursors");
      
      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	gtk_widget_new (gtk_vbox_get_type (),
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", main_vbox,
			"GtkWidget::visible", TRUE,
			NULL);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
      
      label = gtk_label_new ("Cursor Value:");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0,
						  0, 152,
						  2,
						  10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);

      frame =
	gtk_widget_new (gtk_frame_get_type (),
			"GtkFrame::shadow", GTK_SHADOW_ETCHED_IN,
			"GtkFrame::label_xalign", 0.5,
			"GtkFrame::label", "Cursor Area",
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", vbox,
			"GtkWidget::visible", TRUE,
			NULL);

      darea = gtk_drawing_area_new ();
      gtk_widget_set_usize (darea, 80, 80);
      gtk_container_add (GTK_CONTAINER (frame), darea);
      gtk_signal_connect (GTK_OBJECT (darea),
			  "expose_event",
			  GTK_SIGNAL_FUNC (cursor_expose_event),
			  NULL);
      gtk_widget_set_events (darea, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
      gtk_signal_connect (GTK_OBJECT (darea),
			  "button_press_event",
			  GTK_SIGNAL_FUNC (cursor_event),
			  spinner);
      gtk_widget_show (darea);

      gtk_signal_connect (GTK_OBJECT (spinner), "changed",
			  GTK_SIGNAL_FUNC (set_cursor),
			  darea);

      any =
	gtk_widget_new (gtk_hseparator_get_type (),
			"GtkWidget::visible", TRUE,
			NULL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);
  
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (hbox), 10);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      gtk_widget_show_all (window);

      set_cursor (spinner, darea);
    }
  else
    gtk_widget_destroy (window);
}

/*
 * GtkList
 */
static void
list_add (GtkWidget *widget,
	  GtkWidget *list)
{
  static int i = 1;
  gchar buffer[64];
  GtkWidget *list_item;

  sprintf (buffer, "added item %d", i++);
  list_item = gtk_list_item_new_with_label (buffer);
  gtk_widget_show (list_item);
  gtk_container_add (GTK_CONTAINER (list), list_item);
}

static void
list_remove (GtkWidget *widget,
	     GtkWidget *list)
{
  GList *tmp_list;
  GList *clear_list;

  tmp_list = GTK_LIST (list)->selection;
  clear_list = NULL;

  while (tmp_list)
    {
      clear_list = g_list_prepend (clear_list, tmp_list->data);
      tmp_list = tmp_list->next;
    }

  clear_list = g_list_reverse (clear_list);

  gtk_list_remove_items (GTK_LIST (list), clear_list);

  g_list_free (clear_list);
}

static void
list_clear (GtkWidget *widget,
	    GtkWidget *list)
{
  gtk_list_clear_items (GTK_LIST (list), 3 - 1, 5 - 1);
}

static void
create_list ()
{
  static GtkWidget *window = NULL;
  static char *list_items[] =
  {
    "hello",
    "world",
    "blah",
    "foo",
    "bar",
    "argh",
    "spencer",
    "is a",
    "wussy",
    "programmer",
  };
  static int nlist_items = sizeof (list_items) / sizeof (list_items[0]);

  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *scrolled_win;
  GtkWidget *list;
  GtkWidget *list_item;
  GtkWidget *button;
  GtkWidget *separator;
  int i;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "list");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (box2), scrolled_win, TRUE, TRUE, 0);
      gtk_widget_show (scrolled_win);

      list = gtk_list_new ();
      gtk_list_set_selection_mode (GTK_LIST (list), GTK_SELECTION_MULTIPLE);
      gtk_list_set_selection_mode (GTK_LIST (list), GTK_SELECTION_BROWSE);
      gtk_container_add (GTK_CONTAINER (scrolled_win), list);
      gtk_widget_show (list);

      for (i = 0; i < nlist_items; i++)
	{
	  list_item = gtk_list_item_new_with_label (list_items[i]);
	  gtk_container_add (GTK_CONTAINER (list), list_item);
	  gtk_widget_show (list_item);
	}

      button = gtk_button_new_with_label ("add");
      GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(list_add),
			  list);
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("clear items 3 - 5");
      GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(list_clear),
			  list);
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("remove");
      GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(list_remove),
			  list);
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, TRUE, 0);
      gtk_widget_show (button);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkCList
 */
#define TESTGTK_CLIST_COLUMNS 7
static gint clist_rows = 0;
static gint clist_selected_row = 0;

static void
add1000_clist (GtkWidget *widget, gpointer data)
{
  gint i, row;
  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];
  GdkBitmap *mask;
  GdkPixmap *pixmap;
  
  pixmap = gdk_pixmap_create_from_xpm (GTK_CLIST (data)->clist_window, 
				       &mask, 
				       &GTK_WIDGET (data)->style->white,
				       "test.xpm");

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      texts[i] = text[i];
      sprintf (text[i], "Column %d", i);
    }
  
  texts[3] = NULL;
  sprintf (text[1], "Right");
  sprintf (text[2], "Center");
  
  gtk_clist_freeze (GTK_CLIST (data));
  for (i = 0; i < 1000; i++)
    {
      sprintf (text[0], "Row %d", clist_rows++);
      row = gtk_clist_append (GTK_CLIST (data), texts);
      gtk_clist_set_pixtext (GTK_CLIST (data), row, 3, "Testing", 5, pixmap, mask);
    }
  gtk_clist_thaw (GTK_CLIST (data));

  gdk_pixmap_unref (pixmap);
  gdk_bitmap_unref (mask);
}

static void
add10000_clist (GtkWidget *widget, gpointer data)
{
  gint i;
  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      texts[i] = text[i];
      sprintf (text[i], "Column %d", i);
    }
  
  sprintf (text[1], "Right");
  sprintf (text[2], "Center");
  
  gtk_clist_freeze (GTK_CLIST (data));
  for (i = 0; i < 10000; i++)
    {
      sprintf (text[0], "Row %d", clist_rows++);
      gtk_clist_append (GTK_CLIST (data), texts);
    }
  gtk_clist_thaw (GTK_CLIST (data));

}

void
clear_clist (GtkWidget *widget, gpointer data)
{
  gtk_clist_clear (GTK_CLIST (data));
  clist_rows = 0;
}

void
remove_row_clist (GtkWidget *widget, gpointer data)
{
  gtk_clist_remove (GTK_CLIST (data), clist_selected_row);
  clist_rows--;
}

void
show_titles_clist (GtkWidget *widget, gpointer data)
{
  gtk_clist_column_titles_show (GTK_CLIST (data));
}

void
hide_titles_clist (GtkWidget *widget, gpointer data)
{
  gtk_clist_column_titles_hide (GTK_CLIST (data));
}

void
select_clist (GtkWidget *widget,
	      gint row, 
	      gint column, 
	      GdkEventButton * bevent)
{
  gint i;
  guint8 spacing;
  gchar *text;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GList *list;

  g_print ("GtkCList Selection: row %d column %d button %d\n", 
	   row, column, bevent ? bevent->button : 0);

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      switch (gtk_clist_get_cell_type (GTK_CLIST (widget), row, i))
	{
	case GTK_CELL_TEXT:
	  g_print ("CELL %d GTK_CELL_TEXT\n", i);
	  gtk_clist_get_text (GTK_CLIST (widget), row, i, &text);
	  g_print ("TEXT: %s\n", text);
	  break;

	case GTK_CELL_PIXMAP:
	  g_print ("CELL %d GTK_CELL_PIXMAP\n", i);
	  gtk_clist_get_pixmap (GTK_CLIST (widget), row, i, &pixmap, &mask);
	  g_print ("PIXMAP: %d\n", (int) pixmap);
	  g_print ("MASK: %d\n", (int) mask);
	  break;

	case GTK_CELL_PIXTEXT:
	  g_print ("CELL %d GTK_CELL_PIXTEXT\n", i);
	  gtk_clist_get_pixtext (GTK_CLIST (widget), row, i, &text, &spacing, &pixmap, &mask);
	  g_print ("TEXT: %s\n", text);
	  g_print ("SPACING: %d\n", spacing);
	  g_print ("PIXMAP: %d\n", (int) pixmap);
	  g_print ("MASK: %d\n", (int) mask);
	  break;

	default:
	  break;
	}
    }

  /* print selections list */
  g_print ("\nSelected Rows:");
  list = GTK_CLIST (widget)->selection;
  while (list)
    {
      g_print (" %d ", (gint) list->data);
      list = list->next;
    }

  g_print ("\n\n\n");

  clist_selected_row = row;
}

void
unselect_clist (GtkWidget *widget,
		gint row, 
		gint column, 
		GdkEventButton * bevent)
{
  gint i;
  guint8 spacing;
  gchar *text;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GList *list;

  g_print ("GtkCList Unselection: row %d column %d button %d\n", 
	   row, column, bevent ? bevent->button : 0);

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      switch (gtk_clist_get_cell_type (GTK_CLIST (widget), row, i))
	{
	case GTK_CELL_TEXT:
	  g_print ("CELL %d GTK_CELL_TEXT\n", i);
	  gtk_clist_get_text (GTK_CLIST (widget), row, i, &text);
	  g_print ("TEXT: %s\n", text);
	  break;

	case GTK_CELL_PIXMAP:
	  g_print ("CELL %d GTK_CELL_PIXMAP\n", i);
	  gtk_clist_get_pixmap (GTK_CLIST (widget), row, i, &pixmap, &mask);
	  g_print ("PIXMAP: %d\n", (int) pixmap);
	  g_print ("MASK: %d\n", (int) mask);
	  break;

	case GTK_CELL_PIXTEXT:
	  g_print ("CELL %d GTK_CELL_PIXTEXT\n", i);
	  gtk_clist_get_pixtext (GTK_CLIST (widget), row, i, &text, &spacing, &pixmap, &mask);
	  g_print ("TEXT: %s\n", text);
	  g_print ("SPACING: %d\n", spacing);
	  g_print ("PIXMAP: %d\n", (int) pixmap);
	  g_print ("MASK: %d\n", (int) mask);
	  break;

	default:
	  break;
	}
    }

  /* print selections list */
  g_print ("\nSelected Rows:");
  list = GTK_CLIST (widget)->selection;
  while (list)
    {
      g_print (" %d ", (gint) list->data);
      list = list->next;
    }

  g_print ("\n\n\n");

  clist_selected_row = row;
}

static void
insert_row_clist (GtkWidget *widget, gpointer data)
{
  static char *text[] =
  {
    "This",
    "is",
    "a",
    "inserted",
    "row",
    "la la la la la",
    "la la la la"
  };

  gtk_clist_insert (GTK_CLIST (data), clist_selected_row, text);
  clist_rows++;
}

static void
clist_warning_test (GtkWidget *button,
		    GtkWidget *clist)
{
  GtkWidget *child;
  static gboolean add_remove = FALSE;

  add_remove = !add_remove;

  child = gtk_label_new ("Test");
  gtk_widget_ref (child);
  gtk_object_sink (child);

  if (add_remove)
    gtk_container_add (GTK_CONTAINER (clist), child);
  else
    {
      child->parent = clist;
      gtk_container_remove (GTK_CONTAINER (clist), child);
      child->parent = NULL;
    }

  gtk_widget_destroy (child);
  gtk_widget_unref (child);
}

static void
create_clist ()
{
  gint i;
  static GtkWidget *window = NULL;

  static char *titles[] =
  {
    "Title 0",
    "Title 1",
    "Title 2",
    "Title 3",
    "Title 4",
    "Title 5",
    "Title 6"
  };

  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];

  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *clist;
  GtkWidget *button;
  GtkWidget *separator;


  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "clist");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_hbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, FALSE, 0);
      gtk_widget_show (box2);

      /* create GtkCList here so we have a pointer to throw at the 
       * button callbacks -- more is done with it later */
      clist = gtk_clist_new_with_titles (TESTGTK_CLIST_COLUMNS, titles);
      /*clist = gtk_clist_new (TESTGTK_CLIST_COLUMNS);*/

      /* control buttons */
      button = gtk_button_new_with_label ("Add 1,000 Rows With Pixmaps");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) add1000_clist,
                          (gpointer) clist);

      gtk_widget_show (button);


      button = gtk_button_new_with_label ("Add 10,000 Rows");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) add10000_clist,
                          (gpointer) clist);

      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Clear List");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) clear_clist,
                          (gpointer) clist);

      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Remove Row");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) remove_row_clist,
                          (gpointer) clist);

      gtk_widget_show (button);

      /* second layer of buttons */
      box2 = gtk_hbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, FALSE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("Insert Row");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) insert_row_clist,
                          (gpointer) clist);

      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Show Title Buttons");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) show_titles_clist,
                          (gpointer) clist);

      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Hide Title Buttons");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) hide_titles_clist,
                          (gpointer) clist);

      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Warning Test");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          (GtkSignalFunc) clist_warning_test,
                          (gpointer) clist);

      gtk_widget_show (button);

      /* vbox for the list itself */
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);

      /* 
       * the rest of the clist configuration
       */
      gtk_clist_set_row_height (GTK_CLIST (clist), 20);
      
      gtk_signal_connect (GTK_OBJECT (clist), 
			  "select_row",
			  (GtkSignalFunc) select_clist, 
			  NULL);

      gtk_signal_connect (GTK_OBJECT (clist), 
			  "unselect_row",
			  (GtkSignalFunc) unselect_clist, 
			  NULL);

      gtk_clist_set_column_width (GTK_CLIST (clist), 0, 100);

      for (i = 1; i < TESTGTK_CLIST_COLUMNS; i++)
	gtk_clist_set_column_width (GTK_CLIST (clist), i, 80);

      gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
      gtk_clist_set_policy (GTK_CLIST (clist), 
			    GTK_POLICY_AUTOMATIC,
			    GTK_POLICY_AUTOMATIC);

      gtk_clist_set_policy (GTK_CLIST (clist), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

      gtk_clist_set_column_justification (GTK_CLIST (clist), 1, GTK_JUSTIFY_RIGHT);
      gtk_clist_set_column_justification (GTK_CLIST (clist), 2, GTK_JUSTIFY_CENTER);
      
      for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
	{
	  texts[i] = text[i];
	  sprintf (text[i], "Column %d", i);
	}

      sprintf (text[1], "Right");
      sprintf (text[2], "Center");

      for (i = 0; i < 100; i++)
	{
	  sprintf (text[0], "Row %d", clist_rows++);
	  gtk_clist_append (GTK_CLIST (clist), texts);
	}

      gtk_container_border_width (GTK_CONTAINER (clist), 5);
      gtk_box_pack_start (GTK_BOX (box2), clist, TRUE, TRUE, 0);
      gtk_widget_show (clist);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));

      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    {
      clist_rows = 0;
      gtk_widget_destroy (window);
    }

}

/*
 * GtkColorSelect
 */
void
color_selection_ok (GtkWidget               *w,
                    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;
  gdouble color[4];

  colorsel=GTK_COLOR_SELECTION(cs->colorsel);

  gtk_color_selection_get_color(colorsel,color);
  gtk_color_selection_set_color(colorsel,color);
}

void
color_selection_changed (GtkWidget *w,
                         GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;
  gdouble color[4];

  colorsel=GTK_COLOR_SELECTION(cs->colorsel);
  gtk_color_selection_get_color(colorsel,color);
}

void
create_color_selection ()
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      gtk_preview_set_install_cmap (TRUE);
      gtk_widget_push_visual (gtk_preview_get_visual ());
      gtk_widget_push_colormap (gtk_preview_get_cmap ());

      window = gtk_color_selection_dialog_new ("color selection dialog");

      gtk_color_selection_set_opacity (
        GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (window)->colorsel),
	TRUE);

      gtk_color_selection_set_update_policy(
        GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (window)->colorsel),
	GTK_UPDATE_CONTINUOUS);

      gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &window);

      gtk_signal_connect (
	GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (window)->colorsel),
	"color_changed",
	GTK_SIGNAL_FUNC(color_selection_changed),
	window);

      gtk_signal_connect (
	GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (window)->ok_button),
	"clicked",
	GTK_SIGNAL_FUNC(color_selection_ok),
	window);

      gtk_signal_connect_object (
        GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (window)->cancel_button),
	"clicked",
	GTK_SIGNAL_FUNC(gtk_widget_destroy),
	GTK_OBJECT (window));

      gtk_widget_pop_colormap ();
      gtk_widget_pop_visual ();
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

void
file_selection_hide_fileops (GtkWidget *widget,
			     GtkFileSelection *fs)
{
  gtk_file_selection_hide_fileop_buttons (fs);
}

void
file_selection_ok (GtkWidget        *w,
		   GtkFileSelection *fs)
{
  g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
  gtk_widget_destroy (GTK_WIDGET (fs));
}

void
create_file_selection ()
{
  static GtkWidget *window = NULL;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_file_selection_new ("file selection dialog");

      gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (window));

      gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (window)->ok_button),
			  "clicked", GTK_SIGNAL_FUNC(file_selection_ok),
			  window);
      gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (window)->cancel_button),
				 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      
      button = gtk_button_new_with_label ("Hide Fileops");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  (GtkSignalFunc) file_selection_hide_fileops, 
			  (gpointer) window);
      gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (window)->action_area), 
			  button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Show Fileops");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_file_selection_show_fileop_buttons, 
				 (gpointer) window);
      gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (window)->action_area), 
			  button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      
      
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * GtkDialog
 */
static GtkWidget *dialog_window = NULL;

void
label_toggle (GtkWidget  *widget,
	      GtkWidget **label)
{
  if (!(*label))
    {
      *label = gtk_label_new ("Dialog Test");
      gtk_signal_connect (GTK_OBJECT (*label),
			  "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  label);
      gtk_misc_set_padding (GTK_MISC (*label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->vbox), 
			  *label, TRUE, TRUE, 0);
      gtk_widget_show (*label);
    }
  else
    gtk_widget_destroy (*label);
}

void
create_dialog ()
{
  static GtkWidget *label;
  GtkWidget *button;

  if (!dialog_window)
    {
      dialog_window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (dialog_window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &dialog_window);

      gtk_window_set_title (GTK_WINDOW (dialog_window), "dialog");
      gtk_container_border_width (GTK_CONTAINER (dialog_window), 0);

      button = gtk_button_new_with_label ("OK");
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Toggle");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (label_toggle),
			  &label);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area),
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      label = NULL;
    }

  if (!GTK_WIDGET_VISIBLE (dialog_window))
    gtk_widget_show (dialog_window);
  else
    gtk_widget_destroy (dialog_window);
}


/*
 * GtkRange
 */
void
create_range_controls ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *scrollbar;
  GtkWidget *scale;
  GtkWidget *separator;
  GtkObject *adjustment;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "range controls");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      adjustment = gtk_adjustment_new (0.0, 0.0, 101.0, 0.1, 1.0, 1.0);

      scale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_usize (GTK_WIDGET (scale), 150, 30);
      gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
      gtk_scale_set_digits (GTK_SCALE (scale), 1);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (adjustment));
      gtk_range_set_update_policy (GTK_RANGE (scrollbar), 
				   GTK_UPDATE_CONTINUOUS);
      gtk_box_pack_start (GTK_BOX (box2), scrollbar, TRUE, TRUE, 0);
      gtk_widget_show (scrollbar);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * GtkRulers
 */
void
create_rulers ()
{
  static GtkWidget *window = NULL;
  GtkWidget *table;
  GtkWidget *ruler;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "rulers");
      gtk_widget_set_usize (window, 300, 300);
      gtk_widget_set_events (window, 
			     GDK_POINTER_MOTION_MASK 
			     | GDK_POINTER_MOTION_HINT_MASK);
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      table = gtk_table_new (2, 2, FALSE);
      gtk_container_add (GTK_CONTAINER (window), table);
      gtk_widget_show (table);

      ruler = gtk_hruler_new ();
      gtk_ruler_set_range (GTK_RULER (ruler), 5, 15, 0, 20);

      gtk_signal_connect_object (
        GTK_OBJECT (window), 
	"motion_notify_event",
	GTK_SIGNAL_FUNC(
          GTK_WIDGET_CLASS (GTK_OBJECT (ruler)->klass)->motion_notify_event),
	GTK_OBJECT (ruler));

      gtk_table_attach (GTK_TABLE (table), ruler, 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (ruler);


      ruler = gtk_vruler_new ();
      gtk_ruler_set_range (GTK_RULER (ruler), 5, 15, 0, 20);

      gtk_signal_connect_object (
        GTK_OBJECT (window), 
	"motion_notify_event",
	GTK_SIGNAL_FUNC (GTK_WIDGET_CLASS (GTK_OBJECT (ruler)->klass)->motion_notify_event),
	GTK_OBJECT (ruler));

      gtk_table_attach (GTK_TABLE (table), ruler, 0, 1, 1, 2,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (ruler);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


static void
text_toggle_editable (GtkWidget *checkbutton,
		       GtkWidget *text)
{
   gtk_text_set_editable(GTK_TEXT(text),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
text_toggle_word_wrap (GtkWidget *checkbutton,
		       GtkWidget *text)
{
   gtk_text_set_word_wrap(GTK_TEXT(text),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

/*
 * GtkText
 */
void
create_text ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *check;
  GtkWidget *separator;
  GtkWidget *table;
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *text;

  FILE *infile;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_name (window, "text window");
      gtk_widget_set_usize (window, 500, 500);
      gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, FALSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
      gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
      gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
      gtk_widget_show (table);

      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), TRUE);
      gtk_table_attach (GTK_TABLE (table), text, 0, 1, 0, 1,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (text);

      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL, 0, 0);
      gtk_widget_show (hscrollbar);

      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
			GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);

      gtk_text_freeze (GTK_TEXT (text));

      gtk_widget_realize (text);

      infile = fopen("testgtk.c", "r");
      
      if (infile)
	{
	  char buffer[1024];
	  int nchars;
	  
	  while (1)
	    {
	      nchars = fread(buffer, 1, 1024, infile);
	      gtk_text_insert (GTK_TEXT (text), NULL, NULL,
			       NULL, buffer, nchars);
	      
	      if (nchars < 1024)
		break;
	    }
	  
	  fclose (infile);
	}
      
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->black, NULL, 
		       "And even ", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->bg[GTK_STATE_NORMAL], NULL, 
		       "colored", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->black, NULL, 
		       "text", -1);

      gtk_text_thaw (GTK_TEXT (text));

      hbox = gtk_hbutton_box_new ();
      gtk_box_pack_start (GTK_BOX (box2), hbox, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      check = gtk_check_button_new_with_label("Editable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT(check), "toggled",
			  GTK_SIGNAL_FUNC(text_toggle_editable), text);
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(check), TRUE);
      gtk_widget_show (check);

      check = gtk_check_button_new_with_label("Wrap Words");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT(check), "toggled",
			  GTK_SIGNAL_FUNC(text_toggle_word_wrap), text);
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(check), FALSE);
      gtk_widget_show (check);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * GtkNotebook
 */

GdkPixmap *book_open;
GdkPixmap *book_closed;
GdkBitmap *book_open_mask;
GdkBitmap *book_closed_mask;

static char * book_open_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c #808080",
"o      c white",
"                ",
"  ..            ",
" .Xo.    ...    ",
" .Xoo. ..oo.    ",
" .Xooo.Xooo...  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
"  .Xoo.Xoo..X.  ",
"   .Xo.o..ooX.  ",
"    .X..XXXXX.  ",
"    ..X.......  ",
"     ..         ",
"                "};

static char * book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

static void
page_switch (GtkWidget *widget, GtkNotebookPage *page, gint page_num)
{
  GtkNotebookPage *oldpage;
  GtkWidget *pixwid;

  oldpage = GTK_NOTEBOOK (widget)->cur_page;

  if (page == oldpage)
    return;

  pixwid = ((GtkBoxChild*)(GTK_BOX (page->tab_label)->children->data))->widget;
  gtk_pixmap_set (GTK_PIXMAP (pixwid), book_open, book_open_mask);
  pixwid = ((GtkBoxChild*) (GTK_BOX (page->menu_label)->children->data))->widget;
  gtk_pixmap_set (GTK_PIXMAP (pixwid), book_open, book_open_mask);

  if (oldpage)
    {
      pixwid = ((GtkBoxChild*) (GTK_BOX 
				(oldpage->tab_label)->children->data))->widget;
      gtk_pixmap_set (GTK_PIXMAP (pixwid), book_closed, book_closed_mask);
      pixwid = ((GtkBoxChild*) (GTK_BOX (oldpage->menu_label)->children->data))->widget;
      gtk_pixmap_set (GTK_PIXMAP (pixwid), book_closed, book_closed_mask);
    }
}

static void
create_pages (GtkNotebook *notebook, gint start, gint end)
{
  GtkWidget *child = NULL;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *box;
  GtkWidget *hbox;
  GtkWidget *label_box;
  GtkWidget *menu_box;
  GtkWidget *button;
  GtkWidget *pixwid;
  gint i;
  char buffer[32];

  for (i = start; i <= end; i++)
    {
      sprintf (buffer, "Page %d", i);
     
      switch (i%4)
	{
	case 3:
	  child = gtk_button_new_with_label (buffer);
	  gtk_container_border_width (GTK_CONTAINER(child), 10);
	  break;
	case 2:
	  child = gtk_label_new (buffer);
	  break;
	case 1:
	  child = gtk_frame_new (buffer);
	  gtk_container_border_width (GTK_CONTAINER (child), 10);
      
	  box = gtk_vbox_new (TRUE,0);
	  gtk_container_border_width (GTK_CONTAINER (box), 10);
	  gtk_container_add (GTK_CONTAINER (child), box);

	  label = gtk_label_new (buffer);
	  gtk_box_pack_start (GTK_BOX(box), label, TRUE, TRUE, 5);

	  entry = gtk_entry_new ();
	  gtk_box_pack_start (GTK_BOX(box), entry, TRUE, TRUE, 5);
      
	  hbox = gtk_hbox_new (TRUE,0);
	  gtk_box_pack_start (GTK_BOX(box), hbox, TRUE, TRUE, 5);

	  button = gtk_button_new_with_label ("Ok");
	  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, TRUE, 5);

	  button = gtk_button_new_with_label ("Cancel");
	  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, TRUE, 5);
	  break;
	case 0:
	  child = gtk_frame_new (buffer);
	  gtk_container_border_width (GTK_CONTAINER (child), 10);

	  label = gtk_label_new (buffer);
	  gtk_container_add (GTK_CONTAINER (child), label);
	  break;
	}

      gtk_widget_show_all (child);

      label_box = gtk_hbox_new (FALSE, 0);
      pixwid = gtk_pixmap_new (book_closed, book_closed_mask);
      gtk_box_pack_start (GTK_BOX (label_box), pixwid, FALSE, TRUE, 0);
      gtk_misc_set_padding (GTK_MISC (pixwid), 3, 1);
      label = gtk_label_new (buffer);
      gtk_box_pack_start (GTK_BOX (label_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (label_box);
      
      menu_box = gtk_hbox_new (FALSE, 0);
      pixwid = gtk_pixmap_new (book_closed, book_closed_mask);
      gtk_box_pack_start (GTK_BOX (menu_box), pixwid, FALSE, TRUE, 0);
      gtk_misc_set_padding (GTK_MISC (pixwid), 3, 1);
      label = gtk_label_new (buffer);
      gtk_box_pack_start (GTK_BOX (menu_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (menu_box);

      gtk_notebook_append_page_menu (notebook, child, label_box, menu_box);
    }
}

static void
rotate_notebook (GtkButton   *button,
		 GtkNotebook *notebook)
{
  gtk_notebook_set_tab_pos (notebook, (notebook->tab_pos + 1) % 4);
}

static void
standard_notebook (GtkButton   *button,
		   GtkNotebook *notebook)
{
  gint i;

  gtk_notebook_set_show_tabs (notebook, TRUE);
  gtk_notebook_set_scrollable (notebook, FALSE);
  if (g_list_length (notebook->children) == 15)
    for (i = 0; i < 10; i++)
      gtk_notebook_remove_page (notebook, 5);
}

static void
notabs_notebook (GtkButton   *button,
		 GtkNotebook *notebook)
{
  gint i;

  gtk_notebook_set_show_tabs (notebook, FALSE);
  if (g_list_length (notebook->children) == 15)
    for (i = 0; i < 10; i++)
      gtk_notebook_remove_page (notebook, 5);
}

static void
scrollable_notebook (GtkButton   *button,
		     GtkNotebook *notebook)
{
  gtk_notebook_set_show_tabs (notebook, TRUE);
  gtk_notebook_set_scrollable (notebook, TRUE);
  if (g_list_length (notebook->children) == 5)
    create_pages (notebook, 6, 15);
}

static void
notebook_popup (GtkToggleButton *button,
		GtkNotebook     *notebook)
{
  if (button->active)
    gtk_notebook_popup_enable (notebook);
  else
    gtk_notebook_popup_disable (notebook);
}

static void
create_notebook ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *notebook;
  GtkWidget *omenu;
  GtkWidget *menu;
  GtkWidget *submenu;
  GtkWidget *menuitem;
  GSList *group;
  GdkColor *transparent = NULL;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "notebook");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      notebook = gtk_notebook_new ();
      gtk_signal_connect (GTK_OBJECT (notebook), "switch_page",
			  GTK_SIGNAL_FUNC (page_switch), NULL);
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
      gtk_box_pack_start (GTK_BOX (box1), notebook, TRUE, TRUE, 0);
      gtk_container_border_width (GTK_CONTAINER (notebook), 10);

      gtk_widget_realize (notebook);
      book_open = gdk_pixmap_create_from_xpm_d (notebook->window,
						&book_open_mask, 
						transparent, 
						book_open_xpm);
      book_closed = gdk_pixmap_create_from_xpm_d (notebook->window,
						  &book_closed_mask,
						  transparent, 
						  book_closed_xpm);

      create_pages (GTK_NOTEBOOK (notebook), 1, 5);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 10);
      
      box2 = gtk_hbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      
      omenu = gtk_option_menu_new ();
      menu = gtk_menu_new ();
      submenu = NULL;
      group = NULL;
      
      menuitem = gtk_radio_menu_item_new_with_label (group, "Standard");
      gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
				 GTK_SIGNAL_FUNC (standard_notebook),
				 GTK_OBJECT (notebook));
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
      gtk_menu_append (GTK_MENU (menu), menuitem);
      gtk_widget_show (menuitem);
      menuitem = gtk_radio_menu_item_new_with_label (group, "w/o Tabs");
      gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
				 GTK_SIGNAL_FUNC (notabs_notebook),
				 GTK_OBJECT (notebook));
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
      gtk_menu_append (GTK_MENU (menu), menuitem);
      gtk_widget_show (menuitem);
      menuitem = gtk_radio_menu_item_new_with_label (group, "Scrollable");
      gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
				 GTK_SIGNAL_FUNC (scrollable_notebook),
				 GTK_OBJECT (notebook));
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));
      gtk_menu_append (GTK_MENU (menu), menuitem);
      gtk_widget_show (menuitem);
      
      gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
      gtk_box_pack_start (GTK_BOX (box2), omenu, FALSE, FALSE, 0);
      button = gtk_check_button_new_with_label ("enable popup menu");
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT(button), "clicked",
			  GTK_SIGNAL_FUNC (notebook_popup),
			  GTK_OBJECT (notebook));
      
      box2 = gtk_hbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      
      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      button = gtk_button_new_with_label ("next");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_notebook_next_page),
				 GTK_OBJECT (notebook));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("prev");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_notebook_prev_page),
				 GTK_OBJECT (notebook));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("rotate");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (rotate_notebook),
			  notebook);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/*
 * GtkPanes
 */
void
create_panes ()
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Panes");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      vpaned = gtk_vpaned_new ();
      gtk_container_add (GTK_CONTAINER (window), vpaned);
      gtk_container_border_width (GTK_CONTAINER(vpaned), 5);
      gtk_widget_show (vpaned);

      hpaned = gtk_hpaned_new ();
      gtk_paned_add1 (GTK_PANED (vpaned), hpaned);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 60, 60);
      gtk_paned_add1 (GTK_PANED (hpaned), frame);
      gtk_widget_show (frame);
      
      button = gtk_button_new_with_label ("Hi there");
      gtk_container_add (GTK_CONTAINER(frame), button);
      gtk_widget_show (button);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 80, 60);
      gtk_paned_add2 (GTK_PANED (hpaned), frame);
      gtk_widget_show (frame);

      gtk_widget_show (hpaned);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 60, 80);
      gtk_paned_add2 (GTK_PANED (vpaned), frame);
      gtk_widget_show (frame);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Drag -N- Drop
 */

gint
dnd_drop_destroy_popup (GtkWidget *widget, GtkWindow **window)
{
  if(GTK_IS_BUTTON(widget)) /* I.e. they clicked the close button */
    gtk_widget_destroy(GTK_WIDGET(*window));
  else {
    gtk_grab_remove(GTK_WIDGET(*window));
    *window = NULL;
  }

  return FALSE;
}

void
dnd_drop (GtkWidget *button, GdkEvent *event)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox, *lbl, *btn;
  gchar *msg;

  /* DND doesn't obey gtk_grab's, so check if we're already displaying
   * drop modal dialog first
   */
  if (window)
    return;

  window = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_container_border_width (GTK_CONTAINER(window), 10);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC(dnd_drop_destroy_popup),
		      &window);
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		      GTK_SIGNAL_FUNC(gtk_false),
		      &window);

  vbox = gtk_vbox_new(FALSE, 5);

  /* Display message that we got from drop source */
  msg = g_malloc(strlen(event->dropdataavailable.data)
		 + strlen(event->dropdataavailable.data_type) + 100);
  sprintf(msg, "Drop data of type %s was:\n\n%s",
	  event->dropdataavailable.data_type,
	  (char *)event->dropdataavailable.data);
  lbl = gtk_label_new(msg);
  gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_FILL);
  g_free(msg);
  gtk_widget_show(lbl);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), lbl);

  /* Provide an obvious way out of this heinousness */
  btn = gtk_button_new_with_label("Continue with life in\nspite of this oppression");
  gtk_signal_connect_object (GTK_OBJECT (btn), "clicked",
			     GTK_SIGNAL_FUNC(gtk_widget_destroy),
			     GTK_OBJECT (window));
  gtk_widget_show(btn);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), btn);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show(vbox);
  gtk_grab_add(window);
  gtk_widget_show(window);
}

void
dnd_drag_request (GtkWidget *button, GdkEvent *event)
{
#define DND_STRING "Bill Gates demands royalties for\nyour use of his innovation."
  gtk_widget_dnd_data_set (button, event, DND_STRING, strlen(DND_STRING) + 1);
}

void
create_dnd ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *separator;

  /* For clarity... */
  char *possible_drag_types[] = {"text/plain"};
  char *accepted_drop_types[] = {"text/plain"};

  static GtkWidget *drag_icon = NULL;
  static GtkWidget *drop_icon = NULL;

  if (!window)
    {
      GdkPoint hotspot = {5,5};
      
      if (!drag_icon)
	{
	  drag_icon = shape_create_icon ("Modeller.xpm",
					 440, 140, 0,0, GTK_WINDOW_POPUP);
	  
	  gtk_signal_connect (GTK_OBJECT (drag_icon), "destroy",
			      GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			      &drag_icon);

	  gtk_widget_hide (drag_icon);
	}
      
      if (!drop_icon)
	{
	  drop_icon = shape_create_icon ("3DRings.xpm",
					 440, 140, 0,0, GTK_WINDOW_POPUP);
	  
	  gtk_signal_connect (GTK_OBJECT (drop_icon), "destroy",
			      GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			      &drop_icon);

	  gtk_widget_hide (drop_icon);
	}

      gdk_dnd_set_drag_shape(drag_icon->window,
			     &hotspot,
			     drop_icon->window,
			     &hotspot);

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Drag -N- Drop");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);

      frame = gtk_frame_new ("Drag");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);
      gtk_widget_show (frame);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);
      gtk_widget_show (box3);

      /*
       * FROM Button
       */
      button = gtk_button_new_with_label ("Drag me!");
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      /*
       * currently, the widget has to be realized to
       * set dnd on it, this needs to change
       */
      gtk_widget_realize (button);
      gtk_signal_connect (GTK_OBJECT (button),
			  "drag_request_event",
			  GTK_SIGNAL_FUNC(dnd_drag_request),
			  button);
      
      gtk_widget_dnd_drag_set (button, TRUE, possible_drag_types, 1);


      frame = gtk_frame_new ("Drop");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);
      gtk_widget_show (frame);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);
      gtk_widget_show (box3);


      /*
       * TO Button
       */
      button = gtk_button_new_with_label ("To");
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      gtk_widget_realize (button);
      gtk_signal_connect (GTK_OBJECT (button), 
			  "drop_data_available_event",
			  GTK_SIGNAL_FUNC(dnd_drop),
			  button);

      gtk_widget_dnd_drop_set (button, TRUE, accepted_drop_types, 1, FALSE);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));

      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Shaped Windows
 */
static GdkWindow *root_win = NULL;

typedef struct _cursoroffset {gint x,y;} CursorOffset;

static void
shape_pressed (GtkWidget *widget, GdkEventButton *event)
{
  CursorOffset *p;

  /* ignore double and triple click */
  if (event->type != GDK_BUTTON_PRESS)
    return;

  p = gtk_object_get_user_data (GTK_OBJECT(widget));
  p->x = (int) event->x;
  p->y = (int) event->y;

  gtk_grab_add (widget);
  gdk_pointer_grab (widget->window, TRUE,
		    GDK_BUTTON_RELEASE_MASK |
		    GDK_BUTTON_MOTION_MASK |
		    GDK_POINTER_MOTION_HINT_MASK,
		    NULL, NULL, 0);
}


static void
shape_released (GtkWidget *widget)
{
  gtk_grab_remove (widget);
  gdk_pointer_ungrab (0);
}

static void
shape_motion (GtkWidget      *widget, 
	      GdkEventMotion *event)
{
  gint xp, yp;
  CursorOffset * p;
  GdkModifierType mask;

  p = gtk_object_get_user_data (GTK_OBJECT (widget));

  /*
   * Can't use event->x / event->y here 
   * because I need absolute coordinates.
   */
  gdk_window_get_pointer (root_win, &xp, &yp, &mask);
  gtk_widget_set_uposition (widget, xp  - p->x, yp  - p->y);
}

GtkWidget *
shape_create_icon (char     *xpm_file,
		   gint      x,
		   gint      y,
		   gint      px,
		   gint      py,
		   gint      window_type)
{
  GtkWidget *window;
  GtkWidget *pixmap;
  GtkWidget *fixed;
  CursorOffset* icon_pos;
  GdkGC* gc;
  GdkBitmap *gdk_pixmap_mask;
  GdkPixmap *gdk_pixmap;
  GtkStyle *style;

  style = gtk_widget_get_default_style ();
  gc = style->black_gc;	

  /*
   * GDK_WINDOW_TOPLEVEL works also, giving you a title border
   */
  window = gtk_window_new (window_type);
  
  fixed = gtk_fixed_new ();
  gtk_widget_set_usize (fixed, 100,100);
  gtk_container_add (GTK_CONTAINER (window), fixed);
  gtk_widget_show (fixed);
  
  gtk_widget_set_events (window, 
			 gtk_widget_get_events (window) |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_POINTER_MOTION_HINT_MASK |
			 GDK_BUTTON_PRESS_MASK);

  gtk_widget_realize (window);
  gdk_pixmap = gdk_pixmap_create_from_xpm (window->window, &gdk_pixmap_mask, 
					   &style->bg[GTK_STATE_NORMAL],
					   xpm_file);

  pixmap = gtk_pixmap_new (gdk_pixmap, gdk_pixmap_mask);
  gtk_fixed_put (GTK_FIXED (fixed), pixmap, px,py);
  gtk_widget_show (pixmap);
  
  gtk_widget_shape_combine_mask (window, gdk_pixmap_mask, px,py);


  gtk_signal_connect (GTK_OBJECT (window), "button_press_event",
		      GTK_SIGNAL_FUNC (shape_pressed),NULL);
  gtk_signal_connect (GTK_OBJECT (window), "button_release_event",
		      GTK_SIGNAL_FUNC (shape_released),NULL);
  gtk_signal_connect (GTK_OBJECT (window), "motion_notify_event",
		      GTK_SIGNAL_FUNC (shape_motion),NULL);

  icon_pos = g_new (CursorOffset, 1);
  gtk_object_set_user_data(GTK_OBJECT(window), icon_pos);

  gtk_widget_set_uposition (window, x, y);
  gtk_widget_show (window);
  
  return window;
}

void 
create_shapes ()
{
  /* Variables used by the Drag/Drop and Shape Window demos */
  static GtkWidget *modeller = NULL;
  static GtkWidget *sheets = NULL;
  static GtkWidget *rings = NULL;

  root_win = gdk_window_foreign_new (GDK_ROOT_WINDOW ());

  if (!modeller)
    {
      modeller = shape_create_icon ("Modeller.xpm",
				    440, 140, 0,0, GTK_WINDOW_POPUP);

      gtk_signal_connect (GTK_OBJECT (modeller), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &modeller);
    }
  else
    gtk_widget_destroy (modeller);

  if (!sheets)
    {
      sheets = shape_create_icon ("FilesQueue.xpm",
				  580, 170, 0,0, GTK_WINDOW_POPUP);

      gtk_signal_connect (GTK_OBJECT (sheets), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &sheets);

    }
  else
    gtk_widget_destroy (sheets);

  if (!rings)
    {
      rings = shape_create_icon ("3DRings.xpm",
				 460, 270, 25,25, GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (rings), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &rings);
    }
  else
    gtk_widget_destroy (rings);
}

void
create_wmhints ()
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *button;
  GtkWidget *box1;
  GtkWidget *box2;

  GdkBitmap *circles;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "WM Hints");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      gtk_widget_realize (window);
      
      circles = gdk_bitmap_create_from_data (window->window,
					     circles_bits,
					     circles_width,
					     circles_height);
      gdk_window_set_icon (window->window, NULL,
			   circles, circles);
      
      gdk_window_set_icon_name (window->window, "WMHints Test Icon");
  
      gdk_window_set_decorations (window->window, GDK_DECOR_ALL | GDK_DECOR_MENU);
      gdk_window_set_functions (window->window, GDK_FUNC_ALL | GDK_FUNC_RESIZE);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      label = gtk_label_new ("Try iconizing me!");
      gtk_widget_set_usize (label, 150, 50);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);
      gtk_widget_show (label);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));

      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Progress Bar
 */
static int progress_timer = 0;

gint
progress_timeout (gpointer data)
{
  gfloat new_val;

  new_val = GTK_PROGRESS_BAR (data)->percentage;
  if (new_val >= 1.0)
    new_val = 0.0;
  new_val += 0.02;

  gtk_progress_bar_update (GTK_PROGRESS_BAR (data), new_val);

  return TRUE;
}

static void
destroy_progress (GtkWidget  *widget,
		  GtkWidget **window)
{
  gtk_timeout_remove (progress_timer);
  progress_timer = 0;
  *window = NULL;
}

void
create_progress_bar ()
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *pbar;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_progress),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "dialog");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      label = gtk_label_new ("progress...");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
      gtk_widget_show (label);

      pbar = gtk_progress_bar_new ();
      gtk_widget_set_usize (pbar, 200, 20);
      gtk_box_pack_start (GTK_BOX (vbox), pbar, TRUE, TRUE, 0);
      gtk_widget_show (pbar);

      progress_timer = gtk_timeout_add (100, progress_timeout, pbar);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Color Preview
 */
static int color_idle = 0;

gint
color_idle_func (GtkWidget *preview)
{
  static int count = 1;
  guchar buf[768];
  int i, j, k;

  for (i = 0; i < 256; i++)
    {
      for (j = 0, k = 0; j < 256; j++)
	{
	  buf[k+0] = i + count;
	  buf[k+1] = 0;
	  buf[k+2] = j + count;
	  k += 3;
	}

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
    }

  count += 1;

  gtk_widget_draw (preview, NULL);

  return TRUE;
}

static void
color_preview_destroy (GtkWidget  *widget,
		       GtkWidget **window)
{
  gtk_idle_remove (color_idle);
  color_idle = 0;

  *window = NULL;
}

void
create_color_preview ()
{
  static GtkWidget *window = NULL;
  GtkWidget *preview;
  guchar buf[768];
  int i, j, k;

  if (!window)
    {
      gtk_widget_push_visual (gtk_preview_get_visual ());
      gtk_widget_push_colormap (gtk_preview_get_cmap ());

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(color_preview_destroy),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_border_width (GTK_CONTAINER (window), 10);

      preview = gtk_preview_new (GTK_PREVIEW_COLOR);
      gtk_preview_size (GTK_PREVIEW (preview), 256, 256);
      gtk_container_add (GTK_CONTAINER (window), preview);
      gtk_widget_show (preview);

      for (i = 0; i < 256; i++)
	{
	  for (j = 0, k = 0; j < 256; j++)
	    {
	      buf[k+0] = i;
	      buf[k+1] = 0;
	      buf[k+2] = j;
	      k += 3;
	    }

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
	}

      color_idle = gtk_idle_add ((GtkFunction) color_idle_func, preview);

      gtk_widget_pop_colormap ();
      gtk_widget_pop_visual ();
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Gray Preview
 */
static int gray_idle = 0;

gint
gray_idle_func (GtkWidget *preview)
{
  static int count = 1;
  guchar buf[256];
  int i, j;

  for (i = 0; i < 256; i++)
    {
      for (j = 0; j < 256; j++)
	buf[j] = i + j + count;

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
    }

  count += 1;

  gtk_widget_draw (preview, NULL);

  return TRUE;
}

static void
gray_preview_destroy (GtkWidget  *widget,
		      GtkWidget **window)
{
  gtk_idle_remove (gray_idle);
  gray_idle = 0;

  *window = NULL;
}

void
create_gray_preview ()
{
  static GtkWidget *window = NULL;
  GtkWidget *preview;
  guchar buf[256];
  int i, j;

  if (!window)
    {
      gtk_widget_push_visual (gtk_preview_get_visual ());
      gtk_widget_push_colormap (gtk_preview_get_cmap ());

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gray_preview_destroy),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_border_width (GTK_CONTAINER (window), 10);

      preview = gtk_preview_new (GTK_PREVIEW_GRAYSCALE);
      gtk_preview_size (GTK_PREVIEW (preview), 256, 256);
      gtk_container_add (GTK_CONTAINER (window), preview);
      gtk_widget_show (preview);

      for (i = 0; i < 256; i++)
	{
	  for (j = 0; j < 256; j++)
	    buf[j] = i + j;

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
	}

      gray_idle = gtk_idle_add ((GtkFunction) gray_idle_func, preview);

      gtk_widget_pop_colormap ();
      gtk_widget_pop_visual ();
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Selection Test
 */
void
selection_test_received (GtkWidget *list, GtkSelectionData *data)
{
  GdkAtom *atoms;
  GtkWidget *list_item;
  GList *item_list;
  int i, l;

  if (data->length < 0)
    {
      g_print ("Selection retrieval failed\n");
      return;
    }
  if (data->type != GDK_SELECTION_TYPE_ATOM)
    {
      g_print ("Selection \"TARGETS\" was not returned as atoms!\n");
      return;
    }

  /* Clear out any current list items */

  gtk_list_clear_items (GTK_LIST(list), 0, -1);

  /* Add new items to list */

  atoms = (GdkAtom *)data->data;

  item_list = NULL;
  l = data->length / sizeof (GdkAtom);
  for (i = 0; i < l; i++)
    {
      char *name;
      name = gdk_atom_name (atoms[i]);
      if (name != NULL)
	{
	  list_item = gtk_list_item_new_with_label (name);
	  g_free (name);
	}
      else
	list_item = gtk_list_item_new_with_label ("(bad atom)");

      gtk_widget_show (list_item);
      item_list = g_list_append (item_list, list_item);
    }

  gtk_list_append_items (GTK_LIST (list), item_list);

  return;
}

void
selection_test_get_targets (GtkWidget *widget, GtkWidget *list)
{
  static GdkAtom targets_atom = GDK_NONE;

  if (targets_atom == GDK_NONE)
    targets_atom = gdk_atom_intern ("TARGETS", FALSE);

  gtk_selection_convert (list, GDK_SELECTION_PRIMARY, targets_atom,
			 GDK_CURRENT_TIME);
}

void
create_selection_test ()
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *scrolled_win;
  GtkWidget *list;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Selection Test");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      /* Create the list */

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox,
			  TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      label = gtk_label_new ("Gets available targets for current selection");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_widget_set_usize (scrolled_win, 100, 200);
      gtk_widget_show (scrolled_win);

      list = gtk_list_new ();
      gtk_container_add (GTK_CONTAINER (scrolled_win), list);

      gtk_signal_connect (GTK_OBJECT(list), "selection_received",
			  GTK_SIGNAL_FUNC (selection_test_received), NULL);
      gtk_widget_show (list);

      /* .. And create some buttons */
      button = gtk_button_new_with_label ("Get Targets");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (selection_test_get_targets), list);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Gamma Curve
 */
void
create_gamma_curve ()
{
  static GtkWidget *window = NULL, *curve;
  static int count = 0;
  gfloat vec[256];
  gint max;
  gint i;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_border_width (GTK_CONTAINER (window), 10);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      curve = gtk_gamma_curve_new ();
      gtk_container_add (GTK_CONTAINER (window), curve);
      gtk_widget_show (curve);
    }

  max = 127 + (count % 2)*128;
  gtk_curve_set_range (GTK_CURVE (GTK_GAMMA_CURVE (curve)->curve),
		       0, max, 0, max);
  for (i = 0; i < max; ++i)
    vec[i] = (127 / sqrt (max)) * sqrt (i);
  gtk_curve_set_vector (GTK_CURVE (GTK_GAMMA_CURVE (curve)->curve),
			max, vec);

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else if (count % 4 == 3)
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  ++count;
}

static int scroll_test_pos = 0.0;
static GdkGC *scroll_test_gc = NULL;

static gint
scroll_test_expose (GtkWidget *widget, GdkEventExpose *event,
		    GtkAdjustment *adj)
{
  gint i,j;
  gint imin, imax, jmin, jmax;
  
  imin = (event->area.x) / 10;
  imax = (event->area.x + event->area.width + 9) / 10;

  jmin = ((int)adj->value + event->area.y) / 10;
  jmax = ((int)adj->value + event->area.y + event->area.height + 9) / 10;

  gdk_window_clear_area (widget->window,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	gdk_draw_rectangle (widget->window, 
			    widget->style->black_gc,
			    TRUE,
			    10*i, 10*j - (int)adj->value, 1+i%10, 1+j%10);

  return TRUE;
}

static void
scroll_test_configure (GtkWidget *widget, GdkEventConfigure *event,
		       GtkAdjustment *adj)
{
  adj->page_increment = 0.9 * widget->allocation.height;
  adj->page_size = widget->allocation.height;

  gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

static void
scroll_test_adjustment_changed (GtkAdjustment *adj, GtkWidget *widget)
{
  gint source_min = (int)adj->value - scroll_test_pos;
  gint source_max = source_min + widget->allocation.height;
  gint dest_min = 0;
  gint dest_max = widget->allocation.height;
  GdkRectangle rect;
  GdkEvent *event;

  scroll_test_pos = adj->value;

  if (!GTK_WIDGET_DRAWABLE (widget))
    return;

  if (source_min < 0)
    {
      rect.x = 0; 
      rect.y = 0;
      rect.width = widget->allocation.width;
      rect.height = -source_min;
      if (rect.height > widget->allocation.height)
	rect.height = widget->allocation.height;

      source_min = 0;
      dest_min = rect.height;
    }
  else
    {
      rect.x = 0;
      rect.y = 2*widget->allocation.height - source_max;
      if (rect.y < 0)
	rect.y = 0;
      rect.width = widget->allocation.width;
      rect.height = widget->allocation.height - rect.y;

      source_max = widget->allocation.height;
      dest_max = rect.y;
    }

  if (source_min != source_max)
    {
      if (scroll_test_gc == NULL)
	{
	  scroll_test_gc = gdk_gc_new (widget->window);
	  gdk_gc_set_exposures (scroll_test_gc, TRUE);
	}

      gdk_draw_pixmap (widget->window,
		       scroll_test_gc,
		       widget->window,
		       0, source_min,
		       0, dest_min,
		       widget->allocation.width,
		       source_max - source_min);

      /* Make sure graphics expose events are processed before scrolling
       * again */
      
      while ((event = gdk_event_get_graphics_expose (widget->window)) != NULL)
	{
	  gtk_widget_event (widget, event);
	  if (event->expose.count == 0)
	    {
	      gdk_event_free (event);
	      break;
	    }
	  gdk_event_free (event);
	}
    }


  if (rect.height != 0)
    gtk_widget_draw (widget, &rect);
}


void
create_scroll_test ()
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *drawing_area;
  GtkWidget *scrollbar;
  GtkWidget *button;
  GtkAdjustment *adj;
  
  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Scroll Test");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox,
			  TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      drawing_area = gtk_drawing_area_new ();
      gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area), 200, 200);
      gtk_box_pack_start (GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);
      gtk_widget_show (drawing_area);

      gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK);

      adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 180.0, 200.0));
      scroll_test_pos = 0.0;

      scrollbar = gtk_vscrollbar_new (adj);
      gtk_box_pack_start (GTK_BOX (hbox), scrollbar, FALSE, FALSE, 0);
      gtk_widget_show (scrollbar);

      gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
			  GTK_SIGNAL_FUNC (scroll_test_expose), adj);
      gtk_signal_connect (GTK_OBJECT (drawing_area), "configure_event",
			  GTK_SIGNAL_FUNC (scroll_test_configure), adj);

      
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (scroll_test_adjustment_changed),
			  drawing_area);
      
      /* .. And create some buttons */

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Timeout Test
 */
static int timer = 0;

gint
timeout_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set (GTK_LABEL (label), buffer);

  return TRUE;
}

void
start_timeout_test (GtkWidget *widget,
		    GtkWidget *label)
{
  if (!timer)
    {
      timer = gtk_timeout_add (100, (GtkFunction) timeout_test, label);
    }
}

void
stop_timeout_test (GtkWidget *widget,
		   gpointer   data)
{
  if (timer)
    {
      gtk_timeout_remove (timer);
      timer = 0;
    }
}

void
destroy_timeout_test (GtkWidget  *widget,
		      GtkWidget **window)
{
  stop_timeout_test (NULL, NULL);

  *window = NULL;
}

void
create_timeout_test ()
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_timeout_test),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Timeout Test");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      gtk_misc_set_padding (GTK_MISC (label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(start_timeout_test),
			  label);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(stop_timeout_test),
			  NULL);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Idle Test
 */
static int idle = 0;

gint
idle_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set (GTK_LABEL (label), buffer);

  return TRUE;
}

void
start_idle_test (GtkWidget *widget,
		 GtkWidget *label)
{
  if (!idle)
    {
      idle = gtk_idle_add ((GtkFunction) idle_test, label);
    }
}

void
stop_idle_test (GtkWidget *widget,
		gpointer   data)
{
  if (idle)
    {
      gtk_idle_remove (idle);
      idle = 0;
    }
}

void
destroy_idle_test (GtkWidget  *widget,
		   GtkWidget **window)
{
  stop_idle_test (NULL, NULL);

  *window = NULL;
}

void
create_idle_test ()
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_idle_test),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Idle Test");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      gtk_misc_set_padding (GTK_MISC (label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(start_idle_test),
			  label);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(stop_idle_test),
			  NULL);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Test of recursive mainloop
 */

void
mainloop_destroyed (GtkWidget *w, GtkWidget **window)
{
  *window = NULL;
  gtk_main_quit ();
}

void
create_mainloop ()
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_title (GTK_WINDOW (window), "Test Main Loop");

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(mainloop_destroyed),
			  &window);

      label = gtk_label_new ("In recursive main loop...");
      gtk_misc_set_padding (GTK_MISC(label), 20, 20);

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), label,
			  TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("Leave");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), button, 
			  FALSE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));

      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show (window);

      g_print ("create_mainloop: start\n");
      gtk_main ();
      g_print ("create_mainloop: done\n");
    }
  else
    gtk_widget_destroy (window);
}


/*
 * Main Window and Exit
 */
void
do_exit (GtkWidget *widget, GtkWidget *window)
{
  gtk_widget_destroy (window);
  gtk_main_quit ();
}

void
create_main_window ()
{
  struct {
    char *label;
    void (*func) ();
  } buttons[] =
    {
      { "button box", create_button_box },
      { "buttons", create_buttons },
      { "check buttons", create_check_buttons },
      { "clist", create_clist},
      { "color selection", create_color_selection },
      { "cursors", create_cursors },
      { "dialog", create_dialog },
      { "dnd", create_dnd },
      { "entry", create_entry },
      { "file selection", create_file_selection },
      { "gamma curve", create_gamma_curve },
      { "handle box", create_handle_box },
      { "list", create_list },
      { "menus", create_menus },
      { "miscellaneous", NULL },
      { "notebook", create_notebook },
      { "panes", create_panes },
      { "pixmap", create_pixmap },
      { "preview color", create_color_preview },
      { "preview gray", create_gray_preview },
      { "progress bar", create_progress_bar },
      { "radio buttons", create_radio_buttons },
      { "range controls", create_range_controls },
      { "reparent", create_reparent },
      { "rulers", create_rulers },
      { "scrolled windows", create_scrolled_windows },
      { "shapes", create_shapes },
      { "spinbutton", create_spins },
      { "statusbar", create_statusbar },
      { "test idle", create_idle_test },
      { "test mainloop", create_mainloop },
      { "test scrolling", create_scroll_test },
      { "test selection", create_selection_test },
      { "test timeout", create_timeout_test },
      { "text", create_text },
      { "toggle buttons", create_toggle_buttons },
      { "toolbar", create_toolbar },
      { "tooltips", create_tooltips },
      { "tree", create_tree_mode_window},
      { "WM hints", create_wmhints },
    };
  int nbuttons = sizeof (buttons) / sizeof (buttons[0]);
  GtkWidget *window;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *scrolled_window;
  GtkWidget *button;
  GtkWidget *label;
  gchar buffer[64];
  GtkWidget *separator;
  int i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "main window");
  gtk_widget_set_usize (window, 200, 400);
  gtk_widget_set_uposition (window, 20, 20);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC(gtk_main_quit),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		      GTK_SIGNAL_FUNC (gtk_false),
		      NULL);

  box1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_widget_show (box1);

  if (gtk_micro_version > 0)
    sprintf (buffer,
	     "Gtk+ v%d.%d.%d",
	     gtk_major_version,
	     gtk_minor_version,
	     gtk_micro_version);
  else
    sprintf (buffer,
	     "Gtk+ v%d.%d",
	     gtk_major_version,
	     gtk_minor_version);

  label = gtk_label_new (buffer);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 0);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_border_width (GTK_CONTAINER (scrolled_window), 10);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
     		                  GTK_POLICY_AUTOMATIC, 
                                  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box1), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  box2 = gtk_vbox_new (FALSE, 0);
  gtk_container_border_width (GTK_CONTAINER (box2), 10);
  gtk_container_add (GTK_CONTAINER (scrolled_window), box2);
  gtk_widget_show (box2);

  for (i = 0; i < nbuttons; i++)
    {
      button = gtk_button_new_with_label (buttons[i].label);
      if (buttons[i].func)
        gtk_signal_connect (GTK_OBJECT (button), 
			    "clicked", 
			    GTK_SIGNAL_FUNC(buttons[i].func),
			    NULL);
      else
        gtk_widget_set_sensitive (button, FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);

  box2 = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (box2), 10);
  gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
  gtk_widget_show (box2);

  button = gtk_button_new_with_label ("close");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (do_exit),
		      window);
  gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  gtk_widget_show (window);
}

int
main (int argc, char *argv[])
{
  gtk_set_locale ();

  gtk_init (&argc, &argv);

#ifdef HAVE_LIBGLE
  gle_init (&argc, &argv);
#endif /* !HAVE_LIBGLE */

  gtk_rc_parse ("testgtkrc");

  create_main_window ();

  gtk_main ();

  return 0;
}
