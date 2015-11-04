/* gtkpathbarboxprivate.h
 *
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __GTK_PATH_BAR_BOX_PRIVATE_H__
#define __GTK_PATH_BAR_BOX_PRIVATE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include "gtkwidget.h"
#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GTK_TYPE_PATH_BAR_BOX            (gtk_path_bar_box_get_type())
#define GTK_PATH_BAR_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATH_BAR_BOX, GtkPathBarBox))
#define GTK_PATH_BAR_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATH_BAR_BOX, GtkPathBarBoxClass))
#define GTK_IS_PATH_BAR_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATH_BAR_BOX))
#define GTK_IS_PATH_BAR_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATH_BAR_BOX)
#define GTK_PATH_BAR_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATH_BAR_BOX, GtkPathBarBoxClass))

typedef struct _GtkPathBarBox GtkPathBarBox;
typedef struct _GtkPathBarBoxClass GtkPathBarBoxClass;
typedef struct _GtkPathBarBoxPrivate GtkPathBarBoxPrivate;

struct _GtkPathBarBoxClass
{
  GtkContainerClass parent_class;

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkPathBarBox
{
  GtkContainer parent_instance;
};

GType             gtk_path_bar_box_get_type                (void) G_GNUC_CONST;

GtkWidget        *gtk_path_bar_box_new                     (void);

G_END_DECLS

#endif /* GTK_PATH_BAR_BOX_PRIVATE_H_ */
