#include "sampleeditor.h"
#include <gtk/gtk.h>

enum {
  PROP_SAMPLE_TEXT = 1,
  PROP_EDITING,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _SampleEditor
{
  GtkWidget parent;

  GtkTextView *edit;
  char *sample_text;
  gboolean editing;
};

struct _SampleEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(SampleEditor, sample_editor, GTK_TYPE_WIDGET);

static void
sample_editor_init (SampleEditor *self)
{
  self->sample_text = g_strdup ("Some sample text is better than other sample text");
  self->editing = FALSE;

  gtk_widget_set_layout_manager (GTK_WIDGET (self),
                                 gtk_box_layout_new (GTK_ORIENTATION_VERTICAL));
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
sample_editor_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), SAMPLE_EDITOR_TYPE);

  G_OBJECT_CLASS (sample_editor_parent_class)->dispose (object);
}

static void
sample_editor_finalize (GObject *object)
{
  SampleEditor *self = SAMPLE_EDITOR (object);

  g_free (self->sample_text);

  G_OBJECT_CLASS (sample_editor_parent_class)->finalize (object);
}

static void
update_editing (SampleEditor *self,
                gboolean      editing)
{
  GtkTextBuffer *buffer;

  if (self->editing == editing)
    return;

  self->editing = editing;

  buffer = gtk_text_view_get_buffer (self->edit);

  if (self->editing)
    {
      gtk_text_buffer_set_text (buffer, self->sample_text, -1);
      gtk_widget_grab_focus (GTK_WIDGET (self->edit));
    }
  else
    {
      GtkTextIter start, end;

      g_free (self->sample_text);
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      self->sample_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SAMPLE_TEXT]);
    }
}

static void
sample_editor_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  SampleEditor *self = SAMPLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_SAMPLE_TEXT:
      g_free (self->sample_text);
      self->sample_text = g_strdup (g_value_get_string (value));
      break;

    case PROP_EDITING:
      update_editing (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sample_editor_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  SampleEditor *self = SAMPLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_SAMPLE_TEXT:
      g_value_set_string (value, self->sample_text);
      break;

    case PROP_EDITING:
      g_value_set_boolean (value, self->editing);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sample_editor_class_init (SampleEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = sample_editor_dispose;
  object_class->finalize = sample_editor_finalize;
  object_class->get_property = sample_editor_get_property;
  object_class->set_property = sample_editor_set_property;

  properties[PROP_SAMPLE_TEXT] =
      g_param_spec_string ("sample-text", "", "",
                           "",
                           G_PARAM_READWRITE);

  properties[PROP_EDITING] =
      g_param_spec_boolean ("editing", "", "",
                            FALSE,
                            G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/sampleeditor.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), SampleEditor, edit);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "sampleeditor");
}
