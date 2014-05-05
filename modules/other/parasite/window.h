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
#ifndef _PARASITE_WINDOW_H_
#define _PARASITE_WINDOW_H_


#include <gtk/gtk.h>

#define PARASITE_TYPE_WINDOW            (parasite_window_get_type())
#define PARASITE_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PARASITE_TYPE_WINDOW, ParasiteWindow))
#define PARASITE_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PARASITE_TYPE_WINDOW, ParasiteWindowClass))
#define PARASITE_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PARASITE_TYPE_WINDOW))
#define PARASITE_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PARASITE_TYPE_WINDOW))
#define PARASITE_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PARASITE_TYPE_WINDOW, ParasiteWindowClass))


#define TREE_TEXT_SCALE 0.8
#define TREE_CHECKBOX_SIZE (gint)(0.8 * 13)

typedef struct
{
  GtkWindow parent;

  GtkWidget *widget_tree;
  GtkWidget *prop_list;
  GtkWidget *child_prop_list;
  GtkWidget *python_shell;
  GtkWidget *button_path;
  GtkWidget *classes_list;
  GtkWidget *widget_css_editor;
  GtkWidget *object_hierarchy;

  GtkWidget *widget_popup;

  GtkWidget *selected_widget;
  GtkWidget *flash_widget;

  gint flash_count;
  gint flash_cnx;

} ParasiteWindow;

typedef struct
{
  GtkWindowClass parent;
} ParasiteWindowClass;


G_BEGIN_DECLS

GType      parasite_window_get_type (void);
GtkWidget *parasite_window_new      (void);

void       gtkparasite_flash_widget       (ParasiteWindow *parasite,
                                           GtkWidget      *widget);

GtkWidget *gtkparasite_inspect_button_new (ParasiteWindow *parasite);

G_END_DECLS

#endif // _PARASITE_WINDOW_H_

// vim: set et sw=2 ts=2:
