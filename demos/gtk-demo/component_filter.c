#include "component_filter.h"
#include "minigraph.h"

typedef enum {
  IDENTITY,
  LEVELS,
  LINEAR,
  GAMMA,
  DISCRETE,
  TABLE,
} FilterKind;

struct _ComponentFilter
{
  GtkWidget parent_instance;

  GtkDropDown *box;
  GtkDropDown *kind;
  GtkStack *stack;
  GtkSpinButton *levels;
  GtkSpinButton *linear_m;
  GtkSpinButton *linear_b;
  GtkSpinButton *gamma_amp;
  GtkSpinButton *gamma_exp;
  GtkSpinButton *gamma_ofs;
  GtkSpinButton *discrete_size;
  union {
    GtkSpinButton *discrete_values[6];
    struct {
      GtkSpinButton *discrete_value0;
      GtkSpinButton *discrete_value1;
      GtkSpinButton *discrete_value2;
      GtkSpinButton *discrete_value3;
      GtkSpinButton *discrete_value4;
      GtkSpinButton *discrete_value5;
    };
  };
  MiniGraph *graph;

  FilterKind filter_kind;

  GskComponentTransfer *component_transfer;
};

struct _ComponentFilterClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_TRANSFER = 1,
  NUM_PROPERTIES,
};

G_DEFINE_TYPE (ComponentFilter, component_filter, GTK_TYPE_WIDGET)

static void
component_filter_init (ComponentFilter *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->component_transfer = gsk_component_transfer_new_identity ();
}

static void
component_filter_dispose (GObject *object)
{
  ComponentFilter *self = COMPONENT_FILTER (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), component_filter_get_type ());

  G_OBJECT_CLASS (component_filter_parent_class)->dispose (object);
}

static void
component_filter_finalize (GObject *object)
{
  ComponentFilter *self = COMPONENT_FILTER (object);

  gsk_component_transfer_free (self->component_transfer);

  G_OBJECT_CLASS (component_filter_parent_class)->finalize (object);
}

static void
update_component_transfer (ComponentFilter *self)
{
  const char *page[] = { "identity", "levels", "linear", "gamma", "discrete", "discrete" };

  self->filter_kind = gtk_drop_down_get_selected (self->kind);
  gtk_stack_set_visible_child_name (self->stack, page[self->filter_kind]);

  gsk_component_transfer_free (self->component_transfer);

  switch (self->filter_kind)
    {
    case IDENTITY:
      self->component_transfer = gsk_component_transfer_new_identity ();
      mini_graph_set_identity (self->graph);
      break;
    case LEVELS:
      {
        guint n = gtk_spin_button_get_value_as_int (self->levels);
        self->component_transfer = gsk_component_transfer_new_levels (n);
        mini_graph_set_levels (self->graph, n);
      }
      break;
    case LINEAR:
      {
        float m = gtk_spin_button_get_value (self->linear_m);
        float b = gtk_spin_button_get_value (self->linear_b);
        self->component_transfer = gsk_component_transfer_new_linear (m, b);
        mini_graph_set_linear (self->graph, m, b);
      }
      break;
    case GAMMA:
      {
        float amp = gtk_spin_button_get_value (self->gamma_amp);
        float exp = gtk_spin_button_get_value (self->gamma_exp);
        float ofs = gtk_spin_button_get_value (self->gamma_ofs);
        self->component_transfer = gsk_component_transfer_new_gamma (amp, exp, ofs);
        mini_graph_set_gamma (self->graph, amp, exp, ofs);
      }
      break;
    case DISCRETE:
      {
        guint n = gtk_spin_button_get_value_as_int (self->discrete_size);
        float values[6];
        for (guint i = 0; i < 6; i++)
          {
            gtk_widget_set_visible (GTK_WIDGET (self->discrete_values[i]), i < n);
            values[i] = gtk_spin_button_get_value (self->discrete_values[i]);
          }
        self->component_transfer = gsk_component_transfer_new_discrete (n, values);
        mini_graph_set_discrete (self->graph, n, values);
      }
      break;
    case TABLE:
      {
        guint n = gtk_spin_button_get_value_as_int (self->discrete_size);
        float values[6];
        for (guint i = 0; i < 6; i++)
          {
            gtk_widget_set_visible (GTK_WIDGET (self->discrete_values[i]), i < n);
            values[i] = gtk_spin_button_get_value (self->discrete_values[i]);
          }
        self->component_transfer = gsk_component_transfer_new_table (n, values);
        mini_graph_set_table (self->graph, n, values);
      }
      break;
    default:
      g_assert_not_reached ();
    }

  g_object_notify (G_OBJECT (self), "transfer");
}

static void
component_filter_get_property (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  ComponentFilter *self = COMPONENT_FILTER (object);

  switch (prop_id)
    {
    case PROP_TRANSFER:
      g_value_set_boxed (value, self->component_transfer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
component_filter_class_init (ComponentFilterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (mini_graph_get_type ());

  object_class->dispose = component_filter_dispose;
  object_class->finalize = component_filter_finalize;
  object_class->get_property = component_filter_get_property;

  g_object_class_install_property (object_class,
                                   PROP_TRANSFER,
                                   g_param_spec_boxed ("transfer", NULL, NULL,
                                                       GSK_TYPE_COMPONENT_TRANSFER,
                                                       G_PARAM_READABLE));

  gtk_widget_class_set_template_from_resource (widget_class, "/image_filter/component_filter.ui");

  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, box);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, kind);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, stack);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, levels);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, linear_m);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, linear_b);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, gamma_amp);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, gamma_exp);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, gamma_ofs);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_size);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_value0);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_value1);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_value2);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_value3);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_value4);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, discrete_value5);
  gtk_widget_class_bind_template_child (widget_class, ComponentFilter, graph);
  gtk_widget_class_bind_template_callback (widget_class, update_component_transfer);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

GtkWidget *
component_filter_new (void)
{
  return g_object_new (component_filter_get_type (), NULL);
}

GskComponentTransfer *
component_filter_get_component_transfer (ComponentFilter *self)
{
  return self->component_transfer;
}
