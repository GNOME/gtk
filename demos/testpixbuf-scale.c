#include "config.h"
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>

GdkInterpType interp_type = GDK_INTERP_BILINEAR;
int overall_alpha = 255;
GdkPixbuf *pixbuf;
GtkWidget *darea;
  
void
set_interp_type (GtkWidget *widget, gpointer data)
{
  guint types[] = { GDK_INTERP_NEAREST,
                    GDK_INTERP_BILINEAR,
                    GDK_INTERP_TILES,
                    GDK_INTERP_HYPER };

  interp_type = types[gtk_combo_box_get_active (GTK_COMBO_BOX (widget))];
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
  cairo_t *cr;

  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
  
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, event->area.width, event->area.height);

  gdk_pixbuf_composite_color (pixbuf, dest,
			      0, 0, event->area.width, event->area.height,
			      -event->area.x, -event->area.y,
			      (double) widget->allocation.width / gdk_pixbuf_get_width (pixbuf),
			      (double) widget->allocation.height / gdk_pixbuf_get_height (pixbuf),
			      interp_type, overall_alpha,
			      event->area.x, event->area.y, 16, 0xaaaaaa, 0x555555);

  cr = gdk_cairo_create (event->window);

  gdk_cairo_set_source_pixbuf (cr, dest, 0, 0);
  gdk_cairo_rectangle (cr, &event->area);
  cairo_fill (cr);

  cairo_destroy (cr);
  g_object_unref (dest);
  
  return TRUE;
}

extern void pixbuf_init();

int
main(int argc, char **argv)
{
	GtkWidget *window, *vbox;
        GtkWidget *combo_box;
	GtkWidget *alignment;
	GtkWidget *hbox, *label, *hscale;
	GtkAdjustment *adjustment;
	GtkRequisition scratch_requisition;
        const gchar *creator;
        GError *error;
        
	pixbuf_init ();

	gtk_init (&argc, &argv);

	if (argc != 2) {
		fprintf (stderr, "Usage: testpixbuf-scale FILE\n");
		exit (1);
	}

        error = NULL;
	pixbuf = gdk_pixbuf_new_from_file (argv[1], &error);
	if (!pixbuf) {
		fprintf (stderr, "Cannot load image: %s\n",
                         error->message);
                g_error_free (error);
		exit(1);
	}

        creator = gdk_pixbuf_get_option (pixbuf, "tEXt::Software");
        if (creator)
                g_print ("%s was created by '%s'\n", argv[1], creator);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (window, "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

        combo_box = gtk_combo_box_text_new ();

        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), "NEAREST");
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), "BILINEAR");
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), "TILES");
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), "HYPER");

        gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 1);

        g_signal_connect (combo_box, "changed",
                          G_CALLBACK (set_interp_type),
                          NULL);
	
	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new ("Overall Alpha:");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (overall_alpha, 0, 255, 1, 10, 0));
	g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (overall_changed_cb), NULL);
	
	hscale = gtk_hscale_new (adjustment);
	gtk_scale_set_digits (GTK_SCALE (hscale), 0);
	gtk_box_pack_start (GTK_BOX (hbox), hscale, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (alignment), combo_box);
	gtk_widget_show_all (vbox);

	/* Compute the size without the drawing area, so we know how big to make the default size */
	gtk_widget_size_request (vbox, &scratch_requisition);

	darea = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (vbox), darea, TRUE, TRUE, 0);

	g_signal_connect (darea, "expose_event",
			  G_CALLBACK (expose_cb), NULL);

	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_pixbuf_get_width (pixbuf),
				     scratch_requisition.height + gdk_pixbuf_get_height (pixbuf));
	
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
