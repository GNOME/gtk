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

void
destroy_window (GtkWidget  *widget,
		GtkWidget **window)
{
  *window = NULL;
}

void
button_window (GtkWidget *widget,
	       GtkWidget *button)
{
  if (!GTK_WIDGET_VISIBLE (button))
    gtk_widget_show (button);
  else
    gtk_widget_hide (button);
}

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
bbox_widget_destroy (GtkWidget* widget, GtkWidget* todestroy)
{
}

void
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
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
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

void
test_hbbox ()
{
  create_bbox_window (TRUE, "Spread", 50, 40, 85, 28, GTK_BUTTONBOX_SPREAD);
  create_bbox_window (TRUE, "Edge", 200, 40, 85, 25, GTK_BUTTONBOX_EDGE);
  create_bbox_window (TRUE, "Start", 350, 40, 85, 25, GTK_BUTTONBOX_START);
  create_bbox_window (TRUE, "End", 500, 15, 30, 25, GTK_BUTTONBOX_END);
}

void
test_vbbox ()
{
  create_bbox_window (FALSE, "Spread", 50, 40, 85, 25, GTK_BUTTONBOX_SPREAD);
  create_bbox_window (FALSE, "Edge", 250, 40, 85, 28, GTK_BUTTONBOX_EDGE);
  create_bbox_window (FALSE, "Start", 450, 40, 85, 25, GTK_BUTTONBOX_START);
  create_bbox_window (FALSE, "End", 650, 15, 30, 25, GTK_BUTTONBOX_END);
} 

void
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
			GTK_SIGNAL_FUNC(destroy_window), &window);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC(destroy_window), &window);
    
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

GtkWidget *
new_pixmap (char      *filename,
	    GdkWindow *window,
	    GdkColor  *background)
{
  GtkWidget *wpixmap;
  GdkPixmap *pixmap;
  GdkBitmap *mask;

  pixmap = gdk_pixmap_create_from_xpm (window, &mask,
				       background,
				       "test.xpm");
  wpixmap = gtk_pixmap_new (pixmap, mask);

  return wpixmap;
}

void
set_toolbar_horizontal (GtkWidget *widget,
			gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_HORIZONTAL);
}

void
set_toolbar_vertical (GtkWidget *widget,
		      gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_VERTICAL);
}

void
set_toolbar_icons (GtkWidget *widget,
		   gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_ICONS);
}

void
set_toolbar_text (GtkWidget *widget,
	          gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_TEXT);
}

void
set_toolbar_both (GtkWidget *widget,
		  gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_BOTH);
}

void
set_toolbar_small_space (GtkWidget *widget,
			 gpointer   data)
{
  gtk_toolbar_set_space_size (GTK_TOOLBAR (data), 5);
}

void
set_toolbar_big_space (GtkWidget *widget,
		       gpointer   data)
{
  gtk_toolbar_set_space_size (GTK_TOOLBAR (data), 10);
}

void
set_toolbar_enable (GtkWidget *widget,
		    gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), TRUE);
}

void
set_toolbar_disable (GtkWidget *widget,
		     gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), FALSE);
}

void
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
			  GTK_SIGNAL_FUNC (destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC (destroy_window),
			  &window);

      gtk_container_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize (window);

      toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Horizontal", "Horizontal toolbar layout",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_horizontal, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Vertical", "Vertical toolbar layout",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_vertical, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Icons", "Only show toolbar icons",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_icons, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Text", "Only show toolbar text",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_text, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Both", "Show toolbar icons and text",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_both, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      entry = gtk_entry_new ();
      gtk_widget_show(entry);
      gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), NULL, entry);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Small", "Use small spaces",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_small_space, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Big", "Use big spaces",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_big_space, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Enable", "Enable tooltips",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_enable, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Disable", "Disable tooltips",
			       GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						       &window->style->bg[GTK_STATE_NORMAL])),
			       (GtkSignalFunc) set_toolbar_disable, toolbar);

      gtk_container_add (GTK_CONTAINER (window), toolbar);
      gtk_widget_show (toolbar);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

