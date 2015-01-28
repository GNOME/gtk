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

#include "gtkcsstransientnodeprivate.h"

G_DEFINE_TYPE (GtkCssNode, gtk_css_node, G_TYPE_OBJECT)

void
gtk_css_node_set_invalid (GtkCssNode *node,
                          gboolean    invalid)
{
  if (node->invalid == invalid)
    return;

  GTK_CSS_NODE_GET_CLASS (node)->set_invalid (node, invalid);
}

static void
gtk_css_node_dispose (GObject *object)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  while (cssnode->first_child)
    {
      gtk_css_node_set_parent (cssnode->first_child, NULL);
    }

  G_OBJECT_CLASS (gtk_css_node_parent_class)->dispose (object);
}

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
gtk_css_node_real_invalidate (GtkCssNode   *cssnode,
                              GtkCssChange  change)
{
}

static void
gtk_css_node_real_set_invalid (GtkCssNode *node,
                               gboolean    invalid)
{
  node->invalid = invalid;

  if (invalid && node->parent)
    gtk_css_node_set_invalid (node->parent, invalid);
}

static GtkWidgetPath *
gtk_css_node_real_create_widget_path (GtkCssNode *cssnode)
{
  return gtk_widget_path_new ();
}

static const GtkWidgetPath *
gtk_css_node_real_get_widget_path (GtkCssNode *cssnode)
{
  return NULL;
}

static void
gtk_css_node_class_init (GtkCssNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_node_dispose;
  object_class->finalize = gtk_css_node_finalize;

  klass->invalidate = gtk_css_node_real_invalidate;
  klass->set_invalid = gtk_css_node_real_set_invalid;
  klass->create_widget_path = gtk_css_node_real_create_widget_path;
  klass->get_widget_path = gtk_css_node_real_get_widget_path;
}

static void
gtk_css_node_init (GtkCssNode *cssnode)
{
  cssnode->decl = gtk_css_node_declaration_new ();
}

void
gtk_css_node_set_parent (GtkCssNode *node,
                         GtkCssNode *parent)
{
  if (node->parent == parent)
    return;

  /* Take a reference here so the whole function has a reference */
  g_object_ref (node);

  if (node->parent != NULL)
    {
      if (!GTK_IS_CSS_TRANSIENT_NODE (node))
        {
          if (node->previous_sibling)
            node->previous_sibling->next_sibling = node->next_sibling;
          else
            node->parent->first_child = node->next_sibling;

          if (node->next_sibling)
            node->next_sibling->previous_sibling = node->previous_sibling;
          else
            node->parent->last_child = node->previous_sibling;

          node->parent->n_children--;
        }

      node->parent = NULL;
      node->next_sibling = NULL;
      node->previous_sibling = NULL;

      g_object_unref (node);
    }

  if (parent)
    {
      node->parent = parent;

      if (!GTK_IS_CSS_TRANSIENT_NODE (node))
        {
          parent->n_children++;

          if (parent->last_child)
            {
              parent->last_child->next_sibling = node;
              node->previous_sibling = parent->last_child;
            }
          parent->last_child = node;

          if (parent->first_child == NULL)
            parent->first_child = node;
        }

      if (node->invalid)
        gtk_css_node_set_invalid (parent, TRUE);
    }

  gtk_css_node_invalidate (node, GTK_CSS_CHANGE_ANY_PARENT | GTK_CSS_CHANGE_ANY_SIBLING);

  if (node->parent == NULL)
    g_object_unref (node);
}

GtkCssNode *
gtk_css_node_get_parent (GtkCssNode *cssnode)
{
  return cssnode->parent;
}

GtkCssNode *
gtk_css_node_get_first_child (GtkCssNode *cssnode)
{
  return cssnode->first_child;
}

GtkCssNode *
gtk_css_node_get_last_child (GtkCssNode *cssnode)
{
  return cssnode->last_child;
}

GtkCssNode *
gtk_css_node_get_previous_sibling (GtkCssNode *cssnode)
{
  return cssnode->previous_sibling;
}

GtkCssNode *
gtk_css_node_get_next_sibling (GtkCssNode *cssnode)
{
  return cssnode->next_sibling;
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
  if (gtk_css_node_declaration_set_type (&cssnode->decl, widget_type))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_NAME);
}

GType
gtk_css_node_get_widget_type (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_type (cssnode->decl);
}

void
gtk_css_node_set_id (GtkCssNode *cssnode,
                     const char *id)
{
  if (gtk_css_node_declaration_set_id (&cssnode->decl, id))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_ID);
}

const char *
gtk_css_node_get_id (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_id (cssnode->decl);
}

void
gtk_css_node_set_state (GtkCssNode    *cssnode,
                        GtkStateFlags  state_flags)
{
  if (gtk_css_node_declaration_set_state (&cssnode->decl, state_flags))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_STATE);
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

void
gtk_css_node_add_class (GtkCssNode *cssnode,
                        GQuark      style_class)
{
  if (gtk_css_node_declaration_add_class (&cssnode->decl, style_class))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
}

void
gtk_css_node_remove_class (GtkCssNode *cssnode,
                           GQuark      style_class)
{
  if (gtk_css_node_declaration_remove_class (&cssnode->decl, style_class))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
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

void
gtk_css_node_add_region (GtkCssNode     *cssnode,
                         GQuark          region,
                         GtkRegionFlags  flags)
{
  if (gtk_css_node_declaration_add_region (&cssnode->decl, region, flags))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_REGION);
}

void
gtk_css_node_remove_region (GtkCssNode *cssnode,
                            GQuark      region)
{
  if (gtk_css_node_declaration_remove_region (&cssnode->decl, region))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_REGION);
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

void
gtk_css_node_invalidate (GtkCssNode   *cssnode,
                         GtkCssChange  change)
{
  GTK_CSS_NODE_GET_CLASS (cssnode)->invalidate (cssnode, change);

  gtk_css_node_set_invalid (cssnode, TRUE);
}

GtkWidgetPath *
gtk_css_node_create_widget_path (GtkCssNode *cssnode)
{
  return GTK_CSS_NODE_GET_CLASS (cssnode)->create_widget_path (cssnode);
}

const GtkWidgetPath *
gtk_css_node_get_widget_path (GtkCssNode *cssnode)
{
  return GTK_CSS_NODE_GET_CLASS (cssnode)->get_widget_path (cssnode);
}

