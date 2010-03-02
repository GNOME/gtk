
#include <gtk/gtk.h>

GtkWidget *hscale, *vscale;

static void cb_pos_menu_select( GtkWidget       *item,
                                GtkPositionType  pos )
{
    /* Set the value position on both scale widgets */
    gtk_scale_set_value_pos (GTK_SCALE (hscale), pos);
    gtk_scale_set_value_pos (GTK_SCALE (vscale), pos);
}

static void cb_update_menu_select( GtkWidget     *item,
                                   GtkUpdateType  policy )
{
    /* Set the update policy for both scale widgets */
    gtk_range_set_update_policy (GTK_RANGE (hscale), policy);
    gtk_range_set_update_policy (GTK_RANGE (vscale), policy);
}

static void cb_digits_scale( GtkAdjustment *adj )
{
    /* Set the number of decimal places to which adj->value is rounded */
    gtk_scale_set_digits (GTK_SCALE (hscale), (gint) adj->value);
    gtk_scale_set_digits (GTK_SCALE (vscale), (gint) adj->value);
}

static void cb_page_size( GtkAdjustment *get,
                          GtkAdjustment *set )
{
    /* Set the page size and page increment size of the sample
     * adjustment to the value specified by the "Page Size" scale */
    set->page_size = get->value;
    set->page_increment = get->value;

    /* This sets the adjustment and makes it emit the "changed" signal to
       reconfigure all the widgets that are attached to this signal.  */
    gtk_adjustment_set_value (set, CLAMP (set->value,
					  set->lower,
					  (set->upper - set->page_size)));
    g_signal_emit_by_name(G_OBJECT(set), "changed");
}

static void cb_draw_value( GtkToggleButton *button )
{
    /* Turn the value display on the scale widgets off or on depending
     *  on the state of the checkbutton */
    gtk_scale_set_draw_value (GTK_SCALE (hscale), button->active);
    gtk_scale_set_draw_value (GTK_SCALE (vscale), button->active);
}

/* Convenience functions */

static GtkWidget *make_menu_item ( gchar     *name,
                                   GCallback  callback,
                                   gpointer   data )
{
    GtkWidget *item;

    item = gtk_menu_item_new_with_label (name);
    g_signal_connect (item, "activate",
	              callback, (gpointer) data);
    gtk_widget_show (item);

    return item;
}

static void scale_set_default_values( GtkScale *scale )
{
    gtk_range_set_update_policy (GTK_RANGE (scale),
                                 GTK_UPDATE_CONTINUOUS);
    gtk_scale_set_digits (scale, 1);
    gtk_scale_set_value_pos (scale, GTK_POS_TOP);
    gtk_scale_set_draw_value (scale, TRUE);
}

/* makes the sample window */

static void create_range_controls( void )
{
    GtkWidget *window;
    GtkWidget *box1, *box2, *box3;
    GtkWidget *button;
    GtkWidget *scrollbar;
    GtkWidget *separator;
    GtkWidget *opt, *menu, *item;
    GtkWidget *label;
    GtkWidget *scale;
    GtkObject *adj1, *adj2;

    /* Standard window-creating stuff */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_main_quit),
                      NULL);
    gtk_window_set_title (GTK_WINDOW (window), "range controls");

    box1 = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), box1);
    gtk_widget_show (box1);

    box2 = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);

    /* value, lower, upper, step_increment, page_increment, page_size */
    /* Note that the page_size value only makes a difference for
     * scrollbar widgets, and the highest value you'll get is actually
     * (upper - page_size). */
    adj1 = gtk_adjustment_new (0.0, 0.0, 101.0, 0.1, 1.0, 1.0);

    vscale = gtk_vscale_new (GTK_ADJUSTMENT (adj1));
    scale_set_default_values (GTK_SCALE (vscale));
    gtk_box_pack_start (GTK_BOX (box2), vscale, TRUE, TRUE, 0);
    gtk_widget_show (vscale);

    box3 = gtk_vbox_new (FALSE, 10);
    gtk_box_pack_start (GTK_BOX (box2), box3, TRUE, TRUE, 0);
    gtk_widget_show (box3);

    /* Reuse the same adjustment */
    hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj1));
    gtk_widget_set_size_request (GTK_WIDGET (hscale), 200, -1);
    scale_set_default_values (GTK_SCALE (hscale));
    gtk_box_pack_start (GTK_BOX (box3), hscale, TRUE, TRUE, 0);
    gtk_widget_show (hscale);

    /* Reuse the same adjustment again */
    scrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (adj1));
    /* Notice how this causes the scales to always be updated
     * continuously when the scrollbar is moved */
    gtk_range_set_update_policy (GTK_RANGE (scrollbar),
                                 GTK_UPDATE_CONTINUOUS);
    gtk_box_pack_start (GTK_BOX (box3), scrollbar, TRUE, TRUE, 0);
    gtk_widget_show (scrollbar);

    box2 = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);

    /* A checkbutton to control whether the value is displayed or not */
    button = gtk_check_button_new_with_label("Display value on scale widgets");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    g_signal_connect (button, "toggled",
                      G_CALLBACK (cb_draw_value), NULL);
    gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    gtk_widget_show (button);

    box2 = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);

    /* An option menu to change the position of the value */
    label = gtk_label_new ("Scale Value Position:");
    gtk_box_pack_start (GTK_BOX (box2), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    opt = gtk_option_menu_new ();
    menu = gtk_menu_new ();

    item = make_menu_item ("Top",
                           G_CALLBACK (cb_pos_menu_select),
                           GINT_TO_POINTER (GTK_POS_TOP));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = make_menu_item ("Bottom", G_CALLBACK (cb_pos_menu_select),
                           GINT_TO_POINTER (GTK_POS_BOTTOM));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = make_menu_item ("Left", G_CALLBACK (cb_pos_menu_select),
                           GINT_TO_POINTER (GTK_POS_LEFT));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = make_menu_item ("Right", G_CALLBACK (cb_pos_menu_select),
                           GINT_TO_POINTER (GTK_POS_RIGHT));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_option_menu_set_menu (GTK_OPTION_MENU (opt), menu);
    gtk_box_pack_start (GTK_BOX (box2), opt, TRUE, TRUE, 0);
    gtk_widget_show (opt);

    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);

    box2 = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);

    /* Yet another option menu, this time for the update policy of the
     * scale widgets */
    label = gtk_label_new ("Scale Update Policy:");
    gtk_box_pack_start (GTK_BOX (box2), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    opt = gtk_option_menu_new ();
    menu = gtk_menu_new ();

    item = make_menu_item ("Continuous",
                           G_CALLBACK (cb_update_menu_select),
                           GINT_TO_POINTER (GTK_UPDATE_CONTINUOUS));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = make_menu_item ("Discontinuous",
                           G_CALLBACK (cb_update_menu_select),
                           GINT_TO_POINTER (GTK_UPDATE_DISCONTINUOUS));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = make_menu_item ("Delayed",
                           G_CALLBACK (cb_update_menu_select),
                           GINT_TO_POINTER (GTK_UPDATE_DELAYED));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_option_menu_set_menu (GTK_OPTION_MENU (opt), menu);
    gtk_box_pack_start (GTK_BOX (box2), opt, TRUE, TRUE, 0);
    gtk_widget_show (opt);

    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);

    box2 = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);

    /* An HScale widget for adjusting the number of digits on the
     * sample scales. */
    label = gtk_label_new ("Scale Digits:");
    gtk_box_pack_start (GTK_BOX (box2), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    adj2 = gtk_adjustment_new (1.0, 0.0, 5.0, 1.0, 1.0, 0.0);
    g_signal_connect (adj2, "value_changed",
                      G_CALLBACK (cb_digits_scale), NULL);
    scale = gtk_hscale_new (GTK_ADJUSTMENT (adj2));
    gtk_scale_set_digits (GTK_SCALE (scale), 0);
    gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
    gtk_widget_show (scale);

    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);

    box2 = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);

    /* And, one last HScale widget for adjusting the page size of the
     * scrollbar. */
    label = gtk_label_new ("Scrollbar Page Size:");
    gtk_box_pack_start (GTK_BOX (box2), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    adj2 = gtk_adjustment_new (1.0, 1.0, 101.0, 1.0, 1.0, 0.0);
    g_signal_connect (adj2, "value-changed",
                      G_CALLBACK (cb_page_size), (gpointer) adj1);
    scale = gtk_hscale_new (GTK_ADJUSTMENT (adj2));
    gtk_scale_set_digits (GTK_SCALE (scale), 0);
    gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
    gtk_widget_show (scale);

    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
    gtk_widget_show (separator);

    box2 = gtk_vbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
    gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
    gtk_widget_show (box2);

    button = gtk_button_new_with_label ("Quit");
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (gtk_main_quit),
                              NULL);
    gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_grab_default (button);
    gtk_widget_show (button);

    gtk_widget_show (window);
}

int main( int   argc,
          char *argv[] )
{
    gtk_init (&argc, &argv);

    create_range_controls ();

    gtk_main ();

    return 0;
}

