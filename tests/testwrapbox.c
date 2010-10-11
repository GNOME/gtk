/* testwrapbox.c
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

enum {
  SIMPLE_ITEMS = 0,
  WRAPPY_ITEMS,
  STOCK_ITEMS
};

#define INITIAL_ALLOCATION_MODE GTK_WRAP_ALLOCATE_HOMOGENEOUS
#define INITIAL_SPREADING       GTK_WRAP_BOX_SPREAD_START
#define INITIAL_MINIMUM_LENGTH  3
#define INITIAL_HSPACING        2
#define INITIAL_VSPACING        2

static GtkWrapBox    *the_wrapbox       = NULL;
static gint           items_type       = SIMPLE_ITEMS;
static GtkOrientation text_orientation = GTK_ORIENTATION_HORIZONTAL;
static gboolean       items_xexpand    = TRUE;
static gboolean       items_yexpand    = TRUE;


static void
populate_wrapbox_simple (GtkWrapBox *wrapbox)
{
  GtkWidget *widget, *frame;
  gint i;

  for (i = 0; i < 30; i++)
    {
      gchar *text = g_strdup_printf ("Item %02d", i);

      widget = gtk_label_new (text);
      frame  = gtk_frame_new (NULL);
      gtk_widget_show (widget);
      gtk_widget_show (frame);

      gtk_container_add (GTK_CONTAINER (frame), widget);

      if (text_orientation == GTK_ORIENTATION_VERTICAL)
        gtk_label_set_angle (GTK_LABEL (widget), 90);

      if (items_xexpand)
        gtk_widget_set_hexpand (frame, TRUE);
      if (items_yexpand)
        gtk_widget_set_vexpand (frame, TRUE);

      gtk_wrap_box_insert_child (GTK_WRAP_BOX (wrapbox), frame, -1);

      g_free (text);
    }
}

static void
populate_wrapbox_wrappy (GtkWrapBox *wrapbox)
{
  GtkWidget *widget, *frame;
  gint i;

  const gchar *strings[] = {
    "These are", "some wrappy label", "texts", "of various", "lengths.",
    "They should always be", "shown", "consecutively. Except it's",
    "hard to say", "where exactly the", "label", "will wrap", "and where exactly",
    "the actual", "container", "will wrap.", "This label is really really really long !", 
    "Let's add some more", "labels to the",
    "mix. Just to", "make sure we", "got something to work", "with here."
  };

  for (i = 0; i < G_N_ELEMENTS (strings); i++)
    {
      widget = gtk_label_new (strings[i]);
      frame  = gtk_frame_new (NULL);
      gtk_widget_show (widget);
      gtk_widget_show (frame);

      if (text_orientation == GTK_ORIENTATION_VERTICAL)
        gtk_label_set_angle (GTK_LABEL (widget), 90);

      gtk_container_add (GTK_CONTAINER (frame), widget);

      gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
      gtk_label_set_line_wrap_mode (GTK_LABEL (widget), PANGO_WRAP_WORD);
      gtk_label_set_width_chars (GTK_LABEL (widget), 10);

      if (items_xexpand)
        gtk_widget_set_hexpand (frame, TRUE);
      if (items_yexpand)
        gtk_widget_set_vexpand (frame, TRUE);

      gtk_wrap_box_insert_child (GTK_WRAP_BOX (wrapbox), frame, -1);
    }
}


static void
populate_wrapbox_stock (GtkWrapBox *wrapbox)
{
  GtkWidget *widget;
  static GSList *stock_ids = NULL;
  GSList *l;
  gint i;

  if (!stock_ids)
    stock_ids = gtk_stock_list_ids ();

  for (i = 0, l = stock_ids; i < 30 && l != NULL; i++, l = l->next)
    {
      gchar *stock_id = l->data;

      widget = gtk_button_new_from_stock (stock_id);
      gtk_widget_show (widget);

      if (items_xexpand)
        gtk_widget_set_hexpand (widget, TRUE);
      if (items_yexpand)
        gtk_widget_set_vexpand (widget, TRUE);

      gtk_wrap_box_insert_child (GTK_WRAP_BOX (wrapbox), widget, -1);
    }
}

static void 
populate_items (GtkWrapBox *wrapbox)
{
  GList *children, *l;
  
  /* Remove all children first */
  children = gtk_container_get_children (GTK_CONTAINER (wrapbox));
  for (l = children; l; l = l->next)
    {
      GtkWidget *child = l->data;

      gtk_container_remove (GTK_CONTAINER (wrapbox), child);
    }
  g_list_free (children);

  if (items_type == SIMPLE_ITEMS)
    populate_wrapbox_simple (wrapbox);
  else if (items_type == WRAPPY_ITEMS)
    populate_wrapbox_wrappy (wrapbox);
  else if (items_type == STOCK_ITEMS)
    populate_wrapbox_stock (wrapbox);
}


