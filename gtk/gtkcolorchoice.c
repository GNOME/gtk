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
  GdkRGBA color;
  gboolean use_alpha;

  GTask *task;
  GtkColorChooserWindow *window;
};

enum
{
  PROP_PARENT = 1,
  PROP_TITLE,
  PROP_COLOR,
  PROP_USE_ALPHA,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkColorChoice, gtk_color_choice, G_TYPE_OBJECT)

static void
gtk_color_choice_init (GtkColorChoice *self)
{
  self->title = g_strdup ("");
  self->use_alpha = TRUE;
}

static void
gtk_color_choice_finalize (GObject *object)
{
  GtkColorChoice *self = GTK_COLOR_CHOICE (object);

  g_free (self->title);

  G_OBJECT_CLASS (gtk_color_choice_parent_class)->finalize (object);
}

static void
gtk_color_choice_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkColorChoice *self = GTK_COLOR_CHOICE (object);

  switch (property_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, self->parent);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
      break;

    case PROP_USE_ALPHA:
      g_value_set_boolean (value, self->use_alpha);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_color_choice_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorChoice *self = GTK_COLOR_CHOICE (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      gtk_color_choice_set_parent (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      gtk_color_choice_set_title (self, g_value_get_string (value));
      break;

    case PROP_COLOR:
      gtk_color_choice_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_USE_ALPHA:
      gtk_color_choice_set_use_alpha (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_choice_class_init (GtkColorChoiceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_color_choice_finalize;
  object_class->get_property = gtk_color_choice_get_property;
  object_class->set_property = gtk_color_choice_set_property;

  properties[PROP_PARENT] =
      g_param_spec_object ("parent", NULL, NULL,
                           GTK_TYPE_WINDOW,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           "",
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_COLOR] =
      g_param_spec_boxed ("color", NULL, NULL,
                          GDK_TYPE_RGBA,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_USE_ALPHA] =
      g_param_spec_boolean ("use-alpha", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Public API */
/* {{{ Constructor */

GtkColorChoice *
gtk_color_choice_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_CHOICE, NULL);
}

/* }}} */
/* {{{ Setters and getters */

GtkWindow *
gtk_color_choice_get_parent (GtkColorChoice *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_CHOICE (self), NULL);

  return self->parent;
}

void
gtk_color_choice_set_parent (GtkColorChoice *self,
                             GtkWindow      *parent)
{
  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  if (self->parent == parent)
    return;

  if (self->parent)
    g_object_remove_weak_pointer (G_OBJECT (self->parent), (void **)&self->parent);

  self->parent = parent;

  if (self->parent)
    g_object_add_weak_pointer (G_OBJECT (self->parent), (void **)&self->parent);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PARENT]);
}

const char *
gtk_color_choice_get_title (GtkColorChoice *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_CHOICE (self), NULL);

  return self->title;
}

void
gtk_color_choice_set_title (GtkColorChoice *self,
                            const char     *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));
  g_return_if_fail (title != NULL);

  if (g_str_equal (self->title, title))
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

void
gtk_color_choice_get_color (GtkColorChoice *self,
                            GdkRGBA        *color)
{
  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));
  g_return_if_fail (color != NULL);

  *color = self->color;
}

void
gtk_color_choice_set_color (GtkColorChoice *self,
                            GdkRGBA        *color)
{
  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));
  g_return_if_fail (color != NULL);

  if (gdk_rgba_equal (&self->color, &color))
    return;

  self->color = *color;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

gboolean
gtk_color_choice_get_use_alpha (GtkColorChoice *self)
{
  g_return_val_if_fail (GTK_IS_COLOR_CHOICE (self), TRUE);

  return self->use_alpha;
}

void
gtk_color_choice_set_use_alpha (GtkColorChoice *self,
                                gboolean        use_alpha)
{
  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));

  if (self->use_alpha == use_alpha)
    return;

  self->use_alpha = use_alpha;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_ALPHA]);
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
gtk_color_choice_present (GtkColorChoice       *self,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data)
{
  GtkColorChooserWindow *window;
  GTask *task;

  g_return_if_fail (GTK_IS_COLOR_CHOICE (self));
  g_return_if_fail (self->task == NULL);

  window = GTK_COLOR_CHOOSER_WINDOW (gtk_color_chooser_window_new (self->title, self->parent));
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (window), &self->color);
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (window), self->use_alpha);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), self);

  g_signal_connect (gtk_color_chooser_window_get_ok_button (window), "clicked",
                    G_CALLBACK (ok_button_clicked), self);
  g_signal_connect (gtk_color_chooser_window_get_cancel_button (window), "clicked",
                    G_CALLBACK (cancel_button_clicked), self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_color_choice_present);

  self->window = window;
  self->task = task;

  gtk_window_present (GTK_WINDOW (window));
}

GdkRGBA *
gtk_color_choice_present_finish (GtkColorChoice  *self,
                                 GAsyncResult    *result,
                                 GError         **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

/* }}} */
/* }}} */

/* vim:set foldmethod=marker expandtab: */
