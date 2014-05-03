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
#ifndef _PARASITE_PYTHON_SHELL_H_
#define _PARASITE_PYTHON_SHELL_H_

typedef struct _ParasitePythonShell         ParasitePythonShell;
typedef struct _ParasitePythonShellClass    ParasitePythonShellClass;
typedef struct _ParasitePythonShellPrivate  ParasitePythonShellPrivate;

#include <gtk/gtk.h>

#define PARASITE_TYPE_PYTHON_SHELL (parasite_python_shell_get_type())
#define PARASITE_PYTHON_SHELL(obj) \
		(G_TYPE_CHECK_INSTANCE_CAST((obj), PARASITE_TYPE_PYTHON_SHELL, ParasitePythonShell))
#define PARASITE_PYTHON_SHELL_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_CAST((klass), PARASITE_TYPE_PYTHON_SHELL, ParasitePythonShellClass))
#define PARASITE_IS_PYTHON_SHELL(obj) \
		(G_TYPE_CHECK_INSTANCE_TYPE((obj), PARASITE_TYPE_PYTHON_SHELL))
#define PARASITE_IS_PYTHON_SHELL_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_TYPE((klass), PARASITE_TYPE_PYTHON_SHELL))
#define PARASITE_PYTHON_SHELL_GET_CLASS(obj) \
		(G_TYPE_INSTANCE_GET_CLASS ((obj), PARASITE_TYPE_PYTHON_SHELL, ParasitePythonShellClass))


struct _ParasitePythonShell
{
  GtkBox parent_object;
  ParasitePythonShellPrivate *priv;
};

struct _ParasitePythonShellClass
{
 GtkBoxClass parent_class;
};

G_BEGIN_DECLS

GType parasite_python_shell_get_type(void);

GtkWidget *parasite_python_shell_new(void);
void parasite_python_shell_append_text(ParasitePythonShell *python_shell,
                                       const char *str,
                                       const char *tag);
void parasite_python_shell_focus(ParasitePythonShell *python_shell);

G_END_DECLS

#endif // _PARASITE_PYTHON_SHELL_H_
