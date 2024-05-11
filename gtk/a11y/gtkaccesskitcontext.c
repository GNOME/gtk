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
#include "gtkinscriptionprivate.h"
#include "gtklabelprivate.h"
#include "gtkpasswordentry.h"
#include "gtkroot.h"
#include "gtkscrolledwindow.h"
#include "gtkstack.h"
#include "gtktextview.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"

#include "gtkmenubutton.h"
#include "gtkcolordialogbutton.h"
#include "gtkfontdialogbutton.h"
#include "gtkscalebutton.h"
#include "print/gtkprinteroptionwidgetprivate.h"

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

  double text_layout_x;
  double text_layout_y;
  guint text_layout_serial;
  GArray *text_layout_children;
};

G_DEFINE_TYPE (GtkAccessKitContext, gtk_accesskit_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_accesskit_context_finalize (GObject *gobject)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (gobject);

  g_clear_object (&self->root);
  g_clear_pointer (&self->text_layout_children, g_array_unref);

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

/* Adapted from gtkatspitext.c */
static GtkText *
gtk_editable_get_text_widget (GtkEditable *editable)
{
  guint redirects = 0;

  do {
    if (GTK_IS_TEXT (editable))
      return GTK_TEXT (editable);

    if (++redirects >= 6)
      g_assert_not_reached ();

    editable = gtk_editable_get_delegate (editable);
  } while (editable != NULL);

  return NULL;
}

static void
gtk_accesskit_context_update_text_contents (GtkATContext *ctx,
                                            GtkAccessibleTextContentChange change,
                                            unsigned int start,
                                            unsigned int end)
{
  GtkAccessKitContext *self = GTK_ACCESSKIT_CONTEXT (ctx);

  /* Force layout invalidation here. This is necessary because GtkText
     keeps recreating the layout, so the serial number ends up the same
     whenever text changes. */
  self->text_layout_serial = 0;

  queue_update (self);

  /* TODO? */
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
      return ACCESSKIT_ROLE_BUTTON;

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
  accesskit_role accesskit_role;

  if (GTK_IS_MENU_BUTTON (accessible) ||
      GTK_IS_COLOR_DIALOG_BUTTON (accessible) ||
      GTK_IS_FONT_DIALOG_BUTTON (accessible) ||
      GTK_IS_SCALE_BUTTON (accessible)
#ifdef G_OS_UNIX
      || GTK_IS_PRINTER_OPTION_WIDGET (accessible))
#endif
    )
    return ACCESSKIT_ROLE_GENERIC_CONTAINER;

  /* ARIA does not have a "password entry" role, so we need to fudge it here */
  if (GTK_IS_PASSWORD_ENTRY (accessible))
    return ACCESSKIT_ROLE_PASSWORD_INPUT;

  /* ARIA does not have a "scroll area" role */
  if (GTK_IS_SCROLLED_WINDOW (accessible))
    return ACCESSKIT_ROLE_SCROLL_VIEW;

  accesskit_role = gtk_accessible_role_to_accesskit_role (role);

  if (accesskit_role == ACCESSKIT_ROLE_TEXT_INPUT &&
      gtk_at_context_has_accessible_property (context, GTK_ACCESSIBLE_PROPERTY_MULTI_LINE))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (context,
                                                      GTK_ACCESSIBLE_PROPERTY_MULTI_LINE);
      if (gtk_boolean_accessible_value_get (value))
        accesskit_role = ACCESSKIT_ROLE_MULTILINE_TEXT_INPUT;
    }

  return accesskit_role;
}

guint32
gtk_accesskit_context_get_id (GtkAccessKitContext *self)
{
  g_assert (self->root);
  return self->id;
}

typedef void (*AccessKitFlagSetter) (accesskit_node_builder *);

static gboolean
set_flag_from_state (GtkATContext           *ctx,
                     GtkAccessibleState      state,
                     AccessKitFlagSetter     setter,
                     accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_state (ctx, state))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_state (ctx, state);
      if (gtk_boolean_accessible_value_get (value))
        {
          setter (builder);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
set_flag_from_property (GtkATContext           *ctx,
                        GtkAccessibleProperty   property,
                        AccessKitFlagSetter     setter,
                        accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_property (ctx, property))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, property);
      if (gtk_boolean_accessible_value_get (value))
        {
          setter (builder);
          return TRUE;
        }
    }

  return FALSE;
}

typedef void (*AccessKitOptionalFlagSetter) (accesskit_node_builder *, bool);

static gboolean
set_optional_flag_from_state (GtkATContext               *ctx,
                              GtkAccessibleState          state,
                              AccessKitOptionalFlagSetter setter,
                              accesskit_node_builder     *builder)
{
  if (gtk_at_context_has_accessible_state (ctx, state))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_state (ctx, state);
      setter (builder, gtk_boolean_accessible_value_get (value));
      return TRUE;
    }

  return FALSE;
}

static gboolean
set_toggled (GtkATContext           *ctx,
             GtkAccessibleState      state,
             accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_state (ctx, state))
    {
      GtkAccessibleValue *value;
      accesskit_toggled toggled;

      value = gtk_at_context_get_accessible_state (ctx, state);

      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
          toggled = ACCESSKIT_TOGGLED_FALSE;
          break;

        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          toggled = ACCESSKIT_TOGGLED_TRUE;
          break;

        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          toggled = ACCESSKIT_TOGGLED_MIXED;
          break;
        }

      accesskit_node_builder_set_toggled (builder, toggled);
      return TRUE;
    }

  return FALSE;
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

static gboolean
set_string_from_relation (GtkATContext           *ctx,
                          GtkAccessibleRelation   relation,
                          AccessKitStringSetter   setter,
                          accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_relation (ctx, relation))
    {
      GtkAccessibleValue *value;
      const char *str;

      value = gtk_at_context_get_accessible_relation (ctx, relation);
      str = gtk_string_accessible_value_get (value);
      if (str)
        {
          setter (builder, str);
          return TRUE;
        }
    }

  return FALSE;
}

typedef void (*AccessKitSizeSetter) (accesskit_node_builder *, size_t);

static gboolean
set_size_from_property (GtkATContext           *ctx,
                        GtkAccessibleProperty   property,
                        AccessKitSizeSetter     setter,
                        accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_property (ctx, property))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, property);
      setter (builder, gtk_int_accessible_value_get (value));
      return TRUE;
    }

  return FALSE;
}

static gboolean
set_size_from_relation (GtkATContext           *ctx,
                        GtkAccessibleRelation   relation,
                        AccessKitSizeSetter     setter,
                        accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_relation (ctx, relation))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_relation (ctx, relation);
      setter (builder, gtk_int_accessible_value_get (value));
      return TRUE;
    }

  return FALSE;
}

typedef void (*AccessKitDoubleSetter) (accesskit_node_builder *, double);

static gboolean
set_double_property (GtkATContext           *ctx,
                     GtkAccessibleProperty   property,
                     AccessKitDoubleSetter   setter,
                     accesskit_node_builder *builder)
{
  if (gtk_at_context_has_accessible_property (ctx, property))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, property);
      setter (builder, gtk_number_accessible_value_get (value));
      return TRUE;
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

static void
set_bounds_from_pango (accesskit_node_builder *builder,
                       double                 base_x,
                       double                 base_y,
                       PangoRectangle         *pango_rect)
{
  accesskit_rect rect;

  rect.x0 = base_x + (double)(pango_rect->x) / PANGO_SCALE;
  rect.y0 = base_y + (double)(pango_rect->y) / PANGO_SCALE;
  rect.x1 = base_x + (double)(pango_rect->x + pango_rect->width) / PANGO_SCALE;
  rect.y1 = base_y + (double)(pango_rect->y + pango_rect->height) / PANGO_SCALE;
  accesskit_node_builder_set_bounds (builder, rect);
}

static accesskit_node_id
run_node_id (GtkAccessKitContext *self,
             gint                 start_index)
{
  return ((accesskit_node_id)self->id << 32) | start_index;
}

static void
add_run_node (GtkAccessKitContext    *self,
              accesskit_tree_update  *update,
              gint                    start_index,
              accesskit_node_builder *builder)
{
  accesskit_node_id id = run_node_id (self, start_index);
  accesskit_node *node = accesskit_node_builder_build (builder);

  accesskit_tree_update_push_node (update, id, node);
  g_array_append_val (self->text_layout_children, id);
}

typedef struct _GtkAccessKitRunInfo
{
  PangoLayoutRun *run;
  PangoRectangle extents;
} GtkAccessKitRunInfo;

static int
compare_run_info (gconstpointer a, gconstpointer b)
{
  const GtkAccessKitRunInfo *run_info_a = a;
  const GtkAccessKitRunInfo *run_info_b = b;
  int a_offset = run_info_a->run->item->offset;
  int b_offset = run_info_b->run->item->offset;

  if (a_offset < b_offset)
    return -1;
  if (a_offset > b_offset)
    return 1;
  return 0;
}

static void
add_text_layout_inner (GtkAccessKitContext   *self,
                       accesskit_tree_update *update,
                       PangoLayout           *layout,
                       double                 base_x,
                       double                 base_y)
{
  const char *text = pango_layout_get_text (layout);
  const PangoLogAttr *log_attrs =
    pango_layout_get_log_attrs_readonly (layout, NULL);
  PangoLayoutIter *iter = pango_layout_get_iter (layout);
  GArray *line_runs = NULL;
  guint usv_offset = 0, byte_offset = 0;

  do
    {
      PangoLayoutRun *run = pango_layout_iter_get_run_readonly (iter);

      if (run)
        {
          GtkAccessKitRunInfo run_info;

          run_info.run = run;
          pango_layout_iter_get_run_extents (iter, NULL, &run_info.extents);

          if (!line_runs)
            line_runs = g_array_new (FALSE, FALSE, sizeof (GtkAccessKitRunInfo));
          g_array_append_val (line_runs, run_info);
        }
      else
        {
          PangoLayoutLine *line = pango_layout_iter_get_line_readonly (iter);

          if (line_runs)
            {
              GtkAccessKitRunInfo *line_runs_data =
                (GtkAccessKitRunInfo *) (line_runs->data);
              guint prev_run_usv_offset = 0;
              guint i;

              g_array_sort (line_runs, compare_run_info);

              for (i = 0; i < line_runs->len; i++)
                {
                  GtkAccessKitRunInfo *run_info = &line_runs_data[i];
                  PangoLayoutRun *run = run_info->run;
                  PangoItem *item = run->item;
                  accesskit_node_builder *builder =
                    accesskit_node_builder_new (ACCESSKIT_ROLE_INLINE_TEXT_BOX);
                  guint node_text_byte_count;
                  gchar *node_text;
                  accesskit_text_direction dir;
                  int *log_widths = g_new0 (int, item->num_chars);
                  GArray *char_lengths =
                    g_array_new (FALSE, FALSE, sizeof (uint8_t));
                  GArray *word_lengths =
                    g_array_new (FALSE, FALSE, sizeof (uint8_t));
                  GArray *char_positions =
                    g_array_new (FALSE, FALSE, sizeof (float));
                  GArray *char_widths =
                    g_array_new (FALSE, FALSE, sizeof (float));
                  guint run_start_usv_offset = usv_offset;
                  guint last_word_start_char_offset = 0;
                  guint char_count = 0;
                  float char_pos = 0.0f;

                  g_assert (byte_offset == item->offset);

                  if (i > 0)
                    {
                      accesskit_node_id id =
                        run_node_id (self, prev_run_usv_offset);
                      accesskit_node_builder_set_previous_on_line (builder, id);
                    }

                  set_bounds_from_pango (builder, base_x, base_y,
                                         &run_info->extents);

                  if (i == (line_runs->len - 1))
                    node_text_byte_count =
                      line->length - (item->offset - line->start_index);
                  else
                    node_text_byte_count = item->length;
                  node_text = g_strndup (text + item->offset,
                                         node_text_byte_count);
                  accesskit_node_builder_set_value (builder, node_text);

                  /* The following logic for determining the run's direction
                     is copied from update_run in pango-layout.c. */
                  if ((item->analysis.level % 2) == 0)
                    dir = ACCESSKIT_TEXT_DIRECTION_LEFT_TO_RIGHT;
                  else
                    dir = ACCESSKIT_TEXT_DIRECTION_RIGHT_TO_LEFT;
                  accesskit_node_builder_set_text_direction (builder, dir);

                  /* TODO: attributes, once the AccessKit backends support them */

                  pango_glyph_item_get_logical_widths (run, text, log_widths);

                  while (byte_offset < (item->offset + node_text_byte_count))
                    {
                      guint char_start_byte_offset = byte_offset;
                      uint8_t char_len;

                      if (log_attrs[usv_offset].is_word_start &&
                          (char_count > last_word_start_char_offset))
                        {
                          uint8_t word_len =
                            char_count - last_word_start_char_offset;

                          g_array_append_val (word_lengths, word_len);
                          last_word_start_char_offset = char_count;
                        }

                      if (byte_offset >= (item->offset + item->length))
                        {
                          float width = 0.0f;

                          byte_offset =
                            item->offset + node_text_byte_count;
                          usv_offset +=
                            g_utf8_strlen (node_text + item->length,
                                           node_text_byte_count - item->length);
                          g_array_append_val (char_positions, char_pos);
                          g_array_append_val (char_widths, width);
                        }
                      else
                        {
                          float width = 0.0f;

                          do
                            {
                              width +=
                                (float) (log_widths[usv_offset - run_start_usv_offset]) / PANGO_SCALE;
                              byte_offset =
                                g_utf8_next_char (text + byte_offset) - text;
                              usv_offset++;
                            }
                          while (byte_offset < (item->offset + item->length) &&
                                 !log_attrs[usv_offset].is_cursor_position);

                          g_array_append_val (char_positions, char_pos);
                          g_array_append_val (char_widths, width);
                          char_pos += width;
                        }

                      char_len = byte_offset - char_start_byte_offset;
                      g_array_append_val (char_lengths, char_len);
                      char_count++;
                    }

                  if (char_count > last_word_start_char_offset)
                    {
                      uint8_t word_len =
                        char_count - last_word_start_char_offset;

                      g_array_append_val (word_lengths, word_len);
                    }

                  accesskit_node_builder_set_character_lengths (builder,
                                                                char_lengths->len,
                                                                (uint8_t *) char_lengths->data);
                  g_array_unref (char_lengths);
                  accesskit_node_builder_set_word_lengths (builder,
                                                           word_lengths->len,
                                                           (uint8_t *) word_lengths->data);
                  g_array_unref (word_lengths);
                  accesskit_node_builder_set_character_positions (builder,
                                                                  char_positions->len,
                                                                  (float *) char_positions->data);
                  g_array_unref (char_positions);
                  accesskit_node_builder_set_character_widths (builder,
                                                               char_widths->len,
                                                               (float *) char_widths->data);
                  g_array_unref (char_widths);

                  if (i < (line_runs->len - 1))
                    {
                      accesskit_node_id id = run_node_id (self, usv_offset);
                      accesskit_node_builder_set_next_on_line (builder, id);
                    }

                  add_run_node (self, update, run_start_usv_offset, builder);
                  prev_run_usv_offset = run_start_usv_offset;
                  g_free (node_text);
                  g_free (log_widths);
                }

              g_clear_pointer (&line_runs, g_array_unref);
            }
          else
            {
              accesskit_node_builder *builder =
                accesskit_node_builder_new (ACCESSKIT_ROLE_INLINE_TEXT_BOX);
              PangoRectangle extents;
              gchar *line_text =
                g_strndup (text + line->start_index, line->length);
              accesskit_text_direction dir;
              uint8_t char_len = line->length;
              uint8_t char_count = line->length ? 1 : 0;
              float coord = 0.0f;

              g_assert (byte_offset == line->start_index);

              pango_layout_iter_get_run_extents (iter, NULL, &extents);
              set_bounds_from_pango (builder, base_x, base_y, &extents);
              accesskit_node_builder_set_value (builder, line_text);

              switch (pango_layout_line_get_resolved_direction (line))
                {
                case PANGO_DIRECTION_RTL:
                case PANGO_DIRECTION_TTB_RTL:
                case PANGO_DIRECTION_WEAK_RTL:
                  dir = ACCESSKIT_TEXT_DIRECTION_RIGHT_TO_LEFT;
                  break;

                default:
                  dir = ACCESSKIT_TEXT_DIRECTION_LEFT_TO_RIGHT;
                  break;
                }
              accesskit_node_builder_set_text_direction (builder, dir);

              accesskit_node_builder_set_character_lengths (builder, char_count,
                                                            &char_len);
              accesskit_node_builder_set_word_lengths (builder, 1,
                                                       &char_count);
              accesskit_node_builder_set_character_positions (builder, char_count,
                                                              &coord);
              accesskit_node_builder_set_character_widths (builder, char_count,
                                                           &coord);

              add_run_node (self, update, usv_offset, builder);
              byte_offset += line->length;
              usv_offset += g_utf8_strlen (line_text, line->length);
              g_free (line_text);
            }
        }
    }
  while (pango_layout_iter_next_run (iter));

  /* Iteration should always end with a null run, and processing that null run
     should dispose of line_runs (see above). */
  g_assert (!line_runs);
}

