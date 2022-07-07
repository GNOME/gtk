#include "fontcontrols.h"
#include "rangeedit.h"

#include <gtk/gtk.h>
#include <hb-ot.h>

enum {
  PROP_SIZE = 1,
  PROP_LETTERSPACING,
  PROP_LINE_HEIGHT,
  PROP_FOREGROUND,
  PROP_BACKGROUND,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _FontControls
{
  GtkWidget parent;

  GtkAdjustment *size_adjustment;
  GtkAdjustment *letterspacing_adjustment;
  GtkAdjustment *line_height_adjustment;
  GtkColorButton *foreground;
  GtkColorButton *background;

  GSimpleAction *reset_action;
};

struct _FontControlsClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(FontControls, font_controls, GTK_TYPE_WIDGET);

static void
size_changed (GtkAdjustment *adjustment,
              FontControls   *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIZE]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
letterspacing_changed (GtkAdjustment *adjustment,
                       FontControls   *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LETTERSPACING]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
line_height_changed (GtkAdjustment *adjustment,
                     FontControls   *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LINE_HEIGHT]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
color_set (GtkColorButton *button,
           GParamSpec     *pspec,
           FontControls    *self)
{
  if (button == self->foreground)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOREGROUND]);
  else
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
swap_colors (GtkButton   *button,
             FontControls *self)
{
  GdkRGBA fg;
  GdkRGBA bg;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->foreground), &fg);
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->background), &bg);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->foreground), &bg);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->background), &fg);
}

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       FontControls   *self)
{
  gtk_adjustment_set_value (self->size_adjustment, 12.);
  gtk_adjustment_set_value (self->letterspacing_adjustment, 0.);
  gtk_adjustment_set_value (self->line_height_adjustment, 1.);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->foreground), &(GdkRGBA){0., 0., 0., 1.0 });
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->background), &(GdkRGBA){1., 1., 1., 1.0 });

  g_simple_action_set_enabled (self->reset_action, FALSE);
}

static void
font_controls_init (FontControls *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->reset_action = g_simple_action_new ("reset", NULL);
  g_simple_action_set_enabled (self->reset_action, FALSE);
  g_signal_connect (self->reset_action, "activate", G_CALLBACK (reset), self);
}

static void
font_controls_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), FONT_CONTROLS_TYPE);

  G_OBJECT_CLASS (font_controls_parent_class)->dispose (object);
}

static void
font_controls_finalize (GObject *object)
{
  //FontControls *self = FONT_CONTROLS (object);

  G_OBJECT_CLASS (font_controls_parent_class)->finalize (object);
}

static void
font_controls_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  FontControls *self = FONT_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_SIZE:
      gtk_adjustment_set_value (self->size_adjustment, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_controls_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  FontControls *self = FONT_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_SIZE:
      g_value_set_float (value, gtk_adjustment_get_value (self->size_adjustment));
      break;

    case PROP_LETTERSPACING:
      g_value_set_int (value, (int) gtk_adjustment_get_value (self->letterspacing_adjustment));
      break;

    case PROP_LINE_HEIGHT:
      g_value_set_float (value, gtk_adjustment_get_value (self->line_height_adjustment));
      break;

    case PROP_FOREGROUND:
      {
        GdkRGBA rgba;
        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->foreground), &rgba);
        g_value_set_boxed (value, &rgba);
      }
      break;

    case PROP_BACKGROUND:
      {
        GdkRGBA rgba;
        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->background), &rgba);
        g_value_set_boxed (value, &rgba);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_controls_class_init (FontControlsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_ensure (RANGE_EDIT_TYPE);

  object_class->dispose = font_controls_dispose;
  object_class->finalize = font_controls_finalize;
  object_class->get_property = font_controls_get_property;
  object_class->set_property = font_controls_set_property;

  properties[PROP_SIZE] =
      g_param_spec_float ("size", "", "",
                          0., 100., 12.,
                          G_PARAM_READABLE);

  properties[PROP_LETTERSPACING] =
      g_param_spec_int ("letterspacing", "", "",
                        -G_MAXINT, G_MAXINT, 0,
                        G_PARAM_READABLE);

  properties[PROP_LINE_HEIGHT] =
      g_param_spec_float ("line-height", "", "",
                          0., 100., 1.,
                          G_PARAM_READABLE);

  properties[PROP_FOREGROUND] =
      g_param_spec_boxed ("foreground", "", "",
                          GDK_TYPE_RGBA,
                          G_PARAM_READABLE);

  properties[PROP_BACKGROUND] =
      g_param_spec_boxed ("background", "", "",
                          GDK_TYPE_RGBA,
                          G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontcontrols.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontControls, size_adjustment);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontControls, letterspacing_adjustment);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontControls, line_height_adjustment);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontControls, foreground);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontControls, background);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), size_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), letterspacing_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), line_height_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), color_set);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), swap_colors);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontcontrols");
}

FontControls *
font_controls_new (void)
{
  return g_object_new (FONT_CONTROLS_TYPE, NULL);
}

GAction *
font_controls_get_reset_action (FontControls *self)
{
  return G_ACTION (self->reset_action);
}
