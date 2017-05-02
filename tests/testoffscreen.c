/*
 * testoffscreen.c
 */

#undef GTK_DISABLE_DEPRECATED

#include <math.h>
#include <gtk/gtk.h>
#include "gtkoffscreenbox.h"


static void
combo_changed_cb (GtkWidget *combo,
		  gpointer   data)
{
  GtkWidget *label = GTK_WIDGET (data);
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  gtk_label_set_ellipsize (GTK_LABEL (label), (PangoEllipsizeMode)active);
}

static gboolean
layout_expose_handler (GtkWidget      *widget,
                       GdkEventExpose *event)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GdkWindow *bin_window = gtk_layout_get_bin_window (layout);
  cairo_t *cr;

  gint i,j;
  gint imin, imax, jmin, jmax;

  if (event->window != bin_window)
    return FALSE;

  imin = (event->area.x) / 10;
  imax = (event->area.x + event->area.width + 9) / 10;

  jmin = (event->area.y) / 10;
  jmax = (event->area.y + event->area.height + 9) / 10;

  cr = gdk_cairo_create (bin_window);

  for (i = imin; i < imax; i++)
    for (j = jmin; j < jmax; j++)
      if ((i + j) % 2)
        cairo_rectangle (cr,
                         10 * i, 10 * j,
                         1 + i % 10, 1 + j % 10);

  cairo_fill (cr);

  cairo_destroy (cr);

  return FALSE;
}

static gboolean
scroll_layout (gpointer data)
{
  GtkWidget *layout = data;
  GtkAdjustment *adj;

  adj = gtk_layout_get_hadjustment (GTK_LAYOUT (layout));
  gtk_adjustment_set_value (adj,
			    gtk_adjustment_get_value (adj) + 5.0);
  return TRUE;
}

static guint layout_timeout;

static void
create_layout (GtkWidget *vbox)
{
  GtkWidget *layout;
  GtkWidget *scrolledwindow;
  GtkWidget *button;
  gchar buf[16];
  gint i, j;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);
  gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (scrolledwindow),
				     GTK_CORNER_TOP_RIGHT);

  gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);

  layout = gtk_layout_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), layout);

  /* We set step sizes here since GtkLayout does not set
   * them itself.
   */
  GTK_LAYOUT (layout)->hadjustment->step_increment = 10.0;
  GTK_LAYOUT (layout)->vadjustment->step_increment = 10.0;

  gtk_widget_set_events (layout, GDK_EXPOSURE_MASK);
  g_signal_connect (layout, "expose_event",
		    G_CALLBACK (layout_expose_handler),
                    NULL);

  gtk_layout_set_size (GTK_LAYOUT (layout), 1600, 128000);

  for (i = 0 ; i < 16 ; i++)
    for (j = 0 ; j < 16 ; j++)
      {
	g_snprintf (buf, sizeof (buf), "Button %d, %d", i, j);

	if ((i + j) % 2)
	  button = gtk_button_new_with_label (buf);
	else
	  button = gtk_label_new (buf);

	gtk_layout_put (GTK_LAYOUT (layout), button,
			j * 100, i * 100);
      }

  for (i = 16; i < 1280; i++)
    {
      g_snprintf (buf, sizeof (buf), "Button %d, %d", i, 0);

      if (i % 2)
	button = gtk_button_new_with_label (buf);
      else
	button = gtk_label_new (buf);

      gtk_layout_put (GTK_LAYOUT (layout), button,
		      0, i * 100);
    }

  layout_timeout = g_timeout_add (1000, scroll_layout, layout);
}

static void
create_treeview (GtkWidget *vbox)
{
  GtkWidget *scrolledwindow;
  GtkListStore *store;
  GtkWidget *tree_view;
  GSList *stock_ids;
  GSList *list;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);

  gtk_container_add (GTK_CONTAINER (scrolledwindow), tree_view);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view), -1,
                                               "Icon",
                                               gtk_cell_renderer_pixbuf_new (),
                                               "stock-id", 0,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view), -1,
                                               "Label",
                                               gtk_cell_renderer_text_new (),
                                               "text", 1,
                                               NULL);

  stock_ids = gtk_stock_list_ids ();

  for (list = stock_ids; list; list = g_slist_next (list))
    {
      const gchar *stock_id = list->data;
      GtkStockItem item;

      if (gtk_stock_lookup (stock_id, &item))
        {
          gtk_list_store_insert_with_values (store, NULL, -1,
                                             0, item.stock_id,
                                             1, item.label,
                                             -1);
        }
    }

  g_slist_foreach (stock_ids, (GFunc) g_free, NULL);
  g_slist_free (stock_ids);
}

static GtkWidget *
create_widgets (void)
{
  GtkWidget *main_hbox, *main_vbox;
  GtkWidget *vbox, *hbox, *label, *combo, *entry, *button, *cb;
  GtkWidget *sw, *text_view;

  main_vbox = gtk_vbox_new (0, FALSE);

  main_hbox = gtk_hbox_new (0, FALSE);
  gtk_box_pack_start (GTK_BOX (main_vbox), main_hbox, TRUE, TRUE, 0);

  vbox = gtk_vbox_new (0, FALSE);
  gtk_box_pack_start (GTK_BOX (main_hbox), vbox, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (0, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new ("This label may be ellipsized\nto make it fit.");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "NONE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "START");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "MIDDLE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "END");
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);

  g_signal_connect (combo, "changed",
                    G_CALLBACK (combo_changed_cb),
                    label);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "an entry - lots of text.... lots of text.... lots of text.... lots of text.... ");
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  label = gtk_label_new ("Label after entry.");
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Button");
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_check_button_new_with_mnemonic ("_Check button");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  cb = gtk_combo_box_text_new ();
  entry = gtk_entry_new ();
  gtk_widget_show (entry);
  gtk_container_add (GTK_CONTAINER (cb), entry);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), "item0");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), "item1");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), "item1");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), "item2");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), "item2");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), "item2");
  gtk_entry_set_text (GTK_ENTRY (entry), "hello world ♥ foo");
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
  gtk_box_pack_start (GTK_BOX (vbox), cb, TRUE, TRUE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  text_view = gtk_text_view_new ();
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (sw), text_view);

  create_layout (vbox);

  create_treeview (main_hbox);

  return main_vbox;
}


static void
scale_changed (GtkRange        *range,
	       GtkOffscreenBox *offscreen_box)
{
  gtk_offscreen_box_set_angle (offscreen_box, gtk_range_get_value (range));
}

static GtkWidget *scale = NULL;

static void
remove_clicked (GtkButton *button,
                GtkWidget *widget)
{
  gtk_widget_destroy (widget);
  g_source_remove (layout_timeout);

  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
  gtk_widget_set_sensitive (scale, FALSE);
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *window, *widget, *vbox, *button;
  GtkWidget *offscreen = NULL;
  gboolean use_offscreen;

  gtk_init (&argc, &argv);

  use_offscreen = argc == 1;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300,300);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  vbox = gtk_vbox_new (0, FALSE);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  scale = gtk_hscale_new_with_range (0,
				     G_PI * 2,
				     0.01);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Remove child 2");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  if (use_offscreen)
    {
      offscreen = gtk_offscreen_box_new ();

      g_signal_connect (scale, "value_changed",
                        G_CALLBACK (scale_changed),
                        offscreen);
    }
  else
    {
      offscreen = gtk_vpaned_new ();
    }

  gtk_box_pack_start (GTK_BOX (vbox), offscreen, TRUE, TRUE, 0);

  widget = create_widgets ();
  if (use_offscreen)
    gtk_offscreen_box_add1 (GTK_OFFSCREEN_BOX (offscreen),
			    widget);
  else
    gtk_paned_add1 (GTK_PANED (offscreen), widget);

  widget = create_widgets ();
  if (1)
    {
      GtkWidget *widget2, *box2, *offscreen2;

      offscreen2 = gtk_offscreen_box_new ();
      gtk_box_pack_start (GTK_BOX (widget), offscreen2, FALSE, FALSE, 0);

      g_signal_connect (scale, "value_changed",
                        G_CALLBACK (scale_changed),
                        offscreen2);

      box2 = gtk_vbox_new (FALSE, 0);
      gtk_offscreen_box_add2 (GTK_OFFSCREEN_BOX (offscreen2), box2);

      widget2 = gtk_button_new_with_label ("Offscreen in offscreen");
      gtk_box_pack_start (GTK_BOX (box2), widget2, FALSE, FALSE, 0);

      widget2 = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (widget2), "Offscreen in offscreen");
      gtk_box_pack_start (GTK_BOX (box2), widget2, FALSE, FALSE, 0);
    }

  if (use_offscreen)
    gtk_offscreen_box_add2 (GTK_OFFSCREEN_BOX (offscreen), widget);
  else
    gtk_paned_add2 (GTK_PANED (offscreen), widget);

  gtk_widget_show_all (window);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (remove_clicked),
                    widget);

  /* redirect */
  if (0)
    {
      GtkWidget *redirect_win;

      redirect_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (redirect_win), 400,400);
      gtk_widget_show (redirect_win);
      gtk_widget_realize (redirect_win);
      gtk_widget_realize (window);
      gdk_window_redirect_to_drawable (window->window,
				       GDK_DRAWABLE (redirect_win->window),
				       0, 0, 0, 0, -1, -1);
    }

  gtk_main ();

  return 0;
}
