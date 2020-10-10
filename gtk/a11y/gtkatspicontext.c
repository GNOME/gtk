/* gtkatspicontext.c: AT-SPI GtkATContext implementation
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

#include "gtkatspicontextprivate.h"

#include "gtkatspicacheprivate.h"
#include "gtkatspirootprivate.h"
#include "gtkatspiprivate.h"
#include "gtkatspiutilsprivate.h"
#include "gtkatspipangoprivate.h"

#include "a11y/atspi/atspi-accessible.h"
#include "a11y/atspi/atspi-text.h"

#include "gtkdebug.h"
#include "gtkwindow.h"
#include "gtklabel.h"
#include "gtklabelprivate.h"
#include "gtkroot.h"

#include <gio/gio.h>

#include <locale.h>

#if defined(GDK_WINDOWING_WAYLAND)
# include <gdk/wayland/gdkwaylanddisplay.h>
#endif
#if defined(GDK_WINDOWING_X11)
# include <gdk/x11/gdkx11display.h>
# include <gdk/x11/gdkx11property.h>
#endif

struct _GtkAtSpiContext
{
  GtkATContext parent_instance;

  /* The root object, used as a entry point */
  GtkAtSpiRoot *root;

  /* The cache object, used to retrieve ATContexts */
  GtkAtSpiCache *cache;

  /* The address for the ATSPI accessibility bus */
  char *bus_address;

  /* The object path of the ATContext on the bus */
  char *context_path;

  /* Just a pointer; the connection is owned by the GtkAtSpiRoot
   * associated to the GtkATContext
   */
  GDBusConnection *connection;
};

enum
{
  PROP_BUS_ADDRESS = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAtSpiContext, gtk_at_spi_context, GTK_TYPE_AT_CONTEXT)

static void
collect_states (GtkAtSpiContext    *self,
                GVariantBuilder *builder)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  GtkAccessibleValue *value;
  guint64 state = 0;

  state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_VISIBLE);

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_BUSY))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_BUSY);
      if (gtk_boolean_accessible_value_get (value))
        state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_BUSY);
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_CHECKED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_CHECKED);
      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_CHECKED);
          break;
        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_INDETERMINATE);
          break;
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED);
      if (!gtk_boolean_accessible_value_get (value))
        state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SENSITIVE);
    }
  else
    state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SENSITIVE);

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_EXPANDABLE);
          if (gtk_boolean_accessible_value_get (value))
            state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_EXPANDED);
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID);
      switch (gtk_invalid_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_INVALID_TRUE:
        case GTK_ACCESSIBLE_INVALID_GRAMMAR:
        case GTK_ACCESSIBLE_INVALID_SPELLING:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_INVALID);
          break;
        case GTK_ACCESSIBLE_INVALID_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_PRESSED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_PRESSED);
      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_PRESSED);
          break;
        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_INDETERMINATE);
          break;
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SELECTABLE);
          if (gtk_boolean_accessible_value_get (value))
            state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SELECTED);
        }
    }

  g_variant_builder_add (builder, "u", (guint32) (state & 0xffffffff));
  g_variant_builder_add (builder, "u", (guint32) (state >> 32));
}

static void
collect_relations (GtkAtSpiContext *self,
                   GVariantBuilder *builder)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  struct {
    GtkAccessibleRelation r;
    AtspiRelationType s;
  } map[] = {
    { GTK_ACCESSIBLE_RELATION_LABELLED_BY, ATSPI_RELATION_LABELLED_BY },
    { GTK_ACCESSIBLE_RELATION_CONTROLS, ATSPI_RELATION_CONTROLLER_FOR },
    { GTK_ACCESSIBLE_RELATION_DESCRIBED_BY, ATSPI_RELATION_DESCRIBED_BY },
    { GTK_ACCESSIBLE_RELATION_FLOW_TO, ATSPI_RELATION_FLOWS_TO},
  };
  GtkAccessibleValue *value;
  GList *list, *l;
  GtkATContext *target_ctx;
  const char *unique_name;
  int i;

  unique_name = g_dbus_connection_get_unique_name (self->connection);

  for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
      if (!gtk_at_context_has_accessible_relation (ctx, map[i].r))
        continue;

      GVariantBuilder b = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(so)"));

      value = gtk_at_context_get_accessible_relation (ctx, map[i].r);
      list = gtk_reference_list_accessible_value_get (value);

      for (l = list; l; l = l->next)
        {
          target_ctx = gtk_accessible_get_at_context (GTK_ACCESSIBLE (l->data));
          g_variant_builder_add (&b, "(so)",
                                 unique_name,
                                 GTK_AT_SPI_CONTEXT (target_ctx)->context_path);
        }

      g_variant_builder_add (builder, "(ua(so))", map[i].s, &b);
    }
}

