/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcolorchoicebutton.h"
#include "gtkcolorchoice.h"

#include "gtkbinlayout.h"
#include "gtkbutton.h"
#include "gtkcolorswatchprivate.h"
#include "gtkdragsource.h"
#include "gtkdroptarget.h"
#include <glib/gi18n-lib.h>
//#include "gtkmain.h"
#include "gtkprivate.h"
//#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"


static gboolean drop           (GtkDropTarget        *dest,
                                const GValue         *value,
                                double                x,
                                double                y,
                                GtkColorChoiceButton *self);
static GdkContentProvider *
                  drag_prepare (GtkDragSource        *source,
                                double                x,
                                double                y,
                                GtkColorChoiceButton *self);
static void     button_clicked (GtkColorChoiceButton *self);

/* {{{ GObject implementation */

struct _GtkColorChoiceButton
{
  GtkWidget parent_instance;

  GdkRGBA color;
  gboolean use_alpha;
  char *title;

  GtkWidget *button;
  GtkWidget *swatch;
  GtkColorChoice *choice;
};

/* Properties */
enum
{
  PROP_COLOR = 1,
  PROP_USE_ALPHA,
  PROP_TITLE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkColorChoiceButton, gtk_color_choice_button, GTK_TYPE_WIDGET)

static void
gtk_color_choice_button_init (GtkColorChoiceButton *self)
{
  PangoLayout *layout;
  PangoRectangle rect;
  GtkDragSource *source;
  GtkDropTarget *dest;

  self->use_alpha = TRUE;
  self->title = g_strdup ("");

  self->button = gtk_button_new ();
  g_signal_connect_swapped (self->button, "clicked", G_CALLBACK (button_clicked), self);
  gtk_widget_set_parent (self->button, GTK_WIDGET (self));

  self->swatch = g_object_new (GTK_TYPE_COLOR_SWATCH,
                               "accessible-role", GTK_ACCESSIBLE_ROLE_IMG,
                               "selectable", FALSE,
                               "has-menu", FALSE,
                               "can-drag", FALSE,
                               NULL);
  gtk_widget_set_can_focus (self->swatch, FALSE);
  gtk_widget_remove_css_class (self->swatch, "activatable");

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "Black");
  pango_layout_get_pixel_extents (layout, NULL, &rect);
  g_object_unref (layout);

  gtk_widget_set_size_request (self->swatch, rect.width, rect.height);

  gtk_button_set_child (GTK_BUTTON (self->button), self->swatch);

  dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (drop), self);
  gtk_widget_add_controller (GTK_WIDGET (self->button), GTK_EVENT_CONTROLLER (dest));

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (source),
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (self->button, GTK_EVENT_CONTROLLER (source));
  gtk_widget_add_css_class (self->button, "color");
}

static void
gtk_color_choice_button_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkColorChoiceButton *self = GTK_COLOR_CHOICE_BUTTON (object);

  switch (param_id)
    {
    case PROP_COLOR:
      gtk_color_choice_button_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_USE_ALPHA:
      gtk_color_choice_button_set_use_alpha (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      gtk_color_choice_button_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_color_choice_button_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkColorChoiceButton *self = GTK_COLOR_CHOICE_BUTTON (object);

  switch (param_id)
    {
    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
      break;

    case PROP_USE_ALPHA:
      g_value_set_boolean (value, self->use_alpha);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_color_choice_button_dispose (GObject *object)
{
  GtkColorChoiceButton *self = GTK_COLOR_CHOICE_BUTTON (object);

  g_clear_pointer (&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_color_choice_button_parent_class)->dispose (object);
}

static void
gtk_color_choice_button_finalize (GObject *object)
{
  GtkColorChoiceButton *self = GTK_COLOR_CHOICE_BUTTON (object);

  g_free (self->title);

  G_OBJECT_CLASS (gtk_color_choice_button_parent_class)->finalize (object);
}

static void
gtk_color_choice_button_class_init (GtkColorChoiceButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_color_choice_button_get_property;
  object_class->set_property = gtk_color_choice_button_set_property;
  object_class->dispose = gtk_color_choice_button_dispose;
  object_class->finalize = gtk_color_choice_button_finalize;

  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;

  properties[PROP_COLOR] =
      g_param_spec_boxed ("color", NULL, NULL,
                          GDK_TYPE_RGBA,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

  properties[PROP_USE_ALPHA] =
      g_param_spec_boolean ("use-alpha", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           "",
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "colorbutton");
}

/* }}} */
/* {{{ Private API, callbacks */

static guint
scale_round (double value,
             double scale)
{
  value = floor (value * scale + 0.5);
  value = CLAMP (value, 0, scale);
  return (guint)value;
}

static char *
accessible_color_name (const GdkRGBA *color)
{
  if (color->alpha < 1.0)
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%, Alpha %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100),
                            scale_round (color->alpha, 100));
  else
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100));
}

