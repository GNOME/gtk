/* GtkToolPalette -- A tool palette with categories and DnD support
 * Copyright (C) 2008  Openismus GmbH
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 */

#include "gtktoolpaletteprivate.h"
#include "gtkmarshalers.h"

#include <gtk/gtk.h>
#include <string.h>

#define DEFAULT_ICON_SIZE       GTK_ICON_SIZE_SMALL_TOOLBAR
#define DEFAULT_ORIENTATION     GTK_ORIENTATION_VERTICAL
#define DEFAULT_TOOLBAR_STYLE   GTK_TOOLBAR_ICONS

#define DEFAULT_CHILD_EXCLUSIVE FALSE
#define DEFAULT_CHILD_EXPAND    FALSE

#define P_(msgid) (msgid)

/**
 * SECTION:GtkToolPalette
 * @short_description: A tool palette with categories
 * @include: gtktoolpalette.h
 *
 * An #GtkToolPalette allows it to add #GtkToolItem<!-- -->s to a palette like container
 * with different categories and drag and drop support.
 *
 * An #GtkToolPalette is created with a call to gtk_tool_palette_new().
 *
 * #GtkToolItem<!-- -->s cannot be added directly to an #GtkToolPalette, instead they
 * are added to an #GtkToolItemGroup which can than be added to an #GtkToolPalette. To add
 * an #GtkToolItemGroup to an #GtkToolPalette use gtk_container_add().
 *
 * |[
 * GtkWidget *palette, *group;
 * GtkToolItem *item;
 *
 * palette = gtk_tool_palette_new ();
 * group = gtk_tool_item_group_new (_("Test Category"));
 * gtk_container_add (GTK_CONTAINER (palette), group);
 *
 * item = gtk_tool_button_new_from_stock (GTK_STOCK_OK);
 * gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
 * ]|
 *
 * The easiest way to use drag and drop with GtkToolPalette is to call gtk_tool_palette_add_drag_dest()
 * with the desired drag source @palette and the desired drag target @widget. Than gtk_tool_palette_get_drag_item()
 * can be used to get the dragged item in the #GtkWidget::drag-data-received signal handler of the drag target.
 *
 * |[
 * static void
 * passive_canvas_drag_data_received (GtkWidget        *widget,
 *                                    GdkDragContext   *context,
 *                                    gint              x,
 *                                    gint              y,
 *                                    GtkSelectionData *selection,
 *                                    guint             info,
 *                                    guint             time,
 *                                    gpointer          data)
 * {
 *   GtkWidget *palette;
 *   GtkWidget *item;
 *
 *   /<!-- -->* Get the dragged item *<!-- -->/
 *   palette = gtk_widget_get_ancestor (gtk_drag_get_source_widget (context), GTK_TYPE_TOOL_PALETTE);
 *   if (palette != NULL)
 *     item = gtk_tool_palette_get_drag_item (GTK_TOOL_PALETTE (palette), selection);
 *
 *   /<!-- -->* Do something with item *<!-- -->/
 * }
 *
 * GtkWidget *target, palette;
 *
 * palette = gtk_tool_palette_new ();
 * target = gtk_drawing_area_new ();
 *
 * g_signal_connect (G_OBJECT (target), "drag-data-received",
 *                   G_CALLBACK (passive_canvas_drag_data_received), NULL);   
 * gtk_tool_palette_add_drag_dest (GTK_TOOL_PALETTE (palette), target,
 *                                 GTK_DEST_DEFAULT_ALL,
 *                                 GTK_TOOL_PALETTE_DRAG_ITEMS,
 *                                 GDK_ACTION_COPY);
 * ]|
 *
 * Since: 2.18
 */

typedef struct _GtkToolItemGroupInfo   GtkToolItemGroupInfo;
typedef struct _GtkToolPaletteDragData GtkToolPaletteDragData;

enum
{
  PROP_NONE,
  PROP_ICON_SIZE,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
};

enum
{
  CHILD_PROP_NONE,
  CHILD_PROP_EXCLUSIVE,
  CHILD_PROP_EXPAND,
};

struct _GtkToolItemGroupInfo
{
  GtkToolItemGroup *widget;

  guint             notify_collapsed;
  guint             exclusive : 1;
  guint             expand : 1;
};

struct _GtkToolPalettePrivate
{
  GtkToolItemGroupInfo *groups;
  gsize                 groups_size;
  gsize                 groups_length;

  GtkAdjustment        *hadjustment;
  GtkAdjustment        *vadjustment;

  GtkIconSize           icon_size;
  GtkOrientation        orientation;
  GtkToolbarStyle       style;

  GtkWidget            *expanding_child;

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  GtkSizeGroup         *text_size_group;
#endif

  guint                 sparse_groups : 1;
  guint                 drag_source : 2;
};

struct _GtkToolPaletteDragData
{
  GtkToolPalette *palette;
  GtkWidget      *item;
};

static GdkAtom dnd_target_atom_item = GDK_NONE;
static GdkAtom dnd_target_atom_group = GDK_NONE;

static const GtkTargetEntry dnd_targets[] =
{
  { "application/x-GTK-tool-palette-item", GTK_TARGET_SAME_APP, 0 },
  { "application/x-GTK-tool-palette-group", GTK_TARGET_SAME_APP, 0 },
};

G_DEFINE_TYPE (GtkToolPalette,
               gtk_tool_palette,
               GTK_TYPE_CONTAINER);

static void
gtk_tool_palette_init (GtkToolPalette *palette)
{
  palette->priv = G_TYPE_INSTANCE_GET_PRIVATE (palette,
                                               GTK_TYPE_TOOL_PALETTE,
                                               GtkToolPalettePrivate);

  palette->priv->groups_size = 4;
  palette->priv->groups_length = 0;
  palette->priv->groups = g_new0 (GtkToolItemGroupInfo, palette->priv->groups_size);

  palette->priv->icon_size = DEFAULT_ICON_SIZE;
  palette->priv->orientation = DEFAULT_ORIENTATION;
  palette->priv->style = DEFAULT_TOOLBAR_STYLE;

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  palette->priv->text_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
#endif
}

static void
gtk_tool_palette_reconfigured (GtkToolPalette *palette)
{
  guint i;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      if (palette->priv->groups[i].widget)
        _gtk_tool_item_group_palette_reconfigured (palette->priv->groups[i].widget);
    }

  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (palette));
}

static void
gtk_tool_palette_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        if ((guint) g_value_get_enum (value) != palette->priv->icon_size)
          {
            palette->priv->icon_size = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      case PROP_ORIENTATION:
        if ((guint) g_value_get_enum (value) != palette->priv->orientation)
          {
            palette->priv->orientation = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      case PROP_TOOLBAR_STYLE:
        if ((guint) g_value_get_enum (value) != palette->priv->style)
          {
            palette->priv->style = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        g_value_set_enum (value, gtk_tool_palette_get_icon_size (palette));
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, gtk_tool_palette_get_orientation (palette));
        break;

      case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, gtk_tool_palette_get_style (palette));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_dispose (GObject *object)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);
  guint i;

  if (palette->priv->hadjustment)
    {
      g_object_unref (palette->priv->hadjustment);
      palette->priv->hadjustment = NULL;
    }

  if (palette->priv->vadjustment)
    {
      g_object_unref (palette->priv->vadjustment);
      palette->priv->vadjustment = NULL;
    }

  for (i = 0; i < palette->priv->groups_size; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (group->notify_collapsed)
        {
          g_signal_handler_disconnect (group->widget, group->notify_collapsed);
          group->notify_collapsed = 0;
        }
    }

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  if (palette->priv->text_size_group)
    {
      g_object_unref (palette->priv->text_size_group);
      palette->priv->text_size_group = NULL;
    }
#endif

  G_OBJECT_CLASS (gtk_tool_palette_parent_class)->dispose (object);
}

static void
gtk_tool_palette_finalize (GObject *object)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  if (palette->priv->groups)
    {
      palette->priv->groups_length = 0;
      g_free (palette->priv->groups);
      palette->priv->groups = NULL;
    }

  G_OBJECT_CLASS (gtk_tool_palette_parent_class)->finalize (object);
}

static void
gtk_tool_palette_size_request (GtkWidget      *widget,
                               GtkRequisition *requisition)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GtkRequisition child_requisition;
  guint i;

  requisition->width = 0;
  requisition->height = 0;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (!group->widget)
        continue;

      gtk_widget_size_request (GTK_WIDGET (group->widget), &child_requisition);

      if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
        {
          requisition->width = MAX (requisition->width, child_requisition.width);
          requisition->height += child_requisition.height;
        }
      else
        {
          requisition->width += child_requisition.width;
          requisition->height = MAX (requisition->height, child_requisition.height);
        }
    }

  requisition->width += border_width * 2;
  requisition->height += border_width * 2;
}

static void
gtk_tool_palette_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GtkAdjustment *adjustment = NULL;
  GtkAllocation child_allocation;

  gint n_expand_groups = 0;
  gint remaining_space = 0;
  gint expand_space = 0;

  gint page_start, page_size = 0;
  gint offset = 0;
  guint i;

  gint min_offset = -1, max_offset = -1;

  gint x;

  gint *group_sizes = g_newa(gint, palette->priv->groups_length);

  GtkTextDirection direction = gtk_widget_get_direction (widget);

  GTK_WIDGET_CLASS (gtk_tool_palette_parent_class)->size_allocate (widget, allocation);

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      adjustment = palette->priv->vadjustment;
      page_size = allocation->height;
    }
  else
    {
      adjustment = palette->priv->hadjustment;
      page_size = allocation->width;
    }

  if (adjustment)
    offset = gtk_adjustment_get_value (adjustment);
  if (GTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
      GTK_TEXT_DIR_RTL == direction)
    offset = -offset;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.width = allocation->width - border_width * 2;
  else
    child_allocation.height = allocation->height - border_width * 2;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    remaining_space = allocation->height;
  else
    remaining_space = allocation->width;

  /* figure out the required size of all groups to be able to distribute the
   * remaining space on allocation */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      gint size;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      if (gtk_tool_item_group_get_n_items (group->widget))
        {
          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            size = _gtk_tool_item_group_get_height_for_width (group->widget, child_allocation.width);
          else
            size = _gtk_tool_item_group_get_width_for_height (group->widget, child_allocation.height);

          if (group->expand && !gtk_tool_item_group_get_collapsed (group->widget))
            n_expand_groups += 1;
        }
      else
        size = 0;

      remaining_space -= size;
      group_sizes[i] = size;

      /* if the widget is currently expanding an offset which allows to display as much of the
       * widget as possible is calculated */
      if (widget == palette->priv->expanding_child)
        {
          gint limit =
            GTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
            child_allocation.width : child_allocation.height;

          gint real_size;
          guint j;

          min_offset = 0;

          for (j = 0; j < i; ++j)
            min_offset += group_sizes[j];

          max_offset = min_offset + group_sizes[i];

          real_size = _gtk_tool_item_group_get_size_for_limit
            (GTK_TOOL_ITEM_GROUP (widget), limit,
             GTK_ORIENTATION_VERTICAL == palette->priv->orientation,
             FALSE);

          if (size == real_size)
            palette->priv->expanding_child = NULL;
        }
    }

  if (n_expand_groups > 0)
    {
      remaining_space = MAX (0, remaining_space);
      expand_space = remaining_space / n_expand_groups;
    }

  if (max_offset != -1)
    {
      gint limit =
        GTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
        allocation->height : allocation->width;

      offset = MIN (MAX (offset, max_offset - limit), min_offset);
    }

  if (remaining_space > 0)
    offset = 0;

  x = border_width;
  child_allocation.y = border_width;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.y -= offset;
  else
    x -= offset;

  /* allocate all groups at the calculated positions */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      GtkWidget *widget;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      if (gtk_tool_item_group_get_n_items (group->widget))
        {
          gint size = group_sizes[i];

          if (group->expand && !gtk_tool_item_group_get_collapsed (group->widget))
            {
              size += MIN (expand_space, remaining_space);
              remaining_space -= expand_space;
            }

          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.height = size;
          else
            child_allocation.width = size;

          if (GTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
              GTK_TEXT_DIR_RTL == direction)
            child_allocation.x = allocation->width - x - child_allocation.width;
          else
            child_allocation.x = x;

          gtk_widget_size_allocate (widget, &child_allocation);
          gtk_widget_show (widget);

          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.y += child_allocation.height;
          else
            x += child_allocation.width;
        }
      else
        gtk_widget_hide (widget);
    }

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      child_allocation.y += border_width;
      child_allocation.y += offset;

      page_start = child_allocation.y;
    }
  else
    {
      x += border_width;
      x += offset;

      page_start = x;
    }

  /* update the scrollbar to match the displayed adjustment */
  if (adjustment)
    {
      gdouble value;

      adjustment->page_increment = page_size * 0.9;
      adjustment->step_increment = page_size * 0.1;
      adjustment->page_size = page_size;

      if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation ||
          GTK_TEXT_DIR_LTR == direction)
        {
          adjustment->lower = 0;
          adjustment->upper = MAX (0, page_start);

          value = MIN (offset, adjustment->upper - adjustment->page_size);
          gtk_adjustment_clamp_page (adjustment, value, offset + page_size);
        }
      else
        {
          adjustment->lower = page_size - MAX (0, page_start);
          adjustment->upper = page_size;

          offset = -offset;

          value = MAX (offset, adjustment->lower);
          gtk_adjustment_clamp_page (adjustment, offset, value + page_size);
        }

      gtk_adjustment_changed (adjustment);
    }
}

static gboolean
gtk_tool_palette_expose_event (GtkWidget      *widget,
                               GdkEventExpose *event)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GdkDisplay *display;
  cairo_t *cr;
  guint i;

  display = gdk_drawable_get_display (widget->window);

  if (!gdk_display_supports_composite (display))
    return FALSE;

  cr = gdk_cairo_create (widget->window);
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  cairo_push_group (cr);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if (palette->priv->groups[i].widget)
      _gtk_tool_item_group_paint (palette->priv->groups[i].widget, cr);

  cairo_pop_group_to_source (cr);

  cairo_paint (cr);
  cairo_destroy (cr);

  return FALSE;
}

static void
gtk_tool_palette_realize (GtkWidget *widget)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  GdkWindowAttr attributes;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK
                        | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                        | GDK_BUTTON_MOTION_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, attributes_mask);

  gdk_window_set_user_data (widget->window, widget);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  gtk_container_forall (GTK_CONTAINER (widget),
                        (GtkCallback) gtk_widget_set_parent_window,
                        widget->window);

  gtk_widget_queue_resize_no_redraw (widget);
}

static void
gtk_tool_palette_adjustment_value_changed (GtkAdjustment *adjustment G_GNUC_UNUSED,
                                           gpointer       data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  gtk_tool_palette_size_allocate (widget, &widget->allocation);
}

