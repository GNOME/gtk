/* Path/Map
 *
 * This demo shows how to draw a map using paths.
 *
 * The world map that is used here is a path with 211 lines and 1569 cubic
 * Bėzier segments in 121 contours.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "path_world.c"

#define GTK_TYPE_PATH_WALK (gtk_path_walk_get_type ())
G_DECLARE_FINAL_TYPE (GtkPathWalk, gtk_path_walk, GTK, PATH_WALK, GtkWidget)

#define POINT_SIZE 8

enum {
  PROP_0,
  PROP_PATH,
  N_PROPS
};

struct _GtkPathWalk
{
  GtkWidget parent_instance;

  GskPath *path;
  graphene_rect_t bounds;
};

struct _GtkPathWalkClass
{
  GtkWidgetClass parent_class;
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (GtkPathWalk, gtk_path_walk, GTK_TYPE_WIDGET)

static void
gtk_path_walk_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkPathWalk *self = GTK_PATH_WALK (widget);
  double width = gtk_widget_get_width (widget);
  double height = gtk_widget_get_height (widget);
  GskStroke *stroke;

  if (self->path == NULL)
    return;

  gtk_snapshot_save (snapshot);

  stroke = gsk_stroke_new (2.0);
  gtk_snapshot_push_stroke (snapshot, self->path, stroke);
  gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 0, 0, 0, 1 }, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (snapshot);
  gsk_stroke_free (stroke);

  gtk_snapshot_restore (snapshot);
}

static void
gtk_path_walk_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkPathWalk *self = GTK_PATH_WALK (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = (int) ceilf (self->bounds.size.width);
  else
    *minimum = *natural = (int) ceilf (self->bounds.size.height);
}

static void
gtk_path_walk_set_path (GtkPathWalk *self,
                        GskPath     *path)
{
  if (self->path == path)
    return;

  g_clear_pointer (&self->path, gsk_path_unref);
  graphene_rect_init (&self->bounds, 0, 0, 0, 0);
  if (path)
    {
      GskStroke *stroke;

      self->path = gsk_path_ref (path);
      stroke = gsk_stroke_new (2.0);
      gsk_path_get_stroke_bounds (path, stroke, &self->bounds);
      gsk_stroke_free (stroke);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

static void
gtk_path_walk_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)

{
  GtkPathWalk *self = GTK_PATH_WALK (object);

  switch (prop_id)
    {
    case PROP_PATH:
      gtk_path_walk_set_path (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_path_walk_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkPathWalk *self = GTK_PATH_WALK (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_boxed (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_path_walk_dispose (GObject *object)
{
  GtkPathWalk *self = GTK_PATH_WALK (object);

  g_clear_pointer (&self->path, gsk_path_unref);

  G_OBJECT_CLASS (gtk_path_walk_parent_class)->dispose (object);
}

static void
gtk_path_walk_class_init (GtkPathWalkClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_path_walk_dispose;
  object_class->set_property = gtk_path_walk_set_property;
  object_class->get_property = gtk_path_walk_get_property;

  widget_class->snapshot = gtk_path_walk_snapshot;
  widget_class->measure = gtk_path_walk_measure;

  properties[PROP_PATH] =
    g_param_spec_boxed ("path",
                        NULL, NULL,
                        GSK_TYPE_PATH,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_path_walk_init (GtkPathWalk *self)
{
  GskPath *path = gsk_path_parse (path_world);
  gtk_path_walk_set_path (self, path);
  gsk_path_unref (path);
}

GtkWidget *
gtk_path_walk_new (void)
{
  GtkPathWalk *self;

  self = g_object_new (GTK_TYPE_PATH_WALK, NULL);

  return GTK_WIDGET (self);
}

GtkWidget *
do_path_walk (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;

      g_type_ensure (GTK_TYPE_PATH_WALK);

      builder = gtk_builder_new_from_resource ("/path_walk/path_walk.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
