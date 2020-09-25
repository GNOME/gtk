#include "gtkshaderbin.h"

typedef struct {
  GskGLShader *shader;
  GtkStateFlags state;
  GtkStateFlags state_mask;
  gboolean compiled;
  gboolean compiled_ok;
} ShaderInfo;

struct _GtkShaderBin
{
  GtkWidget parent_instance;
  GtkWidget *child;
  ShaderInfo *active_shader;
  GPtrArray *shaders;
  guint tick_id;
  float time;
  gint64 first_frame_time;
};

struct _GtkShaderBinClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkShaderBin, gtk_shader_bin, GTK_TYPE_WIDGET)

static void
shader_info_free (ShaderInfo *info)
{
  g_object_unref (info->shader);
  g_free (info);
}

static void
gtk_shader_bin_finalize (GObject *object)
{
  GtkShaderBin *self = GTK_SHADER_BIN (object);

  g_ptr_array_free (self->shaders, TRUE);

  G_OBJECT_CLASS (gtk_shader_bin_parent_class)->finalize (object);
}

static void
gtk_shader_bin_dispose (GObject *object)
{
  GtkShaderBin *self = GTK_SHADER_BIN (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_shader_bin_parent_class)->dispose (object);
}

static gboolean
gtk_shader_bin_tick (GtkWidget     *widget,
                     GdkFrameClock *frame_clock,
                     gpointer       unused)
{
  GtkShaderBin *self = GTK_SHADER_BIN (widget);
  gint64 frame_time;

  frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  if (self->first_frame_time == 0)
    self->first_frame_time = frame_time;
  self->time = (frame_time - self->first_frame_time) / (float)G_USEC_PER_SEC;

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
gtk_shader_bin_init (GtkShaderBin *self)
{
  self->shaders = g_ptr_array_new_with_free_func ((GDestroyNotify)shader_info_free);
}

void
gtk_shader_bin_update_active_shader (GtkShaderBin *self)
{
  GtkStateFlags new_state = gtk_widget_get_state_flags (GTK_WIDGET (self));
  ShaderInfo *new_shader = NULL;

  for (int i = 0; i < self->shaders->len; i++)
    {
      ShaderInfo *info = g_ptr_array_index (self->shaders, i);

      if ((info->state_mask & new_state) == info->state)
        {
          new_shader = info;
          break;
        }
    }

  if (self->active_shader == new_shader)
    return;

  self->active_shader = new_shader;
  self->first_frame_time = 0;

  if (self->active_shader)
    {
      if (self->tick_id == 0)
        self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                      gtk_shader_bin_tick,
                                                      NULL, NULL);
    }
  else
    {
      if (self->tick_id != 0)
        {
          gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
          self->tick_id = 0;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_shader_bin_state_flags_changed (GtkWidget        *widget,
                                    GtkStateFlags     previous_state_flags)
{
  GtkShaderBin *self = GTK_SHADER_BIN (widget);

  gtk_shader_bin_update_active_shader (self);
}

void
gtk_shader_bin_add_shader (GtkShaderBin *self,
                           GskGLShader  *shader,
                           GtkStateFlags state,
                           GtkStateFlags state_mask)
{
  ShaderInfo *info = g_new0 (ShaderInfo, 1);
  info->shader = g_object_ref (shader);
  info->state = state;
  info->state_mask = state_mask;

  g_ptr_array_add (self->shaders, info);

  gtk_shader_bin_update_active_shader (self);
}

void
gtk_shader_bin_set_child (GtkShaderBin *self,
                          GtkWidget    *child)
{

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }
}

GtkWidget *
gtk_shader_bin_get_child   (GtkShaderBin *self)
{
  return self->child;
}

static void
gtk_shader_bin_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkShaderBin *self = GTK_SHADER_BIN (widget);
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (self->active_shader)
    {
      if (!self->active_shader->compiled)
        {
          GtkNative *native = gtk_widget_get_native (widget);
          GskRenderer *renderer = gtk_native_get_renderer (native);
          GError *error = NULL;

          self->active_shader->compiled = TRUE;
          self->active_shader->compiled_ok =
            gsk_gl_shader_try_compile_for (self->active_shader->shader,
                                           renderer, &error);
          if (!self->active_shader->compiled_ok)
            {
              g_warning ("GtkShaderBin failed to compile shader: %s", error->message);
              g_error_free (error);
            }
        }

      if (self->active_shader->compiled_ok)
        {
          gtk_snapshot_push_gl_shader (snapshot, self->active_shader->shader,
                                       &GRAPHENE_RECT_INIT(0, 0, width, height),
                                       gsk_gl_shader_format_args (self->active_shader->shader,
                                                                  "u_time", &self->time,
                                                                  NULL),
                                       1);
          gtk_widget_snapshot_child (widget, self->child, snapshot);
          gtk_snapshot_pop (snapshot);

          return;
        }
    }

  /* Non-shader fallback */
  gtk_widget_snapshot_child (widget, self->child, snapshot);
}

static void
gtk_shader_bin_class_init (GtkShaderBinClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_shader_bin_finalize;
  object_class->dispose = gtk_shader_bin_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  widget_class->snapshot = gtk_shader_bin_snapshot;
  widget_class->state_flags_changed = gtk_shader_bin_state_flags_changed;
}

GtkWidget *
gtk_shader_bin_new (void)
{
  GtkShaderBin *self;

  self = g_object_new (GTK_TYPE_SHADER_BIN, NULL);

  return GTK_WIDGET (self);
}