static void
gtk_tool_palette_set_scroll_adjustments (GtkWidget     *widget,
                                         GtkAdjustment *hadjustment,
                                         GtkAdjustment *vadjustment)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);

  if (palette->priv->hadjustment)
    g_object_unref (palette->priv->hadjustment);
  if (palette->priv->vadjustment)
    g_object_unref (palette->priv->vadjustment);

  if (hadjustment)
    g_object_ref_sink (hadjustment);
  if (vadjustment)
    g_object_ref_sink (vadjustment);

  palette->priv->hadjustment = hadjustment;
  palette->priv->vadjustment = vadjustment;

  if (palette->priv->hadjustment)
    g_signal_connect (palette->priv->hadjustment, "value-changed",
                      G_CALLBACK (gtk_tool_palette_adjustment_value_changed),
                      palette);
  if (palette->priv->vadjustment)
    g_signal_connect (palette->priv->vadjustment, "value-changed",
                      G_CALLBACK (gtk_tool_palette_adjustment_value_changed),
                      palette);
}

static void
gtk_tool_palette_repack (GtkToolPalette *palette)
{
  guint si, di;

  for (si = di = 0; di < palette->priv->groups_length; ++si)
    {
      if (palette->priv->groups[si].widget)
        {
          palette->priv->groups[di] = palette->priv->groups[si];
          ++di;
        }
      else
        --palette->priv->groups_length;
    }

  palette->priv->sparse_groups = FALSE;
}

static void
gtk_tool_palette_add (GtkContainer *container,
                      GtkWidget    *child)
{
  GtkToolPalette *palette;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  palette = GTK_TOOL_PALETTE (container);

  if (palette->priv->groups_length == palette->priv->groups_size)
    gtk_tool_palette_repack (palette);

  if (palette->priv->groups_length == palette->priv->groups_size)
    {
      gsize old_size = palette->priv->groups_size;
      gsize new_size = old_size * 2;

      palette->priv->groups = g_renew (GtkToolItemGroupInfo,
                                       palette->priv->groups,
                                       new_size);

      memset (palette->priv->groups + old_size, 0,
              sizeof (GtkToolItemGroupInfo) * old_size);

      palette->priv->groups_size = new_size;
    }

  palette->priv->groups[palette->priv->groups_length].widget = g_object_ref_sink (child);
  palette->priv->groups_length += 1;

  gtk_widget_set_parent (child, GTK_WIDGET (palette));
}

static void
gtk_tool_palette_remove (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkToolPalette *palette;
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  palette = GTK_TOOL_PALETTE (container);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if ((GtkWidget*) palette->priv->groups[i].widget == child)
      {
        g_object_unref (child);
        gtk_widget_unparent (child);

        memset (&palette->priv->groups[i], 0, sizeof (GtkToolItemGroupInfo));
        palette->priv->sparse_groups = TRUE;
      }
}

static void
gtk_tool_palette_forall (GtkContainer *container,
                         gboolean      internals G_GNUC_UNUSED,
                         GtkCallback   callback,
                         gpointer      callback_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);
  guint i;

  if (palette->priv->groups)
    {
      for (i = 0; i < palette->priv->groups_length; ++i)
        if (palette->priv->groups[i].widget)
          callback (GTK_WIDGET (palette->priv->groups[i].widget),
                    callback_data);
    }
}

static GType
gtk_tool_palette_child_type (GtkContainer *container G_GNUC_UNUSED)
{
  return GTK_TYPE_TOOL_ITEM_GROUP;
}

static void
gtk_tool_palette_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        gtk_tool_palette_set_exclusive (palette, child, g_value_get_boolean (value));
        break;

      case CHILD_PROP_EXPAND:
        gtk_tool_palette_set_expand (palette, child, g_value_get_boolean (value));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        g_value_set_boolean (value, gtk_tool_palette_get_exclusive (palette, child));
        break;

      case CHILD_PROP_EXPAND:
        g_value_set_boolean (value, gtk_tool_palette_get_expand (palette, child));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_class_init (GtkToolPaletteClass *cls)
{
  GObjectClass      *oclass   = G_OBJECT_CLASS (cls);
  GtkWidgetClass    *wclass   = GTK_WIDGET_CLASS (cls);
  GtkContainerClass *cclass   = GTK_CONTAINER_CLASS (cls);

  oclass->set_property        = gtk_tool_palette_set_property;
  oclass->get_property        = gtk_tool_palette_get_property;
  oclass->dispose             = gtk_tool_palette_dispose;
  oclass->finalize            = gtk_tool_palette_finalize;

  wclass->size_request        = gtk_tool_palette_size_request;
  wclass->size_allocate       = gtk_tool_palette_size_allocate;
  wclass->expose_event        = gtk_tool_palette_expose_event;
  wclass->realize             = gtk_tool_palette_realize;

  cclass->add                 = gtk_tool_palette_add;
  cclass->remove              = gtk_tool_palette_remove;
  cclass->forall              = gtk_tool_palette_forall;
  cclass->child_type          = gtk_tool_palette_child_type;
  cclass->set_child_property  = gtk_tool_palette_set_child_property;
  cclass->get_child_property  = gtk_tool_palette_get_child_property;

  cls->set_scroll_adjustments = gtk_tool_palette_set_scroll_adjustments;
  /**
   * GtkToolPalette::set-scroll-adjustments:
   * @widget: the GtkToolPalette that received the signal
   * @hadjustment: The horizontal adjustment 
   * @vadjustment: The vertical adjustment
   *
   * The ::set-scroll-adjustments when FIXME
   *
   * Since: 2.18
   */
  wclass->set_scroll_adjustments_signal =
    g_signal_new ("set-scroll-adjustments",
                  G_TYPE_FROM_CLASS (oclass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkToolPaletteClass, set_scroll_adjustments),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ADJUSTMENT,
                  GTK_TYPE_ADJUSTMENT);

  g_object_class_install_property (oclass, PROP_ICON_SIZE,
                                   g_param_spec_enum ("icon-size",
                                                      P_("Icon Size"),
                                                      P_("The size of palette icons"),
                                                      GTK_TYPE_ICON_SIZE,
                                                      DEFAULT_ICON_SIZE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      P_("Orientation"),
                                                      P_("Orientation of the tool palette"),
                                                      GTK_TYPE_ORIENTATION,
                                                      DEFAULT_ORIENTATION,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_TOOLBAR_STYLE,
                                   g_param_spec_enum ("toolbar-style",
                                                      P_("Toolbar Style"),
                                                      P_("Style of items in the tool palette"),
                                                      GTK_TYPE_TOOLBAR_STYLE,
                                                      DEFAULT_TOOLBAR_STYLE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXCLUSIVE,
                                              g_param_spec_boolean ("exclusive",
                                                                    P_("Exclusive"),
                                                                    P_("Whether the item group should be the only expanded at a given time"),
                                                                    DEFAULT_CHILD_EXCLUSIVE,
                                                                    G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                                    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXPAND,
                                              g_param_spec_boolean ("expand",
                                                                    P_("Expand"),
                                                                    P_("Whether the item group should receive extra space when the palette grows"),
                                                                    DEFAULT_CHILD_EXPAND,
                                                                    G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                                    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (GtkToolPalettePrivate));

  dnd_target_atom_item = gdk_atom_intern_static_string (dnd_targets[0].target);
  dnd_target_atom_group = gdk_atom_intern_static_string (dnd_targets[1].target);
}

/**
 * gtk_tool_palette_new:
 *
 * Creates a new tool palette.
 *
 * Returns: a new #GtkToolPalette.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_new (void)
{
  return g_object_new (GTK_TYPE_TOOL_PALETTE, NULL);
}

/**
 * gtk_tool_palette_set_icon_size:
 * @palette: an #GtkToolPalette.
 * @icon_size: the #GtkIconSize that icons in the tool palette shall have.
 *
 * Sets the size of icons in the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_icon_size (GtkToolPalette *palette,
                                GtkIconSize     icon_size)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (icon_size != palette->priv->icon_size)
    g_object_set (palette, "icon-size", icon_size, NULL);
}

/**
 * gtk_tool_palette_set_orientation:
 * @palette: an #GtkToolPalette.
 * @orientation: the #GtkOrientation that the tool palette shall have.
 *
 * Sets the orientation (horizontal or vertical) of the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_orientation (GtkToolPalette *palette,
                                  GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (orientation != palette->priv->orientation)
    g_object_set (palette, "orientation", orientation, NULL);
}

/**
 * gtk_tool_palette_set_style:
 * @palette: an #GtkToolPalette.
 * @style: the #GtkToolbarStyle that items in the tool palette shall have.
 *
 * Sets the style (text, icons or both) of items in the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_style (GtkToolPalette  *palette,
                            GtkToolbarStyle  style)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (style != palette->priv->style)
    g_object_set (palette, "toolbar-style", style, NULL);
}

/**
 * gtk_tool_palette_get_icon_size:
 * @palette: an #GtkToolPalette.
 *
 * Gets the size of icons in the tool palette. See gtk_tool_palette_set_icon_size().
 * 
 * Returns: the #GtkIconSize of icons in the tool palette.
 *
 * Since: 2.18
 */
GtkIconSize
gtk_tool_palette_get_icon_size (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_ICON_SIZE);
  return palette->priv->icon_size;
}

/**
 * gtk_tool_palette_get_orientation:
 * @palette: an #GtkToolPalette.
 *
 * Gets the orientation (horizontal or vertical) of the tool palette. See gtk_tool_palette_set_orientation().
 *
 * Returns the #GtkOrientation of the tool palette.
 */
GtkOrientation
gtk_tool_palette_get_orientation (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_ORIENTATION);
  return palette->priv->orientation;
}

/**
 * gtk_tool_palette_get_style:
 * @palette: an #GtkToolPalette.
 *
 * Gets the style (icons, text or both) of items in the tool palette.
 *
 * Returns: the #GtkToolbarStyle of items in the tool palette.
 *
 * Since: 2.18
 */
GtkToolbarStyle
gtk_tool_palette_get_style (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_TOOLBAR_STYLE);
  return palette->priv->style;
}

/**
 * gtk_tool_palette_set_group_position:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @position: a new index for group.
 *
 * Sets the position of the group as an index of the tool palette.
 * If position is 0 the group will become the first child, if position is
 * -1 it will become the last child.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_group_position (GtkToolPalette *palette,
                                     GtkWidget      *group,
                                     gint            position)
{
  GtkToolItemGroupInfo group_info;
  gint old_position;
  gpointer src, dst;
  gsize len;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  gtk_tool_palette_repack (palette);

  g_return_if_fail (position >= -1);

  if (-1 == position)
    position = palette->priv->groups_length - 1;

  g_return_if_fail ((guint) position < palette->priv->groups_length);

  if (GTK_TOOL_ITEM_GROUP (group) == palette->priv->groups[position].widget)
    return;

  old_position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (old_position >= 0);

  group_info = palette->priv->groups[old_position];

  if (position < old_position)
    {
      dst = palette->priv->groups + position + 1;
      src = palette->priv->groups + position;
      len = old_position - position;
    }
  else
    {
      dst = palette->priv->groups + old_position;
      src = palette->priv->groups + old_position + 1;
      len = position - old_position;
    }

  memmove (dst, src, len * sizeof (*palette->priv->groups));
  palette->priv->groups[position] = group_info;

  gtk_widget_queue_resize (GTK_WIDGET (palette));
}

static void
gtk_tool_palette_group_notify_collapsed (GtkToolItemGroup *group,
                                         GParamSpec       *pspec G_GNUC_UNUSED,
                                         gpointer          data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (data);
  guint i;

  if (gtk_tool_item_group_get_collapsed (group))
    return;

  for (i = 0; i < palette->priv->groups_size; ++i)
    {
      GtkToolItemGroup *current_group = palette->priv->groups[i].widget;

      if (current_group && current_group != group)
        gtk_tool_item_group_set_collapsed (palette->priv->groups[i].widget, TRUE);
    }
}

/**
 * gtk_tool_palette_set_exclusive:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @exclusive: whether the group should be exclusive or not.
 *
 * Sets whether the group should be exclusive or not. If an exclusive group is expanded
 * all other groups are collapsed.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_exclusive (GtkToolPalette *palette,
                                GtkWidget      *group,
                                gboolean        exclusive)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = &palette->priv->groups[position];

  if (exclusive == group_info->exclusive)
    return;

  group_info->exclusive = exclusive;

  if (group_info->exclusive != (0 != group_info->notify_collapsed))
    {
      if (group_info->exclusive)
        {
          group_info->notify_collapsed =
            g_signal_connect (group, "notify::collapsed",
                              G_CALLBACK (gtk_tool_palette_group_notify_collapsed),
                              palette);
        }
      else
        {
          g_signal_handler_disconnect (group, group_info->notify_collapsed);
          group_info->notify_collapsed = 0;
        }
    }

  gtk_tool_palette_group_notify_collapsed (group_info->widget, NULL, palette);
  gtk_widget_child_notify (group, "exclusive");
}

/**
 * gtk_tool_palette_set_expand:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @expand: whether the group should be given extra space.
 *
 * Sets whether the group should be given extra space.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_expand (GtkToolPalette *palette,
                             GtkWidget      *group,
                             gboolean        expand)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = &palette->priv->groups[position];

  if (expand != group_info->expand)
    {
      group_info->expand = expand;
      gtk_widget_queue_resize (GTK_WIDGET (palette));
      gtk_widget_child_notify (group, "expand");
    }
}

/**
 * gtk_tool_palette_get_group_position:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup.
 *
 * Gets the position of @group in @palette as index. see gtk_tool_palette_set_group_position().
 *
 * Returns: the index of group or -1 if @group is not a child of @palette.
 *
 * Since: 2.18
 */
gint
gtk_tool_palette_get_group_position (GtkToolPalette *palette,
                                     GtkWidget      *group)
{
  guint i;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), -1);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), -1);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if ((gpointer) group == palette->priv->groups[i].widget)
      return i;

  return -1;
}

/**
 * gtk_tool_palette_get_exclusive:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 *
 * Gets whether group is exclusive or not. See gtk_tool_palette_set_exclusive().
 *
 * Returns: %TRUE if group is exclusive.
 *
 * Since: 2.18
 */
gboolean
gtk_tool_palette_get_exclusive (GtkToolPalette *palette,
                                GtkWidget      *group)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXCLUSIVE);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXCLUSIVE);

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXCLUSIVE);

  return palette->priv->groups[position].exclusive;
}

/**
 * gtk_tool_palette_get_expand:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 *
 * Gets whether group should be given extra space. See gtk_tool_palette_set_expand().
 *
 * Returns: %TRUE if group should be given extra space, %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean
gtk_tool_palette_get_expand (GtkToolPalette *palette,
                             GtkWidget      *group)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXPAND);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXPAND);

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXPAND);

  return palette->priv->groups[position].expand;
}

/**
 * gtk_tool_palette_get_drop_item:
 * @palette: an #GtkToolPalette.
 * @x: the x position.
 * @y: the y position.
 *
 * Gets the item at position (x, y). See gtk_tool_palette_get_drop_group().
 *
 * Returns: the #GtkToolItem at position or %NULL if there is no such item.
 *
 * Since: 2.18
 */
GtkToolItem*
gtk_tool_palette_get_drop_item (GtkToolPalette *palette,
                                gint            x,
                                gint            y)
{
  GtkWidget *group = gtk_tool_palette_get_drop_group (palette, x, y);

  if (group)
    return gtk_tool_item_group_get_drop_item (GTK_TOOL_ITEM_GROUP (group),
                                              x - group->allocation.x,
                                              y - group->allocation.y);

  return NULL;
}

/**
 * gtk_tool_palette_get_drop_group:
 * @palette: an #GtkToolPalette.
 * @x: the x position.
 * @y: the y position.
 *
 * Gets the group at position (x, y).
 *
 * Returns: the #GtkToolItemGroup at position or %NULL if there is no such group.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_get_drop_group (GtkToolPalette *palette,
                                 gint            x,
                                 gint            y)
{
  GtkAllocation *allocation;
  guint i;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);

  allocation = &GTK_WIDGET (palette)->allocation;

  g_return_val_if_fail (x >= 0 && x < allocation->width, NULL);
  g_return_val_if_fail (y >= 0 && y < allocation->height, NULL);

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      GtkWidget *widget;
      gint x0, y0;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      x0 = x - widget->allocation.x;
      y0 = y - widget->allocation.y;

      if (x0 >= 0 && x0 < widget->allocation.width &&
          y0 >= 0 && y0 < widget->allocation.height)
        return widget;
    }

  return NULL;
}

/**
 * gtk_tool_palette_get_drag_item:
 * @palette: an #GtkToolPalette.
 * @selection: a #GtkSelectionData.
 *
 * Get the dragged item from the selection. This could be a #GtkToolItem or 
 * an #GtkToolItemGroup.
 *
 * Returns: the dragged item in selection.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_get_drag_item (GtkToolPalette         *palette,
                                const GtkSelectionData *selection)
{
  GtkToolPaletteDragData *data;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  g_return_val_if_fail (NULL != selection, NULL);

  g_return_val_if_fail (selection->format == 8, NULL);
  g_return_val_if_fail (selection->length == sizeof (GtkToolPaletteDragData), NULL);
  g_return_val_if_fail (selection->target == dnd_target_atom_item ||
                        selection->target == dnd_target_atom_group,
                        NULL);

  data = (GtkToolPaletteDragData*) selection->data;

  g_return_val_if_fail (data->palette == palette, NULL);

  if (dnd_target_atom_item == selection->target)
    g_return_val_if_fail (GTK_IS_TOOL_ITEM (data->item), NULL);
  else if (dnd_target_atom_group == selection->target)
    g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (data->item), NULL);

  return data->item;
}

/**
 * gtk_tool_palette_set_drag_source:
 * @palette: an #GtkToolPalette.
 * @targets: the #GtkToolPaletteDragTargets which the widget should support.
 *
 * Sets the tool palette as a drag source. Enables all groups and items in
 * the tool palette as drag sources on button 1 and button 3 press with copy
 * and move actions.
 *
 * See gtk_drag_source_set().
 *
 * Since: 2.18
 *
 */
void
gtk_tool_palette_set_drag_source (GtkToolPalette            *palette,
                                  GtkToolPaletteDragTargets  targets)
{
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if ((palette->priv->drag_source & targets) == targets)
    return;

  palette->priv->drag_source |= targets;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      if (palette->priv->groups[i].widget)
        gtk_container_forall (GTK_CONTAINER (palette->priv->groups[i].widget),
                              _gtk_tool_palette_child_set_drag_source,
                              palette);
    }
}

/**
 * gtk_tool_palette_add_drag_dest:
 * @palette: an #GtkToolPalette.
 * @widget: a #GtkWidget which should be a drag destination for palette.
 * @flags: the flags that specify what actions GTK+ should take for drops on that widget.
 * @targets: the #GtkToolPaletteDragTargets which the widget should support.
 * @actions: the #GdkDragAction<!-- -->s which the widget should suppport.
 *
 * Sets the tool palette as drag source (see gtk_tool_palette_set_drag_source) and
 * sets widget as a drag destination for drags from palette. With flags the actions
 * (like highlighting and target checking) which should be performed by GTK+ for
 * drops on widget can be specified. With targets the supported drag targets 
 * (groups and/or items) can be specified. With actions the supported drag actions
 * (copy and move) can be specified.
 *
 * See gtk_drag_dest_set().
 *
 * Since: 2.18
 */
void
gtk_tool_palette_add_drag_dest (GtkToolPalette            *palette,
                                GtkWidget                 *widget,
                                GtkDestDefaults            flags,
                                GtkToolPaletteDragTargets  targets,
                                GdkDragAction              actions)
{
  GtkTargetEntry entries[G_N_ELEMENTS (dnd_targets)];
  gint n_entries = 0;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_tool_palette_set_drag_source (palette,
                                    targets);

  if (targets & GTK_TOOL_PALETTE_DRAG_ITEMS)
    entries[n_entries++] = dnd_targets[0];
  if (targets & GTK_TOOL_PALETTE_DRAG_GROUPS)
    entries[n_entries++] = dnd_targets[1];

  gtk_drag_dest_set (widget, flags, entries, n_entries, actions);
}

void
_gtk_tool_palette_get_item_size (GtkToolPalette *palette,
                                 GtkRequisition *item_size,
                                 gboolean        homogeneous_only,
                                 gint           *requested_rows)
{
  GtkRequisition max_requisition;
  gint max_rows;
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (NULL != item_size);

  max_requisition.width = 0;
  max_requisition.height = 0;
  max_rows = 0;

  /* iterate over all groups and calculate the max item_size and max row request */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkRequisition requisition;
      gint rows;
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (!group->widget)
        continue;

      _gtk_tool_item_group_item_size_request (group->widget, &requisition, homogeneous_only, &rows);

      max_requisition.width = MAX (max_requisition.width, requisition.width);
      max_requisition.height = MAX (max_requisition.height, requisition.height);
      max_rows = MAX (max_rows, rows);
    }

  *item_size = max_requisition;
  if (requested_rows)
    *requested_rows = max_rows;
}

static void
gtk_tool_palette_item_drag_data_get (GtkWidget        *widget,
                                     GdkDragContext   *context G_GNUC_UNUSED,
                                     GtkSelectionData *selection,
                                     guint             info G_GNUC_UNUSED,
                                     guint             time G_GNUC_UNUSED,
                                     gpointer          data)
{
  GtkToolPaletteDragData drag_data = { GTK_TOOL_PALETTE (data), NULL };

  if (selection->target == dnd_target_atom_item)
    drag_data.item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM);

  if (drag_data.item)
    gtk_selection_data_set (selection, selection->target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

static void
gtk_tool_palette_child_drag_data_get (GtkWidget        *widget,
                                      GdkDragContext   *context G_GNUC_UNUSED,
                                      GtkSelectionData *selection,
                                      guint             info G_GNUC_UNUSED,
                                      guint             time G_GNUC_UNUSED,
                                      gpointer          data)
{
  GtkToolPaletteDragData drag_data = { GTK_TOOL_PALETTE (data), NULL };

  if (selection->target == dnd_target_atom_group)
    drag_data.item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM_GROUP);

  if (drag_data.item)
    gtk_selection_data_set (selection, selection->target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

void
_gtk_tool_palette_child_set_drag_source (GtkWidget *child,
                                         gpointer   data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (data);

  /* Check drag_source,
   * to work properly when called from gtk_tool_item_group_insert().
   */
  if (!palette->priv->drag_source)
    return;

  if (GTK_IS_TOOL_ITEM (child) &&
      (palette->priv->drag_source & GTK_TOOL_PALETTE_DRAG_ITEMS))
    {
      /* Connect to child instead of the item itself,
       * to work arround bug 510377.
       */
      if (GTK_IS_TOOL_BUTTON (child))
        child = gtk_bin_get_child (GTK_BIN (child));

      if (!child)
        return;

      gtk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[0], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (gtk_tool_palette_item_drag_data_get),
                        palette);
    }
  else if (GTK_IS_BUTTON (child) && 
           (palette->priv->drag_source & GTK_TOOL_PALETTE_DRAG_GROUPS))
    {
      gtk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[1], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (gtk_tool_palette_child_drag_data_get),
                        palette);
    }
}

/**
 * gtk_tool_palette_get_drag_target_item:
 *
 * Get the target entry for a dragged #GtkToolItem.
 *
 * Returns: the #GtkTargetEntry for a dragged item.
 *
 * Since: 2.18
 */
G_CONST_RETURN GtkTargetEntry*
gtk_tool_palette_get_drag_target_item (void)
{
  return &dnd_targets[0];
}

/**
 * gtk_tool_palette_get_drag_target_group:
 *
 * Get the target entry for a dragged #GtkToolItemGroup.
 *
 * Returns: the #GtkTargetEntry for a dragged group.
 *
 * Since: 2.18
 */
G_CONST_RETURN GtkTargetEntry*
gtk_tool_palette_get_drag_target_group (void)
{
  return &dnd_targets[1];
}

void
_gtk_tool_palette_set_expanding_child (GtkToolPalette *palette,
                                       GtkWidget      *widget)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  palette->priv->expanding_child = widget;
}

GtkAdjustment*
gtk_tool_palette_get_hadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  return palette->priv->hadjustment;
}

GtkAdjustment*
gtk_tool_palette_get_vadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  return palette->priv->vadjustment;
}

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090

GtkSizeGroup *
_gtk_tool_palette_get_size_group (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);

  return palette->priv->text_size_group;
}

#endif
/* GtkToolPalette -- A tool palette with categories and DnD support
 * Copyright (C) 2008  Openismus GmbH
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 */

#include "gtktoolpaletteprivate.h"
#include "gtkmarshalers.h"

#include <gtk/gtk.h>
#include <string.h>

#define DEFAULT_ICON_SIZE       GTK_ICON_SIZE_SMALL_TOOLBAR
#define DEFAULT_ORIENTATION     GTK_ORIENTATION_VERTICAL
#define DEFAULT_TOOLBAR_STYLE   GTK_TOOLBAR_ICONS

#define DEFAULT_CHILD_EXCLUSIVE FALSE
#define DEFAULT_CHILD_EXPAND    FALSE

#define P_(msgid) (msgid)

/**
 * SECTION:GtkToolPalette
 * @short_description: A tool palette with categories
 * @include: gtktoolpalette.h
 *
 * An #GtkToolPalette allows it to add #GtkToolItem<!-- -->s to a palette like container
 * with different categories and drag and drop support.
 *
 * An #GtkToolPalette is created with a call to gtk_tool_palette_new().
 *
 * #GtkToolItem<!-- -->s cannot be added directly to an #GtkToolPalette, instead they
 * are added to an #GtkToolItemGroup which can than be added to an #GtkToolPalette. To add
 * an #GtkToolItemGroup to an #GtkToolPalette use gtk_container_add().
 *
 * |[
 * GtkWidget *palette, *group;
 * GtkToolItem *item;
 *
 * palette = gtk_tool_palette_new ();
 * group = gtk_tool_item_group_new (_("Test Category"));
 * gtk_container_add (GTK_CONTAINER (palette), group);
 *
 * item = gtk_tool_button_new_from_stock (GTK_STOCK_OK);
 * gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
 * ]|
 *
 * The easiest way to use drag and drop with GtkToolPalette is to call gtk_tool_palette_add_drag_dest()
 * with the desired drag source @palette and the desired drag target @widget. Than gtk_tool_palette_get_drag_item()
 * can be used to get the dragged item in the #GtkWidget::drag-data-received signal handler of the drag target.
 *
 * |[
 * static void
 * passive_canvas_drag_data_received (GtkWidget        *widget,
 *                                    GdkDragContext   *context,
 *                                    gint              x,
 *                                    gint              y,
 *                                    GtkSelectionData *selection,
 *                                    guint             info,
 *                                    guint             time,
 *                                    gpointer          data)
 * {
 *   GtkWidget *palette;
 *   GtkWidget *item;
 *
 *   /<!-- -->* Get the dragged item *<!-- -->/
 *   palette = gtk_widget_get_ancestor (gtk_drag_get_source_widget (context), GTK_TYPE_TOOL_PALETTE);
 *   if (palette != NULL)
 *     item = gtk_tool_palette_get_drag_item (GTK_TOOL_PALETTE (palette), selection);
 *
 *   /<!-- -->* Do something with item *<!-- -->/
 * }
 *
 * GtkWidget *target, palette;
 *
 * palette = gtk_tool_palette_new ();
 * target = gtk_drawing_area_new ();
 *
 * g_signal_connect (G_OBJECT (target), "drag-data-received",
 *                   G_CALLBACK (passive_canvas_drag_data_received), NULL);   
 * gtk_tool_palette_add_drag_dest (GTK_TOOL_PALETTE (palette), target,
 *                                 GTK_DEST_DEFAULT_ALL,
 *                                 GTK_TOOL_PALETTE_DRAG_ITEMS,
 *                                 GDK_ACTION_COPY);
 * ]|
 *
 * Since: 2.18
 */

typedef struct _GtkToolItemGroupInfo   GtkToolItemGroupInfo;
typedef struct _GtkToolPaletteDragData GtkToolPaletteDragData;

enum
{
  PROP_NONE,
  PROP_ICON_SIZE,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
};

enum
{
  CHILD_PROP_NONE,
  CHILD_PROP_EXCLUSIVE,
  CHILD_PROP_EXPAND,
};

struct _GtkToolItemGroupInfo
{
  GtkToolItemGroup *widget;

  guint             notify_collapsed;
  guint             exclusive : 1;
  guint             expand : 1;
};

struct _GtkToolPalettePrivate
{
  GtkToolItemGroupInfo *groups;
  gsize                 groups_size;
  gsize                 groups_length;

  GtkAdjustment        *hadjustment;
  GtkAdjustment        *vadjustment;

  GtkIconSize           icon_size;
  GtkOrientation        orientation;
  GtkToolbarStyle       style;

  GtkWidget            *expanding_child;

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  GtkSizeGroup         *text_size_group;
#endif

  guint                 sparse_groups : 1;
  guint                 drag_source : 2;
};

struct _GtkToolPaletteDragData
{
  GtkToolPalette *palette;
  GtkWidget      *item;
};

static GdkAtom dnd_target_atom_item = GDK_NONE;
static GdkAtom dnd_target_atom_group = GDK_NONE;

static const GtkTargetEntry dnd_targets[] =
{
  { "application/x-GTK-tool-palette-item", GTK_TARGET_SAME_APP, 0 },
  { "application/x-GTK-tool-palette-group", GTK_TARGET_SAME_APP, 0 },
};

G_DEFINE_TYPE (GtkToolPalette,
               gtk_tool_palette,
               GTK_TYPE_CONTAINER);

static void
gtk_tool_palette_init (GtkToolPalette *palette)
{
  palette->priv = G_TYPE_INSTANCE_GET_PRIVATE (palette,
                                               GTK_TYPE_TOOL_PALETTE,
                                               GtkToolPalettePrivate);

  palette->priv->groups_size = 4;
  palette->priv->groups_length = 0;
  palette->priv->groups = g_new0 (GtkToolItemGroupInfo, palette->priv->groups_size);

  palette->priv->icon_size = DEFAULT_ICON_SIZE;
  palette->priv->orientation = DEFAULT_ORIENTATION;
  palette->priv->style = DEFAULT_TOOLBAR_STYLE;

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  palette->priv->text_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
#endif
}

static void
gtk_tool_palette_reconfigured (GtkToolPalette *palette)
{
  guint i;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      if (palette->priv->groups[i].widget)
        _gtk_tool_item_group_palette_reconfigured (palette->priv->groups[i].widget);
    }

  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (palette));
}

static void
gtk_tool_palette_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        if ((guint) g_value_get_enum (value) != palette->priv->icon_size)
          {
            palette->priv->icon_size = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      case PROP_ORIENTATION:
        if ((guint) g_value_get_enum (value) != palette->priv->orientation)
          {
            palette->priv->orientation = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      case PROP_TOOLBAR_STYLE:
        if ((guint) g_value_get_enum (value) != palette->priv->style)
          {
            palette->priv->style = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        g_value_set_enum (value, gtk_tool_palette_get_icon_size (palette));
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, gtk_tool_palette_get_orientation (palette));
        break;

      case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, gtk_tool_palette_get_style (palette));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_dispose (GObject *object)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);
  guint i;

  if (palette->priv->hadjustment)
    {
      g_object_unref (palette->priv->hadjustment);
      palette->priv->hadjustment = NULL;
    }

  if (palette->priv->vadjustment)
    {
      g_object_unref (palette->priv->vadjustment);
      palette->priv->vadjustment = NULL;
    }

  for (i = 0; i < palette->priv->groups_size; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (group->notify_collapsed)
        {
          g_signal_handler_disconnect (group->widget, group->notify_collapsed);
          group->notify_collapsed = 0;
        }
    }

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  if (palette->priv->text_size_group)
    {
      g_object_unref (palette->priv->text_size_group);
      palette->priv->text_size_group = NULL;
    }
#endif

  G_OBJECT_CLASS (gtk_tool_palette_parent_class)->dispose (object);
}

static void
gtk_tool_palette_finalize (GObject *object)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  if (palette->priv->groups)
    {
      palette->priv->groups_length = 0;
      g_free (palette->priv->groups);
      palette->priv->groups = NULL;
    }

  G_OBJECT_CLASS (gtk_tool_palette_parent_class)->finalize (object);
}

static void
gtk_tool_palette_size_request (GtkWidget      *widget,
                               GtkRequisition *requisition)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GtkRequisition child_requisition;
  guint i;

  requisition->width = 0;
  requisition->height = 0;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (!group->widget)
        continue;

      gtk_widget_size_request (GTK_WIDGET (group->widget), &child_requisition);

      if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
        {
          requisition->width = MAX (requisition->width, child_requisition.width);
          requisition->height += child_requisition.height;
        }
      else
        {
          requisition->width += child_requisition.width;
          requisition->height = MAX (requisition->height, child_requisition.height);
        }
    }

  requisition->width += border_width * 2;
  requisition->height += border_width * 2;
}

static void
gtk_tool_palette_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GtkAdjustment *adjustment = NULL;
  GtkAllocation child_allocation;

  gint n_expand_groups = 0;
  gint remaining_space = 0;
  gint expand_space = 0;

  gint page_start, page_size = 0;
  gint offset = 0;
  guint i;

  gint min_offset = -1, max_offset = -1;

  gint x;

  gint *group_sizes = g_newa(gint, palette->priv->groups_length);

  GtkTextDirection direction = gtk_widget_get_direction (widget);

  GTK_WIDGET_CLASS (gtk_tool_palette_parent_class)->size_allocate (widget, allocation);

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      adjustment = palette->priv->vadjustment;
      page_size = allocation->height;
    }
  else
    {
      adjustment = palette->priv->hadjustment;
      page_size = allocation->width;
    }

  if (adjustment)
    offset = gtk_adjustment_get_value (adjustment);
  if (GTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
      GTK_TEXT_DIR_RTL == direction)
    offset = -offset;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.width = allocation->width - border_width * 2;
  else
    child_allocation.height = allocation->height - border_width * 2;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    remaining_space = allocation->height;
  else
    remaining_space = allocation->width;

  /* figure out the required size of all groups to be able to distribute the
   * remaining space on allocation */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      gint size;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      if (gtk_tool_item_group_get_n_items (group->widget))
        {
          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            size = _gtk_tool_item_group_get_height_for_width (group->widget, child_allocation.width);
          else
            size = _gtk_tool_item_group_get_width_for_height (group->widget, child_allocation.height);

          if (group->expand && !gtk_tool_item_group_get_collapsed (group->widget))
            n_expand_groups += 1;
        }
      else
        size = 0;

      remaining_space -= size;
      group_sizes[i] = size;

      /* if the widget is currently expanding an offset which allows to display as much of the
       * widget as possible is calculated */
      if (widget == palette->priv->expanding_child)
        {
          gint limit =
            GTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
            child_allocation.width : child_allocation.height;

          gint real_size;
          guint j;

          min_offset = 0;

          for (j = 0; j < i; ++j)
            min_offset += group_sizes[j];

          max_offset = min_offset + group_sizes[i];

          real_size = _gtk_tool_item_group_get_size_for_limit
            (GTK_TOOL_ITEM_GROUP (widget), limit,
             GTK_ORIENTATION_VERTICAL == palette->priv->orientation,
             FALSE);

          if (size == real_size)
            palette->priv->expanding_child = NULL;
        }
    }

  if (n_expand_groups > 0)
    {
      remaining_space = MAX (0, remaining_space);
      expand_space = remaining_space / n_expand_groups;
    }

  if (max_offset != -1)
    {
      gint limit =
        GTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
        allocation->height : allocation->width;

      offset = MIN (MAX (offset, max_offset - limit), min_offset);
    }

  if (remaining_space > 0)
    offset = 0;

  x = border_width;
  child_allocation.y = border_width;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.y -= offset;
  else
    x -= offset;

  /* allocate all groups at the calculated positions */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      GtkWidget *widget;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      if (gtk_tool_item_group_get_n_items (group->widget))
        {
          gint size = group_sizes[i];

          if (group->expand && !gtk_tool_item_group_get_collapsed (group->widget))
            {
              size += MIN (expand_space, remaining_space);
              remaining_space -= expand_space;
            }

          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.height = size;
          else
            child_allocation.width = size;

          if (GTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
              GTK_TEXT_DIR_RTL == direction)
            child_allocation.x = allocation->width - x - child_allocation.width;
          else
            child_allocation.x = x;

          gtk_widget_size_allocate (widget, &child_allocation);
          gtk_widget_show (widget);

          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.y += child_allocation.height;
          else
            x += child_allocation.width;
        }
      else
        gtk_widget_hide (widget);
    }

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      child_allocation.y += border_width;
      child_allocation.y += offset;

      page_start = child_allocation.y;
    }
  else
    {
      x += border_width;
      x += offset;

      page_start = x;
    }

  /* update the scrollbar to match the displayed adjustment */
  if (adjustment)
    {
      gdouble value;

      adjustment->page_increment = page_size * 0.9;
      adjustment->step_increment = page_size * 0.1;
      adjustment->page_size = page_size;

      if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation ||
          GTK_TEXT_DIR_LTR == direction)
        {
          adjustment->lower = 0;
          adjustment->upper = MAX (0, page_start);

          value = MIN (offset, adjustment->upper - adjustment->page_size);
          gtk_adjustment_clamp_page (adjustment, value, offset + page_size);
        }
      else
        {
          adjustment->lower = page_size - MAX (0, page_start);
          adjustment->upper = page_size;

          offset = -offset;

          value = MAX (offset, adjustment->lower);
          gtk_adjustment_clamp_page (adjustment, offset, value + page_size);
        }

      gtk_adjustment_changed (adjustment);
    }
}

static gboolean
gtk_tool_palette_expose_event (GtkWidget      *widget,
                               GdkEventExpose *event)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GdkDisplay *display;
  cairo_t *cr;
  guint i;

  display = gdk_drawable_get_display (widget->window);

  if (!gdk_display_supports_composite (display))
    return FALSE;

  cr = gdk_cairo_create (widget->window);
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  cairo_push_group (cr);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if (palette->priv->groups[i].widget)
      _gtk_tool_item_group_paint (palette->priv->groups[i].widget, cr);

  cairo_pop_group_to_source (cr);

  cairo_paint (cr);
  cairo_destroy (cr);

  return FALSE;
}

static void
gtk_tool_palette_realize (GtkWidget *widget)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  GdkWindowAttr attributes;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK
                        | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                        | GDK_BUTTON_MOTION_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, attributes_mask);

  gdk_window_set_user_data (widget->window, widget);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  gtk_container_forall (GTK_CONTAINER (widget),
                        (GtkCallback) gtk_widget_set_parent_window,
                        widget->window);

  gtk_widget_queue_resize_no_redraw (widget);
}

static void
gtk_tool_palette_adjustment_value_changed (GtkAdjustment *adjustment G_GNUC_UNUSED,
                                           gpointer       data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  gtk_tool_palette_size_allocate (widget, &widget->allocation);
}

static void
gtk_tool_palette_set_scroll_adjustments (GtkWidget     *widget,
                                         GtkAdjustment *hadjustment,
                                         GtkAdjustment *vadjustment)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);

  if (palette->priv->hadjustment)
    g_object_unref (palette->priv->hadjustment);
  if (palette->priv->vadjustment)
    g_object_unref (palette->priv->vadjustment);

  if (hadjustment)
    g_object_ref_sink (hadjustment);
  if (vadjustment)
    g_object_ref_sink (vadjustment);

  palette->priv->hadjustment = hadjustment;
  palette->priv->vadjustment = vadjustment;

  if (palette->priv->hadjustment)
    g_signal_connect (palette->priv->hadjustment, "value-changed",
                      G_CALLBACK (gtk_tool_palette_adjustment_value_changed),
                      palette);
  if (palette->priv->vadjustment)
    g_signal_connect (palette->priv->vadjustment, "value-changed",
                      G_CALLBACK (gtk_tool_palette_adjustment_value_changed),
                      palette);
}

static void
gtk_tool_palette_repack (GtkToolPalette *palette)
{
  guint si, di;

  for (si = di = 0; di < palette->priv->groups_length; ++si)
    {
      if (palette->priv->groups[si].widget)
        {
          palette->priv->groups[di] = palette->priv->groups[si];
          ++di;
        }
      else
        --palette->priv->groups_length;
    }

  palette->priv->sparse_groups = FALSE;
}

static void
gtk_tool_palette_add (GtkContainer *container,
                      GtkWidget    *child)
{
  GtkToolPalette *palette;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  palette = GTK_TOOL_PALETTE (container);

  if (palette->priv->groups_length == palette->priv->groups_size)
    gtk_tool_palette_repack (palette);

  if (palette->priv->groups_length == palette->priv->groups_size)
    {
      gsize old_size = palette->priv->groups_size;
      gsize new_size = old_size * 2;

      palette->priv->groups = g_renew (GtkToolItemGroupInfo,
                                       palette->priv->groups,
                                       new_size);

      memset (palette->priv->groups + old_size, 0,
              sizeof (GtkToolItemGroupInfo) * old_size);

      palette->priv->groups_size = new_size;
    }

  palette->priv->groups[palette->priv->groups_length].widget = g_object_ref_sink (child);
  palette->priv->groups_length += 1;

  gtk_widget_set_parent (child, GTK_WIDGET (palette));
}

static void
gtk_tool_palette_remove (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkToolPalette *palette;
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  palette = GTK_TOOL_PALETTE (container);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if ((GtkWidget*) palette->priv->groups[i].widget == child)
      {
        g_object_unref (child);
        gtk_widget_unparent (child);

        memset (&palette->priv->groups[i], 0, sizeof (GtkToolItemGroupInfo));
        palette->priv->sparse_groups = TRUE;
      }
}

static void
gtk_tool_palette_forall (GtkContainer *container,
                         gboolean      internals G_GNUC_UNUSED,
                         GtkCallback   callback,
                         gpointer      callback_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);
  guint i;

  if (palette->priv->groups)
    {
      for (i = 0; i < palette->priv->groups_length; ++i)
        if (palette->priv->groups[i].widget)
          callback (GTK_WIDGET (palette->priv->groups[i].widget),
                    callback_data);
    }
}

static GType
gtk_tool_palette_child_type (GtkContainer *container G_GNUC_UNUSED)
{
  return GTK_TYPE_TOOL_ITEM_GROUP;
}

