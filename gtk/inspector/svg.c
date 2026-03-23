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

#include "gtkbinlayout.h"
#include "gtkstack.h"
#include "gtksvgprivate.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtktextview.h"
#include "gtktextbuffer.h"

struct _GtkInspectorSvg
{
  GtkWidget parent;

  GObject *object;
  GtkTextView *xml_view;
  GtkTextBuffer *xml_buffer;

  guint timeout;
  GList *errors;
};

typedef struct _GtkInspectorSvgClass
{
  GtkWidgetClass parent_class;
} GtkInspectorSvgClass;

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
  GBytes *xml = gtk_svg_serialize (svg);

  g_signal_handlers_block_by_func (self->xml_buffer, xml_changed, self);
  gtk_text_buffer_set_text (self->xml_buffer,
                            g_bytes_get_data (xml, NULL),
                            g_bytes_get_size (xml));
  update_timeout (self);
  g_signal_handlers_unblock_by_func (self->xml_buffer, xml_changed, self);
  g_bytes_unref (xml);
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
}

static void
dispose (GObject *o)
{
  GtkInspectorSvg *self = GTK_INSPECTOR_SVG (o);

  g_list_free_full (self->errors, svg_error_free);
  self->errors = NULL;

  if (self->timeout)
    {
      g_source_remove (self->timeout);
      self->timeout = 0;
    }

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/svg.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSvg, xml_view);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSvg, xml_buffer);
  gtk_widget_class_bind_template_callback (widget_class, xml_changed);
  gtk_widget_class_bind_template_callback (widget_class, query_tooltip_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

// vim: set et sw=2 ts=2:
