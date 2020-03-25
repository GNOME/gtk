/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Red Hat, Inc.
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
#include "gtkwidgetprivate.h"
#include "gtkpopoverholder.h"
#include "gtknative.h"


struct _GtkPopoverHolder
{
  GtkWidget parent_instance;
  GtkWidget *child;
  GtkPopover *popover;
};

struct _GtkPopoverHolderClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkPopoverHolder, gtk_popover_holder, GTK_TYPE_WIDGET)

static void
gtk_popover_holder_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkPopoverHolder *self = GTK_POPOVER_HOLDER (widget);

  if (self->child && _gtk_widget_get_visible (self->child))
    gtk_widget_size_allocate (self->child,
                              &(GtkAllocation) {
                                0, 0,
                                width, height
                              }, baseline);

  if (self->popover)
    gtk_native_check_resize (GTK_NATIVE (self->popover));
}

static void
gtk_popover_holder_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  GtkPopoverHolder *self = GTK_POPOVER_HOLDER (widget);

  if (self->child != NULL && _gtk_widget_get_visible (self->child))
    gtk_widget_measure (self->child,
                        orientation,
                        for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
  else
    {
      *minimum = 0;
      *natural = 0;
    }
}

static void
gtk_popover_holder_dispose (GObject *object)
{
  GtkPopoverHolder *self = GTK_POPOVER_HOLDER (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  if (self->popover)
    {
      gtk_popover_set_relative_to (self->popover, NULL);
      self->popover = NULL;
    }

  G_OBJECT_CLASS (gtk_popover_holder_parent_class)->dispose (object);
}

static void
gtk_popover_holder_class_init (GtkPopoverHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_popover_holder_dispose;

  widget_class->measure = gtk_popover_holder_measure;
  widget_class->size_allocate = gtk_popover_holder_size_allocate;
}

static void
gtk_popover_holder_init (GtkPopoverHolder *self)
{
}

GtkWidget *
gtk_popover_holder_new (void)
{
  return g_object_new (GTK_TYPE_POPOVER_HOLDER, NULL);
}

GtkWidget *
gtk_popover_holder_get_child (GtkPopoverHolder *self)
{
  g_return_val_if_fail (GTK_IS_POPOVER_HOLDER (self), NULL);

  return self->child;
}

void
gtk_popover_holder_set_child (GtkPopoverHolder *self,
                              GtkWidget        *child)
{
  g_return_if_fail (GTK_IS_POPOVER_HOLDER (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (self->child)
    gtk_widget_unparent (self->child);

  self->child = child;

  if (self->child)
    gtk_widget_set_parent (self->child, GTK_WIDGET (self));
}

GtkPopover *
gtk_popover_holder_get_popover (GtkPopoverHolder *self)
{
  g_return_val_if_fail (GTK_IS_POPOVER_HOLDER (self), NULL);

  return self->popover;
}

void
gtk_popover_holder_set_popover (GtkPopoverHolder *self,
                                GtkPopover       *popover)
{
  g_return_if_fail (GTK_IS_POPOVER_HOLDER (self));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (self->popover)
    {
      if (gtk_widget_get_visible (GTK_WIDGET (self->popover)))
        gtk_widget_hide (GTK_WIDGET (self->popover));

      gtk_popover_set_relative_to (self->popover, NULL);
    }

  self->popover = popover;

  if (self->popover)
    {
      gtk_popover_set_relative_to (self->popover, GTK_WIDGET (self));
    }
}