static void
gtk_tool_palette_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        gtk_tool_palette_set_exclusive (palette, child, g_value_get_boolean (value));
        break;

      case CHILD_PROP_EXPAND:
        gtk_tool_palette_set_expand (palette, child, g_value_get_boolean (value));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        g_value_set_boolean (value, gtk_tool_palette_get_exclusive (palette, child));
        break;

      case CHILD_PROP_EXPAND:
        g_value_set_boolean (value, gtk_tool_palette_get_expand (palette, child));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_class_init (GtkToolPaletteClass *cls)
{
  GObjectClass      *oclass   = G_OBJECT_CLASS (cls);
  GtkWidgetClass    *wclass   = GTK_WIDGET_CLASS (cls);
  GtkContainerClass *cclass   = GTK_CONTAINER_CLASS (cls);

  oclass->set_property        = gtk_tool_palette_set_property;
  oclass->get_property        = gtk_tool_palette_get_property;
  oclass->dispose             = gtk_tool_palette_dispose;
  oclass->finalize            = gtk_tool_palette_finalize;

  wclass->size_request        = gtk_tool_palette_size_request;
  wclass->size_allocate       = gtk_tool_palette_size_allocate;
  wclass->expose_event        = gtk_tool_palette_expose_event;
  wclass->realize             = gtk_tool_palette_realize;

  cclass->add                 = gtk_tool_palette_add;
  cclass->remove              = gtk_tool_palette_remove;
  cclass->forall              = gtk_tool_palette_forall;
  cclass->child_type          = gtk_tool_palette_child_type;
  cclass->set_child_property  = gtk_tool_palette_set_child_property;
  cclass->get_child_property  = gtk_tool_palette_get_child_property;

  cls->set_scroll_adjustments = gtk_tool_palette_set_scroll_adjustments;
  /**
   * GtkToolPalette::set-scroll-adjustments:
   * @widget: the GtkToolPalette that received the signal
   * @hadjustment: The horizontal adjustment 
   * @vadjustment: The vertical adjustment
   *
   * The ::set-scroll-adjustments when FIXME
   *
   * Since: 2.18
   */
  wclass->set_scroll_adjustments_signal =
    g_signal_new ("set-scroll-adjustments",
                  G_TYPE_FROM_CLASS (oclass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkToolPaletteClass, set_scroll_adjustments),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ADJUSTMENT,
                  GTK_TYPE_ADJUSTMENT);

  g_object_class_install_property (oclass, PROP_ICON_SIZE,
                                   g_param_spec_enum ("icon-size",
                                                      P_("Icon Size"),
                                                      P_("The size of palette icons"),
                                                      GTK_TYPE_ICON_SIZE,
                                                      DEFAULT_ICON_SIZE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      P_("Orientation"),
                                                      P_("Orientation of the tool palette"),
                                                      GTK_TYPE_ORIENTATION,
                                                      DEFAULT_ORIENTATION,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_TOOLBAR_STYLE,
                                   g_param_spec_enum ("toolbar-style",
                                                      P_("Toolbar Style"),
                                                      P_("Style of items in the tool palette"),
                                                      GTK_TYPE_TOOLBAR_STYLE,
                                                      DEFAULT_TOOLBAR_STYLE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXCLUSIVE,
                                              g_param_spec_boolean ("exclusive",
                                                                    P_("Exclusive"),
                                                                    P_("Whether the item group should be the only expanded at a given time"),
                                                                    DEFAULT_CHILD_EXCLUSIVE,
                                                                    G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                                    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXPAND,
                                              g_param_spec_boolean ("expand",
                                                                    P_("Expand"),
                                                                    P_("Whether the item group should receive extra space when the palette grows"),
                                                                    DEFAULT_CHILD_EXPAND,
                                                                    G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                                    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (GtkToolPalettePrivate));

  dnd_target_atom_item = gdk_atom_intern_static_string (dnd_targets[0].target);
  dnd_target_atom_group = gdk_atom_intern_static_string (dnd_targets[1].target);
}

/**
 * gtk_tool_palette_new:
 *
 * Creates a new tool palette.
 *
 * Returns: a new #GtkToolPalette.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_new (void)
{
  return g_object_new (GTK_TYPE_TOOL_PALETTE, NULL);
}

/**
 * gtk_tool_palette_set_icon_size:
 * @palette: an #GtkToolPalette.
 * @icon_size: the #GtkIconSize that icons in the tool palette shall have.
 *
 * Sets the size of icons in the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_icon_size (GtkToolPalette *palette,
                                GtkIconSize     icon_size)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (icon_size != palette->priv->icon_size)
    g_object_set (palette, "icon-size", icon_size, NULL);
}

/**
 * gtk_tool_palette_set_orientation:
 * @palette: an #GtkToolPalette.
 * @orientation: the #GtkOrientation that the tool palette shall have.
 *
 * Sets the orientation (horizontal or vertical) of the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_orientation (GtkToolPalette *palette,
                                  GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (orientation != palette->priv->orientation)
    g_object_set (palette, "orientation", orientation, NULL);
}

/**
 * gtk_tool_palette_set_style:
 * @palette: an #GtkToolPalette.
 * @style: the #GtkToolbarStyle that items in the tool palette shall have.
 *
 * Sets the style (text, icons or both) of items in the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_style (GtkToolPalette  *palette,
                            GtkToolbarStyle  style)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (style != palette->priv->style)
    g_object_set (palette, "toolbar-style", style, NULL);
}

/**
 * gtk_tool_palette_get_icon_size:
 * @palette: an #GtkToolPalette.
 *
 * Gets the size of icons in the tool palette. See gtk_tool_palette_set_icon_size().
 * 
 * Returns: the #GtkIconSize of icons in the tool palette.
 *
 * Since: 2.18
 */
GtkIconSize
gtk_tool_palette_get_icon_size (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_ICON_SIZE);
  return palette->priv->icon_size;
}

/**
 * gtk_tool_palette_get_orientation:
 * @palette: an #GtkToolPalette.
 *
 * Gets the orientation (horizontal or vertical) of the tool palette. See gtk_tool_palette_set_orientation().
 *
 * Returns the #GtkOrientation of the tool palette.
 */
GtkOrientation
gtk_tool_palette_get_orientation (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_ORIENTATION);
  return palette->priv->orientation;
}

/**
 * gtk_tool_palette_get_style:
 * @palette: an #GtkToolPalette.
 *
 * Gets the style (icons, text or both) of items in the tool palette.
 *
 * Returns: the #GtkToolbarStyle of items in the tool palette.
 *
 * Since: 2.18
 */
GtkToolbarStyle
gtk_tool_palette_get_style (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_TOOLBAR_STYLE);
  return palette->priv->style;
}

/**
 * gtk_tool_palette_set_group_position:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @position: a new index for group.
 *
 * Sets the position of the group as an index of the tool palette.
 * If position is 0 the group will become the first child, if position is
 * -1 it will become the last child.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_group_position (GtkToolPalette *palette,
                                     GtkWidget      *group,
                                     gint            position)
{
  GtkToolItemGroupInfo group_info;
  gint old_position;
  gpointer src, dst;
  gsize len;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  gtk_tool_palette_repack (palette);

  g_return_if_fail (position >= -1);

  if (-1 == position)
    position = palette->priv->groups_length - 1;

  g_return_if_fail ((guint) position < palette->priv->groups_length);

  if (GTK_TOOL_ITEM_GROUP (group) == palette->priv->groups[position].widget)
    return;

  old_position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (old_position >= 0);

  group_info = palette->priv->groups[old_position];

  if (position < old_position)
    {
      dst = palette->priv->groups + position + 1;
      src = palette->priv->groups + position;
      len = old_position - position;
    }
  else
    {
      dst = palette->priv->groups + old_position;
      src = palette->priv->groups + old_position + 1;
      len = position - old_position;
    }

  memmove (dst, src, len * sizeof (*palette->priv->groups));
  palette->priv->groups[position] = group_info;

  gtk_widget_queue_resize (GTK_WIDGET (palette));
}

static void
gtk_tool_palette_group_notify_collapsed (GtkToolItemGroup *group,
                                         GParamSpec       *pspec G_GNUC_UNUSED,
                                         gpointer          data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (data);
  guint i;

  if (gtk_tool_item_group_get_collapsed (group))
    return;

  for (i = 0; i < palette->priv->groups_size; ++i)
    {
      GtkToolItemGroup *current_group = palette->priv->groups[i].widget;

      if (current_group && current_group != group)
        gtk_tool_item_group_set_collapsed (palette->priv->groups[i].widget, TRUE);
    }
}

/**
 * gtk_tool_palette_set_exclusive:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @exclusive: whether the group should be exclusive or not.
 *
 * Sets whether the group should be exclusive or not. If an exclusive group is expanded
 * all other groups are collapsed.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_exclusive (GtkToolPalette *palette,
                                GtkWidget      *group,
                                gboolean        exclusive)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = &palette->priv->groups[position];

  if (exclusive == group_info->exclusive)
    return;

  group_info->exclusive = exclusive;

  if (group_info->exclusive != (0 != group_info->notify_collapsed))
    {
      if (group_info->exclusive)
        {
          group_info->notify_collapsed =
            g_signal_connect (group, "notify::collapsed",
                              G_CALLBACK (gtk_tool_palette_group_notify_collapsed),
                              palette);
        }
      else
        {
          g_signal_handler_disconnect (group, group_info->notify_collapsed);
          group_info->notify_collapsed = 0;
        }
    }

  gtk_tool_palette_group_notify_collapsed (group_info->widget, NULL, palette);
  gtk_widget_child_notify (group, "exclusive");
}

/**
 * gtk_tool_palette_set_expand:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @expand: whether the group should be given extra space.
 *
 * Sets whether the group should be given extra space.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_expand (GtkToolPalette *palette,
                             GtkWidget      *group,
                             gboolean        expand)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = &palette->priv->groups[position];

  if (expand != group_info->expand)
    {
      group_info->expand = expand;
      gtk_widget_queue_resize (GTK_WIDGET (palette));
      gtk_widget_child_notify (group, "expand");
    }
}

/**
 * gtk_tool_palette_get_group_position:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup.
 *
 * Gets the position of @group in @palette as index. see gtk_tool_palette_set_group_position().
 *
 * Returns: the index of group or -1 if @group is not a child of @palette.
 *
 * Since: 2.18
 */
gint
gtk_tool_palette_get_group_position (GtkToolPalette *palette,
                                     GtkWidget      *group)
{
  guint i;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), -1);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), -1);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if ((gpointer) group == palette->priv->groups[i].widget)
      return i;

  return -1;
}

/**
 * gtk_tool_palette_get_exclusive:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 *
 * Gets whether group is exclusive or not. See gtk_tool_palette_set_exclusive().
 *
 * Returns: %TRUE if group is exclusive.
 *
 * Since: 2.18
 */
gboolean
gtk_tool_palette_get_exclusive (GtkToolPalette *palette,
                                GtkWidget      *group)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXCLUSIVE);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXCLUSIVE);

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXCLUSIVE);

  return palette->priv->groups[position].exclusive;
}

/**
 * gtk_tool_palette_get_expand:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 *
 * Gets whether group should be given extra space. See gtk_tool_palette_set_expand().
 *
 * Returns: %TRUE if group should be given extra space, %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean
gtk_tool_palette_get_expand (GtkToolPalette *palette,
                             GtkWidget      *group)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXPAND);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXPAND);

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXPAND);

  return palette->priv->groups[position].expand;
}

/**
 * gtk_tool_palette_get_drop_item:
 * @palette: an #GtkToolPalette.
 * @x: the x position.
 * @y: the y position.
 *
 * Gets the item at position (x, y). See gtk_tool_palette_get_drop_group().
 *
 * Returns: the #GtkToolItem at position or %NULL if there is no such item.
 *
 * Since: 2.18
 */
GtkToolItem*
gtk_tool_palette_get_drop_item (GtkToolPalette *palette,
                                gint            x,
                                gint            y)
{
  GtkWidget *group = gtk_tool_palette_get_drop_group (palette, x, y);

  if (group)
    return gtk_tool_item_group_get_drop_item (GTK_TOOL_ITEM_GROUP (group),
                                              x - group->allocation.x,
                                              y - group->allocation.y);

  return NULL;
}

/**
 * gtk_tool_palette_get_drop_group:
 * @palette: an #GtkToolPalette.
 * @x: the x position.
 * @y: the y position.
 *
 * Gets the group at position (x, y).
 *
 * Returns: the #GtkToolItemGroup at position or %NULL if there is no such group.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_get_drop_group (GtkToolPalette *palette,
                                 gint            x,
                                 gint            y)
{
  GtkAllocation *allocation;
  guint i;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);

  allocation = &GTK_WIDGET (palette)->allocation;

  g_return_val_if_fail (x >= 0 && x < allocation->width, NULL);
  g_return_val_if_fail (y >= 0 && y < allocation->height, NULL);

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      GtkWidget *widget;
      gint x0, y0;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      x0 = x - widget->allocation.x;
      y0 = y - widget->allocation.y;

      if (x0 >= 0 && x0 < widget->allocation.width &&
          y0 >= 0 && y0 < widget->allocation.height)
        return widget;
    }

  return NULL;
}

/**
 * gtk_tool_palette_get_drag_item:
 * @palette: an #GtkToolPalette.
 * @selection: a #GtkSelectionData.
 *
 * Get the dragged item from the selection. This could be a #GtkToolItem or 
 * an #GtkToolItemGroup.
 *
 * Returns: the dragged item in selection.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_get_drag_item (GtkToolPalette         *palette,
                                const GtkSelectionData *selection)
{
  GtkToolPaletteDragData *data;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  g_return_val_if_fail (NULL != selection, NULL);

  g_return_val_if_fail (selection->format == 8, NULL);
  g_return_val_if_fail (selection->length == sizeof (GtkToolPaletteDragData), NULL);
  g_return_val_if_fail (selection->target == dnd_target_atom_item ||
                        selection->target == dnd_target_atom_group,
                        NULL);

  data = (GtkToolPaletteDragData*) selection->data;

  g_return_val_if_fail (data->palette == palette, NULL);

  if (dnd_target_atom_item == selection->target)
    g_return_val_if_fail (GTK_IS_TOOL_ITEM (data->item), NULL);
  else if (dnd_target_atom_group == selection->target)
    g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (data->item), NULL);

  return data->item;
}

/**
 * gtk_tool_palette_set_drag_source:
 * @palette: an #GtkToolPalette.
 * @targets: the #GtkToolPaletteDragTargets which the widget should support.
 *
 * Sets the tool palette as a drag source. Enables all groups and items in
 * the tool palette as drag sources on button 1 and button 3 press with copy
 * and move actions.
 *
 * See gtk_drag_source_set().
 *
 * Since: 2.18
 *
 */
void
gtk_tool_palette_set_drag_source (GtkToolPalette            *palette,
                                  GtkToolPaletteDragTargets  targets)
{
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if ((palette->priv->drag_source & targets) == targets)
    return;

  palette->priv->drag_source |= targets;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      if (palette->priv->groups[i].widget)
        gtk_container_forall (GTK_CONTAINER (palette->priv->groups[i].widget),
                              _gtk_tool_palette_child_set_drag_source,
                              palette);
    }
}

/**
 * gtk_tool_palette_add_drag_dest:
 * @palette: an #GtkToolPalette.
 * @widget: a #GtkWidget which should be a drag destination for palette.
 * @flags: the flags that specify what actions GTK+ should take for drops on that widget.
 * @targets: the #GtkToolPaletteDragTargets which the widget should support.
 * @actions: the #GdkDragAction<!-- -->s which the widget should suppport.
 *
 * Sets the tool palette as drag source (see gtk_tool_palette_set_drag_source) and
 * sets widget as a drag destination for drags from palette. With flags the actions
 * (like highlighting and target checking) which should be performed by GTK+ for
 * drops on widget can be specified. With targets the supported drag targets 
 * (groups and/or items) can be specified. With actions the supported drag actions
 * (copy and move) can be specified.
 *
 * See gtk_drag_dest_set().
 *
 * Since: 2.18
 */
void
gtk_tool_palette_add_drag_dest (GtkToolPalette            *palette,
                                GtkWidget                 *widget,
                                GtkDestDefaults            flags,
                                GtkToolPaletteDragTargets  targets,
                                GdkDragAction              actions)
{
  GtkTargetEntry entries[G_N_ELEMENTS (dnd_targets)];
  gint n_entries = 0;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_tool_palette_set_drag_source (palette,
                                    targets);

  if (targets & GTK_TOOL_PALETTE_DRAG_ITEMS)
    entries[n_entries++] = dnd_targets[0];
  if (targets & GTK_TOOL_PALETTE_DRAG_GROUPS)
    entries[n_entries++] = dnd_targets[1];

  gtk_drag_dest_set (widget, flags, entries, n_entries, actions);
}

