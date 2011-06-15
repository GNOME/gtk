/* GAIL - The GNOME Accessibility Implementation Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gailtextview.h"
#include <libgail-util/gailmisc.h>

static void       gail_text_view_class_init            (GailTextViewClass *klass);
static void       gail_text_view_init                  (GailTextView      *text_view);

static void       gail_text_view_real_initialize       (AtkObject         *obj,
                                                        gpointer          data);
static void       gail_text_view_real_notify_gtk       (GObject           *obj,
                                                        GParamSpec        *pspec);

static void       gail_text_view_finalize              (GObject           *object);

static void       atk_text_interface_init              (AtkTextIface     *iface);

/* atkobject.h */

static AtkStateSet* gail_text_view_ref_state_set       (AtkObject        *accessible);

/* atktext.h */

static gchar*     gail_text_view_get_text_after_offset (AtkText          *text,
                                                        gint             offset,
                                                        AtkTextBoundary  boundary_type,
                                                        gint             *start_offset,
                                                        gint             *end_offset);
static gchar*     gail_text_view_get_text_at_offset    (AtkText          *text,
                                                        gint             offset,
                                                        AtkTextBoundary  boundary_type,
                                                        gint             *start_offset,
                                                        gint             *end_offset);
static gchar*     gail_text_view_get_text_before_offset (AtkText         *text,
                                                        gint             offset,
                                                        AtkTextBoundary  boundary_type,
                                                        gint             *start_offset,
                                                        gint             *end_offset);
static gchar*     gail_text_view_get_text              (AtkText*text,
                                                        gint             start_offset,
                                                        gint             end_offset);
static gunichar   gail_text_view_get_character_at_offset (AtkText        *text,
                                                        gint             offset);
static gint       gail_text_view_get_character_count   (AtkText          *text);
static gint       gail_text_view_get_caret_offset      (AtkText          *text);
static gboolean   gail_text_view_set_caret_offset      (AtkText          *text,
                                                        gint             offset);
static gint       gail_text_view_get_offset_at_point   (AtkText          *text,
                                                        gint             x,
                                                        gint             y,
                                                        AtkCoordType     coords);
static gint       gail_text_view_get_n_selections      (AtkText          *text);
static gchar*     gail_text_view_get_selection         (AtkText          *text,
                                                        gint             selection_num,
                                                        gint             *start_offset,
                                                        gint             *end_offset);
static gboolean   gail_text_view_add_selection         (AtkText          *text,
                                                        gint             start_offset,
                                                        gint             end_offset);
static gboolean   gail_text_view_remove_selection      (AtkText          *text,
                                                        gint             selection_num);
static gboolean   gail_text_view_set_selection         (AtkText          *text,
                                                        gint             selection_num,
                                                        gint             start_offset,
                                                        gint             end_offset);
static void       gail_text_view_get_character_extents (AtkText          *text,
                                                        gint             offset,
                                                        gint             *x,
                                                        gint             *y,
                                                        gint             *width,
                                                        gint             *height,
                                                        AtkCoordType     coords);
static AtkAttributeSet *  gail_text_view_get_run_attributes 
                                                       (AtkText          *text,
                                                        gint             offset,
                                                        gint             *start_offset,
                                                        gint             *end_offset);
static AtkAttributeSet *  gail_text_view_get_default_attributes 
                                                       (AtkText          *text);
/* atkeditabletext.h */

static void       atk_editable_text_interface_init     (AtkEditableTextIface *iface);
static gboolean   gail_text_view_set_run_attributes    (AtkEditableText  *text,
                                                        AtkAttributeSet  *attrib_set,
                                                        gint             start_offset,
                                                        gint             end_offset);
static void       gail_text_view_set_text_contents     (AtkEditableText  *text,
                                                        const gchar      *string);
static void       gail_text_view_insert_text           (AtkEditableText  *text,
                                                        const gchar      *string,
                                                        gint             length,
                                                        gint             *position);
static void       gail_text_view_copy_text             (AtkEditableText  *text,
                                                        gint             start_pos,
                                                        gint             end_pos);
static void       gail_text_view_cut_text              (AtkEditableText  *text,
                                                        gint             start_pos,
                                                        gint             end_pos);
static void       gail_text_view_delete_text           (AtkEditableText  *text,
                                                        gint             start_pos,
                                                        gint             end_pos);
static void       gail_text_view_paste_text            (AtkEditableText  *text,
                                                        gint             position);
static void       gail_text_view_paste_received        (GtkClipboard     *clipboard,
                                                        const gchar      *text,
                                                        gpointer         data);
/* AtkStreamableContent */
static void       atk_streamable_content_interface_init    (AtkStreamableContentIface *iface);
static gint       gail_streamable_content_get_n_mime_types (AtkStreamableContent *streamable);
static const gchar* gail_streamable_content_get_mime_type (AtkStreamableContent *streamable,
								    gint i);
static GIOChannel* gail_streamable_content_get_stream       (AtkStreamableContent *streamable,
							     const gchar *mime_type);
/* getURI requires atk-1.12.0 or later
static void       gail_streamable_content_get_uri          (AtkStreamableContent *streamable);
*/

/* Callbacks */

static void       _gail_text_view_insert_text_cb       (GtkTextBuffer    *buffer,
                                                        GtkTextIter      *arg1,
                                                        gchar            *arg2,
                                                        gint             arg3,
                                                        gpointer         user_data);
static void       _gail_text_view_delete_range_cb      (GtkTextBuffer    *buffer,
                                                        GtkTextIter      *arg1,
                                                        GtkTextIter      *arg2,
                                                        gpointer         user_data);
static void       _gail_text_view_changed_cb           (GtkTextBuffer    *buffer,
                                                        gpointer         user_data);
static void       _gail_text_view_mark_set_cb          (GtkTextBuffer    *buffer,
                                                        GtkTextIter      *arg1,
                                                        GtkTextMark      *arg2,
                                                        gpointer         user_data);
static gchar*            get_text_near_offset          (AtkText          *text,
                                                        GailOffsetType   function,
                                                        AtkTextBoundary  boundary_type,
                                                        gint             offset,
                                                        gint             *start_offset,
                                                        gint             *end_offset);
static gint             get_insert_offset              (GtkTextBuffer    *buffer);
static gint             get_selection_bound            (GtkTextBuffer    *buffer);
static void             emit_text_caret_moved          (GailTextView     *gail_text_view,
                                                        gint             insert_offset);
static gint             insert_idle_handler            (gpointer         data);

typedef struct _GailTextViewPaste                       GailTextViewPaste;

struct _GailTextViewPaste
{
  GtkTextBuffer* buffer;
  gint position;
};

G_DEFINE_TYPE_WITH_CODE (GailTextView, gail_text_view, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT, atk_editable_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_STREAMABLE_CONTENT, atk_streamable_content_interface_init))

static void
gail_text_view_class_init (GailTextViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  gobject_class->finalize = gail_text_view_finalize;

  class->ref_state_set = gail_text_view_ref_state_set;
  class->initialize = gail_text_view_real_initialize;

  widget_class->notify_gtk = gail_text_view_real_notify_gtk;
}

static void
gail_text_view_init (GailTextView      *text_view)
{
  text_view->textutil = NULL;
  text_view->signal_name = NULL;
  text_view->previous_insert_offset = -1;
  text_view->previous_selection_bound = -1;
  text_view->insert_notify_handler = 0;
}

static void
setup_buffer (GtkTextView  *view, 
              GailTextView *gail_view)
{
  GtkTextBuffer *buffer;

  buffer = view->buffer;
  if (buffer == NULL)
    return;

  if (gail_view->textutil)
    g_object_unref (gail_view->textutil);

  gail_view->textutil = gail_text_util_new ();
  gail_text_util_buffer_setup (gail_view->textutil, buffer);

  /* Set up signal callbacks */
  g_signal_connect_data (buffer, "insert-text",
     (GCallback) _gail_text_view_insert_text_cb, view, NULL, 0);
  g_signal_connect_data (buffer, "delete-range",
     (GCallback) _gail_text_view_delete_range_cb, view, NULL, 0);
  g_signal_connect_data (buffer, "mark-set",
     (GCallback) _gail_text_view_mark_set_cb, view, NULL, 0);
  g_signal_connect_data (buffer, "changed",
     (GCallback) _gail_text_view_changed_cb, view, NULL, 0);

}

static void
gail_text_view_real_initialize (AtkObject *obj,
                                gpointer  data)
{
  GtkTextView *view;
  GailTextView *gail_view;

  ATK_OBJECT_CLASS (gail_text_view_parent_class)->initialize (obj, data);

  view = GTK_TEXT_VIEW (data);

  gail_view = GAIL_TEXT_VIEW (obj);
  setup_buffer (view, gail_view);

  obj->role = ATK_ROLE_TEXT;

}

static void
gail_text_view_finalize (GObject            *object)
{
  GailTextView *text_view = GAIL_TEXT_VIEW (object);

  g_object_unref (text_view->textutil);
  if (text_view->insert_notify_handler)
    g_source_remove (text_view->insert_notify_handler);

  G_OBJECT_CLASS (gail_text_view_parent_class)->finalize (object);
}

