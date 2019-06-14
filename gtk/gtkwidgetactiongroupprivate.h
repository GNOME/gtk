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

#ifndef __GTK_WIDGET_ACTION_GROUP_PRIVATE_H__
#define __GTK_WIDGET_ACTION_GROUP_PRIVATE_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_WIDGET_ACTION_GROUP           (gtk_widget_action_group_get_type ())
#define GTK_WIDGET_ACTION_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WIDGET_ACTION_GROUP, GtkWidgetActionGroup))
#define GTK_IS_WIDGET_ACTION_GROUP(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_WIDGET_ACTION_GROUP))

typedef struct _GtkWidgetActionGroup GtkWidgetActionGroup;

typedef struct {
  char *prefix;
  char *name;

  GtkWidgetActionActivate activate;
  GtkWidgetActionQuery    query;
  GtkWidgetActionChange   change;
} GtkWidgetAction;


GType           gtk_widget_action_group_get_type (void) G_GNUC_CONST;

GActionGroup *  gtk_widget_action_group_new      (GtkWidget  *widget,
                                                  const char *prefix,
                                                  GPtrArray  *actions);

G_END_DECLS

#endif /* __GTK_WIDGET_ACTION_GROUP_PRIVATE_H__ */
