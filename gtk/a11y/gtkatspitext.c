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

#include "gtkaccessibletextprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkdebug.h"
#include "gtkeditable.h"
#include "gtkentryprivate.h"
#include "gtkinscriptionprivate.h"
#include "gtklabelprivate.h"
#include "gtkpangoprivate.h"
#include "gtkpasswordentryprivate.h"
#include "gtksearchentryprivate.h"
#include "gtkspinbuttonprivate.h"
#include "gtktextbufferprivate.h"
#include "gtktextviewprivate.h"

#include <gio/gio.h>

static GtkAccessibleTextGranularity
atspi_granularity_to_gtk (AtspiTextGranularity granularity)
{
  switch (granularity)
    {
    case ATSPI_TEXT_GRANULARITY_CHAR:
      return GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER;
    case ATSPI_TEXT_GRANULARITY_WORD:
      return GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD;
    case ATSPI_TEXT_GRANULARITY_SENTENCE:
      return GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE;
    case ATSPI_TEXT_GRANULARITY_LINE:
      return GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE;
    case ATSPI_TEXT_GRANULARITY_PARAGRAPH:
      return GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH;
    default:
      g_assert_not_reached ();
    }
}

/* {{{ GtkAccessibleText */

static void
accessible_text_handle_method (GDBusConnection       *connection,
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
  GtkAccessibleText *accessible_text = GTK_ACCESSIBLE_TEXT (accessible);

  if (g_strcmp0 (method_name, "GetCaretOffset") == 0)
    {
      guint offset;

      offset = gtk_accessible_text_get_caret_position (accessible_text);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", (int)offset));
    }
  else if (g_strcmp0 (method_name, "SetCaretOffset") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "GetText") == 0)
    {
      int start, end;
      GBytes *contents;

      g_variant_get (parameters, "(ii)", &start, &end);

      contents = gtk_accessible_text_get_contents (accessible_text, start, end < 0 ? G_MAXUINT : end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", g_bytes_get_data (contents, NULL)));

      g_bytes_unref (contents);
    }
  else if (g_strcmp0 (method_name, "GetTextBeforeOffset") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "This method is deprecated in favor of GetStringAtOffset");
    }
  else if (g_strcmp0 (method_name, "GetTextAtOffset") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "This method is deprecated in favor of GetStringAtOffset");
    }
  else if (g_strcmp0 (method_name, "GetTextAfterOffset") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "This method is deprecated in favor of GetStringAtOffset");
    }
  else if (g_strcmp0 (method_name, "GetCharacterAtOffset") == 0)
    {
      int offset;
      gunichar ch = 0;

      g_variant_get (parameters, "(i)", &offset);

      GBytes *text = gtk_accessible_text_get_contents (accessible_text, offset, offset + 1);

      if (text != NULL)
        {
          const char *str = g_bytes_get_data (text, NULL);
          if (g_utf8_strlen (str, -1) > 0)
            ch = g_utf8_get_char (str);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ch));
    }
  else if (g_strcmp0 (method_name, "GetStringAtOffset") == 0)
    {
      unsigned int start, end;
      int offset;
      AtspiTextGranularity granularity;
      GBytes *bytes;

      g_variant_get (parameters, "(iu)", &offset, &granularity);

      bytes = gtk_accessible_text_get_contents_at (accessible_text, offset,
                                                   atspi_granularity_to_gtk (granularity),
                                                   &start, &end);

      if (bytes == NULL)
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", "", -1, -1));
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", g_bytes_get_data (bytes, NULL), start, end));

      g_bytes_unref (bytes);
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      gsize n_attrs = 0;
      GtkAccessibleTextRange *ranges = NULL;
      int start, end;
      char **attr_names = NULL;
      char **attr_values = NULL;

      g_variant_get (parameters, "(i)", &offset);

      gtk_accessible_text_get_attributes (accessible_text,
                                          offset,
                                          &n_attrs,
                                          &ranges,
                                          &attr_names,
                                          &attr_values);

      start = 0;
      end = G_MAXINT;

      for (int i = 0; i < n_attrs; i++)
        {
          g_variant_builder_add (&builder, "{ss}", attr_names[i], attr_values[i]);
          start = MAX (start, ranges[i].start);
          end = MIN (end, start + ranges[i].length);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));

      g_clear_pointer (&ranges, g_free);
      g_strfreev (attr_names);
      g_strfreev (attr_values);
    }
  else if (g_strcmp0 (method_name, "GetAttributeValue") == 0)
    {
      int offset;
      const char *name;
      const char *val = "";
      char **names, **values;
      GtkAccessibleTextRange *ranges;
      gsize n_ranges;

      g_variant_get (parameters, "(i&s)", &offset, &name);

      gtk_accessible_text_get_attributes (accessible_text, offset,
                                          &n_ranges, &ranges,
                                          &names, &values);

      for (unsigned i = 0; names[i] != NULL; i++)
        {
          if (g_strcmp0 (names[i], name) == 0)
            {
              val = values[i];
              break;
            }
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", val));

      g_strfreev (names);
      g_strfreev (values);
    }
  else if (g_strcmp0 (method_name, "GetAttributeRun") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      gboolean include_defaults = FALSE;
      int offset;
      gsize n_ranges = 0;
      GtkAccessibleTextRange *ranges = NULL;
      int start, end;
      char **attr_names = NULL;
      char **attr_values = NULL;
      gboolean res;

      g_variant_get (parameters, "(ib)", &offset, &include_defaults);

      res = gtk_accessible_text_get_attributes_run (accessible_text,
                                                    offset,
                                                    include_defaults,
                                                    &n_ranges,
                                                    &ranges,
                                                    &attr_names,
                                                    &attr_values);
      if (!res)
        {
          /* No attributes */
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, 0, 0));
          return;
        }

      for (unsigned i = 0; attr_names[i] != NULL; i++)
        g_variant_builder_add (&builder, "{ss}", attr_names[i], attr_values[i]);

      start = 0;
      end = G_MAXINT;
      for (unsigned i = 0; i < n_ranges; i++)
        {
          start = MAX (start, ranges[i].start);
          end = MIN (end, start + ranges[i].length);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));

      g_clear_pointer (&ranges, g_free);
      g_strfreev (attr_names);
      g_strfreev (attr_values);
    }
  else if (g_strcmp0 (method_name, "GetDefaultAttributes") == 0 ||
           g_strcmp0 (method_name, "GetDefaultAttributeSet") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      char **names, **values;

      gtk_accessible_text_get_default_attributes (accessible_text, &names, &values);

      for (unsigned i = 0; names[i] != NULL; i++)
        g_variant_builder_add (&builder, "{ss}", names[i], values[i]);

      g_strfreev (names);
      g_strfreev (values);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss})", &builder));
    }
  else if (g_strcmp0 (method_name, "GetOffsetAtPoint") == 0)
    {
      int x, y;
      guint coords_type;
      int nx, ny;
      graphene_point_t p;
      unsigned int offset;

      g_variant_get (parameters, "(iiu)", &x, &y, &coords_type);

      if (coords_type != ATSPI_COORD_TYPE_PARENT && coords_type != ATSPI_COORD_TYPE_WINDOW)
        {
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_NOT_SUPPORTED,
                                                         "Unsupported coordinate space");
          return;
        }

      gtk_at_spi_translate_coordinates_to_accessible (accessible, coords_type, x, y, &nx, &ny);

      p = GRAPHENE_POINT_INIT (nx, ny);
      if (!gtk_accessible_text_get_offset (accessible_text, &p, &offset))
        {
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_FAILED,
                                                         "Could not determine offset");
          return;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", (int)offset));
    }
  else if (g_strcmp0 (method_name, "GetNSelections") == 0)
    {
      gsize n_ranges;
      GtkAccessibleTextRange *ranges = NULL;

      if (!gtk_accessible_text_get_selection (accessible_text, &n_ranges, &ranges))
        n_ranges = 0;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", (int)n_ranges));

      g_clear_pointer (&ranges, g_free);
    }
  else if (g_strcmp0 (method_name, "GetSelection") == 0)
    {
      int num;
      gsize n_ranges;
      GtkAccessibleTextRange *ranges = NULL;

      g_variant_get (parameters, "(i)", &num);

      if (!gtk_accessible_text_get_selection (accessible_text, &n_ranges, &ranges))
        n_ranges = 0;

      if (num < 0 || num >= n_ranges)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid selection: %d", num);
      else
        {
          int start = ranges[num].start;
          int end = start + ranges[num].length;

          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ii)", start, end));
        }

      g_clear_pointer (&ranges, g_free);
    }
  else if (g_strcmp0 (method_name, "AddSelection") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "RemoveSelection") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "SetSelection") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "GetCharacterExtents") == 0)
    {
      int offset;
      unsigned int coords_type;
      graphene_rect_t extents;
      int x, y, w, h;

      g_variant_get (parameters, "(iu)", &offset, &coords_type);

      if (coords_type != ATSPI_COORD_TYPE_PARENT && coords_type != ATSPI_COORD_TYPE_WINDOW)
        {
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_NOT_SUPPORTED,
                                                         "Unsupported coordinate space");
          return;
        }

      if (!gtk_accessible_text_get_extents (accessible_text, offset, offset + 1, &extents))
        {
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_FAILED,
                                                         "Failed to get extents");
          return;
        }

      gtk_at_spi_translate_coordinates_from_accessible (accessible, coords_type, extents.origin.x, extents.origin.y, &x, &y);
      w = extents.size.width;
      h = extents.size.height;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(iiii)", x, y, w, h));
    }
  else if (g_strcmp0 (method_name, "GetRangeExtents") == 0)
    {
      int start, end;
      guint coords_type;
      graphene_rect_t extents;
      int x, y, w, h;

      g_variant_get (parameters, "(iiu)", &start, &end, &coords_type);

      if (coords_type != ATSPI_COORD_TYPE_PARENT && coords_type != ATSPI_COORD_TYPE_WINDOW)
        {
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_NOT_SUPPORTED,
                                                         "Unsupported coordinate space");
          return;
        }

      if (!gtk_accessible_text_get_extents (accessible_text, start, end, &extents))
        {
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_FAILED,
                                                         "Failed to get extents");
          return;
        }

      gtk_at_spi_translate_coordinates_from_accessible (accessible, coords_type, extents.origin.x, extents.origin.y, &x, &y);
      w = extents.size.width;
      h = extents.size.height;

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(iiii)", x, y, w, h));
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
accessible_text_get_property (GDBusConnection  *connection,
                              const gchar      *sender,
                              const gchar      *object_path,
                              const gchar      *interface_name,
                              const gchar      *property_name,
                              GError          **error,
                              gpointer          user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkAccessibleText *accessible_text = GTK_ACCESSIBLE_TEXT (accessible);

  if (g_strcmp0 (property_name, "CharacterCount") == 0)
    {
      GBytes *contents;
      const char *str;
      gsize len;

      contents = gtk_accessible_text_get_contents (accessible_text, 0, G_MAXUINT);
      str = g_bytes_get_data (contents, NULL);
      len = g_utf8_strlen (str, -1);
      g_bytes_unref (contents);

      return g_variant_new_int32 ((int) len);
    }
  else if (g_strcmp0 (property_name, "CaretOffset") == 0)
    {
      guint offset;

      offset = gtk_accessible_text_get_caret_position (accessible_text);

      return g_variant_new_int32 ((int) offset);
    }

  return NULL;
}

static const GDBusInterfaceVTable accessible_text_vtable = {
  accessible_text_handle_method,
  accessible_text_get_property,
  NULL,
};

/* }}} */
/* {{{ GtkEditable */

static GtkText *
gtk_editable_get_text_widget (GtkWidget *widget)
{
  if (GTK_IS_EDITABLE (widget))
    {
      GtkEditable *editable;
      guint redirects = 0;

      editable = GTK_EDITABLE (widget);

      do {
        if (GTK_IS_TEXT (editable))
          return GTK_TEXT (editable);

        if (++redirects >= 6)
          g_assert_not_reached ();

        editable = gtk_editable_get_delegate (editable);
      } while (editable != NULL);
    }

  return NULL;
}

static void
editable_handle_method (GDBusConnection       *connection,
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
  GtkText *text_widget = gtk_editable_get_text_widget (widget);

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
      PangoLayout *layout = gtk_text_get_layout (text_widget);
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
      PangoLayout *layout = gtk_text_get_layout (text_widget);
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
      PangoLayout *layout = gtk_text_get_layout (text_widget);
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
      PangoLayout *layout = gtk_text_get_layout (text_widget);
      int offset;
      AtspiTextGranularity granularity;
      char *string;
      unsigned int start, end;

      g_variant_get (parameters, "(iu)", &offset, &granularity);

      string = gtk_pango_get_string_at (layout, offset,
                                        atspi_granularity_to_gtk (granularity),
                                        &start, &end);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(sii)", string, start, end));
      g_free (string);
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      PangoLayout *layout = gtk_text_get_layout (text_widget);
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      unsigned int start, end;
      char **names, **values;

      g_variant_get (parameters, "(i)", &offset);

      gtk_pango_get_run_attributes (layout, offset, &names, &values, &start, &end);

      for (unsigned i = 0; names[i] != NULL; i++)
        g_variant_builder_add (&builder, "{ss}", names[i], values[i]);

      g_strfreev (names);
      g_strfreev (values);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetAttributeValue") == 0)
    {
      PangoLayout *layout = gtk_text_get_layout (text_widget);
      int offset;
      const char *name;
      unsigned int start, end;
      const char *val = "";
      char **names, **values;

      g_variant_get (parameters, "(i&s)", &offset, &name);

      gtk_pango_get_run_attributes (layout, offset, &names, &values, &start, &end);

      for (unsigned i = 0; names[i] != NULL; i++)
        {
          if (g_strcmp0 (names[i], name) == 0)
            {
              val = values[i];
              break;
            }
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", val));

      g_strfreev (names);
      g_strfreev (values);
    }
  else if (g_strcmp0 (method_name, "GetAttributeRun") == 0)
    {
      PangoLayout *layout = gtk_text_get_layout (text_widget);
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      int offset;
      gboolean include_defaults;
      unsigned int start, end;
      char **names, **values;

      g_variant_get (parameters, "(ib)", &offset, &include_defaults);

      if (include_defaults)
        {
          gtk_pango_get_default_attributes (layout, &names, &values);

          for (unsigned i = 0; names[i] != NULL; i++)
            g_variant_builder_add (&builder, "{ss}", names[i], values[i]);

          g_strfreev (names);
          g_strfreev (values);
        }

      gtk_pango_get_run_attributes (layout, offset, &names, &values, &start, &end);

      for (unsigned i = 0; names[i] != NULL; i++)
        g_variant_builder_add (&builder, "{ss}", names[i], values[i]);

      g_strfreev (names);
      g_strfreev (values);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss}ii)", &builder, start, end));
    }
  else if (g_strcmp0 (method_name, "GetDefaultAttributes") == 0 ||
           g_strcmp0 (method_name, "GetDefaultAttributeSet") == 0)
    {
      PangoLayout *layout = gtk_text_get_layout (text_widget);
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
      char **names, **values;

      gtk_pango_get_default_attributes (layout, &names, &values);

      for (unsigned i = 0; names[i] != NULL; i++)
        g_variant_builder_add (&builder, "{ss}", names[i], values[i]);

      g_strfreev (names);
      g_strfreev (values);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a{ss})", &builder));
    }
  else if (g_strcmp0 (method_name, "GetOffsetAtPoint") == 0)
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
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
editable_get_property (GDBusConnection  *connection,
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

static const GDBusInterfaceVTable editable_vtable = {
  editable_handle_method,
  editable_get_property,
  NULL,
};

/* }}} */

const GDBusInterfaceVTable *
gtk_atspi_get_text_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_ACCESSIBLE_TEXT (accessible))
    return &accessible_text_vtable;
  else if (GTK_IS_EDITABLE (accessible))
    return &editable_vtable;

  return NULL;
}

typedef struct {
  void (* text_changed)      (gpointer    data,
                              const char *kind,
                              int         start,
                              int         end,
                              const char *text);
  void (* selection_changed) (gpointer    data,
                              const char *kind,
                              int         cursor_position);

  gpointer data;
  GtkTextBuffer *buffer;
  int cursor_position;
  int selection_bound;
} TextChanged;

/* {{{ GtkEditable notification */

static void
insert_text_cb (GtkEditable *editable,
                char        *new_text,
                int          new_text_length,
                int         *position,
                TextChanged *changed)
{
  int length;

  if (new_text_length == 0)
    return;

  length = g_utf8_strlen (new_text, new_text_length);

  char *inserted_text = g_utf8_substring (new_text, 0, length);
  changed->text_changed (changed->data, "insert", *position - length, length, inserted_text);
  g_free (inserted_text);
}

static void
delete_text_cb (GtkEditable *editable,
                int          start,
                int          end,
                TextChanged *changed)
{
  char *text;

  if (start == end)
    return;

  text = gtk_editable_get_chars (editable, start, end);

  if (end < 0)
    end = g_utf8_strlen(text, -1);

  changed->text_changed (changed->data, "delete", start, end - start, text);
  g_free (text);
}

static void
update_selection (TextChanged *changed,
                  int          cursor_position,
                  int          selection_bound)
{
  gboolean caret_moved, bound_moved;
  gboolean had_selection, has_selection;

  caret_moved = cursor_position != changed->cursor_position;
  bound_moved = selection_bound != changed->selection_bound;
  had_selection = changed->cursor_position != changed->selection_bound;
  has_selection = cursor_position != selection_bound;

  if (!caret_moved && !bound_moved)
    return;

  changed->cursor_position = cursor_position;
  changed->selection_bound = selection_bound;

  if (caret_moved)
    changed->selection_changed (changed->data, "text-caret-moved", changed->cursor_position);

  if (had_selection || has_selection)
    changed->selection_changed (changed->data, "text-selection-changed", 0);
}

static void
notify_cb (GObject     *object,
           GParamSpec  *pspec,
           TextChanged *changed)
{
  if (g_strcmp0 (pspec->name, "cursor-position") == 0 ||
      g_strcmp0 (pspec->name, "selection-bound") == 0)
    {
      int cursor_position, selection_bound;

      gtk_editable_get_selection_bounds (GTK_EDITABLE (object), &cursor_position, &selection_bound);
      update_selection (changed, cursor_position, selection_bound);
    }
}

/* }}} */

void
gtk_atspi_connect_text_signals (GtkAccessible *accessible,
                                GtkAtspiTextChangedCallback text_changed,
                                GtkAtspiTextSelectionCallback selection_changed,
                                gpointer   data)
{
  TextChanged *changed;
  GtkText *text;

  if (GTK_IS_ACCESSIBLE_TEXT (accessible) || !GTK_IS_EDITABLE (accessible))
    return;

  changed = g_new0 (TextChanged, 1);
  changed->text_changed = text_changed;
  changed->selection_changed = selection_changed;
  changed->data = data;

  g_object_set_data_full (G_OBJECT (accessible), "accessible-text-data", changed, g_free);

  text = gtk_editable_get_text_widget (GTK_WIDGET (accessible));
  if (text)
    {
      g_signal_connect_after (text, "insert-text", G_CALLBACK (insert_text_cb), changed);
      g_signal_connect (text, "delete-text", G_CALLBACK (delete_text_cb), changed);
      g_signal_connect (text, "notify", G_CALLBACK (notify_cb), changed);

      gtk_editable_get_selection_bounds (GTK_EDITABLE (text), &changed->cursor_position, &changed->selection_bound);
    }
}

void
gtk_atspi_disconnect_text_signals (GtkAccessible *accessible)
{
  TextChanged *changed;
  GtkText *text;

  if (GTK_IS_ACCESSIBLE_TEXT (accessible) || !GTK_IS_EDITABLE (accessible))
    return;

  changed = g_object_get_data (G_OBJECT (accessible), "accessible-text-data");
  if (changed == NULL)
    return;

  text = gtk_editable_get_text_widget (GTK_WIDGET (accessible));

  if (text)
    {
      g_signal_handlers_disconnect_by_func (text, insert_text_cb, changed);
      g_signal_handlers_disconnect_by_func (text, delete_text_cb, changed);
      g_signal_handlers_disconnect_by_func (text, notify_cb, changed);
    }

  g_object_set_data (G_OBJECT (accessible), "accessible-text-data", NULL);
}

/* vim:set foldmethod=marker: */
