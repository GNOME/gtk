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
#ifndef _GTK_INSPECTOR_PYTHON_SHELL_H_
#define _GTK_INSPECTOR_PYTHON_SHELL_H_

typedef struct _GtkInspectorPythonShell         GtkInspectorPythonShell;
typedef struct _GtkInspectorPythonShellClass    GtkInspectorPythonShellClass;
typedef struct _GtkInspectorPythonShellPrivate  GtkInspectorPythonShellPrivate;

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_PYTHON_SHELL (gtk_inspector_python_shell_get_type())
#define GTK_INSPECTOR_PYTHON_SHELL(obj) \
		(G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_PYTHON_SHELL, GtkInspectorPythonShell))
#define GTK_INSPECTOR_PYTHON_SHELL_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_PYTHON_SHELL, GtkInspectorPythonShellClass))
#define GTK_INSPECTOR_IS_PYTHON_SHELL(obj) \
		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_PYTHON_SHELL))
#define GTK_INSPECTOR_IS_PYTHON_SHELL_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_PYTHON_SHELL))
#define GTK_INSPECTOR_PYTHON_SHELL_GET_CLASS(obj) \
		(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_INSPECTOR_PYTHON_SHELL, GtkInspectorPythonShellClass))


struct _GtkInspectorPythonShell
{
  GtkBox parent_object;
  GtkInspectorPythonShellPrivate *priv;
};

struct _GtkInspectorPythonShellClass
{
 GtkBoxClass parent_class;
};

G_BEGIN_DECLS

GType gtk_inspector_python_shell_get_type(void);

GtkWidget *gtk_inspector_python_shell_new(void);
void gtk_inspector_python_shell_append_text(GtkInspectorPythonShell *python_shell,
                                       const char *str,
                                       const char *tag);
void gtk_inspector_python_shell_focus(GtkInspectorPythonShell *python_shell);

G_END_DECLS

#endif // _GTK_INSPECTOR_PYTHON_SHELL_H_
