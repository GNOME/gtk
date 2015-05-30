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

#include "gtkcssanimatedstyleprivate.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtksettingsprivate.h"
#include "gtktypebuiltins.h"

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_CSS_RADICAL_CHANGE (GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_SOURCE | GTK_CSS_CHANGE_PARENT_STYLE)

G_DEFINE_TYPE (GtkCssNode, gtk_css_node, G_TYPE_OBJECT)

enum {
  NODE_ADDED,
  NODE_REMOVED,
  STYLE_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CLASSES,
  PROP_ID,
  PROP_STATE,
  PROP_VISIBLE,
  PROP_WIDGET_TYPE,
  NUM_PROPERTIES
};

static guint cssnode_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *cssnode_properties[NUM_PROPERTIES];

static GtkStyleProviderPrivate *
gtk_css_node_get_style_provider_or_null (GtkCssNode *cssnode)
{
  return GTK_CSS_NODE_GET_CLASS (cssnode)->get_style_provider (cssnode);
}

static void
gtk_css_node_set_invalid (GtkCssNode *node,
                          gboolean    invalid)
{
  if (node->invalid == invalid)
    return;

  node->invalid = invalid;

  if (node->parent)
    {
      if (invalid && node->visible)
        gtk_css_node_set_invalid (node->parent, TRUE);
    }
  else
    {
      if (invalid)
        GTK_CSS_NODE_GET_CLASS (node)->queue_validate (node);
      else
        GTK_CSS_NODE_GET_CLASS (node)->dequeue_validate (node);
    }
}

static void
gtk_css_node_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  switch (property_id)
    {
    case PROP_CLASSES:
      g_value_take_boxed (value, gtk_css_node_get_classes (cssnode));
      break;

    case PROP_ID:
      g_value_set_string (value, gtk_css_node_get_id (cssnode));
      break;

    case PROP_STATE:
      g_value_set_flags (value, gtk_css_node_get_state (cssnode));
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, gtk_css_node_get_visible (cssnode));
      break;

    case PROP_WIDGET_TYPE:
      g_value_set_gtype (value, gtk_css_node_get_widget_type (cssnode));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_css_node_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  switch (property_id)
    {
    case PROP_CLASSES:
      gtk_css_node_set_classes (cssnode, g_value_get_boxed (value));
      break;

    case PROP_ID:
      gtk_css_node_set_id (cssnode, g_value_get_string (value));
      break;

    case PROP_STATE:
      gtk_css_node_set_state (cssnode, g_value_get_flags (value));
      break;

    case PROP_VISIBLE:
      gtk_css_node_set_visible (cssnode, g_value_get_boolean (value));
      break;

    case PROP_WIDGET_TYPE:
      gtk_css_node_set_widget_type (cssnode, g_value_get_gtype (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_css_node_dispose (GObject *object)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  while (cssnode->first_child)
    {
      gtk_css_node_set_parent (cssnode->first_child, NULL);
    }

  gtk_css_node_set_invalid (cssnode, FALSE);

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

static gboolean
may_use_global_parent_cache (GtkCssNode *node)
{
  GtkStyleProviderPrivate *provider;
  GtkCssNode *parent;
  
  parent = gtk_css_node_get_parent (node);
  if (parent == NULL)
    return FALSE;

  provider = gtk_css_node_get_style_provider_or_null (node);
  if (provider != NULL && provider != gtk_css_node_get_style_provider (parent))
    return FALSE;

  return TRUE;
}

static GtkCssStyle *
lookup_in_global_parent_cache (GtkCssNode                  *node,
                               GtkCssStyle                 *parent,
                               const GtkCssNodeDeclaration *decl)
{
  GHashTable *cache;
  GtkCssStyle *style;

  if (parent == NULL ||
      !may_use_global_parent_cache (node))
    return NULL;

  cache = g_object_get_data (G_OBJECT (parent), "gtk-global-cache");
  if (cache == NULL)
    return NULL;

  style = g_hash_table_lookup (cache, decl);

  return style;
}

static gboolean
may_be_stored_in_parent_cache (GtkCssStyle *style)
{
  GtkCssChange change;

  change = gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style));

  /* The cache is shared between all children of the parent, so if a
   * style depends on a sibling it is not independant of the child.
   */
  if (change & GTK_CSS_CHANGE_ANY_SIBLING)
    return FALSE;

  /* Again, the cache is shared between all children of the parent.
   * If the position is relevant, no child has the same style.
   */
  if (change & GTK_CSS_CHANGE_POSITION)
    return FALSE;

  return TRUE;
}

static void
store_in_global_parent_cache (GtkCssNode                  *node,
                              GtkCssStyle                 *parent,
                              const GtkCssNodeDeclaration *decl,
                              GtkCssStyle                 *style)
{
  GHashTable *cache;

  g_assert (GTK_IS_CSS_STATIC_STYLE (style));

  if (parent == NULL ||
      !may_use_global_parent_cache (node))
    return;

  if (!may_be_stored_in_parent_cache (style))
    return;

  cache = g_object_get_data (G_OBJECT (parent), "gtk-global-cache");
  if (cache == NULL)
    {
      cache = g_hash_table_new_full (gtk_css_node_declaration_hash,
                                     gtk_css_node_declaration_equal,
                                     (GDestroyNotify) gtk_css_node_declaration_unref,
                                     g_object_unref);
      g_object_set_data_full (G_OBJECT (parent), "gtk-global-cache", cache, (GDestroyNotify) g_hash_table_destroy);
    }

  g_hash_table_insert (cache,
                       gtk_css_node_declaration_ref ((GtkCssNodeDeclaration *) decl),
                       g_object_ref (style));
}

static GtkCssStyle *
gtk_css_node_create_style (GtkCssNode *cssnode)
{
  const GtkCssNodeDeclaration *decl;
  GtkCssMatcher matcher;
  GtkCssStyle *parent;
  GtkCssStyle *style;

  decl = gtk_css_node_get_declaration (cssnode);
  parent = cssnode->parent ? cssnode->parent->style : NULL;

  style = lookup_in_global_parent_cache (cssnode, parent, decl);
  if (style)
    return g_object_ref (style);

  if (gtk_css_node_init_matcher (cssnode, &matcher))
    style = gtk_css_static_style_new_compute (gtk_css_node_get_style_provider (cssnode),
                                              &matcher,
                                              parent);
  else
    style = gtk_css_static_style_new_compute (gtk_css_node_get_style_provider (cssnode),
                                              NULL,
                                              parent);

  store_in_global_parent_cache (cssnode, parent, decl, style);

  return style;
}

static gboolean
should_create_transitions (GtkCssChange change)
{
  return (change & GTK_CSS_CHANGE_ANIMATIONS) == 0;
}

static gboolean
gtk_css_style_needs_recreation (GtkCssStyle  *style,
                                GtkCssChange  change)
{
  /* Try to avoid invalidating if we can */
  if (change & GTK_CSS_RADICAL_CHANGE)
    return TRUE;

  if (GTK_IS_CSS_ANIMATED_STYLE (style))
    style = GTK_CSS_ANIMATED_STYLE (style)->style;

  if (gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style)) & change)
    return TRUE;
  else
    return FALSE;
}

