/*
 * Copyright (c) 2026 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "svg.h"
#include "window.h"

#include "gtkalertdialog.h"
#include "gtkbinlayout.h"
#include "gtkdialogerror.h"
#include "gtkfiledialog.h"
#include "gtkstack.h"
#include "svg/gtksvgserializeprivate.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtktextview.h"
#include "gtktextbuffer.h"
#include "gtkoverlay.h"
#include "gtktogglebutton.h"

struct _GtkInspectorSvg
{
  GtkWidget parent;

  GObject *object;
  GtkOverlay *overlay;
  GtkTextView *xml_view;
  GtkTextBuffer *xml_buffer;
  GtkToggleButton *button;

  guint timeout;
  GList *errors;

  GtkSvgSerializeFlags flags;
};

typedef struct _GtkInspectorSvgClass
{
  GtkWidgetClass parent_class;
} GtkInspectorSvgClass;

enum {
  PROP_0,
  PROP_BUTTON,
  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (GtkInspectorSvg, gtk_inspector_svg, GTK_TYPE_WIDGET)

typedef struct
{
  GError *error;
  GtkTextIter start;
  GtkTextIter end;
} SvgError;

static void
svg_error_free (gpointer data)
{
  SvgError *error = data;

  g_error_free (error->error);
  g_free (error);
}

typedef struct
{
  GtkInspectorSvg *self;
  const char *text;
} ErrorData;

static void
error_cb (GtkSvg   *svg,
          GError   *error,
          gpointer  data)
{
/* Without GLib 2.88, we don't get usable location
 * information from GMarkup, so don't try to highlight
 * errors
 */
#if GLIB_CHECK_VERSION (2, 88, 0)
  ErrorData *d = data;
  GtkInspectorSvg *self = d->self;
  SvgError *svg_error;
  size_t offset;
  const GtkSvgLocation *start, *end;
  const char *tag;

  if (error->domain != GTK_SVG_ERROR)
    return;

  start = gtk_svg_error_get_start (error);
  end = gtk_svg_error_get_end (error);

  svg_error = g_new (SvgError, 1);
  svg_error->error = g_error_copy (error);

  offset = g_utf8_pointer_to_offset (d->text, d->text + start->bytes);
  gtk_text_buffer_get_iter_at_offset (self->xml_buffer, &svg_error->start, offset);
  offset = g_utf8_pointer_to_offset (d->text, d->text + end->bytes);
  gtk_text_buffer_get_iter_at_offset (self->xml_buffer, &svg_error->end, offset);

  self->errors = g_list_append (self->errors, svg_error);

  if (gtk_text_iter_equal (&svg_error->start, &svg_error->end))
    gtk_text_iter_forward_chars (&svg_error->end, 1);

  if (error->code == GTK_SVG_ERROR_IGNORED_ELEMENT)
    tag = "ignored";
  else if (error->code == GTK_SVG_ERROR_NOT_IMPLEMENTED)
    tag = "unimplemented";
  else
    tag = "error";

  gtk_text_buffer_apply_tag_by_name (self->xml_buffer,
                                     tag,
                                     &svg_error->start,
                                     &svg_error->end);
#endif
}

static void
update_timeout (gpointer data)
{
  GtkInspectorSvg *self = data;
  GtkSvg *svg = GTK_SVG (self->object);
  GtkTextIter start, end;
  char *text;
  GBytes *bytes = NULL;
  gulong handler;
  ErrorData d;

  self->timeout = 0;

  gtk_text_buffer_get_bounds (self->xml_buffer, &start, &end);
  gtk_text_buffer_remove_all_tags (self->xml_buffer, &start, &end);

  text = gtk_text_buffer_get_text (self->xml_buffer, &start, &end, FALSE);
  bytes = g_bytes_new_take (text, strlen (text));

  g_list_free_full (self->errors, svg_error_free);
  self->errors = NULL;

  d.self = self;
  d.text = text;

  handler = g_signal_connect (svg, "error", G_CALLBACK (error_cb), &d);
  gtk_svg_load_from_bytes (svg, bytes);
  g_signal_handler_disconnect (svg, handler);
}

static void
xml_changed (GtkInspectorSvg *self)
{
  if (self->timeout != 0)
    g_source_remove (self->timeout);
  self->timeout = g_timeout_add_once (100, update_timeout, self);
}

static void
update_xml (GtkInspectorSvg *self)
{
  GtkSvg *svg = GTK_SVG (self->object);
  GBytes *xml;

  xml = gtk_svg_serialize_full (svg, NULL, 0, self->flags);

  g_signal_handlers_block_by_func (self->xml_buffer, xml_changed, self);
  gtk_text_buffer_set_text (self->xml_buffer,
                            g_bytes_get_data (xml, NULL),
                            g_bytes_get_size (xml));
  g_signal_handlers_unblock_by_func (self->xml_buffer, xml_changed, self);
  g_bytes_unref (xml);
}

static void
save_to_file (GtkInspectorSvg *self,
              GFile           *file)
{
  GtkTextIter start, end;
  char *text;
  GError *error = NULL;

  gtk_text_buffer_get_bounds (self->xml_buffer, &start, &end);
  text = gtk_text_buffer_get_text (self->xml_buffer, &start, &end, FALSE);

  if (!g_file_replace_contents (file,
                                text, strlen (text),
                                NULL,
                                FALSE,
                                0,
                                NULL,
                                NULL,
                                &error))
    {
      GtkAlertDialog *alert;

      alert = gtk_alert_dialog_new ("This did not work");
      gtk_alert_dialog_set_detail (alert, error->message);
      gtk_alert_dialog_show (alert, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
      g_object_unref (alert);
      g_error_free (error);
    }

  g_free (text);
}

static void
save_cb (GObject      *source,
         GAsyncResult *result,
         gpointer      data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkInspectorSvg *self = data;
  GError *error = NULL;
  GFile *file;

  file = gtk_file_dialog_save_finish (dialog, result, &error);
  if (!file)
    {
      if (g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED))
        g_print ("%s", error->message);
      g_error_free (error);
      return;
    }

  save_to_file (self, file);

  g_object_unref (file);
}

static void
save_as (GtkWidget  *widget,
         const char *action_name,
         GVariant   *parameter)
{
  GtkFileDialog *dialog;

  dialog = gtk_file_dialog_new ();

  gtk_file_dialog_set_title (dialog, "Save SVG");
  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_initial_name (dialog, "saved.svg");
  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (widget)),
                        NULL,
                        save_cb,
                        widget);
  g_object_unref (dialog);
}

static void
copy_clipboard (GtkWidget  *widget,
                const char *action_name,
                GVariant   *parameter)
{
  GtkInspectorSvg *self = GTK_INSPECTOR_SVG (widget);
  GdkClipboard *clipboard;
  GtkTextIter start, end;
  char *text;

  gtk_text_buffer_get_bounds (self->xml_buffer, &start, &end);
  text = gtk_text_buffer_get_text (self->xml_buffer, &start, &end, FALSE);

  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_set_text (clipboard, text);
  g_free (text);
}

static void
button_clicked (GtkButton       *button,
                GtkInspectorSvg *self)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    self->flags = GTK_SVG_SERIALIZE_AT_CURRENT_TIME |
                  GTK_SVG_SERIALIZE_INCLUDE_STATE |
                  GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS |
                  GTK_SVG_SERIALIZE_NO_COMPAT;
  else
    self->flags = GTK_SVG_SERIALIZE_DEFAULT;

  if (self->object)
    update_xml (self);
}

static gboolean
query_tooltip_cb (GtkWidget       *widget,
                  int              x,
                  int              y,
                  gboolean         keyboard_tip,
                  GtkTooltip      *tooltip,
                  GtkInspectorSvg *self)
{
  GtkTextIter iter;

  if (!GTK_IS_SVG (self->object))
    return FALSE;

  if (keyboard_tip)
    {
      int offset;

      g_object_get (self->xml_view, "cursor-position", &offset, NULL);
      gtk_text_buffer_get_iter_at_offset (self->xml_buffer, &iter, offset);
    }
  else
    {
      int bx, by, trailing;

      gtk_text_view_window_to_buffer_coords (self->xml_view,
                                             GTK_TEXT_WINDOW_TEXT,
                                             x, y, &bx, &by);
      gtk_text_view_get_iter_at_position (self->xml_view, &iter, &trailing, bx, by);
    }

  for (GList *l = self->errors; l; l = l->next)
    {
      SvgError *error = l->data;

      if (gtk_text_iter_in_range (&iter, &error->start, &error->end))
        {
          gtk_tooltip_set_text (tooltip, error->error->message);
          return TRUE;
        }
    }

  return FALSE;
}

void
gtk_inspector_svg_set_object (GtkInspectorSvg *self,
                              GObject         *object)
{
  GtkWidget *stack;
  GtkStackPage *page;

  g_set_object (&self->object, object);

  stack = gtk_widget_get_parent (GTK_WIDGET (self));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (self));

  if (GTK_IS_SVG (self->object))
    {
      gtk_stack_page_set_visible (page, TRUE);
      update_xml (self);
    }
  else
    {
      gtk_stack_page_set_visible (page, FALSE);
    }
}

static void
gtk_inspector_svg_init (GtkInspectorSvg *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->flags = GTK_SVG_SERIALIZE_DEFAULT;
}

static void
constructed (GObject *object)
{
  GtkInspectorSvg *sl = GTK_INSPECTOR_SVG (object);

  G_OBJECT_CLASS (gtk_inspector_svg_parent_class)->constructed (object);

  g_signal_connect (sl->button, "clicked", G_CALLBACK (button_clicked), sl);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorSvg *sl = GTK_INSPECTOR_SVG (object);

  switch (param_id)
    {
    case PROP_BUTTON:
      g_value_set_object (value, sl->button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorSvg *sl = GTK_INSPECTOR_SVG (object);

  switch (param_id)
    {
    case PROP_BUTTON:
      sl->button = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
dispose (GObject *o)
{
  GtkInspectorSvg *self = GTK_INSPECTOR_SVG (o);

  g_list_free_full (self->errors, svg_error_free);
  self->errors = NULL;

  g_clear_handle_id (&self->timeout, g_source_remove);

  g_clear_object (&self->object);

  gtk_widget_dispose_template (GTK_WIDGET (o), GTK_TYPE_INSPECTOR_SVG);

  G_OBJECT_CLASS (gtk_inspector_svg_parent_class)->dispose (o);
}

static void
gtk_inspector_svg_class_init (GtkInspectorSvgClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;

  props[PROP_BUTTON] = g_param_spec_object ("button", NULL, NULL,
                                            GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, N_PROPS, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/svg.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSvg, overlay);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSvg, xml_view);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSvg, xml_buffer);
  gtk_widget_class_bind_template_callback (widget_class, xml_changed);
  gtk_widget_class_bind_template_callback (widget_class, query_tooltip_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_install_action (widget_class, "widget.save-as", NULL, save_as);
  gtk_widget_class_install_action (widget_class, "widget.copy-clipboard", NULL, copy_clipboard);
}

// vim: set et sw=2 ts=2:
