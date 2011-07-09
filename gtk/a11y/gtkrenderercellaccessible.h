/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_RENDERER_CELL_ACCESSIBLE_H__
#define __GTK_RENDERER_CELL_ACCESSIBLE_H__

#include <atk/atk.h>
#include "gtkcellaccessible.h"

G_BEGIN_DECLS

#define GTK_TYPE_RENDERER_CELL_ACCESSIBLE            (_gtk_renderer_cell_accessible_get_type ())
#define GTK_RENDERER_CELL_ACCESSIBLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_RENDERER_CELL_ACCESSIBLE, GtkRendererCellAccessible))
#define GTK_RENDERER_CELL_ACCESSIBLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_RENDERER_CELL_ACCESSIBLE, GtkRendererCellAccessibleClass))
#define GTK_IS_RENDERER_CELL_ACCESSIBLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_RENDERER_CELL_ACCESSIBLE))
#define GTK_IS_RENDERER_CELL_ACCESSIBLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_RENDERER_CELL_ACCESSIBLE))
#define GTK_RENDERER_CELL_ACCESSIBLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RENDERER_CELL_ACCESSIBLE, GtkRendererCellAccessibleClass))

typedef struct _GtkRendererCellAccessible      GtkRendererCellAccessible;
typedef struct _GtkRendererCellAccessibleClass GtkRendererCellAccessibleClass;

struct _GtkRendererCellAccessible
{
  GtkCellAccessible  parent;
  GtkCellRenderer   *renderer;
};

struct _GtkRendererCellAccessibleClass
{
  GtkCellAccessibleClass parent_class;
  gchar **property_list;
  gboolean (*update_cache) (GtkRendererCellAccessible *cell,
                            gboolean                   emit_change_signal);
};

GType      _gtk_renderer_cell_accessible_get_type     (void);

AtkObject *_gtk_renderer_cell_accessible_new          (void);
gboolean   _gtk_renderer_cell_accessible_update_cache (GtkRendererCellAccessible *cell,
                                                       gboolean          emit_change_signal);

G_END_DECLS

#endif /* __GTK_RENDERER_CELL_ACCESSIBLE_H__ */
