/* gtkpathbar.h
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
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

#ifndef __GTK_PATH_BAR__
#define __GTK_PATH_BAR__

#include "gtkcontainer.h"

G_BEGIN_DECLS

typedef struct _GtkPathBar      GtkPathBar;
typedef struct _GtkPathBarClass GtkPathBarClass;


#define GTK_TYPE_PATH_BAR                 (gtk_path_bar_get_type ())
#define GTK_PATH_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATH_BAR, GtkPathBar))
#define GTK_PATH_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATH_BAR, GtkPathBarClass))
#define GTK_IS_PATH_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATH_BAR))
#define GTK_IS_PATH_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATH_BAR))
#define GTK_PATH_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATH_BAR, GtkPathBarClass))

struct _GtkPathBar
{
  GtkContainer parent;

  const char *path;
  GList *button_list;
  gint16 slider_width;
  gint16 spacing;
  gint16 button_offset;
  guint slider_visible : 1;
  GtkWidget *up_slider_button;
  GtkWidget *down_slider_button;
};

struct _GtkPathBarClass
{
  GtkContainerClass parent_class;

  void (* path_clicked) (GtkPathBar *path_bar);
};

GType gtk_path_bar_get_type (void) G_GNUC_CONST;
void  gtk_path_bar_set_path (GtkPathBar  *path_bar,
			     const gchar *path);

G_END_DECLS

#endif /* __GTK_PATH_BAR__ */
