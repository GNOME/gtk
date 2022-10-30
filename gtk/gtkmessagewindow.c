/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkmessagewindowprivate.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

#include <string.h>

struct _GtkMessageWindow
{
  GtkWindow parent;

  GtkBox *message_area;
  GtkLabel *message;
  GtkLabel *detail;
  GtkBox *buttons;
};

struct _GtkMessageWindowClass
{
  GtkWindowClass parent_class;
};

G_DEFINE_TYPE (GtkMessageWindow, gtk_message_window, GTK_TYPE_WINDOW)

static void
gtk_message_window_init (GtkMessageWindow *self)
{
  GtkSettings *settings;
  gboolean use_caret;

  gtk_widget_add_css_class (GTK_WIDGET (self), "message");

  gtk_widget_init_template (GTK_WIDGET (self));

  settings = gtk_widget_get_settings (GTK_WIDGET (self));
  g_object_get (settings, "gtk-keynav-use-caret", &use_caret, NULL);
  gtk_label_set_selectable (self->message, use_caret);
  gtk_label_set_selectable (self->detail, use_caret);
}

static void
gtk_message_window_class_init (GtkMessageWindowClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkmessagewindow.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkMessageWindow, message_area);
  gtk_widget_class_bind_template_child (widget_class, GtkMessageWindow, message);
  gtk_widget_class_bind_template_child (widget_class, GtkMessageWindow, detail);
  gtk_widget_class_bind_template_child (widget_class, GtkMessageWindow, buttons);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_DIALOG);
}

GtkMessageWindow *
gtk_message_window_new (void)
{
  return g_object_new (GTK_TYPE_MESSAGE_WINDOW, NULL);
}

void
gtk_message_window_set_message (GtkMessageWindow *self,
                                const char       *message)
{
  gtk_label_set_text (self->message, message);
}

void
gtk_message_window_set_detail (GtkMessageWindow *self,
                               const char       *detail)
{
  gtk_label_set_text (self->detail, detail);
  if (detail != NULL)
    {
      gtk_widget_show (GTK_WIDGET (self->detail));
      gtk_widget_add_css_class (GTK_WIDGET (self->message), "title");
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->detail));
      gtk_widget_remove_css_class (GTK_WIDGET (self->message), "title");
    }
}

void
gtk_message_window_add_button (GtkMessageWindow *self,
                               GtkWidget        *button)
{
  gtk_box_append (self->buttons, button);
}

#if 0
void
gtk_message_window_set_default_button (GtkMessageWindow *self,
                                       GtkButton        *button)
{
  gtk_window_set_default_widget (GTK_WINDOW (self), GTK_WIDGET (button));
}

void
gtk_message_window_set_cancel_button (GtkMessageWindow *self,
                                      GtkButton        *button)
{
  /* FIXME */
}
#endif

void
gtk_message_window_add_extra_widget (GtkMessageWindow *self,
                                     GtkWidget        *extra)
{
  gtk_box_append (self->message_area, extra);
}

