/* GTK+ - accessibility implementations
 * Copyright (C) 2002, 2004  Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkiconviewaccessibleprivate.h"

#include <string.h>

#include "gtk/gtkadjustment.h"
#include "gtk/gtkiconviewprivate.h"
#include "gtk/gtkcellrendererpixbuf.h"
#include "gtk/gtkcellrenderertext.h"
#include "gtk/gtkpango.h"
#include "gtk/gtkwidgetprivate.h"

#define GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE      (_gtk_icon_view_item_accessible_get_type ())
#define GTK_ICON_VIEW_ITEM_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE, GtkIconViewItemAccessible))
#define GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE))

struct _GtkIconViewAccessiblePrivate
{
  GList *items;
  GtkTreeModel *model;
};

typedef struct
{
  AtkObject parent;

  GtkIconViewItem *item;
  GtkWidget *widget;
  AtkStateSet *state_set;
  gchar *text;
  gchar *action_description;
  gchar *image_description;
  guint action_idle_handler;
} GtkIconViewItemAccessible;

typedef struct
{
  AtkObjectClass parent_class;

} GtkIconViewItemAccessibleClass;

GType _gtk_icon_view_item_accessible_get_type (void);

static gboolean gtk_icon_view_item_accessible_is_showing (GtkIconViewItemAccessible *item);

static void atk_component_item_interface_init (AtkComponentIface *iface);
static void atk_action_item_interface_init    (AtkActionIface    *iface);
static void atk_text_item_interface_init      (AtkTextIface      *iface);
static void atk_image_item_interface_init     (AtkImageIface     *iface);

G_DEFINE_TYPE_WITH_CODE (GtkIconViewItemAccessible, _gtk_icon_view_item_accessible, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_item_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_item_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_item_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_item_interface_init))


static gboolean
idle_do_action (gpointer data)
{
  GtkIconViewItemAccessible *item;
  GtkIconView *icon_view;
  GtkTreePath *path;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (data);
  item->action_idle_handler = 0;

  if (item->widget != NULL)
    {
      icon_view = GTK_ICON_VIEW (item->widget);
      path = gtk_tree_path_new_from_indices (item->item->index, -1);
      gtk_icon_view_item_activated (icon_view, path);
      gtk_tree_path_free (path);
    }

  return FALSE;
}

static gboolean
gtk_icon_view_item_accessible_do_action (AtkAction *action,
                                         gint       i)
{
  GtkIconViewItemAccessible *item;

  if (i != 0)
    return FALSE;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (action);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return FALSE;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return FALSE;

  if (!item->action_idle_handler)
    {
      item->action_idle_handler = gdk_threads_add_idle (idle_do_action, item);
      g_source_set_name_by_id (item->action_idle_handler, "[gtk+] idle_do_action");
    }

  return TRUE;
}

static gint
gtk_icon_view_item_accessible_get_n_actions (AtkAction *action)
{
        return 1;
}

static const gchar *
gtk_icon_view_item_accessible_get_description (AtkAction *action,
                                               gint       i)
{
  GtkIconViewItemAccessible *item;

  if (i != 0)
    return NULL;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (action);

  if (item->action_description)
    return item->action_description;
  else
    return "Activate item";
}

static const gchar *
gtk_icon_view_item_accessible_get_name (AtkAction *action,
                                        gint       i)
{
  if (i != 0)
    return NULL;

  return "activate";
}

static gboolean
gtk_icon_view_item_accessible_set_description (AtkAction   *action,
                                               gint         i,
                                               const gchar *description)
{
  GtkIconViewItemAccessible *item;

  if (i != 0)
    return FALSE;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (action);

  g_free (item->action_description);
  item->action_description = g_strdup (description);

  return TRUE;
}

static void
atk_action_item_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_icon_view_item_accessible_do_action;
  iface->set_description = gtk_icon_view_item_accessible_set_description;
  iface->get_name = gtk_icon_view_item_accessible_get_name;
  iface->get_n_actions = gtk_icon_view_item_accessible_get_n_actions;
  iface->get_description = gtk_icon_view_item_accessible_get_description;
}

static const gchar *
gtk_icon_view_item_accessible_get_image_description (AtkImage *image)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  return item->image_description;
}

static gboolean
gtk_icon_view_item_accessible_set_image_description (AtkImage    *image,
                                                     const gchar *description)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  g_free (item->image_description);
  item->image_description = g_strdup (description);

  return TRUE;
}

typedef struct {
  GdkRectangle box;
  gboolean     pixbuf_found;
} GetPixbufBoxData;

static gboolean
get_pixbuf_foreach (GtkCellRenderer    *renderer,
                    const GdkRectangle *cell_area,
                    const GdkRectangle *cell_background,
                    GetPixbufBoxData   *data)
{
  if (GTK_IS_CELL_RENDERER_PIXBUF (renderer))
    {
      data->box = *cell_area;
      data->pixbuf_found = TRUE;
    }
  return (data->pixbuf_found != FALSE);
}

static gboolean
get_pixbuf_box (GtkIconView     *icon_view,
                GtkIconViewItem *item,
                GdkRectangle    *box)
{
  GetPixbufBoxData data = { { 0, }, FALSE };
  GtkCellAreaContext *context;

  context = g_ptr_array_index (icon_view->priv->row_contexts, item->row);

  _gtk_icon_view_set_cell_data (icon_view, item);
  gtk_cell_area_foreach_alloc (icon_view->priv->cell_area, context,
                               GTK_WIDGET (icon_view),
                               &item->cell_area, &item->cell_area,
                               (GtkCellAllocCallback)get_pixbuf_foreach, &data);

  *box = data.box;

  return data.pixbuf_found;
}

static gboolean
get_text_foreach (GtkCellRenderer  *renderer,
                  gchar           **text)
{
  if (GTK_IS_CELL_RENDERER_TEXT (renderer))
    {
      g_object_get (renderer, "text", text, NULL);
      return TRUE;
    }
  return FALSE;
}

static gchar *
get_text (GtkIconView     *icon_view,
          GtkIconViewItem *item)
{
  gchar *text = NULL;

  _gtk_icon_view_set_cell_data (icon_view, item);
  gtk_cell_area_foreach (icon_view->priv->cell_area,
                         (GtkCellCallback)get_text_foreach, &text);

  return text;
}

static void
gtk_icon_view_item_accessible_get_image_size (AtkImage *image,
                                              gint     *width,
                                              gint     *height)
{
  GtkIconViewItemAccessible *item;
  GdkRectangle box;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

  *width = 0;
  *height = 0;

  if (get_pixbuf_box (GTK_ICON_VIEW (item->widget), item->item, &box))
    {
      *width = box.width;
      *height = box.height;
    }
}

static void
gtk_icon_view_item_accessible_get_image_position (AtkImage    *image,
                                                  gint        *x,
                                                  gint        *y,
                                                  AtkCoordType coord_type)
{
  GtkIconViewItemAccessible *item;
  GdkRectangle box;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (image);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

  atk_component_get_extents (ATK_COMPONENT (image), x, y, NULL, NULL,
                             coord_type);

  if (get_pixbuf_box (GTK_ICON_VIEW (item->widget), item->item, &box))
    {
      *x+= box.x - item->item->cell_area.x;
      *y+= box.y - item->item->cell_area.y;
    }

}

static void
atk_image_item_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gtk_icon_view_item_accessible_get_image_description;
  iface->set_image_description = gtk_icon_view_item_accessible_set_image_description;
  iface->get_image_size = gtk_icon_view_item_accessible_get_image_size;
  iface->get_image_position = gtk_icon_view_item_accessible_get_image_position;
}

static gchar *
gtk_icon_view_item_accessible_get_text (AtkText *text,
                                        gint     start_pos,
                                        gint     end_pos)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);
  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  if (item->text)
    return g_utf8_substring (item->text, start_pos, end_pos > -1 ? end_pos : g_utf8_strlen (item->text, -1));
  else
    return g_strdup ("");
}

static gunichar
gtk_icon_view_item_accessible_get_character_at_offset (AtkText *text,
                                                       gint     offset)
{
  GtkIconViewItemAccessible *item;
  gchar *string;
  gchar *index;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);
  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return '\0';

  string = item->text;

  if (!string)
    return '\0';

  if (offset >= g_utf8_strlen (string, -1))
    return '\0';

  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static PangoLayout *
create_pango_layout (GtkIconViewItemAccessible *item)
{
  PangoLayout *layout;

  layout = gtk_widget_create_pango_layout (item->widget, item->text);

  return layout;
}

static gchar *
gtk_icon_view_item_accessible_get_text_before_offset (AtkText         *atk_text,
                                                      gint             offset,
                                                      AtkTextBoundary  boundary_type,
                                                      gint            *start_offset,
                                                      gint            *end_offset)
{
  GtkIconViewItemAccessible *item;
  PangoLayout *layout;
  gchar *text;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (atk_text);
  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  layout = create_pango_layout (item);
  text = _gtk_pango_get_text_before (layout, boundary_type, offset, start_offset, end_offset);
  g_object_unref (layout);

  return text;
}

static gchar *
gtk_icon_view_item_accessible_get_text_at_offset (AtkText         *atk_text,
                                                  gint             offset,
                                                  AtkTextBoundary  boundary_type,
                                                  gint            *start_offset,
                                                  gint            *end_offset)
{
  GtkIconViewItemAccessible *item;
  PangoLayout *layout;
  gchar *text;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (atk_text);
  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  layout = create_pango_layout (item);
  text = _gtk_pango_get_text_at (layout, boundary_type, offset, start_offset, end_offset);
  g_object_unref (layout);

  return text;
}

static gchar *
gtk_icon_view_item_accessible_get_text_after_offset (AtkText         *atk_text,
                                                     gint             offset,
                                                     AtkTextBoundary  boundary_type,
                                                     gint            *start_offset,
                                                     gint            *end_offset)
{
  GtkIconViewItemAccessible *item;
  PangoLayout *layout;
  gchar *text;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (atk_text);
  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return NULL;

  layout = create_pango_layout (item);
  text = _gtk_pango_get_text_after (layout, boundary_type, offset, start_offset, end_offset);
  g_object_unref (layout);

  return text;
}

static gint
gtk_icon_view_item_accessible_get_character_count (AtkText *text)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);
  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return 0;

  if (item->text)
    return g_utf8_strlen (item->text, -1);
  else
    return 0;
}

static void
gtk_icon_view_item_accessible_get_character_extents (AtkText      *text,
                                                     gint         offset,
                                                     gint         *x,
                                                     gint         *y,
                                                     gint         *width,
                                                     gint         *height,
                                                     AtkCoordType coord_type)
{
  GtkIconViewItemAccessible *item;
#if 0
  GtkIconView *icon_view;
  PangoRectangle char_rect;
  const gchar *item_text;
  gint index;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

#if 0
  icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
  gtk_icon_view_update_item_text (icon_view, item->item);
  item_text = pango_layout_get_text (icon_view->priv->layout);
  index = g_utf8_offset_to_pointer (item_text, offset) - item_text;
  pango_layout_index_to_pos (icon_view->priv->layout, index, &char_rect);

  atk_component_get_position (ATK_COMPONENT (text), x, y, coord_type);
  *x += item->item->layout_x - item->item->x + char_rect.x / PANGO_SCALE;
  /* Look at gtk_icon_view_paint_item() to see where the text is. */
  *x -=  ((item->item->width - item->item->layout_width) / 2) + (MAX (item->item->pixbuf_width, icon_view->priv->item_width) - item->item->width) / 2,
  *y += item->item->layout_y - item->item->y + char_rect.y / PANGO_SCALE;
  *width = char_rect.width / PANGO_SCALE;
  *height = char_rect.height / PANGO_SCALE;
#endif
}

static gint
gtk_icon_view_item_accessible_get_offset_at_point (AtkText      *text,
                                                   gint          x,
                                                   gint          y,
                                                   AtkCoordType coord_type)
{
  GtkIconViewItemAccessible *item;
  gint offset = 0;
#if 0
  GtkIconView *icon_view;
  const gchar *item_text;
  gint index;
  gint l_x, l_y;
#endif

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (text);

  if (!GTK_IS_ICON_VIEW (item->widget))
    return -1;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return -1;

#if 0
  icon_view = GTK_ICON_VIEW (item->widget);
      /* FIXME we probably have to use GailTextCell to salvage this */
  gtk_icon_view_update_item_text (icon_view, item->item);
  atk_component_get_position (ATK_COMPONENT (text), &l_x, &l_y, coord_type);
  x -= l_x + item->item->layout_x - item->item->x;
  x +=  ((item->item->width - item->item->layout_width) / 2) + (MAX (item->item->pixbuf_width, icon_view->priv->item_width) - item->item->width) / 2,
  y -= l_y + item->item->layout_y - item->item->y;
  item_text = pango_layout_get_text (icon_view->priv->layout);
  if (!pango_layout_xy_to_index (icon_view->priv->layout, 
                                x * PANGO_SCALE,
                                y * PANGO_SCALE,
                                &index, NULL))
    {
      if (x < 0 || y < 0)
        index = 0;
      else
        index = -1;
    } 
  if (index == -1)
    offset = g_utf8_strlen (item_text, -1);
  else
    offset = g_utf8_pointer_to_offset (item_text, item_text + index);
#endif
  return offset;
}

