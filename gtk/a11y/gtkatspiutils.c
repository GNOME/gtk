/* gtkatspiutils.c: Shared utilities for ATSPI
 *
 * Copyright 2020  GNOME Foundation
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

#include "gtkatspiutilsprivate.h"

#include "gdk/gdkprofilerprivate.h"
#include "gtkenums.h"
#include "gtkscrolledwindow.h"

typedef struct
{
  const GDBusInterfaceVTable *vtable;
  gpointer user_data;
  GDestroyNotify user_data_free_func;
  const char *type_name;
} ProfiledVTable;

static void
profiled_end_mark (gint64          before,
                   const char     *name,
                   const char     *operation,
                   const char     *interface_name,
                   const char     *object_path,
                   ProfiledVTable *data)
{
  char *mark_name;

  if (before == 0 || !GDK_PROFILER_IS_RUNNING)
    return;

  mark_name = g_strconcat ("AT-SPI ", operation, " ", name, NULL);
  gdk_profiler_end_markf (before, mark_name, "%s on %s (%s)",
                          interface_name, data->type_name, object_path);
  g_free (mark_name);
}

static void
profiled_method_call (GDBusConnection       *connection,
                      const char            *sender,
                      const char            *object_path,
                      const char            *interface_name,
                      const char            *method_name,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  ProfiledVTable *data = user_data;
  gint64 before = GDK_PROFILER_CURRENT_TIME;

  data->vtable->method_call (connection, sender, object_path, interface_name,
                             method_name, parameters, invocation, data->user_data);

  profiled_end_mark (before, method_name, "call", interface_name, object_path, data);
}

static GVariant *
profiled_get_property (GDBusConnection  *connection,
                       const char       *sender,
                       const char       *object_path,
                       const char       *interface_name,
                       const char       *property_name,
                       GError          **error,
                       gpointer          user_data)
{
  ProfiledVTable *data = user_data;
  gint64 before = GDK_PROFILER_CURRENT_TIME;
  GVariant *result;

  result = data->vtable->get_property (connection, sender, object_path,
                                       interface_name, property_name, error,
                                       data->user_data);

  profiled_end_mark (before, property_name, "get", interface_name, object_path, data);

  return result;
}

static gboolean
profiled_set_property (GDBusConnection  *connection,
                       const char       *sender,
                       const char       *object_path,
                       const char       *interface_name,
                       const char       *property_name,
                       GVariant         *value,
                       GError          **error,
                       gpointer          user_data)
{
  ProfiledVTable *data = user_data;
  gint64 before = GDK_PROFILER_CURRENT_TIME;
  gboolean result;

  result = data->vtable->set_property (connection, sender, object_path,
                                       interface_name, property_name, value,
                                       error, data->user_data);

  profiled_end_mark (before, property_name, "set", interface_name, object_path, data);

  return result;
}

static void
profiled_vtable_free (gpointer user_data)
{
  ProfiledVTable *data = user_data;

  if (data->user_data_free_func != NULL)
    data->user_data_free_func (data->user_data);

  g_free (data);
}

guint
gtk_at_spi_register_object (GDBusConnection             *connection,
                            const char                  *object_path,
                            GDBusInterfaceInfo          *interface_info,
                            const GDBusInterfaceVTable  *vtable,
                            gpointer                     user_data,
                            GDestroyNotify               user_data_free_func,
                            GError                     **error)
{
  GDBusInterfaceVTable profiled_vtable = {
    vtable->method_call != NULL ? profiled_method_call : NULL,
    vtable->get_property != NULL ? profiled_get_property : NULL,
    vtable->set_property != NULL ? profiled_set_property : NULL,
  };
  ProfiledVTable *data = g_new (ProfiledVTable, 1);

  data->vtable = vtable;
  data->user_data = user_data;
  data->user_data_free_func = user_data_free_func;

  if (GTK_IS_AT_CONTEXT (user_data))
    {
      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (user_data));

      data->type_name = G_OBJECT_TYPE_NAME (accessible);
    }
  else
    {
      data->type_name = G_OBJECT_TYPE_NAME (user_data);
    }

  return g_dbus_connection_register_object (connection, object_path,
                                            interface_info, &profiled_vtable,
                                            data, profiled_vtable_free, error);
}

/*< private >
 * gtk_accessible_role_to_atspi_role:
 * @role: a `GtkAccessibleRole`
 *
 * Converts a `GtkAccessibleRole` value to the equivalent ATSPI role.
 *
 * Returns: an #AtspiRole
 */
