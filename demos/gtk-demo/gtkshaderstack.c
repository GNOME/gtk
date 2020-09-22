#include "gtkshaderstack.h"

struct _GtkShaderStack
{
  GtkWidget parent_instance;

  GskGLShader *shader;
  GPtrArray *children;
  int current;
  int next;

  guint tick_id;
  float time;
  float duration;
  gint64 start_time;
};

struct _GtkShaderStackClass
{
  GtkWidgetClass parent_class;
};


G_DEFINE_TYPE (GtkShaderStack, gtk_shader_stack, GTK_TYPE_WIDGET)

static void
gtk_shader_stack_finalize (GObject *object)
{
  GtkShaderStack *self = GTK_SHADER_STACK (object);

  g_object_unref (self->shader);

  G_OBJECT_CLASS (gtk_shader_stack_parent_class)->finalize (object);
}

static gboolean
transition_cb (GtkWidget     *widget,
               GdkFrameClock *clock,
               gpointer       unused)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  gint64 frame_time;

  frame_time = gdk_frame_clock_get_frame_time (clock);

  if (self->start_time == 0)
    self->start_time = frame_time;

  self->time = (frame_time - self->start_time) / (float)G_USEC_PER_SEC;

  gtk_widget_queue_draw (widget);

  if (self->time >= self->duration)
    {
      self->current = self->next;
      self->next = -1;

      return G_SOURCE_REMOVE;
    }
  else
    return G_SOURCE_CONTINUE;
}

static void
start_transition (GtkShaderStack *self)
{
  self->start_time = 0;
  self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                transition_cb,
                                                NULL, NULL);
}

static void
stop_transition (GtkShaderStack *self)
{
  if (self->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
      self->tick_id = 0;
    }

  self->next = -1;
}

static void
gtk_shader_stack_dispose (GObject *object)
{
  GtkShaderStack *self = GTK_SHADER_STACK (object);

  stop_transition (self);

  g_clear_pointer (&self->children, g_ptr_array_unref);

  G_OBJECT_CLASS (gtk_shader_stack_parent_class)->dispose (object);
}

static void
clicked_cb (GtkGestureClick *gesture,
            guint            n_pressed,
            double           x,
            double           y,
            gpointer         data)
{
  GtkShaderStack *self = GTK_SHADER_STACK (data);

  stop_transition (self);

  self->next = (self->current + 1) % self->children->len;

  start_transition (self);
}

static void
gtk_shader_stack_init (GtkShaderStack *self)
{
  GtkGesture *gesture;

  self->children = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_widget_unparent);
  self->current = -1;
  self->next = -1;
  self->duration = 1.0;

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (clicked_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_shader_stack_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  int i;

  *minimum = 0;
  *natural = 0;

  for (i = 0; i < self->children->len; i++)
    {
      GtkWidget *child = g_ptr_array_index (self->children, i);
      int child_min, child_nat;

      if (gtk_widget_get_visible (child))
        {
          gtk_widget_measure (child, orientation, for_size, &child_min, &child_nat, NULL, NULL);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
        }
    }
}

static void
gtk_shader_stack_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  GtkAllocation child_allocation;
  GtkWidget *child;
  int i;

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = width;
  child_allocation.height = height;

  for (i = 0; i < self->children->len; i++)
    {
      child = g_ptr_array_index (self->children, i);
      if (gtk_widget_get_visible (child))
        gtk_widget_size_allocate (child, &child_allocation, -1);
    }
}

static void
gtk_shader_stack_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  int width, height;
  GtkWidget *current, *next;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  current = g_ptr_array_index (self->children, self->current);

  if (self->next == -1)
    {
      gtk_widget_snapshot_child (widget, current, snapshot);
    }
  else
    {
      graphene_vec4_t args;

      next = g_ptr_array_index (self->children, self->next);

      gtk_snapshot_push_glshader (snapshot, self->shader,
                                  graphene_vec4_init (&args, self->time, self->time / self->duration, 0, 0),
                                  &GRAPHENE_RECT_INIT(0, 0, width, height),
                                  2);
      gtk_widget_snapshot_child (widget, next, snapshot);
      gtk_snapshot_pop (snapshot); /* Fallback */
      gtk_widget_snapshot_child (widget, current, snapshot);
      gtk_snapshot_pop (snapshot); /* current child */
      gtk_widget_snapshot_child (widget, next, snapshot);
      gtk_snapshot_pop (snapshot); /* next child */
    }
}

static void
gtk_shader_stack_class_init (GtkShaderStackClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_shader_stack_finalize;
  object_class->dispose = gtk_shader_stack_dispose;

  widget_class->snapshot = gtk_shader_stack_snapshot;
  widget_class->measure = gtk_shader_stack_measure;
  widget_class->size_allocate = gtk_shader_stack_size_allocate;
}

GtkWidget *
gtk_shader_stack_new (void)
{
  return g_object_new (GTK_TYPE_SHADER_STACK, NULL);
}

void
gtk_shader_stack_set_shader (GtkShaderStack *self,
                             GskGLShader    *shader)
{
  g_set_object (&self->shader, shader);
}

void
gtk_shader_stack_add_child (GtkShaderStack *self,
                            GtkWidget      *child)
{
  g_ptr_array_add (self->children, child);
  gtk_widget_set_parent (child, GTK_WIDGET (self));
  gtk_widget_queue_resize (GTK_WIDGET (self));

  if (self->current == -1)
    self->current = 0;
}