static void
gail_text_view_real_notify_gtk (GObject             *obj,
                                GParamSpec          *pspec)
{
  if (!strcmp (pspec->name, "editable"))
    {
      AtkObject *atk_obj;
      gboolean editable;

      atk_obj = gtk_widget_get_accessible (GTK_WIDGET (obj));
      editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (obj));
      atk_object_notify_state_change (atk_obj, ATK_STATE_EDITABLE,
                                      editable);
    }
  else if (!strcmp (pspec->name, "buffer"))
    {
      AtkObject *atk_obj;

      atk_obj = gtk_widget_get_accessible (GTK_WIDGET (obj));
      setup_buffer (GTK_TEXT_VIEW (obj), GAIL_TEXT_VIEW (atk_obj));
    }
  else
    GAIL_WIDGET_CLASS (gail_text_view_parent_class)->notify_gtk (obj, pspec);
}

/* atkobject.h */

static AtkStateSet*
gail_text_view_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkTextView *text_view;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_text_view_parent_class)->ref_state_set (accessible);
  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
    return state_set;

  text_view = GTK_TEXT_VIEW (widget);

  if (gtk_text_view_get_editable (text_view))
    atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);
  atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

  return state_set;
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_text_view_get_text;
  iface->get_text_after_offset = gail_text_view_get_text_after_offset;
  iface->get_text_at_offset = gail_text_view_get_text_at_offset;
  iface->get_text_before_offset = gail_text_view_get_text_before_offset;
  iface->get_character_at_offset = gail_text_view_get_character_at_offset;
  iface->get_character_count = gail_text_view_get_character_count;
  iface->get_caret_offset = gail_text_view_get_caret_offset;
  iface->set_caret_offset = gail_text_view_set_caret_offset;
  iface->get_offset_at_point = gail_text_view_get_offset_at_point;
  iface->get_character_extents = gail_text_view_get_character_extents;
  iface->get_n_selections = gail_text_view_get_n_selections;
  iface->get_selection = gail_text_view_get_selection;
  iface->add_selection = gail_text_view_add_selection;
  iface->remove_selection = gail_text_view_remove_selection;
  iface->set_selection = gail_text_view_set_selection;
  iface->get_run_attributes = gail_text_view_get_run_attributes;
  iface->get_default_attributes = gail_text_view_get_default_attributes;
}

static gchar*
gail_text_view_get_text (AtkText *text,
                         gint    start_offset,
                         gint    end_offset)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;
  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gchar*
gail_text_view_get_text_after_offset (AtkText         *text,
                                      gint            offset,
                                      AtkTextBoundary boundary_type,
                                      gint            *start_offset,
                                      gint            *end_offset)
{
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  return get_text_near_offset (text, GAIL_AFTER_OFFSET,
                               boundary_type, offset, 
                               start_offset, end_offset);
}

static gchar*
gail_text_view_get_text_at_offset (AtkText         *text,
                                   gint            offset,
                                   AtkTextBoundary boundary_type,
                                   gint            *start_offset,
                                   gint            *end_offset)
{
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  return get_text_near_offset (text, GAIL_AT_OFFSET,
                               boundary_type, offset, 
                               start_offset, end_offset);
}

static gchar*
gail_text_view_get_text_before_offset (AtkText         *text,
                                       gint            offset,
                                       AtkTextBoundary boundary_type,
                                       gint            *start_offset,
                                       gint            *end_offset)
{
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  return get_text_near_offset (text, GAIL_BEFORE_OFFSET,
                               boundary_type, offset, 
                               start_offset, end_offset);
}

static gunichar
gail_text_view_get_character_at_offset (AtkText *text,
                                        gint    offset)
{
  GtkWidget *widget;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  gchar *string;
  gunichar unichar;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    return '\0';

  buffer = GAIL_TEXT_VIEW (text)->textutil->buffer;
  if (offset >= gtk_text_buffer_get_char_count (buffer))
    return '\0';

  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
  end = start;
  gtk_text_iter_forward_char (&end);
  string = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
  unichar = g_utf8_get_char (string);
  g_free(string);
  return unichar;
}

static gint
gail_text_view_get_character_count (AtkText *text)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;
  return gtk_text_buffer_get_char_count (buffer);
}

static gint
gail_text_view_get_caret_offset (AtkText *text)
{
  GtkTextView *view;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  view = GTK_TEXT_VIEW (widget);
  return get_insert_offset (view->buffer);
}

static gboolean
gail_text_view_set_caret_offset (AtkText *text,
                                 gint    offset)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter pos_itr;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_iter_at_offset (buffer,  &pos_itr, offset);
  gtk_text_buffer_place_cursor (buffer, &pos_itr);
  gtk_text_view_scroll_to_iter (view, &pos_itr, 0, FALSE, 0, 0);
  return TRUE;
}

static gint
gail_text_view_get_offset_at_point (AtkText      *text,
                                    gint         x,
                                    gint         y,
                                    AtkCoordType coords)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter loc_itr;
  gint x_widget, y_widget, x_window, y_window, buff_x, buff_y;
  GtkWidget *widget;
  GdkWindow *window;
  GdkRectangle rect;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return -1;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

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
  /*
   * Clamp point to visible rectangle
   */
  buff_x = CLAMP (buff_x, rect.x, rect.x + rect.width - 1);
  buff_y = CLAMP (buff_y, rect.y, rect.y + rect.height - 1);

  gtk_text_view_get_iter_at_location (view, &loc_itr, buff_x, buff_y);
  /*
   * The iter at a location sometimes points to the next character.
   * See bug 111031. We work around that
   */
  gtk_text_view_get_iter_location (view, &loc_itr, &rect);
  if (buff_x < rect.x)
    gtk_text_iter_backward_char (&loc_itr);
  return gtk_text_iter_get_offset (&loc_itr);
}

static void
gail_text_view_get_character_extents (AtkText      *text,
                                      gint         offset,
                                      gint         *x,
                                      gint         *y,
                                      gint         *width,
                                      gint         *height,
                                      AtkCoordType coords)
{
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkWidget *widget;
  GdkRectangle rectangle;
  GdkWindow *window;
  gint x_widget, y_widget, x_window, y_window;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;
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

static AtkAttributeSet*
gail_text_view_get_run_attributes (AtkText *text,
                                   gint    offset,
                                   gint    *start_offset,
                                   gint    *end_offset)
{
  GtkTextView *view;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  view = GTK_TEXT_VIEW (widget);

  return gail_misc_buffer_get_run_attributes (view->buffer, offset, 
                                              start_offset, end_offset);
}

static AtkAttributeSet*
gail_text_view_get_default_attributes (AtkText *text)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextAttributes *text_attrs;
  AtkAttributeSet *attrib_set = NULL;
  PangoFontDescription *font;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  view = GTK_TEXT_VIEW (widget);
  text_attrs = gtk_text_view_get_default_attributes (view);

  font = text_attrs->font;

  if (font)
    {
      attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                              ATK_TEXT_ATTR_STYLE);

      attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                              ATK_TEXT_ATTR_VARIANT);

      attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                              ATK_TEXT_ATTR_STRETCH);
    }

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_JUSTIFICATION);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_DIRECTION);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_WRAP_MODE);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_FG_STIPPLE);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_BG_STIPPLE);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_FG_COLOR);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_BG_COLOR);

  if (font)
    {
      attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                              ATK_TEXT_ATTR_FAMILY_NAME);
    }

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_LANGUAGE);

  if (font)
    {
      attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                              ATK_TEXT_ATTR_WEIGHT);
    }

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_SCALE);

  if (font)
    {
      attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                              ATK_TEXT_ATTR_SIZE);
    }

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_STRIKETHROUGH);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_UNDERLINE);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_RISE);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_BG_FULL_HEIGHT);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                         ATK_TEXT_ATTR_PIXELS_BELOW_LINES);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_PIXELS_ABOVE_LINES);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_EDITABLE);
    
  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_INVISIBLE);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_INDENT);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_RIGHT_MARGIN);

  attrib_set = gail_misc_add_to_attr_set (attrib_set, text_attrs, 
                                          ATK_TEXT_ATTR_LEFT_MARGIN);

  gtk_text_attributes_unref (text_attrs);
  return attrib_set;
}

static gint
gail_text_view_get_n_selections (AtkText *text)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gint select_start, select_end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return -1;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  select_start = gtk_text_iter_get_offset (&start);
  select_end = gtk_text_iter_get_offset (&end);

  if (select_start != select_end)
     return 1;
  else
     return 0;
}

static gchar*
gail_text_view_get_selection (AtkText *text,
                              gint    selection_num,
                              gint    *start_pos,
                              gint    *end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

 /* Only let the user get the selection if one is set, and if the
  * selection_num is 0.
  */
  if (selection_num != 0)
     return NULL;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  *start_pos = gtk_text_iter_get_offset (&start);
  *end_pos = gtk_text_iter_get_offset (&end);

  if (*start_pos != *end_pos)
    return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  else
    return NULL;
}