static GtkCssStyle *
gtk_css_node_real_update_style (GtkCssNode   *cssnode,
                                GtkCssChange  change,
                                gint64        timestamp,
                                GtkCssStyle  *style)
{
  GtkCssStyle *static_style, *new_static_style, *new_style;

  if (GTK_IS_CSS_ANIMATED_STYLE (style))
    {
      static_style = GTK_CSS_ANIMATED_STYLE (style)->style;
    }
  else
    {
      static_style = style;
    }

  if (gtk_css_style_needs_recreation (static_style, change))
    new_static_style = gtk_css_node_create_style (cssnode);
  else
    new_static_style = g_object_ref (static_style);

  if (new_static_style != static_style || (change & GTK_CSS_CHANGE_ANIMATIONS))
    {
      GtkCssNode *parent = gtk_css_node_get_parent (cssnode);
      new_style = gtk_css_animated_style_new (new_static_style,
                                              parent ? gtk_css_node_get_style (parent) : NULL,
                                              timestamp,
                                              gtk_css_node_get_style_provider (cssnode),
                                              should_create_transitions (change) ? style : NULL);
    }
  else if (GTK_IS_CSS_ANIMATED_STYLE (style) && (change & GTK_CSS_CHANGE_TIMESTAMP))
    {
      new_style = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (style),
                                                      static_style,
                                                      timestamp);
    }
  else
    {
      new_style = g_object_ref (style);
    }

  if (!gtk_css_style_is_static (new_style))
    gtk_css_node_set_invalid (cssnode, TRUE);

  g_object_unref (new_static_style);

  return new_style;
}

static void
gtk_css_node_real_invalidate (GtkCssNode *node)
{
}

static void
gtk_css_node_real_queue_validate (GtkCssNode *node)
{
}

static void
gtk_css_node_real_dequeue_validate (GtkCssNode *node)
{
}

static void
gtk_css_node_real_validate (GtkCssNode *node)
{
}

gboolean
gtk_css_node_real_init_matcher (GtkCssNode     *cssnode,
                                GtkCssMatcher  *matcher)
{
  _gtk_css_matcher_node_init (matcher, cssnode);

  return TRUE;
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

static GtkStyleProviderPrivate *
gtk_css_node_real_get_style_provider (GtkCssNode *cssnode)
{
  return NULL;
}

static GdkFrameClock *
gtk_css_node_real_get_frame_clock (GtkCssNode *cssnode)
{
  return NULL;
}

static void
gtk_css_node_real_node_removed (GtkCssNode *parent,
                                GtkCssNode *node,
                                GtkCssNode *previous)
{
  if (node->previous_sibling)
    node->previous_sibling->next_sibling = node->next_sibling;
  else
    node->parent->first_child = node->next_sibling;

  if (node->next_sibling)
    node->next_sibling->previous_sibling = node->previous_sibling;
  else
    node->parent->last_child = node->previous_sibling;

  node->previous_sibling = NULL;
  node->next_sibling = NULL;
  node->parent = NULL;
}

static void
gtk_css_node_real_node_added (GtkCssNode *parent,
                              GtkCssNode *node,
                              GtkCssNode *new_previous)
{
  if (new_previous)
    {
      node->previous_sibling = new_previous;
      node->next_sibling = new_previous->next_sibling;
      new_previous->next_sibling = node;
    }
  else
    {
      node->next_sibling = parent->first_child;
      parent->first_child = node;
    }

  if (node->next_sibling)
    node->next_sibling->previous_sibling = node;
  else
    parent->last_child = node;

  node->parent = parent;
}

static void
gtk_css_node_real_style_changed (GtkCssNode  *cssnode,
                                 GtkCssStyle *old_style,
                                 GtkCssStyle *new_style)
{
  g_object_ref (new_style);
  g_object_unref (old_style);

  cssnode->style = new_style;
}

static void
gtk_css_node_class_init (GtkCssNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_css_node_get_property;
  object_class->set_property = gtk_css_node_set_property;
  object_class->dispose = gtk_css_node_dispose;
  object_class->finalize = gtk_css_node_finalize;

  klass->update_style = gtk_css_node_real_update_style;
  klass->invalidate = gtk_css_node_real_invalidate;
  klass->validate = gtk_css_node_real_validate;
  klass->queue_validate = gtk_css_node_real_queue_validate;
  klass->dequeue_validate = gtk_css_node_real_dequeue_validate;
  klass->init_matcher = gtk_css_node_real_init_matcher;
  klass->create_widget_path = gtk_css_node_real_create_widget_path;
  klass->get_widget_path = gtk_css_node_real_get_widget_path;
  klass->get_style_provider = gtk_css_node_real_get_style_provider;
  klass->get_frame_clock = gtk_css_node_real_get_frame_clock;

  klass->node_added = gtk_css_node_real_node_added;
  klass->node_removed = gtk_css_node_real_node_removed;
  klass->style_changed = gtk_css_node_real_style_changed;

  cssnode_signals[NODE_ADDED] =
    g_signal_new (I_("node-added"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCssNodeClass, node_added),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_CSS_NODE, GTK_TYPE_CSS_NODE);
  cssnode_signals[NODE_REMOVED] =
    g_signal_new (I_("node-removed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCssNodeClass, node_removed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_CSS_NODE, GTK_TYPE_CSS_NODE);
  cssnode_signals[STYLE_CHANGED] =
    g_signal_new (I_("style-changed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCssNodeClass, style_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_CSS_STYLE, GTK_TYPE_CSS_STYLE);

  cssnode_properties[PROP_CLASSES] =
    g_param_spec_boxed ("classes", "Classes",
                         "List of classes",
                         G_TYPE_STRV,
                         G_PARAM_READWRITE
                         | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_ID] =
    g_param_spec_string ("id", "ID",
                         "Unique ID",
                         NULL,
                         G_PARAM_READWRITE
                         | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_STATE] =
    g_param_spec_flags ("state", "State",
                        "State flags",
                        GTK_TYPE_STATE_FLAGS,
                        0,
                        G_PARAM_READWRITE
                        | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_VISIBLE] =
    g_param_spec_boolean ("visible", "Visible",
                          "If other nodes can see this node",
                          TRUE,
                          G_PARAM_READWRITE
                          | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_WIDGET_TYPE] =
    g_param_spec_gtype ("widget-type", "Widget type",
                        "GType of the widget",
                        G_TYPE_NONE,
                        G_PARAM_READWRITE
                        | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, cssnode_properties);
}

static void
gtk_css_node_init (GtkCssNode *cssnode)
{
  cssnode->decl = gtk_css_node_declaration_new ();

  cssnode->style = g_object_ref (gtk_css_static_style_get_default ());

  cssnode->visible = TRUE;
}

static GdkFrameClock *
gtk_css_node_get_frame_clock_or_null (GtkCssNode *cssnode)
{
  while (cssnode->parent)
    cssnode = cssnode->parent;

  return GTK_CSS_NODE_GET_CLASS (cssnode)->get_frame_clock (cssnode);
}

static gint64
gtk_css_node_get_timestamp (GtkCssNode *cssnode)
{
  GdkFrameClock *frameclock;

  frameclock = gtk_css_node_get_frame_clock_or_null (cssnode);
  if (frameclock == NULL)
    return 0;

  return gdk_frame_clock_get_frame_time (frameclock);
}

static void
gtk_css_node_parent_was_unset (GtkCssNode *node)
{
  if (node->invalid)
    GTK_CSS_NODE_GET_CLASS (node)->queue_validate (node);
}

static void
gtk_css_node_parent_will_be_set (GtkCssNode *node)
{
  if (node->invalid)
    GTK_CSS_NODE_GET_CLASS (node)->dequeue_validate (node);
}

static void
gtk_css_node_invalidate_style (GtkCssNode *cssnode)
{
  if (cssnode->style_is_invalid)
    return;

  cssnode->style_is_invalid = TRUE;
  gtk_css_node_set_invalid (cssnode, TRUE);
  
  if (cssnode->first_child)
    gtk_css_node_invalidate_style (cssnode->first_child);
  
  if (cssnode->next_sibling)
    gtk_css_node_invalidate_style (cssnode->next_sibling);
}

static void
gtk_css_node_reposition (GtkCssNode *node,
                         GtkCssNode *new_parent,
                         GtkCssNode *previous)
{
  GtkCssNode *old_parent;

  g_assert (! (new_parent == NULL && previous != NULL));

  old_parent = node->parent;
  /* Take a reference here so the whole function has a reference */
  g_object_ref (node);

  if (node->next_sibling)
    {
      if (node->visible)
        gtk_css_node_invalidate (node->next_sibling, GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_ANY_SIBLING);
    }

  if (old_parent != NULL)
    {
      g_signal_emit (old_parent, cssnode_signals[NODE_REMOVED], 0, node, node->previous_sibling);
    }

  if (old_parent != new_parent)
    {
      if (old_parent == NULL)
        {
          gtk_css_node_parent_will_be_set (node);
        }
      else
        {
          g_object_unref (node);
        }

      if (gtk_css_node_get_style_provider_or_null (node) == NULL)
        gtk_css_node_invalidate_style_provider (node);
      gtk_css_node_invalidate (node, GTK_CSS_CHANGE_TIMESTAMP | GTK_CSS_CHANGE_ANIMATIONS);

      if (new_parent)
        {
          g_object_ref (node);

          if (node->pending_changes)
            new_parent->needs_propagation = TRUE;
          if (node->invalid && node->visible)
            gtk_css_node_set_invalid (new_parent, TRUE);
        }
      else
        {
          gtk_css_node_parent_was_unset (node);
        }
    }

  if (new_parent)
    {
      g_signal_emit (new_parent, cssnode_signals[NODE_ADDED], 0, node, previous);
    }

  if (node->next_sibling)
    gtk_css_node_invalidate_style (node->next_sibling);

  gtk_css_node_invalidate (node, GTK_CSS_CHANGE_ANY_PARENT | GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_ANY_SIBLING);

  g_object_unref (node);
}

void
gtk_css_node_set_parent (GtkCssNode *node,
                         GtkCssNode *parent)
{
  if (node->parent == parent)
    return;

  gtk_css_node_reposition (node, parent, parent ? parent->last_child : NULL);
}

void
gtk_css_node_set_after (GtkCssNode *cssnode,
                        GtkCssNode *previous_sibling)
{
  if (cssnode->previous_sibling == previous_sibling)
    return;

  gtk_css_node_reposition (cssnode,
                           previous_sibling->parent,
                           previous_sibling);
}

void
gtk_css_node_set_before (GtkCssNode *cssnode,
                         GtkCssNode *next_sibling)
{
  if (cssnode->next_sibling == next_sibling)
    return;

  gtk_css_node_reposition (cssnode,
                           next_sibling->parent,
                           next_sibling->previous_sibling);
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

static gboolean
gtk_css_node_set_style (GtkCssNode  *cssnode,
                        GtkCssStyle *style)
{
  if (cssnode->style == style)
    return FALSE;

  g_signal_emit (cssnode, cssnode_signals[STYLE_CHANGED], 0, cssnode->style, style);
  return TRUE;
}

static void
gtk_css_node_propagate_pending_changes (GtkCssNode *cssnode,
                                        gboolean    style_changed)
{
  GtkCssChange change, child_change;
  GtkCssNode *child;

  change = _gtk_css_change_for_child (cssnode->pending_changes);
  if (style_changed)
    change |= GTK_CSS_CHANGE_PARENT_STYLE;

  if (!cssnode->needs_propagation && change == 0)
    return;

  for (child = gtk_css_node_get_first_child (cssnode);
       child;
       child = gtk_css_node_get_next_sibling (child))
    {
      child_change = child->pending_changes;
      gtk_css_node_invalidate (child, change);
      if (child->visible)
        change |= _gtk_css_change_for_sibling (child_change);
    }

  cssnode->needs_propagation = FALSE;
}

static gboolean
gtk_css_node_needs_new_style (GtkCssNode *cssnode)
{
  return cssnode->style_is_invalid || cssnode->needs_propagation;
}

static void
gtk_css_node_ensure_style (GtkCssNode *cssnode,
                           gint64      current_time)
{
  gboolean style_changed;

  if (!gtk_css_node_needs_new_style (cssnode))
    return;

  if (cssnode->parent)
    gtk_css_node_ensure_style (cssnode->parent, current_time);

  if (cssnode->style_is_invalid)
    {
      GtkCssStyle *new_style;

      if (cssnode->previous_sibling)
        gtk_css_node_ensure_style (cssnode->previous_sibling, current_time);

      new_style = GTK_CSS_NODE_GET_CLASS (cssnode)->update_style (cssnode,
                                                                  cssnode->pending_changes,
                                                                  current_time,
                                                                  cssnode->style);

      style_changed = gtk_css_node_set_style (cssnode, new_style);
      g_object_unref (new_style);
    }
  else
    {
      style_changed = FALSE;
    }

  gtk_css_node_propagate_pending_changes (cssnode, style_changed);

  cssnode->pending_changes = 0;
  cssnode->style_is_invalid = FALSE;
}

GtkCssStyle *
gtk_css_node_get_style (GtkCssNode *cssnode)
{
  if (gtk_css_node_needs_new_style (cssnode))
    {
      gint64 timestamp = gtk_css_node_get_timestamp (cssnode);

      gtk_css_node_ensure_style (cssnode, timestamp);
    }

  return cssnode->style;
}

void
gtk_css_node_set_visible (GtkCssNode *cssnode,
                          gboolean    visible)
{
  if (cssnode->visible == visible)
    return;

  cssnode->visible = visible;
  g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_VISIBLE]);

  if (cssnode->next_sibling)
    gtk_css_node_invalidate (cssnode->next_sibling, GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_ANY_SIBLING);
}

gboolean
gtk_css_node_get_visible (GtkCssNode *cssnode)
{
  return cssnode->visible;
}

void
gtk_css_node_set_widget_type (GtkCssNode *cssnode,
                              GType       widget_type)
{
  if (gtk_css_node_declaration_set_type (&cssnode->decl, widget_type))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_NAME);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_WIDGET_TYPE]);
    }
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
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_ID);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_ID]);
    }
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
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_STATE);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_STATE]);
    }
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

static void
gtk_css_node_clear_classes (GtkCssNode *cssnode)
{
  GList *list, *l;
  
  list = gtk_css_node_declaration_list_classes (cssnode->decl);

  for (l = list; l; l = l->next)
    {
      gtk_css_node_remove_class (cssnode, GPOINTER_TO_UINT (l->data));
    }

  g_list_free (list);
}

void
gtk_css_node_set_classes (GtkCssNode  *cssnode,
                          const char **classes)
{
  guint i;

  g_object_freeze_notify (G_OBJECT (cssnode));

  gtk_css_node_clear_classes (cssnode);

  if (classes)
    {
      for (i = 0; classes[i] != NULL; i++)
        {
          gtk_css_node_add_class (cssnode, g_quark_from_string (classes[i]));
        }
    }

  g_object_thaw_notify (G_OBJECT (cssnode));
}

char **
gtk_css_node_get_classes (GtkCssNode *cssnode)
{
  GList *list, *l;
  GPtrArray *result;
  
  list = gtk_css_node_declaration_list_classes (cssnode->decl);
  result = g_ptr_array_new ();

  for (l = list; l; l = l->next)
    {
      g_ptr_array_add (result, g_strdup (g_quark_to_string (GPOINTER_TO_UINT (l->data))));
    }

  g_ptr_array_add (result, NULL);

  g_list_free (list);

  return (char **) g_ptr_array_free (result, FALSE);
}

void
gtk_css_node_add_class (GtkCssNode *cssnode,
                        GQuark      style_class)
{
  if (gtk_css_node_declaration_add_class (&cssnode->decl, style_class))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_CLASSES]);
    }
}

void
gtk_css_node_remove_class (GtkCssNode *cssnode,
                           GQuark      style_class)
{
  if (gtk_css_node_declaration_remove_class (&cssnode->decl, style_class))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_CLASSES]);
    }
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

void
gtk_css_node_invalidate_style_provider (GtkCssNode *cssnode)
{
  GtkCssNode *child;

  gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_SOURCE);

  for (child = cssnode->first_child;
       child;
       child = child->next_sibling)
    {
      if (gtk_css_node_get_style_provider_or_null (child) == NULL)
        gtk_css_node_invalidate_style_provider (child);
    }
}

static void
gtk_css_node_invalidate_timestamp (GtkCssNode *cssnode)
{
  GtkCssNode *child;

  if (!cssnode->invalid)
    return;

  if (!gtk_css_style_is_static (cssnode->style))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_TIMESTAMP);

  for (child = cssnode->first_child; child; child = child->next_sibling)
    {
      gtk_css_node_invalidate_timestamp (child);
    }
}

void
gtk_css_node_invalidate_frame_clock (GtkCssNode *cssnode,
                                     gboolean    just_timestamp)
{
  /* frame clock is handled by the top level */
  if (cssnode->parent)
    return;

  gtk_css_node_invalidate_timestamp (cssnode);

  if (!just_timestamp)
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_ANIMATIONS);
}

