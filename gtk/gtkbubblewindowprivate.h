/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_BUBBLE_WINDOW_H__
#define __GTK_BUBBLE_WINDOW_H__

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_BUBBLE_WINDOW           (_gtk_bubble_window_get_type ())
#define GTK_BUBBLE_WINDOW(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_BUBBLE_WINDOW, GtkBubbleWindow))
#define GTK_BUBBLE_WINDOW_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), GTK_TYPE_BUBBLE_WINDOW, GtkBubbleWindowClass))
#define GTK_IS_BUBBLE_WINDOW(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_BUBBLE_WINDOW))
#define GTK_IS_BUBBLE_WINDOW_CLASS(o)    (G_TYPE_CHECK_CLASS_TYPE ((o), GTK_TYPE_BUBBLE_WINDOW))
#define GTK_BUBBLE_WINDOW_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_BUBBLE_WINDOW, GtkBubbleWindowClass))

typedef struct _GtkBubbleWindow GtkBubbleWindow;
typedef struct _GtkBubbleWindowClass GtkBubbleWindowClass;

struct _GtkBubbleWindow
{
  GtkWindow parent_instance;

  /*< private >*/
  gpointer priv;
};

struct _GtkBubbleWindowClass
{
  GtkWindowClass parent_class;
};

GType       _gtk_bubble_window_get_type        (void) G_GNUC_CONST;

GtkWidget * _gtk_bubble_window_new             (void);

void        _gtk_bubble_window_set_relative_to (GtkBubbleWindow *window,
                                                GdkWindow       *relative_to);
GdkWindow * _gtk_bubble_window_get_relative_to (GtkBubbleWindow *window);

void        _gtk_bubble_window_set_pointing_to (GtkBubbleWindow       *window,
                                                cairo_rectangle_int_t *rect);
gboolean    _gtk_bubble_window_get_pointing_to (GtkBubbleWindow       *window,
                                                cairo_rectangle_int_t *rect);
void        _gtk_bubble_window_set_position    (GtkBubbleWindow       *window,
                                                GtkPositionType        position);

GtkPositionType
            _gtk_bubble_window_get_position    (GtkBubbleWindow       *window);

void        _gtk_bubble_window_popup           (GtkBubbleWindow       *window,
                                                GdkWindow             *relative_to,
                                                cairo_rectangle_int_t *pointing_to,
                                                GtkPositionType        position);

void        _gtk_bubble_window_popdown         (GtkBubbleWindow       *window);

gboolean    _gtk_bubble_window_grab            (GtkBubbleWindow       *window,
                                                GdkDevice             *device,
                                                guint32                activate_time);

void        _gtk_bubble_window_ungrab          (GtkBubbleWindow       *window);

G_END_DECLS

#endif /* __GTK_BUBBLE_WINDOW_H__ */