void
_gtk_tool_palette_get_item_size (GtkToolPalette *palette,
                                 GtkRequisition *item_size,
                                 gboolean        homogeneous_only,
                                 gint           *requested_rows)
{
  GtkRequisition max_requisition;
  gint max_rows;
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (NULL != item_size);

  max_requisition.width = 0;
  max_requisition.height = 0;
  max_rows = 0;

  /* iterate over all groups and calculate the max item_size and max row request */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkRequisition requisition;
      gint rows;
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (!group->widget)
        continue;

      _gtk_tool_item_group_item_size_request (group->widget, &requisition, homogeneous_only, &rows);

      max_requisition.width = MAX (max_requisition.width, requisition.width);
      max_requisition.height = MAX (max_requisition.height, requisition.height);
      max_rows = MAX (max_rows, rows);
    }

  *item_size = max_requisition;
  if (requested_rows)
    *requested_rows = max_rows;
}

static void
gtk_tool_palette_item_drag_data_get (GtkWidget        *widget,
                                     GdkDragContext   *context G_GNUC_UNUSED,
                                     GtkSelectionData *selection,
                                     guint             info G_GNUC_UNUSED,
                                     guint             time G_GNUC_UNUSED,
                                     gpointer          data)
{
  GtkToolPaletteDragData drag_data = { GTK_TOOL_PALETTE (data), NULL };

  if (selection->target == dnd_target_atom_item)
    drag_data.item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM);

  if (drag_data.item)
    gtk_selection_data_set (selection, selection->target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

static void
gtk_tool_palette_child_drag_data_get (GtkWidget        *widget,
                                      GdkDragContext   *context G_GNUC_UNUSED,
                                      GtkSelectionData *selection,
                                      guint             info G_GNUC_UNUSED,
                                      guint             time G_GNUC_UNUSED,
                                      gpointer          data)
{
  GtkToolPaletteDragData drag_data = { GTK_TOOL_PALETTE (data), NULL };

  if (selection->target == dnd_target_atom_group)
    drag_data.item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM_GROUP);

  if (drag_data.item)
    gtk_selection_data_set (selection, selection->target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

void
_gtk_tool_palette_child_set_drag_source (GtkWidget *child,
                                         gpointer   data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (data);

  /* Check drag_source,
   * to work properly when called from gtk_tool_item_group_insert().
   */
  if (!palette->priv->drag_source)
    return;

  if (GTK_IS_TOOL_ITEM (child) &&
      (palette->priv->drag_source & GTK_TOOL_PALETTE_DRAG_ITEMS))
    {
      /* Connect to child instead of the item itself,
       * to work arround bug 510377.
       */
      if (GTK_IS_TOOL_BUTTON (child))
        child = gtk_bin_get_child (GTK_BIN (child));

      if (!child)
        return;

      gtk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[0], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (gtk_tool_palette_item_drag_data_get),
                        palette);
    }
  else if (GTK_IS_BUTTON (child) && 
           (palette->priv->drag_source & GTK_TOOL_PALETTE_DRAG_GROUPS))
    {
      gtk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[1], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (gtk_tool_palette_child_drag_data_get),
                        palette);
    }
}

/**
 * gtk_tool_palette_get_drag_target_item:
 *
 * Get the target entry for a dragged #GtkToolItem.
 *
 * Returns: the #GtkTargetEntry for a dragged item.
 *
 * Since: 2.18
 */
G_CONST_RETURN GtkTargetEntry*
gtk_tool_palette_get_drag_target_item (void)
{
  return &dnd_targets[0];
}

/**
 * gtk_tool_palette_get_drag_target_group:
 *
 * Get the target entry for a dragged #GtkToolItemGroup.
 *
 * Returns: the #GtkTargetEntry for a dragged group.
 *
 * Since: 2.18
 */
G_CONST_RETURN GtkTargetEntry*
gtk_tool_palette_get_drag_target_group (void)
{
  return &dnd_targets[1];
}

void
_gtk_tool_palette_set_expanding_child (GtkToolPalette *palette,
                                       GtkWidget      *widget)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  palette->priv->expanding_child = widget;
}

GtkAdjustment*
gtk_tool_palette_get_hadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  return palette->priv->hadjustment;
}

GtkAdjustment*
gtk_tool_palette_get_vadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  return palette->priv->vadjustment;
}

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090

GtkSizeGroup *
_gtk_tool_palette_get_size_group (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);

  return palette->priv->text_size_group;
}

#endif
/* GtkToolPalette -- A tool palette with categories and DnD support
 * Copyright (C) 2008  Openismus GmbH
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 */

#include "gtktoolpaletteprivate.h"
#include "gtkmarshalers.h"

#include <gtk/gtk.h>
#include <string.h>

#define DEFAULT_ICON_SIZE       GTK_ICON_SIZE_SMALL_TOOLBAR
#define DEFAULT_ORIENTATION     GTK_ORIENTATION_VERTICAL
#define DEFAULT_TOOLBAR_STYLE   GTK_TOOLBAR_ICONS

#define DEFAULT_CHILD_EXCLUSIVE FALSE
#define DEFAULT_CHILD_EXPAND    FALSE

#define P_(msgid) (msgid)

/**
 * SECTION:GtkToolPalette
 * @short_description: A tool palette with categories
 * @include: gtktoolpalette.h
 *
 * An #GtkToolPalette allows it to add #GtkToolItem<!-- -->s to a palette like container
 * with different categories and drag and drop support.
 *
 * An #GtkToolPalette is created with a call to gtk_tool_palette_new().
 *
 * #GtkToolItem<!-- -->s cannot be added directly to an #GtkToolPalette, instead they
 * are added to an #GtkToolItemGroup which can than be added to an #GtkToolPalette. To add
 * an #GtkToolItemGroup to an #GtkToolPalette use gtk_container_add().
 *
 * |[
 * GtkWidget *palette, *group;
 * GtkToolItem *item;
 *
 * palette = gtk_tool_palette_new ();
 * group = gtk_tool_item_group_new (_("Test Category"));
 * gtk_container_add (GTK_CONTAINER (palette), group);
 *
 * item = gtk_tool_button_new_from_stock (GTK_STOCK_OK);
 * gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
 * ]|
 *
 * The easiest way to use drag and drop with GtkToolPalette is to call gtk_tool_palette_add_drag_dest()
 * with the desired drag source @palette and the desired drag target @widget. Than gtk_tool_palette_get_drag_item()
 * can be used to get the dragged item in the #GtkWidget::drag-data-received signal handler of the drag target.
 *
 * |[
 * static void
 * passive_canvas_drag_data_received (GtkWidget        *widget,
 *                                    GdkDragContext   *context,
 *                                    gint              x,
 *                                    gint              y,
 *                                    GtkSelectionData *selection,
 *                                    guint             info,
 *                                    guint             time,
 *                                    gpointer          data)
 * {
 *   GtkWidget *palette;
 *   GtkWidget *item;
 *
 *   /<!-- -->* Get the dragged item *<!-- -->/
 *   palette = gtk_widget_get_ancestor (gtk_drag_get_source_widget (context), GTK_TYPE_TOOL_PALETTE);
 *   if (palette != NULL)
 *     item = gtk_tool_palette_get_drag_item (GTK_TOOL_PALETTE (palette), selection);
 *
 *   /<!-- -->* Do something with item *<!-- -->/
 * }
 *
 * GtkWidget *target, palette;
 *
 * palette = gtk_tool_palette_new ();
 * target = gtk_drawing_area_new ();
 *
 * g_signal_connect (G_OBJECT (target), "drag-data-received",
 *                   G_CALLBACK (passive_canvas_drag_data_received), NULL);   
 * gtk_tool_palette_add_drag_dest (GTK_TOOL_PALETTE (palette), target,
 *                                 GTK_DEST_DEFAULT_ALL,
 *                                 GTK_TOOL_PALETTE_DRAG_ITEMS,
 *                                 GDK_ACTION_COPY);
 * ]|
 *
 * Since: 2.18
 */

typedef struct _GtkToolItemGroupInfo   GtkToolItemGroupInfo;
typedef struct _GtkToolPaletteDragData GtkToolPaletteDragData;

enum
{
  PROP_NONE,
  PROP_ICON_SIZE,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
};

enum
{
  CHILD_PROP_NONE,
  CHILD_PROP_EXCLUSIVE,
  CHILD_PROP_EXPAND,
};

struct _GtkToolItemGroupInfo
{
  GtkToolItemGroup *widget;

  guint             notify_collapsed;
  guint             exclusive : 1;
  guint             expand : 1;
};

struct _GtkToolPalettePrivate
{
  GtkToolItemGroupInfo *groups;
  gsize                 groups_size;
  gsize                 groups_length;

  GtkAdjustment        *hadjustment;
  GtkAdjustment        *vadjustment;

  GtkIconSize           icon_size;
  GtkOrientation        orientation;
  GtkToolbarStyle       style;

  GtkWidget            *expanding_child;

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  GtkSizeGroup         *text_size_group;
#endif

  guint                 sparse_groups : 1;
  guint                 drag_source : 2;
};

struct _GtkToolPaletteDragData
{
  GtkToolPalette *palette;
  GtkWidget      *item;
};

static GdkAtom dnd_target_atom_item = GDK_NONE;
static GdkAtom dnd_target_atom_group = GDK_NONE;

static const GtkTargetEntry dnd_targets[] =
{
  { "application/x-GTK-tool-palette-item", GTK_TARGET_SAME_APP, 0 },
  { "application/x-GTK-tool-palette-group", GTK_TARGET_SAME_APP, 0 },
};

G_DEFINE_TYPE (GtkToolPalette,
               gtk_tool_palette,
               GTK_TYPE_CONTAINER);

static void
gtk_tool_palette_init (GtkToolPalette *palette)
{
  palette->priv = G_TYPE_INSTANCE_GET_PRIVATE (palette,
                                               GTK_TYPE_TOOL_PALETTE,
                                               GtkToolPalettePrivate);

  palette->priv->groups_size = 4;
  palette->priv->groups_length = 0;
  palette->priv->groups = g_new0 (GtkToolItemGroupInfo, palette->priv->groups_size);

  palette->priv->icon_size = DEFAULT_ICON_SIZE;
  palette->priv->orientation = DEFAULT_ORIENTATION;
  palette->priv->style = DEFAULT_TOOLBAR_STYLE;

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  palette->priv->text_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
#endif
}

static void
gtk_tool_palette_reconfigured (GtkToolPalette *palette)
{
  guint i;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      if (palette->priv->groups[i].widget)
        _gtk_tool_item_group_palette_reconfigured (palette->priv->groups[i].widget);
    }

  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (palette));
}

static void
gtk_tool_palette_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        if ((guint) g_value_get_enum (value) != palette->priv->icon_size)
          {
            palette->priv->icon_size = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      case PROP_ORIENTATION:
        if ((guint) g_value_get_enum (value) != palette->priv->orientation)
          {
            palette->priv->orientation = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      case PROP_TOOLBAR_STYLE:
        if ((guint) g_value_get_enum (value) != palette->priv->style)
          {
            palette->priv->style = g_value_get_enum (value);
            gtk_tool_palette_reconfigured (palette);
          }
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        g_value_set_enum (value, gtk_tool_palette_get_icon_size (palette));
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, gtk_tool_palette_get_orientation (palette));
        break;

      case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, gtk_tool_palette_get_style (palette));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_dispose (GObject *object)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);
  guint i;

  if (palette->priv->hadjustment)
    {
      g_object_unref (palette->priv->hadjustment);
      palette->priv->hadjustment = NULL;
    }

  if (palette->priv->vadjustment)
    {
      g_object_unref (palette->priv->vadjustment);
      palette->priv->vadjustment = NULL;
    }

  for (i = 0; i < palette->priv->groups_size; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (group->notify_collapsed)
        {
          g_signal_handler_disconnect (group->widget, group->notify_collapsed);
          group->notify_collapsed = 0;
        }
    }

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090
  if (palette->priv->text_size_group)
    {
      g_object_unref (palette->priv->text_size_group);
      palette->priv->text_size_group = NULL;
    }
#endif

  G_OBJECT_CLASS (gtk_tool_palette_parent_class)->dispose (object);
}

static void
gtk_tool_palette_finalize (GObject *object)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (object);

  if (palette->priv->groups)
    {
      palette->priv->groups_length = 0;
      g_free (palette->priv->groups);
      palette->priv->groups = NULL;
    }

  G_OBJECT_CLASS (gtk_tool_palette_parent_class)->finalize (object);
}

static void
gtk_tool_palette_size_request (GtkWidget      *widget,
                               GtkRequisition *requisition)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GtkRequisition child_requisition;
  guint i;

  requisition->width = 0;
  requisition->height = 0;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (!group->widget)
        continue;

      gtk_widget_size_request (GTK_WIDGET (group->widget), &child_requisition);

      if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
        {
          requisition->width = MAX (requisition->width, child_requisition.width);
          requisition->height += child_requisition.height;
        }
      else
        {
          requisition->width += child_requisition.width;
          requisition->height = MAX (requisition->height, child_requisition.height);
        }
    }

  requisition->width += border_width * 2;
  requisition->height += border_width * 2;
}

