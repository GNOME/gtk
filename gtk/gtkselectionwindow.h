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

#ifndef __GTK_SELECTION_WINDOW_H__
#define __GTK_SELECTION_WINDOW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbubblewindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_SELECTION_WINDOW           (gtk_selection_window_get_type ())
#define GTK_SELECTION_WINDOW(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_SELECTION_WINDOW, GtkSelectionWindow))
#define GTK_SELECTION_WINDOW_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), GTK_TYPE_SELECTION_WINDOW, GtkSelectionWindowClass))
#define GTK_IS_SELECTION_WINDOW(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_SELECTION_WINDOW))
#define GTK_IS_SELECTION_WINDOW_CLASS(o)    (G_TYPE_CHECK_CLASS_TYPE ((o), GTK_TYPE_SELECTION_WINDOW))
#define GTK_SELECTION_WINDOW_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_SELECTION_WINDOW, GtkSelectionWindowClass))

typedef struct _GtkSelectionWindow GtkSelectionWindow;
typedef struct _GtkSelectionWindowClass GtkSelectionWindowClass;
typedef struct _GtkSelectionWindowPrivate GtkSelectionWindowPrivate;

struct _GtkSelectionWindow
{
  GtkBubbleWindow parent_instance;

  /*< private >*/
  gpointer _priv;
};

struct _GtkSelectionWindowClass
{
  GtkBubbleWindowClass parent_class;

  void (* cut)   (GtkSelectionWindow *window);
  void (* copy)  (GtkSelectionWindow *window);
  void (* paste) (GtkSelectionWindow *window);
};

GType       gtk_selection_window_get_type (void) G_GNUC_CONST;

GtkWidget * gtk_selection_window_new      (void);

void        gtk_selection_window_set_editable      (GtkSelectionWindow *window,
                                                    gboolean            editable);
gboolean    gtk_selection_window_get_editable      (GtkSelectionWindow *window);
void        gtk_selection_window_set_has_selection (GtkSelectionWindow *window,
                                                    gboolean            has_selection);
gboolean    gtk_selection_window_get_has_selection (GtkSelectionWindow *window);

GtkWidget * gtk_selection_window_get_toolbar (GtkSelectionWindow *window);


G_END_DECLS

#endif /* __GTK_SELECTION_POPUP_H__ */