static gboolean
drop (GtkDropTarget        *dest,
      const GValue         *value,
      double                x,
      double                y,
      GtkColorChoiceButton *self)
{
  GdkRGBA *color = g_value_get_boxed (value);

  gtk_color_choice_button_set_color (self, color);

  return TRUE;
}

static GdkContentProvider *
drag_prepare (GtkDragSource        *source,
              double                x,
              double                y,
              GtkColorChoiceButton *self)
{
  return gdk_content_provider_new_typed (GDK_TYPE_RGBA, &self->color);
}

static void
color_chosen (GObject      *source,
              GAsyncResult *result,
              gpointer      data)
{
  GtkColorChoice *choice = GTK_COLOR_CHOICE (source);
  GtkColorChoiceButton *self = data;
  GdkRGBA *color;
  GError *error = NULL;

  color = gtk_color_choice_choose_finish (choice, result, &error);
  if (color)
    {
      gtk_color_choice_button_set_color (self, color);
      gdk_rgba_free (color);
    }
  else
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
    }
}

static void
button_clicked (GtkColorChoiceButton *self)
{
  GtkRoot *root;
  GtkWindow *parent = NULL;
  GtkColorChoice *choice;

  root = gtk_widget_get_root (GTK_WIDGET (self));
  if (GTK_IS_WINDOW (root))
    parent = GTK_WINDOW (root);

  choice = gtk_color_choice_new (parent, self->title, self->use_alpha);
  gtk_color_choice_choose (choice, &self->color, NULL, color_chosen, self);
  g_object_unref (choice);
}

/* }}} */
/* {{{ Public API */
/* {{{ Constructor */

GtkColorChoiceButton *
gtk_color_choice_button_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_CHOICE_BUTTON, NULL);
}

/* }}} */
/* {{{ Setters and Getters */

void
gtk_color_choice_button_set_color (GtkColorChoiceButton *self,
                                   const GdkRGBA        *color)
{
  char *text;

  g_return_if_fail (GTK_IS_COLOR_CHOICE_BUTTON (self));
  g_return_if_fail (color != NULL);

  if (gdk_rgba_equal (&self->color, color))
    return;

  self->color = *color;
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (self->swatch), color);

  text = accessible_color_name (color);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->swatch),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, text,
                                  -1);
  g_free (text);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

const GdkRGBA *
gtk_color_choice_button_get_color (GtkColorChoiceButton *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_CHOICE_BUTTON (self), NULL);

  return &self->color;
}

void
gtk_color_choice_button_set_use_alpha (GtkColorChoiceButton *self,
                                       gboolean              use_alpha)
{
  g_return_if_fail (GTK_IS_COLOR_CHOICE_BUTTON (self));

  if (self->use_alpha == use_alpha)
    return;

  self->use_alpha = use_alpha;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_ALPHA]);
}

gboolean
gtk_color_choice_button_get_use_alpha (GtkColorChoiceButton *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_CHOICE_BUTTON (self), TRUE);

  return self->use_alpha;
}

void
gtk_color_choice_button_set_title (GtkColorChoiceButton *self,
                                   const char           *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_COLOR_CHOICE_BUTTON (self));
  g_return_if_fail (title != NULL);

  if (g_str_equal (self->title, title))
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/* }}} */
/* }}} */

/* vim:set foldmethod=marker expandtab: */