static AtspiRole
gtk_accessible_role_to_atspi_role (GtkAccessibleRole role)
{
  switch (role)
    {
    case GTK_ACCESSIBLE_ROLE_ALERT:
      return ATSPI_ROLE_ALERT;

    case GTK_ACCESSIBLE_ROLE_ALERT_DIALOG:
      return ATSPI_ROLE_ALERT;

    case GTK_ACCESSIBLE_ROLE_APPLICATION:
      return ATSPI_ROLE_FRAME;

    case GTK_ACCESSIBLE_ROLE_ARTICLE:
      return ATSPI_ROLE_ARTICLE;

    case GTK_ACCESSIBLE_ROLE_BANNER:
      break;

    case GTK_ACCESSIBLE_ROLE_BLOCK_QUOTE:
      return ATSPI_ROLE_BLOCK_QUOTE;

    case GTK_ACCESSIBLE_ROLE_BUTTON:
      return ATSPI_ROLE_PUSH_BUTTON;

    case GTK_ACCESSIBLE_ROLE_CAPTION:
      return ATSPI_ROLE_CAPTION;

    case GTK_ACCESSIBLE_ROLE_CELL:
      return ATSPI_ROLE_TABLE_CELL;

    case GTK_ACCESSIBLE_ROLE_CHECKBOX:
      return ATSPI_ROLE_CHECK_BOX;

    case GTK_ACCESSIBLE_ROLE_COLUMN_HEADER:
      return ATSPI_ROLE_COLUMN_HEADER;

    case GTK_ACCESSIBLE_ROLE_COMBO_BOX:
      return ATSPI_ROLE_COMBO_BOX;

    case GTK_ACCESSIBLE_ROLE_COMMAND:
      break;

    case GTK_ACCESSIBLE_ROLE_COMMENT:
      return ATSPI_ROLE_COMMENT;

    case GTK_ACCESSIBLE_ROLE_COMPOSITE:
      break;

    case GTK_ACCESSIBLE_ROLE_DIALOG:
      return ATSPI_ROLE_DIALOG;

    case GTK_ACCESSIBLE_ROLE_DOCUMENT:
      return ATSPI_ROLE_DOCUMENT_TEXT;

    case GTK_ACCESSIBLE_ROLE_FEED:
      break;

    case GTK_ACCESSIBLE_ROLE_FORM:
      return ATSPI_ROLE_FORM;

    case GTK_ACCESSIBLE_ROLE_GENERIC:
      return ATSPI_ROLE_PANEL;

    case GTK_ACCESSIBLE_ROLE_GRID:
      return ATSPI_ROLE_TABLE;

    case GTK_ACCESSIBLE_ROLE_GRID_CELL:
      return ATSPI_ROLE_TABLE_CELL;

    case GTK_ACCESSIBLE_ROLE_GROUP:
      return ATSPI_ROLE_GROUPING;

    case GTK_ACCESSIBLE_ROLE_HEADING:
      return ATSPI_ROLE_HEADING;

    case GTK_ACCESSIBLE_ROLE_IMG:
      return ATSPI_ROLE_IMAGE;

    case GTK_ACCESSIBLE_ROLE_INPUT:
      return ATSPI_ROLE_ENTRY;

    case GTK_ACCESSIBLE_ROLE_LABEL:
      return ATSPI_ROLE_LABEL;

    case GTK_ACCESSIBLE_ROLE_LANDMARK:
      return ATSPI_ROLE_LANDMARK;

    case GTK_ACCESSIBLE_ROLE_LEGEND:
      return ATSPI_ROLE_LABEL;

    case GTK_ACCESSIBLE_ROLE_LINK:
      return ATSPI_ROLE_LINK;

    case GTK_ACCESSIBLE_ROLE_LIST:
      return ATSPI_ROLE_LIST;

    case GTK_ACCESSIBLE_ROLE_LIST_BOX:
      return ATSPI_ROLE_LIST_BOX;

    case GTK_ACCESSIBLE_ROLE_LIST_ITEM:
      return ATSPI_ROLE_LIST_ITEM;

    case GTK_ACCESSIBLE_ROLE_LOG:
      return ATSPI_ROLE_LOG;

    case GTK_ACCESSIBLE_ROLE_MAIN:
      break;

    case GTK_ACCESSIBLE_ROLE_MARQUEE:
      return ATSPI_ROLE_MARQUEE;

    case GTK_ACCESSIBLE_ROLE_MATH:
      return ATSPI_ROLE_MATH;

    case GTK_ACCESSIBLE_ROLE_METER:
      return ATSPI_ROLE_LEVEL_BAR;

    case GTK_ACCESSIBLE_ROLE_MENU:
      return ATSPI_ROLE_MENU;

    case GTK_ACCESSIBLE_ROLE_MENU_BAR:
      return ATSPI_ROLE_MENU_BAR;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM:
      return ATSPI_ROLE_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX:
      return ATSPI_ROLE_CHECK_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO:
      return ATSPI_ROLE_RADIO_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_NAVIGATION:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_NONE:
      return ATSPI_ROLE_INVALID;

    case GTK_ACCESSIBLE_ROLE_NOTE:
      return ATSPI_ROLE_FOOTNOTE;

    case GTK_ACCESSIBLE_ROLE_OPTION:
      return ATSPI_ROLE_OPTION_PANE;

    case GTK_ACCESSIBLE_ROLE_PARAGRAPH:
      return ATSPI_ROLE_PARAGRAPH;

    case GTK_ACCESSIBLE_ROLE_PRESENTATION:
      return ATSPI_ROLE_INVALID;

    case GTK_ACCESSIBLE_ROLE_PROGRESS_BAR:
      return ATSPI_ROLE_PROGRESS_BAR;

    case GTK_ACCESSIBLE_ROLE_RADIO:
      return ATSPI_ROLE_RADIO_BUTTON;

    case GTK_ACCESSIBLE_ROLE_RADIO_GROUP:
      return ATSPI_ROLE_GROUPING;

    case GTK_ACCESSIBLE_ROLE_RANGE:
      break;

    case GTK_ACCESSIBLE_ROLE_REGION:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_ROW:
      return ATSPI_ROLE_TABLE_ROW;

    case GTK_ACCESSIBLE_ROLE_ROW_GROUP:
      return ATSPI_ROLE_GROUPING;

    case GTK_ACCESSIBLE_ROLE_ROW_HEADER:
      return ATSPI_ROLE_ROW_HEADER;

    case GTK_ACCESSIBLE_ROLE_SCROLLBAR:
      return ATSPI_ROLE_SCROLL_BAR;

    case GTK_ACCESSIBLE_ROLE_SEARCH:
      return ATSPI_ROLE_FORM;

    case GTK_ACCESSIBLE_ROLE_SEARCH_BOX:
      return ATSPI_ROLE_ENTRY;

    case GTK_ACCESSIBLE_ROLE_SECTION:
      return ATSPI_ROLE_SECTION;

    case GTK_ACCESSIBLE_ROLE_SECTION_HEAD:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_SELECT:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_SEPARATOR:
      return ATSPI_ROLE_SEPARATOR;

    case GTK_ACCESSIBLE_ROLE_SLIDER:
      return ATSPI_ROLE_SLIDER;

    case GTK_ACCESSIBLE_ROLE_SPIN_BUTTON:
      return ATSPI_ROLE_SPIN_BUTTON;

    case GTK_ACCESSIBLE_ROLE_STATUS:
      return ATSPI_ROLE_STATUS_BAR;

    case GTK_ACCESSIBLE_ROLE_STRUCTURE:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_SWITCH:
      return ATSPI_ROLE_SWITCH;

    case GTK_ACCESSIBLE_ROLE_TAB:
      return ATSPI_ROLE_PAGE_TAB;

    case GTK_ACCESSIBLE_ROLE_TABLE:
      return ATSPI_ROLE_TABLE;

    case GTK_ACCESSIBLE_ROLE_TAB_LIST:
      return ATSPI_ROLE_PAGE_TAB_LIST;

    case GTK_ACCESSIBLE_ROLE_TAB_PANEL:
      return ATSPI_ROLE_PANEL;

    case GTK_ACCESSIBLE_ROLE_TEXT_BOX:
      return ATSPI_ROLE_TEXT;

    case GTK_ACCESSIBLE_ROLE_TIME:
      return ATSPI_ROLE_TEXT;

    case GTK_ACCESSIBLE_ROLE_TIMER:
      return ATSPI_ROLE_TIMER;

    case GTK_ACCESSIBLE_ROLE_TOOLBAR:
      return ATSPI_ROLE_TOOL_BAR;

    case GTK_ACCESSIBLE_ROLE_TOOLTIP:
      return ATSPI_ROLE_TOOL_TIP;

    case GTK_ACCESSIBLE_ROLE_TREE:
      return ATSPI_ROLE_TREE;

    case GTK_ACCESSIBLE_ROLE_TREE_GRID:
      return ATSPI_ROLE_TREE_TABLE;

    case GTK_ACCESSIBLE_ROLE_TREE_ITEM:
      return ATSPI_ROLE_TREE_ITEM;

    case GTK_ACCESSIBLE_ROLE_WIDGET:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_WINDOW:
      return ATSPI_ROLE_FRAME;

    case GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON:
      return ATSPI_ROLE_TOGGLE_BUTTON;

    case GTK_ACCESSIBLE_ROLE_TERMINAL:
      return ATSPI_ROLE_TERMINAL;

    default:
      break;
    }

  return ATSPI_ROLE_FILLER;
}