GtkWidget *
make_toolbar (GtkWidget *window)
{
  GtkWidget *toolbar;

  if (!GTK_WIDGET_REALIZED (window))
    gtk_widget_realize (window);

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Horizontal", "Horizontal toolbar layout",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_horizontal, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Vertical", "Vertical toolbar layout",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_vertical, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Icons", "Only show toolbar icons",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_icons, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Text", "Only show toolbar text",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_text, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Both", "Show toolbar icons and text",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_both, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Small", "Use small spaces",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_small_space, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Big", "Use big spaces",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_big_space, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Enable", "Enable tooltips",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_enable, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Disable", "Disable tooltips",
			   GTK_PIXMAP (new_pixmap ("test.xpm", window->window,
						   &window->style->bg[GTK_STATE_NORMAL])),
			   (GtkSignalFunc) set_toolbar_disable, toolbar);

  return toolbar;
}

void
create_handle_box ()
{
  static GtkWidget* window = NULL;
  GtkWidget* hbox;
#if 0
  GtkWidget* button;
#endif
	
  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window),
			  "Handle Box Test");
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC(destroy_window), &window);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC(destroy_window), &window);
    
    gtk_container_border_width (GTK_CONTAINER (window), 20);
    
    hbox = gtk_handle_box_new ();
    gtk_container_add (GTK_CONTAINER (window), hbox);
    gtk_widget_show (hbox);
#if 0
#if 0
    button = gtk_toggle_button_new_with_label ("Let's try this");
#else
    button = gtk_label_new ("Let's try this");
#endif
    gtk_container_add (GTK_CONTAINER (hbox), button);
    gtk_widget_set_usize(button, 250, 40);
    gtk_widget_show (button);
#else
    {
      GtkWidget *toolbar = make_toolbar (window);
      gtk_container_add (GTK_CONTAINER (hbox), toolbar);
      gtk_widget_show (toolbar);
    }
#endif
  }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


void
reparent_label (GtkWidget *widget,
		GtkWidget *new_parent)
{
  GtkWidget *label;

  label = gtk_object_get_user_data (GTK_OBJECT (widget));

  gtk_widget_reparent (label, new_parent);
}

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
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
                          GTK_SIGNAL_FUNC(destroy_window),
                          &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                          GTK_SIGNAL_FUNC(destroy_window),
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

void
create_tooltips ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkTooltips *tooltips;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(destroy_window),
                          &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                          GTK_SIGNAL_FUNC(destroy_window),
                          &window);

      gtk_window_set_title (GTK_WINDOW (window), "tooltips");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      tooltips=gtk_tooltips_new();

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

      gtk_tooltips_set_tips(tooltips,button,"This is button 1");

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      gtk_tooltips_set_tips(tooltips,button,"This is button 2");

      button = gtk_toggle_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      gtk_tooltips_set_tips (tooltips, button, "This is button 3. This is also a really long tooltip which probably won't fit on a single line and will therefore need to be wrapped. Hopefully the wrapping will work correctly.");

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

      gtk_tooltips_set_tips (tooltips, button, "Push this button to close window");
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

