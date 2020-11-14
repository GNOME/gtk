#include "simplelabel.h"

struct _SimpleLabel
{
  GtkWidget parent_instance;

  PangoLayout *layout;
  int min_chars;
  int nat_chars;

  int min_width;
  int nat_width;
  int height;
};

struct _SimpleLabelClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_TEXT = 1,
  PROP_MIN_CHARS,
  PROP_NAT_CHARS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (SimpleLabel, simple_label, GTK_TYPE_WIDGET)

static void
simple_label_init (SimpleLabel *self)
{
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "");
  pango_layout_set_ellipsize (self->layout, PANGO_ELLIPSIZE_NONE);
  pango_layout_set_wrap (self->layout, PANGO_WRAP_WORD);
  pango_layout_set_width (self->layout, -1);
}

static void
simple_label_dispose (GObject *object)
{
  SimpleLabel *self = SIMPLE_LABEL (object);

  g_clear_object (&self->layout);

  G_OBJECT_CLASS (simple_label_parent_class)->dispose (object);
}

static void
simple_label_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  SimpleLabel *self = SIMPLE_LABEL (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      simple_label_set_text (self, g_value_get_string (value));
      break;

    case PROP_MIN_CHARS:
      simple_label_set_min_chars (self, g_value_get_int (value));
      break;

    case PROP_NAT_CHARS:
      simple_label_set_nat_chars (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
simple_label_get_property (GObject     *object,
                           guint        prop_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
  SimpleLabel *self = SIMPLE_LABEL (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, pango_layout_get_text (self->layout));
      break;

    case PROP_MIN_CHARS:
      g_value_set_int (value, self->min_chars);
      break;

    case PROP_NAT_CHARS:
      g_value_set_int (value, self->nat_chars);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
simple_label_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  SimpleLabel *self = SIMPLE_LABEL (widget);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum = *natural = self->height;
    }
  else
    {
      *minimum = self->min_width;
      *natural = self->nat_width;
    }
}

static void
simple_label_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  SimpleLabel *self = SIMPLE_LABEL (widget);

  pango_layout_set_width (self->layout, width * PANGO_SCALE);
}

static void
simple_label_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  SimpleLabel *self = SIMPLE_LABEL (widget);
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_snapshot_render_layout (snapshot, context, 0, 0, self->layout);
}

static void
simple_label_class_init (SimpleLabelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = simple_label_dispose;
  object_class->set_property = simple_label_set_property;
  object_class->get_property = simple_label_get_property;

  widget_class->measure = simple_label_measure;
  widget_class->size_allocate = simple_label_size_allocate;
  widget_class->snapshot = simple_label_snapshot;

  properties[PROP_TEXT] =
    g_param_spec_string ("text", "Text", "Text",
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_MIN_CHARS] =
    g_param_spec_int ("min-chars", "Minimum Characters", "Minimum Characters",
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE);

  properties[PROP_NAT_CHARS] =
    g_param_spec_int ("nat-chars", "Natural Characters", "Natural Characters",
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

GtkWidget *
simple_label_new (void)
{
  return g_object_new (SIMPLE_TYPE_LABEL, NULL);
}

void
simple_label_set_text (SimpleLabel *self,
                       const char  *text)
{
  if (strcmp (text, pango_layout_get_text (self->layout)) == 0)
    return;

  pango_layout_set_text (self->layout, text, -1);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

static void
recalculate (SimpleLabel *self)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  int char_width;
  int digit_width;
  int width;
  int ascent;
  int descent;

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  ascent = pango_font_metrics_get_ascent (metrics);
  descent = pango_font_metrics_get_descent (metrics);
  pango_font_metrics_unref (metrics);

  width = MAX (char_width, digit_width);

  self->min_width = (width * self->min_chars) / PANGO_SCALE;
  self->nat_width = (width * self->nat_chars) / PANGO_SCALE;
  self->height = (ascent + descent) / PANGO_SCALE;
}

void
simple_label_set_min_chars (SimpleLabel *self,
                            int          min_chars)
{
  if (self->min_chars == min_chars)
    return;

  self->min_chars = min_chars;

  recalculate (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_CHARS]);
}

void
simple_label_set_nat_chars (SimpleLabel *self,
                            int          nat_chars)
{
  if (self->nat_chars == nat_chars)
    return;

  self->nat_chars = nat_chars;

  recalculate (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAT_CHARS]);
}
