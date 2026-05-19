#include "demo2widget.h"
#include "demo2layout.h"

struct _Demo2Widget
{
  GtkWidget parent_instance;

  gint64 start_time;
  gint64 end_time;
  float start_position;
  float end_position;
  float start_offset;
  float end_offset;
  GtkOrientation orientation;
  int direction;
  unsigned int tick_id;
};

struct _Demo2WidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (Demo2Widget, demo2_widget, GTK_TYPE_WIDGET)

static void
demo2_widget_init (Demo2Widget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

static void
demo2_widget_dispose (GObject *object)
{
  Demo2Widget *self = DEMO2_WIDGET (object);
  GtkWidget *child;

  if (self->tick_id)
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
  self->tick_id = 0;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (demo2_widget_parent_class)->dispose (object);
}

static gboolean
update_position (GtkWidget     *widget,
                 GdkFrameClock *clock,
                 gpointer       data)
{
  Demo2Widget *self = DEMO2_WIDGET (widget);
  Demo2Layout *layout = DEMO2_LAYOUT (gtk_widget_get_layout_manager (widget));
  gint64 now;
  double t;

  now = gdk_frame_clock_get_frame_time (clock);

  t = (now - self->start_time) / (double) (self->end_time - self->start_time);

  demo2_layout_set_position (layout, self->start_position + t * (self->end_position - self->start_position));
  demo2_layout_set_offset (layout, self->start_offset + t * (self->end_offset - self->start_offset));
  gtk_widget_queue_allocate (widget);

  if (now >= self->end_time)
    {
      self->end_position = self->start_position = demo2_layout_get_position (layout);
      self->end_offset = self->start_offset = demo2_layout_get_offset (layout);

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        self->end_position += 10 * self->direction;
      else
        self->end_offset += 10 * self->direction;

      self->start_time = g_get_monotonic_time ();
      self->end_time = self->start_time + 0.5 * G_TIME_SPAN_SECOND;
    }

  return G_SOURCE_CONTINUE;
}

static void
update_animation (Demo2Widget *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  if (self->direction == 0)
    {
      if (self->tick_id)
        gtk_widget_remove_tick_callback (widget, self->tick_id);
      self->tick_id = 0;
    }
  else
    {
      if (!self->tick_id)
        {
          Demo2Layout *layout = DEMO2_LAYOUT (gtk_widget_get_layout_manager (widget));

          self->end_position = self->start_position = demo2_layout_get_position (layout);
          self->end_offset = self->start_offset = demo2_layout_get_offset (layout);

          if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
            self->end_position += 10 * self->direction;
          else
            self->end_offset += 10 * self->direction;

          self->start_time = g_get_monotonic_time ();
          self->end_time = self->start_time + 0.5 * G_TIME_SPAN_SECOND;

          self->tick_id = gtk_widget_add_tick_callback (widget, update_position, NULL, NULL);
        }
    }
}

static void
stop_animation (GtkWidget  *widget,
                const char *action,
                GVariant   *parameters)
{
  Demo2Widget *self = DEMO2_WIDGET (widget);

  self->direction = 0;

  update_animation (self);
}

static void
rotate_sphere (GtkWidget  *widget,
               const char *action,
               GVariant   *parameters)
{
  Demo2Widget *self = DEMO2_WIDGET (widget);
  GtkOrientation orientation;
  int direction;

  g_variant_get (parameters, "(ii)", &orientation, &direction);

  self->orientation = orientation;
  self->direction = direction;

  update_animation (self);
}

static void
demo2_widget_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      /* our layout manager sets this for children that are out of view */
      if (!gtk_widget_get_child_visible (child))
        continue;

      gtk_widget_snapshot_child (widget, child, snapshot);
    }
}

static void
demo2_widget_class_init (Demo2WidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo2_widget_dispose;

  widget_class->snapshot = demo2_widget_snapshot;

  gtk_widget_class_install_action (widget_class, "rotate", "(ii)", rotate_sphere);
  gtk_widget_class_install_action (widget_class, "stop", NULL, stop_animation);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Left, GDK_NO_MODIFIER_MASK,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Right, GDK_NO_MODIFIER_MASK,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Up, GDK_NO_MODIFIER_MASK,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Down, GDK_NO_MODIFIER_MASK,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_VERTICAL, -1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Escape, GDK_NO_MODIFIER_MASK,
                                       "stop", NULL);

  /* here is where we use our custom layout manager */
  gtk_widget_class_set_layout_manager_type (widget_class, DEMO2_TYPE_LAYOUT);
}

GtkWidget *
demo2_widget_new (void)
{
  return g_object_new (DEMO2_TYPE_WIDGET, NULL);
}

void
demo2_widget_add_child (Demo2Widget *self,
                        GtkWidget   *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

GtkOrientation
demo2_widget_get_orientation (Demo2Widget *self)
{
  return self->orientation;
}

int
demo2_widget_get_direction (Demo2Widget *self)
{
  return self->direction;
}

void
demo2_widget_set_orientation (Demo2Widget    *self,
                              GtkOrientation  orientation)
{
  self->orientation = orientation;

  update_animation (self);
}

void
demo2_widget_set_direction (Demo2Widget *self,
                            int          direction)
{
  self->direction = direction;

  update_animation (self);
}
