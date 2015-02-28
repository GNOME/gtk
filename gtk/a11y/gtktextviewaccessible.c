/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtktextviewaccessibleprivate.h"
#include "gtk/gtkwidgetprivate.h"

struct _GtkTextViewAccessiblePrivate
{
  gint insert_offset;
  gint selection_bound;
};

static void       insert_text_cb       (GtkTextBuffer    *buffer,
                                                        GtkTextIter      *arg1,
                                                        gchar            *arg2,
                                                        gint             arg3,
                                                        gpointer         user_data);
static void       delete_range_cb      (GtkTextBuffer    *buffer,
                                                        GtkTextIter      *arg1,
                                                        GtkTextIter      *arg2,
                                                        gpointer         user_data);
static void       mark_set_cb          (GtkTextBuffer    *buffer,
                                                        GtkTextIter      *arg1,
                                                        GtkTextMark      *arg2,
                                                        gpointer         user_data);


static void atk_editable_text_interface_init      (AtkEditableTextIface      *iface);
static void atk_text_interface_init               (AtkTextIface              *iface);
static void atk_streamable_content_interface_init (AtkStreamableContentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTextViewAccessible, gtk_text_view_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkTextViewAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT, atk_editable_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_STREAMABLE_CONTENT, atk_streamable_content_interface_init))


static void
gtk_text_view_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_text_view_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_TEXT;
}

static void
gtk_text_view_accessible_notify_gtk (GObject    *obj,
                                     GParamSpec *pspec)
{
  AtkObject *atk_obj;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (obj));

  if (!strcmp (pspec->name, "editable"))
    {
      gboolean editable;

      editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (obj));
      atk_object_notify_state_change (atk_obj, ATK_STATE_EDITABLE, editable);
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_text_view_accessible_parent_class)->notify_gtk (obj, pspec);
}

static AtkStateSet*
gtk_text_view_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_text_view_accessible_parent_class)->ref_state_set (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    {
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
      return state_set;
    }

  if (gtk_text_view_get_editable (GTK_TEXT_VIEW (widget)))
    atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);
  atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

  return state_set;
}

static void
gtk_text_view_accessible_change_buffer (GtkTextViewAccessible *accessible,
                                        GtkTextBuffer         *old_buffer,
                                        GtkTextBuffer         *new_buffer)
{
  if (old_buffer)
    {
      g_signal_handlers_disconnect_matched (old_buffer, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, accessible);

      g_signal_emit_by_name (accessible,
                             "text-changed::delete",
                             0,
                             gtk_text_buffer_get_char_count (old_buffer));
    }

  if (new_buffer)
    {
      g_signal_connect_after (new_buffer, "insert-text", G_CALLBACK (insert_text_cb), accessible);
      g_signal_connect (new_buffer, "delete-range", G_CALLBACK (delete_range_cb), accessible);
      g_signal_connect_after (new_buffer, "mark-set", G_CALLBACK (mark_set_cb), accessible);

      g_signal_emit_by_name (accessible,
                             "text-changed::insert",
                             0,
                             gtk_text_buffer_get_char_count (new_buffer));
    }
}

static void
gtk_text_view_accessible_widget_set (GtkAccessible *accessible)
{
  gtk_text_view_accessible_change_buffer (GTK_TEXT_VIEW_ACCESSIBLE (accessible),
                                          NULL,
                                          gtk_text_view_get_buffer (GTK_TEXT_VIEW (gtk_accessible_get_widget (accessible))));
}

static void
gtk_text_view_accessible_widget_unset (GtkAccessible *accessible)
{
  gtk_text_view_accessible_change_buffer (GTK_TEXT_VIEW_ACCESSIBLE (accessible),
                                          gtk_text_view_get_buffer (GTK_TEXT_VIEW (gtk_accessible_get_widget (accessible))),
                                          NULL);
}

static void
gtk_text_view_accessible_class_init (GtkTextViewAccessibleClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GtkAccessibleClass *accessible_class = GTK_ACCESSIBLE_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  accessible_class->widget_set = gtk_text_view_accessible_widget_set;
  accessible_class->widget_unset = gtk_text_view_accessible_widget_unset;

  class->ref_state_set = gtk_text_view_accessible_ref_state_set;
  class->initialize = gtk_text_view_accessible_initialize;

  widget_class->notify_gtk = gtk_text_view_accessible_notify_gtk;
}

static void
gtk_text_view_accessible_init (GtkTextViewAccessible *accessible)
{
  accessible->priv = gtk_text_view_accessible_get_instance_private (accessible);
}

static gchar *
gtk_text_view_accessible_get_text (AtkText *text,
                                   gint     start_offset,
                                   gint     end_offset)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gchar *
gtk_text_view_accessible_get_text_after_offset (AtkText         *text,
                                                gint             offset,
                                                AtkTextBoundary  boundary_type,
                                                gint            *start_offset,
                                                gint            *end_offset)
{
  GtkWidget *widget;
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter pos;
  GtkTextIter start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;
  if (boundary_type == ATK_TEXT_BOUNDARY_LINE_START)
    {
      gtk_text_view_forward_display_line (view, &end);
      start = end;
      gtk_text_view_forward_display_line (view, &end);
    }
  else if (boundary_type == ATK_TEXT_BOUNDARY_LINE_END)
    {
      gtk_text_view_forward_display_line_end (view, &end);
      start = end;
      gtk_text_view_forward_display_line (view, &end);
      gtk_text_view_forward_display_line_end (view, &end);
    }
  else
    _gtk_text_buffer_get_text_after (buffer, boundary_type, &pos, &start, &end);

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

static gchar *
gtk_text_view_accessible_get_text_at_offset (AtkText         *text,
                                             gint             offset,
                                             AtkTextBoundary  boundary_type,
                                             gint            *start_offset,
                                             gint            *end_offset)
{
  GtkWidget *widget;
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter pos;
  GtkTextIter start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;
  if (boundary_type == ATK_TEXT_BOUNDARY_LINE_START)
    {
      gtk_text_view_backward_display_line_start (view, &start);
      gtk_text_view_forward_display_line (view, &end);
    }
  else if (boundary_type == ATK_TEXT_BOUNDARY_LINE_END)
    {
      gtk_text_view_backward_display_line_start (view, &start);
      if (!gtk_text_iter_is_start (&start))
        {
          gtk_text_view_backward_display_line (view, &start);
          gtk_text_view_forward_display_line_end (view, &start);
        }
      gtk_text_view_forward_display_line_end (view, &end);
    }
  else
    _gtk_text_buffer_get_text_at (buffer, boundary_type, &pos, &start, &end);

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

static gchar *
gtk_text_view_accessible_get_text_before_offset (AtkText         *text,
                                                 gint             offset,
                                                 AtkTextBoundary  boundary_type,
                                                 gint            *start_offset,
                                                 gint            *end_offset)
{
  GtkWidget *widget;
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter pos;
  GtkTextIter start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  if (boundary_type == ATK_TEXT_BOUNDARY_LINE_START)
    {
      gtk_text_view_backward_display_line_start (view, &start);
      end = start;
      gtk_text_view_backward_display_line (view, &start);
      gtk_text_view_backward_display_line_start (view, &start);
    }
  else if (boundary_type == ATK_TEXT_BOUNDARY_LINE_END)
    {
      gtk_text_view_backward_display_line_start (view, &start);
      if (!gtk_text_iter_is_start (&start))
        {
          gtk_text_view_backward_display_line (view, &start);
          end = start;
          gtk_text_view_forward_display_line_end (view, &end);
          if (!gtk_text_iter_is_start (&start))
            {
              if (gtk_text_view_backward_display_line (view, &start))
                gtk_text_view_forward_display_line_end (view, &start);
              else
                gtk_text_iter_set_offset (&start, 0);
            }
        }
      else
        end = start;
    }
  else
    _gtk_text_buffer_get_text_before (buffer, boundary_type, &pos, &start, &end);

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

static gunichar
gtk_text_view_accessible_get_character_at_offset (AtkText *text,
                                                  gint     offset)
{
  GtkWidget *widget;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  gchar *string;
  gunichar unichar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return '\0';

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  if (offset >= gtk_text_buffer_get_char_count (buffer))
    return '\0';

  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
  end = start;
  gtk_text_iter_forward_char (&end);
  string = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
  unichar = g_utf8_get_char (string);
  g_free (string);

  return unichar;
}

static gint
gtk_text_view_accessible_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  return gtk_text_buffer_get_char_count (buffer);
}

static gint
get_insert_offset (GtkTextBuffer *buffer)
{
  GtkTextMark *insert;
  GtkTextIter iter;

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  return gtk_text_iter_get_offset (&iter);
}

static gint
gtk_text_view_accessible_get_caret_offset (AtkText *text)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  return get_insert_offset (buffer);
}

static gboolean
gtk_text_view_accessible_set_caret_offset (AtkText *text,
                                           gint     offset)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);

  gtk_text_buffer_get_iter_at_offset (buffer,  &iter, offset);
  gtk_text_buffer_place_cursor (buffer, &iter);
  gtk_text_view_scroll_to_iter (view, &iter, 0, FALSE, 0, 0);

  return TRUE;
}

static gint
gtk_text_view_accessible_get_offset_at_point (AtkText      *text,
                                              gint          x,
                                              gint          y,
                                              AtkCoordType  coords)
{
  GtkTextView *view;
  GtkTextIter iter;
  gint x_widget, y_widget, x_window, y_window, buff_x, buff_y;
  GtkWidget *widget;
  GdkWindow *window;
  GdkRectangle rect;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return -1;

  view = GTK_TEXT_VIEW (widget);
  window = gtk_text_view_get_window (view, GTK_TEXT_WINDOW_WIDGET);
  gdk_window_get_origin (window, &x_widget, &y_widget);

  if (coords == ATK_XY_SCREEN)
    {
      x = x - x_widget;
      y = y - y_widget;
    }
  else if (coords == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (window);
      gdk_window_get_origin (window, &x_window, &y_window);

      x = x - x_widget + x_window;
      y = y - y_widget + y_window;
    }
  else
    return -1;

  gtk_text_view_window_to_buffer_coords (view, GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &buff_x, &buff_y);
  gtk_text_view_get_visible_rect (view, &rect);

  /* Clamp point to visible rectangle */
  buff_x = CLAMP (buff_x, rect.x, rect.x + rect.width - 1);
  buff_y = CLAMP (buff_y, rect.y, rect.y + rect.height - 1);

  gtk_text_view_get_iter_at_location (view, &iter, buff_x, buff_y);

  /* The iter at a location sometimes points to the next character.
   * See bug 111031. We work around that
   */
  gtk_text_view_get_iter_location (view, &iter, &rect);
  if (buff_x < rect.x)
    gtk_text_iter_backward_char (&iter);
  return gtk_text_iter_get_offset (&iter);
}

static void
gtk_text_view_accessible_get_character_extents (AtkText      *text,
                                                gint          offset,
                                                gint         *x,
                                                gint         *y,
                                                gint         *width,
                                                gint         *height,
                                                AtkCoordType  coords)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkWidget *widget;
  GdkRectangle rectangle;
  GdkWindow *window;
  gint x_widget, y_widget, x_window, y_window;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
  gtk_text_view_get_iter_location (view, &iter, &rectangle);

  window = gtk_text_view_get_window (view, GTK_TEXT_WINDOW_WIDGET);
  gdk_window_get_origin (window, &x_widget, &y_widget);

  *height = rectangle.height;
  *width = rectangle.width;

  gtk_text_view_buffer_to_window_coords (view, GTK_TEXT_WINDOW_WIDGET,
    rectangle.x, rectangle.y, x, y);
  if (coords == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (window);
      gdk_window_get_origin (window, &x_window, &y_window);
      *x += x_widget - x_window;
      *y += y_widget - y_window;
    }
  else if (coords == ATK_XY_SCREEN)
    {
      *x += x_widget;
      *y += y_widget;
    }
  else
    {
      *x = 0;
      *y = 0;
      *height = 0;
      *width = 0;
    }
}

static AtkAttributeSet *
add_text_attribute (AtkAttributeSet  *attributes,
                    AtkTextAttribute  attr,
                    gchar            *value)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = value;

  return g_slist_prepend (attributes, at);
}

static AtkAttributeSet *
add_text_int_attribute (AtkAttributeSet  *attributes,
                        AtkTextAttribute  attr,
                        gint              i)

{
  gchar *value;

  value = g_strdup (atk_text_attribute_get_value (attr, i));

  return add_text_attribute (attributes, attr, value);
}

