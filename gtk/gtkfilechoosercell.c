/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkfilechoosercellprivate.h"

#include "gtkprivate.h"
#include "gtkbinlayout.h"
#include "gtkgestureclick.h"
#include "gtkgesturelongpress.h"
#include "gtkfilechooserwidget.h"

struct _GtkFileChooserCell
{
  GtkWidget parent_instance;

  GFileInfo *item;
  gboolean selected;
  guint position;
};

struct _GtkFileChooserCellClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkFileChooserCell, gtk_file_chooser_cell, GTK_TYPE_WIDGET)

enum
{
  PROP_POSITION = 1,
  PROP_SELECTED,
  PROP_ITEM,
};

static void
popup_menu (GtkFileChooserCell *self,
            double              x,
            double              y)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *impl;
  double xx, yy;

  impl = gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_CHOOSER_WIDGET);

  gtk_widget_translate_coordinates (widget, GTK_WIDGET (impl),
                                    x, y, &xx, &yy);

  gtk_widget_activate_action (widget, "item.popup-file-list-menu",
                              "(udd)", self->position, xx, yy);
}

static void
file_chooser_cell_clicked (GtkEventController *controller,
                           int                 n_press,
                           double              x,
                           double              y)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (widget);

  popup_menu (self, x, y);
}

static void
file_chooser_cell_long_pressed (GtkEventController *controller,
                                double              x,
                                double              y)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (widget);

  popup_menu (self, x, y);
}

static void
gtk_file_chooser_cell_init (GtkFileChooserCell *self)
{
  GtkGesture *gesture;

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (file_chooser_cell_clicked), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_long_press_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (file_chooser_cell_long_pressed), NULL);
}

static void
gtk_file_chooser_cell_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_file_chooser_cell_parent_class)->dispose (object);
}

static void
gtk_file_chooser_cell_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      self->position = g_value_get_uint (value);
      break;

    case PROP_SELECTED:
      self->selected = g_value_get_boolean (value);
      break;

    case PROP_ITEM:
      self->item = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_cell_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      g_value_set_uint (value, self->position);
      break;

    case PROP_SELECTED:
      g_value_set_boolean (value, self->selected);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_file_chooser_cell_class_init (GtkFileChooserCellClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_file_chooser_cell_dispose;
  object_class->set_property = gtk_file_chooser_cell_set_property;
  object_class->get_property = gtk_file_chooser_cell_get_property;

  g_object_class_install_property (object_class, PROP_POSITION,
                                   g_param_spec_uint ("position", NULL, NULL,
                                                      0, G_MAXUINT, 0,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_SELECTED,
                                   g_param_spec_boolean ("selected", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_ITEM,
                                   g_param_spec_object ("item", NULL, NULL,
                                                        G_TYPE_FILE_INFO,
                                                        GTK_PARAM_READWRITE));

  gtk_widget_class_set_css_name (widget_class, I_("filelistcell"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

GtkFileChooserCell *
gtk_file_chooser_cell_new (void)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_CELL, NULL);
}