static void
gtk_tool_palette_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GtkAdjustment *adjustment = NULL;
  GtkAllocation child_allocation;

  gint n_expand_groups = 0;
  gint remaining_space = 0;
  gint expand_space = 0;

  gint page_start, page_size = 0;
  gint offset = 0;
  guint i;

  gint min_offset = -1, max_offset = -1;

  gint x;

  gint *group_sizes = g_newa(gint, palette->priv->groups_length);

  GtkTextDirection direction = gtk_widget_get_direction (widget);

  GTK_WIDGET_CLASS (gtk_tool_palette_parent_class)->size_allocate (widget, allocation);

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      adjustment = palette->priv->vadjustment;
      page_size = allocation->height;
    }
  else
    {
      adjustment = palette->priv->hadjustment;
      page_size = allocation->width;
    }

  if (adjustment)
    offset = gtk_adjustment_get_value (adjustment);
  if (GTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
      GTK_TEXT_DIR_RTL == direction)
    offset = -offset;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.width = allocation->width - border_width * 2;
  else
    child_allocation.height = allocation->height - border_width * 2;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    remaining_space = allocation->height;
  else
    remaining_space = allocation->width;

  /* figure out the required size of all groups to be able to distribute the
   * remaining space on allocation */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      gint size;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      if (gtk_tool_item_group_get_n_items (group->widget))
        {
          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            size = _gtk_tool_item_group_get_height_for_width (group->widget, child_allocation.width);
          else
            size = _gtk_tool_item_group_get_width_for_height (group->widget, child_allocation.height);

          if (group->expand && !gtk_tool_item_group_get_collapsed (group->widget))
            n_expand_groups += 1;
        }
      else
        size = 0;

      remaining_space -= size;
      group_sizes[i] = size;

      /* if the widget is currently expanding an offset which allows to display as much of the
       * widget as possible is calculated */
      if (widget == palette->priv->expanding_child)
        {
          gint limit =
            GTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
            child_allocation.width : child_allocation.height;

          gint real_size;
          guint j;

          min_offset = 0;

          for (j = 0; j < i; ++j)
            min_offset += group_sizes[j];

          max_offset = min_offset + group_sizes[i];

          real_size = _gtk_tool_item_group_get_size_for_limit
            (GTK_TOOL_ITEM_GROUP (widget), limit,
             GTK_ORIENTATION_VERTICAL == palette->priv->orientation,
             FALSE);

          if (size == real_size)
            palette->priv->expanding_child = NULL;
        }
    }

  if (n_expand_groups > 0)
    {
      remaining_space = MAX (0, remaining_space);
      expand_space = remaining_space / n_expand_groups;
    }

  if (max_offset != -1)
    {
      gint limit =
        GTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
        allocation->height : allocation->width;

      offset = MIN (MAX (offset, max_offset - limit), min_offset);
    }

  if (remaining_space > 0)
    offset = 0;

  x = border_width;
  child_allocation.y = border_width;

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.y -= offset;
  else
    x -= offset;

  /* allocate all groups at the calculated positions */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      GtkWidget *widget;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      if (gtk_tool_item_group_get_n_items (group->widget))
        {
          gint size = group_sizes[i];

          if (group->expand && !gtk_tool_item_group_get_collapsed (group->widget))
            {
              size += MIN (expand_space, remaining_space);
              remaining_space -= expand_space;
            }

          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.height = size;
          else
            child_allocation.width = size;

          if (GTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
              GTK_TEXT_DIR_RTL == direction)
            child_allocation.x = allocation->width - x - child_allocation.width;
          else
            child_allocation.x = x;

          gtk_widget_size_allocate (widget, &child_allocation);
          gtk_widget_show (widget);

          if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.y += child_allocation.height;
          else
            x += child_allocation.width;
        }
      else
        gtk_widget_hide (widget);
    }

  if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      child_allocation.y += border_width;
      child_allocation.y += offset;

      page_start = child_allocation.y;
    }
  else
    {
      x += border_width;
      x += offset;

      page_start = x;
    }

  /* update the scrollbar to match the displayed adjustment */
  if (adjustment)
    {
      gdouble value;

      adjustment->page_increment = page_size * 0.9;
      adjustment->step_increment = page_size * 0.1;
      adjustment->page_size = page_size;

      if (GTK_ORIENTATION_VERTICAL == palette->priv->orientation ||
          GTK_TEXT_DIR_LTR == direction)
        {
          adjustment->lower = 0;
          adjustment->upper = MAX (0, page_start);

          value = MIN (offset, adjustment->upper - adjustment->page_size);
          gtk_adjustment_clamp_page (adjustment, value, offset + page_size);
        }
      else
        {
          adjustment->lower = page_size - MAX (0, page_start);
          adjustment->upper = page_size;

          offset = -offset;

          value = MAX (offset, adjustment->lower);
          gtk_adjustment_clamp_page (adjustment, offset, value + page_size);
        }

      gtk_adjustment_changed (adjustment);
    }
}

static gboolean
gtk_tool_palette_expose_event (GtkWidget      *widget,
                               GdkEventExpose *event)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);
  GdkDisplay *display;
  cairo_t *cr;
  guint i;

  display = gdk_drawable_get_display (widget->window);

  if (!gdk_display_supports_composite (display))
    return FALSE;

  cr = gdk_cairo_create (widget->window);
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  cairo_push_group (cr);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if (palette->priv->groups[i].widget)
      _gtk_tool_item_group_paint (palette->priv->groups[i].widget, cr);

  cairo_pop_group_to_source (cr);

  cairo_paint (cr);
  cairo_destroy (cr);

  return FALSE;
}

static void
gtk_tool_palette_realize (GtkWidget *widget)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  GdkWindowAttr attributes;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK
                        | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                        | GDK_BUTTON_MOTION_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, attributes_mask);

  gdk_window_set_user_data (widget->window, widget);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  gtk_container_forall (GTK_CONTAINER (widget),
                        (GtkCallback) gtk_widget_set_parent_window,
                        widget->window);

  gtk_widget_queue_resize_no_redraw (widget);
}

static void
gtk_tool_palette_adjustment_value_changed (GtkAdjustment *adjustment G_GNUC_UNUSED,
                                           gpointer       data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  gtk_tool_palette_size_allocate (widget, &widget->allocation);
}

static void
gtk_tool_palette_set_scroll_adjustments (GtkWidget     *widget,
                                         GtkAdjustment *hadjustment,
                                         GtkAdjustment *vadjustment)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (widget);

  if (palette->priv->hadjustment)
    g_object_unref (palette->priv->hadjustment);
  if (palette->priv->vadjustment)
    g_object_unref (palette->priv->vadjustment);

  if (hadjustment)
    g_object_ref_sink (hadjustment);
  if (vadjustment)
    g_object_ref_sink (vadjustment);

  palette->priv->hadjustment = hadjustment;
  palette->priv->vadjustment = vadjustment;

  if (palette->priv->hadjustment)
    g_signal_connect (palette->priv->hadjustment, "value-changed",
                      G_CALLBACK (gtk_tool_palette_adjustment_value_changed),
                      palette);
  if (palette->priv->vadjustment)
    g_signal_connect (palette->priv->vadjustment, "value-changed",
                      G_CALLBACK (gtk_tool_palette_adjustment_value_changed),
                      palette);
}

static void
gtk_tool_palette_repack (GtkToolPalette *palette)
{
  guint si, di;

  for (si = di = 0; di < palette->priv->groups_length; ++si)
    {
      if (palette->priv->groups[si].widget)
        {
          palette->priv->groups[di] = palette->priv->groups[si];
          ++di;
        }
      else
        --palette->priv->groups_length;
    }

  palette->priv->sparse_groups = FALSE;
}

static void
gtk_tool_palette_add (GtkContainer *container,
                      GtkWidget    *child)
{
  GtkToolPalette *palette;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  palette = GTK_TOOL_PALETTE (container);

  if (palette->priv->groups_length == palette->priv->groups_size)
    gtk_tool_palette_repack (palette);

  if (palette->priv->groups_length == palette->priv->groups_size)
    {
      gsize old_size = palette->priv->groups_size;
      gsize new_size = old_size * 2;

      palette->priv->groups = g_renew (GtkToolItemGroupInfo,
                                       palette->priv->groups,
                                       new_size);

      memset (palette->priv->groups + old_size, 0,
              sizeof (GtkToolItemGroupInfo) * old_size);

      palette->priv->groups_size = new_size;
    }

  palette->priv->groups[palette->priv->groups_length].widget = g_object_ref_sink (child);
  palette->priv->groups_length += 1;

  gtk_widget_set_parent (child, GTK_WIDGET (palette));
}

static void
gtk_tool_palette_remove (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkToolPalette *palette;
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  palette = GTK_TOOL_PALETTE (container);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if ((GtkWidget*) palette->priv->groups[i].widget == child)
      {
        g_object_unref (child);
        gtk_widget_unparent (child);

        memset (&palette->priv->groups[i], 0, sizeof (GtkToolItemGroupInfo));
        palette->priv->sparse_groups = TRUE;
      }
}

static void
gtk_tool_palette_forall (GtkContainer *container,
                         gboolean      internals G_GNUC_UNUSED,
                         GtkCallback   callback,
                         gpointer      callback_data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);
  guint i;

  if (palette->priv->groups)
    {
      for (i = 0; i < palette->priv->groups_length; ++i)
        if (palette->priv->groups[i].widget)
          callback (GTK_WIDGET (palette->priv->groups[i].widget),
                    callback_data);
    }
}

static GType
gtk_tool_palette_child_type (GtkContainer *container G_GNUC_UNUSED)
{
  return GTK_TYPE_TOOL_ITEM_GROUP;
}

static void
gtk_tool_palette_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        gtk_tool_palette_set_exclusive (palette, child, g_value_get_boolean (value));
        break;

      case CHILD_PROP_EXPAND:
        gtk_tool_palette_set_expand (palette, child, g_value_get_boolean (value));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        g_value_set_boolean (value, gtk_tool_palette_get_exclusive (palette, child));
        break;

      case CHILD_PROP_EXPAND:
        g_value_set_boolean (value, gtk_tool_palette_get_expand (palette, child));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_palette_class_init (GtkToolPaletteClass *cls)
{
  GObjectClass      *oclass   = G_OBJECT_CLASS (cls);
  GtkWidgetClass    *wclass   = GTK_WIDGET_CLASS (cls);
  GtkContainerClass *cclass   = GTK_CONTAINER_CLASS (cls);

  oclass->set_property        = gtk_tool_palette_set_property;
  oclass->get_property        = gtk_tool_palette_get_property;
  oclass->dispose             = gtk_tool_palette_dispose;
  oclass->finalize            = gtk_tool_palette_finalize;

  wclass->size_request        = gtk_tool_palette_size_request;
  wclass->size_allocate       = gtk_tool_palette_size_allocate;
  wclass->expose_event        = gtk_tool_palette_expose_event;
  wclass->realize             = gtk_tool_palette_realize;

  cclass->add                 = gtk_tool_palette_add;
  cclass->remove              = gtk_tool_palette_remove;
  cclass->forall              = gtk_tool_palette_forall;
  cclass->child_type          = gtk_tool_palette_child_type;
  cclass->set_child_property  = gtk_tool_palette_set_child_property;
  cclass->get_child_property  = gtk_tool_palette_get_child_property;

  cls->set_scroll_adjustments = gtk_tool_palette_set_scroll_adjustments;
  /**
   * GtkToolPalette::set-scroll-adjustments:
   * @widget: the GtkToolPalette that received the signal
   * @hadjustment: The horizontal adjustment 
   * @vadjustment: The vertical adjustment
   *
   * The ::set-scroll-adjustments when FIXME
   *
   * Since: 2.18
   */
  wclass->set_scroll_adjustments_signal =
    g_signal_new ("set-scroll-adjustments",
                  G_TYPE_FROM_CLASS (oclass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkToolPaletteClass, set_scroll_adjustments),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ADJUSTMENT,
                  GTK_TYPE_ADJUSTMENT);

  g_object_class_install_property (oclass, PROP_ICON_SIZE,
                                   g_param_spec_enum ("icon-size",
                                                      P_("Icon Size"),
                                                      P_("The size of palette icons"),
                                                      GTK_TYPE_ICON_SIZE,
                                                      DEFAULT_ICON_SIZE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      P_("Orientation"),
                                                      P_("Orientation of the tool palette"),
                                                      GTK_TYPE_ORIENTATION,
                                                      DEFAULT_ORIENTATION,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_TOOLBAR_STYLE,
                                   g_param_spec_enum ("toolbar-style",
                                                      P_("Toolbar Style"),
                                                      P_("Style of items in the tool palette"),
                                                      GTK_TYPE_TOOLBAR_STYLE,
                                                      DEFAULT_TOOLBAR_STYLE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXCLUSIVE,
                                              g_param_spec_boolean ("exclusive",
                                                                    P_("Exclusive"),
                                                                    P_("Whether the item group should be the only expanded at a given time"),
                                                                    DEFAULT_CHILD_EXCLUSIVE,
                                                                    G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                                    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXPAND,
                                              g_param_spec_boolean ("expand",
                                                                    P_("Expand"),
                                                                    P_("Whether the item group should receive extra space when the palette grows"),
                                                                    DEFAULT_CHILD_EXPAND,
                                                                    G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                                    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (GtkToolPalettePrivate));

  dnd_target_atom_item = gdk_atom_intern_static_string (dnd_targets[0].target);
  dnd_target_atom_group = gdk_atom_intern_static_string (dnd_targets[1].target);
}

/**
 * gtk_tool_palette_new:
 *
 * Creates a new tool palette.
 *
 * Returns: a new #GtkToolPalette.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_new (void)
{
  return g_object_new (GTK_TYPE_TOOL_PALETTE, NULL);
}

/**
 * gtk_tool_palette_set_icon_size:
 * @palette: an #GtkToolPalette.
 * @icon_size: the #GtkIconSize that icons in the tool palette shall have.
 *
 * Sets the size of icons in the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_icon_size (GtkToolPalette *palette,
                                GtkIconSize     icon_size)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (icon_size != palette->priv->icon_size)
    g_object_set (palette, "icon-size", icon_size, NULL);
}

/**
 * gtk_tool_palette_set_orientation:
 * @palette: an #GtkToolPalette.
 * @orientation: the #GtkOrientation that the tool palette shall have.
 *
 * Sets the orientation (horizontal or vertical) of the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_orientation (GtkToolPalette *palette,
                                  GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (orientation != palette->priv->orientation)
    g_object_set (palette, "orientation", orientation, NULL);
}

/**
 * gtk_tool_palette_set_style:
 * @palette: an #GtkToolPalette.
 * @style: the #GtkToolbarStyle that items in the tool palette shall have.
 *
 * Sets the style (text, icons or both) of items in the tool palette.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_style (GtkToolPalette  *palette,
                            GtkToolbarStyle  style)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if (style != palette->priv->style)
    g_object_set (palette, "toolbar-style", style, NULL);
}

/**
 * gtk_tool_palette_get_icon_size:
 * @palette: an #GtkToolPalette.
 *
 * Gets the size of icons in the tool palette. See gtk_tool_palette_set_icon_size().
 * 
 * Returns: the #GtkIconSize of icons in the tool palette.
 *
 * Since: 2.18
 */
GtkIconSize
gtk_tool_palette_get_icon_size (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_ICON_SIZE);
  return palette->priv->icon_size;
}

/**
 * gtk_tool_palette_get_orientation:
 * @palette: an #GtkToolPalette.
 *
 * Gets the orientation (horizontal or vertical) of the tool palette. See gtk_tool_palette_set_orientation().
 *
 * Returns the #GtkOrientation of the tool palette.
 */
GtkOrientation
gtk_tool_palette_get_orientation (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_ORIENTATION);
  return palette->priv->orientation;
}

/**
 * gtk_tool_palette_get_style:
 * @palette: an #GtkToolPalette.
 *
 * Gets the style (icons, text or both) of items in the tool palette.
 *
 * Returns: the #GtkToolbarStyle of items in the tool palette.
 *
 * Since: 2.18
 */
GtkToolbarStyle
gtk_tool_palette_get_style (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_TOOLBAR_STYLE);
  return palette->priv->style;
}

/**
 * gtk_tool_palette_set_group_position:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @position: a new index for group.
 *
 * Sets the position of the group as an index of the tool palette.
 * If position is 0 the group will become the first child, if position is
 * -1 it will become the last child.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_group_position (GtkToolPalette *palette,
                                     GtkWidget      *group,
                                     gint            position)
{
  GtkToolItemGroupInfo group_info;
  gint old_position;
  gpointer src, dst;
  gsize len;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  gtk_tool_palette_repack (palette);

  g_return_if_fail (position >= -1);

  if (-1 == position)
    position = palette->priv->groups_length - 1;

  g_return_if_fail ((guint) position < palette->priv->groups_length);

  if (GTK_TOOL_ITEM_GROUP (group) == palette->priv->groups[position].widget)
    return;

  old_position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (old_position >= 0);

  group_info = palette->priv->groups[old_position];

  if (position < old_position)
    {
      dst = palette->priv->groups + position + 1;
      src = palette->priv->groups + position;
      len = old_position - position;
    }
  else
    {
      dst = palette->priv->groups + old_position;
      src = palette->priv->groups + old_position + 1;
      len = position - old_position;
    }

  memmove (dst, src, len * sizeof (*palette->priv->groups));
  palette->priv->groups[position] = group_info;

  gtk_widget_queue_resize (GTK_WIDGET (palette));
}

static void
gtk_tool_palette_group_notify_collapsed (GtkToolItemGroup *group,
                                         GParamSpec       *pspec G_GNUC_UNUSED,
                                         gpointer          data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (data);
  guint i;

  if (gtk_tool_item_group_get_collapsed (group))
    return;

  for (i = 0; i < palette->priv->groups_size; ++i)
    {
      GtkToolItemGroup *current_group = palette->priv->groups[i].widget;

      if (current_group && current_group != group)
        gtk_tool_item_group_set_collapsed (palette->priv->groups[i].widget, TRUE);
    }
}

/**
 * gtk_tool_palette_set_exclusive:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @exclusive: whether the group should be exclusive or not.
 *
 * Sets whether the group should be exclusive or not. If an exclusive group is expanded
 * all other groups are collapsed.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_exclusive (GtkToolPalette *palette,
                                GtkWidget      *group,
                                gboolean        exclusive)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = &palette->priv->groups[position];

  if (exclusive == group_info->exclusive)
    return;

  group_info->exclusive = exclusive;

  if (group_info->exclusive != (0 != group_info->notify_collapsed))
    {
      if (group_info->exclusive)
        {
          group_info->notify_collapsed =
            g_signal_connect (group, "notify::collapsed",
                              G_CALLBACK (gtk_tool_palette_group_notify_collapsed),
                              palette);
        }
      else
        {
          g_signal_handler_disconnect (group, group_info->notify_collapsed);
          group_info->notify_collapsed = 0;
        }
    }

  gtk_tool_palette_group_notify_collapsed (group_info->widget, NULL, palette);
  gtk_widget_child_notify (group, "exclusive");
}

/**
 * gtk_tool_palette_set_expand:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 * @expand: whether the group should be given extra space.
 *
 * Sets whether the group should be given extra space.
 *
 * Since: 2.18
 */
void
gtk_tool_palette_set_expand (GtkToolPalette *palette,
                             GtkWidget      *group,
                             gboolean        expand)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = &palette->priv->groups[position];

  if (expand != group_info->expand)
    {
      group_info->expand = expand;
      gtk_widget_queue_resize (GTK_WIDGET (palette));
      gtk_widget_child_notify (group, "expand");
    }
}

/**
 * gtk_tool_palette_get_group_position:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup.
 *
 * Gets the position of @group in @palette as index. see gtk_tool_palette_set_group_position().
 *
 * Returns: the index of group or -1 if @group is not a child of @palette.
 *
 * Since: 2.18
 */
gint
gtk_tool_palette_get_group_position (GtkToolPalette *palette,
                                     GtkWidget      *group)
{
  guint i;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), -1);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), -1);

  for (i = 0; i < palette->priv->groups_length; ++i)
    if ((gpointer) group == palette->priv->groups[i].widget)
      return i;

  return -1;
}

/**
 * gtk_tool_palette_get_exclusive:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 *
 * Gets whether group is exclusive or not. See gtk_tool_palette_set_exclusive().
 *
 * Returns: %TRUE if group is exclusive.
 *
 * Since: 2.18
 */
gboolean
gtk_tool_palette_get_exclusive (GtkToolPalette *palette,
                                GtkWidget      *group)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXCLUSIVE);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXCLUSIVE);

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXCLUSIVE);

  return palette->priv->groups[position].exclusive;
}

/**
 * gtk_tool_palette_get_expand:
 * @palette: an #GtkToolPalette.
 * @group: an #GtkToolItemGroup which is a child of palette.
 *
 * Gets whether group should be given extra space. See gtk_tool_palette_set_expand().
 *
 * Returns: %TRUE if group should be given extra space, %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean
gtk_tool_palette_get_expand (GtkToolPalette *palette,
                             GtkWidget      *group)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXPAND);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXPAND);

  position = gtk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXPAND);

  return palette->priv->groups[position].expand;
}

/**
 * gtk_tool_palette_get_drop_item:
 * @palette: an #GtkToolPalette.
 * @x: the x position.
 * @y: the y position.
 *
 * Gets the item at position (x, y). See gtk_tool_palette_get_drop_group().
 *
 * Returns: the #GtkToolItem at position or %NULL if there is no such item.
 *
 * Since: 2.18
 */
GtkToolItem*
gtk_tool_palette_get_drop_item (GtkToolPalette *palette,
                                gint            x,
                                gint            y)
{
  GtkWidget *group = gtk_tool_palette_get_drop_group (palette, x, y);

  if (group)
    return gtk_tool_item_group_get_drop_item (GTK_TOOL_ITEM_GROUP (group),
                                              x - group->allocation.x,
                                              y - group->allocation.y);

  return NULL;
}

/**
 * gtk_tool_palette_get_drop_group:
 * @palette: an #GtkToolPalette.
 * @x: the x position.
 * @y: the y position.
 *
 * Gets the group at position (x, y).
 *
 * Returns: the #GtkToolItemGroup at position or %NULL if there is no such group.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_get_drop_group (GtkToolPalette *palette,
                                 gint            x,
                                 gint            y)
{
  GtkAllocation *allocation;
  guint i;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);

  allocation = &GTK_WIDGET (palette)->allocation;

  g_return_val_if_fail (x >= 0 && x < allocation->width, NULL);
  g_return_val_if_fail (y >= 0 && y < allocation->height, NULL);

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];
      GtkWidget *widget;
      gint x0, y0;

      if (!group->widget)
        continue;

      widget = GTK_WIDGET (group->widget);

      x0 = x - widget->allocation.x;
      y0 = y - widget->allocation.y;

      if (x0 >= 0 && x0 < widget->allocation.width &&
          y0 >= 0 && y0 < widget->allocation.height)
        return widget;
    }

  return NULL;
}

/**
 * gtk_tool_palette_get_drag_item:
 * @palette: an #GtkToolPalette.
 * @selection: a #GtkSelectionData.
 *
 * Get the dragged item from the selection. This could be a #GtkToolItem or 
 * an #GtkToolItemGroup.
 *
 * Returns: the dragged item in selection.
 *
 * Since: 2.18
 */
GtkWidget*
gtk_tool_palette_get_drag_item (GtkToolPalette         *palette,
                                const GtkSelectionData *selection)
{
  GtkToolPaletteDragData *data;

  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  g_return_val_if_fail (NULL != selection, NULL);

  g_return_val_if_fail (selection->format == 8, NULL);
  g_return_val_if_fail (selection->length == sizeof (GtkToolPaletteDragData), NULL);
  g_return_val_if_fail (selection->target == dnd_target_atom_item ||
                        selection->target == dnd_target_atom_group,
                        NULL);

  data = (GtkToolPaletteDragData*) selection->data;

  g_return_val_if_fail (data->palette == palette, NULL);

  if (dnd_target_atom_item == selection->target)
    g_return_val_if_fail (GTK_IS_TOOL_ITEM (data->item), NULL);
  else if (dnd_target_atom_group == selection->target)
    g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (data->item), NULL);

  return data->item;
}

/**
 * gtk_tool_palette_set_drag_source:
 * @palette: an #GtkToolPalette.
 * @targets: the #GtkToolPaletteDragTargets which the widget should support.
 *
 * Sets the tool palette as a drag source. Enables all groups and items in
 * the tool palette as drag sources on button 1 and button 3 press with copy
 * and move actions.
 *
 * See gtk_drag_source_set().
 *
 * Since: 2.18
 *
 */
void
gtk_tool_palette_set_drag_source (GtkToolPalette            *palette,
                                  GtkToolPaletteDragTargets  targets)
{
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));

  if ((palette->priv->drag_source & targets) == targets)
    return;

  palette->priv->drag_source |= targets;

  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      if (palette->priv->groups[i].widget)
        gtk_container_forall (GTK_CONTAINER (palette->priv->groups[i].widget),
                              _gtk_tool_palette_child_set_drag_source,
                              palette);
    }
}

/**
 * gtk_tool_palette_add_drag_dest:
 * @palette: an #GtkToolPalette.
 * @widget: a #GtkWidget which should be a drag destination for palette.
 * @flags: the flags that specify what actions GTK+ should take for drops on that widget.
 * @targets: the #GtkToolPaletteDragTargets which the widget should support.
 * @actions: the #GdkDragAction<!-- -->s which the widget should suppport.
 *
 * Sets the tool palette as drag source (see gtk_tool_palette_set_drag_source) and
 * sets widget as a drag destination for drags from palette. With flags the actions
 * (like highlighting and target checking) which should be performed by GTK+ for
 * drops on widget can be specified. With targets the supported drag targets 
 * (groups and/or items) can be specified. With actions the supported drag actions
 * (copy and move) can be specified.
 *
 * See gtk_drag_dest_set().
 *
 * Since: 2.18
 */
void
gtk_tool_palette_add_drag_dest (GtkToolPalette            *palette,
                                GtkWidget                 *widget,
                                GtkDestDefaults            flags,
                                GtkToolPaletteDragTargets  targets,
                                GdkDragAction              actions)
{
  GtkTargetEntry entries[G_N_ELEMENTS (dnd_targets)];
  gint n_entries = 0;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_tool_palette_set_drag_source (palette,
                                    targets);

  if (targets & GTK_TOOL_PALETTE_DRAG_ITEMS)
    entries[n_entries++] = dnd_targets[0];
  if (targets & GTK_TOOL_PALETTE_DRAG_GROUPS)
    entries[n_entries++] = dnd_targets[1];

  gtk_drag_dest_set (widget, flags, entries, n_entries, actions);
}

void
_gtk_tool_palette_get_item_size (GtkToolPalette *palette,
                                 GtkRequisition *item_size,
                                 gboolean        homogeneous_only,
                                 gint           *requested_rows)
{
  GtkRequisition max_requisition;
  gint max_rows;
  guint i;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (NULL != item_size);

  max_requisition.width = 0;
  max_requisition.height = 0;
  max_rows = 0;

  /* iterate over all groups and calculate the max item_size and max row request */
  for (i = 0; i < palette->priv->groups_length; ++i)
    {
      GtkRequisition requisition;
      gint rows;
      GtkToolItemGroupInfo *group = &palette->priv->groups[i];

      if (!group->widget)
        continue;

      _gtk_tool_item_group_item_size_request (group->widget, &requisition, homogeneous_only, &rows);

      max_requisition.width = MAX (max_requisition.width, requisition.width);
      max_requisition.height = MAX (max_requisition.height, requisition.height);
      max_rows = MAX (max_rows, rows);
    }

  *item_size = max_requisition;
  if (requested_rows)
    *requested_rows = max_rows;
}

static void
gtk_tool_palette_item_drag_data_get (GtkWidget        *widget,
                                     GdkDragContext   *context G_GNUC_UNUSED,
                                     GtkSelectionData *selection,
                                     guint             info G_GNUC_UNUSED,
                                     guint             time G_GNUC_UNUSED,
                                     gpointer          data)
{
  GtkToolPaletteDragData drag_data = { GTK_TOOL_PALETTE (data), NULL };

  if (selection->target == dnd_target_atom_item)
    drag_data.item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM);

  if (drag_data.item)
    gtk_selection_data_set (selection, selection->target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

static void
gtk_tool_palette_child_drag_data_get (GtkWidget        *widget,
                                      GdkDragContext   *context G_GNUC_UNUSED,
                                      GtkSelectionData *selection,
                                      guint             info G_GNUC_UNUSED,
                                      guint             time G_GNUC_UNUSED,
                                      gpointer          data)
{
  GtkToolPaletteDragData drag_data = { GTK_TOOL_PALETTE (data), NULL };

  if (selection->target == dnd_target_atom_group)
    drag_data.item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM_GROUP);

  if (drag_data.item)
    gtk_selection_data_set (selection, selection->target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

void
_gtk_tool_palette_child_set_drag_source (GtkWidget *child,
                                         gpointer   data)
{
  GtkToolPalette *palette = GTK_TOOL_PALETTE (data);

  /* Check drag_source,
   * to work properly when called from gtk_tool_item_group_insert().
   */
  if (!palette->priv->drag_source)
    return;

  if (GTK_IS_TOOL_ITEM (child) &&
      (palette->priv->drag_source & GTK_TOOL_PALETTE_DRAG_ITEMS))
    {
      /* Connect to child instead of the item itself,
       * to work arround bug 510377.
       */
      if (GTK_IS_TOOL_BUTTON (child))
        child = gtk_bin_get_child (GTK_BIN (child));

      if (!child)
        return;

      gtk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[0], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (gtk_tool_palette_item_drag_data_get),
                        palette);
    }
  else if (GTK_IS_BUTTON (child) && 
           (palette->priv->drag_source & GTK_TOOL_PALETTE_DRAG_GROUPS))
    {
      gtk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[1], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (gtk_tool_palette_child_drag_data_get),
                        palette);
    }
}

/**
 * gtk_tool_palette_get_drag_target_item:
 *
 * Get the target entry for a dragged #GtkToolItem.
 *
 * Returns: the #GtkTargetEntry for a dragged item.
 *
 * Since: 2.18
 */
G_CONST_RETURN GtkTargetEntry*
gtk_tool_palette_get_drag_target_item (void)
{
  return &dnd_targets[0];
}

/**
 * gtk_tool_palette_get_drag_target_group:
 *
 * Get the target entry for a dragged #GtkToolItemGroup.
 *
 * Returns: the #GtkTargetEntry for a dragged group.
 *
 * Since: 2.18
 */
G_CONST_RETURN GtkTargetEntry*
gtk_tool_palette_get_drag_target_group (void)
{
  return &dnd_targets[1];
}

void
_gtk_tool_palette_set_expanding_child (GtkToolPalette *palette,
                                       GtkWidget      *widget)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (palette));
  palette->priv->expanding_child = widget;
}

GtkAdjustment*
gtk_tool_palette_get_hadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  return palette->priv->hadjustment;
}

GtkAdjustment*
gtk_tool_palette_get_vadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);
  return palette->priv->vadjustment;
}

#ifdef HAVE_EXTENDED_TOOL_SHELL_SUPPORT_BUG_535090

GtkSizeGroup *
_gtk_tool_palette_get_size_group (GtkToolPalette *palette)
{
  g_return_val_if_fail (GTK_IS_TOOL_PALETTE (palette), NULL);

  return palette->priv->text_size_group;
}

#endif
