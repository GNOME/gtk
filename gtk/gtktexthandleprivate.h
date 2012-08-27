/* GTK - The GIMP Toolkit
 * Copyright © 2012 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_TEXT_HANDLE_PRIVATE_H__
#define __GTK_TEXT_HANDLE_PRIVATE_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_TEXT_HANDLE           (_gtk_text_handle_get_type ())
#define GTK_TEXT_HANDLE(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_TEXT_HANDLE, GtkTextHandle))
#define GTK_TEXT_HANDLE_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), GTK_TYPE_TEXT_HANDLE, GtkTextHandleClass))
#define GTK_IS_TEXT_HANDLE(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_TEXT_HANDLE))
#define GTK_IS_TEXT_HANDLE_CLASS(o)    (G_TYPE_CHECK_CLASS_TYPE ((o), GTK_TYPE_TEXT_HANDLE))
#define GTK_TEXT_HANDLE_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_TEXT_HANDLE, GtkTextHandleClass))

typedef struct _GtkTextHandle GtkTextHandle;
typedef struct _GtkTextHandleClass GtkTextHandleClass;

typedef enum
{
  GTK_TEXT_HANDLE_POSITION_CURSOR,
  GTK_TEXT_HANDLE_POSITION_SELECTION_START,
  GTK_TEXT_HANDLE_POSITION_SELECTION_END = GTK_TEXT_HANDLE_POSITION_CURSOR
} GtkTextHandlePosition;

typedef enum
{
  GTK_TEXT_HANDLE_MODE_NONE,
  GTK_TEXT_HANDLE_MODE_CURSOR,
  GTK_TEXT_HANDLE_MODE_SELECTION
} GtkTextHandleMode;

struct _GtkTextHandle
{
  GObject parent_instance;
  gpointer priv;
};

struct _GtkTextHandleClass
{
  GObjectClass parent_class;

  void (* handle_dragged) (GtkTextHandle         *handle,
                           GtkTextHandlePosition  pos,
                           gint                   x,
                           gint                   y);
  void (* drag_finished)  (GtkTextHandle         *handle,
                           GtkTextHandlePosition  pos);
};

GType           _gtk_text_handle_get_type     (void) G_GNUC_CONST;

GtkTextHandle * _gtk_text_handle_new          (GtkWidget             *parent);

void            _gtk_text_handle_set_mode     (GtkTextHandle         *handle,
                                               GtkTextHandleMode      mode);
GtkTextHandleMode
                _gtk_text_handle_get_mode     (GtkTextHandle         *handle);
void            _gtk_text_handle_set_position (GtkTextHandle         *handle,
                                               GtkTextHandlePosition  pos,
                                               GdkRectangle          *rect);
void            _gtk_text_handle_set_visible  (GtkTextHandle         *handle,
                                               GtkTextHandlePosition  pos,
                                               gboolean               visible);

void            _gtk_text_handle_set_relative_to (GtkTextHandle *handle,
                                                  GdkWindow     *window);

gboolean        _gtk_text_handle_get_is_dragged (GtkTextHandle         *handle,
                                                 GtkTextHandlePosition  pos);

G_END_DECLS

#endif /* __GTK_TEXT_HANDLE_PRIVATE_H__ */