static int
get_index_in_parent (GtkWidget *widget)
{
  GtkWidget *parent = gtk_widget_get_parent (widget);
  GtkWidget *child;
  int idx;

  idx = 0;
  for (child = gtk_widget_get_first_child (parent);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_get_visible (child))
        continue;

      if (child == widget)
        break;

      idx++;
    }

  return idx;
}

static int
get_index_in_toplevels (GtkWidget *widget)
{
  GListModel *toplevels = gtk_window_get_toplevels ();
  guint n_toplevels = g_list_model_get_n_items (toplevels);
  GtkWidget *window;
  int idx;

  idx = 0;
  for (guint i = 0; i < n_toplevels; i++)
    {
      window = g_list_model_get_item (toplevels, i);

      g_object_unref (window);

      if (!gtk_widget_get_visible (window))
        continue;

      if (window == widget)
        break;

      idx += 1;
    }

  return idx;
}

static void
handle_accessible_method (GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  if (g_strcmp0 (method_name, "GetRole") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      guint atspi_role = gtk_accessible_role_to_atspi_role (role);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", atspi_role));
    }
  else if (g_strcmp0 (method_name, "GetRoleName") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      const char *name = gtk_accessible_role_to_name (role, NULL);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));
    }
  else if (g_strcmp0 (method_name, "GetLocalizedRoleName") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      const char *name = gtk_accessible_role_to_name (role, GETTEXT_PACKAGE);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));
    }
  else if (g_strcmp0 (method_name, "GetState") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(au)"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("au"));
      collect_states (self, &builder);
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(a{ss})"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));
      g_variant_builder_add (&builder, "{ss}", "toolkit", "GTK");
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetApplication") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("((so))", gtk_at_spi_root_to_ref (self->root)));
    }
  else if (g_strcmp0 (method_name, "GetChildAtIndex") == 0)
    {
      GtkWidget *child = NULL;
      int idx, real_idx = 0;

      g_variant_get (parameters, "(i)", &idx);

      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkWidget *widget = GTK_WIDGET (accessible);

      real_idx = 0;
      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          if (real_idx == idx)
            break;

          real_idx += 1;
        }

      if (child == NULL)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "No child with index %d", idx);
          return;
        }

      GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child));

      const char *name = g_dbus_connection_get_unique_name (self->connection);
      const char *path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("((so))", name, path));
    }
  else if (g_strcmp0 (method_name, "GetChildren") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(so)"));

      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkWidget *widget = GTK_WIDGET (accessible);
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child));

          const char *name = g_dbus_connection_get_unique_name (self->connection);
          const char *path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));

          g_variant_builder_add (&builder, "(so)", name, path);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(so))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetIndexInParent") == 0)
    {
      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      int idx;

      if (GTK_IS_ROOT (accessible))
        idx = get_index_in_toplevels (GTK_WIDGET (accessible));
      else
        idx = get_index_in_parent (GTK_WIDGET (accessible));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", idx));
    }
  else if (g_strcmp0 (method_name, "GetRelationSet") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(ua(so))"));
       collect_relations (self, &builder);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(ua(so)))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetInterfaces") == 0)
    {
      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("as"));

      g_variant_builder_add (&builder, "s", "org.a11y.atspi.Accessible");
      if (GTK_IS_LABEL (accessible))
        g_variant_builder_add (&builder, "s", "org.a11y.atspi.Text");
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", &builder));
    }

}

