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

G_DEFINE_TYPE (GtkCssNode, gtk_css_node, G_TYPE_OBJECT)

static void
gtk_css_node_finalize (GObject *object)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  if (cssnode->style)
    g_object_unref (cssnode->style);
  gtk_css_node_declaration_unref (cssnode->decl);

  G_OBJECT_CLASS (gtk_css_node_parent_class)->finalize (object);
}

static void
gtk_css_node_class_init (GtkCssNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_node_finalize;
}

static void
gtk_css_node_init (GtkCssNode *cssnode)
{
  cssnode->decl = gtk_css_node_declaration_new ();
}

GtkCssNode *
gtk_css_node_new (void)
{
  return g_object_new (GTK_TYPE_CSS_NODE, NULL);
}

GtkCssNode *
gtk_css_node_copy (GtkCssNode *cssnode)
{
  GtkCssNode *copy;

  copy = gtk_css_node_new ();
  gtk_css_node_declaration_unref (copy->decl);
  copy->decl = gtk_css_node_declaration_ref (cssnode->decl);

  return copy;
}

void
gtk_css_node_set_parent (GtkCssNode *cssnode,
                         GtkCssNode *parent)
{
  cssnode->parent = parent;
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

void
gtk_css_node_set_widget_type (GtkCssNode *cssnode,
                              GType       widget_type)
{
  gtk_css_node_declaration_set_type (&cssnode->decl, widget_type);
}

GType
gtk_css_node_get_widget_type (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_type (cssnode->decl);
}

gboolean
gtk_css_node_set_id (GtkCssNode *cssnode,
                     const char *id)
{
  return gtk_css_node_declaration_set_id (&cssnode->decl, id);
}

const char *
gtk_css_node_get_id (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_id (cssnode->decl);
}

gboolean
gtk_css_node_set_state (GtkCssNode    *cssnode,
                        GtkStateFlags  state_flags)
{
  return gtk_css_node_declaration_set_state (&cssnode->decl, state_flags);
}

GtkStateFlags
gtk_css_node_get_state (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_state (cssnode->decl);
}

void
gtk_css_node_set_junction_sides (GtkCssNode       *cssnode,
                                 GtkJunctionSides  junction_sides)
{
  gtk_css_node_declaration_set_junction_sides (&cssnode->decl, junction_sides);
}

GtkJunctionSides
gtk_css_node_get_junction_sides (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_junction_sides (cssnode->decl);
}

gboolean
gtk_css_node_add_class (GtkCssNode *cssnode,
                        GQuark      style_class)
{
  return gtk_css_node_declaration_add_class (&cssnode->decl, style_class);
}

gboolean
gtk_css_node_remove_class (GtkCssNode *cssnode,
                           GQuark      style_class)
{
  return gtk_css_node_declaration_remove_class (&cssnode->decl, style_class);
}

gboolean
gtk_css_node_has_class (GtkCssNode *cssnode,
                        GQuark      style_class)
{
  return gtk_css_node_declaration_has_class (cssnode->decl, style_class);
}

GList *
gtk_css_node_list_classes (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_list_classes (cssnode->decl);
}

gboolean
gtk_css_node_add_region (GtkCssNode     *cssnode,
                         GQuark          region,
                         GtkRegionFlags  flags)
{
  return gtk_css_node_declaration_add_region (&cssnode->decl, region, flags);
}

gboolean
gtk_css_node_remove_region (GtkCssNode *cssnode,
                            GQuark      region)
{
  return gtk_css_node_declaration_remove_region (&cssnode->decl, region);
}

gboolean
gtk_css_node_has_region (GtkCssNode     *cssnode,
                         GQuark          region,
                         GtkRegionFlags *out_flags)
{
  return gtk_css_node_declaration_has_region (cssnode->decl, region, out_flags);
}

GList *
gtk_css_node_list_regions (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_list_regions (cssnode->decl);
}


const GtkCssNodeDeclaration *
gtk_css_node_get_declaration (GtkCssNode *cssnode)
{
  return cssnode->decl;
}

GtkCssNodeDeclaration *
gtk_css_node_dup_declaration (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_ref (cssnode->decl);
}
