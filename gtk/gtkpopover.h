/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#ifndef __GTK_POPOVER_H__
#define __GTK_POPOVER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_POPOVER                 (gtk_popover_get_type ())
#define GTK_POPOVER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPOVER, GtkPopover))
#define GTK_POPOVER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_POPOVER, GtkPopoverClass))
#define GTK_IS_POPOVER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPOVER))
#define GTK_IS_POPOVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_POPOVER))
#define GTK_POPOVER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_POPOVER, GtkPopoverClass))

typedef struct _GtkPopover       GtkPopover;
typedef struct _GtkPopoverClass  GtkPopoverClass;

struct _GtkPopover
{
  GtkBin parent;
};

struct _GtkPopoverClass
{
  GtkBinClass parent_class;

  void (* closed)           (GtkPopover *popover);
  void (* activate_default) (GtkPopover *popover);

  /*< private >*/

  gpointer reserved[8];
};

GDK_AVAILABLE_IN_ALL
GType           gtk_popover_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_popover_new             (GtkWidget    *relative_to);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_set_relative_to (GtkPopover   *popover,
                                             GtkWidget    *relative_to);
GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_popover_get_relative_to (GtkPopover   *popover);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_set_pointing_to (GtkPopover         *popover,
                                             const GdkRectangle *rect);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_popover_get_pointing_to (GtkPopover         *popover,
                                             GdkRectangle       *rect);
GDK_AVAILABLE_IN_ALL
void            gtk_popover_set_position    (GtkPopover         *popover,
                                             GtkPositionType     position);
GDK_AVAILABLE_IN_ALL
GtkPositionType gtk_popover_get_position    (GtkPopover         *popover);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_set_autohide    (GtkPopover         *popover,
                                             gboolean            autohide);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_popover_get_autohide    (GtkPopover         *popover);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_set_has_arrow   (GtkPopover         *popover,
                                             gboolean            has_arrow);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_popover_get_has_arrow   (GtkPopover         *popover);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_popup (GtkPopover *popover);
GDK_AVAILABLE_IN_ALL
void            gtk_popover_popdown (GtkPopover *popover);

GDK_AVAILABLE_IN_ALL
void gtk_popover_set_default_widget (GtkPopover *popover,
                                     GtkWidget  *widget);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_popover_get_popovers (void);

G_END_DECLS

#endif /* __GTK_POPOVER_H__ */
