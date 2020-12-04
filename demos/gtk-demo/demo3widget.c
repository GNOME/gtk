#include <math.h>
#include "demo3widget.h"

enum
{
  PROP_PAINTABLE = 1,
  PROP_SCALE
};

struct _Demo3Widget
{
  GtkWidget parent_instance;

  GdkPaintable *paintable;
  float scale;

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
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
demo3_widget_dispose (GObject *object)
{
  Demo3Widget *self = DEMO3_WIDGET (object);

  g_clear_object (&self->paintable);
  g_clear_pointer (&self->menu, gtk_widget_unparent);

  G_OBJECT_CLASS (demo3_widget_parent_class)->dispose (object);
}

static void
demo3_widget_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  Demo3Widget *self = DEMO3_WIDGET (widget);
  int x, y, width, height;
  double w, h;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  w = self->scale * gdk_paintable_get_intrinsic_width (self->paintable);
  h = self->scale * gdk_paintable_get_intrinsic_height (self->paintable);

  x = MAX (0, (width - ceil (w)) / 2);
  y = MAX (0, (height - ceil (h)) / 2);

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gdk_paintable_snapshot (self->paintable, snapshot, w, h);
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
  int size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = gdk_paintable_get_intrinsic_width (self->paintable);
  else
    size = gdk_paintable_get_intrinsic_height (self->paintable);

  *minimum = *natural = self->scale * size;
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
    case PROP_PAINTABLE:
      g_clear_object (&self->paintable);
      self->paintable = g_value_dup_object (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    case PROP_SCALE:
      self->scale = g_value_get_float (value);
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
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_SCALE:
      g_value_set_float (value, self->scale);
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

  g_object_class_install_property (object_class, PROP_PAINTABLE,
      g_param_spec_object ("paintable", "Paintable", "Paintable",
                           GDK_TYPE_PAINTABLE,
                           G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_SCALE,
      g_param_spec_float ("scale", "Scale", "Scale",
                          0.0, 10.0, 1.0,
                          G_PARAM_READWRITE));

  /* These are the actions that we are using in the menu */
  gtk_widget_class_install_action (widget_class, "zoom.in", NULL, zoom_cb);
  gtk_widget_class_install_action (widget_class, "zoom.out", NULL, zoom_cb);
  gtk_widget_class_install_action (widget_class, "zoom.reset", NULL, zoom_cb);

  gtk_widget_class_set_template_from_resource (widget_class, "/menu/demo3widget.ui");
  gtk_widget_class_bind_template_child (widget_class, Demo3Widget, menu);
  gtk_widget_class_bind_template_callback (widget_class, pressed_cb);
}

GtkWidget *
demo3_widget_new (const char *resource)
{
  Demo3Widget *self;
  GdkPixbuf *pixbuf;
  GdkPaintable *paintable;

  pixbuf = gdk_pixbuf_new_from_resource (resource, NULL);
  paintable = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf));

  self = g_object_new (DEMO3_TYPE_WIDGET, "paintable", paintable, NULL);

  g_object_unref (pixbuf);
  g_object_unref (paintable);

  return GTK_WIDGET (self);
}
