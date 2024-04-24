/* gtkaccesskitcontext.c: AccessKit GtkATContext implementation
 *
 * Copyright 2024  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkaccesskitcontextprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtkaccesskitrootprivate.h"

#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtkeditable.h"
#include "gtkentryprivate.h"
#include "gtkpasswordentry.h"
#include "gtkroot.h"
#include "gtkscrolledwindow.h"
#include "gtkstack.h"
#include "gtktextview.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"

#include <locale.h>

struct _GtkAccessKitContext
{
  GtkATContext parent_instance;

  /* Root object for the surface */
  GtkAccessKitRoot *root;

  /* The AccessKit node ID; meaningless if root is null. Note that this ID
     is not a full 64-bit AccessKit node ID, for two reasons:
     1. It isn't possible to convert 64-bit integers to pointers
        (as required to use these IDs as GHashTable keys) on all platforms.
     2. By using only 32 bits for the IDs of AT contexts, we can use the other
        32 bits to identify inline text boxes or other children within
        a given context. */
  guint32 id;
};

G_DEFINE_TYPE (GtkAccessKitContext, gtk_accesskit_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_accesskit_context_finalize (GObject *gobject)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (gobject);

  g_clear_object (&self->root);

  G_OBJECT_CLASS (gtk_accesskit_context_parent_class)->finalize (gobject);
}

static void
gtk_accesskit_context_realize (GtkATContext *context)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (context);
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);

  if (GTK_IS_ROOT (accessible))
    self->root = gtk_accesskit_root_new (GTK_ROOT (accessible));
  else
    {
      GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);
      GtkATContext *parent_ctx = gtk_accessible_get_at_context (parent);
      GtkAccessKitContext *parent_accesskit_ctx = GTK_ACCESSKIT_CONTEXT (parent_ctx);
      gtk_at_context_realize (parent_ctx);
      self->root = g_object_ref (parent_accesskit_ctx->root);
      g_object_unref (parent_ctx);
      g_object_unref (parent);
    }

  self->id = gtk_accesskit_root_add_context (self->root, self);
}

static void
gtk_accesskit_context_unrealize (GtkATContext *context)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (context);
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);

  GTK_DEBUG (A11Y, "Unrealizing AccessKit context for accessible '%s'",
                   G_OBJECT_TYPE_NAME (accessible));

  gtk_accesskit_root_remove_context (self->root, self->id);

  g_clear_object (&self->root);
}

static void
queue_update (GtkAccessKitContext *self)
{
  if (!self->root)
    return;

  gtk_accesskit_root_queue_update (self->root, self->id);
}

static void
gtk_accesskit_context_state_change (GtkATContext                *ctx,
                                    GtkAccessibleStateChange     changed_states,
                                    GtkAccessiblePropertyChange  changed_properties,
                                    GtkAccessibleRelationChange  changed_relations,
                                    GtkAccessibleAttributeSet   *states,
                                    GtkAccessibleAttributeSet   *properties,
                                    GtkAccessibleAttributeSet   *relations)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  queue_update (self);
}

static void
gtk_accesskit_context_platform_change (GtkATContext                *ctx,
                                       GtkAccessiblePlatformChange  changed_platform)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  queue_update (self);
}

static void
gtk_accesskit_context_bounds_change (GtkATContext *ctx)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  queue_update (self);
}

static void
gtk_accesskit_context_child_change (GtkATContext             *ctx,
                                    GtkAccessibleChildChange  change,
                                    GtkAccessible            *child)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  queue_update (self);
}

static void
gtk_accesskit_context_announce (GtkATContext                      *context,
                                const char                        *message,
                                GtkAccessibleAnnouncementPriority  priority)
{
  /* TODO */
}

static void
gtk_accesskit_context_update_caret_position (GtkATContext *ctx)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  queue_update (self);
}

static void
gtk_accesskit_context_update_selection_bound (GtkATContext *ctx)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  queue_update (self);
}

static void
gtk_accesskit_context_update_text_contents (GtkATContext *ctx,
                                            GtkAccessibleTextContentChange change,
                                            unsigned int start,
                                            unsigned int end)
{
  /* TODO */
}

static void
gtk_accesskit_context_class_init (GtkAccessKitContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  gobject_class->finalize = gtk_accesskit_context_finalize;

  context_class->realize = gtk_accesskit_context_realize;
  context_class->unrealize = gtk_accesskit_context_unrealize;
  context_class->state_change = gtk_accesskit_context_state_change;
  context_class->platform_change = gtk_accesskit_context_platform_change;
  context_class->bounds_change = gtk_accesskit_context_bounds_change;
  context_class->child_change = gtk_accesskit_context_child_change;
  context_class->announce = gtk_accesskit_context_announce;
  context_class->update_caret_position = gtk_accesskit_context_update_caret_position;
  context_class->update_selection_bound = gtk_accesskit_context_update_selection_bound;
  context_class->update_text_contents = gtk_accesskit_context_update_text_contents;
}

static void
gtk_accesskit_context_init (GtkAccessKitContext *self)
{
}

GtkATContext *
gtk_accesskit_create_context (GtkAccessibleRole  accessible_role,
                              GtkAccessible     *accessible,
                              GdkDisplay        *display)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return g_object_new (GTK_TYPE_ACCESSKIT_CONTEXT,
                       "accessible-role", accessible_role,
                       "accessible", accessible,
                       "display", display,
                       NULL);
}

static accesskit_role
gtk_accessible_role_to_accesskit_role (GtkAccessibleRole role)
{
  switch (role)
    {
    case GTK_ACCESSIBLE_ROLE_ALERT:
      return ACCESSKIT_ROLE_ALERT;

    case GTK_ACCESSIBLE_ROLE_ALERT_DIALOG:
      return ACCESSKIT_ROLE_ALERT_DIALOG;

    case GTK_ACCESSIBLE_ROLE_APPLICATION:
      return ACCESSKIT_ROLE_WINDOW;

    case GTK_ACCESSIBLE_ROLE_ARTICLE:
      return ACCESSKIT_ROLE_ARTICLE;

    case GTK_ACCESSIBLE_ROLE_BANNER:
      return ACCESSKIT_ROLE_BANNER;

    case GTK_ACCESSIBLE_ROLE_BLOCK_QUOTE:
      return ACCESSKIT_ROLE_BLOCKQUOTE;

    case GTK_ACCESSIBLE_ROLE_BUTTON:
      return ACCESSKIT_ROLE_BUTTON;

    case GTK_ACCESSIBLE_ROLE_CAPTION:
      return ACCESSKIT_ROLE_CAPTION;

    case GTK_ACCESSIBLE_ROLE_CELL:
      return ACCESSKIT_ROLE_CELL;

    case GTK_ACCESSIBLE_ROLE_CHECKBOX:
      return ACCESSKIT_ROLE_CHECK_BOX;

    case GTK_ACCESSIBLE_ROLE_COLUMN_HEADER:
      return ACCESSKIT_ROLE_COLUMN_HEADER;

    case GTK_ACCESSIBLE_ROLE_COMBO_BOX:
      return ACCESSKIT_ROLE_COMBO_BOX;

    case GTK_ACCESSIBLE_ROLE_COMMAND:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_COMMENT:
      return ACCESSKIT_ROLE_COMMENT;

    case GTK_ACCESSIBLE_ROLE_COMPOSITE:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_DIALOG:
      return ACCESSKIT_ROLE_DIALOG;

    case GTK_ACCESSIBLE_ROLE_DOCUMENT:
      return ACCESSKIT_ROLE_DOCUMENT;

    case GTK_ACCESSIBLE_ROLE_FEED:
      return ACCESSKIT_ROLE_FEED;

    case GTK_ACCESSIBLE_ROLE_FORM:
      return ACCESSKIT_ROLE_FORM;

    case GTK_ACCESSIBLE_ROLE_GENERIC:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_GRID:
      return ACCESSKIT_ROLE_GRID;

    case GTK_ACCESSIBLE_ROLE_GRID_CELL:
      return ACCESSKIT_ROLE_CELL;

    case GTK_ACCESSIBLE_ROLE_GROUP:
      return ACCESSKIT_ROLE_GROUP;

    case GTK_ACCESSIBLE_ROLE_HEADING:
      return ACCESSKIT_ROLE_HEADING;

    case GTK_ACCESSIBLE_ROLE_IMG:
      return ACCESSKIT_ROLE_IMAGE;

    case GTK_ACCESSIBLE_ROLE_INPUT:
      return ACCESSKIT_ROLE_TEXT_INPUT;

    case GTK_ACCESSIBLE_ROLE_LABEL:
      return ACCESSKIT_ROLE_STATIC_TEXT;

    case GTK_ACCESSIBLE_ROLE_LANDMARK:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_LEGEND:
      return ACCESSKIT_ROLE_LEGEND;

    case GTK_ACCESSIBLE_ROLE_LINK:
      return ACCESSKIT_ROLE_LINK;

    case GTK_ACCESSIBLE_ROLE_LIST:
      return ACCESSKIT_ROLE_LIST;

    case GTK_ACCESSIBLE_ROLE_LIST_BOX:
      return ACCESSKIT_ROLE_LIST_BOX;

    case GTK_ACCESSIBLE_ROLE_LIST_ITEM:
      return ACCESSKIT_ROLE_LIST_ITEM;

    case GTK_ACCESSIBLE_ROLE_LOG:
      return ACCESSKIT_ROLE_LOG;

    case GTK_ACCESSIBLE_ROLE_MAIN:
      return ACCESSKIT_ROLE_MAIN;

    case GTK_ACCESSIBLE_ROLE_MARQUEE:
      return ACCESSKIT_ROLE_MARQUEE;

    case GTK_ACCESSIBLE_ROLE_MATH:
      return ACCESSKIT_ROLE_MATH;

    case GTK_ACCESSIBLE_ROLE_METER:
      return ACCESSKIT_ROLE_METER;

    case GTK_ACCESSIBLE_ROLE_MENU:
      return ACCESSKIT_ROLE_MENU;

    case GTK_ACCESSIBLE_ROLE_MENU_BAR:
      return ACCESSKIT_ROLE_MENU_BAR;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM:
      return ACCESSKIT_ROLE_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX:
      return ACCESSKIT_ROLE_MENU_ITEM_CHECK_BOX;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO:
      return ACCESSKIT_ROLE_MENU_ITEM_RADIO;

    case GTK_ACCESSIBLE_ROLE_NAVIGATION:
      return ACCESSKIT_ROLE_NAVIGATION;

    case GTK_ACCESSIBLE_ROLE_NONE:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_NOTE:
      return ACCESSKIT_ROLE_NOTE;

    case GTK_ACCESSIBLE_ROLE_OPTION:
      return ACCESSKIT_ROLE_LIST_BOX_OPTION;

    case GTK_ACCESSIBLE_ROLE_PARAGRAPH:
      return ACCESSKIT_ROLE_PARAGRAPH;

    case GTK_ACCESSIBLE_ROLE_PRESENTATION:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_PROGRESS_BAR:
      return ACCESSKIT_ROLE_PROGRESS_INDICATOR;

    case GTK_ACCESSIBLE_ROLE_RADIO:
      return ACCESSKIT_ROLE_RADIO_BUTTON;

    case GTK_ACCESSIBLE_ROLE_RADIO_GROUP:
      return ACCESSKIT_ROLE_RADIO_GROUP;

    case GTK_ACCESSIBLE_ROLE_RANGE:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_REGION:
      return ACCESSKIT_ROLE_REGION;

    case GTK_ACCESSIBLE_ROLE_ROW:
      return ACCESSKIT_ROLE_ROW;

    case GTK_ACCESSIBLE_ROLE_ROW_GROUP:
      return ACCESSKIT_ROLE_ROW_GROUP;

    case GTK_ACCESSIBLE_ROLE_ROW_HEADER:
      return ACCESSKIT_ROLE_ROW_HEADER;

    case GTK_ACCESSIBLE_ROLE_SCROLLBAR:
      return ACCESSKIT_ROLE_SCROLL_BAR;

    case GTK_ACCESSIBLE_ROLE_SEARCH:
      return ACCESSKIT_ROLE_SEARCH;

    case GTK_ACCESSIBLE_ROLE_SEARCH_BOX:
      return ACCESSKIT_ROLE_SEARCH_INPUT;

    case GTK_ACCESSIBLE_ROLE_SECTION:
      return ACCESSKIT_ROLE_SECTION;

    case GTK_ACCESSIBLE_ROLE_SECTION_HEAD:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_SELECT:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_SEPARATOR:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_SLIDER:
      return ACCESSKIT_ROLE_SLIDER;

    case GTK_ACCESSIBLE_ROLE_SPIN_BUTTON:
      return ACCESSKIT_ROLE_SPIN_BUTTON;

    case GTK_ACCESSIBLE_ROLE_STATUS:
      return ACCESSKIT_ROLE_STATUS;

    case GTK_ACCESSIBLE_ROLE_STRUCTURE:
      return ACCESSKIT_ROLE_GENERIC_CONTAINER;

    case GTK_ACCESSIBLE_ROLE_SWITCH:
      return ACCESSKIT_ROLE_SWITCH;

    case GTK_ACCESSIBLE_ROLE_TAB:
      return ACCESSKIT_ROLE_TAB;

    case GTK_ACCESSIBLE_ROLE_TABLE:
      return ACCESSKIT_ROLE_TABLE;

    case GTK_ACCESSIBLE_ROLE_TAB_LIST:
      return ACCESSKIT_ROLE_TAB_LIST;

    case GTK_ACCESSIBLE_ROLE_TAB_PANEL:
      return ACCESSKIT_ROLE_TAB_PANEL;

    case GTK_ACCESSIBLE_ROLE_TEXT_BOX:
      return ACCESSKIT_ROLE_TEXT_INPUT;

    case GTK_ACCESSIBLE_ROLE_TIME:
      return ACCESSKIT_ROLE_TIME_INPUT;

    case GTK_ACCESSIBLE_ROLE_TIMER:
      return ACCESSKIT_ROLE_TIMER;

    case GTK_ACCESSIBLE_ROLE_TOOLBAR:
      return ACCESSKIT_ROLE_TOOLBAR;

    case GTK_ACCESSIBLE_ROLE_TOOLTIP:
      return ACCESSKIT_ROLE_TOOLTIP;

    case GTK_ACCESSIBLE_ROLE_TREE:
      return ACCESSKIT_ROLE_TREE;

    case GTK_ACCESSIBLE_ROLE_TREE_GRID:
      return ACCESSKIT_ROLE_TREE_GRID;

    case GTK_ACCESSIBLE_ROLE_TREE_ITEM:
      return ACCESSKIT_ROLE_TREE_ITEM;

    case GTK_ACCESSIBLE_ROLE_WIDGET:
      return ACCESSKIT_ROLE_UNKNOWN;

    case GTK_ACCESSIBLE_ROLE_WINDOW:
      return ACCESSKIT_ROLE_WINDOW;

    case GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON:
      return ACCESSKIT_ROLE_TOGGLE_BUTTON;

    case GTK_ACCESSIBLE_ROLE_TERMINAL:
      return ACCESSKIT_ROLE_TERMINAL;

    default:
      break;
    }

  return ACCESSKIT_ROLE_UNKNOWN;
}

static accesskit_role
accesskit_role_for_context (GtkATContext *context)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);
  GtkAccessibleRole role = gtk_at_context_get_accessible_role (context);

  /* ARIA does not have a "password entry" role, so we need to fudge it here */
  if (GTK_IS_PASSWORD_ENTRY (accessible))
    return ACCESSKIT_ROLE_PASSWORD_INPUT;

  /* ARIA does not have a "scroll area" role */
  if (GTK_IS_SCROLLED_WINDOW (accessible))
    return ACCESSKIT_ROLE_SCROLL_VIEW;

  return gtk_accessible_role_to_accesskit_role (role);
}

guint32
gtk_accesskit_context_get_id (GtkAccessKitContext *self)
{
  g_assert (self->root);
  return self->id;
}

typedef void (*AccessKitStringSetter) (accesskit_node_builder *, const char *);

static gboolean
set_string_property (GtkATContext           *ctx,
                     GtkAccessibleProperty   property,
                     AccessKitStringSetter   setter,
                     accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_property (ctx, property))
    {
      GtkAccessibleValue *value;
      const char *str;

      value = gtk_at_context_get_accessible_property (ctx, property);
      str = gtk_string_accessible_value_get (value);
      if (str)
        {
          setter (builder, str);
          return TRUE;
        }
    }

  return FALSE;
}

typedef void (*AccessKitNodeIdSetter) (accesskit_node_builder *, accesskit_node_id);

static gboolean
set_single_relation (GtkATContext           *ctx,
                    GtkAccessibleRelation   relation,
                    AccessKitNodeIdSetter   setter,
                    accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_relation (ctx, relation))
    {
      GtkAccessibleValue *value;
      GtkAccessible *target;

      value = gtk_at_context_get_accessible_relation (ctx, relation);
      target = gtk_reference_accessible_value_get (value);

      if (target)
        {
          GtkATContext *target_ctx = gtk_accessible_get_at_context (target);

          gtk_at_context_realize (target_ctx);
          setter (builder, GTK_ACCESSKIT_CONTEXT (target_ctx)->id);
          g_object_unref (target_ctx);

          return TRUE;
        }
    }

  return FALSE;
}

typedef void (*AccessKitNodeIdPusher) (accesskit_node_builder *, accesskit_node_id);

static gboolean
set_multi_relation (GtkATContext           *ctx,
                    GtkAccessibleRelation   relation,
                    AccessKitNodeIdPusher   pusher,
                    accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_relation (ctx, relation))
    {
      GtkAccessibleValue *value;
      GList *l;
      gboolean has_target;

      value = gtk_at_context_get_accessible_relation (ctx, relation);
      l = gtk_reference_list_accessible_value_get (value);
      has_target = (l != NULL);

      for (; l; l = l->next)
        {
          GtkATContext *target_ctx =
            gtk_accessible_get_at_context (GTK_ACCESSIBLE (l->data));

          gtk_at_context_realize (target_ctx);
          pusher (builder, GTK_ACCESSKIT_CONTEXT (target_ctx)->id);
          g_object_unref (target_ctx);
        }

      return has_target;
    }

  return FALSE;
}

accesskit_node *
gtk_accesskit_context_build_node (GtkAccessKitContext      *self,
                                  accesskit_node_class_set *node_classes)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  accesskit_role role = accesskit_role_for_context (ctx);
  accesskit_node_builder *builder = accesskit_node_builder_new (role);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  int x, y, width, height;
  GtkAccessible *child = gtk_accessible_get_first_accessible_child (accessible);

  if (gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    {
      accesskit_vec2 p = {x, y};
      accesskit_affine transform = accesskit_affine_translate (p);
      accesskit_rect bounds = {0, 0, width, height};
      accesskit_node_builder_set_transform (builder, transform);
      accesskit_node_builder_set_bounds (builder, bounds);
    }

  while (child)
    {
      GtkATContext *child_ctx = gtk_accessible_get_at_context (child);
      GtkAccessKitContext *child_accesskit_ctx = GTK_ACCESSKIT_CONTEXT (child_ctx);
      GtkAccessible *next = gtk_accessible_get_next_accessible_sibling (child);

      gtk_at_context_realize (child_ctx);
      accesskit_node_builder_push_child (builder, child_accesskit_ctx->id);
      g_object_unref (child_ctx);
      g_object_unref (child);
      child = next;
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_HIDDEN))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_HIDDEN);
      if (gtk_boolean_accessible_value_get (value))
        accesskit_node_builder_set_hidden (builder);
    }

  set_string_property (ctx, GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                       accesskit_node_builder_set_description, builder);
  set_string_property (ctx, GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS,
                       accesskit_node_builder_set_keyboard_shortcut, builder);
  set_string_property (ctx, GTK_ACCESSIBLE_PROPERTY_LABEL,
                       accesskit_node_builder_set_name, builder);
  set_string_property (ctx, GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER,
                       accesskit_node_builder_set_placeholder, builder);
  set_string_property (ctx, GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION,
                       accesskit_node_builder_set_role_description, builder);
  set_string_property (ctx, GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT,
                       accesskit_node_builder_set_value, builder);

  set_single_relation (ctx, GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT,
                       accesskit_node_builder_set_active_descendant, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_CONTROLS,
                      accesskit_node_builder_push_controlled, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                      accesskit_node_builder_push_described_by, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_DETAILS,
                      accesskit_node_builder_push_detail, builder);
  set_single_relation (ctx, GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE,
                       accesskit_node_builder_set_error_message, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_FLOW_TO,
                      accesskit_node_builder_push_flow_to, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_LABELLED_BY,
                      accesskit_node_builder_push_labelled_by, builder);

  accesskit_node_builder_set_class_name (builder,
                                         g_type_name (G_TYPE_FROM_INSTANCE (accessible)));

  /* TODO: properties */

  return accesskit_node_builder_build (builder, node_classes);
}
