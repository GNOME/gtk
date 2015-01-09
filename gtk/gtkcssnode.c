/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkcssnodeprivate.h"

GtkCssNode *
gtk_css_node_new (void)
{
  GtkCssNode *cssnode;

  cssnode = g_slice_new0 (GtkCssNode);
  cssnode->decl = gtk_css_node_declaration_new ();

  return cssnode;
}

void
gtk_css_node_free (GtkCssNode *cssnode)
{
  if (cssnode->style)
    g_object_unref (cssnode->style);
  gtk_css_node_declaration_unref (cssnode->decl);
  g_slice_free (GtkCssNode, cssnode);
}

GtkCssNode *
gtk_css_node_get_parent (GtkCssNode *cssnode)
{
  return cssnode->parent;
}

void
gtk_css_node_set_style (GtkCssNode  *cssnode,
                        GtkCssStyle *style)
{
  if (cssnode->style == style)
    return;

  if (style)
    g_object_ref (style);

  if (cssnode->style)
    g_object_unref (cssnode->style);

  cssnode->style = style;
}

GtkCssStyle *
gtk_css_node_get_style (GtkCssNode *cssnode)
{
  return cssnode->style;
}

