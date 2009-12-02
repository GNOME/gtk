/* GTK - The GIMP Toolkit
 * Copyright (C) 2007 Openismus GmbH
 *
 * Author:
 *      Mathias Hasselmann <mathias@openismus.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_EXTENDED_LAYOUT_H__
#define __GTK_EXTENDED_LAYOUT_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_EXTENDED_LAYOUT            (gtk_extended_layout_get_type ())
#define GTK_EXTENDED_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EXTENDED_LAYOUT, GtkExtendedLayout))
#define GTK_EXTENDED_LAYOUT_CLASS(klass)    ((GtkExtendedLayoutIface*)g_type_interface_peek ((klass), GTK_TYPE_EXTENDED_LAYOUT))
#define GTK_IS_EXTENDED_LAYOUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EXTENDED_LAYOUT))
#define GTK_EXTENDED_LAYOUT_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_EXTENDED_LAYOUT, GtkExtendedLayoutIface))

typedef struct _GtkExtendedLayout           GtkExtendedLayout;
typedef struct _GtkExtendedLayoutIface      GtkExtendedLayoutIface;

struct _GtkExtendedLayoutIface
{
  GTypeInterface g_iface;

  /* virtual table */

  void (*get_desired_size)     (GtkExtendedLayout  *layout,
                                GtkRequisition     *minimum_size,
                                GtkRequisition     *natural_size);
  void (*get_width_for_height) (GtkExtendedLayout  *layout,
                                gint                height,
                                gint               *minimum_width,
                                gint               *natural_width);
  void (*get_height_for_width) (GtkExtendedLayout  *layout,
                                gint                width,
                                gint               *minimum_height,
                                gint               *natural_height);
};

GType gtk_extended_layout_get_type             (void) G_GNUC_CONST;

void  gtk_extended_layout_get_desired_size     (GtkExtendedLayout *layout,
                                                GtkRequisition    *minimum_size,
                                                GtkRequisition    *natural_size);
void  gtk_extended_layout_get_width_for_height (GtkExtendedLayout *layout,
                                                gint               height,
                                                gint              *minimum_width,
                                                gint              *natural_width);
void  gtk_extended_layout_get_height_for_width (GtkExtendedLayout *layout,
                                                gint               width,
                                                gint              *minimum_height,
                                                gint              *natural_height);

G_END_DECLS

#endif /* __GTK_EXTENDED_LAYOUT_H__ */