/*<private>
 * gtk_atspi_role_for_context:
 * @context: a `GtkATContext`
 *
 * Returns a suitable ATSPI role for a context, taking into account
 * both the `GtkAccessibleRole` set on the context and the type
 * of accessible.
 *
 * Returns: an #AtspiRole
 */
AtspiRole
gtk_atspi_role_for_context (GtkATContext *context)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);
  GtkAccessibleRole role = gtk_at_context_get_accessible_role (context);

  /* ARIA does not have a password entry role, so use the input purpose to distinguish them */
  if (role == GTK_ACCESSIBLE_ROLE_TEXT_BOX)
    {
      if (gtk_accessible_is_password_text (accessible))
        return ATSPI_ROLE_PASSWORD_TEXT;
    }
  /* ARIA does not have a "scroll area" role */
  if (GTK_IS_SCROLLED_WINDOW (accessible))
    return ATSPI_ROLE_SCROLL_PANE;

  return gtk_accessible_role_to_atspi_role (role);
}

GVariant *
gtk_at_spi_null_ref (void)
{
  return g_variant_new ("(so)", "", "/org/a11y/atspi/null");
}

void
gtk_at_spi_emit_children_changed (GDBusConnection         *connection,
                                  const char              *path,
                                  GtkAccessibleChildState  state,
                                  int                      idx,
                                  GVariant                *child_ref)
{
  const char *change;

  switch (state)
    {
    case GTK_ACCESSIBLE_CHILD_STATE_ADDED:
      change = "add";
      break;

    case GTK_ACCESSIBLE_CHILD_STATE_REMOVED:
      change = "remove";
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  g_dbus_connection_emit_signal (connection,
                                 NULL,
                                 path,
                                 "org.a11y.atspi.Event.Object",
                                 "ChildrenChanged",
                                 g_variant_new ("(siiva{sv})", change, idx, 0, child_ref, NULL),
                                 NULL);
}


void
gtk_at_spi_translate_coordinates_to_accessible (GtkAccessible  *accessible,
                                                AtspiCoordType  coordtype,
                                                int             xi,
                                                int             yi,
                                                int            *xo,
                                                int            *yo)
{
  GtkAccessible *parent;
  int x, y, width, height;

  if (coordtype == ATSPI_COORD_TYPE_SCREEN)
    {
      *xo = 0;
      *yo = 0;
      return;
    }

  if (!gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    {
      *xo = xi;
      *yo = yi;
      return;
    }

  /* Transform coords to our parent, we will need that in any case */
  *xo = xi - x;
  *yo = yi - y;

  /* If that's what the caller requested, we're done */
  if (coordtype == ATSPI_COORD_TYPE_PARENT)
    return;

  if (coordtype == ATSPI_COORD_TYPE_WINDOW)
    {
      parent = gtk_accessible_get_accessible_parent (accessible);
      while (parent != NULL)
        {
          g_object_unref (parent);

          if (gtk_accessible_get_bounds (parent, &x, &y, &width, &height))
            {
              *xo = *xo - x;
              *yo = *yo - y;
              parent = gtk_accessible_get_accessible_parent (parent);
            }
          else
            break;
        }
    }
  else
    g_assert_not_reached ();
}

void
gtk_at_spi_translate_coordinates_from_accessible (GtkAccessible *accessible,
                                                  AtspiCoordType     coordtype,
                                                  int                xi,
                                                  int                yi,
                                                  int               *xo,
                                                  int               *yo)
{
  GtkAccessible *parent;
  int x, y, width, height;

  if (coordtype == ATSPI_COORD_TYPE_SCREEN)
    {
      *xo = 0;
      *yo = 0;
      return;
    }

  if (!gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    {
      *xo = xi;
      *yo = yi;
      return;
    }

  /* Transform coords to our parent, we will need that in any case */
  *xo = xi + x;
  *yo = yi + y;

  /* If that's what the caller requested, we're done */
  if (coordtype == ATSPI_COORD_TYPE_PARENT)
    return;

  if (coordtype == ATSPI_COORD_TYPE_WINDOW)
    {
      parent = gtk_accessible_get_accessible_parent (accessible);
      while (parent != NULL)
        {
          g_object_unref (parent);

          if (gtk_accessible_get_bounds (parent, &x, &y, &width, &height))
            {
              *xo = *xo + x;
              *yo = *yo + y;
              parent = gtk_accessible_get_accessible_parent (parent);
            }
          else
            break;
        }
    }
  else
    g_assert_not_reached ();
}
