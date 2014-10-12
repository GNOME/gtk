/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#ifndef _GTK_INSPECTOR_SELECTOR_H_
#define _GTK_INSPECTOR_SELECTOR_H_

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_SELECTOR            (gtk_inspector_selector_get_type())
#define GTK_INSPECTOR_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_SELECTOR, GtkInspectorSelector))
#define GTK_INSPECTOR_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_SELECTOR, GtkInspectorSelectorClass))
#define GTK_INSPECTOR_IS_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_SELECTOR))
#define GTK_INSPECTOR_IS_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_SELECTOR))
#define GTK_INSPECTOR_SELECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_SELECTOR, GtkInspectorSelectorClass))


typedef struct _GtkInspectorSelectorPrivate GtkInspectorSelectorPrivate;

typedef struct _GtkInspectorSelector
{
  GtkBox parent;
  GtkInspectorSelectorPrivate *priv;
} GtkInspectorSelector;

typedef struct _GtkInspectorSelectorClass
{
  GtkBoxClass parent;
} GtkInspectorSelectorClass;

G_BEGIN_DECLS

GType      gtk_inspector_selector_get_type   (void);
void       gtk_inspector_selector_set_object (GtkInspectorSelector *oh,
                                              GObject              *object);

G_END_DECLS

#endif // _GTK_INSPECTOR_SELECTOR_H_

// vim: set et sw=2 ts=2:
