#include <gtk/gtk.h>
#include "gdk-pixbuf.h"

#include <stdio.h>

GdkInterpType interp_type = GDK_INTERP_BILINEAR;
int overall_alpha = 255;
GdkPixbuf *pixbuf;
GtkWidget *darea;
  
void
set_interp_type (GtkWidget *widget, gpointer data)
{
  interp_type = GPOINTER_TO_UINT (data);
  gtk_widget_queue_draw (darea);
}

void
overall_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  if (adjustment->value != overall_alpha)
    {
      overall_alpha = adjustment->value;
      gtk_widget_queue_draw (darea);
    }
}

gboolean
expose_cb (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkPixbuf *dest;

  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
  
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, event->area.width, event->area.height);

  gdk_pixbuf_composite_color (pixbuf, dest,
			      0, 0, event->area.width, event->area.height,
			      -event->area.x, -event->area.y,
			      (double) widget->allocation.width / gdk_pixbuf_get_width (pixbuf),
			      (double) widget->allocation.height / gdk_pixbuf_get_height (pixbuf),
			      interp_type, overall_alpha,
			      event->area.x, event->area.y, 16, 0xaaaaaa, 0x555555);

  gdk_pixbuf_render_to_drawable (dest, widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
				 0, 0, event->area.x, event->area.y,
				 event->area.width, event->area.height,
				 GDK_RGB_DITHER_NORMAL, event->area.x, event->area.y);
  
  gdk_pixbuf_unref (dest);
  
  return TRUE;
}

int
main(int argc, char **argv)
{
	GtkWidget *window, *vbox;
	GtkWidget *menuitem, *optionmenu, *menu;
	GtkWidget *alignment;
	GtkWidget *hbox, *label, *hscale;
	GtkAdjustment *adjustment;
	GtkRequisition scratch_requisition;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	if (argc != 2) {
		fprintf (stderr, "Usage: testpixbuf-scale FILE\n");
		exit (1);
	}

	pixbuf = gdk_pixbuf_new_from_file (argv[1]);
	if (!pixbuf) {
		fprintf (stderr, "Cannot load %s\n", argv[1]);
		exit(1);
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new_with_label ("NEAREST");
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (set_interp_type),
			    GUINT_TO_POINTER (GDK_INTERP_NEAREST));
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER (menu), menuitem);
	
	menuitem = gtk_menu_item_new_with_label ("BILINEAR");
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (set_interp_type),
			    GUINT_TO_POINTER (GDK_INTERP_BILINEAR));
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER (menu), menuitem);
	
	menuitem = gtk_menu_item_new_with_label ("TILES");
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (set_interp_type),
			    GUINT_TO_POINTER (GDK_INTERP_TILES));
	gtk_container_add (GTK_CONTAINER (menu), menuitem);

	menuitem = gtk_menu_item_new_with_label ("HYPER");
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (set_interp_type),
			    GUINT_TO_POINTER (GDK_INTERP_HYPER));
	gtk_container_add (GTK_CONTAINER (menu), menuitem);

	optionmenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 1);
	
	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new ("Overall Alpha:");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (overall_alpha, 0, 255, 1, 10, 0));
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (overall_changed_cb), NULL);
	
	hscale = gtk_hscale_new (adjustment);
	gtk_scale_set_digits (GTK_SCALE (hscale), 0);
	gtk_box_pack_start (GTK_BOX (hbox), hscale, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (alignment), optionmenu);
	gtk_widget_show_all (vbox);

	/* Compute the size without the drawing area, so we know how big to make the default size */
	gtk_widget_size_request (vbox, &scratch_requisition);

	darea = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (vbox), darea, TRUE, TRUE, 0);

	gtk_signal_connect (GTK_OBJECT (darea), "expose_event",
			    GTK_SIGNAL_FUNC (expose_cb), NULL);

	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_pixbuf_get_width (pixbuf),
				     scratch_requisition.height + gdk_pixbuf_get_height (pixbuf));
	
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
