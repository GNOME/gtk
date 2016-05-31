/* gtktab.h
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TAB_H__
#define __GTK_TAB_H__

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GTK_TYPE_TAB                 (gtk_tab_get_type ())
#define GTK_TAB(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TAB, GtkTab))
#define GTK_IS_TAB(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TAB))
#define GTK_TAB_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TAB, GtkTabClass))
#define GTK_IS_TAB_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TAB))
#define GTK_TAB_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TAB, GtkTabClass))

typedef struct _GtkTab      GtkTab;
typedef struct _GtkTabClass GtkTabClass;

struct _GtkTab
{
  GtkContainer parent;
};

struct _GtkTabClass
{
  GtkContainerClass parent_class;

  void (* activate) (GtkTab *tab);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
};

GType            gtk_tab_get_type   (void) G_GNUC_CONST;

const gchar     *gtk_tab_get_title  (GtkTab          *self);
void             gtk_tab_set_title  (GtkTab          *self,
                                     const gchar     *title);
GtkWidget       *gtk_tab_get_widget (GtkTab          *self);
void             gtk_tab_set_widget (GtkTab          *self,
                                     GtkWidget       *widget);
void             gtk_tab_set_child  (GtkTab          *self,
                                     GtkWidget       *child);

G_END_DECLS

#endif /* __GTK_TAB_H__ */
