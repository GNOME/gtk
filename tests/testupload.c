#include <gtk/gtk.h>

static void
add_to_grid (GtkWidget *grid,
             int        left,
             int        top,
             int        n_channels,
             gboolean   premul,
             int        width,
             int        height,
             int        stride,
             GtkWidget *picture)
{
  char *text;

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Channels"), left, top, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Width"), left, top + 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Height"), left, top + 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Stride"), left, top + 3, 1, 1);

  text = g_strdup_printf ("%d%s", n_channels, premul ? " (premul)" : "");
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new (text), left + 1, top, 1, 1);
  g_free (text);
  text = g_strdup_printf ("%d", width);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new (text), left + 1, top + 1, 1, 1);
  g_free (text);
  text = g_strdup_printf ("%d", height);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new (text), left + 1, top + 2, 1, 1);
  g_free (text);
  text = g_strdup_printf ("%d", stride);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new (text), left + 1, top + 3, 1, 1);
  g_free (text);

  gtk_grid_attach (GTK_GRID (grid), picture, left + 2, top + 0, 1, 4);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *grid;
  GdkPixbuf *source, *pb, *pb2;
  GError *error = NULL;
  cairo_t *cr;
  cairo_surface_t *surface;
  GBytes *bytes;
  GdkTexture *texture;

  gtk_init ();

  window = gtk_window_new ();
  grid = gtk_grid_new ();
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_window_set_child (GTK_WINDOW (window), grid);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              200, 200, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  add_to_grid (grid, 0, 0,
               gdk_pixbuf_get_n_channels (source),
               FALSE,
               gdk_pixbuf_get_width (source),
               gdk_pixbuf_get_height (source),
               gdk_pixbuf_get_rowstride (source),
               gtk_picture_new_for_pixbuf (source));

  g_object_unref (source);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              199, 199, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  add_to_grid (grid, 4, 0,
               gdk_pixbuf_get_n_channels (source),
               FALSE,
               gdk_pixbuf_get_width (source),
               gdk_pixbuf_get_height (source),
               gdk_pixbuf_get_rowstride (source),
               gtk_picture_new_for_pixbuf (source));

  g_object_unref (source);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              201, 201, TRUE,
                                              &error);

  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_new_subpixbuf (source, 0, 0, 200, 200);

  add_to_grid (grid, 8, 0,
               gdk_pixbuf_get_n_channels (pb),
               FALSE,
               gdk_pixbuf_get_width (pb),
               gdk_pixbuf_get_height (pb),
               gdk_pixbuf_get_rowstride (pb),
               gtk_picture_new_for_pixbuf (pb));

  g_object_unref (source);
  g_object_unref (pb);


  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              200, 200, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_add_alpha (source, FALSE, 0, 0, 0);

  add_to_grid (grid, 0, 5,
               gdk_pixbuf_get_n_channels (pb),
               FALSE,
               gdk_pixbuf_get_width (pb),
               gdk_pixbuf_get_height (pb),
               gdk_pixbuf_get_rowstride (pb),
               gtk_picture_new_for_pixbuf (pb));

  g_object_unref (source);
  g_object_unref (pb);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              199, 199, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_add_alpha (source, FALSE, 0, 0, 0);

  add_to_grid (grid, 4, 5,
               gdk_pixbuf_get_n_channels (pb),
               FALSE,
               gdk_pixbuf_get_width (pb),
               gdk_pixbuf_get_height (pb),
               gdk_pixbuf_get_rowstride (pb),
               gtk_picture_new_for_pixbuf (pb));

  g_object_unref (source);
  g_object_unref (pb);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              201, 201, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_add_alpha (source, FALSE, 0, 0, 0);
  pb2 = gdk_pixbuf_new_subpixbuf (pb, 0, 0, 200, 200);

  add_to_grid (grid, 8, 5,
               gdk_pixbuf_get_n_channels (pb2),
               FALSE,
               gdk_pixbuf_get_width (pb2),
               gdk_pixbuf_get_height (pb2),
               gdk_pixbuf_get_rowstride (pb2),
               gtk_picture_new_for_pixbuf (pb2));

  g_object_unref (source);
  g_object_unref (pb);
  g_object_unref (pb2);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              200, 200, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_add_alpha (source, FALSE, 0, 0, 0);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb));
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  bytes = g_bytes_new (cairo_image_surface_get_data (surface),
                       cairo_image_surface_get_stride (surface) *
                       cairo_image_surface_get_height (surface));
  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));
  g_bytes_unref (bytes);

  add_to_grid (grid, 0, 10,
               4, TRUE,
               cairo_image_surface_get_width (surface),
               cairo_image_surface_get_height (surface),
               cairo_image_surface_get_stride (surface),
               gtk_picture_new_for_paintable (GDK_PAINTABLE (texture)));

  g_object_unref (source);
  g_object_unref (pb);
  cairo_surface_destroy (surface);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              199, 199, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_add_alpha (source, FALSE, 0, 0, 0);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb));
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  bytes = g_bytes_new (cairo_image_surface_get_data (surface),
                       cairo_image_surface_get_stride (surface) *
                       cairo_image_surface_get_height (surface));
  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));
  g_bytes_unref (bytes);

  add_to_grid (grid, 4, 10,
               4, TRUE,
               cairo_image_surface_get_width (surface),
               cairo_image_surface_get_height (surface),
               cairo_image_surface_get_stride (surface),
               gtk_picture_new_for_paintable (GDK_PAINTABLE (texture)));

  g_object_unref (source);
  g_object_unref (pb);
  cairo_surface_destroy (surface);

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              201, 201, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  pb = gdk_pixbuf_add_alpha (source, FALSE, 0, 0, 0);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb));
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  bytes = g_bytes_new (cairo_image_surface_get_data (surface),
                       cairo_image_surface_get_stride (surface) *
                       cairo_image_surface_get_height (surface));
  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface) - 1,
                                    cairo_image_surface_get_height (surface) - 1,
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));
  g_bytes_unref (bytes);

  add_to_grid (grid, 8, 10,
               4, TRUE,
               gdk_texture_get_width (texture),
               gdk_texture_get_height (texture),
               cairo_image_surface_get_stride (surface),
               gtk_picture_new_for_paintable (GDK_PAINTABLE (texture)));

  g_object_unref (source);
  g_object_unref (pb);
  cairo_surface_destroy (surface);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