static void
add_text_layout (GtkAccessKitContext    *self,
                 accesskit_tree_update  *update,
                 accesskit_node_builder *parent_builder,
                 PangoLayout            *layout,
                 double                  base_x,
                 double                  base_y)
{
  guint serial = pango_layout_get_serial (layout);

  if (serial != self->text_layout_serial || base_x != self->text_layout_x ||
      base_y != self->text_layout_y)
    {
      self->text_layout_serial = serial;
      self->text_layout_x = base_x;
      self->text_layout_y = base_y;
      g_clear_pointer (&self->text_layout_children, g_array_unref);
      self->text_layout_children =
        g_array_new (FALSE, FALSE, sizeof (accesskit_node_id));
      add_text_layout_inner (self, update, layout, base_x, base_y);
    }

  accesskit_node_builder_set_children (parent_builder,
                                       self->text_layout_children->len,
                                       (accesskit_node_id *)
                                       self->text_layout_children->data);
}

void
gtk_accesskit_context_add_to_update (GtkAccessKitContext   *self,
                                     accesskit_tree_update *update)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  accesskit_role role = accesskit_role_for_context (ctx);
  accesskit_node_builder *builder = accesskit_node_builder_new (role);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  int x, y, width, height;
  GtkAccessible *child = gtk_accessible_get_first_accessible_child (accessible);

  if (gtk_accessible_get_platform_state (accessible,
                                         GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE))
    accesskit_node_builder_add_action (builder, ACCESSKIT_ACTION_FOCUS);

  /* TODO: actions */

  if (gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    {
      accesskit_vec2 p;
      accesskit_affine transform;
      accesskit_rect bounds = {0, 0, width, height};

      if (GTK_IS_ROOT (accessible) && GTK_IS_NATIVE (accessible))
        gtk_native_get_surface_transform (GTK_NATIVE (accessible), &p.x, &p.y);
      else
        {
          p.x = x;
          p.y = y;
        }

      transform = accesskit_affine_translate (p);
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

  set_flag_from_state (ctx, GTK_ACCESSIBLE_STATE_BUSY,
                       accesskit_node_builder_set_busy, builder);
  set_flag_from_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED,
                       accesskit_node_builder_set_disabled, builder);
  set_flag_from_state (ctx, GTK_ACCESSIBLE_STATE_HIDDEN,
                       accesskit_node_builder_set_hidden, builder);
  set_flag_from_state (ctx, GTK_ACCESSIBLE_STATE_VISITED,
                       accesskit_node_builder_set_visited, builder);

  set_optional_flag_from_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED,
                                accesskit_node_builder_set_expanded, builder);
  set_optional_flag_from_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED,
                                accesskit_node_builder_set_selected, builder);

  if (!set_toggled (ctx, GTK_ACCESSIBLE_STATE_CHECKED, builder))
    set_toggled (ctx, GTK_ACCESSIBLE_STATE_PRESSED, builder);

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID);

      switch (gtk_invalid_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_INVALID_FALSE:
          break;

        case GTK_ACCESSIBLE_INVALID_TRUE:
          accesskit_node_builder_set_invalid (builder, ACCESSKIT_INVALID_TRUE);
          break;

        case GTK_ACCESSIBLE_INVALID_GRAMMAR:
          accesskit_node_builder_set_invalid (builder, ACCESSKIT_INVALID_GRAMMAR);
          break;

        case GTK_ACCESSIBLE_INVALID_SPELLING:
          accesskit_node_builder_set_invalid (builder, ACCESSKIT_INVALID_SPELLING);
          break;
        }
    }

  set_flag_from_property (ctx, GTK_ACCESSIBLE_PROPERTY_MODAL,
                          accesskit_node_builder_set_modal, builder);
  set_flag_from_property (ctx, GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE,
                          accesskit_node_builder_set_multiselectable, builder);
  set_flag_from_property (ctx, GTK_ACCESSIBLE_PROPERTY_READ_ONLY,
                          accesskit_node_builder_set_read_only, builder);
  set_flag_from_property (ctx, GTK_ACCESSIBLE_PROPERTY_REQUIRED,
                          accesskit_node_builder_set_required, builder);

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

  set_size_from_property (ctx, GTK_ACCESSIBLE_PROPERTY_LEVEL,
                          accesskit_node_builder_set_level, builder);

  set_double_property (ctx, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
                       accesskit_node_builder_set_max_numeric_value, builder);
  set_double_property (ctx, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN,
                       accesskit_node_builder_set_min_numeric_value, builder);
  set_double_property (ctx, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW,
                       accesskit_node_builder_set_numeric_value, builder);

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE);

      switch (gtk_autocomplete_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_AUTOCOMPLETE_NONE:
          break;

        case GTK_ACCESSIBLE_AUTOCOMPLETE_INLINE:
          accesskit_node_builder_set_auto_complete (builder, ACCESSKIT_AUTO_COMPLETE_INLINE);
          break;

        case GTK_ACCESSIBLE_AUTOCOMPLETE_LIST:
          accesskit_node_builder_set_auto_complete (builder, ACCESSKIT_AUTO_COMPLETE_LIST);
          break;

        case GTK_ACCESSIBLE_AUTOCOMPLETE_BOTH:
          accesskit_node_builder_set_auto_complete (builder, ACCESSKIT_AUTO_COMPLETE_BOTH);
          break;
        }
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_HAS_POPUP))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_HAS_POPUP);
      if (gtk_boolean_accessible_value_get (value))
        accesskit_node_builder_set_has_popup(builder, ACCESSKIT_HAS_POPUP_TRUE);
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_ORIENTATION))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_ORIENTATION);

      switch (gtk_orientation_accessible_value_get (value))
        {
        case GTK_ORIENTATION_HORIZONTAL:
          accesskit_node_builder_set_orientation (builder, ACCESSKIT_ORIENTATION_HORIZONTAL);
          break;

        case GTK_ORIENTATION_VERTICAL:
          accesskit_node_builder_set_orientation (builder, ACCESSKIT_ORIENTATION_VERTICAL);
          break;
        }
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_SORT))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_SORT);

      switch (gtk_sort_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_SORT_NONE:
          break;

        case GTK_ACCESSIBLE_SORT_ASCENDING:
          accesskit_node_builder_set_sort_direction (builder, ACCESSKIT_SORT_DIRECTION_ASCENDING);
          break;

        case GTK_ACCESSIBLE_SORT_DESCENDING:
          accesskit_node_builder_set_sort_direction (builder, ACCESSKIT_SORT_DIRECTION_DESCENDING);
          break;

        case GTK_ACCESSIBLE_SORT_OTHER:
          accesskit_node_builder_set_sort_direction (builder, ACCESSKIT_SORT_DIRECTION_OTHER);
          break;
        }
    }

  set_single_relation (ctx, GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT,
                       accesskit_node_builder_set_active_descendant, builder);
  set_single_relation (ctx, GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE,
                       accesskit_node_builder_set_error_message, builder);

  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_CONTROLS,
                      accesskit_node_builder_push_controlled, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                      accesskit_node_builder_push_described_by, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_DETAILS,
                      accesskit_node_builder_push_detail, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_FLOW_TO,
                      accesskit_node_builder_push_flow_to, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_LABELLED_BY,
                      accesskit_node_builder_push_labelled_by, builder);
  set_multi_relation (ctx, GTK_ACCESSIBLE_RELATION_OWNS,
                      accesskit_node_builder_push_owned, builder);

  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_COL_COUNT,
                          accesskit_node_builder_set_column_count, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_COL_INDEX,
                          accesskit_node_builder_set_column_index, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_COL_SPAN,
                          accesskit_node_builder_set_column_span, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_POS_IN_SET,
                          accesskit_node_builder_set_position_in_set, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_ROW_COUNT,
                          accesskit_node_builder_set_row_count, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_ROW_INDEX,
                          accesskit_node_builder_set_row_index, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_ROW_SPAN,
                          accesskit_node_builder_set_row_span, builder);
  set_size_from_relation (ctx, GTK_ACCESSIBLE_RELATION_SET_SIZE,
                          accesskit_node_builder_set_size_of_set, builder);

  set_string_from_relation (ctx, GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT,
                            accesskit_node_builder_set_column_index_text, builder);
  set_string_from_relation (ctx, GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT,
                            accesskit_node_builder_set_row_index_text, builder);

  accesskit_node_builder_set_class_name (builder,
                                         g_type_name (G_TYPE_FROM_INSTANCE (accessible)));

  if (!(gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_LABEL) ||
        gtk_at_context_has_accessible_relation (ctx, GTK_ACCESSIBLE_RELATION_LABELLED_BY)) &&
      gtk_at_context_is_nested_button (ctx))
    {
      GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);
      GtkATContext *parent_ctx = gtk_accessible_get_at_context (parent);

      gtk_at_context_realize (parent_ctx);
      accesskit_node_builder_push_labelled_by (builder,
                                               GTK_ACCESSKIT_CONTEXT (parent_ctx)->id);
      g_object_unref (parent_ctx);
      g_object_unref (parent);
    }

  if (GTK_IS_LABEL (accessible))
    {
      GtkLabel *label = GTK_LABEL (accessible);
      PangoLayout *layout = gtk_label_get_layout (label);
      float x, y;

      gtk_label_get_layout_location (label, &x, &y);
      add_text_layout (self, update, builder, layout, x, y);
    }
  else if (GTK_IS_INSCRIPTION (accessible))
    {
      GtkInscription *inscription = GTK_INSCRIPTION (accessible);
      PangoLayout *layout = gtk_inscription_get_layout (inscription);
      float x, y;

      gtk_inscription_get_layout_location (inscription, &x, &y);
      add_text_layout (self, update, builder, layout, x, y);
    }
  else if (GTK_IS_TEXT (accessible))
    {
      GtkText *text = GTK_TEXT (accessible);
      PangoLayout *layout = gtk_text_get_layout (text);
      int x, y;

      gtk_text_get_layout_offsets (text, &x, &y);
      add_text_layout (self, update, builder, layout, x, y);
    }
  /* TODO: text */

  accesskit_tree_update_push_node (update, self->id,
                                   accesskit_node_builder_build (builder));
}