static GVariant *
handle_accessible_get_property (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *property_name,
                                GError               **error,
                                gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;
  GVariant *res = NULL;

  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "Name") == 0)
    res = g_variant_new_string (gtk_widget_get_name (widget));
  else if (g_strcmp0 (property_name, "Description") == 0)
    {
      char *label = gtk_at_context_get_label (GTK_AT_CONTEXT (self));
      res = g_variant_new_string (label);
      g_free (label);
    }
  else if (g_strcmp0 (property_name, "Locale") == 0)
    res = g_variant_new_string (setlocale (LC_MESSAGES, NULL));
  else if (g_strcmp0 (property_name, "AccessibleId") == 0)
    res = g_variant_new_string ("");
  else if (g_strcmp0 (property_name, "Parent") == 0)
    {
      GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (accessible));

      if (parent == NULL)
        {
          res = gtk_at_spi_root_to_ref (self->root);
        }
      else
        {
          GtkATContext *parent_context =
            gtk_accessible_get_at_context (GTK_ACCESSIBLE (parent));

          if (parent_context != NULL)
            res = g_variant_new ("(so)",
                                 g_dbus_connection_get_unique_name (self->connection),
                                 GTK_AT_SPI_CONTEXT (parent_context)->context_path);
        }

      if (res == NULL)
        res = gtk_at_spi_null_ref ();
    }
  else if (g_strcmp0 (property_name, "ChildCount") == 0)
    {
      int n_children = 0;
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          n_children++;
        }

      res = g_variant_new_int32 (n_children);
    }
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static const GDBusInterfaceVTable accessible_vtable = {
  handle_accessible_method,
  handle_accessible_get_property,
  NULL,
};

