#include <math.h>
#include "demo3widget.h"

enum
{
  PROP_TEXTURE = 1,
  PROP_FILTER,
  PROP_SCALE,
  PROP_ANGLE,
};

struct _Demo3Widget
{
  GtkWidget parent_instance;

  GdkTexture *texture;
  float scale;
  float angle;
  GskScalingFilter filter;

  GtkWidget *menu;
};

struct _Demo3WidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (Demo3Widget, demo3_widget, GTK_TYPE_WIDGET)

static void
demo3_widget_init (Demo3Widget *self)
{
  self->scale = 1.f;
  self->angle = 0.f;
  self->filter = GSK_SCALING_FILTER_LINEAR;
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
demo3_widget_dispose (GObject *object)
{
  Demo3Widget *self = DEMO3_WIDGET (object);

  g_clear_object (&self->texture);

  gtk_widget_dispose_template (GTK_WIDGET (self), DEMO3_TYPE_WIDGET);

  G_OBJECT_CLASS (demo3_widget_parent_class)->dispose (object);
}

static void
demo3_widget_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  Demo3Widget *self = DEMO3_WIDGET (widget);
  int x, y, width, height;
  double w, h, w2, h2;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  w2 = w = self->scale * gdk_texture_get_width (self->texture);
  h2 = h = self->scale * gdk_texture_get_height (self->texture);

  if (G_APPROX_VALUE (self->angle, 90.f, FLT_EPSILON) ||
      G_APPROX_VALUE (self->angle, 270.f, FLT_EPSILON))
    {
      double s = w2;
      w2 = h2;
      h2 = s;
    }

  x = (width - ceil (w2)) / 2;
  y = (height - ceil (h2)) / 2;

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (w2 / 2, h2 / 2));
  gtk_snapshot_rotate (snapshot, self->angle);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- w / 2, - h / 2));
  gtk_snapshot_append_scaled_texture (snapshot,
                                      self->texture,
                                      self->filter,
                                      &GRAPHENE_RECT_INIT (0, 0, w, h));
  gtk_snapshot_restore (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
demo3_widget_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  Demo3Widget *self = DEMO3_WIDGET (widget);
  int width, height;
  int size;

  width = gdk_texture_get_width (self->texture);
  height = gdk_texture_get_height (self->texture);

  if (G_APPROX_VALUE (self->angle, 90.f, FLT_EPSILON) ||
      G_APPROX_VALUE (self->angle, 270.f, FLT_EPSILON))
    {
      int s = width;
      width = height;
      height = s;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = width;
  else
    size = height;

  *minimum = *natural = (int) ceil (self->scale * size);
}

static void
demo3_widget_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  Demo3Widget *self = DEMO3_WIDGET (widget);

  /* Since we are not using a layout manager (who would do this
   * for us), we need to allocate a size for our menu by calling
   * gtk_popover_present().
   */
  gtk_popover_present (GTK_POPOVER (self->menu));
}

static void
demo3_widget_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  Demo3Widget *self = DEMO3_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TEXTURE:
      g_clear_object (&self->texture);
      self->texture = g_value_dup_object (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    case PROP_SCALE:
      self->scale = g_value_get_float (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    case PROP_ANGLE:
      self->angle = fmodf (g_value_get_float (value), 360.f);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    case PROP_FILTER:
      self->filter = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo3_widget_get_property (GObject     *object,
                           guint        prop_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
  Demo3Widget *self = DEMO3_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TEXTURE:
      g_value_set_object (value, self->texture);
      break;

    case PROP_SCALE:
      g_value_set_float (value, self->scale);
      break;

    case PROP_ANGLE:
      g_value_set_float (value, self->angle);
      break;

    case PROP_FILTER:
      g_value_set_enum (value, self->filter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pressed_cb (GtkGestureClick *gesture,
            guint            n_press,
            double           x,
            double           y,
            Demo3Widget     *self)
{
  /* We are placing our menu at the point where
   * the click happened, before popping it up.
   */
  gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                               &(const GdkRectangle){ x, y, 1, 1 });
  gtk_popover_popup (GTK_POPOVER (self->menu));
}

static void
zoom_cb (GtkWidget  *widget,
         const char *action_name,
         GVariant   *parameter)
{
  Demo3Widget *self = DEMO3_WIDGET (widget);
  float scale;

  if (g_str_equal (action_name, "zoom.in"))
    scale = MIN (10, self->scale * M_SQRT2);
  else if (g_str_equal (action_name, "zoom.out"))
    scale = MAX (0.01, self->scale / M_SQRT2);
  else
    scale = 1.0;

  gtk_widget_action_set_enabled (widget, "zoom.in", scale < 10);
  gtk_widget_action_set_enabled (widget, "zoom.out", scale > 0.01);
  gtk_widget_action_set_enabled (widget, "zoom.reset", scale != 1);

  g_object_set (widget, "scale", scale, NULL);
}

static void
rotate_cb (GtkWidget  *widget,
           const char *action_name,
           GVariant   *parameter)
{
  Demo3Widget *self = DEMO3_WIDGET (widget);
  int angle;

  g_variant_get (parameter, "i", &angle);

  g_object_set (widget, "angle", fmodf (self->angle + angle, 360.f), NULL);
}

static void
demo3_widget_class_init (Demo3WidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo3_widget_dispose;
  object_class->set_property = demo3_widget_set_property;
  object_class->get_property = demo3_widget_get_property;

  widget_class->snapshot = demo3_widget_snapshot;
  widget_class->measure = demo3_widget_measure;
  widget_class->size_allocate = demo3_widget_size_allocate;

  g_object_class_install_property (object_class, PROP_TEXTURE,
      g_param_spec_object ("texture", NULL, NULL,
                           GDK_TYPE_TEXTURE,
                           G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_SCALE,
      g_param_spec_float ("scale", NULL, NULL,
                          0.0, 1024.0, 1.0,
                          G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_ANGLE,
      g_param_spec_float ("angle", NULL, NULL,
                          0.0, 360.0, 0.0,
                          G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_FILTER,
      g_param_spec_enum ("filter", NULL, NULL,
                         GSK_TYPE_SCALING_FILTER, GSK_SCALING_FILTER_LINEAR,
                         G_PARAM_READWRITE));

  /* These are the actions that we are using in the menu */
  gtk_widget_class_install_action (widget_class, "zoom.in", NULL, zoom_cb);
  gtk_widget_class_install_action (widget_class, "zoom.out", NULL, zoom_cb);
  gtk_widget_class_install_action (widget_class, "zoom.reset", NULL, zoom_cb);
  gtk_widget_class_install_action (widget_class, "rotate", "i", rotate_cb);

  gtk_widget_class_set_template_from_resource (widget_class, "/menu/demo3widget.ui");
  gtk_widget_class_bind_template_child (widget_class, Demo3Widget, menu);
  gtk_widget_class_bind_template_callback (widget_class, pressed_cb);
}

GtkWidget *
demo3_widget_new (const char *resource)
{
  Demo3Widget *self;
  GdkTexture *texture;

  texture = gdk_texture_new_from_resource (resource);

  self = g_object_new (DEMO3_TYPE_WIDGET, "texture", texture, NULL);

  g_object_unref (texture);

  return GTK_WIDGET (self);
}
