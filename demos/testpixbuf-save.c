/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#include "config.h"
#include <stdio.h>

#include <gtk/gtk.h>

static void
compare_pixbufs (GdkPixbuf *pixbuf, GdkPixbuf *compare, const gchar *file_type)
{
        if ((gdk_pixbuf_get_width (pixbuf) !=
             gdk_pixbuf_get_width (compare)) ||
            (gdk_pixbuf_get_height (pixbuf) !=
             gdk_pixbuf_get_height (compare)) ||
            (gdk_pixbuf_get_n_channels (pixbuf) !=
             gdk_pixbuf_get_n_channels (compare)) ||
            (gdk_pixbuf_get_has_alpha (pixbuf) !=
             gdk_pixbuf_get_has_alpha (compare)) ||
            (gdk_pixbuf_get_bits_per_sample (pixbuf) !=
             gdk_pixbuf_get_bits_per_sample (compare))) {
                fprintf (stderr,
                         "saved %s file differs from copy in memory\n",
                         file_type);
        } else {
                guchar *orig_pixels;
                guchar *compare_pixels;
                gint    orig_rowstride;
                gint    compare_rowstride;
                gint    width;
                gint    height;
                gint    bytes_per_pixel;
                gint    x, y;
                guchar *p1, *p2;
                gint    count = 0;

                orig_pixels = gdk_pixbuf_get_pixels (pixbuf);
                compare_pixels = gdk_pixbuf_get_pixels (compare);

                orig_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
                compare_rowstride = gdk_pixbuf_get_rowstride (compare);

                width = gdk_pixbuf_get_width (pixbuf);
                height = gdk_pixbuf_get_height (pixbuf);

                /*  well...  */
                bytes_per_pixel = gdk_pixbuf_get_n_channels (pixbuf);

                p1 = orig_pixels;
                p2 = compare_pixels;

                for (y = 0; y < height; y++) {
                        for (x = 0; x < width * bytes_per_pixel; x++)
                                count += (*p1++ != *p2++);

                        orig_pixels += orig_rowstride;
                        compare_pixels += compare_rowstride;

                        p1 = orig_pixels;
                        p2 = compare_pixels;
                }

                if (count > 0) {
                        fprintf (stderr,
                                 "saved %s file differs from copy in memory\n",
                                 file_type);
                }
        }
}

static gboolean
save_to_loader (const gchar *buf, gsize count, GError **err, gpointer data)
{
        GdkPixbufLoader *loader = data;

        return gdk_pixbuf_loader_write (loader, (const guchar *)buf, count, err);
}

static GdkPixbuf *
buffer_to_pixbuf (const gchar *buf, gsize count, GError **err)
{
        GdkPixbufLoader *loader;
        GdkPixbuf *pixbuf;

        loader = gdk_pixbuf_loader_new ();
        if (gdk_pixbuf_loader_write (loader, (const guchar *)buf, count, err) &&
            gdk_pixbuf_loader_close (loader, err)) {
                pixbuf = g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));
                g_object_unref (loader);
                return pixbuf;
        } else {
                return NULL;
        }
}

static void
do_compare (GdkPixbuf *pixbuf, GdkPixbuf *compare, GError *err)
{
        if (compare == NULL) {
                fprintf (stderr, "%s", err->message);
                g_error_free (err);
        } else {
                compare_pixbufs (pixbuf, compare, "jpeg");
                g_object_unref (compare);
        }
}