static AtkAttributeSet *
gtk_text_view_accessible_get_run_attributes (AtkText *text,
                                             gint     offset,
                                             gint    *start_offset,
                                             gint    *end_offset)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkWidget *widget;
  GtkTextIter iter;
  AtkAttributeSet *attrib_set = NULL;
  GSList *tags, *temp_tags;
  gdouble scale = 1;
  gboolean val_set = FALSE;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  gtk_text_iter_forward_to_tag_toggle (&iter, NULL);
  *end_offset = gtk_text_iter_get_offset (&iter);

  gtk_text_iter_backward_to_tag_toggle (&iter, NULL);
  *start_offset = gtk_text_iter_get_offset (&iter);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  tags = gtk_text_iter_get_tags (&iter);
  tags = g_slist_reverse (tags);

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "style-set", &val_set, NULL);
      if (val_set)
        {
          PangoStyle style;
          g_object_get (tag, "style", &style, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_STYLE, style);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "variant-set", &val_set, NULL);
      if (val_set)
        {
          PangoVariant variant;
          g_object_get (tag, "variant", &variant, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_VARIANT, variant);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "stretch-set", &val_set, NULL);
      if (val_set)
        {
          PangoStretch stretch;
          g_object_get (tag, "stretch", &stretch, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_STRETCH, stretch);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "justification-set", &val_set, NULL);
      if (val_set)
        {
          GtkJustification justification;
          g_object_get (tag, "justification", &justification, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_JUSTIFICATION, justification);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      GtkTextDirection direction;

      g_object_get (tag, "direction", &direction, NULL);

      if (direction != GTK_TEXT_DIR_NONE)
        {
          val_set = TRUE;
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_DIRECTION, direction);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "wrap-mode-set", &val_set, NULL);
      if (val_set)
        {
          GtkWrapMode wrap_mode;
          g_object_get (tag, "wrap-mode", &wrap_mode, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_WRAP_MODE, wrap_mode);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "foreground-set", &val_set, NULL);
      if (val_set)
        {
          GdkRGBA *rgba;
          gchar *value;

          g_object_get (tag, "foreground-rgba", &rgba, NULL);
          value = g_strdup_printf ("%u,%u,%u",
                                   (guint) rgba->red * 65535,
                                   (guint) rgba->green * 65535,
                                   (guint) rgba->blue * 65535);
          gdk_rgba_free (rgba);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_FG_COLOR, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "background-set", &val_set, NULL);
      if (val_set)
        {
          GdkRGBA *rgba;
          gchar *value;

          g_object_get (tag, "background-rgba", &rgba, NULL);
          value = g_strdup_printf ("%u,%u,%u",
                                   (guint) rgba->red * 65535,
                                   (guint) rgba->green * 65535,
                                   (guint) rgba->blue * 65535);
          gdk_rgba_free (rgba);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_BG_COLOR, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "family-set", &val_set, NULL);

      if (val_set)
        {
          gchar *value;
          g_object_get (tag, "family", &value, NULL);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_FAMILY_NAME, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "language-set", &val_set, NULL);

      if (val_set)
        {
          gchar *value;
          g_object_get (tag, "language", &value, NULL);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_LANGUAGE, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "weight-set", &val_set, NULL);

      if (val_set)
        {
          gint weight;
          gchar *value;
          g_object_get (tag, "weight", &weight, NULL);
          value = g_strdup_printf ("%d", weight);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_WEIGHT, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  /* scale is special as the effective value is the product
   * of all specified values
   */
  temp_tags = tags;
  while (temp_tags)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      gboolean scale_set;

      g_object_get (tag, "scale-set", &scale_set, NULL);
      if (scale_set)
        {
          gdouble font_scale;
          g_object_get (tag, "scale", &font_scale, NULL);
          val_set = TRUE;
          scale *= font_scale;
        }
      temp_tags = temp_tags->next;
    }
  if (val_set)
    {
      gchar *value;
      value = g_strdup_printf ("%g", scale);
      attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_SCALE, value);
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "size-set", &val_set, NULL);
      if (val_set)
        {
          gint size;
          gchar *value;
          g_object_get (tag, "size", &size, NULL);
          value = g_strdup_printf ("%i", size);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_SIZE, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "strikethrough-set", &val_set, NULL);
      if (val_set)
        {
          gboolean strikethrough;
          g_object_get (tag, "strikethrough", &strikethrough, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_STRIKETHROUGH, strikethrough);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "underline-set", &val_set, NULL);
      if (val_set)
        {
          PangoUnderline underline;
          g_object_get (tag, "underline", &underline, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_UNDERLINE, underline);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "rise-set", &val_set, NULL);
      if (val_set)
        {
          gint rise;
          gchar *value;
          g_object_get (tag, "rise", &rise, NULL);
          value = g_strdup_printf ("%i", rise);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_RISE, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "background-full-height-set", &val_set, NULL);
      if (val_set)
        {
          gboolean bg_full_height;
          g_object_get (tag, "background-full-height", &bg_full_height, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_BG_FULL_HEIGHT, bg_full_height);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "pixels-inside-wrap-set", &val_set, NULL);
      if (val_set)
        {
          gint pixels;
          gchar *value;
          g_object_get (tag, "pixels-inside-wrap", &pixels, NULL);
          value = g_strdup_printf ("%i", pixels);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "pixels-below-lines-set", &val_set, NULL);
      if (val_set)
        {
          gint pixels;
          gchar *value;
          g_object_get (tag, "pixels-below-lines", &pixels, NULL);
          value = g_strdup_printf ("%i", pixels);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_PIXELS_BELOW_LINES, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "pixels-above-lines-set", &val_set, NULL);
      if (val_set)
        {
          gint pixels;
          gchar *value;
          g_object_get (tag, "pixels-above-lines", &pixels, NULL);
          value = g_strdup_printf ("%i", pixels);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_PIXELS_ABOVE_LINES, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "editable-set", &val_set, NULL);
      if (val_set)
        {
          gboolean editable;
          g_object_get (tag, "editable", &editable, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_EDITABLE, editable);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "invisible-set", &val_set, NULL);
      if (val_set)
        {
          gboolean invisible;
          g_object_get (tag, "invisible", &invisible, NULL);
          attrib_set = add_text_int_attribute (attrib_set, ATK_TEXT_ATTR_INVISIBLE, invisible);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "indent-set", &val_set, NULL);
      if (val_set)
        {
          gint indent;
          gchar *value;
          g_object_get (tag, "indent", &indent, NULL);
          value = g_strdup_printf ("%i", indent);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_INDENT, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "right-margin-set", &val_set, NULL);
      if (val_set)
        {
          gint margin;
          gchar *value;
          g_object_get (tag, "right-margin", &margin, NULL);
          value = g_strdup_printf ("%i", margin);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_RIGHT_MARGIN, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "left-margin-set", &val_set, NULL);
      if (val_set)
        {
          gint margin;
          gchar *value;
          g_object_get (tag, "left-margin", &margin, NULL);
          value = g_strdup_printf ("%i", margin);
          attrib_set = add_text_attribute (attrib_set, ATK_TEXT_ATTR_LEFT_MARGIN, value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  g_slist_free (tags);
  return attrib_set;
}

static AtkAttributeSet *
gtk_text_view_accessible_get_default_attributes (AtkText *text)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextAttributes *text_attrs;
  AtkAttributeSet *attributes;
  PangoFontDescription *font;
  gchar *value;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  text_attrs = gtk_text_view_get_default_attributes (view);

  attributes = NULL;

  font = text_attrs->font;

  if (font)
    {
      attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_STYLE,
                                           pango_font_description_get_style (font));

      attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_VARIANT,
                                           pango_font_description_get_variant (font));

      attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_STRETCH,
                                           pango_font_description_get_stretch (font));

      value = g_strdup (pango_font_description_get_family (font));
      attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_FAMILY_NAME, value);

      value = g_strdup_printf ("%d", pango_font_description_get_weight (font));
      attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_WEIGHT, value);

      value = g_strdup_printf ("%i", pango_font_description_get_size (font) / PANGO_SCALE);
      attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_SIZE, value);
    }

  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_JUSTIFICATION, text_attrs->justification);
  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_DIRECTION, text_attrs->direction);
  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_WRAP_MODE, text_attrs->wrap_mode);
  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_EDITABLE, text_attrs->editable);
  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_INVISIBLE, text_attrs->invisible);
  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_BG_FULL_HEIGHT, text_attrs->bg_full_height);

  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_STRIKETHROUGH,
                                       text_attrs->appearance.strikethrough);
  attributes = add_text_int_attribute (attributes, ATK_TEXT_ATTR_UNDERLINE,
                                       text_attrs->appearance.underline);

  value = g_strdup_printf ("%u,%u,%u",
                           text_attrs->appearance.bg_color.red,
                           text_attrs->appearance.bg_color.green,
                           text_attrs->appearance.bg_color.blue);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_BG_COLOR, value);

  value = g_strdup_printf ("%u,%u,%u",
                           text_attrs->appearance.fg_color.red,
                           text_attrs->appearance.fg_color.green,
                           text_attrs->appearance.fg_color.blue);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_FG_COLOR, value);

  value = g_strdup_printf ("%g", text_attrs->font_scale);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_SCALE, value);

  value = g_strdup ((gchar *)(text_attrs->language));
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_LANGUAGE, value);

  value = g_strdup_printf ("%i", text_attrs->appearance.rise);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_RISE, value);

  value = g_strdup_printf ("%i", text_attrs->pixels_inside_wrap);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP, value);

  value = g_strdup_printf ("%i", text_attrs->pixels_below_lines);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_PIXELS_BELOW_LINES, value);

  value = g_strdup_printf ("%i", text_attrs->pixels_above_lines);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_PIXELS_ABOVE_LINES, value);

  value = g_strdup_printf ("%i", text_attrs->indent);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_INDENT, value);

  value = g_strdup_printf ("%i", text_attrs->left_margin);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_LEFT_MARGIN, value);

  value = g_strdup_printf ("%i", text_attrs->right_margin);
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_RIGHT_MARGIN, value);

  gtk_text_attributes_unref (text_attrs);
  return attributes;
}

static gint
gtk_text_view_accessible_get_n_selections (AtkText *text)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL))
    return 1;

  return 0;
}

static gchar *
gtk_text_view_accessible_get_selection (AtkText *atk_text,
                                        gint     selection_num,
                                        gint    *start_pos,
                                        gint    *end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *text;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return NULL;

  if (selection_num != 0)
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = gtk_text_view_get_buffer (view);

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  else
    text = NULL;

  *start_pos = gtk_text_iter_get_offset (&start);
  *end_pos = gtk_text_iter_get_offset (&end);

  return text;
}

static gboolean
gtk_text_view_accessible_add_selection (AtkText *text,
                                        gint     start_pos,
                                        gint     end_pos)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

  if (!gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL))
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
      gtk_text_buffer_select_range (buffer, &end, &start);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_text_view_accessible_remove_selection (AtkText *text,
                                           gint     selection_num)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      insert = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      gtk_text_buffer_place_cursor (buffer, &iter);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_text_view_accessible_set_selection (AtkText *text,
                                        gint     selection_num,
                                        gint     start_pos,
                                        gint     end_pos)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
      gtk_text_buffer_select_range (buffer, &end, &start);

      return TRUE;
    }
  else
    return FALSE;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_text_view_accessible_get_text;
  iface->get_text_after_offset = gtk_text_view_accessible_get_text_after_offset;
  iface->get_text_at_offset = gtk_text_view_accessible_get_text_at_offset;
  iface->get_text_before_offset = gtk_text_view_accessible_get_text_before_offset;
  iface->get_character_at_offset = gtk_text_view_accessible_get_character_at_offset;
  iface->get_character_count = gtk_text_view_accessible_get_character_count;
  iface->get_caret_offset = gtk_text_view_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_text_view_accessible_set_caret_offset;
  iface->get_offset_at_point = gtk_text_view_accessible_get_offset_at_point;
  iface->get_character_extents = gtk_text_view_accessible_get_character_extents;
  iface->get_n_selections = gtk_text_view_accessible_get_n_selections;
  iface->get_selection = gtk_text_view_accessible_get_selection;
  iface->add_selection = gtk_text_view_accessible_add_selection;
  iface->remove_selection = gtk_text_view_accessible_remove_selection;
  iface->set_selection = gtk_text_view_accessible_set_selection;
  iface->get_run_attributes = gtk_text_view_accessible_get_run_attributes;
  iface->get_default_attributes = gtk_text_view_accessible_get_default_attributes;
}

/* atkeditabletext.h */

static gboolean
gtk_text_view_accessible_set_run_attributes (AtkEditableText *text,
                                             AtkAttributeSet *attributes,
                                             gint             start_offset,
                                             gint             end_offset)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkWidget *widget;
  GtkTextTag *tag;
  GtkTextIter start;
  GtkTextIter end;
  gint j;
  GdkColor *color;
  gchar** RGB_vals;
  GSList *l;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return FALSE;

  buffer = gtk_text_view_get_buffer (view);

  if (attributes == NULL)
    return FALSE;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);

  tag = gtk_text_buffer_create_tag (buffer, NULL, NULL);

  for (l = attributes; l; l = l->next)
    {
      gchar *name;
      gchar *value;
      AtkAttribute *at;

      at = l->data;

      name = at->name;
      value = at->value;

      if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_LEFT_MARGIN)))
        g_object_set (G_OBJECT (tag), "left-margin", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_RIGHT_MARGIN)))
        g_object_set (G_OBJECT (tag), "right-margin", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_INDENT)))
        g_object_set (G_OBJECT (tag), "indent", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_PIXELS_ABOVE_LINES)))
        g_object_set (G_OBJECT (tag), "pixels-above-lines", atoi (value), NULL);

      else if (!strcmp(name, atk_text_attribute_get_name (ATK_TEXT_ATTR_PIXELS_BELOW_LINES)))
        g_object_set (G_OBJECT (tag), "pixels-below-lines", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP)))
        g_object_set (G_OBJECT (tag), "pixels-inside-wrap", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_SIZE)))
        g_object_set (G_OBJECT (tag), "size", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_RISE)))
        g_object_set (G_OBJECT (tag), "rise", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_WEIGHT)))
        g_object_set (G_OBJECT (tag), "weight", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_BG_FULL_HEIGHT)))
        {
          g_object_set (G_OBJECT (tag), "bg-full-height",
                   (strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_BG_FULL_HEIGHT, 0))),
                   NULL);
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_LANGUAGE)))
        g_object_set (G_OBJECT (tag), "language", value, NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_FAMILY_NAME)))
        g_object_set (G_OBJECT (tag), "family", value, NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_EDITABLE)))
        {
          g_object_set (G_OBJECT (tag), "editable",
                   (strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_EDITABLE, 0))),
                   NULL);
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_INVISIBLE)))
        {
          g_object_set (G_OBJECT (tag), "invisible",
                   (strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_EDITABLE, 0))),
                   NULL);
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_UNDERLINE)))
        {
          for (j = 0; j < 3; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE, j)))
                {
                  g_object_set (G_OBJECT (tag), "underline", j, NULL);
                  break;
                }
            }
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_STRIKETHROUGH)))
        {
          g_object_set (G_OBJECT (tag), "strikethrough",
                   (strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH, 0))),
                   NULL);
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_BG_COLOR)))
        {
          RGB_vals = g_strsplit (value, ",", 3);
          color = g_malloc (sizeof (GdkColor));
          color->red = atoi (RGB_vals[0]);
          color->green = atoi (RGB_vals[1]);
          color->blue = atoi (RGB_vals[2]);
          g_object_set (G_OBJECT (tag), "background-gdk", color, NULL);
        }
 
      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_FG_COLOR)))
        {
          RGB_vals = g_strsplit (value, ",", 3);
          color = g_malloc (sizeof (GdkColor));
          color->red = atoi (RGB_vals[0]);
          color->green = atoi (RGB_vals[1]);
          color->blue = atoi (RGB_vals[2]);
          g_object_set (G_OBJECT (tag), "foreground-gdk", color, NULL);
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_STRETCH)))
        {
          for (j = 0; j < 9; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH, j)))
                {
                  g_object_set (G_OBJECT (tag), "stretch", j, NULL);
                  break;
                }
            }
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_JUSTIFICATION)))
        {
          for (j = 0; j < 4; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, j)))
                {
                  g_object_set (G_OBJECT (tag), "justification", j, NULL);
                  break;
                }
            }
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_DIRECTION)))
        {
          for (j = 0; j < 3; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, j)))
                {
                  g_object_set (G_OBJECT (tag), "direction", j, NULL);
                  break;
                }
            }
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_VARIANT)))
        {
          for (j = 0; j < 2; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT, j)))
                {
                  g_object_set (G_OBJECT (tag), "variant", j, NULL);
                  break;
                }
            }
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_WRAP_MODE)))
        {
          for (j = 0; j < 3; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_WRAP_MODE, j)))
                {
                  g_object_set (G_OBJECT (tag), "wrap-mode", j, NULL);
                  break;
                }
            }
        }

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_STYLE)))
        {
          for (j = 0; j < 3; j++)
            {
              if (!strcmp (value, atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE, j)))
                {
                  g_object_set (G_OBJECT (tag), "style", j, NULL);
                  break;
              }
            }
        }

      else
        return FALSE;
    }

  gtk_text_buffer_apply_tag (buffer, tag, &start, &end);

  return TRUE;
}

static void
gtk_text_view_accessible_set_text_contents (AtkEditableText *text,
                                            const gchar     *string)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_set_text (buffer, string, -1);
}

static void
gtk_text_view_accessible_insert_text (AtkEditableText *text,
                                      const gchar     *string,
                                      gint             length,
                                      gint            *position)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, *position);
  gtk_text_buffer_insert (buffer, &iter, string, length);
}

static void
gtk_text_view_accessible_copy_text (AtkEditableText *text,
                                    gint             start_pos,
                                    gint             end_pos)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *str;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
  str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
}

static void
gtk_text_view_accessible_cut_text (AtkEditableText *text,
                                   gint             start_pos,
                                   gint             end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *str;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = gtk_text_view_get_buffer (view);

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
  str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
  gtk_text_buffer_delete (buffer, &start, &end);
}

static void
gtk_text_view_accessible_delete_text (AtkEditableText *text,
                                      gint             start_pos,
                                      gint             end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start_itr;
  GtkTextIter end_itr;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = gtk_text_view_get_buffer (view);

  gtk_text_buffer_get_iter_at_offset (buffer, &start_itr, start_pos);
  gtk_text_buffer_get_iter_at_offset (buffer, &end_itr, end_pos);
  gtk_text_buffer_delete (buffer, &start_itr, &end_itr);
}

typedef struct
{
  GtkTextBuffer* buffer;
  gint position;
} PasteData;

static void
paste_received (GtkClipboard *clipboard,
                const gchar  *text,
                gpointer      data)
{
  PasteData* paste = data;
  GtkTextIter pos_itr;

  if (text)
    {
      gtk_text_buffer_get_iter_at_offset (paste->buffer, &pos_itr, paste->position);
      gtk_text_buffer_insert (paste->buffer, &pos_itr, text, -1);
    }

  g_object_unref (paste->buffer);
}

static void
gtk_text_view_accessible_paste_text (AtkEditableText *text,
                                     gint             position)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  PasteData paste;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = gtk_text_view_get_buffer (view);

  paste.buffer = buffer;
  paste.position = position;

  g_object_ref (paste.buffer);
  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text (clipboard, paste_received, &paste);
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  iface->set_text_contents = gtk_text_view_accessible_set_text_contents;
  iface->insert_text = gtk_text_view_accessible_insert_text;
  iface->copy_text = gtk_text_view_accessible_copy_text;
  iface->cut_text = gtk_text_view_accessible_cut_text;
  iface->delete_text = gtk_text_view_accessible_delete_text;
  iface->paste_text = gtk_text_view_accessible_paste_text;
  iface->set_run_attributes = gtk_text_view_accessible_set_run_attributes;
}

/* Callbacks */

static void
gtk_text_view_accessible_update_cursor (GtkTextViewAccessible *accessible,
                                        GtkTextBuffer *        buffer)
{
  int prev_insert_offset, prev_selection_bound;
  int insert_offset, selection_bound;
  GtkTextIter iter;

  prev_insert_offset = accessible->priv->insert_offset;
  prev_selection_bound = accessible->priv->selection_bound;

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
  insert_offset = gtk_text_iter_get_offset (&iter);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_selection_bound (buffer));
  selection_bound = gtk_text_iter_get_offset (&iter);

  if (prev_insert_offset == insert_offset && prev_selection_bound == selection_bound)
    return;

  accessible->priv->insert_offset = insert_offset;
  accessible->priv->selection_bound = selection_bound;

  if (prev_insert_offset != insert_offset)
    g_signal_emit_by_name (accessible, "text-caret-moved", insert_offset);

  if (prev_insert_offset != prev_selection_bound || insert_offset != selection_bound)
    g_signal_emit_by_name (accessible, "text-selection-changed");
}

static void
insert_text_cb (GtkTextBuffer *buffer,
                GtkTextIter   *iter,
                gchar         *text,
                gint           len,
                gpointer       data)
{
  GtkTextViewAccessible *accessible = data;
  gint position;
  gint length;

  position = gtk_text_iter_get_offset (iter);
  length = g_utf8_strlen (text, len);
 
  g_signal_emit_by_name (accessible, "text-changed::insert", position - length, length);

  gtk_text_view_accessible_update_cursor (accessible, buffer);
}