static void
mode_changed (GtkComboBox   *box,
              GtkWrapBox    *wrapbox)
{
  GtkWrapAllocationMode mode = gtk_combo_box_get_active (box);
  
  gtk_wrap_box_set_allocation_mode (wrapbox, mode);
}

static void
horizontal_spreading_changed (GtkComboBox   *box,
			      GtkWrapBox    *wrapbox)
{
  GtkWrapBoxSpreading spreading = gtk_combo_box_get_active (box);
  
  gtk_wrap_box_set_horizontal_spreading (wrapbox, spreading);
}

static void
vertical_spreading_changed (GtkComboBox   *box,
			    GtkWrapBox    *wrapbox)
{
  GtkWrapBoxSpreading spreading = gtk_combo_box_get_active (box);
  
  gtk_wrap_box_set_vertical_spreading (wrapbox, spreading);
}

static void
orientation_changed (GtkComboBox   *box,
                     GtkWrapBox *wrapbox)
{
  GtkOrientation orientation = gtk_combo_box_get_active (box);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (wrapbox), orientation);
}

static void
line_length_changed (GtkSpinButton *spin,
                     GtkWrapBox *wrapbox)
{
  gint length = gtk_spin_button_get_value_as_int (spin);
  
  gtk_wrap_box_set_minimum_line_children (wrapbox, length);
}

static void
spacing_changed (GtkSpinButton *button,
                 gpointer       data)
{
  GtkOrientation orientation = GPOINTER_TO_INT (data);
  gint           state = gtk_spin_button_get_value_as_int (button);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_wrap_box_set_horizontal_spacing (the_wrapbox, state);
  else
    gtk_wrap_box_set_vertical_spacing (the_wrapbox, state);
}


static void
items_changed (GtkComboBox   *box,
               GtkWrapBox *wrapbox)
{
  items_type = gtk_combo_box_get_active (box);

  populate_items (wrapbox);
}

static void
text_orientation_changed (GtkComboBox   *box,
                          GtkWrapBox *wrapbox)
{
  text_orientation = gtk_combo_box_get_active (box);
  
  populate_items (wrapbox);
}

static void
child_option_toggled (GtkToggleButton *button,
                      gboolean        *state)
{
  *state = gtk_toggle_button_get_active (button);

  populate_items (the_wrapbox);
}

static GtkWidget *
create_window (void)
{
  GtkWidget *window, *hbox, *vbox, *frame, *wrapbox_cntl, *items_cntl;
  GtkWidget *wrapbox, *widget, *expander, *swindow;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox   = gtk_hbox_new (FALSE, 2);
  vbox   = gtk_vbox_new (FALSE, 6);

  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  gtk_widget_show (vbox);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

  frame = gtk_frame_new ("Wrap Box");
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_widget_show (swindow);
  gtk_container_add (GTK_CONTAINER (frame), swindow);

  wrapbox = gtk_wrap_box_new (INITIAL_ALLOCATION_MODE, INITIAL_SPREADING,
			      INITIAL_SPREADING, INITIAL_HSPACING, INITIAL_VSPACING);
  the_wrapbox = (GtkWrapBox *)wrapbox;
  gtk_wrap_box_set_minimum_line_children (GTK_WRAP_BOX (wrapbox), INITIAL_MINIMUM_LENGTH);
  gtk_widget_show (wrapbox);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (swindow), wrapbox);

  /* Add Wrapbox test control frame */
  expander = gtk_expander_new ("Wrap Box controls");
  gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
  wrapbox_cntl = gtk_vbox_new (FALSE, 2);
  gtk_widget_show (wrapbox_cntl);
  gtk_widget_show (expander);
  gtk_container_add (GTK_CONTAINER (expander), wrapbox_cntl);
  gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, FALSE, 0);

  /* Add Allocation mode control */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Wrap Freely");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Align items");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Homogeneous");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), INITIAL_ALLOCATION_MODE);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the wrapbox allocation mode");
  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (mode_changed), wrapbox);

  /* Add Spreading controls */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread Start");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread End");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread Even");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread Expand");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), INITIAL_SPREADING);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the horizontal spreading mode");
  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (horizontal_spreading_changed), wrapbox);

  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread Start");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread End");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread Even");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Spread Expand");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), INITIAL_SPREADING);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the vertical spreading mode");
  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (vertical_spreading_changed), wrapbox);

  /* Add Orientation control */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Horizontal");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the wrapbox orientation");
  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), wrapbox);

  /* Add minimum line length in items control */
  widget = gtk_spin_button_new_with_range (1, 10, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), INITIAL_MINIMUM_LENGTH);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the minimum amount of items per line before wrapping");
  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (line_length_changed), wrapbox);
  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (line_length_changed), wrapbox);

  /* Add horizontal/vertical spacing controls */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);

  widget = gtk_label_new ("H Spacing");
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

  widget = gtk_spin_button_new_with_range (0, 30, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), INITIAL_HSPACING);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the horizontal spacing between children");
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (spacing_changed), GINT_TO_POINTER (GTK_ORIENTATION_HORIZONTAL));
  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (spacing_changed), GINT_TO_POINTER (GTK_ORIENTATION_HORIZONTAL));

  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), hbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);

  widget = gtk_label_new ("V Spacing");
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

  widget = gtk_spin_button_new_with_range (0, 30, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), INITIAL_VSPACING);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the vertical spacing between children");
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (spacing_changed), GINT_TO_POINTER (GTK_ORIENTATION_VERTICAL));
  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (spacing_changed), GINT_TO_POINTER (GTK_ORIENTATION_VERTICAL));

  gtk_box_pack_start (GTK_BOX (wrapbox_cntl), hbox, FALSE, FALSE, 0);


  /* Add test items control frame */
  expander = gtk_expander_new ("Test item controls");
  gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
  items_cntl = gtk_vbox_new (FALSE, 2);
  gtk_widget_show (items_cntl);
  gtk_widget_show (expander);
  gtk_container_add (GTK_CONTAINER (expander), items_cntl);
  gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, FALSE, 0);

  /* Add Items control */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Simple");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Wrappy");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Stock");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the item set to use");
  gtk_box_pack_start (GTK_BOX (items_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (items_changed), wrapbox);


  /* Add Text Orientation control */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Horizontal");
  gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set the item's text orientation (cant be done for stock buttons)");
  gtk_box_pack_start (GTK_BOX (items_cntl), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (text_orientation_changed), wrapbox);


  /* Add expand options */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);

  widget = gtk_check_button_new_with_label ("X Expand");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set whether the items expand horizontally");
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (child_option_toggled), &items_xexpand);


  widget = gtk_check_button_new_with_label ("Y Expand");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);

  gtk_widget_set_tooltip_text (widget, "Set whether the items expand vertically");
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (child_option_toggled), &items_yexpand);

  gtk_box_pack_start (GTK_BOX (items_cntl), hbox, FALSE, FALSE, 0);

  populate_items (GTK_WRAP_BOX (wrapbox));

  /* This line was added only for the convenience of reproducing
   * a height-for-width inside GtkScrolledWindow bug (bug 629778).
   *   -Tristan
   */
  gtk_window_set_default_size (GTK_WINDOW (window), 390, -1);

  return window;
}



int
main (int argc, char *argv[])
{
  GtkWidget *window;

  gtk_init (&argc, &argv);

  window = create_window ();

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), window);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
