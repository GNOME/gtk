/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkwindow.h"
#include "gtkwindowprivate.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkprivate.h"
#include "gtksettings.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserwindowprivate.h"
#include "gtkcolorchooserwidget.h"

/*
 * GtkColorChooserWindow:
 *
 * A window for choosing a color.
 *
 * ![An example GtkColorChooserWindow](colorchooser.png)
 *
 * `GtkColorChooserWindow` implements the [iface@Gtk.ColorChooser] interface
 * and does not provide much API of its own.
 *
 * To create a `GtkColorChooserWindow`, use [ctor@Gtk.ColorChooserWindow.new].
 *
 * To change the initially selected color, use
 * [method@Gtk.ColorChooser.set_rgba]. To get the selected color use
 * [method@Gtk.ColorChooser.get_rgba].
 */

struct _GtkColorChooserWindow
{
  GtkWindow parent_instance;

  GtkWidget *chooser;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;
};

enum
{
  PROP_RGBA = 1,
  PROP_USE_ALPHA,
  PROP_SHOW_EDITOR
};

static void gtk_color_chooser_window_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorChooserWindow, gtk_color_chooser_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_chooser_window_iface_init))

static void
propagate_notify (GObject               *o,
                  GParamSpec            *pspec,
                  GtkColorChooserWindow *self)
{
  g_object_notify (G_OBJECT (self), pspec->name);
}

static void
color_activated_cb (GtkColorChooser *chooser,
                    GdkRGBA         *color,
                    GtkColorChooserWindow *window)
{
  g_signal_emit_by_name (window->ok_button, "clicked");
}

static void
gtk_color_chooser_window_init (GtkColorChooserWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gtk_color_chooser_window_unmap (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_color_chooser_window_parent_class)->unmap (widget);

  /* We never want the window to come up with the editor,
   * even if it was showing the editor the last time it was used.
   */
  g_object_set (widget, "show-editor", FALSE, NULL);
}

static void
gtk_color_chooser_window_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkColorChooserWindow *self = GTK_COLOR_CHOOSER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self), &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_color_chooser_get_use_alpha (GTK_COLOR_CHOOSER (self->chooser)));
      break;
    case PROP_SHOW_EDITOR:
      {
        gboolean show_editor;
        g_object_get (self->chooser, "show-editor", &show_editor, NULL);
        g_value_set_boolean (value, show_editor);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_window_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserWindow *self = GTK_COLOR_CHOOSER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self), g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      if (gtk_color_chooser_get_use_alpha (GTK_COLOR_CHOOSER (self->chooser)) != g_value_get_boolean (value))
        {
          gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (self->chooser), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SHOW_EDITOR:
      g_object_set (self->chooser,
                    "show-editor", g_value_get_boolean (value),
                    NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_window_dispose (GObject *object)
{
  GtkColorChooserWindow *self = GTK_COLOR_CHOOSER_WINDOW (object);

  g_clear_pointer (&self->chooser, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_color_chooser_window_parent_class)->dispose (object);
}

static void
gtk_color_chooser_window_class_init (GtkColorChooserWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_color_chooser_window_dispose;
  object_class->get_property = gtk_color_chooser_window_get_property;
  object_class->set_property = gtk_color_chooser_window_set_property;

  widget_class->unmap = gtk_color_chooser_window_unmap;

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");
  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", NULL, NULL,
                            FALSE, GTK_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtkcolorchooserwindow.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkColorChooserWindow, chooser);
  gtk_widget_class_bind_template_child (widget_class, GtkColorChooserWindow, ok_button);
  gtk_widget_class_bind_template_child (widget_class, GtkColorChooserWindow, cancel_button);
  gtk_widget_class_bind_template_callback (widget_class, propagate_notify);
  gtk_widget_class_bind_template_callback (widget_class, color_activated_cb);
}

static void
gtk_color_chooser_window_get_rgba (GtkColorChooser *chooser,
                                   GdkRGBA         *color)
{
  GtkColorChooserWindow *self  = GTK_COLOR_CHOOSER_WINDOW (chooser);

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->chooser), color);
}

static void
gtk_color_chooser_window_set_rgba (GtkColorChooser *chooser,
                                   const GdkRGBA   *color)
{
  GtkColorChooserWindow *self = GTK_COLOR_CHOOSER_WINDOW (chooser);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->chooser), color);
}

static void
gtk_color_chooser_window_add_palette (GtkColorChooser *chooser,
                                      GtkOrientation   orientation,
                                      int              colors_per_line,
                                      int              n_colors,
                                      GdkRGBA         *colors)
{
  GtkColorChooserWindow *self = GTK_COLOR_CHOOSER_WINDOW (chooser);

  gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (self->chooser),
                                 orientation, colors_per_line, n_colors, colors);
}

static void
gtk_color_chooser_window_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_chooser_window_get_rgba;
  iface->set_rgba = gtk_color_chooser_window_set_rgba;
  iface->add_palette = gtk_color_chooser_window_add_palette;
}

GtkWidget *
gtk_color_chooser_window_new (const char *title,
                              GtkWindow  *parent)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_WINDOW,
                       "title", title,
                       "transient-for", parent,
                       "modal", TRUE,
                       NULL);
}

void
gtk_color_chooser_window_save_color (GtkColorChooserWindow *self)
{
  GdkRGBA color;

  /* This causes the color chooser widget to save the
   * selected and custom colors to GSettings.
   */
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self), &color);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self), &color);
}

GtkWidget *
gtk_color_chooser_window_get_ok_button (GtkColorChooserWindow *self)
{
  return self->ok_button;
}

GtkWidget *
gtk_color_chooser_window_get_cancel_button (GtkColorChooserWindow *self)
{
  return self->cancel_button;
}