static gboolean
gail_text_view_add_selection (AtkText *text,
                              gint    start_pos,
                              gint    end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter pos_itr;
  GtkTextIter start, end;
  gint select_start, select_end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  select_start = gtk_text_iter_get_offset (&start);
  select_end = gtk_text_iter_get_offset (&end);

 /* If there is already a selection, then don't allow another to be added,
  * since GtkTextView only supports one selected region.
  */
  if (select_start == select_end)
    {
      gtk_text_buffer_get_iter_at_offset (buffer,  &pos_itr, start_pos);
      gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &pos_itr);
      gtk_text_buffer_get_iter_at_offset (buffer,  &pos_itr, end_pos);
      gtk_text_buffer_move_mark_by_name (buffer, "insert", &pos_itr);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gail_text_view_remove_selection (AtkText *text,
                                 gint    selection_num)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextMark *cursor_mark;
  GtkTextIter cursor_itr;
  GtkTextIter start, end;
  gint select_start, select_end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
  select_start = gtk_text_iter_get_offset(&start);
  select_end = gtk_text_iter_get_offset(&end);

  if (select_start != select_end)
    {
     /* Setting the start & end of the selected region to the caret position
      * turns off the selection.
      */
      cursor_mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &cursor_itr, cursor_mark);
      gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &cursor_itr);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gail_text_view_set_selection (AtkText *text,
                              gint    selection_num,
                              gint    start_pos,
                              gint    end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter pos_itr;
  GtkTextIter start, end;
  gint select_start, select_end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return FALSE;
  }

 /* Only let the user move the selection if one is set, and if the
  * selection_num is 0
  */
  if (selection_num != 0)
     return FALSE;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
  select_start = gtk_text_iter_get_offset(&start);
  select_end = gtk_text_iter_get_offset(&end);

  if (select_start != select_end)
    {
      gtk_text_buffer_get_iter_at_offset (buffer,  &pos_itr, start_pos);
      gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &pos_itr);
      gtk_text_buffer_get_iter_at_offset (buffer,  &pos_itr, end_pos);
      gtk_text_buffer_move_mark_by_name (buffer, "insert", &pos_itr);
      return TRUE;
    }
  else
    return FALSE;
}

/* atkeditabletext.h */

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  iface->set_text_contents = gail_text_view_set_text_contents;
  iface->insert_text = gail_text_view_insert_text;
  iface->copy_text = gail_text_view_copy_text;
  iface->cut_text = gail_text_view_cut_text;
  iface->delete_text = gail_text_view_delete_text;
  iface->paste_text = gail_text_view_paste_text;
  iface->set_run_attributes = gail_text_view_set_run_attributes;
}

static gboolean
gail_text_view_set_run_attributes (AtkEditableText *text,
                                   AtkAttributeSet *attrib_set,
                                   gint            start_offset,
                                   gint            end_offset)
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

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return FALSE;

  buffer = view->buffer;

  if (attrib_set == NULL)
    return FALSE;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);

  tag = gtk_text_buffer_create_tag (buffer, NULL, NULL);

  for (l = attrib_set; l; l = l->next)
    {
      gchar *name;
      gchar *value;
      AtkAttribute *at;

      at = l->data;

      name = at->name;
      value = at->value;

      if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_LEFT_MARGIN)))
        g_object_set (G_OBJECT (tag), "left_margin", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_RIGHT_MARGIN)))
        g_object_set (G_OBJECT (tag), "right_margin", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_INDENT)))
        g_object_set (G_OBJECT (tag), "indent", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_PIXELS_ABOVE_LINES)))
        g_object_set (G_OBJECT (tag), "pixels_above_lines", atoi (value), NULL);

      else if (!strcmp(name, atk_text_attribute_get_name (ATK_TEXT_ATTR_PIXELS_BELOW_LINES)))
        g_object_set (G_OBJECT (tag), "pixels_below_lines", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP)))
        g_object_set (G_OBJECT (tag), "pixels_inside_wrap", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_SIZE)))
        g_object_set (G_OBJECT (tag), "size", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_RISE)))
        g_object_set (G_OBJECT (tag), "rise", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_WEIGHT)))
        g_object_set (G_OBJECT (tag), "weight", atoi (value), NULL);

      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_BG_FULL_HEIGHT)))
        {
          g_object_set (G_OBJECT (tag), "bg_full_height", 
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
          g_object_set (G_OBJECT (tag), "background_gdk", color, NULL);
        }
  
      else if (!strcmp (name, atk_text_attribute_get_name (ATK_TEXT_ATTR_FG_COLOR)))
        {
          RGB_vals = g_strsplit (value, ",", 3);
          color = g_malloc (sizeof (GdkColor));
          color->red = atoi (RGB_vals[0]);
          color->green = atoi (RGB_vals[1]);
          color->blue = atoi (RGB_vals[2]);
          g_object_set (G_OBJECT (tag), "foreground_gdk", color, NULL);
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
                  g_object_set (G_OBJECT (tag), "wrap_mode", j, NULL);
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
gail_text_view_set_text_contents (AtkEditableText *text,
                                  const gchar     *string)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = view->buffer;

  /* The -1 indicates that the input string must be null-terminated */
  gtk_text_buffer_set_text (buffer, string, -1);
}

static void
gail_text_view_insert_text (AtkEditableText *text,
                            const gchar     *string,
                            gint            length,
                            gint            *position)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter pos_itr;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = view->buffer;

  gtk_text_buffer_get_iter_at_offset (buffer, &pos_itr, *position);
  gtk_text_buffer_insert (buffer, &pos_itr, string, length);
}

static void
gail_text_view_copy_text   (AtkEditableText *text,
                            gint            start_pos,
                            gint            end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *str;
  GtkClipboard *clipboard;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  buffer = view->buffer;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
  str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
}

static void
gail_text_view_cut_text (AtkEditableText *text,
                         gint            start_pos,
                         gint            end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  gchar *str;
  GtkClipboard *clipboard;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = view->buffer;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
  str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
  gtk_text_buffer_delete (buffer, &start, &end);
}

static void
gail_text_view_delete_text (AtkEditableText *text,
                            gint            start_pos,
                            gint            end_pos)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GtkTextIter start_itr;
  GtkTextIter end_itr;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = view->buffer;

  gtk_text_buffer_get_iter_at_offset (buffer, &start_itr, start_pos);
  gtk_text_buffer_get_iter_at_offset (buffer, &end_itr, end_pos);
  gtk_text_buffer_delete (buffer, &start_itr, &end_itr);
}

static void
gail_text_view_paste_text (AtkEditableText *text,
                           gint            position)
{
  GtkTextView *view;
  GtkWidget *widget;
  GtkTextBuffer *buffer;
  GailTextViewPaste paste_struct;
  GtkClipboard *clipboard;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return;

  view = GTK_TEXT_VIEW (widget);
  if (!gtk_text_view_get_editable (view))
    return;
  buffer = view->buffer;

  paste_struct.buffer = buffer;
  paste_struct.position = position;

  g_object_ref (paste_struct.buffer);
  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text (clipboard,
    gail_text_view_paste_received, &paste_struct);
}

static void
gail_text_view_paste_received (GtkClipboard *clipboard,
                               const gchar  *text,
                               gpointer     data)
{
  GailTextViewPaste* paste_struct = (GailTextViewPaste *)data;
  GtkTextIter pos_itr;

  if (text)
    {
      gtk_text_buffer_get_iter_at_offset (paste_struct->buffer, &pos_itr,
         paste_struct->position);
      gtk_text_buffer_insert (paste_struct->buffer, &pos_itr, text, -1);
    }

  g_object_unref (paste_struct->buffer);
}

/* Callbacks */

/* Note arg1 returns the start of the insert range, arg3 returns the
 * end of the insert range if multiple characters are inserted.  If one
 * character is inserted they have the same value, which is the caret
 * location.  arg2 returns the begin location of the insert.
 */
static void 
_gail_text_view_insert_text_cb (GtkTextBuffer *buffer,
                                GtkTextIter   *arg1, 
                                gchar         *arg2, 
                                gint          arg3,
                                gpointer      user_data)
{
  GtkTextView *text = (GtkTextView *) user_data;
  AtkObject *accessible;
  GailTextView *gail_text_view;
  gint position;
  gint length;

  g_return_if_fail (arg3 > 0);

  accessible = gtk_widget_get_accessible(GTK_WIDGET(text));
  gail_text_view = GAIL_TEXT_VIEW (accessible);

  gail_text_view->signal_name = "text_changed::insert";
  position = gtk_text_iter_get_offset (arg1);
  length = g_utf8_strlen(arg2, arg3);
  
  if (gail_text_view->length == 0)
    {
      gail_text_view->position = position;
      gail_text_view->length = length;
    }
  else if (gail_text_view->position + gail_text_view->length == position)
    {
      gail_text_view->length += length;
    }
  else
    {
      /*
       * We have a non-contiguous insert so report what we have
       */
      if (gail_text_view->insert_notify_handler)
        {
          g_source_remove (gail_text_view->insert_notify_handler);
        }
      gail_text_view->insert_notify_handler = 0;
      insert_idle_handler (gail_text_view);
      gail_text_view->position = position;
      gail_text_view->length = length;
    }
    
  /*
   * The signal will be emitted when the changed signal is received
   */
}

/* Note arg1 returns the start of the delete range, arg2 returns the
 * end of the delete range if multiple characters are deleted.  If one
 * character is deleted they have the same value, which is the caret
 * location.
 */
static void 
_gail_text_view_delete_range_cb (GtkTextBuffer *buffer,
                                 GtkTextIter   *arg1, 
                                 GtkTextIter   *arg2,
                                 gpointer      user_data)
{
  GtkTextView *text = (GtkTextView *) user_data;
  AtkObject *accessible;
  GailTextView *gail_text_view;
  gint offset = gtk_text_iter_get_offset (arg1);
  gint length = gtk_text_iter_get_offset (arg2) - offset;

  accessible = gtk_widget_get_accessible(GTK_WIDGET(text));
  gail_text_view = GAIL_TEXT_VIEW (accessible);
  if (gail_text_view->insert_notify_handler)
    {
      g_source_remove (gail_text_view->insert_notify_handler);
      gail_text_view->insert_notify_handler = 0;
      if (gail_text_view->position == offset && 
          gail_text_view->length == length)
        {
        /*
         * Do not bother with insert and delete notifications
         */
          gail_text_view->signal_name = NULL;
          gail_text_view->position = 0;
          gail_text_view->length = 0;
          return;
        }

      insert_idle_handler (gail_text_view);
    }
  g_signal_emit_by_name (accessible, "text_changed::delete",
                         offset, length);
}

/* Note arg1 and arg2 point to the same offset, which is the caret
 * position after the move
 */
static void 
_gail_text_view_mark_set_cb (GtkTextBuffer *buffer,
                             GtkTextIter   *arg1, 
                             GtkTextMark   *arg2,
                             gpointer      user_data)
{
  GtkTextView *text = (GtkTextView *) user_data;
  AtkObject *accessible;
  GailTextView *gail_text_view;
  const char *mark_name = gtk_text_mark_get_name(arg2);

  accessible = gtk_widget_get_accessible(GTK_WIDGET(text));
  gail_text_view = GAIL_TEXT_VIEW (accessible);

  /*
   * Only generate the signal for the "insert" mark, which
   * represents the cursor.
   */
  if (mark_name && !strcmp(mark_name, "insert"))
    {
      int insert_offset, selection_bound;
      gboolean selection_changed;

      insert_offset = gtk_text_iter_get_offset (arg1);

      selection_bound = get_selection_bound (buffer);
      if (selection_bound != insert_offset)
        {
          if (selection_bound != gail_text_view->previous_selection_bound ||
              insert_offset != gail_text_view->previous_insert_offset)
            {
              selection_changed = TRUE;
            }
          else
            {
              selection_changed = FALSE;
            }
        }
      else if (gail_text_view->previous_selection_bound != gail_text_view->previous_insert_offset)
        {
          selection_changed = TRUE;
        }
      else
        {
          selection_changed = FALSE;
        }

      emit_text_caret_moved (gail_text_view, insert_offset);
      /*
       * insert and selection_bound marks are different to a selection
       * has changed
       */
      if (selection_changed)
        g_signal_emit_by_name (accessible, "text_selection_changed");
      gail_text_view->previous_selection_bound = selection_bound;
    }
}

static void 
_gail_text_view_changed_cb (GtkTextBuffer *buffer,
                            gpointer      user_data)
{
  GtkTextView *text = (GtkTextView *) user_data;
  AtkObject *accessible;
  GailTextView *gail_text_view;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (text));
  gail_text_view = GAIL_TEXT_VIEW (accessible);
  if (gail_text_view->signal_name)
    {
      if (!gail_text_view->insert_notify_handler)
        {
          gail_text_view->insert_notify_handler = gdk_threads_add_idle (insert_idle_handler, accessible);
        }
      return;
    }
  emit_text_caret_moved (gail_text_view, get_insert_offset (buffer));
  gail_text_view->previous_selection_bound = get_selection_bound (buffer);
}

static gchar*
get_text_near_offset (AtkText          *text,
                      GailOffsetType   function,
                      AtkTextBoundary  boundary_type,
                      gint             offset,
                      gint             *start_offset,
                      gint             *end_offset)
{
  GtkTextView *view;
  gpointer layout = NULL;

  view = GTK_TEXT_VIEW (GTK_ACCESSIBLE (text)->widget);

  /*
   * Pass the GtkTextView to the function gail_text_util_get_text() 
   * so it can find the start and end of the current line on the display.
   */
  if (boundary_type == ATK_TEXT_BOUNDARY_LINE_START ||
      boundary_type == ATK_TEXT_BOUNDARY_LINE_END)
    layout = view;

  return gail_text_util_get_text (GAIL_TEXT_VIEW (text)->textutil, layout,
                                  function, boundary_type, offset, 
                                    start_offset, end_offset);
}

static gint
get_insert_offset (GtkTextBuffer *buffer)
{
  GtkTextMark *cursor_mark;
  GtkTextIter cursor_itr;

  cursor_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor_itr, cursor_mark);
  return gtk_text_iter_get_offset (&cursor_itr);
}

static gint
get_selection_bound (GtkTextBuffer *buffer)
{
  GtkTextMark *selection_mark;
  GtkTextIter selection_itr;

  selection_mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_itr, selection_mark);
  return gtk_text_iter_get_offset (&selection_itr);
}

static void
emit_text_caret_moved (GailTextView *gail_text_view,
                       gint          insert_offset)
{
  /*
   * If we have text which has been inserted notify the user
   */
  if (gail_text_view->insert_notify_handler)
    {
      g_source_remove (gail_text_view->insert_notify_handler);
      gail_text_view->insert_notify_handler = 0;
      insert_idle_handler (gail_text_view);
    }

  if (insert_offset != gail_text_view->previous_insert_offset)
    {
      /*
       * If the caret position has not changed then don't bother notifying
       *
       * When mouse click is used to change caret position, notification
       * is received on button down and button up.
       */
      g_signal_emit_by_name (gail_text_view, "text_caret_moved", insert_offset);
      gail_text_view->previous_insert_offset = insert_offset;
    }
}

