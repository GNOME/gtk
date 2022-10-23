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

typedef struct _GtkColorChooserWindowClass   GtkColorChooserWindowClass;

struct _GtkColorChooserWindow
{
  GtkWindow parent_instance;

  GtkWidget *chooser;
};

struct _GtkColorChooserWindowClass
{
  GtkWindowClass parent_class;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
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
                  GtkColorChooserWindow *cc)
{
  g_object_notify (G_OBJECT (cc), pspec->name);
}

static void
save_color (GtkColorChooserWindow *window)
{
  GdkRGBA color;

  /* This causes the color chooser widget to save the
   * selected and custom colors to GSettings.
   */
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (window), &color);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (window), &color);
}

enum
{
  RESPONSE_OK,
  RESPONSE_CANCEL
};

static void
response_cb (GtkWindow *window,
             int        response);

static void
color_activated_cb (GtkColorChooser *chooser,
                    GdkRGBA         *color,
                    GtkWindow       *window)
{
  save_color (GTK_COLOR_CHOOSER_WINDOW (window));
  response_cb (GTK_WINDOW (window), RESPONSE_OK);
}

static void
ok_button_cb (GtkButton *button,
              GtkWindow *window)
{
  response_cb (window, RESPONSE_OK);
}

static void
cancel_button_cb (GtkButton *button,
                  GtkWindow *window)
{
  response_cb (window, RESPONSE_CANCEL);
}

static void
gtk_color_chooser_window_init (GtkColorChooserWindow *cc)
{
  gtk_widget_init_template (GTK_WIDGET (cc));
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
  GtkColorChooserWindow *cc = GTK_COLOR_CHOOSER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc), &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_color_chooser_get_use_alpha (GTK_COLOR_CHOOSER (cc->chooser)));
      break;
    case PROP_SHOW_EDITOR:
      {
        gboolean show_editor;
        g_object_get (cc->chooser, "show-editor", &show_editor, NULL);
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
  GtkColorChooserWindow *cc = GTK_COLOR_CHOOSER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc), g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      if (gtk_color_chooser_get_use_alpha (GTK_COLOR_CHOOSER (cc->chooser)) != g_value_get_boolean (value))
        {
          gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (cc->chooser), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SHOW_EDITOR:
      g_object_set (cc->chooser,
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
  GtkColorChooserWindow *cc = GTK_COLOR_CHOOSER_WINDOW (object);

  g_clear_pointer (&cc->chooser, gtk_widget_unparent);

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

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtkcolorchooserwindow.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkColorChooserWindow, chooser);
  gtk_widget_class_bind_template_callback (widget_class, propagate_notify);
  gtk_widget_class_bind_template_callback (widget_class, color_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, ok_button_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_cb);
}

static void
gtk_color_chooser_window_get_rgba (GtkColorChooser *chooser,
                                   GdkRGBA         *color)
{
  GtkColorChooserWindow *cc = GTK_COLOR_CHOOSER_WINDOW (chooser);

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc->chooser), color);
}

static void
gtk_color_chooser_window_set_rgba (GtkColorChooser *chooser,
                                   const GdkRGBA   *color)
{
  GtkColorChooserWindow *cc = GTK_COLOR_CHOOSER_WINDOW (chooser);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->chooser), color);
}

static void
gtk_color_chooser_window_add_palette (GtkColorChooser *chooser,
                                      GtkOrientation   orientation,
                                      int              colors_per_line,
                                      int              n_colors,
                                      GdkRGBA         *colors)
{
  GtkColorChooserWindow *cc = GTK_COLOR_CHOOSER_WINDOW (chooser);

  gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (cc->chooser),
                                 orientation, colors_per_line, n_colors, colors);
}

static void
gtk_color_chooser_window_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_chooser_window_get_rgba;
  iface->set_rgba = gtk_color_chooser_window_set_rgba;
  iface->add_palette = gtk_color_chooser_window_add_palette;
}

/*
 * gtk_color_chooser_window_new:
 * @title: (nullable): Title of the window
 * @parent: (nullable): Transient parent of the window
 *
 * Creates a new `GtkColorChooserWindow`.
 *
 * Returns: a new `GtkColorChooserWindow`
 */
GtkWidget *
gtk_color_chooser_window_new (const char *title,
                              GtkWindow   *parent)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_WINDOW,
                       "title", title,
                       "transient-for", parent,
                       NULL);
}

static void
cancelled_cb (GCancellable *cancellable,
              GtkWindow    *window)
{
  response_cb (window, RESPONSE_CANCEL);
}

static void
response_cb (GtkWindow *window,
             int        response)
{
  GTask *task = G_TASK (g_object_get_data (G_OBJECT (window), "task"));
  GCancellable *cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, window);

  if (response == RESPONSE_OK)
    {
      save_color (GTK_COLOR_CHOOSER_WINDOW (window));
      g_task_return_boolean (task, TRUE);
    }
  else
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");

  g_object_unref (task);
  gtk_window_destroy (GTK_WINDOW (window));
}

/**
 * gtk_choose_color:
 * @parent: (nullable): parent window
 * @title: title for the color chooser
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): callback to call when the action is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function presents a color chooser to let the user
 * pick a color.
 *
 * The @callback will be called when the window is closed.
 * It should call [function@Gtk.choose_color_finish] to
 * find out whether the operation was completed successfully,
 * and to obtain the resulting color.
 */
void
gtk_choose_color (GtkWindow           *parent,
                  const char          *title,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
  gtk_choose_color_full (parent, title, NULL, NULL, cancellable, callback, user_data);
}

/**
 * gtk_choose_color_full:
 * @parent: (nullable): parent window
 * @title: title for the color chooser
 * @prepare: (nullable) (scope call): callback to set up the color chooser
 * @prepare_data: (closure prepare): data to pass to @prepare
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): callback to call when the action is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function presents a color chooser to let the user
 * pick a color.
 *
 * In addition to [function@Gtk.choose_color], this function takes
 * a @prepare callback that lets you set up the color chooser according
 * to your needs.
 *
 * The @callback will be called when the window is closed.
 * It should call [function@Gtk.choose_color_finish] to
 * find out whether the operation was completed successfully,
 * and to obtain the resulting color.
 */
void
gtk_choose_color_full (GtkWindow                      *parent,
                       const char                     *title,
                       GtkColorChooserPrepareCallback  prepare,
                       gpointer                        prepare_data,
                       GCancellable                   *cancellable,
                       GAsyncReadyCallback             callback,
                       gpointer                        user_data)
{
  GtkWidget *window;
  GTask *task;

  window = gtk_color_chooser_window_new (title, parent);
  if (prepare)
    prepare (GTK_COLOR_CHOOSER (window), prepare);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), window);

  task = g_task_new (window, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_choose_color_full);

  g_object_set_data (G_OBJECT (window), "task", task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_choose_color_finish:
 * @chooser: the `GtkColorChooser`
 * @result: `GAsyncResult` that was passed to @callback
 * @color: return location for the color
 * @error: return location for an error
 *
 * Finishes a gtk_choose_color() or gtk_choose_color_full() call
 * and returns the results.
 *
 * If this function returns `TRUE`, @color contains
 * the color that was chosen.
 *
 * Returns: `TRUE` if the operation was successful
 */
gboolean
gtk_choose_color_finish (GtkColorChooser  *chooser,
                         GAsyncResult     *result,
                         GdkRGBA          *color,
                         GError          **error)
{
  if (!g_task_propagate_boolean (G_TASK (result), error))
    return FALSE;

  gtk_color_chooser_get_rgba (chooser, color);

  return TRUE;
}
