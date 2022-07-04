#include "rangeedit.h"
#include <gtk/gtk.h>
#include <hb-ot.h>

enum {
  PROP_ADJUSTMENT = 1,
  PROP_DEFAULT_VALUE,
  PROP_N_CHARS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _RangeEdit
{
  GtkWidget parent;

  GtkAdjustment *adjustment;
  GtkScale *scale;
  GtkEntry *entry;
  double default_value;
  int n_chars;
};

struct _RangeEditClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (RangeEdit, range_edit, GTK_TYPE_WIDGET);

static void
range_edit_init (RangeEdit *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
range_edit_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), RANGE_EDIT_TYPE);

  G_OBJECT_CLASS (range_edit_parent_class)->dispose (object);
}

static void
range_edit_get_property (GObject      *object,
                         unsigned int  prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  RangeEdit *self = RANGE_EDIT (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, self->adjustment);
      break;

    case PROP_DEFAULT_VALUE:
      g_value_set_double (value, self->default_value);
      break;

    case PROP_N_CHARS:
      g_value_set_int (value, self->n_chars);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    RangeEdit     *self)
{
  char *str;

  str = g_strdup_printf ("%.1f", gtk_adjustment_get_value (adjustment));
  gtk_editable_set_text (GTK_EDITABLE (self->entry), str);
  g_free (str);
}

static void
range_edit_set_property (GObject      *object,
                         unsigned int  prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  RangeEdit *self = RANGE_EDIT (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_set_object (&self->adjustment, g_value_get_object (value));
      g_signal_connect (self->adjustment, "value-changed", G_CALLBACK (adjustment_changed), self);
      adjustment_changed (self->adjustment, self);
      break;

    case PROP_DEFAULT_VALUE:
      self->default_value = g_value_get_double (value);
      break;

    case PROP_N_CHARS:
      self->n_chars = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
range_edit_constructed (GObject *object)
{
  RangeEdit *self = RANGE_EDIT (object);

  gtk_scale_add_mark (self->scale, self->default_value, GTK_POS_TOP, NULL);
}

static void
entry_activated (GtkEntry  *entry,
                 RangeEdit *self)
{
  double value;
  char *err = NULL;

  value = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    gtk_adjustment_set_value (self->adjustment, value);
}

static void
range_edit_class_init (RangeEditClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = range_edit_dispose;
  object_class->get_property = range_edit_get_property;
  object_class->set_property = range_edit_set_property;
  object_class->constructed = range_edit_constructed;

  properties[PROP_ADJUSTMENT] =
      g_param_spec_object ("adjustment", "", "",
                           GTK_TYPE_ADJUSTMENT,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_DEFAULT_VALUE] =
      g_param_spec_double ("default-value", "", "",
                           -G_MAXDOUBLE, G_MAXDOUBLE, 0.,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_N_CHARS] =
      g_param_spec_int ("n-chars", "", "",
                        0, G_MAXINT, 10,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/rangeedit.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RangeEdit, scale);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), RangeEdit, entry);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), entry_activated);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "rangeedit");
}

RangeEdit *
range_edit_new (void)
{
  return g_object_new (RANGE_EDIT_TYPE, NULL);
}