static gint
insert_idle_handler (gpointer data)
{
  GailTextView *gail_text_view;
  GtkTextBuffer *buffer;

  gail_text_view = GAIL_TEXT_VIEW (data);

  g_signal_emit_by_name (data,
                         gail_text_view->signal_name,
                         gail_text_view->position,
                         gail_text_view->length);
  gail_text_view->signal_name = NULL;
  gail_text_view->position = 0;
  gail_text_view->length = 0;

  buffer = gail_text_view->textutil->buffer;
  if (gail_text_view->insert_notify_handler)
    {
    /*
     * If called from idle handler notify caret moved
     */
      gail_text_view->insert_notify_handler = 0;
      emit_text_caret_moved (gail_text_view, get_insert_offset (buffer));
      gail_text_view->previous_selection_bound = get_selection_bound (buffer);
    }

  return FALSE;
}

static void       
atk_streamable_content_interface_init    (AtkStreamableContentIface *iface)
{
  iface->get_n_mime_types = gail_streamable_content_get_n_mime_types;
  iface->get_mime_type = gail_streamable_content_get_mime_type;
  iface->get_stream = gail_streamable_content_get_stream;
}

static gint       gail_streamable_content_get_n_mime_types (AtkStreamableContent *streamable)
{
    gint n_mime_types = 0;

    if (GAIL_IS_TEXT_VIEW (streamable) && GAIL_TEXT_VIEW (streamable)->textutil)
    {
	int i;
	gboolean advertises_plaintext = FALSE;
	GdkAtom *atoms =
	     gtk_text_buffer_get_serialize_formats (
		GAIL_TEXT_VIEW (streamable)->textutil->buffer, 
		&n_mime_types);
	for (i = 0; i < n_mime_types-1; ++i)
	    if (!strcmp ("text/plain", gdk_atom_name (atoms[i])))
		advertises_plaintext = TRUE;
	if (!advertises_plaintext) ++n_mime_types; 
        /* we support text/plain even if the GtkTextBuffer doesn't */
    }
    return n_mime_types;
}

static const gchar*
gail_streamable_content_get_mime_type (AtkStreamableContent *streamable, gint i)
{
    if (GAIL_IS_TEXT_VIEW (streamable) && GAIL_TEXT_VIEW (streamable)->textutil)
    {
	gint n_mime_types = 0;
	GdkAtom *atoms;
	atoms = gtk_text_buffer_get_serialize_formats (
	    GAIL_TEXT_VIEW (streamable)->textutil->buffer, 
	    &n_mime_types);
	if (i < n_mime_types)
	{
	    return gdk_atom_name (atoms [i]);
	}
	else if (i == n_mime_types)
	    return "text/plain";
    }
    return NULL;
}

static GIOChannel*       gail_streamable_content_get_stream       (AtkStreamableContent *streamable,
								   const gchar *mime_type)
{
    gint i, n_mime_types = 0;
    GdkAtom *atoms;
    if (!GAIL_IS_TEXT_VIEW (streamable) || !GAIL_TEXT_VIEW (streamable)->textutil)
	return NULL;
    atoms = gtk_text_buffer_get_serialize_formats (
	GAIL_TEXT_VIEW (streamable)->textutil->buffer, 
	&n_mime_types);
    for (i = 0; i < n_mime_types; ++i) 
    {
	if (!strcmp ("text/plain", mime_type) ||
	    !strcmp (gdk_atom_name (atoms[i]), mime_type)) {
	    GtkTextBuffer *buffer;
	    guint8 *cbuf;
	    GError *err = NULL;
	    gsize len, written;
	    gchar tname[80];
	    GtkTextIter start, end;
	    GIOChannel *gio = NULL;
	    int fd;
	    buffer = GAIL_TEXT_VIEW (streamable)->textutil->buffer;
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
	    if (!err) g_io_channel_write_chars (gio, (const char *) cbuf, (gssize) len, &written, &err);
	    else g_message ("%s", err->message);
	    if (!err) g_io_channel_seek_position (gio, 0, G_SEEK_SET, &err);
	    else g_message ("%s", err->message);
	    if (!err) g_io_channel_flush (gio, &err);
	    else g_message ("%s", err->message);
	    if (err) {
		g_message ("<error writing to stream [%s]>", tname);
		g_error_free (err);
	    }
	    /* make sure the file is removed on unref of the giochannel */
	    else {
		g_unlink (tname);
		return gio; 
	    }
	}
    }
    return NULL;
}

