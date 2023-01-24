#include "colorpicker.h"

#include <gtk/gtk.h>

enum {
  PROP_FOREGROUND= 1,
  PROP_BACKGROUND,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _ColorPicker
{
  GtkWidget parent;

  GtkWidget *grid;
  GtkWidget *fg;
  GtkWidget *bg;
  GtkWidget *swap;

  GSimpleAction *reset_action;
};

struct _ColorPickerClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (ColorPicker, color_picker, GTK_TYPE_WIDGET);

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       ColorPicker   *self)
{
  GdkRGBA fg, bg;

  fg = (GdkRGBA) { 0, 0, 0, 1 };
  bg = (GdkRGBA) { 1, 1, 1, 1 };

  g_object_freeze_notify (G_OBJECT (self));

  g_object_set (self, "foreground", &fg, NULL);
  g_object_set (self, "background", &bg, NULL);

  g_object_thaw_notify (G_OBJECT (self));

  g_simple_action_set_enabled (self->reset_action, FALSE);
}

static void
fg_changed (GObject     *obj,
            GParamSpec  *pspec,
            ColorPicker *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOREGROUND]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
bg_changed (GObject     *obj,
            GParamSpec  *pspec,
            ColorPicker *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
swap_colors (GtkButton   *button,
             ColorPicker *self)
{
  const GdkRGBA *fg, *bg;

  g_object_freeze_notify (G_OBJECT (self));

  g_object_get (self, "foreground", &fg, "background", &bg, NULL);
  g_object_set (self, "foreground", bg, "background", fg, NULL);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
color_picker_init (ColorPicker *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->fg, "notify::rgba", G_CALLBACK (fg_changed), self);
  g_signal_connect (self->bg, "notify::rgba", G_CALLBACK (bg_changed), self);
  g_signal_connect (self->swap, "clicked", G_CALLBACK (swap_colors), self);

  self->reset_action = g_simple_action_new ("reset", NULL);
  g_simple_action_set_enabled (self->reset_action, FALSE);
  g_signal_connect (self->reset_action, "activate", G_CALLBACK (reset), self);

  reset (NULL, NULL, self);
}

static void
color_picker_dispose (GObject *object)
{
  //ColorPicker *self = COLOR_PICKER (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), COLOR_PICKER_TYPE);

  G_OBJECT_CLASS (color_picker_parent_class)->dispose (object);
}

static void
color_picker_finalize (GObject *object)
{
  //ColorPicker *self = COLOR_PICKER (object);

  G_OBJECT_CLASS (color_picker_parent_class)->finalize (object);
}

static void
color_picker_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ColorPicker *self = COLOR_PICKER (object);
  const GdkRGBA *color;

  switch (prop_id)
    {
    case PROP_FOREGROUND:
      color = g_value_get_boxed (value);
      g_object_set (self->fg, "rgba", color, NULL);
      break;

    case PROP_BACKGROUND:
      color = g_value_get_boxed (value);
      g_object_set (self->bg, "rgba", color, NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
color_picker_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  ColorPicker *self = COLOR_PICKER (object);
  const GdkRGBA *color;

  switch (prop_id)
    {
    case PROP_FOREGROUND:
      g_object_get (self->fg, "rgba", &color, NULL);
      g_value_set_boxed (value, color);
      break;

    case PROP_BACKGROUND:
      g_object_get (self->bg, "rgba", &color, NULL);
      g_value_set_boxed (value, color);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
color_picker_class_init (ColorPickerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = color_picker_dispose;
  object_class->finalize = color_picker_finalize;
  object_class->get_property = color_picker_get_property;
  object_class->set_property = color_picker_set_property;

  properties[PROP_FOREGROUND] =
      g_param_spec_boxed ("foreground", "", "",
                          GDK_TYPE_RGBA,
                          G_PARAM_READWRITE);

  properties[PROP_BACKGROUND] =
      g_param_spec_boxed ("background", "", "",
                          GDK_TYPE_RGBA,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/paintable_glyph/colorpicker.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), ColorPicker, grid);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), ColorPicker, fg);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), ColorPicker, bg);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), ColorPicker, swap);

  gtk_widget_class_set_layout_manager_type (GTK_WIDGET_CLASS (class), GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "colorpicker");
}

GAction *
color_picker_get_reset_action (ColorPicker *self)
{
  return G_ACTION (self->reset_action);
}
