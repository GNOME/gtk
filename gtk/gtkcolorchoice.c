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

#include "gtkcolorchoice.h"
#include "gtkcolorchooserwindowprivate.h"
#include "gtkcolorchooser.h"
#include "gtkbutton.h"

/* {{{ GObject implementation */

struct _GtkColorChoice
{
  GObject parent_instance;

  GtkWindow *parent;
  char *title;
  gboolean use_alpha;

  GTask *task;
  GtkColorChooserWindow *window;
};

G_DEFINE_TYPE (GtkColorChoice, gtk_color_choice, G_TYPE_OBJECT)

static void
gtk_color_choice_init (GtkColorChoice *self)
{
}

static void
gtk_color_choice_finalize (GObject *object)
{
  GtkColorChoice *self = GTK_COLOR_CHOICE (object);

  g_assert (self->task == NULL);
  g_assert (self->window == NULL);

  g_free (self->title);

  G_OBJECT_CLASS (gtk_color_choice_parent_class)->finalize (object);
}

static void
gtk_color_choice_class_init (GtkColorChoiceClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gtk_color_choice_finalize;
}

/* }}} */
/* {{{ Public API */
/* {{{ Constructor */

GtkColorChoice *
gtk_color_choice_new (GtkWindow  *parent,
                      const char *title,
                      gboolean    use_alpha)
{
  GtkColorChoice *self;

  self = g_object_new (GTK_TYPE_COLOR_CHOICE, NULL);

  self->parent = parent;
  self->title = g_strdup (title);
  self->use_alpha = use_alpha;

  return self;
}

/* }}} */
/* {{{ Async API */

enum
{
  RESPONSE_OK,
  RESPONSE_CANCEL
};

static void response_cb (GtkColorChoice *self,
                         int             response);

static void
cancelled_cb (GCancellable   *cancellable,
              GtkColorChoice *self)
{
  response_cb (self, RESPONSE_CANCEL);
}

static void
response_cb (GtkColorChoice *self,
             int             response)
{
  GCancellable *cancellable;

  g_assert (self->window != NULL);
  g_assert (self->task != NULL);

  cancellable = g_task_get_cancellable (self->task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, self);

  if (response == RESPONSE_OK)
    {
      GdkRGBA color;

      gtk_color_chooser_window_save_color (GTK_COLOR_CHOOSER_WINDOW (self->window));

      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->window), &color);
      g_task_return_pointer (self->task, gdk_rgba_copy (&color), (GDestroyNotify) gdk_rgba_free);
    }
  else
    g_task_return_new_error (self->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");

  g_clear_pointer ((GtkWindow **)&self->window, gtk_window_destroy);
  g_clear_object (&self->task);
}

static void
ok_button_clicked (GtkButton      *button,
                   GtkColorChoice *self)
{
  response_cb (self, RESPONSE_OK);
}

static void
cancel_button_clicked (GtkButton      *button,
                       GtkColorChoice *self)
{
  response_cb (self, RESPONSE_CANCEL);
}

void
gtk_color_choice_choose (GtkColorChoice       *self,
                         const GdkRGBA        *initial_color,
                         GCancellable         *cancellable,
                         GAsyncReadyCallback   callback,
                         gpointer              user_data)
{
  GtkColorChooserWindow *window;
  GTask *task;

  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));

  window = GTK_COLOR_CHOOSER_WINDOW (gtk_color_chooser_window_new (self->title, self->parent));
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (window), self->use_alpha);
  if (initial_color)
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (window), initial_color);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), self);

  g_signal_connect (gtk_color_chooser_window_get_ok_button (window), "clicked",
                    G_CALLBACK (ok_button_clicked), self);
  g_signal_connect (gtk_color_chooser_window_get_cancel_button (window), "clicked",
                    G_CALLBACK (cancel_button_clicked), self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_color_choice_choose);

  self->window = window;
  self->task = task;

  gtk_window_present (GTK_WINDOW (window));
}

GdkRGBA *
gtk_color_choice_choose_finish (GtkColorChoice  *self,
                                GAsyncResult    *result,
                                GError         **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

/* }}} */
/* }}} */

/* vim:set foldmethod=marker expandtab: */
