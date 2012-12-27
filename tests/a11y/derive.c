/*
 * Copyright (C) 2012 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
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

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>

/* Implement a (trivial) GtkButton subclass, derive GtkButtonAccessible
 * and use the derived accessible for our new button.
 */

typedef GtkButtonAccessible MyButtonAccessible;
typedef GtkButtonAccessibleClass MyButtonAccessibleClass;

G_DEFINE_TYPE (MyButtonAccessible, my_button_accessible, GTK_TYPE_BUTTON_ACCESSIBLE)

static void
my_button_accessible_init (MyButtonAccessible *a)
{
}

static void
my_button_accessible_class_init (MyButtonAccessibleClass *class)
{
}

typedef GtkButton MyButton;
typedef GtkButtonClass MyButtonClass;

G_DEFINE_TYPE (MyButton, my_button, GTK_TYPE_BUTTON)

static void
my_button_init (MyButton *b)
{
}

static void
my_button_class_init (MyButtonClass *class)
{
  gtk_widget_class_set_accessible_type (GTK_WIDGET_CLASS (class),
                                        my_button_accessible_get_type ());
}

int main (int argc, char *argv[])
{
  GtkWidget *widget;
  GtkAccessible *accessible;

  gtk_init (NULL, NULL);

  widget = GTK_WIDGET (g_object_new (my_button_get_type (), NULL));
  accessible = GTK_ACCESSIBLE (gtk_widget_get_accessible (widget));

  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (accessible, my_button_accessible_get_type ()));

  return 0;
}