static void
keypress_check (GtkWidget *widget, GdkEventKey *evt, gpointer data)
{
        GdkPixbuf *pixbuf;
        GtkDrawingArea *da = (GtkDrawingArea*)data;
        GError *err = NULL;
        gchar *buffer;
        gsize count;
        GdkPixbufLoader *loader;

        pixbuf = (GdkPixbuf *) g_object_get_data (G_OBJECT (da), "pixbuf");

        if (evt->keyval == 'q')
                gtk_main_quit ();

        if (evt->keyval == 's' && (evt->state & GDK_CONTROL_MASK)) {
                /* save to callback */
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                }	

                loader = gdk_pixbuf_loader_new ();
                if (!gdk_pixbuf_save_to_callback (pixbuf, save_to_loader, loader, "jpeg",
                                                  &err,
                                                  "quality", "100",
                                                  NULL) ||
                    !gdk_pixbuf_loader_close (loader, &err)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader)),
                                    err);
                        g_object_unref (loader);
                }
        }
        else if (evt->keyval == 'S') {
                /* save to buffer */
                if (!gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &count, "jpeg",
                                                &err,
                                                "quality", "100",
                                                NULL)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    buffer_to_pixbuf (buffer, count, &err),
                                    err);
                }
        }
        else if (evt->keyval == 's') {
                /* save normally */
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                }	

                if (!gdk_pixbuf_save (pixbuf, "foo.jpg", "jpeg",
                                      &err,
                                      "quality", "100",
                                      NULL)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    gdk_pixbuf_new_from_file ("foo.jpg", &err),
                                    err);
                }
        }

        if (evt->keyval == 'p' && (evt->state & GDK_CONTROL_MASK)) {
                /* save to callback */
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                }

                loader = gdk_pixbuf_loader_new ();
                if (!gdk_pixbuf_save_to_callback (pixbuf, save_to_loader, loader, "png",
                                                  &err,
                                                  "tEXt::Software", "testpixbuf-save",
                                                  NULL)
                    || !gdk_pixbuf_loader_close (loader, &err)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader)),
                                    err);
                        g_object_unref (loader);
                }
        }
        else if (evt->keyval == 'P') {
                /* save to buffer */
                if (!gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &count, "png",
                                                &err,
                                                "tEXt::Software", "testpixbuf-save",
                                                NULL)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    buffer_to_pixbuf (buffer, count, &err),
                                    err);
                }
        }
        else if (evt->keyval == 'p') {
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                }

                if (!gdk_pixbuf_save (pixbuf, "foo.png", "png", 
                                      &err,
                                      "tEXt::Software", "testpixbuf-save",
                                      NULL)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare(pixbuf,
                                   gdk_pixbuf_new_from_file ("foo.png", &err),
                                   err);
                }
        }

        if (evt->keyval == 'i' && (evt->state & GDK_CONTROL_MASK)) {
                /* save to callback */
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                }

                loader = gdk_pixbuf_loader_new ();
                if (!gdk_pixbuf_save_to_callback (pixbuf, save_to_loader, loader, "ico",
                                                  &err,
                                                  NULL)
                    || !gdk_pixbuf_loader_close (loader, &err)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader)),
                                    err);
                        g_object_unref (loader);
                }
        }
        else if (evt->keyval == 'I') {
                /* save to buffer */
                if (!gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &count, "ico",
                                                &err,
                                                NULL)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare (pixbuf,
                                    buffer_to_pixbuf (buffer, count, &err),
                                    err);
                }
        }
        else if (evt->keyval == 'i') {
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                }

                if (!gdk_pixbuf_save (pixbuf, "foo.ico", "ico", 
                                      &err,
                                      NULL)) {
                        fprintf (stderr, "%s", err->message);
                        g_error_free (err);
                } else {
                        do_compare(pixbuf,
                                   gdk_pixbuf_new_from_file ("foo.ico", &err),
                                   err);
                }
        }

        if (evt->keyval == 'a') {
                if (pixbuf == NULL) {
                        fprintf (stderr, "PIXBUF NULL\n");
                        return;
                } else {
                        GdkPixbuf *alpha_buf;

                        alpha_buf = gdk_pixbuf_add_alpha (pixbuf,
                                                          FALSE, 0, 0, 0);

                        g_object_set_data_full (G_OBJECT (da),
                                                "pixbuf", alpha_buf,
                                                (GDestroyNotify) g_object_unref);
                }
        }
}


static int
close_app (GtkWidget *widget, gpointer data)
{
        gtk_main_quit ();
        return TRUE;
}

static gboolean
draw_cb (GtkWidget *drawing_area, cairo_t *cr, gpointer data)
{
        GdkPixbuf *pixbuf;
         
        pixbuf = (GdkPixbuf *) g_object_get_data (G_OBJECT (drawing_area),
						  "pixbuf");

        gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
        cairo_paint (cr);

        return FALSE;
}

static int
configure_cb (GtkWidget *drawing_area, GdkEventConfigure *evt, gpointer data)
{
        GdkPixbuf *pixbuf;
                           
        pixbuf = (GdkPixbuf *) g_object_get_data (G_OBJECT (drawing_area),   
						  "pixbuf");
    
        g_print ("X:%d Y:%d\n", evt->width, evt->height);
        if (evt->width != gdk_pixbuf_get_width (pixbuf) || evt->height != gdk_pixbuf_get_height (pixbuf)) {
                GdkWindow *root;
                GdkPixbuf *new_pixbuf;

                root = gdk_get_default_root_window ();
                new_pixbuf = gdk_pixbuf_get_from_window (root,
                                                         0, 0, evt->width, evt->height);
                g_object_set_data_full (G_OBJECT (drawing_area), "pixbuf", new_pixbuf,
                                        (GDestroyNotify) g_object_unref);
        }

        return FALSE;
}

int
main (int argc, char **argv)
{   
        GdkWindow     *root;
        GtkWidget     *window;
        GtkWidget     *vbox;
        GtkWidget     *drawing_area;
        GdkPixbuf     *pixbuf;    
   
        gtk_init (&argc, &argv);   

        root = gdk_get_default_root_window ();
        pixbuf = gdk_pixbuf_get_from_window (root,
                                             0, 0, 150, 160);
   
        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_signal_connect (window, "delete_event",
			  G_CALLBACK (close_app), NULL);
        g_signal_connect (window, "destroy",   
			  G_CALLBACK (close_app), NULL);
   
        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add (GTK_CONTAINER (window), vbox);  
   
        drawing_area = gtk_drawing_area_new ();
        gtk_widget_set_size_request (GTK_WIDGET (drawing_area),
                                     gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf));
        g_signal_connect (drawing_area, "draw",
			  G_CALLBACK (draw_cb), NULL);

        g_signal_connect (drawing_area, "configure_event",
			  G_CALLBACK (configure_cb), NULL);
        g_signal_connect (window, "key_press_event", 
			  G_CALLBACK (keypress_check), drawing_area);    
        g_object_set_data_full (G_OBJECT (drawing_area), "pixbuf", pixbuf,
                                (GDestroyNotify) g_object_unref);
        gtk_box_pack_start (GTK_BOX (vbox), drawing_area, TRUE, TRUE, 0);
   
        gtk_widget_show_all (window);
        gtk_main ();
        return 0;
}