static void
delete_range_cb (GtkTextBuffer *buffer,
                 GtkTextIter   *start,
                 GtkTextIter   *end,
                 gpointer       data)
{
  GtkTextViewAccessible *accessible = data;
  gint offset, length;

  offset = gtk_text_iter_get_offset (start);
  length = gtk_text_iter_get_offset (end) - offset;

  g_signal_emit_by_name (accessible,
                         "text-changed::delete",
                         offset,
                         length);

  gtk_text_view_accessible_update_cursor (accessible, buffer);
}

static void
mark_set_cb (GtkTextBuffer *buffer,
             GtkTextIter   *location,
             GtkTextMark   *mark,
             gpointer       data)
{
  GtkTextViewAccessible *accessible = data;

  /*
   * Only generate the signal for the "insert" mark, which
   * represents the cursor.
   */
  if (mark == gtk_text_buffer_get_insert (buffer))
    {
      gtk_text_view_accessible_update_cursor (accessible, buffer);
    }
  else if (mark == gtk_text_buffer_get_selection_bound (buffer))
    {
      gtk_text_view_accessible_update_cursor (accessible, buffer);
    }
}

static gint
gail_streamable_content_get_n_mime_types (AtkStreamableContent *streamable)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  gint n_mime_types = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (streamable));
  if (widget == NULL)
    return 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  if (buffer)
    {
      gint i;
      gboolean advertises_plaintext = FALSE;
      GdkAtom *atoms;

      atoms = gtk_text_buffer_get_serialize_formats (buffer, &n_mime_types);
      for (i = 0; i < n_mime_types-1; ++i)
        if (!strcmp ("text/plain", gdk_atom_name (atoms[i])))
            advertises_plaintext = TRUE;
      if (!advertises_plaintext)
        n_mime_types++;
    }

  return n_mime_types;
}

static const gchar *
gail_streamable_content_get_mime_type (AtkStreamableContent *streamable,
                                       gint                  i)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (streamable));
  if (widget == NULL)
    return 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  if (buffer)
    {
      gint n_mime_types = 0;
      GdkAtom *atoms;

      atoms = gtk_text_buffer_get_serialize_formats (buffer, &n_mime_types);
      if (i < n_mime_types)
        return gdk_atom_name (atoms [i]);
      else if (i == n_mime_types)
        return "text/plain";
    }

  return NULL;
}

static GIOChannel *
gail_streamable_content_get_stream (AtkStreamableContent *streamable,
                                    const gchar          *mime_type)
{
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  gint i, n_mime_types = 0;
  GdkAtom *atoms;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (streamable));
  if (widget == NULL)
    return 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  if (!buffer)
    return NULL;

  atoms = gtk_text_buffer_get_serialize_formats (buffer, &n_mime_types);

  for (i = 0; i < n_mime_types; ++i)
    {
      if (!strcmp ("text/plain", mime_type) ||
          !strcmp (gdk_atom_name (atoms[i]), mime_type))
        {
          guint8 *cbuf;
          GError *err = NULL;
          gsize len, written;
          gchar tname[80];
          GtkTextIter start, end;
          GIOChannel *gio = NULL;
          int fd;

          gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
          gtk_text_buffer_get_iter_at_offset (buffer, &end, -1);
          if (!strcmp ("text/plain", mime_type))
            {
              cbuf = (guint8*) gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
              len = strlen ((const char *) cbuf);
            }
          else
            {
              cbuf = gtk_text_buffer_serialize (buffer, buffer, atoms[i], &start, &end, &len);
            }
          g_snprintf (tname, 20, "streamXXXXXX");
          fd = g_mkstemp (tname);
          gio = g_io_channel_unix_new (fd);
          g_io_channel_set_encoding (gio, NULL, &err);
          if (!err)
            g_io_channel_write_chars (gio, (const char *) cbuf, (gssize) len, &written, &err);
          else
            g_message ("%s", err->message);
          if (!err)
            g_io_channel_seek_position (gio, 0, G_SEEK_SET, &err);
          else
            g_message ("%s", err->message);
          if (!err)
            g_io_channel_flush (gio, &err);
          else
            g_message ("%s", err->message);
          if (err)
            {
              g_message ("<error writing to stream [%s]>", tname);
              g_error_free (err);
            }
          /* make sure the file is removed on unref of the giochannel */
          else
            {
              g_unlink (tname);
              return gio;
            }
        }
    }

  return NULL;
}

static void
atk_streamable_content_interface_init (AtkStreamableContentIface *iface)
{
  iface->get_n_mime_types = gail_streamable_content_get_n_mime_types;
  iface->get_mime_type = gail_streamable_content_get_mime_type;
  iface->get_stream = gail_streamable_content_get_stream;
}

void
_gtk_text_view_accessible_set_buffer (GtkTextView   *textview,
                                      GtkTextBuffer *old_buffer)
{
  GtkTextViewAccessible *accessible;

  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));
  g_return_if_fail (old_buffer == NULL || GTK_IS_TEXT_BUFFER (old_buffer));

  accessible = GTK_TEXT_VIEW_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (textview)));
  if (accessible == NULL)
    return;

  gtk_text_view_accessible_change_buffer (accessible,
                                          old_buffer,
                                          gtk_text_view_get_buffer (textview));
}

