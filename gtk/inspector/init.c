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

#include "init.h"

#include "actions.h"
#include "cellrenderergraph.h"
#include "css-editor.h"
#include "css-node-tree.h"
#include "data-list.h"
#include "general.h"
#include "gestures.h"
#include "graphdata.h"
#include "magnifier.h"
#include "menu.h"
#include "misc-info.h"
#include "object-hierarchy.h"
#include "object-tree.h"
#include "prop-list.h"
#include "resource-list.h"
#include "selector.h"
#include "signals-list.h"
#include "size-groups.h"
#include "statistics.h"
#include "visual.h"
#include "window.h"
#include "gtkstackcombo.h"

#include "gtkmagnifierprivate.h"

#include "gtkmodulesprivate.h"

void
gtk_inspector_init (void)
{
  static GIOExtensionPoint *extension_point = NULL;

  g_type_ensure (GTK_TYPE_CELL_RENDERER_GRAPH);
  g_type_ensure (GTK_TYPE_GRAPH_DATA);
  g_type_ensure (GTK_TYPE_INSPECTOR_ACTIONS);
  g_type_ensure (GTK_TYPE_INSPECTOR_CSS_EDITOR);
  g_type_ensure (GTK_TYPE_INSPECTOR_CSS_NODE_TREE);
  g_type_ensure (GTK_TYPE_INSPECTOR_DATA_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_GENERAL);
  g_type_ensure (GTK_TYPE_INSPECTOR_GESTURES);
  g_type_ensure (GTK_TYPE_MAGNIFIER);
  g_type_ensure (GTK_TYPE_INSPECTOR_MAGNIFIER);
  g_type_ensure (GTK_TYPE_INSPECTOR_MENU);
  g_type_ensure (GTK_TYPE_INSPECTOR_MISC_INFO);
  g_type_ensure (GTK_TYPE_INSPECTOR_OBJECT_HIERARCHY);
  g_type_ensure (GTK_TYPE_INSPECTOR_OBJECT_TREE);
  g_type_ensure (GTK_TYPE_INSPECTOR_PROP_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_RESOURCE_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_SELECTOR);
  g_type_ensure (GTK_TYPE_INSPECTOR_SIGNALS_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_SIZE_GROUPS);
  g_type_ensure (GTK_TYPE_INSPECTOR_STATISTICS);
  g_type_ensure (GTK_TYPE_INSPECTOR_VISUAL);
  g_type_ensure (GTK_TYPE_INSPECTOR_WINDOW);
  g_type_ensure (GTK_TYPE_STACK_COMBO);

  if (extension_point == NULL)
    {
      GIOModuleScope *scope;
      gchar **paths;
      int i;

      extension_point = g_io_extension_point_register ("gtk-inspector-page");
      g_io_extension_point_set_required_type (extension_point, GTK_TYPE_WIDGET);

      paths = _gtk_get_module_path ("inspector");
      scope = g_io_module_scope_new (G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);

      for (i = 0; paths[i] != NULL; i++)
        g_io_modules_load_all_in_directory_with_scope (paths[i], scope);

      g_strfreev (paths);
      g_io_module_scope_free (scope);
    }
}

// vim: set et sw=2 ts=2:
