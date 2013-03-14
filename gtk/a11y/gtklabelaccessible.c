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

#include <gtk/gtk.h>
#include <gtk/gtkpango.h>
#include "gtkwidgetprivate.h"
#include "gtklabelaccessible.h"

struct _GtkLabelAccessiblePrivate
{
  gint cursor_position;
  gint selection_bound;
};

static void atk_text_interface_init (AtkTextIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLabelAccessible, gtk_label_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gtk_label_accessible_init (GtkLabelAccessible *label)
{
  label->priv = G_TYPE_INSTANCE_GET_PRIVATE (label,
                                             GTK_TYPE_LABEL_ACCESSIBLE,
                                             GtkLabelAccessiblePrivate);
}

static void
gtk_label_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  GtkWidget  *widget;

  ATK_OBJECT_CLASS (gtk_label_accessible_parent_class)->initialize (obj, data);

  widget = GTK_WIDGET (data);

  /*
   * Check whether ancestor of GtkLabel is a GtkButton and if so
   * set accessible parent for GtkLabelAccessible
   */
  while (widget != NULL)
    {
      widget = gtk_widget_get_parent (widget);
      if (GTK_IS_BUTTON (widget))
        {
          atk_object_set_parent (obj, gtk_widget_get_accessible (widget));
          break;
        }
    }

  obj->role = ATK_ROLE_LABEL;
}

static gboolean
check_for_selection_change (GtkLabelAccessible *accessible,
                            GtkLabel           *label)
{
  gboolean ret_val = FALSE;
  gint start, end;

  if (gtk_label_get_selection_bounds (label, &start, &end))
    {
      if (end != accessible->priv->cursor_position ||
          start != accessible->priv->selection_bound)
        ret_val = TRUE;
    }
  else
    {
      ret_val = (accessible->priv->cursor_position != accessible->priv->selection_bound);
    }

  accessible->priv->cursor_position = end;
  accessible->priv->selection_bound = start;

  return ret_val;
}

void
_gtk_label_accessible_text_deleted (GtkLabel *label)
{
  AtkObject *obj;
  const char *text;
  guint length;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (label));
  if (obj == NULL)
    return;

  text = gtk_label_get_text (label);
  length = g_utf8_strlen (text, -1);
  if (length > 0)
    g_signal_emit_by_name (obj, "text-changed::delete", 0, length);
}

void
_gtk_label_accessible_text_inserted (GtkLabel *label)
{
  AtkObject *obj;
  const char *text;
  guint length;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (label));
  if (obj == NULL)
    return;

  text = gtk_label_get_text (label);
  length = g_utf8_strlen (text, -1);
  if (length > 0)
    g_signal_emit_by_name (obj, "text-changed::insert", 0, length);

  if (obj->name == NULL)
    /* The label has changed so notify a change in accessible-name */
    g_object_notify (G_OBJECT (obj), "accessible-name");

  g_signal_emit_by_name (obj, "visible-data-changed");
}

static void
gtk_label_accessible_notify_gtk (GObject    *obj,
                                 GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  AtkObject* atk_obj = gtk_widget_get_accessible (widget);
  GtkLabelAccessible *accessible;

  accessible = GTK_LABEL_ACCESSIBLE (atk_obj);

  if (g_strcmp0 (pspec->name, "cursor-position") == 0)
    {
      g_signal_emit_by_name (atk_obj, "text-caret-moved",
                             _gtk_label_get_cursor_position (GTK_LABEL (widget)));
      if (check_for_selection_change (accessible, GTK_LABEL (widget)))
        g_signal_emit_by_name (atk_obj, "text-selection-changed");
    }
  else if (g_strcmp0 (pspec->name, "selection-bound") == 0)
    {
      if (check_for_selection_change (accessible, GTK_LABEL (widget)))
        g_signal_emit_by_name (atk_obj, "text-selection-changed");
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_label_accessible_parent_class)->notify_gtk (obj, pspec);
}

/* atkobject.h */

static AtkStateSet *
gtk_label_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  state_set = ATK_OBJECT_CLASS (gtk_label_accessible_parent_class)->ref_state_set (accessible);
  atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

  return state_set;
}

static AtkRelationSet *
gtk_label_accessible_ref_relation_set (AtkObject *obj)
{
  GtkWidget *widget;
  AtkRelationSet *relation_set;

  g_return_val_if_fail (GTK_IS_LABEL_ACCESSIBLE (obj), NULL);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  relation_set = ATK_OBJECT_CLASS (gtk_label_accessible_parent_class)->ref_relation_set (obj);

  if (!atk_relation_set_contains (relation_set, ATK_RELATION_LABEL_FOR))
    {
      /* Get the mnemonic widget.
       * The relation set is not updated if the mnemonic widget is changed
       */
      GtkWidget *mnemonic_widget;

      mnemonic_widget = gtk_label_get_mnemonic_widget (GTK_LABEL (widget));

      if (mnemonic_widget)
        {
          AtkObject *accessible_array[1];
          AtkRelation* relation;

          if (!gtk_widget_get_can_focus (mnemonic_widget))
            {
            /*
             * Handle the case where a GtkFileChooserButton is specified
             * as the mnemonic widget. use the combobox which is a child of the
             * GtkFileChooserButton as the mnemonic widget. See bug #359843.
             */
             if (GTK_IS_BOX (mnemonic_widget))
               {
                  GList *list, *tmpl;

                  list = gtk_container_get_children (GTK_CONTAINER (mnemonic_widget));
                  if (g_list_length (list) == 2)
                    {
                      tmpl = g_list_last (list);
                      if (GTK_IS_COMBO_BOX(tmpl->data))
                        {
                          mnemonic_widget = GTK_WIDGET(tmpl->data);
                        }
                    }
                  g_list_free (list);
                }
            }
          accessible_array[0] = gtk_widget_get_accessible (mnemonic_widget);
          relation = atk_relation_new (accessible_array, 1,
                                       ATK_RELATION_LABEL_FOR);
          atk_relation_set_add (relation_set, relation);
          /*
           * Unref the relation so that it is not leaked.
           */
          g_object_unref (relation);
        }
    }
  return relation_set;
}

static const gchar*
gtk_label_accessible_get_name (AtkObject *accessible)
{
  const gchar *name;

  g_return_val_if_fail (GTK_IS_LABEL_ACCESSIBLE (accessible), NULL);

  name = ATK_OBJECT_CLASS (gtk_label_accessible_parent_class)->get_name (accessible);
  if (name != NULL)
    return name;
  else
    {
      /*
       * Get the text on the label
       */
      GtkWidget *widget;

      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
      if (widget == NULL)
        return NULL;

      g_return_val_if_fail (GTK_IS_LABEL (widget), NULL);

      return gtk_label_get_text (GTK_LABEL (widget));
    }
}

static void
gtk_label_accessible_class_init (GtkLabelAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = GTK_WIDGET_ACCESSIBLE_CLASS (klass);

  widget_class->notify_gtk = gtk_label_accessible_notify_gtk;

  class->get_name = gtk_label_accessible_get_name;
  class->ref_state_set = gtk_label_accessible_ref_state_set;
  class->ref_relation_set = gtk_label_accessible_ref_relation_set;
  class->initialize = gtk_label_accessible_initialize;

  g_type_class_add_private (klass, sizeof (GtkLabelAccessiblePrivate));
}

/* atktext.h */

static gchar*
gtk_label_accessible_get_text (AtkText *atk_text,
                               gint     start_pos,
                               gint     end_pos)
{
  GtkWidget *widget;
  const gchar *text;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return NULL;

  text = gtk_label_get_text (GTK_LABEL (widget));

  if (text)
    {
      guint length;
      const gchar *start, *end;

      length = g_utf8_strlen (text, -1);
      if (end_pos < 0 || end_pos > length)
        end_pos = length;
      if (start_pos > length)
        start_pos = length;
      if (end_pos <= start_pos)
        return g_strdup ("");
      start = g_utf8_offset_to_pointer (text, start_pos);
      end = g_utf8_offset_to_pointer (start, end_pos - start_pos);
      return g_strndup (start, end - start);
    }

  return NULL;
}

static gchar *
gtk_label_accessible_get_text_before_offset (AtkText         *text,
                                             gint             offset,
                                             AtkTextBoundary  boundary_type,
                                             gint            *start_offset,
                                             gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_before (gtk_label_get_layout (GTK_LABEL (widget)),
                                     boundary_type, offset,
                                     start_offset, end_offset);
}

static gchar*
gtk_label_accessible_get_text_at_offset (AtkText         *text,
                                         gint             offset,
                                         AtkTextBoundary  boundary_type,
                                         gint            *start_offset,
                                         gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_at (gtk_label_get_layout (GTK_LABEL (widget)),
                                 boundary_type, offset,
                                 start_offset, end_offset);
}

static gchar*
gtk_label_accessible_get_text_after_offset (AtkText         *text,
                                            gint             offset,
                                            AtkTextBoundary  boundary_type,
                                            gint            *start_offset,
                                            gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_after (gtk_label_get_layout (GTK_LABEL (widget)),
                                    boundary_type, offset,
                                    start_offset, end_offset);
}

static gint
gtk_label_accessible_get_character_count (AtkText *atk_text)
{
  GtkWidget *widget;
  const gchar *text;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return 0;

  text = gtk_label_get_text (GTK_LABEL (widget));

  if (text)
    return g_utf8_strlen (text, -1);

  return 0;
}

static gint
gtk_label_accessible_get_caret_offset (AtkText *text)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  return _gtk_label_get_cursor_position (GTK_LABEL (widget));
}

static gboolean
gtk_label_accessible_set_caret_offset (AtkText *text,
                                       gint     offset)
{
  GtkWidget *widget;
  GtkLabel *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
    return FALSE;

  gtk_label_select_region (label, offset, offset);

  return TRUE;
}

static gint
gtk_label_accessible_get_n_selections (AtkText *text)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  if (gtk_label_get_selection_bounds (GTK_LABEL (widget), NULL, NULL))
    return 1;

  return 0;
}

static gchar *
gtk_label_accessible_get_selection (AtkText *atk_text,
                                    gint     selection_num,
                                    gint    *start_pos,
                                    gint    *end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return NULL;

  if (selection_num != 0)
    return NULL;

  label = GTK_LABEL (widget);

  if (gtk_label_get_selection_bounds (label, start_pos, end_pos))
    {
      const gchar *text;

      text = gtk_label_get_text (label);

      if (text)
        return g_utf8_substring (text, *start_pos, *end_pos);
    }

  return NULL;
}

static gboolean
gtk_label_accessible_add_selection (AtkText *text,
                                    gint     start_pos,
                                    gint     end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
    return FALSE;

  if (!gtk_label_get_selection_bounds (label, &start, &end))
    {
      gtk_label_select_region (label, start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_label_accessible_remove_selection (AtkText *text,
                                       gint     selection_num)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
     return FALSE;

  if (gtk_label_get_selection_bounds (label, &start, &end))
    {
      gtk_label_select_region (label, end, end);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_label_accessible_set_selection (AtkText *text,
                                    gint     selection_num,
                                    gint     start_pos,
                                    gint     end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
    return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
    return FALSE;

  if (gtk_label_get_selection_bounds (label, &start, &end))
    {
      gtk_label_select_region (label, start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_label_accessible_get_character_extents (AtkText      *text,
                                            gint          offset,
                                            gint         *x,
                                            gint         *y,
                                            gint         *width,
                                            gint         *height,
                                            AtkCoordType  coords)
{
  GtkWidget *widget;
  GtkLabel *label;
  PangoRectangle char_rect;
  const gchar *label_text;
  gint index, x_layout, y_layout;
  GdkWindow *window;
  gint x_window, y_window;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  label = GTK_LABEL (widget);

  gtk_label_get_layout_offsets (label, &x_layout, &y_layout);
  label_text = gtk_label_get_text (label);
  index = g_utf8_offset_to_pointer (label_text, offset) - label_text;
  pango_layout_index_to_pos (gtk_label_get_layout (label), index, &char_rect);
  pango_extents_to_pixels (&char_rect, NULL);

  window = gtk_widget_get_window (widget);
  gdk_window_get_origin (window, &x_window, &y_window);

  *x = x_window + x_layout + char_rect.x;
  *y = y_window + y_layout + char_rect.y;
  *width = char_rect.width;
  *height = char_rect.height;

  if (coords == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (window);
      gdk_window_get_origin (window, &x_window, &y_window);

      *x -= x_window;
      *y -= y_window;
    }
}

static gint
gtk_label_accessible_get_offset_at_point (AtkText      *atk_text,
                                          gint          x,
                                          gint          y,
                                          AtkCoordType  coords)
{
  GtkWidget *widget;
  GtkLabel *label;
  const gchar *text;
  gint index, x_layout, y_layout;
  gint x_window, y_window;
  gint x_local, y_local;
  GdkWindow *window;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return -1;

  label = GTK_LABEL (widget);

  gtk_label_get_layout_offsets (label, &x_layout, &y_layout);

  window = gtk_widget_get_window (widget);
  gdk_window_get_origin (window, &x_window, &y_window);

  x_local = x - x_layout - x_window;
  y_local = y - y_layout - y_window;

  if (coords == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (window);
      gdk_window_get_origin (window, &x_window, &y_window);

      x_local += x_window;
      y_local += y_window;
    }

  if (!pango_layout_xy_to_index (gtk_label_get_layout (label),
                                 x_local * PANGO_SCALE,
                                 y_local * PANGO_SCALE,
                                 &index, NULL))
    {
      if (x_local < 0 || y_local < 0)
        index = 0;
      else
        index = -1;
    }

  if (index != -1)
    {
      text = gtk_label_get_text (label);
      return g_utf8_pointer_to_offset (text, text + index);
    }

  return -1;
}

static AtkAttributeSet *
add_attribute (AtkAttributeSet  *attributes,
               AtkTextAttribute  attr,
               const gchar      *value)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (value);

  return g_slist_prepend (attributes, at);
}

static AtkAttributeSet*
gtk_label_accessible_get_run_attributes (AtkText *text,
                                         gint     offset,
                                         gint    *start_offset,
                                         gint    *end_offset)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  attributes = NULL;
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_DIRECTION,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION,
                                                 gtk_widget_get_direction (widget)));
  attributes = _gtk_pango_get_run_attributes (attributes,
                                              gtk_label_get_layout (GTK_LABEL (widget)),
                                              offset,
                                              start_offset,
                                              end_offset);

  return attributes;
}

static AtkAttributeSet *
gtk_label_accessible_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  attributes = NULL;
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_DIRECTION,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION,
                                                 gtk_widget_get_direction (widget)));
  attributes = _gtk_pango_get_default_attributes (attributes,
                                                  gtk_label_get_layout (GTK_LABEL (widget)));
  attributes = _gtk_style_context_get_attributes (attributes,
                                                  gtk_widget_get_style_context (widget),
                                                  gtk_widget_get_state_flags (widget));

  return attributes;
}

static gunichar
gtk_label_accessible_get_character_at_offset (AtkText *atk_text,
                                              gint     offset)
{
  GtkWidget *widget;
  const gchar *text;
  gchar *index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return '\0';

  text = gtk_label_get_text (GTK_LABEL (widget));
  if (offset >= g_utf8_strlen (text, -1))
    return '\0';

  index = g_utf8_offset_to_pointer (text, offset);

  return g_utf8_get_char (index);
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_label_accessible_get_text;
  iface->get_character_at_offset = gtk_label_accessible_get_character_at_offset;
  iface->get_text_before_offset = gtk_label_accessible_get_text_before_offset;
  iface->get_text_at_offset = gtk_label_accessible_get_text_at_offset;
  iface->get_text_after_offset = gtk_label_accessible_get_text_after_offset;
  iface->get_character_count = gtk_label_accessible_get_character_count;
  iface->get_caret_offset = gtk_label_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_label_accessible_set_caret_offset;
  iface->get_n_selections = gtk_label_accessible_get_n_selections;
  iface->get_selection = gtk_label_accessible_get_selection;
  iface->add_selection = gtk_label_accessible_add_selection;
  iface->remove_selection = gtk_label_accessible_remove_selection;
  iface->set_selection = gtk_label_accessible_set_selection;
  iface->get_character_extents = gtk_label_accessible_get_character_extents;
  iface->get_offset_at_point = gtk_label_accessible_get_offset_at_point;
  iface->get_run_attributes = gtk_label_accessible_get_run_attributes;
  iface->get_default_attributes = gtk_label_accessible_get_default_attributes;
}

