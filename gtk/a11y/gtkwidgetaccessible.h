/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __GTK_WIDGET_ACCESSIBLE_H__
#define __GTK_WIDGET_ACCESSIBLE_H__

#if !defined (__GTK_A11Y_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk-a11y.h> can be included directly."
#endif

#include <gtk/gtkaccessible.h>

G_BEGIN_DECLS

#define GTK_TYPE_WIDGET_ACCESSIBLE (gtk_widget_accessible_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkWidgetAccessible, gtk_widget_accessible, GTK, WIDGET_ACCESSIBLE, GtkAccessible)

struct _GtkWidgetAccessibleClass
{
  GtkAccessibleClass parent_class;

  /*
   * Signal handler for notify signal on GTK widget
   */
  void (*notify_gtk)                   (GObject             *object,
                                        GParamSpec          *pspec);

};

G_END_DECLS

#endif /* __GTK_WIDGET_ACCESSIBLE_H__ */
