/* Path/Text Fill
 *
 * This demo shows how to use PangoCairo to draw text with more than
 * just a single color.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "paintable.h"

#define GTK_TYPE_PATH_PAINTABLE (gtk_path_paintable_get_type ())
G_DECLARE_FINAL_TYPE (GtkPathPaintable, gtk_path_paintable, GTK, PATH_PAINTABLE, GObject)

struct _GtkPathPaintable
{
  GObject parent_instance;

  int width;
  int height;
  GskPath *path;
  GdkPaintable *background;
};

struct _GtkPathPaintableClass
{
  GObjectClass parent_class;
};

static int
gtk_path_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);

  if (self->background)
    return MAX (gdk_paintable_get_intrinsic_width (self->background), self->width);
  else
    return self->width;
}

static int
gtk_path_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);

  if (self->background)
    return MAX (gdk_paintable_get_intrinsic_height (self->background), self->height);
  else
    return self->height;
}

static void
gtk_path_paintable_snapshot (GdkPaintable *paintable,
                             GdkSnapshot  *snapshot,
                             double        width,
                             double        height)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);

#if 0
  gtk_snapshot_push_fill (snapshot, self->path, GSK_FILL_RULE_WINDING);
#else
  GskStroke *stroke = gsk_stroke_new (4.0);
  gtk_snapshot_push_stroke (snapshot, self->path, stroke);
  gsk_stroke_free (stroke);
#endif

  if (self->background)
    {
      gdk_paintable_snapshot (self->background, snapshot, width, height);
    }
  else
    {
      gtk_snapshot_append_linear_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT (0, 0, width, height),
                                           &GRAPHENE_POINT_INIT (0, 0),
                                           &GRAPHENE_POINT_INIT (width, height),
                                           (GskColorStop[8]) {
                                             { 0.0, { 1.0, 0.0, 0.0, 1.0 } },
                                             { 0.2, { 1.0, 0.0, 0.0, 1.0 } },
                                             { 0.3, { 1.0, 1.0, 0.0, 1.0 } },
                                             { 0.4, { 0.0, 1.0, 0.0, 1.0 } },
                                             { 0.6, { 0.0, 1.0, 1.0, 1.0 } },
                                             { 0.7, { 0.0, 0.0, 1.0, 1.0 } },
                                             { 0.8, { 1.0, 0.0, 1.0, 1.0 } },
                                             { 1.0, { 1.0, 0.0, 1.0, 1.0 } }
                                           },
                                           8);
    }

  gtk_snapshot_pop (snapshot);

}

static GdkPaintableFlags
gtk_path_paintable_get_flags (GdkPaintable *paintable)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);

  if (self->background)
    return gdk_paintable_get_flags (self->background);
  else
    return GDK_PAINTABLE_STATIC_CONTENTS | GDK_PAINTABLE_STATIC_SIZE;
}

static void
gtk_path_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = gtk_path_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_path_paintable_get_intrinsic_height;
  iface->snapshot = gtk_path_paintable_snapshot;
  iface->get_flags = gtk_path_paintable_get_flags;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkPathPaintable, gtk_path_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_path_paintable_paintable_init))

/* Here's the boilerplate for the GObject declaration.
 * We don't need to do anything special here, because we keep no
 * data of our own.
 */
static void
gtk_path_paintable_class_init (GtkPathPaintableClass *klass)
{
}

static void
gtk_path_paintable_init (GtkPathPaintable *self)
{
}

/* And finally, we add a simple constructor.
 * It is declared in the header so that the other examples
 * can use it.
 */
GdkPaintable *
gtk_path_paintable_new (GskPath      *path,
                        GdkPaintable *background,
                        int           width,
                        int           height)
{
  GtkPathPaintable *self;

  self = g_object_new (GTK_TYPE_PATH_PAINTABLE, NULL);
  self->path = path;
  self->background = background;
  if (self->background)
    {
      g_signal_connect_swapped (self->background, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);
      g_signal_connect_swapped (self->background, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);
    }
  self->width = width;
  self->height = height;

  return GDK_PAINTABLE (self);
}

void
gtk_path_paintable_set_path (GtkPathPaintable *self,
                             GskPath          *path)
{
  g_clear_pointer (&self->path, gsk_path_unref);
  self->path = gsk_path_ref (path);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static GskPath *
create_hexagon (GtkWidget *widget)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  gsk_path_builder_move_to (builder, 120, 0);
  gsk_path_builder_line_to (builder, 360, 0);
  gsk_path_builder_line_to (builder, 480, 208);
  gsk_path_builder_line_to (builder, 360, 416);
  gsk_path_builder_line_to (builder, 120, 416);
  gsk_path_builder_line_to (builder, 0, 208);
  gsk_path_builder_close (builder);

  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
create_path_from_text (GtkWidget *widget)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *path;
  PangoLayout *layout;
  PangoFontDescription *desc;
  GskPath *result;

  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, NULL);
  cr = cairo_create (surface);

  layout = gtk_widget_create_pango_layout (widget, "Pango power!\nPango power!\nPango power!");
  desc = pango_font_description_from_string ("sans bold 36");
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);

  pango_cairo_layout_path (cr, layout);
  path = cairo_copy_path_flat (cr);
  result = gsk_path_new_from_cairo (path);

  cairo_path_destroy (path);
  g_object_unref (layout);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return result;
}

static gboolean
update_path (GtkWidget     *widget,
             GdkFrameClock *frame_clock,
             gpointer       measure)
{
  float progress = gdk_frame_clock_get_frame_time (frame_clock) % (60 * G_USEC_PER_SEC) / (float) (30 * G_USEC_PER_SEC);
  GskPathBuilder *builder;
  GskPath *path;
  graphene_point_t pos;
  graphene_vec2_t tangent;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_segment (builder,
                                measure,
                                progress > 1 ? (progress - 1) * gsk_path_measure_get_length (measure) : 0.0,
                                (progress < 1 ? progress : 1.0) * gsk_path_measure_get_length (measure));

  gsk_path_measure_get_point (measure,
                              (progress > 1 ? (progress - 1) : progress) * gsk_path_measure_get_length (measure),
                              &pos,
                              &tangent);
  gsk_path_builder_move_to (builder, pos.x + 5 * graphene_vec2_get_x (&tangent), pos.y + 5 * graphene_vec2_get_y (&tangent));
  gsk_path_builder_line_to (builder, pos.x + 3 * graphene_vec2_get_y (&tangent), pos.y + 3 * graphene_vec2_get_x (&tangent));
  gsk_path_builder_line_to (builder, pos.x - 3 * graphene_vec2_get_y (&tangent), pos.y - 3 * graphene_vec2_get_x (&tangent));
  gsk_path_builder_close (builder);
  path = gsk_path_builder_free_to_path (builder);

  gtk_path_paintable_set_path (GTK_PATH_PAINTABLE (gtk_picture_get_paintable (GTK_PICTURE (widget))),
                               path);
  gsk_path_unref (path);

  return G_SOURCE_CONTINUE;
}

GtkWidget *
do_path_fill (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picture;
      GdkPaintable *paintable;
      GtkMediaStream *stream;
      GskPath *path;
      graphene_rect_t bounds;
      GskPathMeasure *measure;

      window = gtk_window_new ();
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_window_set_title (GTK_WINDOW (window), "Path Fill");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

#if 0
      stream = gtk_media_file_new_for_resource ("/images/gtk-logo.webm");
#else
      stream = gtk_nuclear_media_stream_new ();
#endif
      gtk_media_stream_play (stream);
      gtk_media_stream_set_loop (stream, TRUE);

      path = create_hexagon (window);
      path = create_path_from_text (window);
      gsk_path_get_bounds (path, &bounds);

      paintable = gtk_path_paintable_new (path,
                                          GDK_PAINTABLE (stream),
                                          bounds.origin.x + bounds.size.width,
                                          bounds.origin.y + bounds.size.height);
      picture = gtk_picture_new_for_paintable (paintable);
      measure = gsk_path_measure_new (path);
      gtk_widget_add_tick_callback (picture, update_path, measure, (GDestroyNotify) gsk_path_measure_unref);
      gtk_picture_set_keep_aspect_ratio (GTK_PICTURE (picture), FALSE);
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
      g_object_unref (paintable);

      gtk_window_set_child (GTK_WINDOW (window), picture);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
