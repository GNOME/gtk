#include "config.h"
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>

GdkInterpType interp_type = GDK_INTERP_BILINEAR;
int overall_alpha = 255;
GdkPixbuf *pixbuf;
GtkWidget *darea;
  
static void
set_interp_type (GtkWidget *widget, gpointer data)
{
  guint types[] = { GDK_INTERP_NEAREST,
                    GDK_INTERP_BILINEAR,
                    GDK_INTERP_TILES,
                    GDK_INTERP_HYPER };

  interp_type = types[gtk_combo_box_get_active (GTK_COMBO_BOX (widget))];
  gtk_widget_queue_draw (darea);
}

static void
overall_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  if (gtk_adjustment_get_value (adjustment) != overall_alpha)
    {
      overall_alpha = gtk_adjustment_get_value (adjustment);
      gtk_widget_queue_draw (darea);
    }
}

static void
draw_func (GtkDrawingArea *area,
           cairo_t        *cr,
           int             width,
           int             height,
           gpointer        data)
{
  GdkPixbuf *dest;

  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);

  gdk_pixbuf_composite_color (pixbuf, dest,
			      0, 0, width, height,
			      0, 0,
                              (double) width / gdk_pixbuf_get_width (pixbuf),
                              (double) height / gdk_pixbuf_get_height (pixbuf),
			      interp_type, overall_alpha,
			      0, 0, 16, 0xaaaaaa, 0x555555);

  gdk_cairo_set_source_pixbuf (cr, dest, 0, 0);
  cairo_paint (cr);

  g_object_unref (dest);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main(int argc, char **argv)
{
	GtkWidget *window, *vbox;
        GtkWidget *combo_box;
	GtkWidget *hbox, *label, *hscale;
	GtkAdjustment *adjustment;
	GtkRequisition scratch_requisition;
        const gchar *creator;
        GError *error;
        gboolean done = FALSE;

	gtk_init ();

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

	window = gtk_window_new ();
	g_signal_connect (window, "destroy",
			  G_CALLBACK (quit_cb), &done);
	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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

        gtk_widget_set_halign (combo_box, GTK_ALIGN_START);
	gtk_container_add (GTK_CONTAINER (vbox), combo_box);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);

	label = gtk_label_new ("Overall Alpha:");
	gtk_container_add (GTK_CONTAINER (hbox), label);

	adjustment = gtk_adjustment_new (overall_alpha, 0, 255, 1, 10, 0);
	g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (overall_changed_cb), NULL);

	hscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
	gtk_scale_set_digits (GTK_SCALE (hscale), 0);
        gtk_widget_set_hexpand (hscale, TRUE);
	gtk_container_add (GTK_CONTAINER (hbox), hscale);

	/* Compute the size without the drawing area, so we know how big to make the default size */
        gtk_widget_get_preferred_size ( (vbox),
                                   &scratch_requisition, NULL);

	darea = gtk_drawing_area_new ();
        gtk_widget_set_hexpand (darea, TRUE);
	gtk_container_add (GTK_CONTAINER (vbox), darea);

        gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (darea), draw_func, NULL, NULL);

	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_pixbuf_get_width (pixbuf),
				     scratch_requisition.height + gdk_pixbuf_get_height (pixbuf));
	
	gtk_widget_show (window);

        while (!done)
                g_main_context_iteration (NULL, TRUE);

	return 0;
}