static void
atk_text_item_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_icon_view_item_accessible_get_text;
  iface->get_character_at_offset = gtk_icon_view_item_accessible_get_character_at_offset;
  iface->get_text_before_offset = gtk_icon_view_item_accessible_get_text_before_offset;
  iface->get_text_at_offset = gtk_icon_view_item_accessible_get_text_at_offset;
  iface->get_text_after_offset = gtk_icon_view_item_accessible_get_text_after_offset;
  iface->get_character_count = gtk_icon_view_item_accessible_get_character_count;
  iface->get_character_extents = gtk_icon_view_item_accessible_get_character_extents;
  iface->get_offset_at_point = gtk_icon_view_item_accessible_get_offset_at_point;
}

static void
gtk_icon_view_item_accessible_get_extents (AtkComponent *component,
                                           gint         *x,
                                           gint         *y,
                                           gint         *width,
                                           gint         *height,
                                           AtkCoordType  coord_type)
{
  GtkIconViewItemAccessible *item;
  AtkObject *parent_obj;
  gint l_x, l_y;

  g_return_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (component));

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (component);
  if (!GTK_IS_WIDGET (item->widget))
    return;

  if (atk_state_set_contains_state (item->state_set, ATK_STATE_DEFUNCT))
    return;

  *width = item->item->cell_area.width;
  *height = item->item->cell_area.height;
  if (gtk_icon_view_item_accessible_is_showing (item))
    {
      parent_obj = gtk_widget_get_accessible (item->widget);
      atk_component_get_extents (ATK_COMPONENT (parent_obj), &l_x, &l_y,
                                 NULL, NULL, coord_type);
      *x = l_x + item->item->cell_area.x;
      *y = l_y + item->item->cell_area.y;
    }
  else
    {
      *x = G_MININT;
      *y = G_MININT;
    }
}

static gboolean
gtk_icon_view_item_accessible_grab_focus (AtkComponent *component)
{
  GtkIconViewItemAccessible *item;
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (component), FALSE);

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (component);
  if (!GTK_IS_WIDGET (item->widget))
    return FALSE;

  gtk_widget_grab_focus (item->widget);
  _gtk_icon_view_set_cursor_item (GTK_ICON_VIEW (item->widget), item->item, NULL);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (item->widget));
  if (gtk_widget_is_toplevel (toplevel))
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_window_present (GTK_WINDOW (toplevel));
      G_GNUC_END_IGNORE_DEPRECATIONS
    }

  return TRUE;
}

static void
atk_component_item_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_icon_view_item_accessible_get_extents;
  iface->grab_focus = gtk_icon_view_item_accessible_grab_focus;
}

static gboolean
gtk_icon_view_item_accessible_add_state (GtkIconViewItemAccessible *item,
                                         AtkStateType               state_type,
                                         gboolean                   emit_signal)
{
  gboolean rc;

  rc = atk_state_set_add_state (item->state_set, state_type);

  /* The signal should only be generated if the value changed,
   * not when the item is set up. So states that are set
   * initially should pass FALSE as the emit_signal argument.
   */
  if (emit_signal)
    {
      atk_object_notify_state_change (ATK_OBJECT (item), state_type, TRUE);
      /* If state_type is ATK_STATE_VISIBLE, additional notification */
      if (state_type == ATK_STATE_VISIBLE)
        g_signal_emit_by_name (item, "visible-data-changed");
    }

  return rc;
}

static gboolean
gtk_icon_view_item_accessible_remove_state (GtkIconViewItemAccessible *item,
                                            AtkStateType               state_type,
                                            gboolean                   emit_signal)
{
  if (atk_state_set_contains_state (item->state_set, state_type))
    {
      gboolean rc;

      rc = atk_state_set_remove_state (item->state_set, state_type);

      /* The signal should only be generated if the value changed,
       * not when the item is set up. So states that are set
       * initially should pass FALSE as the emit_signal argument.
       */
      if (emit_signal)
        {
          atk_object_notify_state_change (ATK_OBJECT (item), state_type, FALSE);
          /* If state_type is ATK_STATE_VISIBLE, additional notification */
          if (state_type == ATK_STATE_VISIBLE)
            g_signal_emit_by_name (item, "visible-data-changed");
        }

      return rc;
    }
  else
    return FALSE;
}

static gboolean
gtk_icon_view_item_accessible_is_showing (GtkIconViewItemAccessible *item)
{
  GtkAllocation allocation;
  GtkIconView *icon_view;
  GdkRectangle visible_rect;
  gboolean is_showing;

  /* An item is considered "SHOWING" if any part of the item
   * is in the visible rectangle.
   */
  if (!GTK_IS_ICON_VIEW (item->widget))
    return FALSE;

  if (item->item == NULL)
    return FALSE;

  gtk_widget_get_allocation (item->widget, &allocation);

  icon_view = GTK_ICON_VIEW (item->widget);
  visible_rect.x = 0;
  if (icon_view->priv->hadjustment)
    visible_rect.x += gtk_adjustment_get_value (icon_view->priv->hadjustment);
  visible_rect.y = 0;
  if (icon_view->priv->vadjustment)
    visible_rect.y += gtk_adjustment_get_value (icon_view->priv->vadjustment);
  visible_rect.width = allocation.width;
  visible_rect.height = allocation.height;

  if (((item->item->cell_area.x + item->item->cell_area.width) < visible_rect.x) ||
     ((item->item->cell_area.y + item->item->cell_area.height) < (visible_rect.y)) ||
     (item->item->cell_area.x > (visible_rect.x + visible_rect.width)) ||
     (item->item->cell_area.y > (visible_rect.y + visible_rect.height)))
    is_showing = FALSE;
  else
    is_showing = TRUE;

  return is_showing;
}

static gboolean
gtk_icon_view_item_accessible_set_visibility (GtkIconViewItemAccessible *item,
                                              gboolean                   emit_signal)
{
  if (gtk_icon_view_item_accessible_is_showing (item))
    return gtk_icon_view_item_accessible_add_state (item, ATK_STATE_SHOWING,
                                                    emit_signal);
  else
    return gtk_icon_view_item_accessible_remove_state (item, ATK_STATE_SHOWING,
                                                       emit_signal);
}

static void
_gtk_icon_view_item_accessible_init (GtkIconViewItemAccessible *item)
{
  item->state_set = atk_state_set_new ();

  atk_state_set_add_state (item->state_set, ATK_STATE_ENABLED);
  atk_state_set_add_state (item->state_set, ATK_STATE_FOCUSABLE);
  atk_state_set_add_state (item->state_set, ATK_STATE_SENSITIVE);
  atk_state_set_add_state (item->state_set, ATK_STATE_SELECTABLE);
  atk_state_set_add_state (item->state_set, ATK_STATE_VISIBLE);

  item->action_description = NULL;
  item->image_description = NULL;

  item->action_idle_handler = 0;
}

static void
_gtk_icon_view_item_accessible_finalize (GObject *object)
{
  GtkIconViewItemAccessible *item;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (object);

  if (item->widget)
    g_object_remove_weak_pointer (G_OBJECT (item->widget), (gpointer) &item->widget);

  if (item->state_set)
    g_object_unref (item->state_set);


  g_free (item->text);
  g_free (item->action_description);
  g_free (item->image_description);

  if (item->action_idle_handler)
    {
      g_source_remove (item->action_idle_handler);
      item->action_idle_handler = 0;
    }

  G_OBJECT_CLASS (_gtk_icon_view_item_accessible_parent_class)->finalize (object);
}

static AtkObject*
_gtk_icon_view_item_accessible_get_parent (AtkObject *obj)
{
  GtkIconViewItemAccessible *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (obj), NULL);
  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);

  if (item->widget)
    return gtk_widget_get_accessible (item->widget);
  else
    return NULL;
}

static gint
_gtk_icon_view_item_accessible_get_index_in_parent (AtkObject *obj)
{
  GtkIconViewItemAccessible *item;

  g_return_val_if_fail (GTK_IS_ICON_VIEW_ITEM_ACCESSIBLE (obj), 0);
  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);

  return item->item->index;
}

static AtkStateSet *
_gtk_icon_view_item_accessible_ref_state_set (AtkObject *obj)
{
  GtkIconViewItemAccessible *item;
  GtkIconView *icon_view;

  item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);
  g_return_val_if_fail (item->state_set, NULL);

  if (!item->widget)
    return NULL;

  icon_view = GTK_ICON_VIEW (item->widget);
  if (icon_view->priv->cursor_item == item->item)
    atk_state_set_add_state (item->state_set, ATK_STATE_FOCUSED);
  else
    atk_state_set_remove_state (item->state_set, ATK_STATE_FOCUSED);
  if (item->item->selected)
    atk_state_set_add_state (item->state_set, ATK_STATE_SELECTED);
  else
    atk_state_set_remove_state (item->state_set, ATK_STATE_SELECTED);

  return g_object_ref (item->state_set);
}

static void
_gtk_icon_view_item_accessible_class_init (GtkIconViewItemAccessibleClass *klass)
{
  GObjectClass *gobject_class;
  AtkObjectClass *atk_class;

  gobject_class = (GObjectClass *)klass;
  atk_class = (AtkObjectClass *)klass;

  gobject_class->finalize = _gtk_icon_view_item_accessible_finalize;

  atk_class->get_index_in_parent = _gtk_icon_view_item_accessible_get_index_in_parent;
  atk_class->get_parent = _gtk_icon_view_item_accessible_get_parent;
  atk_class->ref_state_set = _gtk_icon_view_item_accessible_ref_state_set;
}

static void atk_component_interface_init (AtkComponentIface *iface);
static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkIconViewAccessible, gtk_icon_view_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkIconViewAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

typedef struct
{
  AtkObject *item;
  gint       index;
} GtkIconViewItemAccessibleInfo;


static void
gtk_icon_view_item_accessible_info_new (AtkObject *accessible,
                                        AtkObject *item,
                                        gint       index)
{
  GtkIconViewAccessible *view = (GtkIconViewAccessible *)accessible;
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewItemAccessibleInfo *tmp_info;
  GList *items;

  info = g_new (GtkIconViewItemAccessibleInfo, 1);
  info->item = item;
  info->index = index;

  items = view->priv->items;
  while (items)
    {
      tmp_info = items->data;
      if (tmp_info->index > index)
        break;
      items = items->next;
    }
  view->priv->items = g_list_insert_before (view->priv->items, items, info);
}

static gint
gtk_icon_view_accessible_get_n_children (AtkObject *accessible)
{
  GtkIconView *icon_view;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (!widget)
      return 0;

  icon_view = GTK_ICON_VIEW (widget);

  return g_list_length (icon_view->priv->items);
}

static AtkObject *
gtk_icon_view_accessible_find_child (AtkObject *accessible,
                                     gint       index)
{
  GtkIconViewAccessible *view = (GtkIconViewAccessible*)accessible;
  GtkIconViewItemAccessibleInfo *info;
  GList *items;

  items = view->priv->items;

  while (items)
    {
      info = items->data;
      if (info->index == index)
        return info->item;
      items = items->next;
    }

  return NULL;
}

static AtkObject *
gtk_icon_view_accessible_ref_child (AtkObject *accessible,
                                    gint       index)
{
  GtkIconView *icon_view;
  GtkWidget *widget;
  GList *icons;
  AtkObject *obj;
  GtkIconViewItemAccessible *a11y_item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (!widget)
    return NULL;

  icon_view = GTK_ICON_VIEW (widget);
  icons = g_list_nth (icon_view->priv->items, index);
  obj = NULL;
  if (icons)
    {
      GtkIconViewItem *item = icons->data;

      g_return_val_if_fail (item->index == index, NULL);
      obj = gtk_icon_view_accessible_find_child (accessible, index);
      if (!obj)
        {
          obj = g_object_new (GTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE, NULL);
          gtk_icon_view_item_accessible_info_new (accessible, obj, index);
          obj->role = ATK_ROLE_ICON;
          a11y_item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (obj);
          a11y_item->item = item;
          a11y_item->widget = widget;

          g_free (a11y_item->text);
          a11y_item->text = get_text (icon_view, item);

          gtk_icon_view_item_accessible_set_visibility (a11y_item, FALSE);
          g_object_add_weak_pointer (G_OBJECT (widget), (gpointer) &(a11y_item->widget));
       }
      g_object_ref (obj);
    }
  return obj;
}

static void
gtk_icon_view_accessible_traverse_items (GtkIconViewAccessible *view,
                                         GList                 *list)
{
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewItemAccessible *item;
  GList *items;

  if (view->priv->items)
    {
      GtkWidget *widget;
      gboolean act_on_item;

      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (view));
      if (widget == NULL)
        return;

      items = view->priv->items;

      act_on_item = (list == NULL);

      while (items)
        {

          info = (GtkIconViewItemAccessibleInfo *)items->data;
          item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);

          if (act_on_item == FALSE && list == items)
            act_on_item = TRUE;

          if (act_on_item)
            gtk_icon_view_item_accessible_set_visibility (item, TRUE);

          items = items->next;
       }
   }
}

void
_gtk_icon_view_accessible_adjustment_changed (GtkIconView *icon_view)
{
  GtkIconViewAccessible *view;

  view = GTK_ICON_VIEW_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (icon_view)));
  if (view == NULL)
    return;

  gtk_icon_view_accessible_traverse_items (view, NULL);
}

static void
gtk_icon_view_accessible_model_row_changed (GtkTreeModel *tree_model,
                                            GtkTreePath  *path,
                                            GtkTreeIter  *iter,
                                            gpointer      user_data)
{
  AtkObject *atk_obj;
  gint index;
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  GtkIconViewItemAccessible *a11y_item;
  const gchar *name;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  index = gtk_tree_path_get_indices(path)[0];
  a11y_item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (
      gtk_icon_view_accessible_find_child (atk_obj, index));

  if (a11y_item)
    {
      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_obj));
      icon_view = GTK_ICON_VIEW (widget);
      item = a11y_item->item;

      name = atk_object_get_name (ATK_OBJECT (a11y_item));
      if (!name || strcmp (name, "") == 0)
        {
          g_free (a11y_item->text);
          a11y_item->text = get_text (icon_view, item);
        }
    }

  g_signal_emit_by_name (atk_obj, "visible-data-changed");

  return;
}

static void
gtk_icon_view_accessible_model_row_inserted (GtkTreeModel *tree_model,
                                             GtkTreePath  *path,
                                             GtkTreeIter  *iter,
                                             gpointer     user_data)
{
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewAccessible *view;
  GtkIconViewItemAccessible *item;
  GList *items;
  GList *tmp_list;
  AtkObject *atk_obj;
  gint index;

  index = gtk_tree_path_get_indices(path)[0];
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  view = GTK_ICON_VIEW_ACCESSIBLE (atk_obj);

  items = view->priv->items;
  tmp_list = NULL;
  while (items)
    {
      info = items->data;
      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);
      if (info->index != item->item->index)
        {
          if (info->index < index)
            g_warning ("Unexpected index value on insertion %d %d", index, info->index);

          if (tmp_list == NULL)
            tmp_list = items;

          info->index = item->item->index;
        }

      items = items->next;
    }
  gtk_icon_view_accessible_traverse_items (view, tmp_list);
  g_signal_emit_by_name (atk_obj, "children-changed::add",
                         index, NULL, NULL);
  return;
}

static void
gtk_icon_view_accessible_model_row_deleted (GtkTreeModel *tree_model,
                                            GtkTreePath  *path,
                                            gpointer     user_data)
{
  GtkIconViewItemAccessibleInfo *info;
  GtkIconViewAccessible *view;
  GtkIconViewItemAccessible *item;
  GList *items;
  GList *tmp_list;
  GList *deleted_item;
  AtkObject *atk_obj;
  gint index;

  index = gtk_tree_path_get_indices(path)[0];
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  view = GTK_ICON_VIEW_ACCESSIBLE (atk_obj);

  items = view->priv->items;
  tmp_list = NULL;
  deleted_item = NULL;
  info = NULL;
  while (items)
    {
      info = items->data;
      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);
      if (info->index == index)
        {
          deleted_item = items;
        }
      else if (info->index != item->item->index)
        {
          if (tmp_list == NULL)
            tmp_list = items;

          info->index = item->item->index;
        }

      items = items->next;
    }
  if (deleted_item)
    {
      info = deleted_item->data;
      gtk_icon_view_item_accessible_add_state (GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item), ATK_STATE_DEFUNCT, TRUE);
      g_signal_emit_by_name (atk_obj, "children-changed::remove",
                             index, NULL, NULL);
      view->priv->items = g_list_delete_link (view->priv->items, deleted_item);
      g_object_unref (info->item);
      g_free (info);
    }
  gtk_icon_view_accessible_traverse_items (view, tmp_list);

  return;
}

static gint
gtk_icon_view_accessible_item_compare (GtkIconViewItemAccessibleInfo *i1,
                                       GtkIconViewItemAccessibleInfo *i2)
{
  return i1->index - i2->index;
}

static void
gtk_icon_view_accessible_model_rows_reordered (GtkTreeModel *tree_model,
                                               GtkTreePath  *path,
                                               GtkTreeIter  *iter,
                                               gint         *new_order,
                                               gpointer     user_data)
{
  GtkIconViewAccessible *view;
  GtkIconViewItemAccessibleInfo *info;
  GtkIconView *icon_view;
  GtkIconViewItemAccessible *item;
  GList *items;
  AtkObject *atk_obj;
  gint *order;
  gint length, i;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (user_data));
  icon_view = GTK_ICON_VIEW (user_data);
  view = (GtkIconViewAccessible*)atk_obj;

  length = gtk_tree_model_iter_n_children (tree_model, NULL);

  order = g_new (gint, length);
  for (i = 0; i < length; i++)
    order [new_order[i]] = i;

  items = view->priv->items;
  while (items)
    {
      info = items->data;
      item = GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item);
      info->index = order[info->index];
      item->item = g_list_nth_data (icon_view->priv->items, info->index);
      items = items->next;
    }
  g_free (order);
  view->priv->items = g_list_sort (view->priv->items,
                                   (GCompareFunc)gtk_icon_view_accessible_item_compare);

  return;
}

static void
gtk_icon_view_accessible_disconnect_model_signals (GtkTreeModel *model,
                                                   GtkWidget *widget)
{
  GObject *obj;

  obj = G_OBJECT (model);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_row_changed, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_row_inserted, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_row_deleted, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) gtk_icon_view_accessible_model_rows_reordered, widget);
}

static void
gtk_icon_view_accessible_connect_model_signals (GtkIconView *icon_view)
{
  GObject *obj;

  obj = G_OBJECT (icon_view->priv->model);
  g_signal_connect_object (obj, "row-changed",
                           G_CALLBACK (gtk_icon_view_accessible_model_row_changed),
                           icon_view, 0);
  g_signal_connect_object (obj, "row-inserted",
                           G_CALLBACK (gtk_icon_view_accessible_model_row_inserted),
                           icon_view, G_CONNECT_AFTER);
  g_signal_connect_object (obj, "row-deleted",
                           G_CALLBACK (gtk_icon_view_accessible_model_row_deleted),
                           icon_view, G_CONNECT_AFTER);
  g_signal_connect_object (obj, "rows-reordered",
                           G_CALLBACK (gtk_icon_view_accessible_model_rows_reordered),
                           icon_view, G_CONNECT_AFTER);
}

static void
gtk_icon_view_accessible_clear_cache (GtkIconViewAccessible *view)
{
  GtkIconViewItemAccessibleInfo *info;
  GList *items;

  items = view->priv->items;
  while (items)
    {
      info = (GtkIconViewItemAccessibleInfo *) items->data;
      gtk_icon_view_item_accessible_add_state (GTK_ICON_VIEW_ITEM_ACCESSIBLE (info->item), ATK_STATE_DEFUNCT, TRUE);
      g_object_unref (info->item);
      g_free (items->data);
      items = items->next;
    }
  g_list_free (view->priv->items);
  view->priv->items = NULL;
}

static void
gtk_icon_view_accessible_notify_gtk (GObject    *obj,
                                     GParamSpec *pspec)
{
  GtkIconView *icon_view;
  GtkWidget *widget;
  AtkObject *atk_obj;
  GtkIconViewAccessible *view;

  if (strcmp (pspec->name, "model") == 0)
    {
      widget = GTK_WIDGET (obj);
      atk_obj = gtk_widget_get_accessible (widget);
      view = (GtkIconViewAccessible*)atk_obj;
      if (view->priv->model)
        {
          g_object_remove_weak_pointer (G_OBJECT (view->priv->model),
                                        (gpointer *)&view->priv->model);
          gtk_icon_view_accessible_disconnect_model_signals (view->priv->model, widget);
        }
      gtk_icon_view_accessible_clear_cache (view);

      icon_view = GTK_ICON_VIEW (obj);
      view->priv->model = icon_view->priv->model;
      /* If there is no model the GtkIconView is probably being destroyed */
      if (view->priv->model)
        {
          g_object_add_weak_pointer (G_OBJECT (view->priv->model), (gpointer *)&view->priv->model);
          gtk_icon_view_accessible_connect_model_signals (icon_view);
        }
    }

  return;
}

static void
gtk_icon_view_accessible_initialize (AtkObject *accessible,
                                     gpointer   data)
{
  GtkIconViewAccessible *view;
  GtkIconView *icon_view;

  if (ATK_OBJECT_CLASS (gtk_icon_view_accessible_parent_class)->initialize)
    ATK_OBJECT_CLASS (gtk_icon_view_accessible_parent_class)->initialize (accessible, data);

  icon_view = (GtkIconView*)data;
  view = (GtkIconViewAccessible*)accessible;

  g_signal_connect (data, "notify",
                    G_CALLBACK (gtk_icon_view_accessible_notify_gtk), NULL);

  view->priv->model = icon_view->priv->model;
  if (view->priv->model)
    {
      g_object_add_weak_pointer (G_OBJECT (view->priv->model), (gpointer *)&view->priv->model);
      gtk_icon_view_accessible_connect_model_signals (icon_view);
    }

  accessible->role = ATK_ROLE_LAYERED_PANE;
}

