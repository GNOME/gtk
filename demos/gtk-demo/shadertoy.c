/* Shadertoy
 *
 * Play with shaders. Everybody does it.
 */

#include <gtk/gtk.h>

enum {
  PROP_0,
  PROP_TIME,
  PROP_RUNNING,
  N_PROPS
};

G_DECLARE_FINAL_TYPE (GtkShadertoy, gtk_shadertoy, GTK, SHADERTOY, GtkWidget)

struct _GtkShadertoy {
  GtkWidget parent_instance;

  GBytes *fragment_shader;

  GtkWidget *child1;
  GtkWidget *child2;

  guint tick;
  guint64 starttime;

  float time;
  float base;
};

G_DEFINE_TYPE (GtkShadertoy, gtk_shadertoy, GTK_TYPE_WIDGET);

static void
gtk_shadertoy_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkShadertoy *self = GTK_SHADERTOY (widget);
  GtkAllocation alloc;
  graphene_rect_t bounds;
  int offset_x, offset_y;
  float time = (float)(g_get_monotonic_time () - self->starttime)/1000000.0;
  GskRenderNode *node, *child1, *child2;
  GtkAllocation child_alloc;
  GtkSnapshot *child_snapshot;


  if (!self->fragment_shader)
    {
       GTK_WIDGET_CLASS (gtk_shadertoy_parent_class)->snapshot (widget, snapshot);
       return;
    }

  gtk_snapshot_get_offset (snapshot, &offset_x, &offset_y);
  gtk_widget_get_allocation (widget, &alloc);

  bounds.origin.x = offset_x;
  bounds.origin.y = offset_y;
  bounds.size.width = alloc.width;
  bounds.size.height = alloc.height;

  gtk_widget_get_allocation (self->child1, &child_alloc);
  child_snapshot = gtk_snapshot_new (snapshot, "shadertoy child1");
  gtk_snapshot_offset (child_snapshot, offset_x + child_alloc.x, offset_y + child_alloc.y);
  gtk_widget_snapshot (self->child1, child_snapshot);
  gtk_snapshot_offset (child_snapshot, - offset_x - child_alloc.x, - offset_y - child_alloc.y);
  child1 = gtk_snapshot_free (child_snapshot);

  gtk_widget_get_allocation (self->child2, &child_alloc);
  child_snapshot = gtk_snapshot_new (snapshot, "shadertoy child2");
  gtk_snapshot_offset (child_snapshot, offset_x + child_alloc.x, offset_y + child_alloc.y);
  gtk_widget_snapshot (self->child2, child_snapshot);
  gtk_snapshot_offset (child_snapshot, - offset_x - child_alloc.x, - offset_y - child_alloc.y);
  child2 = gtk_snapshot_free (child_snapshot);

  node = gsk_pixel_shader_node_new (&bounds,
                                    child1, child2,
                                    self->fragment_shader,
                                    time);
  gsk_render_node_set_name (node, "shader");
  gtk_snapshot_append_node (snapshot, node);
  gsk_render_node_unref (child1);
  gsk_render_node_unref (child2);
}


static void
gtk_shadertoy_finalize (GObject *object)
{
  GtkShadertoy *self = GTK_SHADERTOY (object);

  if (self->fragment_shader)
    g_bytes_unref (self->fragment_shader);

  G_OBJECT_CLASS (gtk_shadertoy_parent_class)->finalize (object);
}


static void
gtk_shadertoy_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkShadertoy *self = GTK_SHADERTOY (object);

  switch (prop_id)
    {
    case PROP_TIME:
      g_value_set_float (value, self->time);
      break;

    case PROP_RUNNING:
      g_value_set_boolean (value, self->tick != 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void gtk_shadertoy_set_running (GtkShadertoy *self,
                                       gboolean      running);
static void gtk_shadertoy_reset_time (GtkShadertoy *self);

static void
gtk_shadertoy_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkShadertoy *self = GTK_SHADERTOY (object);

  switch (prop_id)
    {
    case PROP_RUNNING:
      gtk_shadertoy_set_running (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shadertoy_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkShadertoy *self = GTK_SHADERTOY (widget);
  gint child1_min, child1_nat;
  gint child2_min, child2_nat;

  *minimum = 0;
  *natural = 0;

   gtk_widget_measure (self->child1, orientation, -1, &child1_min, &child1_nat, NULL, NULL);
   gtk_widget_measure (self->child2, orientation, -1, &child2_min, &child2_nat, NULL, NULL);

   if (orientation == GTK_ORIENTATION_HORIZONTAL)
     {
       *minimum = MAX (*minimum, MAX (child1_min, child2_min));
       *natural = MAX (*natural, MAX (child1_nat, child2_nat));
     }
   else /* VERTICAL */
     {
       *minimum = MAX (*minimum, MAX (child1_min, child2_min));
       *natural = MAX (*natural, MAX (child1_nat, child2_nat));
     }
}

static void
gtk_shadertoy_size_allocate (GtkWidget           *widget,
                         const GtkAllocation *allocation,
                         int                  baseline,
                         GtkAllocation       *out_clip)
{
  GtkShadertoy *self = GTK_SHADERTOY (widget);
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;

  gtk_widget_get_preferred_size (self->child1, &child_requisition, NULL);
  gtk_widget_get_preferred_size (self->child2, &child_requisition, NULL);
  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation->width;
  child_allocation.height = allocation->height;

  gtk_widget_size_allocate (self->child1, &child_allocation, -1, out_clip);
  gtk_widget_size_allocate (self->child2, &child_allocation, -1, out_clip);
}

static void
gtk_shadertoy_class_init (GtkShadertoyClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shadertoy_finalize;
  object_class->get_property = gtk_shadertoy_get_property;
  object_class->set_property = gtk_shadertoy_set_property;
  widget_class->snapshot = gtk_shadertoy_snapshot;
  widget_class->measure = gtk_shadertoy_measure;
  widget_class->size_allocate = gtk_shadertoy_size_allocate;

  pspec = g_param_spec_float ("time", NULL, NULL,
                              0.0, G_MAXFLOAT, 0.0,
                              G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_TIME, pspec);

  pspec = g_param_spec_boolean ("running", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_RUNNING, pspec);
}

static void
gtk_shadertoy_init (GtkShadertoy *self)
{
  GdkPixbuf *pixbuf;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->child2 = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (self->child2), "Stat rosa prisina nomine, nomina nuda tenemus");
  gtk_widget_set_parent (self->child2, GTK_WIDGET (self));

  pixbuf = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg", 400, 400, TRUE, NULL);
  self->child1 = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (pixbuf);
  gtk_widget_set_parent (self->child1, GTK_WIDGET (self));
}

static GtkWidget *
gtk_shadertoy_new (void)
{
  return g_object_new (gtk_shadertoy_get_type (), NULL);
}

static void
gtk_shadertoy_set_fragment_shader (GtkShadertoy *self,
                                   GBytes       *shader)
{
  if (self->fragment_shader)
    g_bytes_unref (self->fragment_shader);
  self->fragment_shader = shader;
  if (self->fragment_shader)
    g_bytes_ref (self->fragment_shader);
}

static gboolean
tick_cb (GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
  GtkShadertoy *self = GTK_SHADERTOY (widget);

  self->time = self->base + (g_get_monotonic_time () - self->starttime) / 1000000.0;
  g_object_notify (G_OBJECT (widget), "time");

  gtk_widget_queue_draw (widget);
  return G_SOURCE_CONTINUE;
}

static void
gtk_shadertoy_set_running (GtkShadertoy *self,
                           gboolean      running)
{
  if (running && self->tick == 0)
    {
      self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
      self->starttime = g_get_monotonic_time ();
      self->time = 0;
    }
  else if (!running && self->tick != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
      self->base += (g_get_monotonic_time () - self->starttime) / 1000000.0;
      self->tick = 0;
    }
  else
    return;

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify (G_OBJECT (self), "running");
}

static gboolean
gtk_shadertoy_is_running (GtkShadertoy *self)
{
  return self->tick != 0;
}

static void
gtk_shadertoy_reset_time (GtkShadertoy *self)
{
  self->base = 0;
  self->time = 0;
  self->starttime = g_get_monotonic_time ();

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify (G_OBJECT (self), "time");
}

static GtkWidget *window = NULL;
static GtkWidget *textview;
static GtkWidget *toy;

static const char *frag_text =
"#version 420 core\n"
"\n"
"layout(location = 0) in vec2 iPos;\n"
"layout(location = 1) in vec2 iTexCoord1;\n"
"layout(location = 2) in vec2 iTexCoord2;\n"
"layout(location = 3) in float iTime;\n"
"layout(location = 4) in vec2 iResolution;\n"
"\n"
"layout(set = 0, binding = 0) uniform sampler2D iTexture1;\n"
"layout(set = 1, binding = 0) uniform sampler2D iTexture2;\n"
"\n"
"layout(location = 0) out vec4 color;\n"
"\n"
"void\n"
"mainImage (out vec4 fragColor, in vec2 fragCoord)\n"
"{\n"
"  vec2 uv = fragCoord.xy / iResolution.xy;\n"
"  fragColor = texture(iTexture1,iTexCoord1*(0.3*sin(iTime) + 0.5));\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"  mainImage (color, iPos);\n"
"}";

static gboolean
update_shader (void)
{
  GError *error = NULL;
  int status;
  char *spv;
  gsize length;
  GBytes *shader;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  char *text;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  if (!g_file_set_contents ("gtk4-demo-shader.frag", text, -1, &error))
    {
      g_print ("Failed to write shader file: %s\n", error->message);
      g_error_free (error);
      g_free (text);
      return FALSE;
    }

  g_free (text);

  if (!g_spawn_command_line_sync ("glslc -fshader-stage=fragment -DCLIP_NONE gtk4-demo-shader.frag -o gtk4-demo-shader.frag.spv", NULL, NULL, &status, &error))
    {
      g_print ("Running glslc failed (%d): %s\n", status, error->message);
      g_error_free (error);
      return FALSE;
    }

  if (!g_file_get_contents ("gtk4-demo-shader.frag.spv", &spv, &length, &error))
    {
      g_print ("Reading compiled shader failed: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  shader = g_bytes_new_take (spv, length);
  gtk_shadertoy_set_fragment_shader (GTK_SHADERTOY (toy), shader);
  g_bytes_unref (shader);

  return TRUE;
}

static void
play_cb (GtkButton *button)
{
  if (gtk_shadertoy_is_running (GTK_SHADERTOY (toy)))
    {
      gtk_shadertoy_set_running (GTK_SHADERTOY (toy), FALSE);
      gtk_button_set_icon_name (button, "media-playback-start-symbolic");
    }
  else
    {
      if (update_shader ())
        {
          gtk_shadertoy_set_running (GTK_SHADERTOY (toy), TRUE);
          gtk_button_set_icon_name (button, "media-playback-stop-symbolic");
        }
    }
}

static void
rewind_cb (GtkButton *button)
{
  gtk_shadertoy_reset_time (GTK_SHADERTOY (toy));
}

static gboolean
format_time (GBinding     *binding,
             const GValue *from_value,
             GValue       *to_value,
             gpointer      data)
{
  char buffer[256];

  g_snprintf (buffer, sizeof (buffer), "%.2f", g_value_get_float (from_value));
  g_value_set_string (to_value, buffer);

  return TRUE;
}

static void
save_cb (GtkButton *button)
{
  GtkFileChooserNative *fs;
  int response;

  fs = gtk_file_chooser_native_new ("Save Shader",
                                    GTK_WINDOW (window),
                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                    "Save", "Cancel");
  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (fs), TRUE);

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (fs));

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GtkTextBuffer *buffer;
      GtkTextIter start, end;
      char *text;
      char *filename;
      GError *error = NULL;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs));

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

      if (!g_file_set_contents (filename, text, -1, &error))
        {
          g_print ("Failed to save :-( %s\n", error->message);
          g_error_free (error);
        }

      g_free (filename);
      g_free (text);
    }

  g_object_unref (fs);
}

static void
load_cb (GtkButton *button)
{
  GtkFileChooserNative *fs;
  int response;

  fs = gtk_file_chooser_native_new ("Load Shader",
                                    GTK_WINDOW (window),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    "Open", "Cancel");
  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (fs), TRUE);

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (fs));

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GtkTextBuffer *buffer;
      char *text;
      gsize len;
      char *filename;
      GError *error = NULL;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs));

      if (!g_file_get_contents (filename, &text, &len, &error))
        {
          g_print ("Failed to load :-( %s\n", error->message);
          g_error_free (error);
        }
      else
        {
          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
          gtk_text_buffer_set_text (buffer, text, len);
          g_free (text);
        }

      g_free (filename);
    }

  g_object_unref (fs);
}

GtkWidget *
do_shadertoy (GtkWidget *do_widget)
{

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *content;
      GtkWidget *rewind;
      GtkWidget *play;
      GtkWidget *time;
      GtkWidget *load;
      GtkWidget *save;
      GtkTextBuffer *buffer;

      builder = gtk_builder_new_from_resource ("/shadertoy/shadertoy.ui");

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      content = GTK_WIDGET (gtk_builder_get_object (builder, "content"));
      textview = GTK_WIDGET (gtk_builder_get_object (builder, "text"));
      rewind = GTK_WIDGET (gtk_builder_get_object (builder, "rewind"));
      play = GTK_WIDGET (gtk_builder_get_object (builder, "play"));
      time = GTK_WIDGET (gtk_builder_get_object (builder, "time"));
      load = GTK_WIDGET (gtk_builder_get_object (builder, "load"));
      save = GTK_WIDGET (gtk_builder_get_object (builder, "save"));

      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      toy = gtk_shadertoy_new ();
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
      gtk_text_buffer_set_text (buffer, frag_text, -1);

      gtk_widget_set_hexpand (toy, TRUE);
      gtk_widget_set_vexpand (toy, TRUE);
      gtk_container_add (GTK_CONTAINER (content), toy);

      g_signal_connect (play, "clicked", G_CALLBACK (play_cb), NULL);
      g_signal_connect (rewind, "clicked", G_CALLBACK (rewind_cb), NULL);
      g_signal_connect (load, "clicked", G_CALLBACK (load_cb), NULL);
      g_signal_connect (save, "clicked", G_CALLBACK (save_cb), NULL);
      g_object_bind_property_full (toy, "time",
                                   time, "label",
                                   G_BINDING_SYNC_CREATE,
                                   format_time,
                                   NULL,
                                   NULL,
                                   NULL);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
