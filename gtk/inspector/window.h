/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef _GTK_INSPECTOR_WINDOW_H_
#define _GTK_INSPECTOR_WINDOW_H_


#include <gtk/gtkwindow.h>

#define GTK_TYPE_INSPECTOR_WINDOW            (gtk_inspector_window_get_type())
#define GTK_INSPECTOR_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_WINDOW, GtkInspectorWindow))
#define GTK_INSPECTOR_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_WINDOW, GtkInspectorWindowClass))
#define GTK_INSPECTOR_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_WINDOW))
#define GTK_INSPECTOR_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_WINDOW))
#define GTK_INSPECTOR_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_WINDOW, GtkInspectorWindowClass))


#define TREE_TEXT_SCALE 0.8
#define TREE_CHECKBOX_SIZE (gint)(0.8 * 13)

typedef struct
{
  GtkWindow parent;

  GtkWidget *top_stack;
  GtkWidget *object_stack;
  GtkWidget *object_tree;
  GtkWidget *object_id;
  GtkWidget *object_details;
  GtkWidget *object_buttons;
  GtkWidget *object_details_button;
  GtkWidget *select_object;
  GtkWidget *prop_list;
  GtkWidget *child_prop_list;
  GtkWidget *selector;
  GtkWidget *signals_list;
  GtkWidget *style_prop_list;
  GtkWidget *classes_list;
  GtkWidget *widget_css_editor;
  GtkWidget *object_hierarchy;
  GtkWidget *size_groups;
  GtkWidget *data_list;
  GtkWidget *actions;
  GtkWidget *menu;
  GtkWidget *misc_info;
  GtkWidget *gestures;

  GtkWidget *invisible;
  GtkWidget *selected_widget;
  GtkWidget *flash_widget;

  gboolean grabbed;

  gint flash_count;
  gint flash_cnx;

} GtkInspectorWindow;

typedef struct
{
  GtkWindowClass parent;
} GtkInspectorWindowClass;


G_BEGIN_DECLS

GType      gtk_inspector_window_get_type    (void);
GtkWidget *gtk_inspector_window_new         (void);

void       gtk_inspector_flash_widget       (GtkInspectorWindow *iw,
                                             GtkWidget          *widget);
void       gtk_inspector_start_highlight    (GtkWidget          *widget);
void       gtk_inspector_stop_highlight     (GtkWidget          *widget);

void       gtk_inspector_on_inspect         (GtkWidget          *widget,
                                             GtkInspectorWindow *iw);

void       gtk_inspector_window_select_widget_under_pointer (GtkInspectorWindow *iw);

                                            

G_END_DECLS


#endif // _GTK_INSPECTOR_WINDOW_H_

// vim: set et sw=2 ts=2:
