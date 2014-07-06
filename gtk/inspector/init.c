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
#include <glib.h>

#include "actions.h"
#include "button-path.h"
#include "classes-list.h"
#include "css-editor.h"
#include "data-list.h"
#include "general.h"
#include "gestures.h"
#include "menu.h"
#include "object-hierarchy.h"
#include "prop-list.h"
#include "python-hooks.h"
#include "python-shell.h"
#include "resource-list.h"
#include "resources.h"
#include "signals-list.h"
#include "size-groups.h"
#include "style-prop-list.h"
#include "visual.h"
#include "widget-tree.h"
#include "window.h"

void
gtk_inspector_init (void)
{
#ifdef ENABLE_PYTHON
  gtk_inspector_python_init ();
#endif

  gtk_inspector_register_resource ();

  g_type_ensure (GTK_TYPE_INSPECTOR_ACTIONS);
  g_type_ensure (GTK_TYPE_INSPECTOR_BUTTON_PATH);
  g_type_ensure (GTK_TYPE_INSPECTOR_CLASSES_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_CSS_EDITOR);
  g_type_ensure (GTK_TYPE_INSPECTOR_DATA_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_GENERAL);
  g_type_ensure (GTK_TYPE_INSPECTOR_GESTURES);
  g_type_ensure (GTK_TYPE_INSPECTOR_MENU);
  g_type_ensure (GTK_TYPE_INSPECTOR_OBJECT_HIERARCHY);
  g_type_ensure (GTK_TYPE_INSPECTOR_PROP_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_PYTHON_SHELL);
  g_type_ensure (GTK_TYPE_INSPECTOR_RESOURCE_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_SIGNALS_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_STYLE_PROP_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_SIZE_GROUPS);
  g_type_ensure (GTK_TYPE_INSPECTOR_VISUAL);
  g_type_ensure (GTK_TYPE_INSPECTOR_WIDGET_TREE);
  g_type_ensure (GTK_TYPE_INSPECTOR_WINDOW);
}


// vim: set et sw=2 ts=2:
