#include <gtk/gtk.h>


static void
horizontal_policy_changed (GtkComboBox *combo_box,
			   GtkViewport *viewport)
{
  GtkScrollablePolicy policy = gtk_combo_box_get_active (combo_box);

  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (viewport), policy);
}

static void
vertical_policy_changed (GtkComboBox *combo_box,
			 GtkViewport *viewport)
{
  GtkScrollablePolicy policy = gtk_combo_box_get_active (combo_box);

  gtk_scrollable_set_vscroll_policy (GTK_SCROLLABLE (viewport), policy);
}

static void
content_width_changed (GtkSpinButton *spin_button,
                       gpointer       data)
{
  GtkScrolledWindow *swindow = data;
  gdouble value;

  value = gtk_spin_button_get_value (spin_button);
  gtk_scrolled_window_set_min_content_width (swindow, (gint)value);
}

static void
content_height_changed (GtkSpinButton *spin_button,
                        gpointer       data)
{
  GtkScrolledWindow *swindow = data;
  gdouble value;

  value = gtk_spin_button_get_value (spin_button);
  gtk_scrolled_window_set_min_content_height (swindow, (gint)value);
}

static void
kinetic_scrolling_changed (GtkToggleButton *toggle_button,
                           gpointer         data)
{
  GtkScrolledWindow *swindow = data;
  gboolean enabled = gtk_toggle_button_get_active (toggle_button);

  gtk_scrolled_window_set_kinetic_scrolling (swindow, enabled);
}

static void
add_row (GtkButton  *button,
         GtkListBox *listbox)
{
  GtkWidget *row;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW, NULL);
  gtk_container_add (GTK_CONTAINER (row), gtk_label_new ("test"));
  gtk_container_add (GTK_CONTAINER (listbox), row);
}

static void
remove_row (GtkButton  *button,
            GtkListBox *listbox)
{
  GList *children, *last;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  last = g_list_last (children);

  if (last)
    gtk_container_remove (GTK_CONTAINER (listbox), last->data);

  g_list_free (children);
}

static void
scrollable_policy (void)
{
  GtkWidget *window, *swindow, *hbox, *vbox, *frame, *cntl, *listbox;
  GtkWidget *viewport, *label, *expander, *widget, *popover;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox   = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  vbox   = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  gtk_widget_show (vbox);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  frame = gtk_frame_new ("Scrolled Window");
  gtk_widget_set_hexpand (frame, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), frame);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_widget_show (swindow);
  gtk_container_add (GTK_CONTAINER (frame), swindow);

  viewport = gtk_viewport_new (NULL, NULL);
  label = gtk_label_new ("Here is a wrapping label with a minimum width-chars of 40 and "
			 "a natural max-width-chars of 100 to demonstrate the usage of "
			 "scrollable widgets \"hscroll-policy\" and \"vscroll-policy\" "
			 "properties. Note also that when playing with the window height, "
			 "one can observe that the vscrollbar disappears as soon as there "
			 "is enough height to fit the content vertically if the window were "
			 "to be allocated a width without a vscrollbar present");

  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars  (GTK_LABEL (label), 40);
  gtk_label_set_max_width_chars  (GTK_LABEL (label), 100);

  gtk_widget_show (label);
  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), label);
  gtk_container_add (GTK_CONTAINER (swindow), viewport);

  /* Add controls here */
  expander = gtk_expander_new ("Controls");
  gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
  cntl = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_show (cntl);
  gtk_widget_show (expander);
  gtk_container_add (GTK_CONTAINER (expander), cntl);
  gtk_container_add (GTK_CONTAINER (vbox), expander);

  /* Add Horizontal policy control here */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_show (hbox);

  widget = gtk_label_new ("hscroll-policy");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Minimum");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Natural");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_set_hexpand (widget, TRUE);

  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (cntl), hbox);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (horizontal_policy_changed), viewport);

  /* Add Vertical policy control here */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_show (hbox);

  widget = gtk_label_new ("vscroll-policy");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Minimum");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Natural");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_set_hexpand (widget, TRUE);

  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (cntl), hbox);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (vertical_policy_changed), viewport);

  /* Content size controls */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("min-content-width");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_spin_button_new_with_range (100.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (cntl), hbox);
  gtk_widget_show (widget);
  gtk_widget_show (hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (content_width_changed), swindow);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("min-content-height");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_spin_button_new_with_range (100.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (cntl), hbox);
  gtk_widget_show (widget);
  gtk_widget_show (hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (content_height_changed), swindow);

  /* Add Kinetic scrolling control here */
  widget = gtk_check_button_new_with_label ("Kinetic scrolling");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (cntl), widget);
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (kinetic_scrolling_changed), swindow);

  gtk_widget_show (window);

  /* Popover */
  popover = gtk_popover_new (NULL);

  widget = gtk_menu_button_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (widget), popover);
  gtk_menu_button_set_label (GTK_MENU_BUTTON (widget), "Popover");
  gtk_container_add (GTK_CONTAINER (cntl), widget);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (popover), vbox);

  /* Popover's scrolled window */
  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add (GTK_CONTAINER (vbox), swindow);

  /* Listbox */
  listbox = gtk_list_box_new ();
  gtk_container_add (GTK_CONTAINER (swindow), listbox);
  gtk_widget_show (listbox);

  /* Min content */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("min-content-width");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_spin_button_new_with_range (0.0, 150.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "min-content-width",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  widget = gtk_label_new ("min-content-height");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);


  widget = gtk_spin_button_new_with_range (0.0, 150.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "min-content-height",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Max content */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("max-content-width");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_spin_button_new_with_range (250.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "max-content-width",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  widget = gtk_label_new ("max-content-height");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  widget = gtk_spin_button_new_with_range (250.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "max-content-height",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Add and Remove buttons */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_button_new_with_label ("Remove");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);

  g_signal_connect (widget, "clicked",
                    G_CALLBACK (remove_row), listbox);

  widget = gtk_button_new_with_label ("Add");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), widget);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  g_signal_connect (widget, "clicked",
                    G_CALLBACK (add_row), listbox);
}


int
main (int argc, char *argv[])
{
  gtk_init ();

  scrollable_policy ();

  gtk_main ();

  return 0;
}
