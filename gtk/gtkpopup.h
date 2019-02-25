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

#ifndef __GTK_POPUP_H__
#define __GTK_POPUP_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_POPUP                 (gtk_popup_get_type ())
#define GTK_POPUP(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPUP, GtkPopup))
#define GTK_POPUP_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_POPUP, GtkPopupClass))
#define GTK_IS_POPUP(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPUP))
#define GTK_IS_POPUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_POPUP))
#define GTK_POPUP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_POPUP, GtkPopupClass))

typedef struct _GtkPopup       GtkPopup;
typedef struct _GtkPopupClass  GtkPopupClass;

struct _GtkPopup
{
  GtkBin parent;
};

struct _GtkPopupClass
{
  GtkBinClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType           gtk_popup_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_popup_new      (void);

GDK_AVAILABLE_IN_ALL
void            gtk_popup_set_relative_to (GtkPopup   *popup,
                                           GtkWidget  *relative_to);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_popup_get_popups (void);

G_END_DECLS

#endif /* __GTK_POPUP_H__ */