GtkWidget*
create_menu (int depth)
{
  GtkWidget *menu;
  GtkWidget *submenu;
  GtkWidget *menuitem;
  GSList *group;
  char buf[32];
  int i, j;

  if (depth < 1)
    return NULL;

  menu = gtk_menu_new ();
  submenu = NULL;
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

      if (depth > 0)
	{
	  if (!submenu)
	    submenu = create_menu (depth - 1);
	  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
	}
    }

  return menu;
}

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);

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
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_label ("bar");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
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
void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void entry_toggle_editable (GtkWidget *checkbutton,
			    GtkWidget *entry)
{
   gtk_entry_set_editable(GTK_ENTRY(entry),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

void
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
      cbitems = g_list_append(cbitems, "item1");
      cbitems = g_list_append(cbitems, "item2");
      cbitems = g_list_append(cbitems, "and item3");
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
      gtk_entry_select_region (GTK_ENTRY (entry), 
			       0, GTK_ENTRY(entry)->text_length);
      gtk_box_pack_start (GTK_BOX (box2), entry, TRUE, TRUE, 0);
      gtk_widget_show (entry);

      cb = gtk_combo_box_new (cbitems);
      gtk_entry_set_text (GTK_ENTRY (cb), "hello world");
      gtk_entry_select_region (GTK_ENTRY (cb),
			       0, GTK_ENTRY(entry)->text_length);
      gtk_box_pack_start (GTK_BOX (box2), cb, TRUE, TRUE, 0);
      gtk_widget_show (cb);

      editable_check = gtk_check_button_new_with_label("Editable");
      gtk_box_pack_start (GTK_BOX (box2), editable_check, TRUE, TRUE, 0);
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
  /*  else
    gtk_widget_destroy (window); */
}

/*
 * GtkList
 */
void
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

void
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

  tmp_list = clear_list;

  while (tmp_list)
    {
      gtk_widget_destroy (GTK_WIDGET (tmp_list->data));
      tmp_list = tmp_list->next;
    }

  g_list_free (clear_list);
}

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
add1000_clist (GtkWidget *widget, gpointer data)
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
  for (i = 0; i < 1000; i++)
    {
      sprintf (text[0], "Row %d", clist_rows++);
      gtk_clist_append (GTK_CLIST (data), texts);
    }
  gtk_clist_thaw (GTK_CLIST (data));

}

void
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
select_clist (GtkWidget *widget,
	      gint row, 
	      gint column, 
	      GdkEventButton *bevent)
{
  gint button = 0;

  if (bevent)
    button = bevent->button;

  g_print ("GtkCList Selection: row %d column %d button %d\n", 
	   row, column, button);

  clist_selected_row = row;
}

void
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

      /* create this here so we have a pointer to throw at the 
       * button callbacks -- more is done with it later */
      clist = gtk_clist_new (TESTGTK_CLIST_COLUMNS, titles);

      button = gtk_button_new_with_label ("Add 1,000 Rows");
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

      gtk_clist_set_column_width (GTK_CLIST (clist), 0, 100);

      for (i = 1; i < TESTGTK_CLIST_COLUMNS; i++)
	gtk_clist_set_column_width (GTK_CLIST (clist), i, 80);

      gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);

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
    gtk_widget_destroy (window);

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
                          GTK_SIGNAL_FUNC(destroy_window),
                          &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                          GTK_SIGNAL_FUNC(destroy_window),
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
file_selection_ok (GtkWidget        *w,
		   GtkFileSelection *fs)
{
  g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

void
create_file_selection ()
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = gtk_file_selection_new ("file selection dialog");
      gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);

      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (window)->ok_button),
			  "clicked", GTK_SIGNAL_FUNC(file_selection_ok),
			  window);
      gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (window)->cancel_button),
				 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
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
      gtk_misc_set_padding (GTK_MISC (*label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->vbox), 
			  *label, TRUE, TRUE, 0);
      gtk_widget_show (*label);
    }
  else
    {
      gtk_widget_destroy (*label);
      *label = NULL;
    }
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &dialog_window);
      gtk_signal_connect (GTK_OBJECT (dialog_window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
			  GTK_SIGNAL_FUNC(label_toggle),
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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


/*
 * GtkText
 */
void
create_text ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *table;
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *text;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_name (window, "text window");

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
      gtk_table_attach_defaults (GTK_TABLE (table), text, 0, 1, 0, 1);
      gtk_widget_show (text);

      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (hscrollbar);

      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_widget_set_usize(vscrollbar, 30, -1);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);

      gtk_text_freeze (GTK_TEXT (text));

      gtk_widget_realize (text);

      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "spencer blah blah blah\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "kimball\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "is\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "a\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "wuss.\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "but\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL,
		       "josephine\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "(his\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "girlfriend\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "is\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "not).\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "why?\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "because\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "spencer\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "puked\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "last\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "night\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "but\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "josephine\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "did\n", -1);
      gtk_text_insert (GTK_TEXT (text), NULL, &text->style->white, NULL, 
		       "not", -1);

      gtk_text_thaw (GTK_TEXT (text));

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
void
rotate_notebook (GtkButton   *button,
		 GtkNotebook *notebook)
{
  gtk_notebook_set_tab_pos (notebook, (notebook->tab_pos + 1) % 4);
}

void
create_notebook ()
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *notebook;
  GtkWidget *frame;
  GtkWidget *label;
  char buffer[32];
  int i;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "notebook");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      notebook = gtk_notebook_new ();
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
      gtk_box_pack_start (GTK_BOX (box2), notebook, TRUE, TRUE, 0);
      gtk_widget_show (notebook);


      for (i = 0; i < 5; i++)
	{
	  sprintf (buffer, "Page %d", i+1);

	  frame = gtk_frame_new (buffer);
	  gtk_container_border_width (GTK_CONTAINER (frame), 10);
	  gtk_widget_set_usize (frame, 200, 150);
	  gtk_widget_show (frame);

	  label = gtk_label_new (buffer);
	  gtk_container_add (GTK_CONTAINER (frame), label);
	  gtk_widget_show (label);

	  label = gtk_label_new (buffer);
	  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);
	}


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_hbox_new (FALSE, 10);
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

      button = gtk_button_new_with_label ("next");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_notebook_next_page),
				 GTK_OBJECT (notebook));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("prev");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_notebook_prev_page),
				 GTK_OBJECT (notebook));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("rotate");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(rotate_notebook),
			  notebook);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
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

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

