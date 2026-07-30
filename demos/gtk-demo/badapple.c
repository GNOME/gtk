/* Background Blur
 *
 * Blurs the background with a video. If the system does not support
 * blurred backgrounds, this will not be very impressive.
 *
 * For effect, there are no decorations.
 * You can close the window using Alt-F4 or via the right-click menu.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

/* First, add the boilerplate for the object itself.
 * This part would normally go in the header.
 */
#define GTK_TYPE_EFFECT_PAINTABLE (gtk_effect_paintable_get_type ())
G_DECLARE_FINAL_TYPE (GtkEffectPaintable, gtk_effect_paintable, GTK, EFFECT_PAINTABLE, GObject)

/* Declare the struct. */
struct _GtkEffectPaintable
{
  GObject parent_instance;

  GdkPaintable *child;
};

struct _GtkEffectPaintableClass
{
  GObjectClass parent_class;
};

static GskPath *
create_path_for_region (const cairo_region_t *region)
{
  GskPathBuilder *builder;
  gsize i, n;

  builder = gsk_path_builder_new ();
  n = cairo_region_num_rectangles (region);

  for (i = 0; i < n; i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (region, i, &rect);
      gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (rect.x, rect.y, rect.width, rect.height));
    }

  return gsk_path_builder_free_to_path (builder);
}

static cairo_region_t *
region_create_from_bitmap (const guchar *data,
                           gsize         width,
                           gsize         height,
                           gsize         stride)
{
  cairo_region_t *region;
  GdkRectangle rect;
  gsize x, y;

  region = cairo_region_create ();

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          /* Search for a continuous range of "non transparent pixels"*/
          gint x0 = x;
          while (x < width)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              if (((data[x / 8] >> (x%8)) & 1) == 0)
#else
              if (((data[x / 8] >> (7-(x%8))) & 1) == 0)
#endif
                /* This pixel is "transparent"*/
                break;
              x++;
            }

          if (x > x0)
            {
              /* Add the pixels (x0, y) to (x, y+1) as a new rectangle
               * in the region
               */
              rect.x = x0;
              rect.width = x - x0;
              rect.y = y;
              rect.height = 1;

              cairo_region_union_rectangle (region, &rect);
            }
        }
      data += stride;
    }

  return region;
}

static GskPath *
create_mask_path (GskRenderNode *node)
{
  cairo_surface_t *surface, *bitmap;
  cairo_t *cr;
  cairo_rectangle_int_t clip;
  cairo_region_t *region;
  graphene_rect_t bounds;
  GskPath *path;

  gsk_render_node_get_bounds (node, &bounds);
  clip.x = floorf (bounds.origin.x);
  clip.y = floorf (bounds.origin.y);
  clip.width = ceilf (bounds.origin.x + bounds.size.width) - clip.x;
  clip.height = ceilf (bounds.origin.y + bounds.size.height) - clip.y;
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, clip.width, clip.height);
  cr = cairo_create (surface);
  cairo_translate (cr, - clip.x, - clip.y);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  bitmap = cairo_image_surface_create (CAIRO_FORMAT_A1, clip.width, clip.height);
  cr = cairo_create (bitmap);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  cairo_surface_flush (bitmap);
  region = region_create_from_bitmap (cairo_image_surface_get_data (bitmap),
                                      clip.width, clip.height,
                                      cairo_image_surface_get_stride (bitmap));
  cairo_surface_destroy (bitmap);
  cairo_region_translate (region, clip.x, clip.y);

  path = create_path_for_region (region);
  cairo_region_destroy (region);

  return path;
}

static void
gtk_effect_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  GtkEffectPaintable *ep = GTK_EFFECT_PAINTABLE (paintable);
  graphene_vec4_t offset;
  graphene_matrix_t matrix;
  GtkSnapshot *child_snapshot;
  GskRenderNode *node;
  GskPath *path;

  if (ep->child == NULL)
    return;

  child_snapshot = gtk_snapshot_new ();

  graphene_vec4_init (&offset, 0, 0, 0, 0);
  graphene_matrix_init_from_float (&matrix, (float[16]) {
      0, 0, 0, 0.2126,
      0, 0, 0, 0.7152,
      0, 0, 0, 0.0722,
      0, 0, 0, 0 });
  gtk_snapshot_push_color_matrix (child_snapshot, &matrix, &offset);
  gdk_paintable_snapshot (ep->child, child_snapshot, width, height);
  gtk_snapshot_pop (child_snapshot); /* color matrix */
  node = gtk_snapshot_free_to_node (child_snapshot);
  if (node == NULL)
    return;

  path = create_mask_path (node);
  gsk_render_node_unref (node);

#if 1
  gtk_snapshot_push_copy (snapshot);
  gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
  gtk_snapshot_push_blur (snapshot, 20);
  gtk_snapshot_append_paste (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height), 0);
  gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 0, 0, 0, 0.05 }, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (snapshot); /* blur */
  gtk_snapshot_pop (snapshot); /* fill */
  gtk_snapshot_pop (snapshot); /* copy */
#else
  gtk_snapshot_append_fill (snapshot, path, GSK_FILL_RULE_WINDING,
                            &(GdkRGBA) { 1, 0, 0, 1 });
#endif

  gsk_path_unref (path);
}

static double
gtk_effect_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkEffectPaintable *self = GTK_EFFECT_PAINTABLE (paintable);

  if (self->child == NULL)
    return 0.0;

  return gdk_paintable_get_intrinsic_aspect_ratio (self->child);
}

static void
gtk_effect_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_effect_paintable_snapshot;
  iface->get_intrinsic_aspect_ratio = gtk_effect_paintable_get_intrinsic_aspect_ratio;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkEffectPaintable, gtk_effect_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_effect_paintable_paintable_init))

static void
gtk_effect_paintable_dispose (GObject *object)
{
  GtkEffectPaintable *self = GTK_EFFECT_PAINTABLE (object);

  if (self->child)
    {
      g_signal_handlers_disconnect_by_func (self->child,
                                            gdk_paintable_invalidate_contents,
                                            self);
      g_signal_handlers_disconnect_by_func (self->child,
                                            gdk_paintable_invalidate_size,
                                            self);
      g_clear_object (&self->child);
    }

  G_OBJECT_CLASS (gtk_effect_paintable_parent_class)->dispose (object);
}

/* Here's the boilerplate for the GObject declaration.
 * We don't need to do anything special here, because we keep no
 * data of our own.
 */
static void
gtk_effect_paintable_class_init (GtkEffectPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_effect_paintable_dispose;
}

static void
gtk_effect_paintable_init (GtkEffectPaintable *ep)
{
}

/* And finally, we add a simple constructor.
 * It is declared in the header so that the other examples
 * can use it.
 */
GdkPaintable *
gtk_effect_paintable_new (GdkPaintable *child)
{
  GtkEffectPaintable *ep;

  ep = g_object_new (GTK_TYPE_EFFECT_PAINTABLE, NULL);
  ep->child = g_object_ref (child);
  g_signal_connect_swapped (child,
                            "invalidate-contents",
                            G_CALLBACK (gdk_paintable_invalidate_contents),
                            ep);
  g_signal_connect_swapped (child,
                            "invalidate-size",
                            G_CALLBACK (gdk_paintable_invalidate_size),
                            ep);

  return GDK_PAINTABLE (ep);
}

GtkWidget *
do_badapple (GtkWidget *do_widget)
{
  GtkMediaStream *video = NULL;
  GdkPaintable *ep;
  GtkWidget *picture, *handle;
  char *filename;
  struct {
    GUserDirectory dir; /* allow G_USER_N_DIRECTORIES for home dir */
    const char *name;
  } options[] = {
    { G_USER_N_DIRECTORIES, "badapple.webm" },
    { G_USER_N_DIRECTORIES, "badapple.mp4" },
    { G_USER_DIRECTORY_VIDEOS, "badapple.webm" },
    { G_USER_DIRECTORY_VIDEOS, "badapple.mp4" },
    { G_USER_DIRECTORY_DOWNLOAD, "badapple.webm" },
    { G_USER_DIRECTORY_DOWNLOAD, "badapple.mp4" },
  };
  gsize i;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Effect Paintable");
      gtk_window_set_default_size (GTK_WINDOW (window), 722, 540);
      gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
      gtk_widget_remove_css_class (window, "background");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      for (i = 0; i < G_N_ELEMENTS (options); i++)
        {
          if (options[i].dir == G_USER_N_DIRECTORIES)
            filename = g_build_filename (g_get_home_dir (), options[i].name, NULL);
          else
            filename = g_build_filename (g_get_user_special_dir (options[i].dir), options[i].name, NULL);

          if (g_file_test (filename, G_FILE_TEST_EXISTS))
            {
              video = gtk_media_file_new_for_filename (filename);
              g_free (filename);
              break;
            }

          g_free (filename);
        }
      if (video == NULL)
        video = gtk_media_file_new_for_resource ("/badapple/gtk-logo-mask.webm");
      gtk_media_stream_set_loop (video, TRUE);
      gtk_media_stream_play (video);
      ep = gtk_effect_paintable_new (GDK_PAINTABLE (video));
      picture = gtk_picture_new_for_paintable (ep);
      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
      gtk_picture_set_isolate_contents (GTK_PICTURE (picture), FALSE);

      handle = gtk_window_handle_new ();
      gtk_window_handle_set_child (GTK_WINDOW_HANDLE (handle), picture);
      gtk_window_set_child (GTK_WINDOW (window), handle);
      g_object_unref (ep);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
