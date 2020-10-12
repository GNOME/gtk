/* gtkatspitext.c: Text interface for GtkAtspiContext
 *
 * Copyright 2020 Red Hat, Inc
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

#include "gtkatspitextprivate.h"

#include "gtkatspiprivate.h"
#include "gtkatspiutilsprivate.h"
#include "gtkatspipangoprivate.h"
#include "gtkatspitextbufferprivate.h"

#include "a11y/atspi/atspi-text.h"

#include "gtkatcontextprivate.h"
#include "gtkdebug.h"
#include "gtkeditable.h"
#include "gtklabelprivate.h"
#include "gtktextprivate.h"
#include "gtktextview.h"

#include <gio/gio.h>

static void
label_handle_method (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
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
      gboolean ret;

      g_variant_get (parameters, "(i)", &offset);

      ret = gtk_label_get_selectable (GTK_LABEL (widget));
      if (ret)
        gtk_label_select_region (GTK_LABEL (widget), offset, offset);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
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

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ch));
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

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", n));
    }
  else if (g_strcmp0 (method_name, "GetSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret = TRUE;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_label_get_selection_bounds (GTK_LABEL (widget), &start, &end))
            ret = FALSE;
        }

      if (!ret)
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

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "RemoveSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_label_get_selectable (GTK_LABEL (widget)) ||
              !gtk_label_get_selection_bounds (GTK_LABEL (widget), &start, &end))
            {
              ret = FALSE;
            }
          else
            {
              gtk_label_select_region (GTK_LABEL (widget), end, end);
              ret = TRUE;
            }
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "SetSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(iii)", &num, &start, &end);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_label_get_selectable (GTK_LABEL (widget)) ||
              !gtk_label_get_selection_bounds (GTK_LABEL (widget), NULL, NULL))
            {
              ret = FALSE;
            }
          else
            {
              gtk_label_select_region (GTK_LABEL (widget), start, end);
              ret = TRUE;
            }
        }
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
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
label_get_property (GDBusConnection  *connection,
                    const gchar      *sender,
                    const gchar      *object_path,
                    const gchar      *interface_name,
                    const gchar      *property_name,
                    GError          **error,
                    gpointer          user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
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
      int offset;

      offset = _gtk_label_get_cursor_position (GTK_LABEL (widget));

      return g_variant_new_int32 (offset);
    }

  return NULL;
}

static const GDBusInterfaceVTable label_vtable = {
  label_handle_method,
  label_get_property,
  NULL,
};


static void
text_handle_method (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "GetCaretOffset") == 0)
    {
      int offset;

      offset = gtk_editable_get_position (GTK_EDITABLE (widget));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", offset));
    }
  else if (g_strcmp0 (method_name, "SetCaretOffset") == 0)
    {
      int offset;
      gboolean ret;

      g_variant_get (parameters, "(i)", &offset);

      gtk_editable_set_position (GTK_EDITABLE (widget), offset);
      ret = TRUE;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "GetText") == 0)
    {
      int start, end;
      const char *text;
      int len;
      char *string;

      g_variant_get (parameters, "(ii)", &start, &end);

      text = gtk_editable_get_text (GTK_EDITABLE (widget));
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
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
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
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
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
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
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

      text = gtk_editable_get_text (GTK_EDITABLE (widget));
      if (0 <= offset && offset < g_utf8_strlen (text, -1))
        ch = g_utf8_get_char (g_utf8_offset_to_pointer (text, offset));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ch));
    }
  else if (g_strcmp0 (method_name, "GetStringAtOffset") == 0)
    {
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
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
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      int start, end;

      g_variant_get (parameters, "(i)", &offset);

      gtk_pango_get_run_attributes (layout, &builder, offset, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetAttributeValue") == 0)
    {
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
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
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
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
      PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));

      gtk_pango_get_default_attributes (layout, &builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss})", &builder));
    }
  else if (g_strcmp0 (method_name, "GetNSelections") == 0)
    {
      int n = 0;

      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), NULL, NULL))
        n = 1;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", n));
    }
  else if (g_strcmp0 (method_name, "GetSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret = TRUE;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
            ret = FALSE;
        }

      if (!ret)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid selection: %d", num);
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ii)", start, end));
    }
  else if (g_strcmp0 (method_name, "AddSelection") == 0)
    {
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), NULL, NULL))
        {
          ret = FALSE;
        }
      else
        {
          gtk_editable_select_region (GTK_EDITABLE (widget), start, end);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "RemoveSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
            {
              ret = FALSE;
            }
          else
            {
              gtk_editable_select_region (GTK_EDITABLE (widget), end, end);
              ret = TRUE;
            }
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "SetSelection") == 0)
    {
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(iii)", &num, &start, &end);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), NULL, NULL))
            {
              ret = FALSE;
            }
          else
            {
              gtk_editable_select_region (GTK_EDITABLE (widget), start, end);
              ret = TRUE;
            }
        }
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
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
text_get_property (GDBusConnection  *connection,
                   const gchar      *sender,
                   const gchar      *object_path,
                   const gchar      *interface_name,
                   const gchar      *property_name,
                   GError          **error,
                   gpointer          user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "CharacterCount") == 0)
    {
      const char *text;
      int len;

      text = gtk_editable_get_text (GTK_EDITABLE (widget));
      len = g_utf8_strlen (text, -1);

      return g_variant_new_int32 (len);
    }
  else if (g_strcmp0 (property_name, "CaretOffset") == 0)
    {
      int offset;

      offset = gtk_editable_get_position (GTK_EDITABLE (widget));

      return g_variant_new_int32 (offset);
    }

  return NULL;
}

static const GDBusInterfaceVTable text_vtable = {
  text_handle_method,
  text_get_property,
  NULL,
};


static void
text_view_handle_method (GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "GetCaretOffset") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextMark *insert;
      GtkTextIter iter;
      int offset;

      insert = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      offset = gtk_text_iter_get_offset (&iter);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", offset));
    }
  else if (g_strcmp0 (method_name, "SetCaretOffset") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter iter;
      int offset;

      g_variant_get (parameters, "(i)", &offset);

      gtk_text_buffer_get_iter_at_offset (buffer,  &iter, offset);
      gtk_text_buffer_place_cursor (buffer, &iter);
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (widget), &iter, 0, FALSE, 0, 0);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
  else if (g_strcmp0 (method_name, "GetText") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int start, end;
      char *string;

      g_variant_get (parameters, "(ii)", &start, &end);

      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
      gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end);

      string = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", string));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetTextBeforeOffset") == 0)
    {
      int offset;
      AtspiTextBoundaryType boundary_type;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &boundary_type);

      string = gtk_text_view_get_text_before (GTK_TEXT_VIEW (widget), offset, boundary_type, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetTextAtOffset") == 0)
    {
      int offset;
      AtspiTextBoundaryType boundary_type;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &boundary_type);

      string = gtk_text_view_get_text_at (GTK_TEXT_VIEW (widget), offset, boundary_type, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetTextAfterOffset") == 0)
    {
      int offset;
      AtspiTextBoundaryType boundary_type;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &boundary_type);

      string = gtk_text_view_get_text_after (GTK_TEXT_VIEW (widget), offset, boundary_type, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetCharacterAtOffset") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      int offset;
      gunichar ch = 0;

      g_variant_get (parameters, "(i)", &offset);

      if (offset >= 0 && offset < gtk_text_buffer_get_char_count (buffer))
        {
          GtkTextIter start, end;
          char *string;
          gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
          end = start;
          gtk_text_iter_forward_char (&end);
          string = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
          ch = g_utf8_get_char (string);
          g_free (string);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ch));
    }
  else if (g_strcmp0 (method_name, "GetStringAtOffset") == 0)
    {
      int offset;
      AtspiTextGranularity granularity;
      char *string;
      int start, end;

      g_variant_get (parameters, "(iu)", &offset, &granularity);

      string = gtk_text_view_get_string_at (GTK_TEXT_VIEW (widget), offset, granularity, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      int start, end;

      g_variant_get (parameters, "(i)", &offset);

      gtk_text_buffer_get_run_attributes (buffer, &builder, offset, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetAttributeValue") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      const char *name;
      int start, end;
      GVariant *attrs;
      const char *val;

      g_variant_get (parameters, "(i&s)", &offset, &name);

      gtk_text_buffer_get_run_attributes (buffer, &builder, offset, &start, &end);

      attrs = g_variant_builder_end (&builder);
      if (!g_variant_lookup (attrs, name, "&s", &val))
        val = "";

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", val));
      g_variant_unref (attrs);
    }
  else if (g_strcmp0 (method_name, "GetAttributeRun") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      gboolean include_defaults;
      int start, end;

      g_variant_get (parameters, "(ib)", &offset, &include_defaults);

      if (include_defaults)
        gtk_text_view_add_default_attributes (GTK_TEXT_VIEW (widget), &builder);

      gtk_text_buffer_get_run_attributes (buffer, &builder, offset, &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetDefaultAttributes") == 0 ||
           g_strcmp0 (method_name, "GetDefaultAttributeSet") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));

      gtk_text_view_add_default_attributes (GTK_TEXT_VIEW (widget), &builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss})", &builder));
    }
  else if (g_strcmp0 (method_name, "GetNSelections") == 0)
    {
      int n = 0;

      if (gtk_text_buffer_get_selection_bounds (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)), NULL, NULL))
        n = 1;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", n));
    }
  else if (g_strcmp0 (method_name, "GetSelection") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int num;
      int start, end;
      gboolean ret = TRUE;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
            ret = FALSE;
          else
            {
              start = gtk_text_iter_get_offset (&start_iter);
              end = gtk_text_iter_get_offset (&end_iter);
            }
        }

      if (!ret)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid selection: %d", num);
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ii)", start, end));
    }
  else if (g_strcmp0 (method_name, "AddSelection") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL))
        {
          ret = FALSE;
        }
      else
        {
          gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
          gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end);
          gtk_text_buffer_select_range (buffer, &start_iter, &end_iter);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ret));
    }
  else if (g_strcmp0 (method_name, "RemoveSelection") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int num;
      gboolean ret;

      g_variant_get (parameters, "(i)", &num);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
            {
              ret = FALSE;
            }
          else
            {
              gtk_text_buffer_select_range (buffer, &end_iter, &end_iter);
              ret = TRUE;
            }
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "SetSelection") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int num;
      int start, end;
      gboolean ret;

      g_variant_get (parameters, "(iii)", &num, &start, &end);

      if (num != 0)
        ret = FALSE;
      else
        {
          if (!gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL))
            {
              ret = FALSE;
            }
          else
            {
              gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
              gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end);
              gtk_text_buffer_select_range (buffer, &end_iter, &end_iter);
              ret = TRUE;
            }
        }
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
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
text_view_get_property (GDBusConnection  *connection,
                        const gchar      *sender,
                        const gchar      *object_path,
                        const gchar      *interface_name,
                        const gchar      *property_name,
                        GError          **error,
                        gpointer          user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "CharacterCount") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      int len;

      len = gtk_text_buffer_get_char_count (buffer);

      return g_variant_new_int32 (len);
    }
  else if (g_strcmp0 (property_name, "CaretOffset") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextMark *insert;
      GtkTextIter iter;
      int offset;

      insert = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      offset = gtk_text_iter_get_offset (&iter);

      return g_variant_new_int32 (offset);
    }

  return NULL;
}

static const GDBusInterfaceVTable text_view_vtable = {
  text_view_handle_method,
  text_view_get_property,
  NULL,
};

const GDBusInterfaceVTable *
gtk_atspi_get_text_vtable (GtkWidget *widget)
{
  if (GTK_IS_LABEL (widget))
    return &label_vtable;
  else if (GTK_IS_TEXT (widget))
    return &text_vtable;
  else if (GTK_IS_TEXT_VIEW (widget))
    return &text_view_vtable;

  return NULL;
}