gboolean
dnd_drop_destroy_popup (GtkWidget *widget, GtkWindow **window)
{
  if(GTK_IS_BUTTON(widget)) /* I.e. they clicked the close button */
    gtk_widget_destroy(GTK_WIDGET(*window));
  else {
    gtk_grab_remove(GTK_WIDGET(*window));
    *window = NULL;
  }
  return TRUE;
}

void
dnd_drop (GtkWidget *button, GdkEvent *event)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox, *lbl, *btn;
  gchar *msg;

  window = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_container_border_width (GTK_CONTAINER(window), 10);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC(dnd_drop_destroy_popup),
		      &window);
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		      GTK_SIGNAL_FUNC(dnd_drop_destroy_popup),
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
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		      GTK_SIGNAL_FUNC(dnd_drop_destroy_popup),
		      &window);
  gtk_widget_show(btn);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), btn);

  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_widget_show(vbox);
  gtk_grab_add(window);
  gtk_widget_show(window);

  g_free (event->dropdataavailable.data);
  g_free (event->dropdataavailable.data_type);
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

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
static GtkWidget *modeller = NULL;
static GtkWidget *sheets = NULL;
static GtkWidget *rings = NULL;

typedef struct _cursoroffset {gint x,y;} CursorOffset;

static void
shape_pressed (GtkWidget *widget)
{
  CursorOffset *p;

  p = gtk_object_get_user_data (GTK_OBJECT(widget));
  gtk_widget_get_pointer (widget, &(p->x), &(p->y));

  gtk_grab_add (widget);
  gdk_pointer_grab (widget->window, TRUE,
		    GDK_BUTTON_RELEASE_MASK |
		    GDK_BUTTON_MOTION_MASK,
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
  
  gdk_pixmap = gdk_pixmap_create_from_xpm (window->window, &gdk_pixmap_mask, 
					   &style->bg[GTK_STATE_NORMAL],
					   xpm_file);

  pixmap = gtk_pixmap_new (gdk_pixmap, gdk_pixmap_mask);
  gtk_fixed_put (GTK_FIXED (fixed), pixmap, px,py);
  gtk_widget_show (pixmap);
  
  gtk_widget_shape_combine_mask (window, gdk_pixmap_mask, px,py);

  gtk_widget_set_events (window, 
			 gtk_widget_get_events (window) |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_BUTTON_PRESS_MASK);

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
  root_win = gdk_window_foreign_new (GDK_ROOT_WINDOW ());

  if (!modeller)
    {
      modeller = shape_create_icon ("Modeller.xpm",
				    440, 140, 0,0, GTK_WINDOW_POPUP);

      gtk_signal_connect (GTK_OBJECT (modeller), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &modeller);
      gtk_signal_connect (GTK_OBJECT (modeller), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &modeller);
    }
  else
    gtk_widget_destroy (modeller);

  if (!sheets)
    {
      sheets = shape_create_icon ("FilesQueue.xpm",
				  580, 170, 0,0, GTK_WINDOW_POPUP);

      gtk_signal_connect (GTK_OBJECT (sheets), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &sheets);
      gtk_signal_connect (GTK_OBJECT (sheets), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &sheets);

    }
  else
    gtk_widget_destroy (sheets);

  if (!rings)
    {
      rings = shape_create_icon ("3DRings.xpm",
				 460, 270, 25,25, GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (rings), "destroy",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &rings);
      gtk_signal_connect (GTK_OBJECT (rings), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
			  &rings);
    }
  else
    gtk_widget_destroy (rings);
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

void
destroy_progress (GtkWidget  *widget,
		  GtkWidget **window)
{
  destroy_window (widget, window);
  gtk_timeout_remove (progress_timer);
  progress_timer = 0;
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
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
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

void
color_preview_destroy (GtkWidget  *widget,
		       GtkWidget **window)
{
  gtk_idle_remove (color_idle);
  color_idle = 0;

  destroy_window (widget, window);
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
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
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

void
gray_preview_destroy (GtkWidget  *widget,
		      GtkWidget **window)
{
  gtk_idle_remove (gray_idle);
  gray_idle = 0;

  destroy_window (widget, window);
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
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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
			  GTK_SIGNAL_FUNC(destroy_window),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(destroy_window),
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

void
timeout_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set (GTK_LABEL (label), buffer);
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
  destroy_window (widget, window);
  stop_timeout_test (NULL, NULL);
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
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
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
  destroy_window (widget, window);
  stop_idle_test (NULL, NULL);
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
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
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

void
test_destroy (GtkWidget  *widget,
	      GtkWidget **window)
{
  destroy_window (widget, window);
  gtk_main_quit ();
}

/*
 * Basic Test
 */
void
create_test ()
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(test_destroy),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			  GTK_SIGNAL_FUNC(test_destroy),
			  &window);


      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_border_width (GTK_CONTAINER (window), 0);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show (window);

      g_print ("create_test: start\n");
      gtk_main ();
      g_print ("create_test: done\n");
    }
  else
    gtk_widget_destroy (window);
}


/*
 * Main Window and Exit
 */
void
do_exit ()
{
  gtk_exit (0);
}

void
create_main_window ()
{
  struct {
    char *label;
    void (*func) ();
  } buttons[] =
    {
      { "buttons", create_buttons },
      { "toggle buttons", create_toggle_buttons },
      { "check buttons", create_check_buttons },
      { "radio buttons", create_radio_buttons },
      { "button box", create_button_box },
      { "toolbar", create_toolbar },
      { "handle box", create_handle_box },
      { "reparent", create_reparent },
      { "pixmap", create_pixmap },
      { "tooltips", create_tooltips },
      { "menus", create_menus },
      { "scrolled windows", create_scrolled_windows },
      { "drawing areas", NULL },
      { "entry", create_entry },
      { "list", create_list },
      { "clist", create_clist},
      { "color selection", create_color_selection },
      { "file selection", create_file_selection },
      { "dialog", create_dialog },
      { "miscellaneous", NULL },
      { "range controls", create_range_controls },
      { "rulers", create_rulers },
      { "text", create_text },
      { "notebook", create_notebook },
      { "panes", create_panes },
      { "shapes", create_shapes },
      { "dnd", create_dnd },
      { "progress bar", create_progress_bar },
      { "preview color", create_color_preview },
      { "preview gray", create_gray_preview },
      { "gamma curve", create_gamma_curve },
      { "test scrolling", create_scroll_test },
      { "test selection", create_selection_test },
      { "test timeout", create_timeout_test },
      { "test idle", create_idle_test },
      { "test", create_test },
    };
  int nbuttons = sizeof (buttons) / sizeof (buttons[0]);
  GtkWidget *window;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *scrolled_window;
  GtkWidget *button;
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
		      GTK_SIGNAL_FUNC(gtk_main_quit),
		      NULL);

  box1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_widget_show (box1);

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
		      GTK_SIGNAL_FUNC(do_exit), NULL);
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
  gtk_rc_parse ("testgtkrc");

  create_main_window ();

  gtk_main ();

  return 0;
}