void
gtk_css_node_invalidate (GtkCssNode   *cssnode,
                         GtkCssChange  change)
{
  if (!cssnode->invalid)
    change &= ~GTK_CSS_CHANGE_TIMESTAMP;

  if (change == 0)
    return;

  cssnode->pending_changes |= change;

  GTK_CSS_NODE_GET_CLASS (cssnode)->invalidate (cssnode);

  if (cssnode->parent)
    cssnode->parent->needs_propagation = TRUE;
  gtk_css_node_invalidate_style (cssnode);
}

void
gtk_css_node_validate_internal (GtkCssNode *cssnode,
                                gint64      timestamp)
{
  GtkCssNode *child;

  /* If you run your application with
   *   GTK_DEBUG=no-css-cache
   * every invalidation will purge the cache and completely query
   * everything anew form the cache. This is slow (in particular
   * when animating), but useful for figuring out bugs.
   *
   * We achieve that by pretending that everything that could have
   * changed has and so we of course totally need to redo everything.
   *
   * Note that this also completely revalidates child widgets all
   * the time.
   */
  if (G_UNLIKELY (gtk_get_debug_flags () & GTK_DEBUG_NO_CSS_CACHE))
    cssnode->pending_changes |= GTK_CSS_CHANGE_ANY;

  if (!cssnode->invalid)
    return;

  gtk_css_node_ensure_style (cssnode, timestamp);

  /* need to set to FALSE then to TRUE here to make it chain up */
  gtk_css_node_set_invalid (cssnode, FALSE);
  if (!gtk_css_style_is_static (cssnode->style))
    gtk_css_node_set_invalid (cssnode, TRUE);

  GTK_CSS_NODE_GET_CLASS (cssnode)->validate (cssnode);

  for (child = gtk_css_node_get_first_child (cssnode);
       child;
       child = gtk_css_node_get_next_sibling (child))
    {
      if (child->visible)
        gtk_css_node_validate_internal (child, timestamp);
    }
}

void
gtk_css_node_validate (GtkCssNode *cssnode)
{
  gint64 timestamp;

  timestamp = gtk_css_node_get_timestamp (cssnode);

  gtk_css_node_validate_internal (cssnode, timestamp);
}

gboolean
gtk_css_node_init_matcher (GtkCssNode     *cssnode,
                           GtkCssMatcher  *matcher)
{
  return GTK_CSS_NODE_GET_CLASS (cssnode)->init_matcher (cssnode, matcher);
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

GtkStyleProviderPrivate *
gtk_css_node_get_style_provider (GtkCssNode *cssnode)
{
  GtkStyleProviderPrivate *result;
  
  result = gtk_css_node_get_style_provider_or_null (cssnode);
  if (result)
    return result;

  if (cssnode->parent)
    return gtk_css_node_get_style_provider (cssnode->parent);

  return GTK_STYLE_PROVIDER_PRIVATE (_gtk_settings_get_style_cascade (gtk_settings_get_default (), 1));
}