static void
gtk_icon_view_accessible_finalize (GObject *object)
{
  GtkIconViewAccessible *view = (GtkIconViewAccessible*)object;

  gtk_icon_view_accessible_clear_cache (view);

  G_OBJECT_CLASS (gtk_icon_view_accessible_parent_class)->finalize (object);
}

static void
gtk_icon_view_accessible_class_init (GtkIconViewAccessibleClass *klass)
{
  GObjectClass *gobject_class;
  AtkObjectClass *atk_class;

  gobject_class = (GObjectClass *)klass;
  atk_class = (AtkObjectClass *)klass;

  gobject_class->finalize = gtk_icon_view_accessible_finalize;

  atk_class->get_n_children = gtk_icon_view_accessible_get_n_children;
  atk_class->ref_child = gtk_icon_view_accessible_ref_child;
  atk_class->initialize = gtk_icon_view_accessible_initialize;
}

static void
gtk_icon_view_accessible_init (GtkIconViewAccessible *accessible)
{
  accessible->priv = gtk_icon_view_accessible_get_instance_private (accessible);
}

static AtkObject*
gtk_icon_view_accessible_ref_accessible_at_point (AtkComponent *component,
                                                  gint          x,
                                                  gint          y,
                                                  AtkCoordType  coord_type)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  gint x_pos, y_pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return NULL;

  icon_view = GTK_ICON_VIEW (widget);
  atk_component_get_extents (component, &x_pos, &y_pos, NULL, NULL, coord_type);
  item = _gtk_icon_view_get_item_at_coords (icon_view, x - x_pos, y - y_pos, TRUE, NULL);
  if (item)
    return gtk_icon_view_accessible_ref_child (ATK_OBJECT (component), item->index);

  return NULL;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->ref_accessible_at_point = gtk_icon_view_accessible_ref_accessible_at_point;
}

static gboolean
gtk_icon_view_accessible_add_selection (AtkSelection *selection,
                                        gint          i)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);

  item = g_list_nth_data (icon_view->priv->items, i);
  if (!item)
    return FALSE;

  _gtk_icon_view_select_item (icon_view, item);

  return TRUE;
}

static gboolean
gtk_icon_view_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkIconView *icon_view;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);
  gtk_icon_view_unselect_all (icon_view);

  return TRUE;
}

static AtkObject*
gtk_icon_view_accessible_ref_selection (AtkSelection *selection,
                                        gint          i)
{
  GList *l;
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  icon_view = GTK_ICON_VIEW (widget);

  l = icon_view->priv->items;
  while (l)
    {
      item = l->data;
      if (item->selected)
        {
          if (i == 0)
            return atk_object_ref_accessible_child (gtk_widget_get_accessible (widget), item->index);
          else
            i--;
        }
      l = l->next;
    }

  return NULL;
}

static gint
gtk_icon_view_accessible_get_selection_count (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  GList *l;
  gint count;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  icon_view = GTK_ICON_VIEW (widget);

  l = icon_view->priv->items;
  count = 0;
  while (l)
    {
      item = l->data;

      if (item->selected)
        count++;

      l = l->next;
    }

  return count;
}

static gboolean
gtk_icon_view_accessible_is_child_selected (AtkSelection *selection,
                                            gint          i)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);

  item = g_list_nth_data (icon_view->priv->items, i);
  if (!item)
    return FALSE;

  return item->selected;
}

static gboolean
gtk_icon_view_accessible_remove_selection (AtkSelection *selection,
                                           gint          i)
{
  GtkWidget *widget;
  GtkIconView *icon_view;
  GtkIconViewItem *item;
  GList *l;
  gint count;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);
  l = icon_view->priv->items;
  count = 0;
  while (l)
    {
      item = l->data;
      if (item->selected)
        {
          if (count == i)
            {
              _gtk_icon_view_unselect_item (icon_view, item);
              return TRUE;
            }
          count++;
        }
      l = l->next;
    }

  return FALSE;
}
 
static gboolean
gtk_icon_view_accessible_select_all_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkIconView *icon_view;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  icon_view = GTK_ICON_VIEW (widget);
  gtk_icon_view_select_all (icon_view);
  return TRUE;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_icon_view_accessible_add_selection;
  iface->clear_selection = gtk_icon_view_accessible_clear_selection;
  iface->ref_selection = gtk_icon_view_accessible_ref_selection;
  iface->get_selection_count = gtk_icon_view_accessible_get_selection_count;
  iface->is_child_selected = gtk_icon_view_accessible_is_child_selected;
  iface->remove_selection = gtk_icon_view_accessible_remove_selection;
  iface->select_all_selection = gtk_icon_view_accessible_select_all_selection;
}