static void
handle_text_method (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "GetCaretOffset") == 0)
    {
      int offset;

      offset = _gtk_label_get_cursor_position (GTK_LABEL (widget));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", offset));
    }
  else if (g_strcmp0 (method_name, "SetCaretOffset") == 0)
    {
      int offset;

      g_variant_get (parameters, "(i)", &offset);

      if (gtk_label_get_selectable (GTK_LABEL (widget)))
        gtk_label_select_region (GTK_LABEL (widget), offset, offset);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "GetText") == 0)
    {
      int start, end;
      const char *text;
      int len;
      char *string;

      g_variant_get (parameters, "(ii)", &start, &end);

      text = gtk_label_get_text (GTK_LABEL (widget));
      len = g_utf8_strlen (text, -1);

      start = CLAMP (start, 0, len);
      end = CLAMP (end, 0, len);

      if (end <= start)
        string = g_strdup ("");
      else
        {
          const char *p, *q;
          p = g_utf8_offset_to_pointer (text, start);
          q = g_utf8_offset_to_pointer (text, end);
          string = g_strndup (p, q - p);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", string));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetTextBeforeOffset") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      int offset;
      AtspiTextBoundaryType boundary_type;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &boundary_type);
      string = gtk_pango_get_text_before (layout, offset, boundary_type, &start, &end);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetTextAtOffset") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      int offset;
      AtspiTextBoundaryType boundary_type;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &boundary_type);
      string = gtk_pango_get_text_at (layout, offset, boundary_type, &start, &end);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetTextAfterOffset") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      int offset;
      AtspiTextBoundaryType boundary_type;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &boundary_type);
      string = gtk_pango_get_text_after (layout, offset, boundary_type, &start, &end);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetCharacterAtOffset") == 0)
    {
      int offset;
      const char *text;
      gunichar ch = 0;

      g_variant_get (parameters, "(i)", &offset);

      text = gtk_label_get_text (GTK_LABEL (widget));

      if (0 <= offset && offset < g_utf8_strlen (text, -1))
        ch = g_utf8_get_char (g_utf8_offset_to_pointer (text, offset));

      g_dbus_method_invocation_return_value (invocation, g_variant_new_int32 (ch));
    }
  else if (g_strcmp0 (method_name, "GetStringAtOffset") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      int offset;
      AtspiTextGranularity granularity;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &granularity);

      string = gtk_pango_get_string_at (layout, offset, granularity, &start, &end);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      int start, end;

      g_variant_get (parameters, "(i)", &offset);

      gtk_pango_get_run_attributes (layout, &builder, offset, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetAttributeValue") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      const char *name;
      int start, end;
      GVariant *attrs;
      const char *val;

      g_variant_get (parameters, "(i&s)", &offset, &name);
      gtk_pango_get_run_attributes (layout, &builder, offset, &start, &end);
      attrs = g_variant_builder_end (&builder);
      if (!g_variant_lookup (attrs, name, "&s", &val))
        val = "";

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", val));
      g_variant_unref (attrs);
    }
  else if (g_strcmp0 (method_name, "GetAttributeRun") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      gboolean include_defaults;
      int start, end;

      g_variant_get (parameters, "(ib)", &offset, &include_defaults);

      if (include_defaults)
        gtk_pango_get_default_attributes (layout, &builder);

      gtk_pango_get_run_attributes (layout, &builder, offset, &start, &end);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetDefaultAttributes") == 0 ||
           g_strcmp0 (method_name, "GetDefaultAttributeSet") == 0)
    {
      PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));

      gtk_pango_get_default_attributes (layout, &builder);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss})", &builder));
    }
  else if (g_strcmp0 (method_name, "GetNSelections") == 0)
    {
      int n = 0;

      if (gtk_label_get_selection_bounds (GTK_LABEL (widget), NULL, NULL))
        n = 1;

      g_dbus_method_invocation_return_value (invocation, g_variant_new_int32 (n));
    }
  else if (g_strcmp0 (method_name, "GetSelection") == 0)
    {
      int num;
      int start, end;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0 ||
          !gtk_label_get_selection_bounds (GTK_LABEL (widget), &start, &end))
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid selection: %d", num);
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ii)", start, end));
    }
  else if (g_strcmp0 (method_name, "AddSelection") == 0)
    {
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (!gtk_label_get_selectable (GTK_LABEL (widget)) ||
          gtk_label_get_selection_bounds (GTK_LABEL (widget), NULL, NULL))
        {
          ret = FALSE;
        }
      else
        {
          gtk_label_select_region (GTK_LABEL (widget), start, end);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new_boolean (ret));
    }
  else if (g_strcmp0 (method_name, "RemoveSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0 ||
          !gtk_label_get_selectable (GTK_LABEL (widget)) ||
          !gtk_label_get_selection_bounds (GTK_LABEL (widget), &start, &end))
        {
          ret = FALSE;
        }
      else
        {
          gtk_label_select_region (GTK_LABEL (widget), end, end);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new_boolean (ret));
    }
  else if (g_strcmp0 (method_name, "SetSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(iii)", &num, &start, &end);

      if (num != 0 ||
          !gtk_label_get_selectable (GTK_LABEL (widget)) ||
          !gtk_label_get_selection_bounds (GTK_LABEL (widget), NULL, NULL))
        {
          ret = FALSE;
        }
      else
        {
          gtk_label_select_region (GTK_LABEL (widget), start, end);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new_boolean (ret));
    }
  else if (g_strcmp0 (method_name, "GetCharacterExtents") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "GetRangeExtents") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "GetBoundedRanges") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "ScrollSubstringTo") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "ScrollSubstringToPoint") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
}

static GVariant *
handle_text_get_property (GDBusConnection  *connection,
                          const gchar      *sender,
                          const gchar      *object_path,
                          const gchar      *interface_name,
                          const gchar      *property_name,
                          GError          **error,
                          gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "CharacterCount") == 0)
    {
      const char *text;
      int len;

      text = gtk_label_get_text (GTK_LABEL (widget));
      len = g_utf8_strlen (text, -1);

      return g_variant_new_int32 (len);
    }
  else if (g_strcmp0 (property_name, "CaretOffset") == 0)
    {
      return g_variant_new_int32 (0);
    }

  return NULL;
}

static const GDBusInterfaceVTable text_vtable = {
  handle_text_method,
  handle_text_get_property,
  NULL,
};

static void
gtk_at_spi_context_register_object (GtkAtSpiContext *self)
{
  g_dbus_connection_register_object (self->connection,
                                     self->context_path,
                                     (GDBusInterfaceInfo *) &atspi_accessible_interface,
                                     &accessible_vtable,
                                     self,
                                     NULL,
                                     NULL);

  if (GTK_IS_LABEL (gtk_at_context_get_accessible (GTK_AT_CONTEXT (self))))
    {
      g_dbus_connection_register_object (self->connection,
                                         self->context_path,
                                         (GDBusInterfaceInfo *) &atspi_text_interface,
                                         &text_vtable,
                                         self,
                                         NULL,
                                         NULL);
    }
}

static void
gtk_at_spi_context_state_change (GtkATContext                *self,
                                 GtkAccessibleStateChange     changed_states,
                                 GtkAccessiblePropertyChange  changed_properties,
                                 GtkAccessibleRelationChange  changed_relations,
                                 GtkAccessibleAttributeSet   *states,
                                 GtkAccessibleAttributeSet   *properties,
                                 GtkAccessibleAttributeSet   *relations)
{
}

static void
gtk_at_spi_context_finalize (GObject *gobject)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  g_free (self->bus_address);
  g_free (self->context_path);

  G_OBJECT_CLASS (gtk_at_spi_context_parent_class)->finalize (gobject);
}

static void
gtk_at_spi_context_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      self->bus_address = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_spi_context_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      g_value_set_string (value, self->bus_address);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_spi_context_constructed (GObject *gobject)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);
  GdkDisplay *display;

  g_assert (self->bus_address);

  /* Every GTK application has a single root AT-SPI object, which
   * handles all the global state, including the cache of accessible
   * objects. We use the GdkDisplay to store it, so it's guaranteed
   * to be unique per-display connection
   */
  display = gtk_at_context_get_display (GTK_AT_CONTEXT (self));
  self->root =
    g_object_get_data (G_OBJECT (display), "-gtk-atspi-root");

  if (self->root == NULL)
    {
      self->root = gtk_at_spi_root_new (self->bus_address);
      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-root",
                              self->root,
                              g_object_unref);
    }

  self->connection = gtk_at_spi_root_get_connection (self->root);

  /* We use the application's object path to build the path of each
   * accessible object exposed on the accessibility bus; the path is
   * also used to access the object cache
   */
  GApplication *application = g_application_get_default ();
  char *base_path = NULL;

  if (application != NULL)
    {
      const char *app_path = g_application_get_dbus_object_path (application);
      base_path = g_strconcat (app_path, "/a11y", NULL);
    }
  else
    {
      char *uuid = g_uuid_string_random ();
      base_path = g_strconcat ("/org/gtk/application/", uuid, "/a11y", NULL);
      g_free (uuid);
    }

  /* We use a unique id to ensure that we don't have conflicting
   * objects on the bus
   */
  char *uuid = g_uuid_string_random ();

  self->context_path = g_strconcat (base_path, "/", uuid, NULL);

  /* UUIDs use '-' as the separator, but that's not a valid character
   * for a DBus object path
   */
  size_t path_len = strlen (self->context_path);
  for (size_t i = 0; i < path_len; i++)
    {
      if (self->context_path[i] == '-')
        self->context_path[i] = '_';
    }

  GTK_NOTE (A11Y, g_message ("ATSPI context path: %s", self->context_path));

  g_free (base_path);
  g_free (uuid);

  gtk_at_spi_context_register_object (self);

  G_OBJECT_CLASS (gtk_at_spi_context_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_context_class_init (GtkAtSpiContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_context_constructed;
  gobject_class->set_property = gtk_at_spi_context_set_property;
  gobject_class->get_property = gtk_at_spi_context_get_property;
  gobject_class->finalize = gtk_at_spi_context_finalize;

  context_class->state_change = gtk_at_spi_context_state_change;

  obj_props[PROP_BUS_ADDRESS] =
    g_param_spec_string ("bus-address", NULL, NULL,
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_spi_context_init (GtkAtSpiContext *self)
{
}

#ifdef GDK_WINDOWING_X11
static char *
get_bus_address_x11 (GdkDisplay *display)
{
  GTK_NOTE (A11Y, g_message ("Acquiring a11y bus via X11..."));

  Display *xdisplay = gdk_x11_display_get_xdisplay (display);
  Atom type_return;
  int format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;
  char *address = NULL;

  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (xdisplay, DefaultRootWindow (xdisplay),
                      gdk_x11_get_xatom_by_name_for_display (display, "AT_SPI_BUS"),
                      0L, BUFSIZ, False,
                      (Atom) 31,
                      &type_return, &format_return, &nitems_return,
                      &bytes_after_return, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  address = g_strdup ((char *) data);

  XFree (data);

  return address;
}
#endif

#if defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
static char *
get_bus_address_dbus (GdkDisplay *display)
{
  GTK_NOTE (A11Y, g_message ("Acquiring a11y bus via DBus..."));

  GError *error = NULL;
  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (error != NULL)
    {
      g_critical ("Unable to acquire session bus: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  GVariant *res =
    g_dbus_connection_call_sync (connection, "org.a11y.Bus",
                                  "/org/a11y/bus",
                                  "org.a11y.Bus",
                                  "GetAddress",
                                  NULL, NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  if (error != NULL)
    {
      g_critical ("Unable to acquire the address of the accessibility bus: %s",
                  error->message);
      g_error_free (error);
    }

  char *address = NULL;
  if (res != NULL)
    {
      g_variant_get (res, "(s)", &address);
      g_variant_unref (res);
    }

  g_object_unref (connection);

  return address;
}
#endif

static const char *
get_bus_address (GdkDisplay *display)
{
  const char *bus_address;

  bus_address = g_object_get_data (G_OBJECT (display), "-gtk-atspi-bus-address");
  if (bus_address != NULL)
    return bus_address;

  /* The bus address environment variable takes precedence; this is the
   * mechanism used by Flatpak to handle the accessibility bus portal
   * between the sandbox and the outside world
   */
  bus_address = g_getenv ("AT_SPI_BUS_ADDRESS");
  if (bus_address != NULL && *bus_address != '\0')
    {
      GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from environment: %s", bus_address));
      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                              g_strdup (bus_address),
                              g_free);
      goto out;
    }

#if defined(GDK_WINDOWING_WAYLAND)
  if (bus_address == NULL)
    {
      if (GDK_IS_WAYLAND_DISPLAY (display))
        {
          char *addr = get_bus_address_dbus (display);

          GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from D-Bus: %s", addr));
          g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                                  addr,
                                  g_free);

          bus_address = addr;
        }
    }
#endif
#if defined(GDK_WINDOWING_X11)
  if (bus_address == NULL)
    {
      if (GDK_IS_X11_DISPLAY (display))
        {
          char *addr = get_bus_address_dbus (display);

          if (addr == NULL)
            {
              addr = get_bus_address_x11 (display);
              GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from X11: %s", addr));
            }
          else
            {
              GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from D-Bus: %s", addr));
            }

          g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                                  addr,
                                  g_free);

          bus_address = addr;
        }
    }
#endif

out:
  return bus_address;
}

GtkATContext *
gtk_at_spi_create_context (GtkAccessibleRole  accessible_role,
                           GtkAccessible     *accessible,
                           GdkDisplay        *display)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  const char *bus_address = get_bus_address (display);

  if (bus_address == NULL)
    return NULL;

#if defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                         "accessible-role", accessible_role,
                         "accessible", accessible,
                         "display", display,
                         "bus-address", bus_address,
                         NULL);
#endif
#if defined(GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display))
    return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                         "accessible-role", accessible_role,
                         "accessible", accessible,
                         "display", display,
                         "bus-address", bus_address,
                         NULL);
#endif

  return NULL;
}

const char *
gtk_at_spi_context_get_context_path (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  return self->context_path;
}
